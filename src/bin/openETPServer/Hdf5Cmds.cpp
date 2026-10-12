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

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <filesystem>
#include <sstream>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

#include <oes/core/utils/ChronoUtils.h>
#include <oes/core/utils/Logger.h>
#include <oes/core/utils/ThousandSep.h>
#include <oes/core/utils/InputByteParser.h>

#include <oes/client/Hdf5.h>
#include <oes/common/etp12/ArrayUtils.h>

#include "EmlAppContext.h"
#include "EtpClient12.h"


using std::endl;
using namespace oes::eml;
using namespace oes::apps::eml;
namespace po = boost::program_options;
namespace fs = std::filesystem;

namespace {

using SimpleTimer = oes::core::HighResTimer;
using oes::core::thou;

enum class Hdf5ObjType : unsigned char {
    eUNKNOWN = 0,
    eGROUP,
    eDATASET,
    eNAMED_DATATYPE,
};
std::array<Hdf5ObjType, 4> hdf5ObTypes() {
    return {
        Hdf5ObjType::eUNKNOWN,
        Hdf5ObjType::eGROUP,
        Hdf5ObjType::eDATASET,
        Hdf5ObjType::eNAMED_DATATYPE,
    };
}
const char* hdf5ObTypeToString(Hdf5ObjType type) {
    switch (type) {
    case Hdf5ObjType::eUNKNOWN:         return "Unknown Object";
    case Hdf5ObjType::eGROUP:           return "Group";
    case Hdf5ObjType::eDATASET:         return "Dataset";
    case Hdf5ObjType::eNAMED_DATATYPE:  return "Named Datatype";
    }
    assert(false);
    return "Unknown Object";
}

enum class Hdf5DataType : unsigned char {
    eUNKNOWN = 0,
    eINTEGER,
    eFLOAT,
    eTIME,
    eSTRING,
    eBITFIELD,
    eOPAQUE,
    eCOMPOUND,
    eREFERENCE,
    eENUM,
    eVLEN,
    eARRAY,
};
std::array<Hdf5DataType, 12> hdf5DataTypes() {
    return {
        Hdf5DataType::eUNKNOWN,
        Hdf5DataType::eINTEGER,
        Hdf5DataType::eFLOAT,
        Hdf5DataType::eTIME,
        Hdf5DataType::eSTRING,
        Hdf5DataType::eBITFIELD,
        Hdf5DataType::eOPAQUE,
        Hdf5DataType::eCOMPOUND,
        Hdf5DataType::eREFERENCE,
        Hdf5DataType::eENUM,
        Hdf5DataType::eVLEN,
        Hdf5DataType::eARRAY,
    };
}
const char* hdf5DataTypeToString(Hdf5DataType type) {
    switch (type) {
    case Hdf5DataType::eUNKNOWN:    return "Unknown";
    case Hdf5DataType::eINTEGER:    return "Integer";
    case Hdf5DataType::eFLOAT:      return "Floating-Point";
    case Hdf5DataType::eTIME:       return "Time";
    case Hdf5DataType::eSTRING:     return "String";
    case Hdf5DataType::eBITFIELD:   return "Bitfield";
    case Hdf5DataType::eOPAQUE:     return "Opaque";
    case Hdf5DataType::eCOMPOUND:   return "Compound";
    case Hdf5DataType::eREFERENCE:  return "Reference";
    case Hdf5DataType::eENUM:       return "Enum";
    case Hdf5DataType::eVLEN:       return "Variable-Length";
    case Hdf5DataType::eARRAY:      return "Array";
    }
    assert(false);
    return "Unknown";
}

enum class Hdf5NumericType : unsigned char {
    eNIL = 0,
    eI08,
    eU08,
    eI16,
    eU16,
    eI32,
    eU32,
    eI64,
    eU64,
    eF32,
    eF64,
};
std::array<Hdf5NumericType, 10> hdf5NumTypes() {
    return {
        Hdf5NumericType::eI08,
        Hdf5NumericType::eU08,
        Hdf5NumericType::eI16,
        Hdf5NumericType::eU16,
        Hdf5NumericType::eI32,
        Hdf5NumericType::eU32,
        Hdf5NumericType::eI64,
        Hdf5NumericType::eU64,
        Hdf5NumericType::eF32,
        Hdf5NumericType::eF64,
    };
}
const char* hdf5NumTypeToString(Hdf5NumericType type) {
    switch (type) {
    case Hdf5NumericType::eNIL: break;
    case Hdf5NumericType::eI08: return "char/bool";
    case Hdf5NumericType::eU08: return "byte";
    case Hdf5NumericType::eI16: return "short";
    case Hdf5NumericType::eU16: return "ushort";
    case Hdf5NumericType::eI32: return "int";
    case Hdf5NumericType::eU32: return "uint";
    case Hdf5NumericType::eI64: return "long";
    case Hdf5NumericType::eU64: return "ulong";
    case Hdf5NumericType::eF32: return "float";
    case Hdf5NumericType::eF64: return "double";
    }
    assert(false);
    return "Unknown";
}

// TODO(unknown): 
#if 0
typedef enum H5D_layout_t {
    H5D_LAYOUT_ERROR = -1,

    H5D_COMPACT = 0,	/*raw data is very small		     */
    H5D_CONTIGUOUS = 1,	/*the default				     */
    H5D_CHUNKED = 2,	/*slow and fancy			     */
    H5D_VIRTUAL = 3,    /*actual data is stored in other datasets     */
    H5D_NLAYOUTS = 4	/*this one must be last!		     */
} H5D_layout_t;

/* Types of chunk index data structures */
typedef enum H5D_chunk_index_t {
    H5D_CHUNK_IDX_BTREE = 0,    /* v1 B-tree index (default)                */
    H5D_CHUNK_IDX_SINGLE = 1,   /* Single Chunk index (cur dims[]=max dims[]=chunk dims[]; filtered & non-filtered) */
    H5D_CHUNK_IDX_NONE = 2,     /* Implicit: No Index (H5D_ALLOC_TIME_EARLY, non-filtered, fixed dims) */
    H5D_CHUNK_IDX_FARRAY = 3,   /* Fixed array (for 0 unlimited dims)       */
    H5D_CHUNK_IDX_EARRAY = 4,   /* Extensible array (for 1 unlimited dim)   */
    H5D_CHUNK_IDX_BT2 = 5,      /* v2 B-tree index (for >1 unlimited dims)  */
    H5D_CHUNK_IDX_NTYPES        /* This one must be last!                   */
} H5D_chunk_index_t;

/* Values for the space allocation time property */
typedef enum H5D_alloc_time_t {
    H5D_ALLOC_TIME_ERROR = -1,
    H5D_ALLOC_TIME_DEFAULT = 0,
    H5D_ALLOC_TIME_EARLY = 1,
    H5D_ALLOC_TIME_LATE = 2,
    H5D_ALLOC_TIME_INCR = 3
} H5D_alloc_time_t;
#endif//0

struct ScopedH5Dataset {
    hid_t dataset_ = -1;
    hid_t filespace_ = -1;
    hid_t data_type_ = -1;

