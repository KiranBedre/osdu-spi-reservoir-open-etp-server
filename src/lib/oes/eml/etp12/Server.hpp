// ============================================================================
// Copyright 2020-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#ifndef OES_EML_ETP12_SERVER_HPP
#define OES_EML_ETP12_SERVER_HPP

#include "Server.h"

namespace oes::eml::etp12 {

//============================================================================

template <typename T>
void handleException(proto12::EtpResponseHandler<T>& res_handler) {
    std::string msg;
    ErrorCodes errc = data12::EINVALID_STATE;

    try {
        std::rethrow_exception(std::current_exception());
    } catch (proto12::EtpResponseStatus& rc) {
        // FIXME: Must default-Construct empty response
        return res_handler(std::move(rc), T());
    } catch (const Todo&) {
        msg = "Server Error: TODO";
        errc = data12::ENOTSUPPORTED;
    } catch (const std::exception& ex) {
        msg = "Server Error: Unexpected Exception: ";
        msg += ex.what();

        // trim trailing newlines
        while (msg.back() == '\n') {
            msg.resize(msg.size() - 1);
        }
    } catch (...) {
        msg = "Server Error: Unknown Exception.";
    }

    // FIXME: Must default-Construct empty response
    res_handler(errorStatus(errc, std::move(msg)), T());
}

//============================================================================

} // namespace oes::eml::etp12

#endif
