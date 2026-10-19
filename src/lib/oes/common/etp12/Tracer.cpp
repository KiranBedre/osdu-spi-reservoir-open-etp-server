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

// From ioapi.h of MiniZip (also in dx/resqml_epc)
// _FILE_OFFSET_BITS must be set *BEFORE* #include "stdio.h" on POSIX
// thus we used void* for ofs_ in header, so we can ensure that in .cpp

#if (!defined(_WIN32)) && (!defined(WIN32)) && (!defined(__APPLE__))
#  ifndef __USE_FILE_OFFSET64
#    define __USE_FILE_OFFSET64
#  endif
#  ifndef __USE_LARGEFILE64
#    define __USE_LARGEFILE64
#  endif
#  ifndef _LARGEFILE64_SOURCE
#    define _LARGEFILE64_SOURCE
#  endif
#  ifndef _FILE_OFFSET_BIT
#    define _FILE_OFFSET_BIT 64
#  endif
#endif

#include "Tracer.h"

#include <oes/core/utils/ChronoUtils.h>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <boost/crc.hpp>
#include <boost/endian/conversion.hpp>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstring>
#include <iterator>
#include <filesystem>
#include <stdexcept>

// See https://stackoverflow.com/a/17223443/103724
#include <chrono>  // chrono::system_clock
#include <ctime>   // localtime
#include <sstream> // stringstream
#include <iomanip> // put_time
#include <string>  // string

#include <stdio.h>

#if (defined _WIN32)
#  include <process.h>
#define getpid() _getpid()
#else
#  include <unistd.h>
#endif

#  ifdef __FreeBSD__
#    define fopen64 fopen
#    define ftello64 ftello
#    define fseeko64 fseeko
#  endif
#  ifdef _MSC_VER
#    define fopen64 fopen
#    if (_MSC_VER >= 1400) && (!(defined(NO_MSCVER_FILE64_FUNC)))
#      define ftello64 _ftelli64
#      define fseeko64 _fseeki64
#    else // old MSC
#      define ftello64 ftell
#      define fseeko64 fseek
#    endif
#  endif

/*!
 * \file oes/eml/etp12/Tracer.h
 * \brief Framework to trace ETP 1.2 clients or servers asynchronuously
 */

//============================================================================
//============================================================================

namespace { // copy/paste from SQLite 3.19.3, except from 3 using just below

using oes::common::etp12::EtpTraceHeader;

using u64 = uint64_t;
using u32 = uint32_t;
using u8  = uint8_t;

/*
** SQLITE_MAX_U32 is a u64 constant that is the maximum u64 value
** that can be stored in a u32 without loss of data.  The value
** is 0x00000000ffffffff.  But because of quirks of some compilers, we
** have to specify the value in the less intuitive manner shown:
*/
#define SQLITE_MAX_U32  ((((u64)1)<<32)-1)

#define SQLITE_PRIVATE static

/*
** A macro to hint to the compiler that a function should not be
** inlined.
*/
#if defined(__GNUC__)
#  define SQLITE_NOINLINE  __attribute__((noinline))
#elif defined(_MSC_VER) && _MSC_VER>=1310
#  define SQLITE_NOINLINE  __declspec(noinline)
#else
#  define SQLITE_NOINLINE
#endif

/*
** Routines to read and write variable-length integers.  These used to
** be defined locally, but now we use the varint routines in the util.c
** file.
*/
SQLITE_PRIVATE int sqlite3PutVarint(unsigned char*, u64);
SQLITE_PRIVATE u8 sqlite3GetVarint(const unsigned char *, u64 *);
SQLITE_PRIVATE u8 sqlite3GetVarint32(const unsigned char *, u32 *);
SQLITE_PRIVATE int sqlite3VarintLen(u64 v);

/*
** The common case is for a varint to be a single byte.  They following
** macros handle the common case without a procedure call, but then call
** the procedure for larger varints.
*/
#define getVarint32(A,B)  \
  (u8)((*(A)<(u8)0x80)?((B)=(u32)*(A)),1:sqlite3GetVarint32((A),(u32 *)&(B)))
#define putVarint32(A,B)  \
  (u8)(((u32)(B)<(u32)0x80)?(*(A)=(unsigned char)(B)),1:\
  sqlite3PutVarint((A),(B)))
#define getVarint    sqlite3GetVarint
#define putVarint    sqlite3PutVarint

//============================================================================

bool write_trace_file_header(FILE* ofs, const EtpTraceHeader& file_header) {
    const size_t file_header_size = file_header.encoded_size();
    assert(file_header_size < 64);

    const char magic_cookie[] = "\0EMR/ETP"; // note we write trailing \0 too
    size_t wrote = fwrite(magic_cookie, 1, sizeof(magic_cookie), ofs);
    if (wrote != 9) {
        return false;
    }

    unsigned char type_and_size = static_cast<unsigned char>(file_header_size);
    type_and_size |= (1 << 7);
    wrote = fwrite(&type_and_size, 1, 1, ofs);
    if (wrote != 1) {
        return false;
    }

    unsigned char buf[64];
    const size_t actual_size = file_header.encode(buf);
    assert(actual_size == file_header_size);
    wrote = fwrite(&buf, 1, actual_size, ofs);
    if (wrote != actual_size) {
        return false;
    }

    // CRC32 of header
    boost::crc_32_type crc32;
    crc32.process_bytes(buf, actual_size);
    unsigned int hash = crc32.checksum();
    boost::endian::native_to_little_inplace(hash); // no-op typically
    static_assert(sizeof(hash) == 4, "CRC32 is 4 bytes");
    wrote = fwrite(&hash, 1, sizeof(hash), ofs);
    if (wrote != sizeof(hash)) {
        return false;
    }

    return 0 == fflush(ofs);
}

bool read_trace_file_header(FILE* ifs, EtpTraceHeader& header, std::string& error) {
    unsigned char buf[64];
    size_t read = fread(buf, 1, 10, ifs);
    if (read != 10) {
        error = "File too small";
        return false;
    }
    if (buf[0] != '\0' || buf[8] != '\0' || strncmp("EMR/ETP", (const char*)buf+1, 7)) {
        error = "Expected magic-cookie not found";
        return false;
    }
    unsigned char type_and_size = buf[9];
    if ((type_and_size & (1<<7)) != (1 << 7)) {
        error = "Invalid trace-file header size";
        return false;
    }
    size_t header_size = (type_and_size & 0x3F);
    assert(header_size < 64);

    read = fread(buf, 1, header_size, ifs);
    if (read != header_size) {
        error = "Cannot read trace-file header";
        return false;
    }
    const size_t actual_size = header.decode(buf);
    if (header_size != actual_size) {
        error = "Cannot decide trace-file header";
        return false;
    }

    // CRC32 of header
    unsigned int read_hash;
    read = fread(&read_hash, 1, sizeof(read_hash), ifs);
    boost::endian::little_to_native_inplace(read_hash); // no-op typically
    if (read != sizeof(read_hash)) {
        return false;
    }
    boost::crc_32_type crc32;
    crc32.process_bytes(buf, actual_size);
    const unsigned int computed_hash = crc32.checksum();
    const bool ok = (read_hash == computed_hash);
    if (!ok) {
        error = "Trace-file header checksum mismatch";
    }
    return ok;
}

} // namespace

