// ============================================================================
// Copyright 2019-2023 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <string>

#include <oes/core/utils/Guid.h>

namespace oes::postgresql {

enum class StatementType
{
    Prepared,
    PreparedWithCursor
};

class OES_POSTGRESQL_EXPORT Statement {
public:
    Statement() = delete;
    Statement(const Statement& stmt);
    Statement(Statement&& stmt) = default;

    [[nodiscard]] const std::string& getName() const;
    [[nodiscard]] const std::string& getSql() const;

    void bind(int columnIndex);
    void bind(int index, int16_t value);
    void bind(int index, int value);
    void bind(int index, int64_t value);
#if defined(__GNUC__) && (__WORDSIZE == 64)
    void bind(int index, long long int value);
#endif
    void bind(int index, bool value);
    void bind(int index, float value);
    void bind(int index, double value);
    void bind(int index, const char* value, int len);
    void bindBytea(int index, char* value, size_t len);
    void bindBytea(int index, const std::vector<const char*>& values, const std::vector<size_t>& lens);
    
    void bind(int index, const void* value, int len);
    void bind(int index, const std::string& value);
    void bind(int index, const oes::core::Guid& value);

    void bind(int index, const std::vector<oes::core::Guid>& value);
    template<typename T>
    void bind(int index, const std::vector<T>& value);
    template<typename T>
    void bind(int index, typename std::vector<T>::const_iterator value_beg, typename std::vector<T>::const_iterator value_end);
    void bindByteaArr(int index, const std::vector<const char*>& values, const std::vector<size_t>& lens);
    void bindByteaArray(int index, const std::vector<std::vector<uint8_t>>& bin_arr);
    void bindTextArray(int index, std::vector<std::string>::const_iterator value_beg, std::vector<std::string>::const_iterator value_end);
    void bindTextArray(int index, const std::vector<std::string>& value);

    int64_t execute();
    bool step();
    void reset();

    int getInt(int index);
    int64_t getInt64(int index);

    double getDouble(int index);
    float getFloat(int index);

    bool getBoolean(int index);

    const char* getString(int index, int& size);
    std::string getString(int index);

    const void* getBlob(int index, int& size);

    oes::core::Guid getGuid(int index);

    uint32_t getColumnType(int index) const;
    const char* getColumnName(int index);
    int getNumColumns() const;

    bool isNull(int index);

    int64_t getAffectedRows() const;

    std::string getTableName(int index) const;

    void close();

    // only makes sense for useCursor mode
    void setChunkSizeInRows(unsigned int chunkSizeInRows);

    class Impl;

private:
    Statement(
        const Connection_ptr& connection,
        std::string sql,
        std::string name,
        StatementType stmtType = StatementType::Prepared
    );

    friend Connection;

    std::shared_ptr<Impl> _impl;
};

} // namespace oes::postgresql
