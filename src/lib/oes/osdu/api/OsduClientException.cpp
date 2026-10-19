// ============================================================================
// Copyright 2022 Emerson Paradigm Holding LLC. All rights reserved.
// Copyright © Microsoft Corporation
// Copyright 2022 EPAM Systems
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

#include <oes/osdu/api/OsduClientException.h>

namespace oes {
    namespace osdu {
        namespace api {
            /*!
            * \brief Instantiates a new OsduClientException exception from a string.
            * \param msg the error message
            */
            OsduClientException::OsduClientException(
                const char* msg
            ) : std::runtime_error(msg)
            {}
        }
    }
}