//============================================================================
//============================================================================

namespace oes::common::etp12 {

//============================================================================

size_t EtpTracerEventInfo::encoded_size(bool header_only) const {
    size_t size = 0;

    // msg is first, to allow skipping unknown events
    size += sqlite3VarintLen(msg_.size());

    auto as_nsec = beg_.time_since_epoch();
    size += sqlite3VarintLen(as_nsec.count());

    as_nsec = std::chrono::duration_cast<std::chrono::nanoseconds>(end_ - beg_);
    size += sqlite3VarintLen(as_nsec.count());

    size += sqlite3VarintLen(tid_);
    size += sqlite3VarintLen(sid_);

    size += sqlite3VarintLen(flags_);

    if (!header_only) {
        size += msg_.size();
    }

    return size;
}

size_t EtpTracerEventInfo::encode(unsigned char* buf, bool header_only) const {
    const unsigned char*const buf0 = buf;

    // msg is first, to allow skipping unknown events
    int size = putVarint(buf, msg_.size());
    buf += size;

    size = putVarint(buf, beg_.time_since_epoch().count());
    buf += size;

    // formatting the duration, since smaller integer thus fewer bytes
    size = putVarint(buf, (end_ - beg_).count());
    buf += size;

    size = putVarint32(buf, tid_);
    buf += size;
    size = putVarint32(buf, sid_);
    buf += size;

    size = putVarint32(buf, flags_);
    buf += size;

    const ptrdiff_t header_size = buf - buf0;
    assert(header_size > 0);

    if (!header_only) {
        std::memcpy(buf, msg_.data(), msg_.size());
        return static_cast<size_t>(header_size) + msg_.size();
    }

    return static_cast<size_t>(header_size);
}

size_t EtpTracerEventInfo::decode(const unsigned char* buf, bool header_only) {
    const unsigned char*const buf0 = buf;

    // msg length is first, to allow skipping unknown events
    u64 len = 0;
    u8 read = getVarint(buf, &len);
    buf += read;

    msg_.resize(len); // how to report the size otherwise?

    u64 nsec = 0;
    read = getVarint(buf, &nsec);
    buf += read;
    decltype(beg_) beg(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::nanoseconds(nsec)
    ));
    beg_ = beg;

    nsec = 0;
    read = getVarint(buf, &nsec);
    buf += read;
    std::chrono::nanoseconds dur(nsec);
    end_ = beg_ + dur;

    read = getVarint32(buf, tid_);
    buf += read;
    read = getVarint32(buf, sid_);
    buf += read;

    read = getVarint32(buf, flags_);
    buf += read;

    const ptrdiff_t header_size = buf - buf0;
    assert(header_size > 0);

    if (!header_only) {
        std::memcpy(&msg_[0], buf, len);
        return static_cast<size_t>(header_size) + msg_.size();
    }

    return static_cast<size_t>(header_size);
}

//============================================================================

EtpTraceHeader EtpTraceHeader::make(Tier tier) {
    EtpTraceHeader hdr;

    hdr.set_tier(tier);

    hdr.started_at_     = std::chrono::system_clock::now();
    hdr.started_at_hrc_ = std::chrono::high_resolution_clock::now();

    hdr.pid_ = getpid();

    return hdr;
}

/*!
 * \brief Gets a trace filename based on this header's ETP version and PID.
 * \param p_host (optional) An host name to blend into the returned filename.
 *     Typically used by client-side tracing for the server's hostname.
 * \return the file name, e.g. EmrEtp12-2016-06-13T09:36:34-02345.trace,
 *     or EmrEtp12-host-2016-06-13T09:36:34-02345.trace
 */
std::string EtpTraceHeader::as_file_name(const std::string* p_host) const {
    std::string prefix = p_host?
        fmt::format("EmrEtp{}{}-{}", etp_major_, etp_minor_, *p_host):
        fmt::format("EmrEtp{}{}", etp_major_, etp_minor_);
    return as_file_name(prefix, pid_, started_at_);
}

/*!
 * \brief Gets a trace filename using the given prefix, process ID, and timestamp.
 *
 * \param prefix the hardcoded prefix to use
 * \param pid the process ID to display, with 0-filled 5 digits
 * \param utc_tp the UTC timestamps, rounded to seconds, to use (converted to local-time)
 * \return the file name, e.g. Prefix-2016-06-13T09:36:34-02345.trace, with \a pid 2345.
 *
 * \note The UTC date-time provided is converted to local-time, for convenience.
 */
std::string EtpTraceHeader::as_file_name(
    const std::string& prefix, int pid,
    std::chrono::system_clock::time_point utc_tp
) {
    using namespace std::chrono;
    using namespace oes::core;
    const auto local_tp = time_point_cast<milliseconds>(utcToLocal(utc_tp));
    std::string date_time = formatDateTime(
        "%Y%m%d-%H%M%S", local_tp, FracSecRes::eWholeSecRound
    );
    return fmt::format("{}-{}-{:05}.trace", prefix, date_time, pid);
}

size_t EtpTraceHeader::encoded_size() const {
    size_t size = 0;

    size += sqlite3VarintLen(format_version_);

    size += sqlite3VarintLen(etp_major_);
    size += sqlite3VarintLen(etp_minor_);
    size += sqlite3VarintLen(etp_patch_);

    size += sqlite3VarintLen(pid_);

    const auto millis_tt = std::chrono::time_point_cast<std::chrono::milliseconds>(started_at_);
    const u64 millis = millis_tt.time_since_epoch().count();
    size += sqlite3VarintLen(millis);

    auto as_nsec = started_at_hrc_.time_since_epoch();
    size += sqlite3VarintLen(as_nsec.count());

    size += sqlite3VarintLen(flags_);

    return size;
}

size_t EtpTraceHeader::encode(unsigned char* buf) const {
    const unsigned char*const buf0 = buf;

    int size = 0;

    size = putVarint32(buf, format_version_);
    buf += size;

    size = putVarint32(buf, etp_major_);
    buf += size;
    size = putVarint32(buf, etp_minor_);
    buf += size;
    size = putVarint32(buf, etp_patch_);
    buf += size;

    size = putVarint32(buf, pid_);
    buf += size;

    // Cannot use nanoseconds on decode() somehow (see below)
    const auto millis_tt = std::chrono::time_point_cast<std::chrono::milliseconds>(started_at_);
    const u64 millis = millis_tt.time_since_epoch().count();
    size = putVarint(buf, millis);
    buf += size;

    size = putVarint(buf, started_at_hrc_.time_since_epoch().count());
    buf += size;

    size = putVarint32(buf, flags_);
    buf += size;

    const ptrdiff_t header_size = buf - buf0;
    assert(0 < header_size && header_size < 64);

    return static_cast<size_t>(header_size);
}

