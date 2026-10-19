// ============================================================================
// Copyright 2020-2021 Emerson Paradigm Holding LLC. All rights reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ============================================================================

#include <iostream>
#include <sstream>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "ServerConfig.h"

#include "Session.h"
#include "Repo.h"

// Exposed Protocols
#include "ArrayProto.h"
#include "DiscoProto.h"
#include "SpaceProto.h"
#include "StoreProto.h"
#include "TransProto.h"
#include "TypesProto.h"
#include "StoreOSDUProto.h"
#include "SpaceOSDUProto.h"

#include <oes/common/auth/AuthN.h>
#include <oes/common/auth/JWToken.h>
#include <oes/common/auth/Entitlement.h>

#include <oes/osdu/api/EntitlementClient.h>

#include <oes/core/utils/Logger.h>
#include <oes/core/utils/String.h>
#include <oes/core/utils/System.h>

#include <oes/postgresql/DbConnectionPool.h>


//============================================================================

namespace oes::eml::openkv {

namespace auth = oes::common::auth;

//============================================================================

OpenKVAuthNMgr builtinAuthMgr(
    OpenKVUserMgr usr_mgr, std::string jwtSecret
) {
    return [usr_mgr, secret=std::move(jwtSecret)](
        const auth::AuthInfo& authenticationInfo,
        auth::AuthDetails* details
    ) {
        auto au = auth::AuthenticatedUser::decodeFromHeader(
            authenticationInfo.authentication_, secret, details
        );
        if (au.ok_) {
            // Decoded OK. Doesn't mean we accept that user/password combo
            // We delegate that decision to the usr_mgr
            std::string local_err;
            std::string& err = details? details->err_msg_: local_err;

            try {
                au.ok_ = usr_mgr(au.name_, au.pswd_);
            } catch (const std::exception& ex) {
                err += "User-Mgr: ";
                err += ex.what();
            } catch (...) {
                err = "User-Mgr: Unknown exception";
            }

            if (!au.ok_ && err.empty()) {
                err = "Unknown user or invalid password";
            } else {
                au.read_only_ = false; // for backward-compatibility
            }
        }
        return au;
    };
}

//============================================================================

template <typename T> const void getSupportedProtocolCapabilities( oes::common::etp12::Datatypes::SupportedProtocol &proto) {
    proto.m_protocolCapabilities.clear();

    int i = 1 ;
    while( const auto msg = T::messageName(i) ) {
        oes::common::etp12::Datatypes::DataValue val;
        const std::string key(msg) ;
        if(key.find("Response") != std::string::npos) {
            ++i;
            continue;
        }
        std::string::size_type pos = key.find('(');
        val.m_item = i;
        proto.m_protocolCapabilities.emplace(std::make_pair(
            pos == std::string::npos ? key : key.substr(0, pos), val
        ));
        ++i;
    }
}

etp12::ServerConfig makeServerConfig(
    DbConnectionPool_ptr db_connection_pool,
    OpenKVAuthNMgr auth_mgr,
    std::string authn_delegation_url,
    OpenKVSpaceMgr space_mgr,
    OpenKVSpaceResolver space_resolver,
    EntitlementMgr entitlement_mgr,
    oes::eml::openETPServerConfig_ptr server_config_ptr
) {
    etp12::ServerConfig config;
    std::string ds_name;

    struct SessionFactoryState {
        DbConnectionPool_ptr db_connection_pool_;
        OpenKVAuthNMgr auth_mgr_;
        OpenKVSpaceMgr space_mgr_;
        OpenKVSpaceResolver space_resolver_;
        EntitlementMgr entitlement_mgr_;
    };

    auto session_state_ptr   = std::make_shared<SessionFactoryState>();
    session_state_ptr->db_connection_pool_ = db_connection_pool;
    session_state_ptr->auth_mgr_           = std::move(auth_mgr);
    session_state_ptr->space_mgr_          = std::move(space_mgr);
    session_state_ptr->space_resolver_     = std::move(space_resolver);
    session_state_ptr->entitlement_mgr_ = std::move(entitlement_mgr);

    config.db_connection_pool_ = db_connection_pool;

    config.session_factory_ = [session_state_ptr, server_config_ptr]( // Important: by-value
        etp12::Handshake shaker,
        etp12::RequestHeaderGetter getter
    )   -> std::shared_ptr<etp12::Session>
    {
        OES_UNUSED(getter);

        std::string db_connection_key;
        const std::string& dbm = server_config_ptr->partitionMode();
        if(dbm.empty()) {
            throw std::runtime_error("Partition mode unknown.");
        }  

        if (dbm == "multiple") {
            db_connection_key = shaker.data_partition_id_;
        } else {
            db_connection_key = "single";
        }

        // Check partition existence
        if (!session_state_ptr->db_connection_pool_->isDatabaseRegistered(db_connection_key)) {
            // Do not throw at this stage, we will later decode the token to find out if the partitionId 
            // is embedded in it. If not, THEN we can throw.
            const auto message = "Early check: Partition with name '" + db_connection_key + "' not found.";
            OES_WARNING(message);
        }

        auto session = std::make_shared<OpenKVSession>(
            std::move(shaker)
        );
        // authZ enabled when an entitlement manager is configured (--authZ != none).
        session->authZ_enabled_ = static_cast<bool>(session_state_ptr->entitlement_mgr_);
        return session;
    };

    config.authn_delegate_url_ = authn_delegation_url ;
    config.user_factory_ = [session_state_ptr, server_config_ptr]( 
        etp12::Handshake shaker
    ) -> auth::AuthenticatedUser_ptr
    {
        auth::AuthInfo auth_info(shaker.authorization_, shaker.data_partition_id_);
        auth::AuthDetails details;
        details.client_id_ = shaker.client_id_;
        auth::AuthenticatedUser user = session_state_ptr->auth_mgr_(
            auth_info, &details
        );

        if (!user.ok_) {
            throw std::runtime_error(details.err_msg_ + ": Authorization refused.");
        }

        // TODO: Password encryption, with login-restrictions, as la OpenDB Login Ticket
        auto user_ptr = std::make_shared<auth::AuthenticatedUser>(std::move(user));
        user_ptr->ip_address_ = shaker.remote_endpoint_;

        if (oes::core::Logger::instance().minSeverityFilter() <= oes::core::Message::Severity::debug) {
            const char* auth_type;

            switch (user.type_) {
            case auth::AuthType::eUnknown:
                auth_type = "NONE";
                break;

            case auth::AuthType::eBasic:
                auth_type = "BASIC";
                OES_DEBUG(
                    "Basic Auth: {}", details.basic_decoded_
                );
                break;

            case auth::AuthType::eBearer:
                auth_type = "JWT";
                OES_DEBUG(
                    "JWT Auth:\n\tHeader: {}\n\tPayload: {}",
                    details.jwt_header_, oes::core::StringUtils::abbreviate(details.jwt_payload_)
                );
                break;

            case auth::AuthType::eOsduToken:
                auth_type = "OSDU";
                OES_DEBUG(
                    "OSDU Auth:\n\tHeader: {}",
                    oes::core::StringUtils::abbreviate(shaker.authorization_)
                );
                break;

                // no default on purpose
            }

            // const std::string& connectivity_mode = server_config_ptr->connectivityMode();
            // if(connectivity_mode.empty()) {
            //     throw std::runtime_error("Connection mode unknown.");
            // }

            // if (connectivity_mode == "standalone") {
            //     OES_DEBUG(
            //         "Auth: {}\n\tusername: {}\n\tpassword: {}",
            //         auth_type, user.name_, user.pswd_
            //     );
            // } else {
            //     OES_DEBUG(
            //         "Auth: {}\n\tname: {}",
            //         auth_type, user.name_
            //     );
            // }
        }

        return user_ptr;
    };

    config.entitlement_factory_ = [session_state_ptr, server_config_ptr](
        etp12::Handshake shaker,
        auth::AuthenticatedUser_ptr user_ptr
    ) -> auth::Entitlement_ptr
    {
        // If shaker.auth_mode per partition (group or policy)
        auth::Entitlement_ptr entitlement_ptr;
        if (session_state_ptr->entitlement_mgr_) {
            entitlement_ptr = session_state_ptr->entitlement_mgr_();
        }

        if (entitlement_ptr) {
            if (entitlement_ptr->isGroupBased()) {
                auto tmp = std::dynamic_pointer_cast<auth::EntitlementGroups>(entitlement_ptr);
                tmp->init(shaker.data_partition_id_, user_ptr, std::string(), std::vector<std::string>());
                entitlement_ptr = std::dynamic_pointer_cast<auth::Entitlement>(tmp);
            } else {
                auto tmp = std::dynamic_pointer_cast<auth::EntitlementPolicy>(entitlement_ptr);
                tmp->init(shaker.data_partition_id_, user_ptr, shaker.session_id_, shaker.client_id_, oes::osdu::api::DSPermission::makeNonePermission());
                entitlement_ptr = std::dynamic_pointer_cast<auth::Entitlement>(tmp);
            }

            if (!entitlement_ptr->isValid()) {
                OES_ERROR("Authorization failed. Session refused.");
                // Still return the entitlement_ptr, to differentiiate from no-auth
                return entitlement_ptr;
            }

            const std::string& connectivity_mode = server_config_ptr->connectivityMode();
            // if(connectivity_mode.empty()) {
            //     throw std::runtime_error("Connection mode unknown.");
            // }      
            if (connectivity_mode == "osdu") {
                const std::string osduEmail = entitlement_ptr->getMemberEmail();
                OES_DEBUG(
                    "Auth: {}\n\temail: {}",
                    "OSDU", osduEmail
                );
            }
        }

        return entitlement_ptr;
    };

    // List of supported ETP protocols
    config.supportedProtocols_ = {
        oes::common::etp12::Datatypes::Protocol::eCore,
        oes::common::etp12::Datatypes::Protocol::eDataspace,
        oes::common::etp12::Datatypes::Protocol::eDiscovery,
        oes::common::etp12::Datatypes::Protocol::eSupportedTypes,
        oes::common::etp12::Datatypes::Protocol::eStore,
        oes::common::etp12::Datatypes::Protocol::eDataArray,
        oes::common::etp12::Datatypes::Protocol::eTransaction,
        oes::common::etp12::Datatypes::Protocol::eCoreOSDU,
        oes::common::etp12::Datatypes::Protocol::eStoreOSDU,
        oes::common::etp12::Datatypes::Protocol::eDataspaceOSDU,
    };

    // the protocol is "owned" by the session, thus does not reference it
    // via a std::shared_ptr or even a std::weak_ptr, but "directly".
    // The session represents the "shared-state between protocols".
    config.protocol_factory_ = [db_connection_pool, server_config_ptr](
        const oes::common::etp12::Datatypes::SupportedProtocol& req_proto, std::weak_ptr<etp12::Session> session
    )    -> oes::common::etp12::Protocol::IProtocol_ptr
    {
        std::weak_ptr<OpenKVSession> ora_session = 
            std::dynamic_pointer_cast<OpenKVSession>(session.lock());
        auto repo = std::make_shared<OpenKVRepo>(ora_session, db_connection_pool, server_config_ptr);
        // Warning: keep consistent with config.server_capabilities_ below
        switch (req_proto.m_protocol) {
        case static_cast<int>(oes::common::etp12::Datatypes::Protocol::eDataspace):
            if (req_proto.m_role == "store") {
                return std::make_shared<OpenKVDataspace>(std::move(repo), ora_session);
            }
            break;
        case static_cast<int>(oes::common::etp12::Datatypes::Protocol::eDiscovery):
            if (req_proto.m_role == "store") {
                return std::make_shared<OpenKVDiscovery>(std::move(repo), ora_session);
            }
            break;
        case static_cast<int>(oes::common::etp12::Datatypes::Protocol::eSupportedTypes):
            if (req_proto.m_role == "store") {
                return std::make_shared<OpenKVSupportedTypes>(std::move(repo), ora_session);
            }
            break;
        case static_cast<int>(oes::common::etp12::Datatypes::Protocol::eStore):
            if (req_proto.m_role == "store") {
                return std::make_shared<OpenKVStore>(std::move(repo), ora_session);
            }
            break;
        case static_cast<int>(oes::common::etp12::Datatypes::Protocol::eDataArray):
            if (req_proto.m_role == "store") {
                return std::make_shared<OpenKVDataArray>(std::move(repo), ora_session);
            }
            break;
        case static_cast<int>(oes::common::etp12::Datatypes::Protocol::eTransaction):
            if (req_proto.m_role == "store") {
                return std::make_shared<OpenKVTransProto>(std::move(repo), ora_session);
            }
            break;
        case static_cast<int>(etp12::Datatypes::Protocol::eStoreOSDU):
            if (req_proto.m_role == "store") {
                return std::make_shared<OpenKVStoreOSDU>(std::move(repo), ora_session);
            }
            break;
        case static_cast<int>(etp12::Datatypes::Protocol::eDataspaceOSDU):
            if (req_proto.m_role == "store") {
                return std::make_shared<OpenKVDataspaceOSDU>(std::move(repo), ora_session);
            }
            break;
        }
        return nullptr;
    };

    std::string version = "OSDU M27"; // To be updated at each release

    etp12::Datatypes::ServerCapabilities& cap = config.server_capabilities_;

    cap.m_applicationName = "ReservoirDDMS";
    cap.m_applicationVersion = version;
    cap.m_contactInformation.m_contactName = "laurent.deny@aspentech.com";

    etp12::Datatypes::Version v12;
    v12.m_major = 1;
    v12.m_minor = 2;
    v12.m_revision = 0;
    v12.m_patch = 0;

    // Warning: keep consistent with config.protocol_factory_ lambda above
    cap.m_supportedProtocols.emplace_back();
    cap.m_supportedProtocols.back().m_protocol = int(etp12::Datatypes::Protocol::eDataspace);
    cap.m_supportedProtocols.back().m_protocolVersion = v12;
    cap.m_supportedProtocols.back().m_role = "store";
    cap.m_supportedProtocols.back().m_protocolCapabilities.clear();

    cap.m_supportedProtocols.emplace_back();
    cap.m_supportedProtocols.back().m_protocol = int(etp12::Datatypes::Protocol::eDiscovery);
    cap.m_supportedProtocols.back().m_protocolVersion = v12;
    cap.m_supportedProtocols.back().m_role = "store";
    cap.m_supportedProtocols.back().m_protocolCapabilities.clear();

    cap.m_supportedProtocols.emplace_back();
    cap.m_supportedProtocols.back().m_protocol = int(etp12::Datatypes::Protocol::eSupportedTypes);
    cap.m_supportedProtocols.back().m_protocolVersion = v12;
    cap.m_supportedProtocols.back().m_role = "store";
    cap.m_supportedProtocols.back().m_protocolCapabilities.clear();

    cap.m_supportedProtocols.emplace_back();
    cap.m_supportedProtocols.back().m_protocol = int(etp12::Datatypes::Protocol::eStore);
    cap.m_supportedProtocols.back().m_protocolVersion = v12;
    cap.m_supportedProtocols.back().m_role = "store";
    cap.m_supportedProtocols.back().m_protocolCapabilities.clear();

    cap.m_supportedProtocols.emplace_back();
    cap.m_supportedProtocols.back().m_protocol = int(etp12::Datatypes::Protocol::eDataArray);
    cap.m_supportedProtocols.back().m_protocolVersion = v12;
    cap.m_supportedProtocols.back().m_role = "store";
    cap.m_supportedProtocols.back().m_protocolCapabilities.clear();

    cap.m_supportedProtocols.emplace_back();
    cap.m_supportedProtocols.back().m_protocol = int(etp12::Datatypes::Protocol::eTransaction);
    cap.m_supportedProtocols.back().m_protocolVersion = v12;
    cap.m_supportedProtocols.back().m_role = "store";
    cap.m_supportedProtocols.back().m_protocolCapabilities.clear();

    // OSDU protocols are at revision 1. For those protocol advertize the exact implementation
    // in the protocol capabilities. Once those protocols are part of a future standard checking
    // the version will be enough.
    v12.m_revision = 1;
    cap.m_supportedProtocols.emplace_back();
    cap.m_supportedProtocols.back().m_protocol = int(etp12::Datatypes::Protocol::eStoreOSDU);
    cap.m_supportedProtocols.back().m_protocolVersion = v12;
    cap.m_supportedProtocols.back().m_role = "store";
    getSupportedProtocolCapabilities<oes::common::etp12::Protocol::IStoreOSDU>(cap.m_supportedProtocols.back()) ;

    cap.m_supportedProtocols.emplace_back();
    cap.m_supportedProtocols.back().m_protocol = int(etp12::Datatypes::Protocol::eDataspaceOSDU);
    cap.m_supportedProtocols.back().m_protocolVersion = v12;
    cap.m_supportedProtocols.back().m_role = "store";
    getSupportedProtocolCapabilities<oes::common::etp12::Protocol::IDataspaceOSDU>(cap.m_supportedProtocols.back());

    cap.m_supportedProtocols.emplace_back();
    cap.m_supportedProtocols.back().m_protocol = int(etp12::Datatypes::Protocol::eCoreOSDU);
    cap.m_supportedProtocols.back().m_protocolVersion = v12;
    cap.m_supportedProtocols.back().m_role = "store";
    getSupportedProtocolCapabilities<oes::common::etp12::Protocol::ICoreOSDU>(cap.m_supportedProtocols.back());

    auto tmp = server_config_ptr->enableAnyNumberOfThreads();
    if (tmp != "true") {
        tmp = "false";
    }
    bool skip_check {false};
    std::istringstream(tmp) >> std::boolalpha >> skip_check;
    config.enable_any_number_of_threads_ = skip_check;
    
    config.session_keep_alive_duration_ = server_config_ptr->sessionKeepAliveDuration();

    config.server_config_ = server_config_ptr;

    return config;
}

//============================================================================

} // namespace oes::eml::openkv
