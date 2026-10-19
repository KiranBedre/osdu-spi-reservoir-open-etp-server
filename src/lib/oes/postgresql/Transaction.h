// ============================================================================
// Copyright 2012-2023 Emerson Paradigm Holding LLC. All rights reserved.
// Copyright 2023 EPAM Systems
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
#include "export.h"
#include "fwd.h"


namespace oes::postgresql
{
    class OES_POSTGRESQL_EXPORT Transaction
    {
    public:
        explicit Transaction(Connection_ptr connection, bool readOnly = false);
        Transaction(Transaction&& transaction) noexcept;
        ~Transaction();

        Transaction(const Transaction&) = delete;
        Transaction& operator=(const Transaction&) = delete;
        Transaction& operator=(Transaction&&) = delete;

        void commit();
        void rollback();
        [[nodiscard]] const Connection_ptr& getConnection() const;

    private:
        Transaction() = default;
        void begin(bool readOnly = false);
        friend void swap(Transaction& first, Transaction& second) noexcept;

        Connection_ptr _connection;
        std::string    _savePointId;
        uint32_t       _debugId = 0;
        bool           _inTransaction = false;
    };

} //namespace oes::postgresql