size_t EtpTraceHeader::decode(const unsigned char* buf) {
    const unsigned char*const buf0 = buf;

    u8 read;

    read = getVarint32(buf, format_version_);
    buf += read;

    read = getVarint32(buf, etp_major_);
    buf += read;
    read = getVarint32(buf, etp_minor_);
    buf += read;
    read = getVarint32(buf, etp_patch_);
    buf += read;

    read = getVarint32(buf, pid_);
    buf += read;

    u64 millis = 0;
    read = getVarint(buf, &millis);
    buf += read;

    // See https://stackoverflow.com/a/31258680/103724
    // Note: nanoseconds does NOT work with system_clock! (on MSVC at least)
    std::chrono::milliseconds kmillis(millis);
    std::chrono::system_clock::time_point kbeg(kmillis);
    started_at_ = kbeg;

    u64 nsec = 0;
    read = getVarint(buf, &nsec);
    buf += read;
    decltype(started_at_hrc_) knsec(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::nanoseconds(nsec)
    ));
    started_at_hrc_ = knsec;

    read = getVarint32(buf, flags_);
    buf += read;

    const ptrdiff_t header_size = buf - buf0;
    assert(header_size > 0);

    return static_cast<size_t>(header_size);
}

//============================================================================

/*!
 * \brief Instantiates a new asynchronuous ETP tracer.
 *
 * This method does not start the tracer (or its thread), but it does
 * write the trace \a file_header to \a filename.
 *
 * \param filename Name of a regular file to trace into.
 *     That file may not exist yet, and will be created,
 *     but its parent directory must already exist and be valid.
 * \param file_header for the version of ETP required to decode Avro binary messages.
 *
 * \see trace_filename() to auto-generate a trace filename
 */
EtpTracer12::EtpTracer12(
    std::string filename, const EtpTraceHeader& file_header
) : filename_(std::move(filename))
{
#if _WIN32
    // Add special Windows-specific "commit" flag, to:
    // Enable the commit flag for the associated filename so that the contents of the
    // file buffer are written directly to disk if either fflush or _flushall is called.
    ofs_ = fopen64(filename_.c_str(), "abc");
#else
    ofs_ = fopen64(filename_.c_str(), "ab");
#endif
    if (!ofs_) {
        throw std::runtime_error("Cannot open file: " + filename_);
    }

    if (!write_trace_file_header((FILE*)ofs_, file_header)) {
        throw std::runtime_error("Error writing trace file header: " + filename_);
    }
    event_queue_.reserve(32);
}

EtpTracer12::~EtpTracer12() {
    stop();
}

/*!
 * \brief Infer a trace filename, from a empty name or a folder name.
 *
 * \param trace_file_or_folder
 *     - when empty, uses EtpTraceHeader::as_file_name() to infer a filename
 *       relative to the current directory.
 *     - when matching an existing file, returns that file name as-is.
 *     - when matching an existing folder / directory, uses EtpTraceHeader::as_file_name()
 *       to infer a filename in that specific directory.
 *     - when neither an existing file or directory, but the parent folder of that
 *       path is valid, assume it is a to-be-created file, to uses that name as-is.
 *     - all other cases are an error.
 * \param p_header (optional) header to get process ID and UTC timestamp from.
 *     If null, a temporary one is used instead.
 * \param p_host (optional) An host name to blend into the returned filename.
 *     Typically used by client-side tracing for the server's hostname.
 * \return the existing or to-be-created trace file name,
 *     relative to the current directory, or an absolute path when
 *     \a trace_file_or_folder is itself an absolute directory.
 *
 * \throws std::runtime_error on error
 * \see EtpTraceHeader::as_file_name(const std::string*)
 */
std::string EtpTracer12::trace_filename(
    std::string trace_file_or_folder,
    const EtpTraceHeader* p_header,
    const std::string* p_host
) {
    EtpTraceHeader local_header;
    if (!p_header) {
        local_header = EtpTraceHeader::make();
    }
    const EtpTraceHeader& header = p_header? *p_header: local_header;

    if (trace_file_or_folder.empty()) {
        return header.as_file_name(p_host);
    }

    namespace fs = std::filesystem;
    fs::path p(trace_file_or_folder);
    fs::file_status fstat = fs::status(p);
    if (fs::is_regular_file(fstat)) {
        return trace_file_or_folder; // use existing file as-is
    }

    if (fs::is_directory(fstat)) {
        std::string rel_path = header.as_file_name(p_host);
        fs::path abs_path = p / rel_path;
        return abs_path.string();
    }

    fs::path pp = p.parent_path();
    fs::file_status dstat = fs::status(pp);
    if (!fs::is_directory(dstat)) {
        throw std::runtime_error("Invalid trace output filename: " + trace_file_or_folder);
    }

    // to-be-created file, in valid folder
    return trace_file_or_folder;
}

// starts backend thread, waiting until it's ready to receive events
void EtpTracer12::start() {
    {
        std::lock_guard<std::mutex> guard(mt_);
        if (backend_status_ != eSTOPPED) {
            return; // already started; silently ignored
        }
        timeout_ = std::chrono::milliseconds(500);
        backend_ = std::thread([this]() {
            this->process_events();
        });
    }
    wait_until(eRUNNING);
}

void EtpTracer12::async_trace(EtpTracerEventInfo info) {
    assert(!info.msg_.empty());

    std::unique_lock<std::mutex> guard(mt_);
    event_queue_.emplace_back(std::move(info));

    guard.unlock();
    cv_.notify_one();
}

// Instructs / schedules backend to flush its trace file,
// returning immediately before the flush may have occurred.
void EtpTracer12::async_flush() {
    {
        std::lock_guard<std::mutex> guard(mt_);
        if (backend_status_ != eSHOULD_STOP) {
            backend_status_ = eSHOULD_FLUSH;
        }
    }
    cv_.notify_one();
}

// Instructs / schedules backend thread to stop, then wait for it to actually finish.
void EtpTracer12::stop() {
    {
        std::lock_guard<std::mutex> guard(mt_);
        if (backend_status_ == eSTOPPED) {
            return; // already stopped; silently ignored
        }
        // if stop() immediately follows a async_flush(),
        // it may "subsume" it (since closing the file also flushes it)
        backend_status_ = eSHOULD_STOP;
    }
    cv_.notify_one();
    wait_until(eSTOPPED);

    if (backend_.joinable()) {
        backend_.join();
    }
}

