// ============================================================================
// Copyright 2018-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#ifndef oes_postgresql_fwd_h
#define oes_postgresql_fwd_h

#include <oes/core/utils/fwd.h>

#include <memory>
#include <vector>


extern "C" {

struct pg_conn;
struct pg_result;
typedef struct pg_conn PGconn;
typedef struct pg_result PGresult;
void PQclear(PGresult* res);
void PQfinish(PGconn* conn);

} // extern "C"

namespace oes::postgresql {

struct PGDeleterFunctor {
    void operator()(PGresult* r) {
        if (r != nullptr) {
            PQclear(r);
        }
    }
    void operator()(PGconn* c) {
        if (c != nullptr) {
            PQfinish(c);
        }
    }
};
using PGresult_uptr = std::unique_ptr<PGresult, PGDeleterFunctor>;
using PGconn_uptr   = std::unique_ptr<PGconn,   PGDeleterFunctor>;

class Connection;
using Connection_ptr = std::shared_ptr<Connection>;
using Connection_wptr = std::weak_ptr<Connection>;

class DbConnectionPool;
using DbConnectionPool_ptr = std::shared_ptr<DbConnectionPool>;

class Statement;
using Statement_ptr = std::shared_ptr<Statement>;

enum PGErrorCode : int;
enum NoticeSeverity : int;

namespace relational {
class PhysicalName;
typedef std::vector<PhysicalName> PhysicalNameVec;
} // namespace relational

} // namespace oes::postgresql

#endif
