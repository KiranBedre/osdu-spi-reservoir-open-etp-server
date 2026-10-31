// ============================================================================
// Copyright 2020-2023 Emerson Paradigm Holding LLC. All rights reserved.
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
#include "../fwd.h"
#include "../export.h"
#include "Session.h"

#include <oes/common/etp12/codegen/Messages.i.h>

#include <string>
#include <map>
#include <vector>

namespace oes::eml::openkv {

using oes::common::etp12::Datatypes::DataValue;
class OES_EML_EXPORT BaseProto
{
public:
    enum class AccessType {
        CAN_READ,
        CAN_WRITE,
    };
    BaseProto(OpenKVRepo_ptr repo, std::weak_ptr<OpenKVSession> session);

    std::shared_ptr<OpenKVSession> session() const { return session_.lock(); }
    OpenKVRepo_ptr repo() const { return repo_; }
    bool registerDataspace(Space& space);
    bool unregisterDataspace(const Space& space);
    bool checkDataspaceAccess(const Space& space, AccessType ro, ErrorInfo& err, bool unlock_op = false) const;
    std::vector<bool> checkDataspacesAccess(const std::vector<Space>& spaces, AccessType ro, ErrorInfo& err, bool unlock_op = false) const;

protected:
    ~BaseProto() = default;

    void newTx(bool ro, std::string msg, std::vector<EmlUri> dataspaceUris, bool local);
    OpenKVScopedTxApi txApi(ApiType type);
    void resetTx();


    OpenKVRepo_ptr repo_;
    std::weak_ptr<OpenKVSession> session_;
};

OES_EML_EXPORT
std::vector<std::string> extractXDataValues(const std::map<std::string, DataValue>& xdata, const std::string& key);

} // namespace oes::eml::openkv