size_t EtpTracer12::io_error_count() {
    std::lock_guard<std::mutex> guard(mt_);
    return io_errors_;
}

// Waits until the backend thread's status matches \a status.
void EtpTracer12::wait_until(int status) {
    std::unique_lock<std::mutex> guard(mt_);
    cv_.wait(guard, [this, status] {
        return backend_status_ == status;
    });
}

// Backend-thread processing loop.
void EtpTracer12::process_events() {
    decltype(event_queue_) local_queue;
    local_queue.reserve(32);

    // notify start() method we are ready to run
    {
        std::lock_guard<std::mutex> guard(mt_);
        backend_status_ = eRUNNING;
    }
    cv_.notify_one();

    for (;;) {
        std::unique_lock<std::mutex> guard(mt_);
        const bool timed_out = (false == cv_.wait_for(
            guard, timeout_, [this] {
                return
                    !this->event_queue_.empty() ||
                    (backend_status_ != eRUNNING)
                ;
            }
        ));

        if (timed_out && (0 == written_since_last_flush_)) {
            // nothing to flush, so backout strategy to wake-up less often
            if (timeout_ < std::chrono::minutes(1)) {
                timeout_ *= 2;
            }
            continue; // go back to waiting for events (or a timeout)
        } else {
            timeout_ = std::chrono::milliseconds(500);
        }

        bool do_flush = false;

        switch (backend_status_) {
        case eSTOPPED: // should not happen
            return;
        case eRUNNING:
            break;
        case eSHOULD_STOP:
            // drain the event queue before stopping
            if (!event_queue_.empty()) {
                size_t written = 0;
                if (!write_events((FILE*)ofs_, event_queue_, written)) {
                    ++io_errors_;
                }
                written_since_last_flush_ += written;
            }
            if (0 != fclose((FILE*)ofs_)) {
                ++io_errors_;
            }
            // notify stop() method we are actually stopped
            backend_status_ = eSTOPPED;
            guard.unlock();
            cv_.notify_one();
            return;
        case eSHOULD_FLUSH:
            do_flush = true;
            backend_status_ = eRUNNING;
            break;
        }

        bool unlocked = false;

        if (!event_queue_.empty()) {
            event_queue_.swap(local_queue);
            guard.unlock();
            unlocked = true;

            size_t written = 0;
            if (!write_events((FILE*)ofs_, local_queue, written)) {
                guard.lock();
                ++io_errors_;
                continue;
            }
            written_since_last_flush_ += written;
        }

        // FIXME: May flush events "posted" after the flush too.
        // Record size of query at flush() request time, to flush only those?

        // Flush when:
        // 1) explicitly requested,
        // 2) soon after (1/2s) after the last written event(s),
        // 3) if we wrote more than 8MB (arbitrary threshold)
        if (do_flush || timed_out || (written_since_last_flush_ > 8*1024*1024)) {
            // no need to block other threads from posting events while we flush
            if (!unlocked) {
                guard.unlock();
            }

            // On Windows at least, this does not seem enough to have the bytes
            // reach the file on disk... Need to kill the ETP server for the trace
            // file to be non-zero. While closing the file "works"...
            if (0 != fflush((FILE*)ofs_)) {
                guard.lock();
                ++io_errors_;
                continue;
            }
            written_since_last_flush_ = 0;
        }

        // go back to waiting for events
    }
}

// Backend-thread specific actual IO. Uses only input args
bool EtpTracer12::write_events(
    FILE* ofs, EtpTracerEventVec& event_queue, size_t& written
) {
    assert(!event_queue.empty());

    // Note: Using fopen/fwrite rather than std::ofstream because of
    // https://cristianadam.eu/20160410/c-plus-plus-i-slash-o-benchmark/
    // but also because of Windows-specific "commit" flag (see Ctor).

    size_t& total_written = written = 0;
    unsigned char buf[EtpTracerEventInfo::max_header_size()];
    for (const auto& evt : event_queue) {
        boost::crc_32_type crc32;

        const size_t event_header_size = evt.encode(&buf[1], true);
        assert(event_header_size < 64); // since high-bit (bit-7) reserved for magic cookie
        buf[0] = static_cast<unsigned char>(event_header_size); // (bit-6 reserved for future)

        crc32.process_bytes(buf + 1, event_header_size); // header-size-byte not part of CRC
        size_t wrote = fwrite(reinterpret_cast<const char*>(buf), 1, event_header_size + 1, ofs);
        if (wrote != (event_header_size + 1)) {
            return false;
        }
        total_written += (event_header_size + 1);

        const size_t msg_bytes = evt.msg_.size();
        if (msg_bytes > 0) {
            crc32.process_bytes(evt.msg_.data(), msg_bytes);
            wrote = fwrite(reinterpret_cast<const char*>(evt.msg_.data()), 1, msg_bytes, ofs);
            if (wrote != msg_bytes) {
                return false;
            }
            total_written += msg_bytes;
        }

        unsigned int hash = crc32.checksum();
        boost::endian::native_to_little_inplace(hash); // no-op typically
        static_assert(sizeof(hash) == 4, "CRC32 is 4 bytes");
        wrote = fwrite(&hash, 1, sizeof(hash), ofs);
        if (wrote != sizeof(hash)) {
            return false;
        }
    }

    // std::cerr << "\nTRACE: Wrote " << total_written << " bytes from "
    //     << event_queue.size() << " events\n" << std::endl;

    event_queue.clear(); // processed events
    return true;
}

auto file_remaining_bytes(FILE* ifs) {
    const auto curr_pos = ftello64(ifs);
    fseeko64(ifs, 0, SEEK_END);
    const auto end_pos = ftello64(ifs);
    const decltype(end_pos) remaining = end_pos - curr_pos;
    if (curr_pos != end_pos) {
        fseeko64(ifs, curr_pos, SEEK_SET); // go back
    }
    return remaining;
}

//============================================================================

struct EtpTracerReader12Impl {
    FILE*const ifs_;
    const size_t max_events_per_call_;
    const size_t io_buffer_size_;
    size_t file_buf_pos_ = 0;
    std::vector<unsigned char> file_buf_;
    EtpTraceHeader curr_file_header_;
    unsigned curr_file_header_id_ = 0;
    bool file_eof_ = false;
    bool trace_eof_ = false;
    size_t io_errors_ = 0;

    EtpTracerReader12Impl(
        FILE* ifs, const EtpTraceHeader& first_trace_header,
        size_t max_events_per_call, size_t io_buffer_size
    );
    ~EtpTracerReader12Impl();

    bool restart();
    EtpTracerEventVec next_events(EtpTraceReaderCtx& ctx);

    bool next_event(EtpTracerEventInfo& evt);
    bool read_msg(EtpTracerEventInfo& evt);
    bool read_hash(unsigned int computed_hash);

