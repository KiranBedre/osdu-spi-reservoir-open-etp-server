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
#ifndef OES_COMMON_COMPRESSUTILS_HPP
#define OES_COMMON_COMPRESSUTILS_HPP

#include "CompressUtils.h"

#include <cassert>
#include <string>
#include <string_view>
#include <stdexcept>
#include <utility>

namespace oes::common {

//============================================================================

struct ZCompressor {
    const std::string_view name_;
    const oes_eml_ztype_e ztype_;
    const decltype(&oes_eml_zlib_nzmin) nzmin_;    // get worse-case compressed size (for allocs)
    const decltype(&oes_eml_zlib_u2z)   u2z_;      // deflate, i.e. compress
    const decltype(&oes_eml_zlib_z2u)   z2u_;      // inflate, i.e. uncompress, knowing sizes a-priori
    const decltype(&oes_eml_gzip_z2u_limit) z2u_limit_;    // inflate, i.e. uncompress
    const decltype(&oes_eml_gzip_z2u_writer) z2u_writer_;  // inflate, i.e. uncompress, in streaming fashion

    size_t bufsize(size_t nu) const;
    bool deflate(const std::string_view u, std::string& z) const;
    bool inflate(const std::string_view z, std::string& u, size_t nu) const;
    std::pair<bool, size_t> inflate(const std::string_view z, std::string& u) const;
    std::pair<bool, size_t> inflate2(const std::string_view z, std::string& u) const;

    constexpr ZCompressor with(oes_eml_ztype_e zt) const {
        return ZCompressor{ name_, zt, nzmin_, u2z_, z2u_, z2u_limit_, z2u_writer_ };
    }
};

constexpr ZCompressor ZLibCompressor{
    "ZLIB", eOES_EML_Z_FAST,
    oes_eml_zlib_nzmin,
    oes_eml_zlib_u2z,
    oes_eml_zlib_z2u,
    nullptr, nullptr
};

constexpr ZCompressor GZipCompressor{
    "GZIP", eOES_EML_Z_FAST,
    oes_eml_gzip_nzmin,
    oes_eml_gzip_u2z,
    oes_eml_gzip_z2u,
    oes_eml_gzip_z2u_limit,
    oes_eml_gzip_z2u_writer
};

inline
size_t ZCompressor::bufsize(size_t nu) const {
    unsigned nz_min = 0;
    int rc = nzmin_(ztype_, nullptr, unsigned(nu), &nz_min, nullptr, 0);
    if (rc != 0) {
        return 0;
    }
    return nz_min;
}

inline
bool ZCompressor::deflate(const std::string_view u_str, std::string& z_str) const {
    const auto* u = reinterpret_cast<const unsigned char*>(u_str.data());
    const auto nu = static_cast<unsigned>(u_str.size());

    unsigned nz_min = 0;
    int rc = nzmin_(ztype_, u, nu, &nz_min, nullptr, 0);
    if (rc != 0) {
        return false;
    }

    z_str.resize(nz_min);
    auto* z = reinterpret_cast<unsigned char*>(z_str.data());
    unsigned nz = nz_min;
    rc = u2z_(ztype_, u, nu, z, &nz, nullptr, 0);
    if (rc != 0) {
        return false;
    }
    assert(nz <= nz_min);
    z_str.resize(nz);
    return true;
}

inline
bool ZCompressor::inflate(const std::string_view z_str, std::string& u_str, size_t nu) const {
    const auto* z = reinterpret_cast<const unsigned char*>(z_str.data());
    const auto nz = static_cast<unsigned>(z_str.size());

    u_str.resize(nu);
    auto* u = reinterpret_cast<unsigned char*>(u_str.data());
    int rc = z2u_(z, nz, u, unsigned(nu), nullptr, 0);
    if (rc != 0) {
        return false;
    }

    return true;
}

inline
std::pair<bool, size_t> ZCompressor::inflate(
    const std::string_view z_str, std::string& u_str
) const {
    if (!z2u_limit_) {
        return { false, 0 };
    }

    const auto* z = reinterpret_cast<const unsigned char*>(z_str.data());
    auto nz = static_cast<unsigned>(z_str.size());
    auto nu = static_cast<unsigned>(u_str.size());

    auto* u = reinterpret_cast<unsigned char*>(u_str.data());
    int rc = z2u_limit_(z, &nz, u, &nu, nullptr, 0);
    if (rc != 0) {
        return { false, 0 };
    }

    assert(nu <= u_str.size());
    u_str.resize(nu);

    assert(nz <= z_str.size());
    size_t trailing_z_bytes = z_str.size() - nz;

    return { true, trailing_z_bytes };
}

struct ZStringOutBuf {
    std::string& str_;

    void append(const unsigned char* u, unsigned nu, char* e, unsigned ne) try {
        str_.append(u, u + nu);
    } catch (const std::exception& ex) {
        if (e) {
            snprintf(e, ne, "std::string::append failed: %s", ex.what());
        }
        throw;
    }

    char* resize(unsigned nu, char* e, unsigned ne) try {
        str_.resize(nu);
        return str_.data();
    } catch (const std::exception& ex) {
        if (e) {
            snprintf(e, ne, "std::string::resize failed: %s", ex.what());
        }
        throw;
    }

    static int zoutbuf(
        void* user_data,
        unsigned char** p_u, unsigned nu,
                 char*    e, unsigned ne
    ) try {
        assert(nu);
        assert(p_u);
        const unsigned char* u_in = *p_u;
        auto* self = static_cast<ZStringOutBuf*>(user_data);
        if (u_in) {
            self->append(u_in, nu, e, ne);
        } else {
            char* u_out = self->resize(nu, e, ne);
            *p_u = reinterpret_cast<unsigned char*>(u_out);
        }
        return 0;
    } catch (...) {
        return 4;
    }
};

inline
std::pair<bool, size_t> ZCompressor::inflate2(
    const std::string_view z_str, std::string& u_str
) const {
    if (!z2u_writer_) {
        return { false, 0 };
    }

    ZStringOutBuf u_buf{ u_str };

    const auto* z = reinterpret_cast<const unsigned char*>(z_str.data());
    auto nz = static_cast<unsigned>(z_str.size());

    int rc = z2u_writer_(z, &nz, nullptr, 0, ZStringOutBuf::zoutbuf, &u_buf);
    if (rc != 0) {
        return { false, 0 };
    }

    assert(nz <= z_str.size());
    size_t trailing_z_bytes = z_str.size() - nz;

    return { true, trailing_z_bytes };
}

//============================================================================

} //namespace oes::common

#endif
