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

#include "HdfAry.h"

namespace oes::common::hdf {
//============================================================================

HdfAry::~HdfAry() noexcept {
    delete[] owned_data;
}

HdfAry::HdfAry(
    HdfAry&& rhs
) noexcept
  : rank(rhs.rank)
  , storage_size(rhs.storage_size)
  , uncompressed_size(rhs.uncompressed_size)
  , elem_type(rhs.elem_type)
  , refd_data(rhs.refd_data)
  , owned_data(rhs.owned_data)
{
    // Copy array elements individually
    dims[0] = rhs.dims[0];
    dims[1] = rhs.dims[1];
    dims[2] = rhs.dims[2];
    dims[3] = rhs.dims[3];

    rhs.owned_data = nullptr;
}

HdfAry& HdfAry::operator=(HdfAry&& rhs) noexcept {
    rank    = rhs.rank;
    dims[0] = rhs.dims[0];
    dims[1] = rhs.dims[1];
    dims[2] = rhs.dims[2];
    dims[3] = rhs.dims[3];

    storage_size        = rhs.storage_size;
    uncompressed_size   = rhs.uncompressed_size;
    elem_type           = rhs.elem_type;

    delete[] owned_data;
    refd_data   = rhs.refd_data;
    owned_data  = rhs.owned_data;
    rhs.owned_data = nullptr;

    return *this;
}

} // namespace oes::common::hdf