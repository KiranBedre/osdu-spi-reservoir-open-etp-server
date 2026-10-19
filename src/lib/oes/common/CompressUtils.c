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

#include "CompressUtils.h"

#include <zlib.h>

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Based on Public-domain SQLite code
// https://sqlite.org/src/file/ext/misc/zipfile.c

// SQLite code uses Byte* (ZLib typedef), instead of unsigned char*, but since
// we want to hide the ZLib-dependency, we use unsigned char* directly.
// (Byte is not unsigned char only on exotic platforms we don't care about)

/*!
 * \internal
 * \brief Uncompress (inflate) a Deflate-compress raw/unwrapped deflate-data.
 *
 * \param z input buffer to decompress from
 * \param nz size in bytes of input buffer
 * \param u output buffer to decompress into
 * \param nu size in bytes of output buffer
 * \param e output buffer to write \em short error message into, on error. Can be null.
 * \param ne size in bytes of output error buffer (32 char long, at least)
 * \return 0 on success; non-zero otherwise.
 *
 * \warning Caller must know a-priori the size of the uncompressed data,
 *          and pre-allocate the output buffer.
 */
int oes_eml_zlib_z2u(
    const unsigned char* z, unsigned nz,
          unsigned char* u, unsigned nu,
                   char* e, unsigned ne
) {
    int zerr;
    z_stream zstr;
    memset(&zstr, 0, sizeof(zstr));

    zstr.next_in = (unsigned char*)z;
    zstr.avail_in = nz;
    zstr.next_out = u;
    zstr.avail_out = nu;

    // See https://stackoverflow.com/a/29020296/103724 for -15
    zerr = inflateInit2(&zstr, -15);
    if (zerr != Z_OK) {
        if (e) {
            snprintf(e, ne, "inflateInit2() failed (%d)", zerr);
        }
        return 1;
    }

    zerr = inflate(&zstr, Z_NO_FLUSH);
    if (zerr != Z_STREAM_END) {
        if (e) {
            snprintf(e, ne, "inflate() failed (%d)", zerr);
        }
        return 2;
    }

    zerr = inflateEnd(&zstr);
    if (zerr != Z_OK) {
        if (e) {
            snprintf(e, ne, "inflateEnd() failed (%d)", zerr);
        }
        return 3;
    }

    return 0;
}

/*!
 * \internal
 *
 * \brief Gets buffer-size upper-bound to compress \a *p_ns bytes of data.
 *
 * \param ztype whether to favor speed or size
 * \param u input buffer to be compressed (may be null)
 * \param nu size in bytes of input buffer
 * \param p_nz Minimum output buffer size required.
 * \param e output buffer to write \em short error message into, on error
 * \param ne size in bytes of output error buffer (32 char long, at least)
 * \return 0 on success; non-zero otherwise.
 */