    bool ensure_can_read(size_t at_least);
    bool ensure_can_read_header();

    bool at_eof();
    bool fread_more(size_t at_least);
    bool fread_into(void* dest, size_t to_read);
};

EtpTracerReader12Impl::EtpTracerReader12Impl(
    FILE* ifs, const EtpTraceHeader& first_trace_header,
    size_t max_events_per_call, size_t io_buffer_size
) : ifs_(ifs)
  , max_events_per_call_(max_events_per_call)
  , io_buffer_size_(io_buffer_size)
  , file_buf_pos_(io_buffer_size)
  , file_buf_(io_buffer_size)
  , curr_file_header_(first_trace_header)
  , curr_file_header_id_(1)
{
}

EtpTracerReader12Impl::~EtpTracerReader12Impl() {
    if (ifs_) {
        fclose(ifs_);
    }
}

bool EtpTracerReader12Impl::restart() {
    assert(trace_eof_);
    assert(file_buf_.size() > file_buf_pos_);

    // We have buffered some content, but a restart
    // needs the FILE pointer to be at the start of the new trace.
    const size_t remaining = file_buf_.size() - file_buf_pos_;
    std::int64_t rewind_of = static_cast<std::int64_t>(remaining);
    fseeko64(ifs_, -rewind_of, SEEK_CUR);

    std::string error;
    EtpTraceHeader file_header;
    if (!read_trace_file_header(ifs_, file_header, error)) {
        return false;
    }

    if (file_header.format_version_ != 1) {
        return false;
    }

    file_buf_.resize(io_buffer_size_);
    file_buf_pos_ = io_buffer_size_;

    file_eof_ = false;
    trace_eof_ = false;

    curr_file_header_ = file_header;
    ++curr_file_header_id_;

    return true;
}

EtpTracerEventVec EtpTracerReader12Impl::next_events(EtpTraceReaderCtx& ctx) {
    EtpTracerEventVec events;

    if (trace_eof_ && !io_errors_) {
        if (!restart()) {
            ++io_errors_;
        }
    }

    for (size_t i = 0; i < max_events_per_call_; ++i) {
        if (io_errors_ || at_eof()) {
            break;
        }
        events.emplace_back();
        auto& evt = events.back();
        if (!next_event(evt)) {
            events.resize(events.size() -1);
            break;
        }
    }

    ctx.eof_ = at_eof();
    ctx.io_errors_ = io_errors_;

    ctx.trace_id_ = curr_file_header_id_;
    ctx.trace_header_ = &curr_file_header_;

    return events;
}

bool EtpTracerReader12Impl::next_event(EtpTracerEventInfo& evt) {
    if (!ensure_can_read_header()) {
        return false;
    }

    const unsigned char event_header_size = file_buf_[file_buf_pos_];
    if (event_header_size == 0) {
        trace_eof_ = true;
        return false; // "concatenated trace file"
    }
    ++file_buf_pos_; // warning: consume the char only when not "restart'ing"

    boost::crc_32_type crc32;

    const size_t bytes_used = evt.decode(&file_buf_[file_buf_pos_], true);
    if (bytes_used != event_header_size) {
        ++io_errors_;
        return false;
    }
    crc32.process_bytes(&file_buf_[file_buf_pos_], bytes_used);
    file_buf_pos_ += bytes_used;

    if (!evt.msg_.empty()) {
        const bool msg_ok = read_msg(evt);
        if (!msg_ok) {
            return false;
        }
        crc32.process_bytes(evt.msg_.data(), evt.msg_.size());
    }

    const unsigned int computed_hash = crc32.checksum();
    return read_hash(computed_hash);
}

bool EtpTracerReader12Impl::ensure_can_read_header() {
    assert(!at_eof()); // cppcheck-suppress assertWithSideEffect

    // +1 because headers are preceded with 1-byte header-byte-size
    if (!ensure_can_read(1 + EtpTracerEventInfo::min_header_size())) {
        return false;
    }

    const unsigned char event_header_size = file_buf_[file_buf_pos_];
    // Not ++file_buf_pos_ since next_event() will read it again

    assert(event_header_size <= EtpTracerEventInfo::max_header_size());
    if (event_header_size >= 64) {
        ++io_errors_;
        return false; // invalid format
    }

    // above we made sure we could read any minimal header,
    // but now we must ensure we can read the *current* header,
    // now that we now it's actual size.
    return ensure_can_read(1 + event_header_size);
}

// \return whether the msg was read from memory
bool EtpTracerReader12Impl::read_msg(EtpTracerEventInfo& evt) {
    assert(!at_eof()); // cppcheck-suppress assertWithSideEffect

    const size_t msg_size = evt.msg_.size();
    const size_t buffered = file_buf_.size();
    assert(buffered >= file_buf_pos_);
    const size_t remaining = buffered - file_buf_pos_;

    if (remaining >= msg_size) {
        // easy case, msg already in memory.
        std::memcpy(&evt.msg_[0], &file_buf_[file_buf_pos_], msg_size);
        file_buf_pos_ += msg_size;
        return true;
    }

    // copy the part we already have in memory
    std::memcpy(&evt.msg_[0], &file_buf_[file_buf_pos_], remaining);

    // and fetch the rest directly from file, bypassing file_buf_
    const size_t to_read = msg_size - remaining;
    if (!fread_into(&evt.msg_[remaining], to_read)) {
        return false;
    }

    // as-if all content was consumed, w/o reading the next event(s)
    file_buf_pos_ = buffered;
    return true;
}

bool EtpTracerReader12Impl::read_hash(unsigned int computed_hash) {
    const size_t hash_size = sizeof(computed_hash);
    if (!ensure_can_read(hash_size)) {
        return false;
    }

    unsigned int read_hash;
    std::memcpy(&read_hash, &file_buf_[file_buf_pos_], hash_size);
    boost::endian::little_to_native_inplace(read_hash); // no-op typically
    file_buf_pos_ += hash_size;

    return read_hash == computed_hash;
}

bool EtpTracerReader12Impl::at_eof() {
    if (file_buf_pos_ < file_buf_.size()) {
        return false;
    }
    if (file_eof_) {
        return true;
    }
    const auto file_remaining = file_remaining_bytes(ifs_);
    if (file_remaining <= 0) {
        file_eof_ = true;
      //file_buf_pos_ = file_buf_.size();
        return true;
    }
    return false;
}

// Must read exactly to_read bytes from the trace file.
bool EtpTracerReader12Impl::fread_into(void* dest, size_t to_read) {
    const size_t has_read = fread(dest, 1, to_read, ifs_);
    if (to_read != has_read) {
        // truncated file?
        ++io_errors_;
        file_buf_.clear();
        file_buf_pos_ = 0;
        return false;
    }
    return true;
}