    ~ScopedH5Dataset() {
        if (data_type_ >= 0) {
            H5Tclose(data_type_);
        }
        if (filespace_ >= 0) {
            H5Sclose(filespace_);
        }
        if (dataset_ >= 0) {
            H5Dclose(dataset_);
        }
    }

    bool read(std::vector<uint8_t>& buf) const {
        // TODO(unknown): Try disable chunk caching, H5Pset_chunk_cache
        // to avoid using 1MB per dataset, even after we close it.
        herr_t read_rc = H5Dread(
            dataset_, data_type_, H5S_ALL,
            filespace_, H5P_DEFAULT, buf.data()
        );
        return read_rc >= 0;
    };

    bool read(
        std::vector<uint8_t>& buf,
        const std::vector<hsize_t>& starts,
        const std::vector<hsize_t>& counts
    ) const {
        herr_t rc = H5Sselect_hyperslab(
            filespace_, H5S_SELECT_SET,
            starts.data(), nullptr, counts.data(), nullptr
        );
        if (rc >= 0) {
            H5::DataSpace mem_space;
            hsize_t slab_size = oes::common::etp12::productOf(counts);
            mem_space.setExtentSimple(1, &slab_size, &slab_size);
            assert(slab_size <= buf.capacity());
            buf.resize(slab_size);

            rc = H5Dread(
                dataset_, data_type_, mem_space.getId(),
                filespace_, H5P_DEFAULT, buf.data()
            );
        }
        return rc >= 0;
    };
};

constexpr int max_rank = 4;

struct Hdf5ObjInfo {
    std::string name_;

    Hdf5ObjType type_ = Hdf5ObjType::eUNKNOWN;
    Hdf5DataType data_type_ = Hdf5DataType::eUNKNOWN;
    unsigned char type_bytes_ = 0;
    bool type_sign_ = true;

    unsigned char rank_ = 0;
    std::array<unsigned, max_rank> dims_{};

    unsigned header_size_ = 0;
    unsigned attr_count_ = 0;

    size_t size_ = 0;
    size_t attr_size_ = 0;
    size_t storage_size_ = 0;


    bool is_numeric() const {
        switch (data_type_) {
        case Hdf5DataType::eINTEGER:
            return 1 <= type_bytes_ && type_bytes_ <= 8;
        case Hdf5DataType::eFLOAT:
            return type_bytes_ == 4 || type_bytes_ == 8;
        default:
            break;
        }
        return false;
    }

    Hdf5NumericType numeric_type() const {
        switch (data_type_) {
        case Hdf5DataType::eINTEGER:
            if (type_sign_) {
                switch (type_bytes_) {
                case  1: return Hdf5NumericType::eI08;
                case  2: return Hdf5NumericType::eI16;
                case  4: return Hdf5NumericType::eI32;
                case  8: return Hdf5NumericType::eI64;
                default: return Hdf5NumericType::eNIL;
                }
            } else {
                switch (type_bytes_) {
                case  1: return Hdf5NumericType::eU08;
                case  2: return Hdf5NumericType::eU16;
                case  4: return Hdf5NumericType::eU32;
                case  8: return Hdf5NumericType::eU64;
                default: return Hdf5NumericType::eNIL;
                }
            }
            break; // unreachable
        case Hdf5DataType::eFLOAT:
            if (type_bytes_ == 4) {
                return Hdf5NumericType::eF32;
            } else if (type_bytes_ == 8) {
                return Hdf5NumericType::eF64;
            }
            break; // unreachable
        default:
            break;
        }
        return Hdf5NumericType::eNIL;
    };

    std::string dims() const {
        std::ostringstream oss;
        for (unsigned char i = 0; i < rank_; ++i) {
            oss << thou(dims_[i]) << " x ";
        }
        auto s = oss.str();
        if (!s.empty()) {
            s.resize(s.size() - 3);
        }
        return s;
    }

    size_t uncompressed_bytes() const {
        size_t full_size = type_bytes_;
        for (unsigned char i = 0; i < rank_; ++i) {
            full_size *= dims_[i];
        }
        return full_size;
    }

