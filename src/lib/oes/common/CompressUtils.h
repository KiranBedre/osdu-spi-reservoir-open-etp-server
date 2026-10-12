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
#ifndef OES_COMMON_COMPRESSUTILS_H
#define OES_COMMON_COMPRESSUTILS_H

#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

enum oes_eml_ztype_e {
    eOES_EML_Z_FAST,    // faster, but less compressed
    eOES_EML_Z_MID,     // neither the fastest, nor the slowest
    eOES_EML_Z_SMALL,   // smaller, but slower
};

typedef int (*ZOutBuf)(
    void* user_data,
    unsigned char** p_u, unsigned nu,
             char*    e, unsigned ne
);


// ZLib (RAW format)
OES_COMMON_EXPORT
int oes_eml_zlib_z2u(
    const unsigned char* z, unsigned nz,
          unsigned char* u, unsigned nu,
                   char* e, unsigned ne
);

OES_COMMON_EXPORT
int oes_eml_zlib_nzmin(
    enum oes_eml_ztype_e ztype,
    const unsigned char* u, unsigned    nu,
                            unsigned* p_nz,
                   char* e, unsigned    ne
);

OES_COMMON_EXPORT
int oes_eml_zlib_u2z(
    enum oes_eml_ztype_e ztype,
    const unsigned char* u, unsigned    nu,
          unsigned char* z, unsigned* p_nz,
                   char* e, unsigned    ne
);


// ZLib (GZip format: http://tools.ietf.org/html/rfc1952)
OES_COMMON_EXPORT
int oes_eml_gzip_z2u(
    const unsigned char* z, unsigned nz,
          unsigned char* u, unsigned nu,
                   char* e, unsigned ne
);

OES_COMMON_EXPORT
int oes_eml_gzip_z2u_limit(
    const unsigned char* z, unsigned* p_nz,
          unsigned char* u, unsigned* p_nu,
                   char* e, unsigned    ne
);

OES_COMMON_EXPORT
int oes_eml_gzip_z2u_writer(
    const unsigned char* z, unsigned* p_nz,
                   char* e, unsigned    ne,
           ZOutBuf zoutbuf, void* user_data
);

OES_COMMON_EXPORT
int oes_eml_gzip_nzmin(
    enum oes_eml_ztype_e ztype,
    const unsigned char* u, unsigned    nu,
                            unsigned* p_nz,
                   char* e, unsigned    ne
);

OES_COMMON_EXPORT
int oes_eml_gzip_u2z(
    enum oes_eml_ztype_e ztype,
    const unsigned char* u, unsigned    nu,
          unsigned char* z, unsigned* p_nz,
                   char* e, unsigned    ne
);


#ifdef __cplusplus
}
#endif
#endif
