// ============================================================================
// Copyright 2008-2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <oes/core/utils/System.h>
#include <oes/core/utils/String.h>

#include <thread>
#include <cstdlib>
#include <fstream>
#include <sstream>

#if defined(_WIN32)
// Needed for gethostname/gethostbyname
#include <winsock.h>
#pragma warning(push, 3)
#include <windows.h>
#include <psapi.h>
#pragma warning(pop)
#else
// Needed for gethostbyname
#include <malloc.h>
#include <unistd.h>
#include <sys/times.h>
#include <sys/types.h>
#include <sys/resource.h> // for getrusage()
#include <time.h>
#endif


/*! \file oes/core/utils/System.h
 * \brief A collection of utilities to retrieve and manipulate system settings.
 * \ingroup oes_core_utils
 */

namespace oes {
namespace core {

    /*! \class System oes/core/utils/System.h
     * \brief A collection of utilities to retrieve and manipulate system settings
     * \ingroup oes_core_utils
     */

    /*!
     * \brief Sets an environment variable
     * \param[in] name name of the environment variable
     * \param[in] value value of the environment variable
     */
    void System::setEnv(const String& name, const String& value) {
#if defined(_WIN32)
        ::_putenv_s(name.c_str(), value.c_str());
#else
        ::setenv(name.c_str(), value.c_str(), 1);
#endif
    }

    /*!
     * \brief Retrieves the value of an environment variable
     * \param[in] name name of the environment variable
     * \retval the value of the environment variable if it exists
     * \retval empty string otherwise
     */
    String System::getEnv(const String& name) {
#if defined(_WIN32)
        const char* s = std::getenv(name.c_str());
#else
        const char* s = std::getenv(name.c_str());
#endif
        return s == nullptr ? String() : String(s);
    }

    /*!
     * \brief Retrieves the value of an environment variable as a boolean.
     *
     * \param[in] name name of the environment variable
     * \param value_on_invalid (Optional; defaults to false) value to return on
     *     an invalid environment variable value, or when empty for example, either
     *     because the variable is defined but empty, or not defined.
     * \retval true when environment variable is 1/true/on/yes.
     * \retval false when environment variable is 0/false/off/no.
     * \retval value_on_invalid in all other cases, including not defined.
     */
    bool System::getBoolEnv(const String& name, bool value_on_invalid) {
        return StringUtils::toBool(getEnv(name), value_on_invalid);
    }

    /*!
     * \brief Gets the name of the current user
     */
    String System::getUserName() {
#if defined(_WIN32)
        return getEnv("USERNAME");
#else
        return getEnv("LOGNAME");
#endif
    }

    /*!
     * \brief Gets the name of the current host name
     */
    String System::getHostName() {
        char hostname[128] = {0};
        ::gethostname(hostname, sizeof(hostname));
        return hostname;
    }

    /*!
     * \brief Splits a system search path into a list of paths
     * \details
     * The functions takes the value of the environment variable \a name
     * representing a system search path (such as the PATH or LD_LIBRARY_PATH
     * environment variables and splits it into a list of paths as in function
     * splitSearchPath(). The resulting is stored in output parameter \a
     * path.
     * \param[in] name name of an environment variable containing a system
     * search path
     * \param[out] path reference to the list of paths contained in \a name
     * \see splitSearchPath()
     */
    void System::getSearchPath(StringList& path, const String& name) {
        splitSearchPath(path, getEnv(name));
    }

    /*!
     * \brief Splits a system search path into a list of paths
     * \details
     * This function takes a system search path string such as in the PATH or
     * LD_LIBRARY_PATH environment variables. The search path is split
     * according the current OS path separator (':' on Unix, ';' on Windows)
     * and the elements are added to the output list \a path.
     *
     * \param[in] value string representing a system search path
     * \param[out] path reference to the list of paths contained in \a value
     * \see StringList::split()
     */
    void System::splitSearchPath(StringList& path, const String& value) {
        if( value.empty() ) {
            return;
        }
#if defined(_WIN32)
        StringUtils::split(path, value, ";", true);
#else
        StringUtils::split(path, value, ":", true);
#endif
    }

    /*!
     * \brief Builds a system search path
     * \details
     * Builds a system search path by concatenating elements in the given \p path
     * separated by the current OS search path separator.
     *
     * \param[in] path a list of paths
     * \return string containing the elements of \a path separated by the
     * current OS path separator
     * \see StringList::join()
     */
    String System::joinSearchPath(const StringList& path) {
#if defined(_WIN32)
        return StringUtils::join(path, ";");
#else
        return StringUtils::join(path, ":");
#endif
    }


