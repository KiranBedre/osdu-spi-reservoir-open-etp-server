#include "DSPermission.h"

namespace oes::osdu::api {

std::string getPermissionName(DSPermissionFlags op) {
    switch (op) {
    case DSPermissionFlags::eCreate:
        return "Create";
    case DSPermissionFlags::eRead:
        return "Read";
    case DSPermissionFlags::eUpdate:
        return "Update";
    case DSPermissionFlags::eDelete:
        return "Delete";
    case DSPermissionFlags::count_:
        break;
    }

    assert(false);
    return "Unkown operation";
}

DSPermissionFlags getPermissionFlag(const std::string& perm_name) {
    if (perm_name == "Create") {
        return DSPermissionFlags::eCreate;
    }
    else if (perm_name == "Read" ) {
        return DSPermissionFlags::eRead;
    }
    else if (perm_name == "Update" ) {
        return DSPermissionFlags::eUpdate;
    }
    else if (perm_name == "Delete" ) {
        return DSPermissionFlags::eDelete;
    }

    assert(false);
    return DSPermissionFlags::count_;
}

std::array<DSPermissionFlags, FlagsNum> getAllPermissionFlags() {
    return std::array<DSPermissionFlags, FlagsNum> {
        DSPermissionFlags::eCreate,
        DSPermissionFlags::eRead,
        DSPermissionFlags::eUpdate,
        DSPermissionFlags::eDelete
    };
}

bool isValidOperationValue(const IType value) {
    return value <= BitsetType(std::string(FlagsNum, '1')).to_ulong();
}

// DSPermissionCache

DSPermissionCache::DSPermissionCache(bool cache, std::chrono::steady_clock::time_point since) {
    is_cached_ = cache;
    cache_since_ = since;
}

// DSPermission

std::chrono::seconds DSPermission::cacheExpirationTime_ = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::minutes(5));
void DSPermission::setCacheExpirationTime(std::chrono::seconds time) {
    DSPermission::cacheExpirationTime_ = time;
}

DSPermission::DSPermission(const DSPermissionFlags flag, bool cache, bool cache_only_permited) {
    if (flag == DSPermissionFlags::count_) {
        throw std::logic_error("DSPermission::count_ must not be used.");
    }
    const auto val = static_cast<IType>(flag);
    bits_ = BitsetType(std::string("1") + std::string(val, '0'));
    initCacheExpirationMap(cache, cache_only_permited);
}

DSPermission::DSPermission(const IType num, bool cache, bool cache_only_permited) {
    if (!isValidOperationValue(num)) {
        throw std::logic_error("DSPermission must be initialzied with a valid value");
    }
    bits_ = BitsetType(num);
    initCacheExpirationMap(cache, cache_only_permited);
}

DSPermission::DSPermission(const std::string& bits, bool cache, bool cache_only_permited) {
    bits_ = BitsetType(bits);
    initCacheExpirationMap(cache, cache_only_permited);
}

DSPermission::DSPermission(const BitsetType& bits, bool cache, bool cache_only_permited) {
    if (!isValidOperationValue(static_cast<IType>(bits.to_ulong()))) {
        throw std::logic_error("DSPermission must be initialzied with a valid value");
    }
    bits_ = bits;
    initCacheExpirationMap(cache, cache_only_permited);
}

void DSPermission::initCacheExpirationMap(bool cache, bool cache_only_permited) {
    bool value {false};
    for(const auto& flag : getAllPermissionFlags() ) {
        if (cache_only_permited) {
            value = cache && bits_[static_cast<int>(flag)]; 
            cache_.emplace(flag, DSPermissionCache(value, std::chrono::steady_clock::now())); 
        } else {
            cache_.emplace(flag, DSPermissionCache(cache, std::chrono::steady_clock::now())); 
        }
    }
}

void DSPermission::addPermission(const DSPermissionFlags& perm_flag, bool cache) {
    (*this).bits_ |= DSPermission(perm_flag, false).bits_;
    if (cache) {
        cache_.at(perm_flag) = DSPermissionCache(true, std::chrono::steady_clock::now());
    }
}

void DSPermission::refreshExpiration(const DSPermissionFlags& perm_flag) {
    cache_.at(perm_flag) = DSPermissionCache(true, std::chrono::steady_clock::now());
}

void DSPermission::clearPermission(const DSPermissionFlags& perm_flag) {
    auto perm_index = static_cast<int>(perm_flag);
    bits_[perm_index] = false;
    cache_.at(perm_flag).is_cached_ = false;
}

void DSPermission::clearAllPermission() {
    bits_ = BitsetType(0);
    for(auto& cache : cache_) {
        cache.second.is_cached_ = false;
    }
}

bool DSPermission::hasPermission(const DSPermission& permission) const {
    const auto flags = permission.getPermissionFlags();
    if (flags.empty()) {
        return false;
    }

    for (const auto& flag : flags ) {
        if (!hasPermission(flag)) {
            return false;
        }
    }
    
    return true;
}

bool DSPermission::hasPermission(const DSPermissionFlags flag) const {
    if (!isValid(flag)) {
        return false;
    }

    return (bits_ & DSPermission(flag, false).bits_).to_ulong() > 0;
}

bool DSPermission::isValid(const DSPermissionFlags& flag) const {
    const auto& cache = cache_.at(flag);

    // if is_cached_ is false, then the cache is not valid
    if (!cache.is_cached_) {
        return false;
    }

    const auto now = std::chrono::steady_clock::now();
    return std::chrono::duration_cast<std::chrono::seconds>(now - cache.cache_since_) <= cacheExpirationTime_;
}

DSPermission::operator bool() const {
    return bits_.to_ulong() > 0;
}

bool operator==(const DSPermission& lhs, const DSPermission& rhs) {
    return lhs.bits_ == rhs.bits_;
}

std::vector<DSPermissionFlags> DSPermission::getPermissionFlags() const {
    std::vector<DSPermissionFlags> flags;

    for (short i = 0; i < FlagsNum; i++) {
        if (bits_[i]) {
            flags.emplace_back(static_cast<DSPermissionFlags>(i));
        }
    }

    return flags;
}

// -- 

DSPermission DSPermission::makeNonePermission(bool cache, bool cache_only_permited) {
    return DSPermission(0, cache, cache_only_permited);
}

DSPermission DSPermission::makeAllPermission(bool cache, bool cache_only_permited) {
    return DSPermission(std::string(FlagsNum, '1'), cache, cache_only_permited);
}

DSPermission DSPermission::makeCreatePermission(bool cache, bool cache_only_permited) {
    return DSPermission(DSPermissionFlags::eCreate, cache, cache_only_permited);
}

DSPermission DSPermission::makeReadPermission(bool cache, bool cache_only_permited) {
    return DSPermission(DSPermissionFlags::eRead, cache, cache_only_permited);
}

DSPermission DSPermission::makeUpdatePermission(bool cache, bool cache_only_permited) {
    return DSPermission(DSPermissionFlags::eUpdate, cache, cache_only_permited);
}

DSPermission DSPermission::makeDeletePermission(bool cache, bool cache_only_permited) {
    return DSPermission(DSPermissionFlags::eDelete, cache, cache_only_permited);
}


} // namespace oes::osdu::api