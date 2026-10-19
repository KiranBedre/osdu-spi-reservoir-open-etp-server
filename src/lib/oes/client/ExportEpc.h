// ============================================================================
// Copyright 2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#ifndef OES_CLIENT_ETP12_EXPORTEPC_H
#define OES_CLIENT_ETP12_EXPORTEPC_H

#include "export.h"
#include "Client.h"
#include "ClientUtils.h"

#include <chrono>
#include <iosfwd>

namespace oes::client {

struct OES_CLIENT_EXPORT ExportEpcOptions: EpcOptions {

    size_t max_queue_depth_ = 10;
    bool overwrite_ = false;

    unsigned int thread_count_ = 0;

    // HDF5 compression. valid range: [0 _ 9].
    // 0 none; 1 fast but bigger; 9 best but slower
    int hdf5_compression_ = 3;
    int hdf5_large_data_compression_ = 3;

};

OES_CLIENT_EXPORT
bool exportEpc(
    Client& client,
    const std::string& etp_space,
    const std::string& epc_filename,
    const std::string& hdf_filename,
    const ExportEpcOptions& options = {},
    EpcStatistics* p_stats = nullptr
);

OES_CLIENT_EXPORT
bool exportEpcMt(
    Client& client,
    const std::string& etp_space,
    const std::string& epc_filename,
    const std::string& hdf_filename,
    const ExportEpcOptions& options = {},
    EpcStatistics* p_stats = nullptr
);

} // namespace oes::client

#endif
