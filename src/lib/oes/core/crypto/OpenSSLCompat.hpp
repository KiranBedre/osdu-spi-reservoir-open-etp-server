// ============================================================================
// Copyright 2019-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#ifndef OES_CORE_CRYPTO_OPENSSLCOMPAT_H
#define OES_CORE_CRYPTO_OPENSSLCOMPAT_H

#include <openssl/opensslv.h>

#if OPENSSL_VERSION_NUMBER < 0x10100000L
/**
 * In OpenSSL v1.1, some new functions were introduced, and other were renamed.
 * This file allows the use of older OpenSSL versions by defining the new functions introduced by v1.1.
 */

#include <openssl/bio.h>
#include <openssl/ecdsa.h>
#include <openssl/evp.h>


int ECDSA_SIG_set0(ECDSA_SIG *sig, BIGNUM *r, BIGNUM *s) {
  if (r == NULL || s == NULL)
    return 0;
  BN_clear_free(sig->r);
  BN_clear_free(sig->s);
  sig->r = r;
  sig->s = s;
  return 1;
}

void ECDSA_SIG_get0(const ECDSA_SIG *sig, const BIGNUM **r, const BIGNUM **s) {
  if (r != NULL)
    *r = sig->r;
  if (s != NULL)
    *s = sig->s;
}

EVP_MD_CTX *EVP_MD_CTX_new(void) {
  return EVP_MD_CTX_create();
}

void EVP_MD_CTX_free(EVP_MD_CTX *ctx) {
  EVP_MD_CTX_destroy(ctx);
}

#endif

#endif /* OES_CORE_CRYPTO_OPENSSLCOMPAT_H */