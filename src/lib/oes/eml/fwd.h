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

#ifndef OES_EML_FWD_H
#define OES_EML_FWD_H

#include <oes/postgresql/fwd.h>
#include <oes/client/fwd.h>
#include <oes/common/fwd.h>

//============================================================================

namespace oes::eml::etp12 {

// etp12/Server.h
struct Handshake;
class  Session;
struct ServerConfig;
class  Server;

} // namespace oes::eml::etp12

//============================================================================

namespace oes::eml::openkv {

class SpaceReader;
class SpaceWriter;

using SpaceReader_ptr = std::shared_ptr<SpaceReader>;
using SpaceWriter_ptr = std::shared_ptr<SpaceWriter>;

struct OpenKVTransaction;
struct OpenKVSession;

class OpenKVRepo;
using OpenKVRepo_ptr = std::shared_ptr<OpenKVRepo>;
class  OpenKVAdminDB;
using  OpenKVAdminDB_ptr  = std::shared_ptr<OpenKVAdminDB>;
using  OpenKVAdminDB_uptr = std::unique_ptr<OpenKVAdminDB>;

// openkv/*Proto.h
struct OpenKVDataArray;
struct OpenKVDataspace;
struct OpenKVDiscovery;
struct OpenKVStore;
struct OpenKVSupportedTypes;
struct OpenKVTransaction;

// These types are extensively used in OpenKV headers
using ::oes::common::etp12::EmlUri;
using ::oes::common::etp12::Datatypes::ErrorInfo;
using ::oes::common::etp12::Datatypes::MessageHeader;
using ::oes::common::etp12::Datatypes::Object::Resource;
using ::oes::common::etp12::Protocol::EtpResponseStatus;

using Connection_ptr = std::shared_ptr<oes::postgresql::Connection>;
using DbConnectionPool_ptr = std::shared_ptr<oes::postgresql::DbConnectionPool>;

} // namespace oes::eml::openkv

//============================================================================

#endif//OES_EML_FWD_H