    std::string desc(bool ignore_compression_factor = false) const {
        const auto usize = uncompressed_bytes();
        return fmt::format(
            "{}D - {} - {} ({} bytes, {:.1f}x)",
            rank_, dims(), is_numeric()?
                hdf5NumTypeToString(numeric_type()):
                hdf5DataTypeToString(data_type_),
            thou(usize), ignore_compression_factor?
                1.0: (1.0 * usize / storage_size_)
        );
    }

    std::unique_ptr<ScopedH5Dataset> open(hid_t file_id) const {
        assert(!name_.empty());

        auto res = std::make_unique<ScopedH5Dataset>();

        res->dataset_ = H5Dopen2(file_id, name_.c_str(), H5P_DEFAULT);
        if (res->dataset_ < 0) {
            return nullptr;
        }

        res->filespace_ = H5Dget_space(res->dataset_);
        if (res->filespace_ < 0) {
            return nullptr;
        }

        res->data_type_ = H5Dget_type(res->dataset_);
        if (res->data_type_ < 0) {
            return nullptr;
        }

        return res;
    }
};

struct Hdf5FileInfo {
    bool datasets_only_ = false;
    bool get_dataspace_ = false;
    std::vector<Hdf5ObjInfo> objects_;

    size_t size_per_type_[static_cast<int>(Hdf5ObjType::eNAMED_DATATYPE) + 1] = { 0 };
    size_t count_per_type_[static_cast<int>(Hdf5ObjType::eNAMED_DATATYPE) + 1] = { 0 };
    size_t header_size_per_type_[static_cast<int>(Hdf5ObjType::eNAMED_DATATYPE) + 1] = { 0 };
    size_t attr_count_per_type_[static_cast<int>(Hdf5ObjType::eNAMED_DATATYPE) + 1] = { 0 };
    size_t count_per_data_type_[static_cast<int>(Hdf5DataType::eARRAY) + 1] = { 0 };
    size_t count_per_rank_and_num_type_[max_rank + 1][static_cast<int>(Hdf5NumericType::eF64) + 1] = { {0} };
    size_t count_per_rank_[max_rank + 1] = { 0 };
    size_t dataset_storage_size_ = 0;
    size_t dataset_uncompressed_size_ = 0;

