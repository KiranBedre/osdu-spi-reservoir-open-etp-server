// ============================================================================
// Copyright 2024 Emerson Paradigm Holding LLC. All rights reserved.
// Copyright 2024 EPAM Systems
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
#ifndef OES_OSDU_API_DSPERMISSION_H
#define OES_OSDU_API_DSPERMISSION_H


#include "../export.h"
//* OES_OSDU_EXPORT

#include <cstdint>
#include <cassert>
#include <string>
#include <bitset>
#include <type_traits>
#include <stdexcept>
#include <array>
#include <vector>
#include <unordered_map>
#include <chrono>

namespace oes::osdu::api {

using namespace std::chrono_literals;
using IType = std::uint8_t;

enum class DSPermissionFlags: IType {
    eCreate,
    eRead,
    eUpdate,
    eDelete,
    count_
};

struct OES_OSDU_EXPORT DSPermissionCache {
    DSPermissionCache() = delete;
    DSPermissionCache(bool cache, std::chrono::steady_clock::time_point since);

    bool is_cached_;
    std::chrono::steady_clock::time_point cache_since_;
};

constexpr IType FlagsNum = static_cast<IType>(DSPermissionFlags::count_);
using BitsetType = std::bitset<FlagsNum>;

OES_OSDU_EXPORT
std::string getPermissionName(DSPermissionFlags op);

OES_OSDU_EXPORT
DSPermissionFlags getPermissionFlag(const std::string& perm_name);

OES_OSDU_EXPORT
std::array<DSPermissionFlags, FlagsNum> getAllPermissionFlags();

bool isValidOperationValue(const IType value);

/**
 * DSPermission: class that represents the permission.
 * 
 * This does not store user information, just its permission.
 * Each DSPermission can store cached permission info as well. If a permission for an operation is added and the cache flag is true,
 * then this permission info is only cached for that operation for certain amount of time. After the expiration time is expired,
 * the permission is not valid anylonger
*/
class OES_OSDU_EXPORT DSPermission {
public:

    DSPermission() = delete;
    
    // cache_only_permited is designed to cache only permited operation, i.g. if we call makeReadPermission only the read op
    // will be cached the other ones are "expired". This way we can make new requests for other permissions without need expiration
    // time. 
    // If  cache_only_permited the makeReadPermission caches all op permission. This way if we ask create permission, it will return
    // false until the expiration and new request is done (done by entitlement class)
    DSPermission(const DSPermissionFlags flag, bool cache = true, bool cache_only_permited = true);
    DSPermission(const IType num, bool cache = true, bool cache_only_permited = true);
    DSPermission(const std::string& bits, bool cache = true, bool cache_only_permited = true);

    void addPermission(const DSPermissionFlags& perm_flag, bool cache = true);
    void refreshExpiration(const DSPermissionFlags& perm_flag);
    void clearPermission(const DSPermissionFlags& perm_flag);
    void clearAllPermission();

    bool hasPermission(const DSPermission& permission) const;
    bool hasPermission(const DSPermissionFlags flag) const;
    bool isValid(const DSPermissionFlags& flag) const;

    std::vector<DSPermissionFlags> getPermissionFlags() const;

    operator bool() const;

    static void setCacheExpirationTime(std::chrono::seconds time);

    static DSPermission makeNonePermission(bool cache = false, bool cache_only_permited = true);
    static DSPermission makeAllPermission(bool cache = false, bool cache_only_permited = true);

    static DSPermission makeCreatePermission(bool cache = false, bool cache_only_permited = true);
    static DSPermission makeReadPermission(bool cache = false, bool cache_only_permited = true);
    static DSPermission makeUpdatePermission(bool cache = false, bool cache_only_permited = true);
    static DSPermission makeDeletePermission(bool cache = false, bool cache_only_permited = true);

    OES_OSDU_EXPORT
    friend bool operator==(const DSPermission& lhs, const DSPermission& rhs);

private:
    static std::chrono::seconds cacheExpirationTime_;

    DSPermission(const BitsetType& bits, bool cache = false, bool cache_only_permited = true);
    void initCacheExpirationMap(bool cache, bool cache_only_permited = true);

private:
    BitsetType bits_;
    std::unordered_map<DSPermissionFlags, DSPermissionCache> cache_;
};


} // namespace oes::osdu::api {

#endif // OES_OSDU_API_DSPERMISSION_H