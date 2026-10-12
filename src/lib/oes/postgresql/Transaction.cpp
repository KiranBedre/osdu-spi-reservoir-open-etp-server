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

#include <utility>
#include <atomic>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/postgresql/Transaction.h>

#include <oes/core/utils/Logger.h>
#include <oes/core/utils/Guid.h>
#include <oes/postgresql/Connection.h>


/*! \file oes/postgresql/Transaction.h
 * \brief PostgreSql transaction class.
 * \ingroup oes_postgresql
 */

namespace
{
    std::atomic_uint32_t nextDebugTransactionId = 0;
}

namespace oes::postgresql {

/*!
 * \class Transaction oes/postgresql/Transaction.h
 * \brief Creates PostgreSql transactions
 * \ingroup oes_postgresql
 * \details
 * RAI-wrapper of PostgreSql transaction, allows to automatically rollback on destruction.
 * Supports nested transactions via PostgreSql SAVEPOINT feature.
 */


/*!
 * \brief Constructs RAI wrapper of PostgreSql transaction and starts it automatically.
 * \param[in] connection PostgreSql connection
 * \param[in] readOnly if true, transaction will be read only
 * \see Connection::begin_transaction()
 */
Transaction::Transaction(Connection_ptr connection, bool readOnly)
    : _connection(std::move(connection))
    , _debugId(++nextDebugTransactionId)
{
    begin(readOnly);
}

/*! Move constructor */
Transaction::Transaction(Transaction&& transaction) noexcept
    : Transaction()
{
    swap(*this, transaction);
}

/*!
 * \brief Ends a transaction
 * \details
 * If the transaction has not been committed, it is automatically rolled-back.
 * \see rollback()
 */
Transaction::~Transaction()
{
    try {
        rollback();
    }
    catch (const std::exception& ex) {
        OES_ERROR("Transaction {:#04x}: Unable to rollback - {}", _debugId, ex.what());
    }
    catch (...) {
        OES_ERROR("Transaction {:#04x}: Unable to rollback - unknown exception", _debugId);
    }
}

/*! Starts a transaction
 * \param[in] readOnly if true, transaction will be read only
*/
void Transaction::begin(bool readOnly)
{
    if (_connection->transaction_status() == TransactionStatus::eActive)
    {
        OES_DEBUG("Transaction {:#04x}: begin nested transaction", _debugId);

        // Uses PostgreSql save points for emulate nested transactions
        _savePointId = "savepoint_" + core::Guid::make().toStringNoDash();
        _connection->execute("SAVEPOINT " + _savePointId);
    }
    else {
        OES_DEBUG("Transaction {:#04x}: begin {}"
            , _debugId
            , readOnly ? "(read only)" : ""
        );
        _connection->execute(readOnly ? "BEGIN READ ONLY" : "BEGIN");
    }
    _inTransaction = true;
}


/*! Commit a transaction */
void Transaction::commit()
{
    if (!_inTransaction) {
        return;
    }

    if (_savePointId.empty()) {
        OES_DEBUG("Transaction {:#04x}: commit", _debugId);
        _connection->execute("COMMIT");
    }
    else {
        OES_DEBUG("Transaction {:#04x}: commit nested transaction", _debugId);
        _connection->execute("RELEASE SAVEPOINT " + _savePointId);
    }
    _inTransaction = false;
}

/*! Rollback a transaction */
void Transaction::rollback()
{
    if (!_inTransaction) {
        return;
    }

    if (_savePointId.empty()) {
        OES_DEBUG("Transaction {:#04x}: rollback", _debugId);
        _connection->execute("ROLLBACK");
    }
    else {
        OES_DEBUG("Transaction {:#04x}: rollback nested transaction", _debugId);
        _connection->execute("ROLLBACK TO SAVEPOINT " + _savePointId);
    }
    _inTransaction = false;
}

/*! Returns DB connection */
const Connection_ptr& Transaction::getConnection() const
{
    return _connection;
}

/*! Swap transactions */
void swap(Transaction& first, Transaction& second) noexcept
{
    using std::swap;
    swap(first._connection, second._connection);
    swap(first._savePointId, second._savePointId);
    swap(first._debugId, second._debugId);
    swap(first._inTransaction, second._inTransaction);
}

} // namespace oes::postgresql
