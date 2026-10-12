
#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/core/utils/Logger.h>

#include <oes/common/auth/DelegateClient.h>
#include <oes/common/auth/DelegateClientException.h>

#include <cassert>

namespace oes::common::auth {

using oes::core::HTTP;

DelegateClient::DelegateClient(const DelegateClientConfig& config)
    : DelegateUrl_ {config.DelegateUrl_} {
    if (config.HTTP_factory_) {
        httpReq_ = config.HTTP_factory_();
    }
    if (!httpReq_) { // make sure we have a http pointer to make requests
        httpReq_ = std::make_shared<oes::core::HTTP>();
    }

    if (DelegateUrl_.empty()) {
        throw DelegateClientException("Authorization delegate url can not be empty.\n");
    }
}

std::string getPayload(const std::string& token) {
    std::string auth = token.substr(7);
    size_t pos = auth.find('.');
    size_t rpos = auth.rfind('.');
    if (pos > 0 && pos < auth.size() && pos < rpos) {
        return base64urlDecode(auth.substr(pos + 1, (rpos - pos - 1)));
    }
    return "";
}

AuthenticatedUser DelegateClient::getUser(const AuthInfo& auth_info, AuthDetails* p_details, bool error_on_missing_partition) const {
    // Fill the user with the content of the jwt payload first.
    const auto auth_hdr = auth_info.authentication_;
    auth::AuthenticatedUser aUser;
    if (auth_hdr.empty()) {
        aUser.ok_ = false;
        OES_ERROR("JWT is empty.");
        return aUser;
    }

    const auto payload = getPayload(auth_hdr);
    if( !payload.empty() ) {
        // This will get the name, email, etc, and more importantly the read-only flag.
        aUser = auth::AuthenticatedUser::decodeFromJson(payload, false, p_details);
        aUser.header_ = auth_hdr;
    }

    // send auth_hdr
    HTTP::Headers auth_headers;
    auth_headers.push_back("Authorization: " + auth_hdr);
    auth_headers.push_back("X-Etp-ClientId: " + p_details->client_id_);
    if (auth_info.partition_id_.empty()) {
        // If the partition Id can be found in the decoded user, we will default to
        // that later, so use it now.
        if (! aUser.partition_id_.empty() ) {
            OES_WARNING("PartitionID inferred from user token.");
            auth_headers.push_back("data-partition-id: " + aUser.partition_id_);
        } else {
            if(error_on_missing_partition) {
                aUser.ok_ = false;
                OES_ERROR("PartitionID empty in multipartition mode.");
                return aUser;
            } else {
                OES_WARNING("PartitionID empty in single mode");
            }
        }
    } else {
        auth_headers.push_back("data-partition-id: " + auth_info.partition_id_);
    }

    assert(httpReq_);
    auto res = httpReq_->get(DelegateUrl_, 0, auth_headers);

    if (res.code == 200) {
        aUser.ok_ = true;
        aUser.type_ = AuthType::eBearer;
    } else {
        const std::string error_str = "HTTP GET: " + DelegateUrl_ +
                                      "ERROR RESPONSE STATUS CODE: " + std::to_string(res.code) +
                                      " " + res.status;
        OES_INFO(error_str);
        aUser.ok_ = false;
    }
    
    return aUser;
}

bool DelegateClient::testDelegateUrl_get(const std::string& del_url, HTTP_ptr http_client, const short expected_code) {
    auto res = http_client->get(del_url, 0, HTTP::Headers());
    if (res.code != expected_code ) {
        const std::string error_str = "HTTP GET: " + del_url +
                                      "ERROR RESPONSE STATUS CODE: " + std::to_string(res.code) +
                                      " " + res.status;
        OES_ERROR("Error trying to reach the delegate server: {}", error_str);
        throw DelegateClientException(error_str.data());
    }
    return true;
}

bool DelegateClient::testDelegateUrl_post(const std::string& del_url, HTTP_ptr http_client, const short expected_code) {
    nlohmann::json j_body;
    j_body["operation"] = "none";
    j_body["dataUri"] = "dummy";

    auto res = http_client->post(del_url, 0, HTTP::Headers(), j_body.dump());
    if (res.code != expected_code ) {
        const std::string error_str = "HTTP POST: " + del_url +
                                      "ERROR RESPONSE STATUS CODE: " + std::to_string(res.code) +
                                      " " + res.status;
        OES_ERROR("Error trying to reach the delegate server: {}", error_str);
        //TODOthrow DelegateClientException(error_str.data());
    }
    return true;
}


} //namespace oes::common::auth