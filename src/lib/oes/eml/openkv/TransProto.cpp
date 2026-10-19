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
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ============================================================================

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "TransProto.h"

#include "AuditLog.h"
#include "Repo.h"
#include "Session.h"
#include "SpaceDML.h"

#include "../etp12/Server.hpp"

#include <oes/core/utils/Assert.h>
#include <oes/core/utils/Logger.h>

#include <oes/common/AvroUtils.h>
#include <oes/common/auth/AuthN.h>
#include <oes/common/etp12/ErrorCodes.h>
#include <oes/core/utils/Metrics.h>
#include <oes/core/utils/MetricsHelper.h>
#include <oes/core/utils/MetricsNames.h>
#include <chrono>

using oes::eml::etp12::errorStatus;
using oes::common::etp12::Datatypes::EINVALID_STATE;
using oes::common::etp12::Datatypes::EMAX_TRANSACTIONS_EXCEEDED;
using oes::common::etp12::Datatypes::ENOTSUPPORTED;
using oes::common::etp12::Datatypes::EREQUEST_DENIED;
using oes::common::etp12::Datatypes::EINVALID_ARGUMENT;

namespace oes::eml::openkv {

//============================================================================

std::mutex OpenKVTransProto::start_mutex_;

OpenKVTransProto::OpenKVTransProto(OpenKVRepo_ptr repo, std::weak_ptr<OpenKVSession> session)
    : BaseProto(std::move(repo), session)
{ }

void OpenKVTransProto::StartTransaction(
    IRequest& ireq, StartTransaction_req_t req,
    EtpResponseHandler<StartTransaction_res_t> process_cb
) try {
    auto start = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(start_mutex_);
    
    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        OES_ERROR("SVR Session has been forcefully terminated.");
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::TRANSACTION_OPS_COUNTER_START_ERROR);
        return;
    }

    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);

    StartTransaction_res_t resp;
    oes::common::etp12::Protocol::EtpResponseStatus rc;

    if (session->tx_) {
        rc = errorStatus(EMAX_TRANSACTIONS_EXCEEDED, "Transaction already active");
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::TRANSACTION_OPS_COUNTER_START_ERROR);
        return process_cb(std::move(rc), std::move(resp));
    }

    if (!req.m_readOnly && session->user_ && session->user_->read_only_) {
        rc = errorStatus(EREQUEST_DENIED, "Cannot start RW transaction for RO user");
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::TRANSACTION_OPS_COUNTER_START_ERROR);
        return process_cb(std::move(rc), std::move(resp));
    }

    // Spec also mentions ELIMIT_EXCEED (12) error, based on
    // TransactionTimeout=X variable, where X is in seconds.

    // Prepare and validate list of involved dataspaces (an empty 'req.m_dataspaceUris' means all dataspaces)
    // ToDo: there is not supported one of ETP 1.2 requirement - case with empty 'dataspaceUris', which means the default dataspace (there is already deserialized model with list of spaces).
    std::vector<EmlUri> txSpaceUris;
    std::vector<std::string_view> invalidUris;
    std::vector<std::string_view> exceededTransactionUris;
    for (const auto& dataspaceUri : req.m_dataspaceUris)
    {
        auto emlUri = EmlUri::parse(dataspaceUri);
        if (emlUri.isValid() && emlUri.isSpace()) {
            // Check that dataspaces are writable in a Transaction
            if(!req.m_readOnly && !OpenKVTransaction::dataspaceWriteable(dataspaceUri)){
                exceededTransactionUris.push_back(dataspaceUri);
            }else{
                txSpaceUris.emplace_back(std::move(emlUri));
            }
        }
        else {
            invalidUris.emplace_back(dataspaceUri);
        }

    }

    if (!invalidUris.empty()) {
        rc = errorStatus(EINVALID_ARGUMENT, "Cannot start transaction, invalid dataspace URI(s): " + core::StringUtils::join(invalidUris, ", "));
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::TRANSACTION_OPS_COUNTER_START_ERROR);
        return process_cb(std::move(rc), std::move(resp));
    }

    if (!exceededTransactionUris.empty()) {
        rc = errorStatus(EMAX_TRANSACTIONS_EXCEEDED, "Cannot start transaction, too many write transaction URI(s): " + core::StringUtils::join(exceededTransactionUris, ", "));
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::TRANSACTION_OPS_COUNTER_START_ERROR);
        return process_cb(std::move(rc), std::move(resp));
    }

    try {
        newTx(req.m_readOnly, std::move(req.m_message), std::move(txSpaceUris), false );
    } catch (const oes::postgresql::PostgreSQLException& ex) {
        rc = errorStatus(EINVALID_STATE, fmt::format(
            "Cannot start Transaction: {}", ex.what()
        ));
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::TRANSACTION_OPS_COUNTER_START_ERROR);
        return process_cb(rc, std::move(resp));
    }

    if (session->check_activities_) {
        session->tx_->trans_ctl_ = std::make_shared<OpenKVTransaction::TransactionCtlMap_t>();
    }

    resp.m_successful = true;
    resp.m_failureReason.clear();
    oes::common::assignUid(resp.m_transactionUuid, session->tx_->uid_);

    // TODO: Record transaction in master table

    // On success - increment success counter and record latency
    oes::core::MetricsHelper::recordCounterWithLatency(oes::core::metrics::TRANSACTION_OPS_COUNTER_START_SUCCESS,oes::core::metrics::TRANSACTION_LATENCY_START,start);

    rc.ok = true;
    process_cb(rc, std::move(resp));
} catch (...) {
    // On error - increment error counter
    oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::TRANSACTION_OPS_COUNTER_START_ERROR);

    etp12::handleException(process_cb);
}

