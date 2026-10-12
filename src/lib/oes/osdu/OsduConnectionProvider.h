// ============================================================================
// Copyright 2022 Emerson Paradigm Holding LLC. All rights reserved.
// Copyright © Microsoft Corporation
// Copyright 2023 Google LLC
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

#include <string>
#include <map>

#include "export.h"
#include "OsduConfig.h"

#include <oes/postgresql/Connection.h>

namespace oes::osdu
{
    class OES_OSDU_EXPORT OsduConnectionInfoProvider
    {
    public:
        static std::map<std::string, oes::postgresql::PostgresConnectParams> getOsduConnectionInfo(const oes::osdu::OsduConfig& app_config);
    };
} // namespace oes::osdu
