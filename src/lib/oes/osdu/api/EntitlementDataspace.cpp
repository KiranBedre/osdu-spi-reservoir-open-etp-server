#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <nlohmann/json.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/join.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/osdu/api/EntitlementDataspace.h>
#include <oes/osdu/api/OsduClientException.h>

#include <oes/core/utils/Json.hpp>
#include <oes/core/utils/Logger.h>
#include <oes/core/utils/Guid.h>
#include <oes/core/utils/String.h>
#include <oes/core/utils/Otel.h>

#include <oes/osdu/api/BaseClient.h>

using oes::core::Guid;

namespace oes::osdu::api {

EntitlementDataspace::EntitlementDataspace() :
    entl_type_ (EntitlementDSType::eBase) {}

EntitlementDataspace::EntitlementDataspace(const EntitlementDSType& type) :
    entl_type_ (type) {}

EntitlementDataspace::EntitlementDataspace(
    const std::string& uid, const std::string& path,
    const std::vector<std::string>& legal_tags, const std::vector<std::string>& legal_countries) :
    uid_(uid),
    path_(path),
    legal_tags_(legal_tags),
    legal_countries_(legal_countries),
    entl_type_(EntitlementDSType::eBase) {}

EntitlementDataspace::EntitlementDataspace(
    const std::string& uid, const std::string& path,
    const std::vector<std::string>& legal_tags, const std::vector<std::string>& legal_countries,
    const EntitlementDSType& type) :
    uid_(uid),
    path_(path),
    legal_tags_(legal_tags),
    legal_countries_(legal_countries),
    entl_type_(type) {}

/* Id valid for URL and email used in a URL*/
std::string EntitlementDataspace::urlId() const{
    
    std::string id = uid_;
    std::replace(id.begin(),id.end(),'/','-');
    id = oes::core::StringUtils::urlEncode(id);
    std::replace(id.begin(),id.end(),'%','_');


    // String too long, create a substitute using deterministic uid to avoid collision
    if(id.length()>64){
        const Guid guid = Guid::make_deterministic(Guid::dns_namespace(), id);
        id = id.substr(0,32) + guid.toStringNoDash();
    }
    return id;
}

bool EntitlementDataspace::isValid() const {
    return !path_.empty() && !uid_.empty();
}

EntitlementDSType EntitlementDataspace::getEntitlementType() const {
    return entl_type_;
}


// Groups

const std::string EntitlementDataspaceGroups::DATASET_TYPE("dataset--ETPDataspace");
const std::string EntitlementDataspaceGroups::VERSION("1.0.0");

EntitlementDataspaceGroups::EntitlementDataspaceGroups() :
    EntitlementDataspace(EntitlementDSType::eGroups) {}

EntitlementDataspaceGroups::EntitlementDataspaceGroups(
    const std::string& uid, const std::string& path,
    const std::vector<std::string>& legal_tags, const std::vector<std::string>& legal_countries,
    const std::vector<std::string>& viewers, const std::vector<std::string>& owners) :
    EntitlementDataspace(uid, path, legal_tags, legal_countries, EntitlementDSType::eGroups),
    viewers_group_(viewers.begin(),viewers.end()),
    owners_group_(owners.begin(),owners.end()){
}

bool EntitlementDataspaceGroups::equals(const EntitlementDataspaceGroups& rhs) const {
    return std::tie(uid_, path_) < std::tie(rhs.uid_, rhs.path_);
}

bool EntitlementDataspaceGroups::lessThan(const EntitlementDataspaceGroups& rhs) const {
    return std::tie(uid_, path_) < std::tie(rhs.uid_, rhs.path_);
}

std::string EntitlementDataspaceGroups::idForQuery(const EntitlementConfigBase& config_osdu) const {
    //"namespace:dataset--ETPDataspace:bb2f2ebb-eb23-5a21-af06-d6ba19f63626"
    return config_osdu.dataPartitionId() + ":" + DATASET_TYPE + ":" + urlId();
}

std::string EntitlementDataspaceGroups::serialize(const EntitlementConfigBase& config_osdu) const {
    nlohmann::json j;
    j["kind"] = config_osdu.schemaId() + ":wks:" + DATASET_TYPE + ":" + VERSION;
    j["acl"]["viewers"] = std::vector<std::string>(viewers_group_.begin(), viewers_group_.end());
    j["acl"]["owners"] = std::vector<std::string>(owners_group_.begin(), owners_group_.end());
    if(legal_tags_.size()>0){
        j["legal"]["legaltags"] = legal_tags_;
    }else if(!config_osdu.legalTags().empty()){
        j["legal"]["legaltags"] = std::vector<std::string> {config_osdu.legalTags()};
    }else{
        j["legal"]["legaltags"] = std::vector<std::string>();
    }
    if(legal_countries_.size()>0){
        j["legal"]["otherRelevantDataCountries"] = legal_countries_;
    }else if(!config_osdu.legalCountries().empty()){
        j["legal"]["otherRelevantDataCountries"] = std::vector<std::string> {config_osdu.legalCountries()};
    }else{
        j["legal"]["otherRelevantDataCountries"] = std::vector<std::string>();
    }
    j["legal"]["status"] = "compliant";
    auto uri = std::string("eml:///dataspace(") + path_ + ")";
    j["data"]["DatasetProperties"]["URI"] = uri;
    j["id"] = idForQuery(config_osdu);
    // put to array
    nlohmann::json jtop = nlohmann::json::array();
    jtop.push_back(j);
    //DBG
    //std::cout << "DBG EntitlementDataspace::serialize:----\n" << jtop.dump(2) << "DBG:----\n";
    //DBG
    return jtop.dump();
}

bool EntitlementDataspaceGroups::deserialize(const std::string& res_body) {
    try {
        nlohmann::json j = nlohmann::json::parse(res_body);
        if (j.contains("acl")) {
            auto& jacl = j["acl"];
            auto get_acl_groups = [&jacl](const std::string& permission, std::set<std::string>& groups) {
                groups.clear();
                if (jacl.contains(permission)) {
                    auto values = jacl[permission].get<std::vector<std::string>>();
                    // TODO temporary get first
                    if (!values.empty()) {
                        groups.insert(values.begin(), values.end());
                    }
                }
            };
            get_acl_groups("viewers", viewers_group_);
            get_acl_groups("owners", owners_group_);
        }
    } catch (const std::exception& ex) {
        OES_ERROR("Failed to parse response body while get dataspace record.\n{}", ex.what());
        return false;
    } catch (...) {
        OES_ERROR("Failed to parse response body while get dataspace record.");
        return false;
    }
    return true;
}

bool EntitlementDataspaceGroups::hasPermission(
    const std::string& permission, const std::set<std::string>& userGroups
) const {
    if (userGroups.empty()) {
        return false;
    }
    const auto& acl = ('v' == permission[0]) ? viewers_group_ : owners_group_;

    for( const std::string& g: acl ){
        if(userGroups.find(g)!=userGroups.end()){
            return true;
        }
    }
    return false;
}

void EntitlementDataspaceGroups::addGroup(const std::string& permission, const std::string& groupName) {
    auto& acl = ('v' == permission[0]) ? viewers_group_ : owners_group_;
    acl.insert(groupName);
}

bool EntitlementDataspaceGroups::checkGroups(const std::set<std::string>& osduGroups) const {
    if (osduGroups.empty()) {
        OES_WARNING("checkGroups: user's group list is empty - listGroups may have failed");
        return false;
    }

    for (const std::string& group : owners_group_) {
        if (osduGroups.find(group) == osduGroups.end()) {
            OES_WARNING("checkGroups: required owner group not found in user's {} groups for dataspace '{}'",
                osduGroups.size(), path_);
            return false;
        }
    }
    return true;
}

// GroupsBulk

const size_t EntitlementDataspaceGroupsBulk::PAGE_SIZE = 100; // storage service maximum

EntitlementDataspaceGroupsBulk::EntitlementDataspaceGroupsBulk() :
    EntitlementDataspace(EntitlementDSType::eGroupsBulk) {}

EntitlementDataspaceGroupsBulk::EntitlementDataspaceGroupsBulk(std::vector<EntitlementDataspaceGroups_ptr> dataspaces) :
    EntitlementDataspace(EntitlementDSType::eGroupsBulk),
    dataspaces_(std::move(dataspaces)) {}

std::string EntitlementDataspaceGroupsBulk::idForQuery(const EntitlementConfigBase& /*config_osdu*/) const {
    return "bulk";
}

std::string EntitlementDataspaceGroupsBulk::queryParams(
    const EntitlementConfigBase& config_osdu, const std::string& cursor) const {
    const std::string kind = config_osdu.schemaId() + ":wks:" +
        EntitlementDataspaceGroups::DATASET_TYPE + ":" + EntitlementDataspaceGroups::VERSION;
    std::string query = "?kind=" + oes::core::StringUtils::urlEncode(kind) + "&limit=" + std::to_string(PAGE_SIZE);
    if (!cursor.empty()) {
        query += "&cursor=" + oes::core::StringUtils::urlEncode(cursor);
    }
    return query;
}

std::unordered_map<std::string, size_t> EntitlementDataspaceGroupsBulk::indexById(
    const EntitlementConfigBase& config_osdu) const {
    std::unordered_map<std::string, size_t> index_by_id;
    index_by_id.reserve(dataspaces_.size());
    for (size_t i = 0; i < dataspaces_.size(); ++i) {
        index_by_id.emplace(dataspaces_[i]->idForQuery(config_osdu), i);
    }
    return index_by_id;
}

bool EntitlementDataspaceGroupsBulk::deserialize(
    const std::string& res_body,
    const std::unordered_map<std::string, size_t>& index_by_id,
    std::vector<bool>& found,
    std::string& cursor) const {
    cursor.clear();
    try {
        const nlohmann::json j = nlohmann::json::parse(res_body);
        const auto& results = j.at("results");
        if (!results.is_array()) {
            throw OsduClientException("results is not an array.");
        }
        for (const auto& rec : results) {
            const auto it_id = rec.find("id");
            if (it_id == rec.end() || !it_id->is_string()) {
                continue;
            }
            const auto it = index_by_id.find(it_id->get<std::string>());
            if (it == index_by_id.end()) {
                continue; // record of another dataspace
            }
            if (dataspaces_[it->second]->deserialize(rec.dump())) {
                found[it->second] = true;
            }
        }
        // Some storage versions still return a cursor on the last page
        const auto it_cursor = j.find("cursor");
        if (!results.empty() && it_cursor != j.end() && it_cursor->is_string()) {
            cursor = it_cursor->get<std::string>();
        }
    } catch (const std::exception& ex) {
        OES_ERROR("Failed to parse response body while listing dataspace records.\n{}", ex.what());
        return false;
    } catch (...) {
        OES_ERROR("Failed to parse response body while listing dataspace records.");
        return false;
    }
    return true;
}

// Policy

const std::string EntitlementDataspacePolicy::DATASET_TYPE("dataset--ETPDataspace");
const std::string EntitlementDataspacePolicy::VERSION("1.0.0");

EntitlementDataspacePolicy::EntitlementDataspacePolicy() :
    EntitlementDataspace(EntitlementDSType::ePolicy) {}

EntitlementDataspacePolicy::EntitlementDataspacePolicy(
    const std::string& uid, const std::string& path, const std::string& uri,
    const std::vector<std::string>& legal_tags, const std::vector<std::string>& legal_countries,
    const PermissionMap& usersPermission_map) :
    EntitlementDataspace(uid, path, legal_tags, legal_countries, EntitlementDSType::ePolicy),
    usersPermissions_(usersPermission_map.begin(), usersPermission_map.end()) {
        uri_ = uri;
    }

bool EntitlementDataspacePolicy::equals(const EntitlementDataspacePolicy& rhs) const {
    return std::tie(uid_, path_) < std::tie(rhs.uid_, rhs.path_);
}

bool EntitlementDataspacePolicy::lessThan(const EntitlementDataspacePolicy& rhs) const {
    return std::tie(uid_, path_) < std::tie(rhs.uid_, rhs.path_);
}

std::string EntitlementDataspacePolicy::idForQuery(const EntitlementConfigBase& config_osdu) const {
    //"namespace:dataset--ETPDataspace:bb2f2ebb-eb23-5a21-af06-d6ba19f63626"
    return config_osdu.dataPartitionId() + ":" + DATASET_TYPE + ":" + urlId();
}

std::string EntitlementDataspacePolicy::serialize(const EntitlementConfigBase& config_osdu) const {
    nlohmann::json j;
    j["kind"] = config_osdu.schemaId() + ":wks:" + DATASET_TYPE + ":" + VERSION;
    if(legal_tags_.size()>0){
        j["legal"]["legaltags"] = legal_tags_;
    }else if(!config_osdu.legalTags().empty()){
        j["legal"]["legaltags"] = std::vector<std::string> {config_osdu.legalTags()};
    }else{
        j["legal"]["legaltags"] = std::vector<std::string>();
    }
    if(legal_countries_.size()>0){
        j["legal"]["otherRelevantDataCountries"] = legal_countries_;
    }else if(!config_osdu.legalCountries().empty()){
        j["legal"]["otherRelevantDataCountries"] = std::vector<std::string> {config_osdu.legalCountries()};
    }else{
        j["legal"]["otherRelevantDataCountries"] = std::vector<std::string>();
    }
    j["legal"]["status"] = "compliant";
    j["data"]["DatasetProperties"]["URI"] = uri_;
    j["id"] = idForQuery(config_osdu);
    // put to array
    nlohmann::json jtop = nlohmann::json::array();
    jtop.push_back(j);
    //DBG
    //std::cout << "DBG EntitlementDataspace::serialize:----\n" << jtop.dump(2) << "DBG:----\n";
    //DBG
    return jtop.dump();
}

std::string EntitlementDataspacePolicy::serializePolicy(
    const EntitlementConfigBase& config_osdu,
    const DSPermissionFlags& operation) const {
    std::vector<std::string> legalTags;
    if (!config_osdu.legalTags().empty()) {
        legalTags = std::vector<std::string>{config_osdu.legalTags()};
    }
    
    std::vector<std::string> other;
    if (!config_osdu.legalCountries().empty()) {
        other = std::vector<std::string>{config_osdu.legalCountries()};
    }

    const auto op_name = getPermissionName(operation);
    
    // add input operation
    nlohmann::json j;
    j["input"]["operation"] = op_name;

    nlohmann::json j_record = nlohmann::json::parse(serialize(config_osdu));
    j["input"]["records"] = j_record;

    return j.dump();
}

std::string EntitlementDataspacePolicy::serializeSimplePolicy(
    const DSPermissionFlags& operation) const {
    nlohmann::json j;
    const auto op_name = getPermissionName(operation);

    std::vector<std::string> arr {uri_};

    j["operation"] = op_name;
    j["dataUris"] = arr;

    return j.dump();
}

std::string EntitlementDataspacePolicy::serializeSimpleRecord() const {
    nlohmann::json j;

    std::vector<std::string> arr {uri_};

    j["dataUri"] = uri_;

    return j.dump();
}

bool EntitlementDataspacePolicy::deserialize(const std::string& res_body) {
    DSPermissionFlags op;
    try {
        nlohmann::json j = nlohmann::json::parse(res_body);
        if (j.contains("input")) {
            auto inp = j["input"];
            if (inp.contains("operation")) {
                auto name = inp["operation"].get<std::string>();
                op = oes::osdu::api::getPermissionFlag(name);
            } else {
                throw OsduClientException("Body does not contain input in its content.");
            }
            
            if (inp.contains("records")) {
                auto records = inp["records"].get<std::vector<nlohmann::json>>();
                for(auto& rec : records) {
                    if (rec.contains("id")) {
                        auto id = rec["id"].get<std::string>();
                        auto pos = id.find_last_of(':');
                        if (pos != std::string::npos && pos + 1 < id.length()) {
                            auto uuid = id.substr(pos + 1);
                            if (usersPermissions_.find(uuid) != usersPermissions_.end()) {
                                usersPermissions_.at(uuid).addPermission(op, true);
                            } else {
                                usersPermissions_.insert(std::make_pair(uuid, DSPermission(op)));
                            }
                        }
                    }
                }
            }
        }

    } catch (const std::exception& ex) {
        OES_ERROR("Failed to parse response body while get dataspace record.\n{}", ex.what());
        return false;
    } catch (...) {
        OES_ERROR("Failed to parse response body while get dataspace record.");
        return false;
    }
    return true;
}

bool EntitlementDataspacePolicy::deserializePolicy(const std::string& res_body) const {
    if (res_body == "true") { // user has authorization
        return true;
    } else if (res_body == "false") { // user does not have authorization
        return false;
    }

    OES_ERROR("Response body for DataspacePolicy is neither true or false");
    return false;
}

bool EntitlementDataspacePolicy::deserializeSimplePolicy(const std::string& res_body) const {
    nlohmann::json j_orig(res_body);
    try {
        const auto str = j_orig.get<std::string>();
        nlohmann::json j = nlohmann::json::parse(str) ;

        auto arr = j.get<std::vector<bool>>();
        if (arr.size() != 1) {
            throw OsduClientException("Response array must have only one value.");
        }

        return arr.at(0);
    } catch (...) {
        OES_ERROR("Response body for EntitlementDataspacesPolicyBulk is invalid.");
        return {};
    }
}


bool EntitlementDataspacePolicy::hasValidPermission(const std::string& user_key, const DSPermissionFlags& permission) const {
    if ( usersPermissions_.find(user_key) == usersPermissions_.end() ) {
        return false;
    }

    return usersPermissions_.at(user_key).isValid(permission);
}

bool EntitlementDataspacePolicy::hasPermission(
    const std::string& user_key,
    const DSPermissionFlags& permission
) const {
    if (usersPermissions_.find(user_key) == usersPermissions_.end()) {
        return false;
    }

    const auto& perm = usersPermissions_.at(user_key);
    if (perm.isValid(permission)) {
        return perm.hasPermission(permission);
    }

    
    return usersPermissions_.at(user_key).hasPermission(permission);
}

void EntitlementDataspacePolicy::addUserPermission(
    const std::string& user_key,
    const DSPermissionFlags& permission
) {
    if (usersPermissions_.find(user_key) != usersPermissions_.end()) {
        usersPermissions_.at(user_key).addPermission(permission, true);
        return;
    }

    usersPermissions_.insert({user_key, DSPermission(permission, true)});
}

void EntitlementDataspacePolicy::removeUser(const std::string& user_key) {
    if (checkUserPermissions(user_key)) {
        usersPermissions_.erase(user_key);
    }
}

bool EntitlementDataspacePolicy::checkUserPermissions(const std::string& user_key) const {
    return usersPermissions_.find(user_key) != usersPermissions_.end();
}

// PolicyBulk

EntitlementDataspacePolicyBulk::EntitlementDataspacePolicyBulk() :
    EntitlementDataspace(EntitlementDSType::ePolicyBulk) {}

EntitlementDataspacePolicyBulk::EntitlementDataspacePolicyBulk(
    std::vector<std::string> uris,
    const std::vector<std::string>& legal_tags,
    const std::vector<std::string>& legal_countries):
    EntitlementDataspace("", "", legal_tags, legal_countries, EntitlementDSType::ePolicyBulk),
    uris_ptr_ (std::make_shared<std::vector<std::string>> (std::move(uris))) {}

std::string EntitlementDataspacePolicyBulk::idForQuery(const EntitlementConfigBase& /*config_osdu*/) const {
    return "bulk";
}

std::vector<std::string> EntitlementDataspacePolicyBulk::serialize(
    const EntitlementConfigBase& config_osdu,
    const DSPermissionFlags& operation) const {
    assert(uris_ptr_);
    
    auto bulk_size_str = config_osdu.authorizationBulkSize();
    size_t bulk_size {0};
    if (bulk_size_str.empty()) {
        bulk_size = 500;
    } else {
        bulk_size = static_cast<size_t>(std::stoull(bulk_size_str));
    }
    
    std::vector<std::string> bodies;
    const auto op_name = getPermissionName(operation);

    auto& uris = (*uris_ptr_);
    size_t n_uris_left = uris.size();
    size_t next;
    auto it = uris.begin();

    while (n_uris_left > 0 and it != uris.end()) {
        next = bulk_size <= n_uris_left ? bulk_size : n_uris_left;
        std::vector<std::string> subvec(it, it + next);

        nlohmann::json j;
        j["operation"] = op_name;
        j["dataUris"] = subvec;
        bodies.push_back(j.dump());

        it += next;
        n_uris_left -= next; 
    }

    return bodies;
}

std::vector<bool> EntitlementDataspacePolicyBulk::deserialize(const std::string& res_body) const {
    nlohmann::json j_orig(res_body);
    try {
        const auto str = j_orig.get<std::string>();
        nlohmann::json j = nlohmann::json::parse(str) ;
        return j.get<std::vector<bool>>();
    } catch (...) {
        OES_ERROR("Response body for EntitlementDataspacesPolicyBulk is invalid.");
        return {};
    }
}


} // namespace oes::osdu::api