    size_t sizeOf(Hdf5ObjType type) const {
        return size_per_type_[static_cast<int>(type)];
    }
    size_t countOf(Hdf5ObjType type) const {
        return count_per_type_[static_cast<int>(type)];
    }
    size_t attrCountOf(Hdf5ObjType type) const {
        return attr_count_per_type_[static_cast<int>(type)];
    }
    size_t headerSizeOf(Hdf5ObjType type) const {
        return header_size_per_type_[static_cast<int>(type)];
    }
};

static herr_t acceptHdf5ObjInfo(
    hid_t loc_id, const char* name, const H5O_info_t* linfo, void* opdata
) {
    const bool is_dataset = (linfo->type == H5O_TYPE_DATASET);

    Hdf5FileInfo* file_info = reinterpret_cast<Hdf5FileInfo*>(opdata);
    if (file_info->datasets_only_ && !is_dataset) {
        return 0; // i.e. continue iterating / visiting
    }

    Hdf5ObjInfo& oinfo = file_info->objects_.emplace_back();

    if (name[0] == '.' && name[1] == '\0') {
        oinfo.name_.clear(); // the root. We'll prepend slash later
    } else {
        oinfo.name_.assign(name); // NOT prefixed with slash
    }
    oinfo.header_size_ = static_cast<unsigned>(linfo->hdr.space.total);
    oinfo.attr_count_ = static_cast<unsigned>(linfo->num_attrs);

    oinfo.size_ = linfo->meta_size.obj.heap_size + linfo->meta_size.obj.index_size;
    oinfo.attr_size_ = linfo->meta_size.attr.heap_size + linfo->meta_size.attr.index_size;

    switch (linfo->type) {
    case H5O_TYPE_UNKNOWN:          oinfo.type_ = Hdf5ObjType::eUNKNOWN; break;
    case H5O_TYPE_GROUP:            oinfo.type_ = Hdf5ObjType::eGROUP; break;
    case H5O_TYPE_DATASET:          oinfo.type_ = Hdf5ObjType::eDATASET; break;
    case H5O_TYPE_NAMED_DATATYPE:   oinfo.type_ = Hdf5ObjType::eNAMED_DATATYPE; break;
    default:                        oinfo.type_ = Hdf5ObjType::eUNKNOWN; break;
    }

    const int idx = static_cast<int>(oinfo.type_);
    file_info->size_per_type_           [idx] += (oinfo.size_ + oinfo.attr_size_);
    file_info->count_per_type_          [idx] += 1;
    file_info->header_size_per_type_    [idx] += oinfo.header_size_;
    file_info->attr_count_per_type_     [idx] += oinfo.attr_count_;

    if (file_info->get_dataspace_ && is_dataset) {
        hid_t dataset = H5Dopen1(loc_id, name);
        hid_t filespace = H5Dget_space(dataset);
        oinfo.rank_ = static_cast<unsigned char>(H5Sget_simple_extent_ndims(filespace));
        if (oinfo.rank_ <= max_rank) {
            hsize_t dims[max_rank]{};
            hsize_t maxdims[max_rank]{};
            H5Sget_simple_extent_dims(filespace, dims, maxdims);
            for (auto i = 0; i < oinfo.rank_; ++i) {
                oinfo.dims_[i] = static_cast<unsigned>(dims[i]);
            }
        }
        oinfo.storage_size_ = H5Dget_storage_size(dataset);

        hid_t data_type = H5Dget_type(dataset);
        const size_t tsize = H5Tget_size(data_type);
        oinfo.type_bytes_ = (tsize > 255)? 255: static_cast<unsigned char>(tsize);
        oinfo.type_sign_ = H5Tget_sign(data_type)? true: false;

        switch (H5Tget_class(data_type)) {
        case H5T_NO_CLASS:  oinfo.data_type_ = Hdf5DataType::eUNKNOWN; break;
        case H5T_INTEGER:   oinfo.data_type_ = Hdf5DataType::eINTEGER; break;
        case H5T_FLOAT:     oinfo.data_type_ = Hdf5DataType::eFLOAT; break;
        case H5T_TIME:      oinfo.data_type_ = Hdf5DataType::eTIME; break;
        case H5T_STRING:    oinfo.data_type_ = Hdf5DataType::eSTRING; break;
        case H5T_BITFIELD:  oinfo.data_type_ = Hdf5DataType::eBITFIELD; break;
        case H5T_OPAQUE:    oinfo.data_type_ = Hdf5DataType::eOPAQUE; break;
        case H5T_COMPOUND:  oinfo.data_type_ = Hdf5DataType::eCOMPOUND; break;
        case H5T_REFERENCE: oinfo.data_type_ = Hdf5DataType::eREFERENCE; break;
        case H5T_ENUM:      oinfo.data_type_ = Hdf5DataType::eENUM; break;
        case H5T_VLEN:      oinfo.data_type_ = Hdf5DataType::eVLEN; break;
        case H5T_ARRAY:     oinfo.data_type_ = Hdf5DataType::eARRAY; break;
        case H5T_NCLASSES:  oinfo.data_type_ = Hdf5DataType::eUNKNOWN; break;
        }

        H5Dclose(dataset);

        file_info->count_per_rank_[oinfo.rank_] += 1;
        file_info->count_per_data_type_[static_cast<int>(oinfo.data_type_)] += 1;
        file_info->dataset_storage_size_ += oinfo.storage_size_;
        file_info->dataset_uncompressed_size_ += oinfo.uncompressed_bytes();
        if (oinfo.is_numeric()) {
            file_info->count_per_rank_and_num_type_[oinfo.rank_][static_cast<int>(oinfo.numeric_type())] += 1;
        }
    }

    return 0; // i.e. continue iterating / visiting
}

bool hdf5_info(const EmlAppContext& ctx) {
    std::string h5_filename = ctx.stringArg(ctx.action());

    const H5::FileAccPropList& fapl = H5::FileAccPropList();
    H5::H5File file(h5_filename, H5F_ACC_RDONLY, H5::FileCreatPropList::DEFAULT, fapl);

    Hdf5FileInfo file_info;
    const herr_t rc = H5Ovisit(
        file.getId(), H5_INDEX_NAME, H5_ITER_NATIVE, &acceptHdf5ObjInfo, &file_info
    );
    if (rc < 0) {
        OES_ERROR("Failed to scan HDF5 file: {}", h5_filename);
        return false;
    }

    for (auto type : hdf5ObTypes()) {
        if (size_t n = file_info.countOf(type)) {
            OES_INFO(
                "{:>18} {}{}", thou(n), hdf5ObTypeToString(type), ((n> 1)? "s": "")
            );
        }
    }
    for (auto type : hdf5ObTypes()) {
        if (size_t n = file_info.attrCountOf(type)) {
            OES_INFO(
                "{:>18} {} attribute{}", thou(n), hdf5ObTypeToString(type), ((n> 1)? "s": "")
            );
        }
    }
    for (auto type : hdf5ObTypes()) {
        if (size_t n = file_info.headerSizeOf(type)) {
            OES_INFO(
                "{:>18} {} header byte{}", thou(n), hdf5ObTypeToString(type), ((n> 1)? "s": "")
            );
        }
    }
    for (auto type : hdf5ObTypes()) {
        if (size_t n = file_info.sizeOf(type)) {
            OES_INFO(
                "{:>18} {} (index & heap) byte{}", thou(n), hdf5ObTypeToString(type), ((n> 1)? "s": "")
            );
        }
    }

    // counts of values of given byte-size-range, by increasing powers of two.
    // from 0, 1, 2, 4, 8, 16, 32, 64, ... , 1KB, ... 1MB, ..., 32MB, etc...
    std::vector<size_t> bytes_histogram_;
    std::vector<const Hdf5ObjInfo*> datasets;
    bytes_histogram_.reserve(32);
    datasets.reserve(file_info.countOf(Hdf5ObjType::eDATASET));

    std::map<std::string, size_t> group2count;
    std::map<std::string, size_t> group_name2count;
    std::map<std::string, size_t> dataset_name2count;
    for (const Hdf5ObjInfo& oinfo : file_info.objects_) {
        size_t last_slash = oinfo.name_.rfind('/');
        if (last_slash != std::string::npos) {
            ++group2count[oinfo.name_.substr(0, last_slash)];

            if (oinfo.type_ == Hdf5ObjType::eDATASET) {
                ++dataset_name2count[oinfo.name_.substr(last_slash + 1)];
            } else {
                ++group_name2count[oinfo.name_.substr(last_slash + 1)];
            }
        } else {
            ++group2count[std::string()];
            if (oinfo.type_ == Hdf5ObjType::eDATASET) {
                ++dataset_name2count[std::string()];
            } else {
                ++group_name2count[std::string()];
            }
        }

        if (oinfo.type_ == Hdf5ObjType::eDATASET) {
            datasets.emplace_back(&oinfo);

            // Not sure 0-size is normal...
            const size_t bucket_idx = (oinfo.size_ == 0)? 0:
                1 + static_cast<size_t>(std::log2(oinfo.size_));

            if (bucket_idx >= bytes_histogram_.size()) {
                bytes_histogram_.resize(bucket_idx + 1);
            }
            ++bytes_histogram_[bucket_idx];
        }
    }

    const size_t topN = ctx.args["top"].as<int>(); // 10 by default

    OES_INFO("\n{:>18} by 'overhead' size-range >>>", "<<< Datasets");
    if (!bytes_histogram_.empty() && bytes_histogram_[0]) {
        OES_INFO("{:>18} empty", thou(bytes_histogram_[0]));
    }
    for (size_t i = 1; i < bytes_histogram_.size(); ++i) {
        const size_t width = thou(static_cast<size_t>(pow(2, bytes_histogram_.size() - 1))).size();
        const size_t count = bytes_histogram_[i];
        OES_INFO(
            "{:>18} ({:>{}} - {:>{}}] bytes", thou(count),
            thou(size_t(pow(2, i - 1))), width,
            thou(size_t(pow(2, i))), width
        );
    }

    using map_entry = std::map<std::string, size_t>::value_type;
    std::vector<const map_entry*> dataset_names;
    dataset_names.reserve(dataset_name2count.size());
    for (const map_entry& entry : dataset_name2count) {
        dataset_names.emplace_back(&entry);
    }
    const size_t topN_byName = std::min<>(topN, dataset_names.size());
    std::partial_sort(
        dataset_names.begin(), dataset_names.begin() + topN_byName, dataset_names.end(),
        [](const map_entry* lhs, const map_entry* rhs) {
            return lhs->second > rhs->second;
        }
    );
    std::stable_sort( // show them alphabetically though, nicer
        dataset_names.begin(), dataset_names.begin() + topN_byName,
        [](const map_entry* lhs, const map_entry* rhs) {
            return lhs->first < rhs->first;
        }
    );
    OES_INFO("\n{:>18} by name - Top {} >>>", "<<< Datasets", topN);
    for (size_t i = 0; i < topN_byName; ++i) {
        const map_entry& entry = *dataset_names[i];
        OES_INFO("{:>18} {}", thou(entry.second), entry.first);
    }

    const size_t topN_bySize = std::min<>(topN, datasets.size());
    std::partial_sort(
        datasets.begin(), datasets.begin() + topN_bySize, datasets.end(),
        [](const Hdf5ObjInfo* lhs, const Hdf5ObjInfo* rhs) {
            return lhs->size_ > rhs->size_;
        }
    );
    OES_INFO("\n{:>18} 'overhead' size - Top {} >>>", "<<< Dataset", topN);
    for (size_t i = 0; i < topN_bySize; ++i) {
        const Hdf5ObjInfo& oinfo = *datasets[i];
        OES_INFO("{:>18} {}", thou(oinfo.size_), oinfo.name_);
    }

    return true;
}

bool hdf5_stats(const EmlAppContext& ctx) {
    std::string h5_filename = ctx.stringArg(ctx.action());

    const H5::FileAccPropList& fapl = H5::FileAccPropList();
    H5::H5File file(h5_filename, H5F_ACC_RDONLY, H5::FileCreatPropList::DEFAULT, fapl);

    Hdf5FileInfo file_info;
    file_info.get_dataspace_ = true;
    const herr_t rc = H5Ovisit(
        file.getId(), H5_INDEX_NAME, H5_ITER_NATIVE, &acceptHdf5ObjInfo, &file_info
    );
    if (rc < 0) {
        OES_ERROR("Failed to scan HDF5 file: {}", h5_filename);
        return false;
    }

    OES_INFO(
        "{:>18} Dataset stored bytes", thou(file_info.dataset_storage_size_)
        );
    OES_INFO(
        "{:>18} Dataset bytes un-chunked & un-compressed",
        thou(file_info.dataset_uncompressed_size_)
    );
    OES_INFO(
        "{:>17.1f}x Compression ratio (exclude metadata overhead)",
        1.0 * file_info.dataset_uncompressed_size_ / file_info.dataset_storage_size_
    );

    OES_INFO("\n{:>18} by rank >>>", "<<< Datasets");
    for (int rank = 0; rank <= max_rank; ++rank) {
        if (size_t n = file_info.count_per_rank_[rank]) {
            assert(rank); // i.e. nothing at rank 0
            OES_INFO("{:>18} {}D", thou(n), rank);
        }
    }

    OES_INFO("\n{:>18} by datatype class >>>", "<<< Datasets");
    for (Hdf5DataType dtype : hdf5DataTypes()) {
        if (size_t n = file_info.count_per_data_type_[static_cast<int>(dtype)]) {
            OES_INFO("{:>18} {}", thou(n), hdf5DataTypeToString(dtype));
        }
    }

    OES_INFO("\n{:>18} by rank & numeric type >>>", "<<< Datasets");
    for (int rank = 1; rank <= max_rank; ++rank) {
    for (Hdf5NumericType ntype : hdf5NumTypes()) {
        if (size_t n = file_info.count_per_rank_and_num_type_[rank][static_cast<int>(ntype)]) {
            assert(rank); // i.e. nothing at rank 0
            OES_INFO(
                "{:>18} {}D - {}", thou(n), rank, hdf5NumTypeToString(ntype)
            );
        }
    }
}

    std::vector<size_t> bytes_histogram_;
    std::vector<const Hdf5ObjInfo*> datasets;
    bytes_histogram_.reserve(32);
    datasets.reserve(file_info.countOf(Hdf5ObjType::eDATASET));
    for (const Hdf5ObjInfo& oinfo : file_info.objects_) {
        if (oinfo.type_ == Hdf5ObjType::eDATASET) {
            datasets.emplace_back(&oinfo);

            // Not sure 0-size is normal...
            // e.g. 2D point array with 0x3 dimensions.
            const size_t bucket_idx = (oinfo.storage_size_ == 0)? 0:
                1 + static_cast<size_t>(std::log2(oinfo.storage_size_));

            if (bucket_idx >= bytes_histogram_.size()) {
                bytes_histogram_.resize(bucket_idx + 1);
            }
            ++bytes_histogram_[bucket_idx];
        }
    }

    const size_t topN = ctx.args["top"].as<int>(); // 10 by default

    OES_INFO("\n{:>18} by 'storage' size-range >>>", "<<< Datasets");
    if (!bytes_histogram_.empty() && bytes_histogram_[0]) {
        OES_INFO("{:>18} empty", thou(bytes_histogram_[0]));
    }
    for (size_t i = 1; i < bytes_histogram_.size(); ++i) {
        const size_t width = thou(static_cast<size_t>(pow(2, bytes_histogram_.size() - 1))).size();
        const size_t count = bytes_histogram_[i];
        OES_INFO( // show empty range in verbose mode only
            "{:>18} ({:>{}} - {:>{}}] bytes", thou(count),
            thou(size_t(pow(2, i - 1))), width,
            thou(size_t(pow(2, i))), width
        );
    }

    const size_t topN_bySize = std::min<>(topN, datasets.size());
    std::partial_sort(
        datasets.begin(), datasets.begin() + topN_bySize, datasets.end(),
        [](const Hdf5ObjInfo* lhs, const Hdf5ObjInfo* rhs) {
            return lhs->storage_size_ > rhs->storage_size_;
        }
    );
    OES_INFO("\n{:>18} 'storage' size - Top {} >>>", "<<< Dataset", topN);
    for (size_t i = 0; i < topN_bySize; ++i) {
        const Hdf5ObjInfo& oinfo = *datasets[i];
        OES_INFO(
            "{:>18} {}\n{:>18} {}",
            thou(oinfo.storage_size_), oinfo.name_, "", oinfo.desc()
        );
    }

    return true;
}

bool hdf5_diff(const EmlAppContext& ctx) {
    size_t max_buf_size = 1024*1024;
    const std::string& mbs_spec = ctx.stringArg("max-buf-size");
    if (!mbs_spec.empty()) { try {
        max_buf_size = oes::core::parseByteSize(mbs_spec, 2147483647); // 2 Gib
    } catch (const std::runtime_error& ex) {
        OES_ERROR("Invalid --max-buf-size option: {}", ex.what());
        return false;
    }
}

    std::string lhs_filename = ctx.stringArg(ctx.action());
    std::string rhs_filename = ctx.stringArg("with");
    if (rhs_filename.empty()) {
        if (!ctx.hasArg("force")) {
            OES_ERROR("No --with file to diff against");
            return false;
        }
        rhs_filename = lhs_filename;
    }

    const fs::path lhs_path(lhs_filename);
    fs::file_status lhs_stat = fs::status(lhs_path);
    if (!fs::is_regular_file(lhs_stat)) {
        OES_ERROR("File not found: {}", lhs_filename);
        return false;
    }

    if (rhs_filename != lhs_filename) {
        const fs::path rhs_path(rhs_filename);
        fs::file_status rhs_stat = fs::status(rhs_path);
        if (!fs::is_regular_file(rhs_stat)) {
            OES_ERROR("File not found: {}", rhs_filename);
            return false;
        }
    } else {
        OES_WARNING("Diff'ing {} with itself", lhs_filename);
    }

    const H5::FileAccPropList fapl;
    H5::H5File lhs(lhs_filename, H5F_ACC_RDONLY, H5::FileCreatPropList::DEFAULT, fapl);
    H5::H5File rhs(rhs_filename, H5F_ACC_RDONLY, H5::FileCreatPropList::DEFAULT, fapl);
    const hid_t lhs_id = lhs.getId();
    const hid_t rhs_id = rhs.getId();

    Hdf5FileInfo lhs_info;
    lhs_info.datasets_only_ = true;
    lhs_info.get_dataspace_ = true;
    const herr_t lhs_rc = H5Ovisit(
        lhs_id, H5_INDEX_NAME, H5_ITER_NATIVE, &acceptHdf5ObjInfo, &lhs_info
    );
    if (lhs_rc < 0) {
        OES_ERROR("Failed to scan HDF5 file: {}", lhs_filename);
        return false;
    }
    const int lhs_width = 2 + static_cast<int>(std::log10(lhs_info.objects_.size()));
    OES_INFO("Scanned {:>{}} object in Left  file",
        thou(lhs_info.objects_.size()), lhs_width);

    Hdf5FileInfo rhs_info;
    rhs_info.datasets_only_ = true;
    rhs_info.get_dataspace_ = true;
    const herr_t rhs_rc = H5Ovisit(
        rhs_id, H5_INDEX_NAME, H5_ITER_NATIVE, &acceptHdf5ObjInfo, &rhs_info
    );
    if (rhs_rc < 0) {
        OES_ERROR("Failed to scan HDF5 file: {}", rhs_filename);
        return false;
    }
    const int rhs_width = 2 + static_cast<int>(std::log10(rhs_info.objects_.size()));
    const int width = std::max<>(lhs_width, rhs_width);
    OES_INFO("Scanned {:>{}} object in Right file",
        thou(rhs_info.objects_.size()), width
    );

    auto& lhs_objs = lhs_info.objects_;
    auto& rhs_objs = rhs_info.objects_;

    auto lt_by_name = [](const auto& lhs, const auto& rhs) {
        return lhs.name_ < rhs.name_;
    };
    auto eq_by_name = [](const auto& lhs, const auto& rhs) {
        return lhs.name_ == rhs.name_;
    };

    // Sort them lexicographically.
    std::sort(lhs_objs.begin(), lhs_objs.end(), lt_by_name);
    std::sort(rhs_objs.begin(), rhs_objs.end(), lt_by_name);

    // There should not be any duplicate parts
    assert(std::unique(lhs_objs.begin(), lhs_objs.end(), eq_by_name) == lhs_objs.end());
    assert(std::unique(rhs_objs.begin(), rhs_objs.end(), eq_by_name) == rhs_objs.end());
    OES_RELEASE_UNUSED(eq_by_name);

    // Find the in parts not found in out-package.
    std::vector<Hdf5ObjInfo> missing_dsets;
    std::set_difference(
        lhs_objs.begin(), lhs_objs.end(),
        rhs_objs.begin(), rhs_objs.end(),
        std::inserter(missing_dsets, missing_dsets.begin()),
        lt_by_name
    );

    // Find the out parts not found in in-package.
    std::vector<Hdf5ObjInfo> extra_dsets;
    std::set_difference(
        rhs_objs.begin(), rhs_objs.end(),
        lhs_objs.begin(), lhs_objs.end(),
        std::inserter(extra_dsets, extra_dsets.begin()),
        lt_by_name
    );

    // Find the parts common to both packages.
    std::vector<Hdf5ObjInfo> common_dsets;
    std::set_intersection(
        lhs_objs.begin(), lhs_objs.end(),
        rhs_objs.begin(), rhs_objs.end(),
        std::back_inserter(common_dsets),
        lt_by_name
    );

    const size_t match_count = common_dsets.size();
    if (!match_count) {
        OES_WARNING("Nothing in common! ({} vs {} datasets)"
            , thou(lhs_objs.size()), thou(rhs_objs.size())
        );
        return true;
    }

    if (!missing_dsets.empty() || !extra_dsets.empty()) {
        OES_WARNING("{} vs {} datasets; {} common; {} missing; {} extra.",
            thou(lhs_objs.size()), thou(rhs_objs.size()),
            thou(match_count), thou(missing_dsets.size()), thou(extra_dsets.size())
        );
    }

    const size_t topN = ctx.args["top"].as<int>(); // 10 by default

    if (!missing_dsets.empty() && topN) {
        OES_INFO("\nTop {} (by-name) missing datasets, out of {}:"
            , thou(std::min<>(topN, missing_dsets.size())), thou(missing_dsets.size())
        );

        size_t count = 0;
        for (const Hdf5ObjInfo& obj : missing_dsets) {
            OES_INFO("- {}: {}", obj.name_, obj.desc());
            if (++count >= topN) {
                break;
            }
        }
    }

    if (!extra_dsets.empty() && topN) {
        OES_INFO("\nTop {} (by-name) extra datasets, out of {}:"
            , thou(std::min<>(topN, extra_dsets.size())), thou(extra_dsets.size())
        );

        size_t count = 0;
        for (const Hdf5ObjInfo& obj : extra_dsets) {
            OES_INFO("- {}: {}", obj.name_, obj.desc());
            if (++count >= topN) {
                break;
            }
        }
    }

    size_t diff_count = 0;
    auto show_diff_header = [&]() {
        if (!diff_count) {
            OES_INFO("\nTop {} (by-name) datasets differences:"
                , thou(topN)
            );
        }
    };

    size_t checked = 0;
    size_t bytes_checked = 0;
    size_t io_error_count = 0;
    size_t possible_int_conversion_diff = 0;
    std::map<std::pair<Hdf5NumericType, Hdf5NumericType>, size_t> int_convs;
    for (const Hdf5ObjInfo& lhs_obj : common_dsets) {
        auto rhs_found = std::lower_bound(
            rhs_objs.begin(), rhs_objs.end(), lhs_obj, lt_by_name
        );
        assert(rhs_found != rhs_objs.end());
        const Hdf5ObjInfo& rhs_obj = *rhs_found;
        assert(lhs_obj.name_ == rhs_obj.name_);
        ++checked;

        // Use desc as "proxy" for rank, dimension, type, total bytes.
        const std::string& lhs_desc = lhs_obj.desc(true);
        const std::string& rhs_desc = rhs_obj.desc(true);
        if (lhs_desc != rhs_desc) {
            show_diff_header();
            OES_INFO("- {}: meta-data diff:\n  {}\n  {}"
                , lhs_obj.name_, lhs_desc, rhs_desc
            );

            if ((lhs_obj.data_type_ == Hdf5DataType::eINTEGER) &&
                (rhs_obj.data_type_ == Hdf5DataType::eINTEGER) &&
                ((lhs_obj.type_bytes_ != rhs_obj.type_bytes_) ||
                 (lhs_obj.type_sign_  != rhs_obj.type_sign_))
            ) {
                // Could be an implicit widening of uint to long (lossless),
                // or even a ulong to long (possibly lossy).
                // TODO(unknown): Do a smarter semantic-value comparison, not a byte-comparison
                ++possible_int_conversion_diff;
                ++int_convs[{ lhs_obj.numeric_type(), rhs_obj.numeric_type() }];
                ++diff_count;
                continue; // i.e. Don't stop diff'ing for those particular diffs
            }

            if (++diff_count >= topN) {
                break;
            }
            continue;
        }

        // Both datasets have the same "shape". Now compare their (uncompressed) bytes
        const auto& obj = lhs_obj;
        const size_t bytes = obj.uncompressed_bytes();

        if (!bytes) {
            continue; // yes, empty datasets do happen...
        }

        assert(lhs_obj.rank_ == rhs_obj.rank_);
        assert(lhs_obj.dims_ == rhs_obj.dims_);
        assert(lhs_obj.type_bytes_ == rhs_obj.type_bytes_);
        assert(lhs_obj.uncompressed_bytes() == rhs_obj.uncompressed_bytes());

        std::vector<uint8_t> lhs_buf(bytes);
        std::vector<uint8_t> rhs_buf(bytes);

        auto lhs_ds = lhs_obj.open(lhs_id);
        auto rhs_ds = rhs_obj.open(rhs_id);
        if (!lhs_ds || !rhs_ds) {
            ++io_error_count;
            continue;
        }

        if (bytes <= max_buf_size) {
            const bool lhs_read_ok = lhs_ds->read(lhs_buf);
            const bool rhs_read_ok = rhs_ds->read(rhs_buf);
            if (!lhs_read_ok || !rhs_read_ok) {
                ++io_error_count;
                continue;
            }

            bytes_checked += bytes;

            if (lhs_buf != rhs_buf) {
                show_diff_header();
                OES_INFO("- {}: data / content diff:\n  {}"
                    , obj.name_, lhs_desc
                );
                if (++diff_count >= topN) {
                    break;
                }
            }
        } else {
            // Warning: This ignores how the datasets are chunked. Could be slow...
            std::vector<int64_t> dims(obj.rank_, 0);
            for (size_t i = 0; i < obj.rank_; ++i) {
                dims[i] = obj.dims_[i];
            }
            auto div = oes::common::etp12::AryDivision::make(dims, obj.type_bytes_, max_buf_size);
            const size_t div_count = div.size();
            for (size_t i = 0; i < div_count; ++i) {
                std::vector<hsize_t> starts, counts;
                const size_t offset = div.initDataSubarraysType(i, starts, counts);

                const bool lhs_read_ok = lhs_ds->read(lhs_buf, starts, counts);
                const bool rhs_read_ok = rhs_ds->read(rhs_buf, starts, counts);
                if (!lhs_read_ok || !rhs_read_ok) {
                    ++io_error_count;
                    break;
                }

                const size_t read_bytes = oes::common::etp12::productOf(counts) * div.esize_;
                bytes_checked += read_bytes;

                if (lhs_buf != rhs_buf) {
                    show_diff_header();
                    OES_INFO("- {}: data / content diff (after {} bytes):\n  {}"
                        , obj.name_, thou(offset), lhs_desc
                    );
                    if (++diff_count >= topN) {
                        break;
                    }
                }
            }
        }
    }

    if (diff_count) {
        int diff_width = static_cast<int>(std::log10(diff_count));
        diff_width += (diff_width / 3); // commas
        diff_width += 1;
        const bool verbose = oes::core::Logger::instance().minSeverityFilter() <= oes::core::Message::Severity::verbose;

            OES_WARNING("{}{} common datasets differ (out of {}).{}"
            , (verbose? "\n": ""), thou(diff_count), thou(match_count)
            , (verbose? "": " (Use -v and --top to see which ones)")
            );
        if (possible_int_conversion_diff == diff_count) {
            OES_WARNING(
                "ALL those differences could be harmless integer conversions:"
            );
        } else if (possible_int_conversion_diff) {
            OES_WARNING(
                "{} of those differences could be harmless integer conversions:"
                , thou(possible_int_conversion_diff)
            );
        }

        // Show the actual integer conversions that actually occurred
        for (const auto& entry : int_convs) {
            OES_WARNING("  {:>{}} {:>8} -> {:<8}",
                thou(entry.second), diff_width,
                hdf5NumTypeToString(entry.first.first),
                hdf5NumTypeToString(entry.first.second)
            );
        }
    } else {
        OES_WARNING("No diffs in {} common datasets ({} uncompressed bytes)."
            , thou(match_count), thou(bytes_checked)
        );
    }

    if (checked != match_count) {
        OES_WARNING("Stopped diff'ing after {} datasets ({:.1f}%)"
            , thou(checked), ((1. * checked) / match_count * 100.)
        );
    }

    return true;
}

} // namespace

