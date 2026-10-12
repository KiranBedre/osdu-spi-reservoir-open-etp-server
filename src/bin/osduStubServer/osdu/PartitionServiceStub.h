// ============================================================================
// Copyright 2023 Emerson Paradigm Holding LLC. All rights reserved.
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
#include <vector>
#include "http/HttpServerStub.h"

namespace oes::testing::osdu
{
    /**
     * OSDU partitions service stub.
     */
    class PartitionServiceStub : public BaseRestRequestHandler
    {
    public:
        static constexpr char resource[] = "partitions";

        PartitionServiceStub();

        void withPartitions(std::initializer_list<std::string> partitions);
        void withPartitions(std::vector<std::string> partitions);
        [[nodiscard]] static std::string getConnectionIdForPartition(const std::string& partition);

        bool getEntitiesList(HttpContext& httpContext) override;
        bool getEntityById(HttpContext& httpContext, const std::string& id) override;
        bool createEntity(HttpContext&) override { return false; }
        bool updateEntity(HttpContext&) override { return false; }
        bool deleteEntity(HttpContext&, const std::string&) override { return false; }

    private:
        std::vector<std::string> partitions_;
    };
}
