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

#ifndef OES_COMMON_ETP12_ERRORCODES_H_
#define OES_COMMON_ETP12_ERRORCODES_H_

namespace oes::common::etp12::Datatypes {

enum ErrorCodes {
    IS_OK                           =    0,

    ENOROLE                         =    1,
    ENOSUPPORTEDPROTOCOLS           =    2,
    EINVALID_MESSAGETYPE            =    3,
    EUNSUPPORTED_PROTOCOL           =    4,
    EINVALID_ARGUMENT               =    5,
    EREQUEST_DENIED                 =    6,
    ENOTSUPPORTED                   =    7,
    EINVALID_STATE                  =    8,
    EINVALID_URI                    =    9,
    EAUTHORIZATION_EXPIRED          =   10,
    ENOT_FOUND                      =   11,
    ELIMIT_EXCEEDED                 =   12,
    ECOMPRESSION_NOTSUPPORTED       =   13,
    EINVALID_OBJECT                 =   14,
    EMAX_TRANSACTIONS_EXCEEDED      =   15,
    EDATAOBJECTTYPE_NOTSUPPORTED    =   16,
    EMAXSIZE_EXCEEDED               =   17,
    EMULTIPART_CANCELLED            =   18,
    EINVALID_MESSAGE                =   19,
    EINVALID_INDEXKIND              =   20,
    ENOSUPPORTEDFORMATS             =   21,
    EREQUESTUUID_REJECTED           =   22,
    EUPDATEGROWINGOBJECT_DENIED     =   23,
    EBACKPRESSURE_LIMIT_EXCEEDED    =   24,
    EBACKPRESSURE_WARNING           =   25,
    ETIMED_OUT                      =   26,
    EAUTHORIZATION_REQUIRED         =   27,
    EAUTHORIZATION_EXPIRING         =   28,
    ENOSUPPORTEDDATAOBJECTTYPES     =   29,

    EINVALID_CHANNELID              = 1002,
    ENOCASCADE_DELETE               = 4003,
    EPLURAL_OBJECT                  = 4004,
    ERETENTION_PERIOD_EXCEEDED      = 5001,
    ENOTGROWINGOBJECT               = 6001,
};

} // namespace oes::eml::etp12::Datatypes

#endif
