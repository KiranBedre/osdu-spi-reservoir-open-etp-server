// ============================================================================
// Copyright 2011-2021 Emerson Paradigm Holding LLC. All rights reserved.
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
#include <oes/core/utils/std_fwd.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <string>
#include <vector>
#include <oes/core/utils/config_leave_3rdparty.h>

#define OES_FWD_DECL_SHARED_PTRS(Type) \
    OES_FWD_DECL_SHARED_PTR(Type);\
    typedef std::vector<Type##_ptr>        Type##_ptrs;\
    typedef std::vector<Const##Type##_ptr> Const##Type##_ptrs

namespace oes {
namespace core {

typedef std::string String;
typedef std::vector<String> StringList;

class Guid;

class JSONReader;
class JSONWriter;
struct JSONParserOptions;
struct JSONWriterOptions;

class Properties;
class NameValueList;
template<typename ElementType> class Dictionary;

OES_FWD_DECL_SHARED_PTRS(UndoStack);
OES_FWD_DECL_SHARED_PTRS(UndoStackRecord);

class Logger;
OES_FWD_DECL_SHARED_PTRS(MessageContext);
OES_FWD_DECL_SHARED_PTRS(MessageFormatter);
OES_FWD_DECL_SHARED_PTRS(MessageFilter);
OES_FWD_DECL_SHARED_PTRS(MessageHandler);

class VersionNumber;

} // namespace core
} // namespace oes
