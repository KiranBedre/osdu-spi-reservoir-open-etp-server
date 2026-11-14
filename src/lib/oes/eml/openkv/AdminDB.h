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

#ifndef OES_EML_OPENKV_ADMINDB_H
#define OES_EML_OPENKV_ADMINDB_H

#include "../fwd.h"
#include "../export.h"

#include <oes/postgresql/Connection.h>
#include <oes/postgresql/Statement.h>

#include <chrono>
#include <map>
#include <optional>
namespace oes::eml::openkv {

//============================================================================

struct OES_EML_EXPORT Space {
    std::string uid_;
    std::string path_;

    // Extra-meta-data split into JSON and Avro (binary) parts,
    // since map values can now be quite complex (e.g. sparse arrays).
    // JSON part still has keys of Avro values, for visibility/troubleshooting.
    std::string xdata_json_;
    std::vector<uint8_t> xdata_avro_;

    std::string dbfile_; // implementation detail
    int64_t created_ = 0; // Microseconds since epoch
    int64_t last_write_ = 0; // Microseconds since epoch
    std::string size_str_; // Human-readable
    bool ro_ = false; // Indicates that the space is read-only

    bool isValid() const;
    explicit operator bool() const { return isValid(); }

    bool equals(const Space& rhs) const;
    bool lessThan(const Space& rhs) const;

    std::map<std::string, ::oes::common::etp12::Datatypes::DataValue> xdataAsMap() const;
    void setXDdataFromMap(const std::map<std::string, ::oes::common::etp12::Datatypes::DataValue>& xdata);

    std::optional<bool> canRead() const;
    std::optional<bool> canWrite() const;

    void setReadAccess(bool value) const;
    void setWriteAccess(bool value) const;

    static bool validatePath(
        const std::string& space_path,
        std::string& path, std::string& db_path,
        std::string& folder, std::string& name,
        std::string& err_msg
    );

    EmlUri uri() const;

protected:
    mutable bool read_access_ = false;
    mutable int64_t read_access_cache_updated_ = 0; // Seconds since epoch

    mutable bool write_access_ = false;
    mutable int64_t write_access_cache_updated_ = 0; // Seconds since epoch
};

inline bool operator==(const Space& lhs, const Space& rhs) {
    return lhs.equals(rhs);
}
inline bool operator!=(const Space& lhs, const Space& rhs) {
    return !lhs.equals(rhs);
}
inline bool operator<(const Space& lhs, const Space& rhs) {
    return lhs.lessThan(rhs);
}

inline void Space::setReadAccess(bool value) const{
    read_access_ = value;
    read_access_cache_updated_ = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

inline void Space::setWriteAccess(bool value) const{
    write_access_ = value;
    write_access_cache_updated_ = std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();
}

//============================================================================
using oes::common::etp12::Datatypes::ErrorInfo;
class OES_EML_EXPORT OpenKVAdminDB {
public:
    explicit OpenKVAdminDB(Connection_ptr conn);
    OpenKVAdminDB(OpenKVAdminDB&&) = default;
    OpenKVAdminDB& operator=(OpenKVAdminDB&&) = default;

    OpenKVAdminDB(const OpenKVAdminDB&) = delete;
    OpenKVAdminDB& operator=(const OpenKVAdminDB&) = delete;

    void reset_statements();

    //! \name Schema-related APIs
    //@{
    bool createSchema();
    bool upgradeSchema(int start_version, int target_version);
    bool isSchemaCreated() const;
    bool isSchemaUpToDate(bool version_only, int* current=nullptr, int* latest=nullptr) const;
    //@}

    //! \name ETP-Space-related APIs
    //@{
    size_t spaceCount() const;
    std::vector<Space> getSpaces(int64_t last_write_microseconds=0);
    Space findSpace(const std::string& uid_or_path);
    Space findSpaceByUid(const std::string& uid);
    Space findSpaceByPath(const std::string& path);
    bool addSpaces(const std::vector<Space>& new_spaces);
    bool dropSpaces(const std::vector<std::string>& uids_or_paths);
    ErrorInfo lockSpace(const Space& space, bool lock_space);
    bool spaceIsReadOnly(const Space& space);
    bool updateSpaceInfo(const Space& space);
    bool updateSpaceXDataJson(const Space& space);
    int64_t createSpaceReference(const Space& source, const Space& target);
    int64_t getSpaceReference(const Space& source, const Space& target);
    bool spaceIsReferenced(const Space& target);
    bool deleteSpaceReference(const Space& source, const Space& target);
    bool spaceIncludesReference(const Space& space);
    std::vector<ErrorInfo> importSpaces(const Space& destination, const std::vector<Space>& sources);
    void dropAdmin();
    //@}

    //! \name User-related APIs
    //@{
    size_t userCount() const;
    std::vector<std::string> getUserNames();
    bool userPasswordMatches(const std::string& name, const std::string& password);
    bool addUser(
        const std::string& name, const std::string& password,
        bool hash_password = true
    );
    //@}

    Connection_ptr handle() { return conn_; }

    int app_id() const;

private:
    using Statement_opt = std::optional<oes::postgresql::Statement>;
    void prepare(Statement_opt& stmt, std::string sql) const;
    void add_clone_schema_function(Connection_ptr conn);
    void add_update_space_rels_function(Connection_ptr conn);
    void add_import_space_content_function(Connection_ptr conn);
    void update_clone_schema_function_12(Connection_ptr conn);

    void update_array_summary_function_11(Connection_ptr conn);
    void update_import_space_content_function_10(Connection_ptr conn);

private:
    Connection_ptr conn_;

    Statement_opt select_one_user_;
    Statement_opt insert_user_;
    Statement_opt select_user_names_;

    Statement_opt select_space_by_uid_;
    Statement_opt select_space_by_path_;
    Statement_opt select_space_by_either_;
    Statement_opt select_spaces_;
    Statement_opt select_spaces_after_;
    Statement_opt insert_space_;
    Statement_opt delete_space_;
    Statement_opt lock_space_;
    Statement_opt select_space_ro_;
    Statement_opt update_space_info_;
    Statement_opt update_space_xdata_json_;
    Statement_opt space_is_referenced_;

    Statement_opt space_includes_reference_;
    Statement_opt create_space_reference_;
    Statement_opt check_space_reference_;
    Statement_opt delete_space_reference_;
};

//============================================================================

} // namespace oes::eml::openkv

#endif//OES_EML_OPENKV_ADMINDB_H