void OpenKVTransProto::CommitTransaction(
    IRequest& ireq, CommitTransaction_req_t req,
    EtpResponseHandler<CommitTransaction_res_t> process_cb
) try {
    auto start = std::chrono::steady_clock::now();
    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        OES_ERROR("SVR Session has been forcefully terminated.");
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::TRANSACTION_OPS_COUNTER_COMMIT_ERROR);
        return;
    }

    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);

    CommitTransaction_res_t resp;
    oes::common::etp12::Protocol::EtpResponseStatus rc;

    auto* tx = session->tx_.get();
    if (!tx) {
        AuditLog::log(*repo_->getServerConfig(), session, AuditLogConstants::OP_COMMIT_TRANSACTION, AuditLogConstants::RESULT_FAILURE, AuditLogConstants::ACTION_EXECUTE, "unknown", "Transaction commit failed: No active transaction");
        resp.m_successful = false;
        resp.m_failureReason = "No active transaction";
        rc.ok = true;
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::TRANSACTION_OPS_COUNTER_COMMIT_ERROR);
        return process_cb(std::move(rc), std::move(resp));
    }

    Guid req_tx_uid;
    oes::common::assignUid(req_tx_uid, req.m_transactionUuid);
    if (req_tx_uid != tx->uid_) {
        AuditLog::log(*repo_->getServerConfig(), session, AuditLogConstants::OP_COMMIT_TRANSACTION, AuditLogConstants::RESULT_FAILURE, AuditLogConstants::ACTION_EXECUTE, req_tx_uid.toXmlString(), fmt::format("Transaction {} commit failed: Unknown transaction uuid", req_tx_uid.toXmlString()));
        resp.m_successful = false;
        resp.m_failureReason = "Unknown transaction uuid";
        rc.ok = true;
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::TRANSACTION_OPS_COUNTER_COMMIT_ERROR);
        return process_cb(std::move(rc), std::move(resp));
    }

    resp.m_transactionUuid = req.m_transactionUuid;

    try {
        if(!tx->ro_){
            for(const EmlUri& dataspace: tx->dataspaceUris_){
                OES_VERBOSE(
                    "[{}] SVR Session({}) Tx: delete_null_res {}",
                    session->client_identification_, session->id(), dataspace.dataspace_
                );
                Space space = repo_->findSpaceInfo(dataspace.dataspace_);
                ErrorInfo err;
                auto writer = repo_->getSpaceWriter(space, err);
                if (writer) {
                    writer->delete_null_res();
                }
            }
        }
        for (auto& [space_uid, r] : tx->spaceUid_spaceReader_map_) {
            // Open blob prevents commit, with following error:
            // cannot commit transaction - SQL statements in progress
            r->close_blob();
        }

        if (!tx->spaceUid_spaceWriter_map_.empty()) {
            if (session->check_activities_ && tx->trans_ctl_) {// && tx->trans_ctl_->new_objs_uuids_.size() != 0) {
                for (auto& [space_uri, ctl] : *tx->trans_ctl_) {
                    Space space = repo_->findSpaceInfo(space_uri);
                    OES_VERBOSE(
                        "[{}] SVR Session({}) Tx: activity checks {}",
                        session->client_identification_, session->id(), space_uri
                    );
                    ErrorInfo err;
                    auto writer = repo_->getSpaceWriter(space, err);

                    if (!writer) {
                        OES_ERROR("Could not get writer for dataspace {}", space_uri);
                        continue;
                    }
                    if(ctl.upsertActivityObjs(*writer, session->userName(), space_uri)) {
                        writer->delete_null_res();
                    }
                }
            }

            OES_VERBOSE(
                "[{}] SVR Session({}) Tx: ref checks.",
                session->client_identification_, session->id()
            );

            size_t missing_ref_count = 0;
            size_t orphan_rel_count = 0;
            std::vector<std::string> missing_containers;
            for (auto& [space_uid, w] : tx->spaceUid_spaceWriter_map_) {
                // Each writer queries its own dataspace schema
                Space space = repo_->findSpaceInfo(space_uid);
                ErrorInfo err;
                if (!repo_->getSpaceConnection(space, err)) {
                    OES_ERROR("Could not switch to dataspace {}: {}", space_uid, err.m_message);
                    continue;
                }

                if (auto N = w->missing_reference_count()) {
                    // List the missing references in error log
                    const std::vector<ReferenceInfo>& missing_refs = w->get_references_infos();
                    for (const auto& ref : missing_refs) {
                        OES_ERROR(
                            "Missing reference in transaction: {}", ref.to_string()
                        );
                    }
                    missing_ref_count += N;
                }
                orphan_rel_count += w->orphan_relation_count();

                for (auto& container : w->check_pending_ary_containers()) {
                    missing_containers.push_back(std::move(container));
                }
            }

            if (missing_ref_count) {
                resp.m_successful = false;
                resp.m_failureReason = fmt::format("{} Missing reference(s)", missing_ref_count);
                rc.ok = true;
                oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::TRANSACTION_OPS_COUNTER_COMMIT_ERROR);
                return process_cb(std::move(rc), std::move(resp));
            }

            if (orphan_rel_count) {
                resp.m_successful = false;
                resp.m_failureReason = fmt::format("{} Orphan reference(s)", orphan_rel_count);
                rc.ok = true;
                oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::TRANSACTION_OPS_COUNTER_COMMIT_ERROR);
                return process_cb(std::move(rc), std::move(resp));
            }

            if(!missing_containers.empty()) {
                rc = errorStatus(EINVALID_STATE, fmt::format("Missing array container(s): {}", oes::core::StringUtils::join(missing_containers, ", ")));
                oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::TRANSACTION_OPS_COUNTER_COMMIT_ERROR);
                return process_cb(std::move(rc), std::move(resp));
            }

            // Open blob prevents commit, with following error:
            // cannot commit transaction - SQL statements in progress
            for (auto& [space_uid, w] : tx->spaceUid_spaceWriter_map_) {
                w->close_blob();
            }
        }

        OES_VERBOSE(
            "[{}] SVR Session({}) Tx: array checks.",
            session->client_identification_, session->id()
        );
        if (auto N = tx->missing_arys_.size()) {
            for(auto& [key, cnt] : tx->missing_arys_){
                OES_ERROR("Missing array: {} in {}", key.second, key.first.toXmlString());
            }
            AuditLog::log(*repo_->getServerConfig(), session, AuditLogConstants::OP_COMMIT_TRANSACTION, AuditLogConstants::RESULT_FAILURE, AuditLogConstants::ACTION_EXECUTE, req_tx_uid.toXmlString(), fmt::format("Transaction {} commit failed: {} Missing array(s)", req_tx_uid.toXmlString(), N));
            resp.m_successful = false;
            resp.m_failureReason = fmt::format("{} Missing array(s)", N);
            rc.ok = true;
            oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::TRANSACTION_OPS_COUNTER_COMMIT_ERROR);
            return process_cb(std::move(rc), std::move(resp));
        }

        if (auto N = tx->orphan_arys_.size()) {
            AuditLog::log(*repo_->getServerConfig(), session, AuditLogConstants::OP_COMMIT_TRANSACTION, AuditLogConstants::RESULT_FAILURE, AuditLogConstants::ACTION_EXECUTE, req_tx_uid.toXmlString(), fmt::format("Transaction {} commit failed: {} Orphan array(s)", req_tx_uid.toXmlString(), N));
            resp.m_successful = false;
            resp.m_failureReason = fmt::format("{} Orphan array(s)", N);
            rc.ok = true;
            oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::TRANSACTION_OPS_COUNTER_COMMIT_ERROR);
            return process_cb(std::move(rc), std::move(resp));
        }

        OES_VERBOSE(
            "[{}] SVR Session({}) Tx: DB Commit.",
            session->client_identification_, session->id()
        );
        tx->db_transaction_.commit();

        OES_VERBOSE(
            "[{}] SVR Session({}) Tx: Finalize.",
            session->client_identification_, session->id()
        );
        if(!tx->ro_){
            for(const EmlUri& dataspace: tx->dataspaceUris_){
                Space space = repo_->findSpaceInfo(dataspace.dataspace_);
                if(repo_->updateSpaceInfo(space)){
                    session = session_.lock().get();
                    if(session != nullptr) {
                        OES_INFO(
                            "[{}] SVR Session({}) Updated dataspace info: {}",
                            session->client_identification_, session->id(), dataspace.dataspace_
                        );
                    }
                }
            }
        }

        resp.m_successful = true;
        resp.m_failureReason.clear();
        
        AuditLog::log(*repo_->getServerConfig(), session, AuditLogConstants::OP_COMMIT_TRANSACTION, AuditLogConstants::RESULT_SUCCESS, AuditLogConstants::ACTION_EXECUTE, req_tx_uid.toXmlString(), fmt::format("Transaction {} committed successfully", req_tx_uid.toXmlString()));

        resetTx();
    } catch (const oes::postgresql::PostgreSQLException& ex) {
        AuditLog::log(*repo_->getServerConfig(), session, AuditLogConstants::OP_COMMIT_TRANSACTION, AuditLogConstants::RESULT_FAILURE, AuditLogConstants::ACTION_EXECUTE, req_tx_uid.toXmlString(), fmt::format("Transaction {} commit failed: Cannot COMMIT Transaction: {}", req_tx_uid.toXmlString(), ex.what()));
        resp.m_successful = false;
        resp.m_failureReason = fmt::format(
            "Cannot COMMIT Transaction: {}", ex.what()
        );
        // Note that we do NOT session_.resetTx(), meaning that the client
        // can decide to RollbackTransaction(), or do more PUTs and retry
        // CommitTransaction perhaps.
    }

    // FIXME: Normally in case of DatabaseException, we'd return an error,
    // with rc.ok = false, except CommitTransaction_res_t has "built-in"
    // .m_successful and .m_failureReason which duplicate ProtocolException.
    // So it's ambiguous what should be done...
    
    // On success - increment success counter and record latency
    oes::core::MetricsHelper::recordCounterWithLatency(oes::core::metrics::TRANSACTION_OPS_COUNTER_COMMIT_SUCCESS,oes::core::metrics::TRANSACTION_LATENCY_COMMIT,start);

    rc.ok = true;
    process_cb(rc, std::move(resp));
} catch (...) {
    // On error - increment error counter
    oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::TRANSACTION_OPS_COUNTER_COMMIT_ERROR);

    etp12::handleException(process_cb);
}

