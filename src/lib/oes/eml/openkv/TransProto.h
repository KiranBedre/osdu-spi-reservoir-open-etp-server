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

#ifndef OES_EML_OPENKV_TRANSPROTO_H
#define OES_EML_OPENKV_TRANSPROTO_H

#include "../fwd.h"
#include "../export.h"
#include "BaseProto.h"

#include <oes/common/etp12/Protocols.h>
#include <thread>

namespace oes::eml::openkv {

using oes::common::etp12::Protocol::IRequest;
using oes::common::etp12::Protocol::EtpResponseHandler;

//============================================================================

struct OES_EML_EXPORT OpenKVTransProto : BaseProto, oes::common::etp12::Protocol::ITransaction
{
    OpenKVTransProto(OpenKVRepo_ptr repo, std::weak_ptr<OpenKVSession> session);

    void StartTransaction( // messageTypeId 1
        IRequest& ireq, StartTransaction_req_t req,
        EtpResponseHandler<StartTransaction_res_t> process_cb // messageTypeId 2
    ) final;

    void CommitTransaction( // messageTypeId 3
        IRequest& ireq, CommitTransaction_req_t req,
        EtpResponseHandler<CommitTransaction_res_t> process_cb // messageTypeId 5
    ) final;

    void RollbackTransaction( // messageTypeId 4
        IRequest& ireq, RollbackTransaction_req_t req,
        EtpResponseHandler<RollbackTransaction_res_t> process_cb // messageTypeId 6
    ) final;

    static std::mutex start_mutex_;
};

//============================================================================

/**
 * Helper class to manage a local transaction when no transaction is ongoing
 * It will create a new transaction if no transaction is ongoing
 * allows to commit its result but roll it back if the destructor is called without commit.
 * It will wait for the transaction to be ready for write if it is not ready yet, up to 5 times with an exponential backoff
 * of 300ms, 600ms, 1200ms, 2400ms, 4800ms
*/
template <typename T> class OES_EML_EXPORT LocalTransactionWriteRaii {
public:
    /**
     * @brief Construct a new Local Transaction Write Rtti object
     * @param proto OpenKVStore containing the session
     * @param message Message to be displayed in the logs
     * @param txSpaceUris List of dataspace URIs to be used in the transaction
    */
    LocalTransactionWriteRaii(T& proto, std::string message, std::vector<EmlUri> txSpaceUris) :
        proto_(proto),
        own_tx_(false),
        retries_(proto_.session()->threads_count()<2 ? 0: 20),
        retry_interval_(std::chrono::milliseconds(400)),
        factor_(1.5),
        max_interval_(std::chrono::milliseconds(2000)) {

        size_t retry_interval_ms = retry_interval_.count();
        const size_t max_interval_ms = max_interval_.count();
        if(proto_.session()->tx_==nullptr){
            num_tries_ = 0 ;
            try_duration_ = 0 ;
            while(retries_ > 0){
                {
                    std::lock_guard<std::mutex> lock(OpenKVTransProto::start_mutex_);
                    num_tries_++ ;
                    bool ready = true;
                    for (const auto& dataspaceUri : txSpaceUris){
                        if(!OpenKVTransaction::dataspaceWriteable(dataspaceUri.to_string())){
                            ready = false;
                            break;
                        }
                    }
                    if(ready){
                        own_tx_ = true;
                        proto_.newTx(false, std::move(message), std::move(txSpaceUris), true);
                        retries_ = 0;

                        if (proto_.session()->check_activities_) {
                            proto_.session()->tx_->trans_ctl_ = std::make_shared<OpenKVTransaction::TransactionCtlMap_t>(); // pass the dataspace?
                        }

                        break ;
                    }
                }
                if (static_cast<size_t>(retry_interval_ms * factor_) > max_interval_ms) {
                    factor_ = 1;
                    retry_interval_ms = max_interval_ms;
                }

                retries_--;
                // wait
                std::this_thread::sleep_for(std::chrono::milliseconds(retry_interval_ms));
                try_duration_ += retry_interval_ms ;
                retry_interval_ms = static_cast<size_t>(retry_interval_ms*factor_); 
            }   
        }
    }

    /**
     * Commit the transaction
    */
    void commit(){
        if(own_tx_){
            proto_.session()->tx_->db_transaction_.commit();
            proto_.resetTx();
            own_tx_ = false;
        }
    }

    bool ready(){
        return proto_.session()->tx_!=nullptr;
    }

    int numTries() const{
        return num_tries_ ;
    }

    double tryDuration() const{
        return double(try_duration_) / 1000.;
    }

    bool isLocal(){
        return own_tx_;
    }
    /**
     * Rollback the transaction if no commit has been called
    */
    ~LocalTransactionWriteRaii(){
        if(own_tx_){
            proto_.session()->tx_->db_transaction_.rollback();
            proto_.resetTx();
        }
    }

    std::shared_ptr<OpenKVTransaction::TransactionCtlMap_t> get_trans_control() {
        if (!ready()) {
            return nullptr;
        }
        return proto_.session()->tx_->trans_ctl_;
    }

private:
    T& proto_;
    bool own_tx_;
    int retries_;
    std::chrono::milliseconds retry_interval_;
    double factor_;
    std::chrono::milliseconds max_interval_;
    // For diagnostics.
    int num_tries_;
    size_t try_duration_;
};

} // namespace oes::eml::openkv

#endif//OES_EML_OPENKV_TRANSPROTO_H