    void System::sleep(int milliseconds) {
        std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
    }

/*! \internal
 * \return the real time since the beginning of the process(s), in seconds.
 */
double System::getRealTime() {
#ifdef _WIN32
    FILETIME f_now;
    SYSTEMTIME s_now;
    GetSystemTime(&s_now);
    SystemTimeToFileTime(&s_now, &f_now);
    LARGE_INTEGER i;
    i.LowPart = f_now.dwLowDateTime;
    i.HighPart = f_now.dwHighDateTime;
    return (double) (i.QuadPart) / 1e7;
#else
    tms t;
    return (double) times(&t) / (double)(sysconf(_SC_CLK_TCK));
#endif
}

/*! \internal
 * \return the user time since the beginning of the process(s), in seconds.
 */
double System::getUserTime() {
#ifdef _WIN32
    FILETIME start, end, kernel, user;
    if (!GetProcessTimes(GetCurrentProcess(), &start, &end, &kernel, &user)) {
        return getRealTime();
    }
    LARGE_INTEGER i;
    i.LowPart = user.dwLowDateTime;
    i.HighPart = user.dwHighDateTime;
    return (double) (i.QuadPart) / 1e7;
#else
    tms t;
    times(&t);
    return (double) (t.tms_utime) / (double)(sysconf(_SC_CLK_TCK));
#endif
}

/*! \class System::MemoryUsed oes/core/utils/System.h
 * \brief Simple structure to return memory statistics of current process.
 * \ingroup oes_core_utils
 */

/*!
 * \brief Default-construts with zero memory usage.
 */
System::MemoryUsed::MemoryUsed(
) : peak_bytes(0)
  , virtual_bytes(0)
  , resident_bytes(0)
{
}

/*!
 * \brief Gets peak memory used in megabytes.
 *
 * Useful to compare two implementations against each other in terms of memory
 * used for example. Uses integer-division, so rounds \b down to closest integer.
 * Uses thousand-separator for easy reading of large values.
 *
 * \return Human-friendly string with peak-memory usage of this process in megabytes.
 *     E.g. "7 MB", or "1,367 MB" means one-thousand-three-hundred-sixty-seven megagbytes.
 */
std::string System::MemoryUsed::peakMB() const {
    int64_t mb = peak_bytes / 1024 / 1024;
    if (mb >= 1000) {
        int64_t gb = mb / 1000;
        mb = (mb - (gb * 1000));
        std::ostringstream oss;
        oss << gb << ',';
        oss.fill('0');
        oss.width(3);
        oss << mb << " MB";
        return oss.str();
    } else {
        std::ostringstream oss;
        oss << mb << " MB";
        return oss.str();
    }
}

/*!
 * \brief Gets how much memory this process is currently using.
 *
 * \return Current memory usage.
 *
 * \note Peak memory usage may be zero, since not always successfully obtained.
 *
 * \note Uses /proc/self/stat and getrusage() on Linux;
 *     GetProcessMemoryInfo() on Windows (or GlobalMemoryStatusEx() if the
 *     former fails).
 */
System::MemoryUsed System::getMemoryUsage() {
#ifdef _WIN32
    HANDLE hProcess = OpenProcess(
        PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
        FALSE, GetCurrentProcessId()
    );
    if (hProcess != NULL) {
        //EmptyWorkingSet(hProcess);
        //SetProcessWorkingSetSize(hProcess, 1024, 8*1024);
        PROCESS_MEMORY_COUNTERS pmc;
        if (GetProcessMemoryInfo(hProcess, &pmc, sizeof(pmc))) {
            CloseHandle(hProcess);

            System::MemoryUsed mem;
            mem.peak_bytes     = pmc.PeakWorkingSetSize;
            mem.virtual_bytes  = pmc.PagefileUsage;
            mem.resident_bytes = pmc.WorkingSetSize;
            return mem;
        }
        CloseHandle(hProcess);
    }
    // fallback - peak memory not supported
    MEMORYSTATUSEX stat;
    GlobalMemoryStatusEx(&stat);

    System::MemoryUsed mem;
    mem.peak_bytes     = 0;  // not provided
    mem.virtual_bytes  = stat.ullTotalVirtual - stat.ullAvailVirtual;
    mem.resident_bytes = stat.ullTotalPhys - stat.ullAvailVirtual;
    return mem;
#else
    // 'file' stat seems to give the most reliable results
    std::ifstream stat_stream("/proc/self/stat", std::ios_base::in);

    // dummy vars for leading entries in stat that we don't care about
    std::string pid, comm, state, ppid, pgrp, session, tty_nr;
    std::string tpgid, flags, minflt, cminflt, majflt, cmajflt;
    std::string utime, stime, cutime, cstime, priority, nice;
    std::string O, itrealvalue, starttime;

    // the two fields we want
    //
    unsigned long vsize;
    long rss;

    // FIXME: This is brittle, if the process' image name contains a space
    //        or even a paren, both of which are possible and easy under Linux,
    //        albeit unusual.
    stat_stream >> pid >> comm >> state >> ppid >> pgrp >> session >> tty_nr
        >> tpgid >> flags >> minflt >> cminflt >> majflt >> cmajflt
        >> utime >> stime >> cutime >> cstime >> priority >> nice
        >> O >> itrealvalue >> starttime >> vsize >> rss; // don't care about the rest

    stat_stream.close();

    System::MemoryUsed mem;
    const long page_size_kb = sysconf(_SC_PAGE_SIZE); // in case x86-64 is configured to use 2MB pages
    mem.virtual_bytes  = vsize;
    mem.resident_bytes = rss * page_size_kb;

    rusage ruse;
    if (getrusage(RUSAGE_SELF, &ruse) == 0) {
        mem.peak_bytes = ruse.ru_maxrss * 1024;
    } else {
        mem.peak_bytes = 0;
    }
    return mem;
#endif
}

} // namespace core
} // namespace oes