void OpenKVTransProto::RollbackTransaction(
    IRequest& ireq, RollbackTransaction_req_t req,
    EtpResponseHandler<RollbackTransaction_res_t> process_cb
) try {
    auto start = std::chrono::steady_clock::now();
    OpenKVSession* session = session_.lock().get();
    if(session == nullptr) {
        OES_ERROR("SVR Session has been forcefully terminated.");
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::TRANSACTION_OPS_COUNTER_ROLLBACK_ERROR);
        return;
    }
    OES_RELEASE_UNUSED(ireq);
    assert(typeOf(ireq) == req.info.type_);

    RollbackTransaction_res_t resp;
    oes::common::etp12::Protocol::EtpResponseStatus rc;

    if (!session->tx_) {
        AuditLog::log(*repo_->getServerConfig(), session, AuditLogConstants::OP_ROLLBACK_TRANSACTION, AuditLogConstants::RESULT_FAILURE, AuditLogConstants::ACTION_EXECUTE, "unknown", "Transaction rollback failed: No active transaction");
        resp.m_successful = false;
        resp.m_failureReason = "No active transaction";
        rc.ok = true;
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::TRANSACTION_OPS_COUNTER_ROLLBACK_ERROR);
        return process_cb(std::move(rc), resp);
    }

    Guid req_tx_uid;
    oes::common::assignUid(req_tx_uid, req.m_transactionUuid);
    if (req_tx_uid != session->tx_->uid_) {
        AuditLog::log(*repo_->getServerConfig(), session, AuditLogConstants::OP_ROLLBACK_TRANSACTION, AuditLogConstants::RESULT_FAILURE, AuditLogConstants::ACTION_EXECUTE, req_tx_uid.toXmlString(), fmt::format("Transaction {} rollback failed: Unknown transaction uuid", req_tx_uid.toXmlString()));
        resp.m_successful = false;
        resp.m_failureReason = "Unknown transaction uuid";
        rc.ok = true;
        oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::TRANSACTION_OPS_COUNTER_ROLLBACK_ERROR);
        return process_cb(std::move(rc), resp);
    }

    resp.m_transactionUuid = req.m_transactionUuid;

    try {
        for (auto& [space_uid, r] : session->tx_->spaceUid_spaceReader_map_) {
            r->close_blob();
        }
        for (auto& [space_uid, w] : session->tx_->spaceUid_spaceWriter_map_) {
            w->close_blob();
        }

        session->tx_->db_transaction_.rollback();

        AuditLog::log(*repo_->getServerConfig(), session, AuditLogConstants::OP_ROLLBACK_TRANSACTION, AuditLogConstants::RESULT_SUCCESS, AuditLogConstants::ACTION_EXECUTE, req_tx_uid.toXmlString(), fmt::format("Transaction {} rolled back successfully", req_tx_uid.toXmlString()));
        resp.m_successful = true;
        resp.m_failureReason.clear();
    } catch (const oes::postgresql::PostgreSQLException& ex) {
        AuditLog::log(*repo_->getServerConfig(), session, AuditLogConstants::OP_ROLLBACK_TRANSACTION, AuditLogConstants::RESULT_FAILURE, AuditLogConstants::ACTION_EXECUTE, req_tx_uid.toXmlString(), fmt::format("Transaction {} rollback failed: Cannot ROLLBACK Transaction: {}", req_tx_uid.toXmlString(), ex.what()));
        resp.m_successful = false;
        resp.m_failureReason = fmt::format(
            "Cannot ROLLBACK Transaction: {}", ex.what()
        );
    }

    // unlike in CommitTransaction, on RollbackTransaction we always
    // consider the TX "dead", so systematically reset it.
    resetTx();

    // FIXME: Normally in case of DatabaseException, we'd return an error,
    // with rc.ok = false, except RollbackTransaction_res_t has "built-in"
    // .m_successful and .m_failureReason which duplicate ProtocolException.
    // So it's ambiguous what should be done...
    
    // On success - increment success counter and record latency
    oes::core::MetricsHelper::recordCounterWithLatency(oes::core::metrics::TRANSACTION_OPS_COUNTER_ROLLBACK_SUCCESS,oes::core::metrics::TRANSACTION_LATENCY_ROLLBACK,start);

    rc.ok = true;
    process_cb(rc, std::move(resp));
} catch (...) {
    // On error - increment error counter
    oes::core::MetricsHelper::updateCounterMetric(oes::core::metrics::TRANSACTION_OPS_COUNTER_ROLLBACK_ERROR);

    etp12::handleException(process_cb);
}

//============================================================================

} // namespace oes::eml::openkv
