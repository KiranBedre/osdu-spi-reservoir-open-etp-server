// ============================================================================
// Copyright 2023-2024 Emerson Paradigm Holding LLC. All rights reserved.
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
#ifndef OES_COMMON_HDFARY_H
#define OES_COMMON_HDFARY_H

#include <cstdint>  // Add this include for uint64_t
#include "export.h"

#include <string>


namespace oes::common::hdf {

typedef long long int hdf_int64;

//============================================================================

// Warning: Micking's ETP's AnyArrayType, but NOT ITS VALUES!
// That's to minimize storage on the SQLite side, where 0 and 1
// are special, with "zero bytes" of storage, i.e. part of the "type"
// of the value, while >1 needs 1 header byte, and 1+ byte(s) of storage.
// Thus we reserve 0 and 1 for most used types, int and double.
enum HdfAryType {
    eAryOfVoid    = -1, // i.e. unknown
    eAryOfInt     = 0,
    eAryOfDouble  = 1,
    eAryOfFloat   = 2,
    eAryOfBytes   = 3,
    eAryOfBoolean = 4,
    eAryOfLong    = 5,
    eAryOfString  = 6,
};

struct OES_COMMON_EXPORT HdfAry {
    HdfAry() = default;
    ~HdfAry() noexcept;

    HdfAry(const HdfAry&) = delete;
    HdfAry& operator=(const HdfAry&) = delete;

    HdfAry(HdfAry&&) noexcept;
    HdfAry& operator=(HdfAry&&) noexcept;

    int rank = -1;
    uint64_t dims[4] = { 0 };
    uint64_t storage_size = 0;
    uint64_t uncompressed_size = 0;
    HdfAryType elem_type = eAryOfVoid;
    const void* refd_data = nullptr;
    unsigned char* owned_data = nullptr;

    const void* data() const {
        return owned_data? owned_data: refd_data;
    }
};

struct HdfRef {
    hdf_int64 obj_id;    //!< The obj containing the reference (\em child-row)
    hdf_int64 xpa_id;    //!< The XML path at which the reference was found
    std::string path;       //!< Unique per ETP-Space
};

} // namespace oes::common::hdf

#endif // OES_COMMON_HDFARY_H