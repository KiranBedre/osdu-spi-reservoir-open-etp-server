// ============================================================================
// Copyright 2016-2021 Emerson Paradigm Holding LLC. All rights reserved.
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
#pragma once

#include "EmlAppContext.h"

#include <oes/client/Client.h>
#include <oes/common/etp12/Protocols.h>
#include <oes/core/utils/Logger.h>

#include <chrono>
#include <memory>

namespace oes::apps::eml {

//!namespace etp = oes::eml::etp12;
namespace etpc = oes::client;
using oes::common::etp12::Datatypes::Object::Dataspace;

//============================================================================

struct EtpDefaults {
    std::string timeout_ = "10s,10s";
    std::string max_msg_size_ = "1MB";

    int max_batch_size_ = 100;

    bool compress_ = false;
    std::string zcompressors_ = "gzip"; // TODO(unknown): lz4, zstd
};

options_description connect_options(unsigned terminalWidth, EtpDefaults defaults = {});
options_description authenticate_options(unsigned terminalWidth);
options_description osdu_partition_options(unsigned terminalWidth);

//============================================================================

struct EtpClient12 {
    const EmlAppContext& ctx_;
    etpc::ClientConfig cfg_;
    std::unique_ptr<etpc::Client> clt_;

    std::chrono::milliseconds connect_timeout_;
    std::chrono::milliseconds put_get_timeout_;

    explicit EtpClient12(const EmlAppContext& ctx);

    bool config();
    bool connect();

    std::vector<Dataspace> globSpaces(
        const std::string& pattern, size_t* total = nullptr
    );
    std::vector<Dataspace> globSpacesSince(
        const std::string& pattern, int64_t lastChangedFilter = 0, size_t* total = nullptr
    );

    template <typename TProto>
    TProto* get_proto() {
        assert(clt_);
        if (auto* proto = clt_->get_proto<TProto>()) {
            return proto;
        }
        OES_ERROR(cfg_.server_url_ + ": No " + TProto::protocolName() + " protocol support");
        return nullptr;
    }

    auto core_proto()       { return get_proto<etpc::proto12::ICore>(); }
    auto store_proto()      { return get_proto<etpc::proto12::IStore>(); }
    auto store_proto_osdu() { return get_proto<etpc::proto12::IStoreOSDU>(); }
    auto array_proto()      { return get_proto<etpc::proto12::IDataArray>(); }
    auto disco_proto()      { return get_proto<etpc::proto12::IDiscovery>(); }
    auto types_proto()      { return get_proto<etpc::proto12::ISupportedTypes>(); }
    auto space_proto()      { return get_proto<etpc::proto12::IDataspace>(); }
    auto space_proto_osdu() { return get_proto<etpc::proto12::IDataspaceOSDU>(); }
    auto trans_proto()      { return get_proto<etpc::proto12::ITransaction>(); }
    auto core_proto_osdu()  { return get_proto<etpc::proto12::ICoreOSDU>(); }
};

//============================================================================

} // namespace oes::apps::eml
