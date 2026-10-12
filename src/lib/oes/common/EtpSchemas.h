// ============================================================================
// Copyright 2018-2021 Emerson Paradigm Holding LLC. All rights reserved.
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
#ifndef OES_COMMON_ETPSCHEMAS_H
#define OES_COMMON_ETPSCHEMAS_H

#include "export.h"

#include <oes/core/utils/config_enter_3rdparty.h>
#include <avro/ValidSchema.hh>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <string>

namespace oes::common {

OES_COMMON_EXPORT
std::string getSpecFile(int major, int minor);

OES_COMMON_EXPORT
avro::ValidSchema getValidSchema(int major, int minor);

OES_COMMON_EXPORT
void getLatestValidSchema(int& major, int& minor);

OES_COMMON_EXPORT
avro::ValidSchema getValidSchemaOfType12(const char* dotted_qname);

} // namespace oes::common

#endif