bool EtpTracerReader12Impl::ensure_can_read(size_t at_least) {
    const size_t buffered = file_buf_.size();
    assert(buffered >= file_buf_pos_);
    const size_t remaining = buffered - file_buf_pos_;

    if (remaining >= at_least) {
        return true;
    }

    if (!fread_more(at_least - remaining)) {
        return false;
    }

    const size_t new_size = file_buf_.size();
    assert(new_size >= file_buf_pos_);
    const size_t new_remaining = new_size - file_buf_pos_;

    assert(new_remaining >= at_least);
    return new_remaining >= at_least;
}

// Must read at_least that many bytes into our internal buffer,
// shifting buffered-but-unread content to the left.
// Warning: Affects file_buf_pos_ and possibly file_buf_.size() too.
bool EtpTracerReader12Impl::fread_more(size_t at_least) {
    const size_t buffered = file_buf_.size();
    assert(buffered >= file_buf_pos_);
    const size_t remaining = buffered - file_buf_pos_;

    // keep "remaining" bytes, if any, by shifting them to the front
    if (remaining > 0) {
        // Use "ring buffer" to avoid moving memory around?
        // Only 32 bytes, not worth it I suspect
        //file_buf_.erase(file_buf_.begin(), file_buf_.begin() + file_buf_pos_);
        //file_buf_.resize(buffered);
        assert(remaining <= file_buf_pos_); // i.e. no overlap (thus not memmove)
        std::memcpy(&file_buf_[0], &file_buf_[file_buf_pos_], remaining);
    }
    file_buf_pos_ = 0;

    const size_t to_read = buffered - remaining; // try to fill our buffer
    if (to_read < at_least) {
        ++io_errors_;
        assert(to_read < at_least);
        return false; // logic error in the code IMHO
    }

    const size_t has_read = fread(&file_buf_[remaining], 1, to_read, ifs_);
    if (has_read < to_read) {
        // Can happen when legitimately reaching EOF,
        // since we tried to fully refill our internal buffer.
        file_buf_.resize(remaining + has_read); // truncate file_buf_

        // at_least indicates we know for sure that a valid trace
        // needs to have that many bytes left to read, thus if we
        // reached EOF before, the trace is corurpt, perhaps truncated
        // from missing flushes or a crash at trace-generation time.
        if (has_read < at_least) {
            ++io_errors_;
            return false; // truncated file?
        }
    }

    return true;
}

//============================================================================

EtpTraceReader12 makeEtpTraceFileReader12(
    const std::string& filename,
    EtpTraceHeader& first_trace_header,
    size_t max_events_per_call,
    size_t io_buffer_size
) {
    FILE* ifs = fopen64(filename.c_str(), "rb");
    if (!ifs) {
        throw std::runtime_error("Cannot open file: " + filename);
    }

    std::string error;
    if (!read_trace_file_header(ifs, first_trace_header, error)) {
        throw std::runtime_error("Not an ETP trace-file: " + filename);
    }

    if (first_trace_header.format_version_ != 1) {
        throw std::runtime_error("Unknown ETP trace-file format version: " + filename);
    }

    std::shared_ptr<EtpTracerReader12Impl> impl(new EtpTracerReader12Impl(
        ifs, first_trace_header,
        std::max<>(size_t(1), max_events_per_call),
        std::max<>(size_t(4*1024), io_buffer_size)
    ));
    return [impl](EtpTraceReaderCtx& ctx) {
        return impl->next_events(ctx);
    };
}

//============================================================================

bool validateEtpTraceFile(
    const std::string& filename,
    EtpTraceHeader& file_header,
    std::ostream& out, std::ostream& err,
    size_t io_buffer_size
) {
    using std::endl;

    FILE* ifs = fopen64(filename.c_str(), "rb");
    if (!ifs) {
        err << "Error: Cannot open file: " << filename << endl;
        return false;
    }

    if (0 != setvbuf(ifs, nullptr, _IOFBF, io_buffer_size)) {
        err << "Warning: setvbuf error" << endl;
    }

    size_t trace_count = 0;
    size_t total_hdr_bytes = 0;
    size_t total_msg_bytes = 0;
    size_t total_event_count = 0;
    size_t error_count = 0;

    // traces may be concatenated, thus this outer loop
    while (!feof(ifs)) {
        std::string error;
        if (!read_trace_file_header(ifs, file_header, error)) {
            err << "Error: Not an ETP trace-file: " << filename << endl;
            err << "Error: " << error << endl;
            fclose(ifs);
            return false;
        }

        if (file_header.format_version_ != 1) {
            err << "Warning: Unknown trace-file format-version: "
                << file_header.format_version_ << endl;
        }

        ++trace_count;

        std::string tier = "???";
        switch (file_header.tier()) {
        case EtpTraceHeader::Tier::eCLIENT: tier = "CLIENT"; break;
        case EtpTraceHeader::Tier::eSERVER: tier = "SERVER"; break;
        case EtpTraceHeader::Tier::ePROXY:  tier = "PROXY ";  break;
        } // no default on purpose

        fmt::print(
            out, "{} ETP {}.{}.{} (PID {:05}) {} UTC\n",
            tier,
            file_header.etp_major_,
            file_header.etp_minor_,
            file_header.etp_patch_,
            file_header.pid_,
            oes::core::formatExtendedIsoDateTime(
                file_header.started_at_,
                oes::core::FracSecRes::eMilliSecRound
            )
        );

        size_t hdr_bytes = 0;
        size_t msg_bytes = 0;

        size_t event_count = 0;
        size_t crc_err_count = 0;
        unsigned char hdr_buf[EtpTracerEventInfo::max_header_size()];
        std::vector<unsigned char> msg_buf_vec(io_buffer_size);
        unsigned char*const msg_buf = &msg_buf_vec[0];

        for (;;) {
            int c = fgetc(ifs);
            if (c == EOF) {
                break;
            }
            if (c == 0) {
                ungetc(c, ifs); // part of magic-cookie, so must be read next (again)
                break; // only valid way is another "concatenated" trace
            }
            if (c & ~0x3F) {
                ++error_count;
                err << "Error: Invalid event header-size" << endl;
                break; // cannot recover
            }
            size_t to_read = static_cast<size_t>(c);
            size_t read = fread(hdr_buf, 1, to_read, ifs);
            if (read != to_read) {
                ++error_count;
                err << "Error: Truncated event header-size" << endl;
                break; // cannot recover
            }
            hdr_bytes += read;

            boost::crc_32_type crc32;
            crc32.process_bytes(hdr_buf, read);

            // message size is always first, thus
            // no need to get exact detail about the header to validate it
            u64 len = 0;
            u8 varint_read = getVarint(hdr_buf, &len);
            OES_RELEASE_UNUSED(varint_read);
            assert(varint_read < to_read);

            size_t actually_read = 0;
            bool msg_truncated = false;
            for (to_read = len; 0 < to_read; to_read -= read) {
                const size_t this_to_read = std::min<>(io_buffer_size, to_read);
                read = fread(msg_buf, 1, this_to_read, ifs);
                actually_read += read;
                if (read != this_to_read) {
                    ++error_count;
                    msg_truncated = true;
                    err << "Error: Truncated event message: Expected " << len
                        << " bytes; read " << actually_read << endl;
                    break; // cannot recover
                }
                crc32.process_bytes(msg_buf, this_to_read);
            }

            if (msg_truncated) {
                break; // cannot recover
            }

            msg_bytes += len;

            const unsigned int computed_hash = crc32.checksum();

            unsigned int read_hash;
            to_read = sizeof(read_hash);
            read = fread(&read_hash, 1, to_read, ifs);
            boost::endian::little_to_native_inplace(read_hash); // no-op typically
            if (read != to_read) {
                ++error_count;
                err << "Error: Truncated event checksum" << endl;
                break; // cannot recover
            }

            if (read_hash != computed_hash) {
                ++error_count;
                ++crc_err_count;
                if (error_count > 10) {
                    err << "Error: Event checksum mismatch: too many errors" << endl;
                    break; // not worth it, we're out-of-sync in the file most likely
                } else {
                    // try continuing
                    err << "Error: Event checksum mismatch: continue anyway" << endl;
                }
            }

            ++event_count;
        }

        out << event_count << " trace events";
        if (crc_err_count) {
            out << " (" << crc_err_count << " with checksum errors)";
        }

        out << "; " << msg_bytes << " message bytes, " << hdr_bytes << " header bytes." << endl;

        total_hdr_bytes   += hdr_bytes;
        total_msg_bytes   += msg_bytes;
        total_event_count += event_count;

        if (error_count > 0) {
            err << "Error: Cannot recover/continue: Aborting." << endl;
            break;
        }
    }

    if (error_count > 0) {
        if (!feof(ifs)) {
            err << "Error: Aborted before reading the whole trace file." << endl;
        } else {
            err << "Error: Unexpected end-of-file: Truncated trace file?" << endl;
        }
    }
    fclose(ifs);

    if (trace_count > 1) {
        out << endl << "TOTAL: " << trace_count << " traces, "
            << total_event_count << " events; "
            << total_msg_bytes << " message bytes, "
            << total_hdr_bytes << " header bytes." << endl;
    }

    return error_count == 0;
}

} // namespace oes::common::etp12

