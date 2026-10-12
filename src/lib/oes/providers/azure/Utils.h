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

#pragma once


#include <string>
#include <map>
#include "../export.h"

namespace oes {
    namespace providers {
        namespace azure {
            OES_PROVIDERS_EXPORT
            std::string urlEncode(const char* msg);

            OES_PROVIDERS_EXPORT
            std::string paramsToString(const std::map<std::string, std::string>& params);

            OES_PROVIDERS_EXPORT
            std::string getEnv(const std::string& name);

            OES_PROVIDERS_EXPORT
            void setupAzureLogging();
        } // namespace azure
    } // namespace providers
} // namespace oes