int oes_eml_zlib_nzmin(
    enum oes_eml_ztype_e ztype,
    const unsigned char* u, unsigned    nu,
                            unsigned* p_nz,
                   char* e, unsigned    ne
) {
    int zerr;
    z_stream zstr;

    memset(&zstr, 0, sizeof(zstr));

    zstr.next_in = (unsigned char*)u;
    zstr.avail_in = nu;

    int level = 1;
    switch (ztype) {
    case eOES_EML_Z_FAST:  level = Z_BEST_SPEED; break; // 1
    case eOES_EML_Z_MID:   level = 5; break;
    case eOES_EML_Z_SMALL: level = Z_BEST_COMPRESSION; break; // 9
    }

    zerr = deflateInit2(&zstr, level, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
    if (zerr != Z_OK) {
        if (e) {
            snprintf(e, ne, "deflateInit2() failed (%d)", zerr);
        }
        return 1;
    }

    *p_nz = deflateBound(&zstr, nu);

    zerr = deflateEnd(&zstr);
    // ToDo: investigate why on Windows there is Z_OK return code, but Z_DATA_ERROR on Linux, check other places with check "zerr != Z_DATA_ERROR"
    if (zerr != Z_OK && zerr != Z_DATA_ERROR) { // necessary, otherwise leaks!
        if (e) {
            snprintf(e, ne, "deflateEnd() failed (%d)", zerr);
        }
        return 3;
    }

    return 0;
}

/*!
 * \internal
 *
 * \brief Compress (Deflate) a buffer.
 * \param ztype whether to favor speed or size
 * \param u input buffer to be compressed
 * \param nu size in bytes of input buffer
 * \param z output buffer to compress into
 * \param p_nz size in bytes of output buffer. On success, will contain
 *        actual size of the compressed data (in raw/unwrapped form).
 * \param e output buffer to write \em short error message into, on error
 * \param ne size in bytes of output error buffer (32 char long, at least)
 * \return 0 on success; non-zero otherwise.
 */
int oes_eml_zlib_u2z(
    enum oes_eml_ztype_e ztype,
    const unsigned char* u, unsigned    nu,
          unsigned char* z, unsigned* p_nz,
                   char* e, unsigned    ne
) {
    int zerr;
    z_stream zstr;

    memset(&zstr, 0, sizeof(zstr));
    zstr.next_in = (unsigned char*)u;
    zstr.avail_in = nu;

    int level = 1;
    switch (ztype) {
    case eOES_EML_Z_FAST:  level = Z_BEST_SPEED; break; // 1
    case eOES_EML_Z_MID:   level = 5; break;
    case eOES_EML_Z_SMALL: level = Z_BEST_COMPRESSION; break; // 9
    }

    zerr = deflateInit2(&zstr, level, Z_DEFLATED, -15, 8, Z_DEFAULT_STRATEGY);
    if (zerr != Z_OK) {
        if (e) {
            snprintf(e, ne, "deflateInit2() failed (%d)", zerr);
        }
        return 1;
    }

    zstr.next_out = z;
    zstr.avail_out = *p_nz;

    zerr = deflate(&zstr, Z_FINISH);
    if (zerr != Z_STREAM_END) {
        if (e) {
            snprintf(e, ne, "deflate() failed (%d)", zerr);
        }
        return 2;
    }
    *p_nz = zstr.total_out;

    zerr = deflateEnd(&zstr);
    if (zerr != Z_OK) {
        if (e) {
            snprintf(e, ne, "deflateEnd() failed (%d)", zerr);
        }
        return 3;
    }

    return 0;
}

#define Z_WINDOW_BITS 15
#define GZIP_ENCODING 16

int oes_eml_gzip_z2u(
    const unsigned char* z, unsigned nz,
          unsigned char* u, unsigned nu,
                   char* e, unsigned ne
) {
    int zerr;
    z_stream zstr;
    memset(&zstr, 0, sizeof(zstr));

    zstr.next_in = (unsigned char*)z;
    zstr.avail_in = nz;
    zstr.next_out = u;
    zstr.avail_out = nu;

    // See https://stackoverflow.com/a/29020296/103724 for 31
    zerr = inflateInit2(&zstr, Z_WINDOW_BITS | GZIP_ENCODING);
    if (zerr != Z_OK) {
        if (e) {
            snprintf(e, ne, "inflateInit2() failed (%d)", zerr);
        }
        return 1;
    }

    zerr = inflate(&zstr, Z_NO_FLUSH);
    if (zerr != Z_STREAM_END) {
        if (e) {
            snprintf(e, ne, "inflate() failed (%d)", zerr);
        }
        return 2;
    }

    zerr = inflateEnd(&zstr);
    if (zerr != Z_OK) {
        if (e) {
            snprintf(e, ne, "inflateEnd() failed (%d)", zerr);
        }
        return 3;
    }

    return 0;
}

/*!
 * \brief GZip-decompress, w/o a-priori knowledge of the uncompressed size.
 *
 * Output buffer to receive uncompressed data, must be pre-allocated to the
 * maximum size allowed on decompression. Client can trim excess, on success,
 * by using the updated \a p_nu size.
 *
 * Similarly, in case there was excess data at the end of the compressed buffer,
 * initially sized as specified on input by \a p_nz, that length will be updated
 * with the actual number of bytes used for decompression, which could be less
 * than the original, in case there's extra data following the compressed stream.
 * 
 * \param z compressed input data buffer;
 * \param p_nz[inout] on input, the compressed data length;
 *     on output, the compressed data length actually used.
 * \param u uncompressed \b pre-allocated output data buffer;
 * \param p_nu[inout] on input, the maximum length of decompressed data;
 *     on output, the actual decompressed data length.
 * \param e output error buffer, to put error message into, on error. Can be null.
 *     error message will be null-terminated, unless exactly \a ne bytes long, in
 *     which case it could be truncated as well.
 * \param nee output error buffer length., to put error message into, on error.
 * \return 0 on success.
 */
int oes_eml_gzip_z2u_limit(
    const unsigned char* z, unsigned* p_nz,
          unsigned char* u, unsigned* p_nu,
                   char* e, unsigned    ne
) {
    int zerr;
    z_stream zstr;
    memset(&zstr, 0, sizeof(zstr));

    zstr.next_in = (unsigned char*)z;
    zstr.avail_in = *p_nz;
    zstr.next_out = u;
    zstr.avail_out = *p_nu;

    // See https://stackoverflow.com/a/29020296/103724 for 31
    zerr = inflateInit2(&zstr, Z_WINDOW_BITS | GZIP_ENCODING);
    if (zerr != Z_OK) {
        if (e) {
            snprintf(e, ne, "inflateInit2() failed (%d)", zerr);
        }
        return 1;
    }

    zerr = inflate(&zstr, Z_NO_FLUSH);
    if (zerr != Z_STREAM_END) {
        if (e) {
            snprintf(e, ne, "inflate() failed (%d)", zerr);
        }
        return 2;
    }

    *p_nz = *p_nz - zstr.avail_in;
    *p_nu = *p_nu - zstr.avail_out;

    zerr = inflateEnd(&zstr);
    if (zerr != Z_OK) {
        if (e) {
            snprintf(e, ne, "inflateEnd() failed (%d)", zerr);
        }
        return 3;
    }

    return 0;
}

int oes_eml_gzip_z2u_writer(
    const unsigned char* z, unsigned* p_nz,
                   char* e, unsigned    ne,
           ZOutBuf zoutbuf, void* user_data
) {
    int zerr;
    z_stream zstr;
    memset(&zstr, 0, sizeof(zstr));

    zstr.next_in = (unsigned char*)z;
    zstr.avail_in = *p_nz;

    // See https://stackoverflow.com/a/29020296/103724 for 31
    zerr = inflateInit2(&zstr, Z_WINDOW_BITS | GZIP_ENCODING);
    if (zerr != Z_OK) {
        if (e) {
            snprintf(e, ne, "inflateInit2() failed (%d)", zerr);
        }
        return 1;
    }

    // based on https://zlib.net/zpipe.c
    unsigned char out[64*1024];
    const unsigned nout = (unsigned)sizeof(out);
    do {
        zstr.avail_out = nout;
        zstr.next_out = out;

        zerr = inflate(&zstr, Z_NO_FLUSH);
        if ((zerr < 0) || (zerr == Z_NEED_DICT)) {
            // Negative zerr are errors, positive values are used for special but normal events.
            if (e) {
                snprintf(e, ne, "inflate() failed (%d)", zerr);
            }
            zerr = inflateEnd(&zstr);
            /*
            ** lets not clobber the previous error
            if (zerr != Z_OK) {
                if (e) {
                    snprintf(e, ne, "inflateEnd() failed (%d)", zerr);
                }
                return 3;
            }
            */
            return 2;
        }

        const unsigned got_bytes = nout - zstr.avail_out;
        unsigned char* p_out = out;
        unsigned char** pp_out = &p_out;
        const int zrc = zoutbuf(user_data, pp_out, got_bytes, e, ne);
        assert(*pp_out == p_out); // unchanged, i.e. used as pure writer
        if (zrc != 0) {
            // we assume the zwriter updated e on error itself
            zerr = inflateEnd(&zstr);
            return zrc;
        }
    } while (zstr.avail_out == 0);

    if (zerr != Z_STREAM_END) {
        if (e) {
            snprintf(e, ne, "inflate() failed (%d)", zerr);
        }
        return 2;
    }

    *p_nz = *p_nz - zstr.avail_in;

    zerr = inflateEnd(&zstr);
    if (zerr != Z_OK) {
        if (e) {
            snprintf(e, ne, "inflateEnd() failed (%d)", zerr);
        }
        return 3;
    }

    return 0;
}

int oes_eml_gzip_nzmin(
    enum oes_eml_ztype_e ztype,
    const unsigned char* u, unsigned    nu,
                            unsigned* p_nz,
                   char* e, unsigned    ne
) {
    int zerr;
    z_stream zstr;

    memset(&zstr, 0, sizeof(zstr));

    zstr.next_in = (unsigned char*)u;
    zstr.avail_in = nu;

    int level = 1;
    switch (ztype) {
    case eOES_EML_Z_FAST:  level = Z_BEST_SPEED; break; // 1
    case eOES_EML_Z_MID:   level = 5; break;
    case eOES_EML_Z_SMALL: level = Z_BEST_COMPRESSION; break; // 9
    }

    zerr = deflateInit2(
        &zstr, level, Z_DEFLATED, Z_WINDOW_BITS | GZIP_ENCODING, 8, Z_DEFAULT_STRATEGY
    );
    if (zerr != Z_OK) {
        if (e) {
            snprintf(e, ne, "deflateInit2() failed (%d)", zerr);
        }
        return 1;
    }

    *p_nz = deflateBound(&zstr, nu);

    zerr = deflateEnd(&zstr);
    if (zerr != Z_OK) { // necessary, otherwise leaks!
        if (e) {
            snprintf(e, ne, "deflateEnd() failed (%d)", zerr);
        }
        return 3;
    }

    return 0;
}

int oes_eml_gzip_u2z(
    enum oes_eml_ztype_e ztype,
    const unsigned char* u, unsigned    nu,
          unsigned char* z, unsigned* p_nz,
                   char* e, unsigned    ne
) {
    int zerr;
    z_stream zstr;

    memset(&zstr, 0, sizeof(zstr));
    zstr.next_in = (unsigned char*)u;
    zstr.avail_in = nu;

    int level = 1;
    switch (ztype) {
    case eOES_EML_Z_FAST:  level = Z_BEST_SPEED; break; // 1
    case eOES_EML_Z_MID:   level = 5; break;
    case eOES_EML_Z_SMALL: level = Z_BEST_COMPRESSION; break; // 9
    }

    zerr = deflateInit2(
        &zstr, level, Z_DEFLATED, Z_WINDOW_BITS | GZIP_ENCODING, 8, Z_DEFAULT_STRATEGY
    );
    if (zerr != Z_OK) {
        if (e) {
            snprintf(e, ne, "deflateInit2() failed (%d)", zerr);
        }
        return 1;
    }

    zstr.next_out = z;
    zstr.avail_out = *p_nz;

    zerr = deflate(&zstr, Z_FINISH);
    if (zerr != Z_STREAM_END) {
        if (e) {
            snprintf(e, ne, "deflate() failed (%d)", zerr);
        }
        return 2;
    }
    *p_nz = zstr.total_out;

    zerr = deflateEnd(&zstr);
    if (zerr != Z_OK) {
        if (e) {
            snprintf(e, ne, "deflateEnd() failed (%d)", zerr);
        }
        return 3;
    }

    return 0;
}

#if defined(_WIN32)

// warning C4100: '<...>' : unreferenced formal parameter
#pragma warning(disable : 4100)

#elif defined(__INTEL_COMPILER)

// warning #869: parameter "<...>" was never referenced
#pragma warning(disable : 869)

#elif defined(__clang__)

#pragma GCC diagnostic ignored "-Wunused-parameter"

#elif defined(__GNUC__) && ((__GNUC__ * 100) + __GNUC_MINOR__) >= 406

// warning: unused parameter '...'
#pragma GCC diagnostic ignored "-Wunused-parameter"

#endif

#if defined(_WIN32)
#pragma warning(disable : 4245) // '=': conversion from 'int' to 'uint64_t', signed/unsigned mismatch
#pragma warning(disable : 4244) // '=': conversion from 'uint32_t' to 'unsigned char', possible loss of data
#pragma warning(disable : 4214) // nonstandard extension used: bit field types other than int
#pragma warning(disable : 4127) // conditional expression is constant
#endif