//============================================================================
//============================================================================

namespace { // copy/paste from SQLite 3.19.3

/*
** The variable-length integer encoding is as follows:
**
** KEY:
**         A = 0xxxxxxx    7 bits of data and one flag bit
**         B = 1xxxxxxx    7 bits of data and one flag bit
**         C = xxxxxxxx    8 bits of data
**
**  7 bits - A
** 14 bits - BA
** 21 bits - BBA
** 28 bits - BBBA
** 35 bits - BBBBA
** 42 bits - BBBBBA
** 49 bits - BBBBBBA
** 56 bits - BBBBBBBA
** 64 bits - BBBBBBBBC
*/

/*
** Write a 64-bit variable-length integer to memory starting at p[0].
** The length of data write will be between 1 and 9 bytes.  The number
** of bytes written is returned.
**
** A variable-length integer consists of the lower 7 bits of each byte
** for all bytes that have the 8th bit set and one byte with the 8th
** bit clear.  Except, if we get to the 9th byte, it stores the full
** 8 bits and is the last byte.
*/
static int SQLITE_NOINLINE putVarint64(unsigned char *p, u64 v){
  int i, j, n;
  u8 buf[10];
  if( v & (((u64)0xff000000)<<32) ){
    p[8] = (u8)v;
    v >>= 8;
    for(i=7; i>=0; i--){
      p[i] = (u8)((v & 0x7f) | 0x80);
      v >>= 7;
    }
    return 9;
  }
  n = 0;
  do{
    buf[n++] = (u8)((v & 0x7f) | 0x80);
    v >>= 7;
  }while( v!=0 );
  buf[0] &= 0x7f;
  assert( n<=9 );
  for(i=0, j=n-1; j>=0; j--, i++){
    p[i] = buf[j];
  }
  return n;
}
SQLITE_PRIVATE int sqlite3PutVarint(unsigned char *p, u64 v){
  if( v<=0x7f ){
    p[0] = v&0x7f;
    return 1;
  }
  if( v<=0x3fff ){
    p[0] = ((v>>7)&0x7f)|0x80;
    p[1] = v&0x7f;
    return 2;
  }
  return putVarint64(p,v);
}

/*
** Bitmasks used by sqlite3GetVarint().  These precomputed constants
** are defined here rather than simply putting the constant expressions
** inline in order to work around bugs in the RVT compiler.
**
** SLOT_2_0     A mask for  (0x7f<<14) | 0x7f
**
** SLOT_4_2_0   A mask for  (0x7f<<28) | SLOT_2_0
*/
#define SLOT_2_0     0x001fc07f
#define SLOT_4_2_0   0xf01fc07f


/*
** Read a 64-bit variable-length integer from memory starting at p[0].
** Return the number of bytes read.  The value is stored in *v.
*/
SQLITE_PRIVATE u8 sqlite3GetVarint(const unsigned char *p, u64 *v){
  u32 a,b,s;

  a = *p;
  /* a: p0 (unmasked) */
  if (!(a&0x80))
  {
    *v = a;
    return 1;
  }

  p++;
  b = *p;
  /* b: p1 (unmasked) */
  if (!(b&0x80))
  {
    a &= 0x7f;
    a = a<<7;
    a |= b;
    *v = a;
    return 2;
  }

  /* Verify that constants are precomputed correctly */
  assert( SLOT_2_0 == ((0x7f<<14) | (0x7f)) );
  assert( SLOT_4_2_0 == ((0xfU<<28) | (0x7f<<14) | (0x7f)) );

  p++;
  a = a<<14;
  a |= *p;
  /* a: p0<<14 | p2 (unmasked) */
  if (!(a&0x80))
  {
    a &= SLOT_2_0;
    b &= 0x7f;
    b = b<<7;
    a |= b;
    *v = a;
    return 3;
  }

  /* CSE1 from below */
  a &= SLOT_2_0;
  p++;
  b = b<<14;
  b |= *p;
  /* b: p1<<14 | p3 (unmasked) */
  if (!(b&0x80))
  {
    b &= SLOT_2_0;
    /* moved CSE1 up */
    /* a &= (0x7f<<14)|(0x7f); */
    a = a<<7;
    a |= b;
    *v = a;
    return 4;
  }

  /* a: p0<<14 | p2 (masked) */
  /* b: p1<<14 | p3 (unmasked) */
  /* 1:save off p0<<21 | p1<<14 | p2<<7 | p3 (masked) */
  /* moved CSE1 up */
  /* a &= (0x7f<<14)|(0x7f); */
  b &= SLOT_2_0;
  s = a;
  /* s: p0<<14 | p2 (masked) */

  p++;
  a = a<<14;
  a |= *p;
  /* a: p0<<28 | p2<<14 | p4 (unmasked) */
  if (!(a&0x80))
  {
    /* we can skip these cause they were (effectively) done above
    ** while calculating s */
    /* a &= (0x7f<<28)|(0x7f<<14)|(0x7f); */
    /* b &= (0x7f<<14)|(0x7f); */
    b = b<<7;
    a |= b;
    s = s>>18;
    *v = ((u64)s)<<32 | a;
    return 5;
  }

  /* 2:save off p0<<21 | p1<<14 | p2<<7 | p3 (masked) */
  s = s<<7;
  s |= b;
  /* s: p0<<21 | p1<<14 | p2<<7 | p3 (masked) */

  p++;
  b = b<<14;
  b |= *p;
  /* b: p1<<28 | p3<<14 | p5 (unmasked) */
  if (!(b&0x80))
  {
    /* we can skip this cause it was (effectively) done above in calc'ing s */
    /* b &= (0x7f<<28)|(0x7f<<14)|(0x7f); */
    a &= SLOT_2_0;
    a = a<<7;
    a |= b;
    s = s>>18;
    *v = ((u64)s)<<32 | a;
    return 6;
  }

  p++;
  a = a<<14;
  a |= *p;
  /* a: p2<<28 | p4<<14 | p6 (unmasked) */
  if (!(a&0x80))
  {
    a &= SLOT_4_2_0;
    b &= SLOT_2_0;
    b = b<<7;
    a |= b;
    s = s>>11;
    *v = ((u64)s)<<32 | a;
    return 7;
  }

  /* CSE2 from below */
  a &= SLOT_2_0;
  p++;
  b = b<<14;
  b |= *p;
  /* b: p3<<28 | p5<<14 | p7 (unmasked) */
  if (!(b&0x80))
  {
    b &= SLOT_4_2_0;
    /* moved CSE2 up */
    /* a &= (0x7f<<14)|(0x7f); */
    a = a<<7;
    a |= b;
    s = s>>4;
    *v = ((u64)s)<<32 | a;
    return 8;
  }

  p++;
  a = a<<15;
  a |= *p;
  /* a: p4<<29 | p6<<15 | p8 (unmasked) */

  /* moved CSE2 up */
  /* a &= (0x7f<<29)|(0x7f<<15)|(0xff); */
  b &= SLOT_2_0;
  b = b<<8;
  a |= b;

  s = s<<4;
  b = p[-4];
  b &= 0x7f;
  b = b>>3;
  s |= b;

  *v = ((u64)s)<<32 | a;

  return 9;
}

/*
** Read a 32-bit variable-length integer from memory starting at p[0].
** Return the number of bytes read.  The value is stored in *v.
**
** If the varint stored in p[0] is larger than can fit in a 32-bit unsigned
** integer, then set *v to 0xffffffff.
**
** A MACRO version, getVarint32, is provided which inlines the
** single-byte case.  All code should use the MACRO version as
** this function assumes the single-byte case has already been handled.
*/
SQLITE_PRIVATE u8 sqlite3GetVarint32(const unsigned char *p, u32 *v){
  u32 a,b;

  /* The 1-byte case.  Overwhelmingly the most common.  Handled inline
  ** by the getVarin32() macro */
  a = *p;
  /* a: p0 (unmasked) */
#ifndef getVarint32
  if (!(a&0x80))
  {
    /* Values between 0 and 127 */
    *v = a;
    return 1;
  }
#endif

  /* The 2-byte case */
  p++;
  b = *p;
  /* b: p1 (unmasked) */
  if (!(b&0x80))
  {
    /* Values between 128 and 16383 */
    a &= 0x7f;
    a = a<<7;
    *v = a | b;
    return 2;
  }

  /* The 3-byte case */
  p++;
  a = a<<14;
  a |= *p;
  /* a: p0<<14 | p2 (unmasked) */
  if (!(a&0x80))
  {
    /* Values between 16384 and 2097151 */
    a &= (0x7f<<14)|(0x7f);
    b &= 0x7f;
    b = b<<7;
    *v = a | b;
    return 3;
  }

  /* A 32-bit varint is used to store size information in btrees.
  ** Objects are rarely larger than 2MiB limit of a 3-byte varint.
  ** A 3-byte varint is sufficient, for example, to record the size
  ** of a 1048569-byte BLOB or string.
  **
  ** We only unroll the first 1-, 2-, and 3- byte cases.  The very
  ** rare larger cases can be handled by the slower 64-bit varint
  ** routine.
  */
#if 1
  {
    u64 v64;
    u8 n;

    p -= 2;
    n = sqlite3GetVarint(p, &v64);
    assert( n>3 && n<=9 );
    if( (v64 & SQLITE_MAX_U32)!=v64 ){
      *v = 0xffffffff;
    }else{
      *v = (u32)v64;
    }
    return n;
  }

#else
  /* For following code (kept for historical record only) shows an
  ** unrolling for the 3- and 4-byte varint cases.  This code is
  ** slightly faster, but it is also larger and much harder to test.
  */
  p++;
  b = b<<14;
  b |= *p;
  /* b: p1<<14 | p3 (unmasked) */
  if (!(b&0x80))
  {
    /* Values between 2097152 and 268435455 */
    b &= (0x7f<<14)|(0x7f);
    a &= (0x7f<<14)|(0x7f);
    a = a<<7;
    *v = a | b;
    return 4;
  }

  p++;
  a = a<<14;
  a |= *p;
  /* a: p0<<28 | p2<<14 | p4 (unmasked) */
  if (!(a&0x80))
  {
    /* Values  between 268435456 and 34359738367 */
    a &= SLOT_4_2_0;
    b &= SLOT_4_2_0;
    b = b<<7;
    *v = a | b;
    return 5;
  }

  /* We can only reach this point when reading a corrupt database
  ** file.  In that case we are not in any hurry.  Use the (relatively
  ** slow) general-purpose sqlite3GetVarint() routine to extract the
  ** value. */
  {
    u64 v64;
    u8 n;

    p -= 4;
    n = sqlite3GetVarint(p, &v64);
    assert( n>5 && n<=9 );
    *v = (u32)v64;
    return n;
  }
#endif
}

/*
** Return the number of bytes that will be needed to store the given
** 64-bit integer.
*/
SQLITE_PRIVATE int sqlite3VarintLen(u64 v){
  int i;
  for(i=1; (v >>= 7)!=0; i++){ assert( i<10 ); }
  return i;
}

} // namespace
