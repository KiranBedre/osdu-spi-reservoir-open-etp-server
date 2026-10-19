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

#include "export.h"
#include "OpenKVClientServerFixture.h"

#include <string>

namespace oes::testing::eml {
    class OES_TESTING_EML_EXPORT OsduOpenKVClientServerFixture : public OpenKVClientServerFixture {
    public:
        OsduOpenKVClientServerFixture();
        ~OsduOpenKVClientServerFixture() override;

        void initServer() override;
        void clearImports() override;
        void clearAdmin() override;

        oes::client::ClientConfig& initClientConfig(const std::string& data_partition_id);
        Client_uptr connectClient(const std::string& data_partition_id);
    };
}