namespace oes::apps::eml {

/****************************************************************************/

int hdf5_main(EmlAppContext& ctx, const UnparsedArgs& unparsed_args) {
    po::options_description cmd_desc(R"(
Usage: openETPServer [global-options] hdf5 --info file.h5
Usage: openETPServer [global-options] hdf5 --diff file1.h5 --with file2.h5

Actions)", ctx.terminalWidth_
    );

    cmd_desc.add_options()
        ("help,h",              "Display this information")

        ("info",    txtArg(),   "Prints HDF5 file statistics")
        ("stats",   txtArg(),   "Prints per-Dataset-type statistics")
        ("diff",    txtArg(),   "Diffs two HDF5 files' dataset's metadata & bytes\n"
                                "(Ignores chunking, compression, attributes, etc...)")
    ;

    po::options_description opt_desc("Options", ctx.terminalWidth_);
    opt_desc.add_options()
        ("top",     intArg(10), "How many entry to list")
        ("with",    txtArg(),   "Which HDF5 file to diff against")
        ("max-buf-size,M",  txtArg("1MB"),  "Maximum buffer size in bytes")
        ("force", "Continue instead of exit, on some warnings/errors")
    ;
    cmd_desc.add(opt_desc);
    cmd_desc.add(osdu_partition_options(ctx.terminalWidth_));

    if (int rc = ctx.reparse(cmd_desc, unparsed_args)) {
        return (rc == -1)? 0: rc;
    }

    EmlAppNamedAction actions[] {
        { "info",   &hdf5_info },
        { "stats",  &hdf5_stats },
        { "diff",   &hdf5_diff },
    };
    return ctx.dispatchActions(actions);
}

} // namespace oes::apps::eml
