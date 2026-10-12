// Copyright 2024 Emerson Paradigm Holding LLC. All rights reserved.
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

#ifndef DELEGATE_CLIENT_EXCEPTION_H
#define DELEGATE_CLIENT_EXCEPTION_H

#include <stdexcept>

#include <oes/common/export.h>

namespace oes::common::auth {

class OES_COMMON_EXPORT DelegateClientException: public std::runtime_error {
public:
    explicit DelegateClientException(const char* msg);
    ~DelegateClientException() throw() override {}    
};

}


#endif // DELEGATE_CLIENT_EXCEPTION_H