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

#include <filesystem>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/join.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <boost/algorithm/string.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "Hdf5.h"
#include "Client.hpp"
#include "ImportEpc.h"
#include "ImportExportUtils.h"

#include <oes/common/etp12/Messages.h>
#include <oes/common/etp12/Protocols.h>
#include <oes/common/AvroUtils.h>
#include <oes/common/HdfAry.h>
#include <oes/common/epc/Package.h>
#include <oes/common/epc/FilePart.h>
#include <oes/common/epc/ContentType.h>
#include <oes/common/etp12/EmlUri.h>
#include <oes/core/utils/ChronoUtils.h>
#include <oes/core/utils/Guid.h>
#include <oes/core/utils/Logger.h>
#include <oes/core/utils/System.h>
#include <oes/core/utils/ThousandSep.h>

#include <thread>
#include <chrono>
#include <map>

using oes::core::thou;
using oes::core::Guid;
using oes::common::etp12::EmlUri;
using oes::client::rpcFor;
using oes::common::etp12::Protocol::IDataArray;
using oes::common::etp12::Datatypes::DataArrayTypes::DataArrayIdentifier;
using oes::common::etp12::Datatypes::DataArrayTypes::PutDataSubarraysType;
using oes::common::etp12::Datatypes::Object::Resource;

namespace fs = std::filesystem;

/*!
 * \file oes/client/ImportEpc.h
 * \brief Imports an EPC file (and associated HDF5 file) to an ETP 1.2 server.
 */

namespace {

const char xmlns_rsq[] = "http://www.energistics.org/energyml/data/resqmlv2";
const char xmlns_eml[] = "http://www.energistics.org/energyml/data/commonv2";
const char xmlns_wit[] = "http://www.energistics.org/energyml/data/witsmlv2";
const char xmlns_prd[] = "http://www.energistics.org/energyml/data/prodmlv2";

//============================================================================

bool isMl(const std::string& uri) {
    if (uri.length() != 49) {
        return false;
    }
    return
        uri == xmlns_rsq ||
        uri == xmlns_eml ||// less common, thus second
        uri == xmlns_wit ||
        uri == xmlns_prd
    ;
}

EmlUri::ML xmlnsToML(const std::string& xmlns, const std::string& schema_version) {
    switch (xmlns.size()) {
    case  0: return EmlUri::eNO_ML;
    case 49: return (xmlns == xmlns_rsq)?  (boost::starts_with(schema_version,"2.2") ? EmlUri::eRESQML22 : EmlUri::eRESQML20):
                    (xmlns == xmlns_eml)?  (boost::starts_with(schema_version,"2.3") ? EmlUri::eEML23 : EmlUri::eEML20):
                    (xmlns == xmlns_wit )?  EmlUri::eWITSML21 :
                    (xmlns == xmlns_prd )?  (boost::starts_with(schema_version,"2.3") ? EmlUri::ePRODML23 : EmlUri::ePRODML22) :
                                            EmlUri::eUNKNOWN_ML;
    default: break;
    }
    return EmlUri::eUNKNOWN_ML;
}

//============================================================================

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

    bool open(hid_t loc_id, const std::string& path) {
        dataset_ = H5Dopen2(loc_id, path.c_str(), H5P_DEFAULT);
        if (dataset_ < 0) {
            OES_ERROR("HDF5 Dataset not found: {}", path);
            return false;
        }

        filespace_ = H5Dget_space(dataset_);
        if (filespace_ < 0) {
            OES_ERROR("Cannot get HDF5 filespace: {}", path);
            return false;
        }

        data_type_ = H5Dget_type(dataset_);
        if (data_type_ < 0) {
            OES_ERROR("Cannot get HDF5 filespace: {}", path);
            return false;
        }

        return true;
    }
};

//============================================================================


const int max_dim = 4;
using oes::common::hdf::HdfAry;
using AnyArray = oes::common::etp12::Datatypes::AnyArray_item_v; // std::variant

bool processDataset(
    hid_t loc_id, const std::string& name, size_t max_bytes, // inputs
    HdfAry& ary, AnyArray& any_ary, bool& too_large // outputs
) {
    using namespace oes::common::hdf;

    ScopedH5Dataset ds;
    if (!ds.open(loc_id, name)) {
        return false; // errors already issues
    }

    ary.rank = H5Sget_simple_extent_ndims(ds.filespace_);
    if (ary.rank > max_dim) {
        OES_ERROR(
            "Unsupported HDF5 rank (>{:3}): {}: {}",
                        max_dim, name, ary.rank
        );
        return false;
    }

    hssize_t npoints = H5Sget_simple_extent_npoints(ds.filespace_);
    hsize_t dims[32] {};
    H5Sget_simple_extent_dims(ds.filespace_, dims, nullptr);

    int64_t full_size = 1;
    switch (ary.rank) {
    case  4: full_size *= (ary.dims[3] = dims[3]); [[fallthrough]];
    case  3: full_size *= (ary.dims[2] = dims[2]); [[fallthrough]];
    case  2: full_size *= (ary.dims[1] = dims[1]); [[fallthrough]];
    case  1: full_size *= (ary.dims[0] = dims[0]); break;
    default: OES_ERROR("Failed rank"); assert(false); return false; // we checked rank <= 4 above
    }

    /*
    ** Not used for now. Should be used by a
    * *{{oes_dxx hdf5 --dump-meta}} command.
    hid_t plist = H5Dget_create_plist(dataset);
    H5D_layout_t layout = H5Pget_layout(plist);

    if (layout == H5D_CHUNKED) {
        H5Sselect_all(filespace);
        hsize_t nchunks = 0;
        H5Dget_num_chunks(dataset, filespace, &nchunks);
        hsize_t chunk_dims[32]{};
        H5Pget_chunk(plist, ary.rank, chunk_dims);
    }
    */

    ary.storage_size = H5Dget_storage_size(ds.dataset_);
    hid_t data_type = ds.data_type_;
    size_t type_size = H5Tget_size(data_type);
    H5T_order_t order = H5Tget_order(data_type);
    size_t precision = H5Tget_precision(data_type);
    H5T_sign_t sign  = H5Tget_sign(data_type);
    OES_UNUSED(order);
    OES_UNUSED(precision); // typically 8*type_size (i.e. bits)

    ary.uncompressed_size = npoints * type_size;

    size_t elem_count = ary.storage_size / type_size;
    OES_UNUSED(elem_count);
    /*
    ** This is wrong, stored bytes can be compressed,
    ** or not allocated and using fill-value, etc...
    assert(type_size * elem_count == ary.storage_size);
    */

    struct HidAndType {
        hid_t hdf_type = 0; // actual "in-file" type (informational only)
        HdfAryType ary_type = eAryOfVoid; // how we will read it.
    };
    // Warning: ETP does NOT support short-arrays or unsigned arrays!
    // For short, we widen to int. For unsigned, we read as signed.
    // In both cases, we rely on HDF5's own implicit conversions.
    const HidAndType integral_read_type[][2] {
        { { H5T_NATIVE_UINT8 , eAryOfBytes }, { H5T_NATIVE_INT8 , eAryOfBoolean } },
        { { H5T_NATIVE_UINT16, eAryOfInt   }, { H5T_NATIVE_INT16, eAryOfInt } },  // Widen shorts to ints
        { { H5T_NATIVE_UINT32, eAryOfLong  }, { H5T_NATIVE_INT32, eAryOfInt  } }, // Widen uints to longs (avoids truncation)
        { { H5T_NATIVE_UINT64, eAryOfLong  }, { H5T_NATIVE_INT64, eAryOfLong } }, // Warning: Possible truncation of uint64_t
    };
    const HidAndType floating_read_type[] {
        { H5T_NATIVE_FLOAT,  eAryOfFloat },
        { H5T_NATIVE_DOUBLE, eAryOfDouble },
    };

    HidAndType read_type;
    H5T_class_t cls = H5Tget_class(data_type);
    switch (cls) {
    case H5T_NO_CLASS:  break;
    case H5T_INTEGER:
        assert(sign == 0 || sign == 1);
        assert(type_size == 1 || type_size == 2 || type_size == 4 || type_size == 8);
        read_type = integral_read_type[int(std::log2(type_size))][sign];
        break;
    case H5T_FLOAT:
        assert(type_size == 4 || type_size == 8);
        read_type = floating_read_type[int(std::log2(type_size)) - 2];
        break;
    case H5T_TIME:      break;
    case H5T_STRING:    break;
    case H5T_BITFIELD:  break;
    case H5T_OPAQUE:    break;
    case H5T_COMPOUND:  break;
    case H5T_REFERENCE: break;
    case H5T_ENUM:      break;
    case H5T_VLEN:      break;
    case H5T_ARRAY:     break;
    case H5T_NCLASSES:  break;
    }

    const bool has_etp_support = (read_type.ary_type != eAryOfVoid);
    if (read_type.hdf_type == 0 || !has_etp_support) {
        OES_ERROR(
            "Unsupported {}HDF5 type: class={} ({} bytes, {}signed): {}",
                (has_etp_support? "": "(by ETP) "), cls, type_size, (sign == H5T_SGN_NONE? "un": ""), name
        );
        return false;
    }

    ary.elem_type = read_type.ary_type;

    if (ary.uncompressed_size > max_bytes) {
        too_large = true;
        return true;
    }
    too_large = false;

    // mem_type is the type of the array we're asking HDF5 to write into.
    // When it differs from the actual (stored) type, HDF5 will do an implicit
    // conversion to match to passed-in memory type (of buf).
    herr_t read_rc = 0;
    auto readHdf5Data = [&read_rc, &ds](void* buf, hid_t mem_type) {
        read_rc = H5Dread(ds.dataset_, mem_type, H5S_ALL, ds.filespace_, H5P_DEFAULT, buf);
    };

    bool ary_type_ok = true;

    switch (ary.elem_type) {
    case eAryOfVoid:
        ary_type_ok = false;
        break;

    case eAryOfInt: {
        oes::common::etp12::Datatypes::ArrayOfInt ary_of;
        ary_of.m_values.resize(full_size);
        readHdf5Data(ary_of.m_values.data(), H5T_NATIVE_INT32);
        any_ary = std::move(ary_of);
    } break;

    case eAryOfDouble: {
        oes::common::etp12::Datatypes::ArrayOfDouble ary_of;
        ary_of.m_values.resize(full_size);
        readHdf5Data(ary_of.m_values.data(), H5T_NATIVE_DOUBLE);
        any_ary = std::move(ary_of);
    } break;

    case eAryOfFloat: {
        oes::common::etp12::Datatypes::ArrayOfFloat ary_of;
        ary_of.m_values.resize(full_size);
        readHdf5Data(ary_of.m_values.data(), H5T_NATIVE_FLOAT);
        any_ary = std::move(ary_of);
    } break;

    case eAryOfBytes: {
        std::vector<uint8_t> ary_of;
        ary_of.resize(full_size);
        readHdf5Data(ary_of.data(), H5T_NATIVE_UINT8);
        any_ary = std::move(ary_of);
    } break;

    case eAryOfBoolean: {
        oes::common::etp12::Datatypes::ArrayOfBoolean ary_of;
        std::unique_ptr<bool[]> bools(new bool[full_size]);
        readHdf5Data(bools.get(), H5T_NATIVE_INT8); // no vector<bool>::data() ...
        ary_of.m_values.assign(bools.get(), bools.get() + full_size);
        any_ary = std::move(ary_of);
    } break;

    case eAryOfLong: {
        oes::common::etp12::Datatypes::ArrayOfLong ary_of;
        ary_of.m_values.resize(full_size);
        readHdf5Data(ary_of.m_values.data(), H5T_NATIVE_INT64);
        any_ary = std::move(ary_of);
    } break;

    case eAryOfString:
        ary_type_ok = false;
        break;

    default:
        ary_type_ok = false;
        break;
    }

    if (!ary_type_ok) {
        OES_ERROR("IDataArray::PutDataArrays: Unsupported Array Type");
        return false;
    }
    if (read_rc < 0) {
        OES_ERROR("IDataArray::PutDataArrays: Cannot read hyperslab");
        return false;
    }

    return true;
}

//============================================================================

struct PutObjTrait {
    using Req_t = oes::common::etp12::Protocol::Store::PutDataObjects;
    using Val_t = oes::common::etp12::Datatypes::Object::DataObject;

    static auto& get_map(Req_t& req) {
        using Map_t = std::remove_cv<decltype(req.m_dataObjects)>::type;
        static_assert(std::is_same<Val_t, typename Map_t::mapped_type>::value, "");

        return req.m_dataObjects;
    }
};

//============================================================================

struct PutAryTrait {
    using Req_t = oes::common::etp12::Protocol::DataArray::PutDataArrays;
    using Val_t = oes::common::etp12::Datatypes::DataArrayTypes::PutDataArraysType;

    static auto& get_map(Req_t& req) {
        using Map_t = std::remove_cv<decltype(req.m_dataArrays)>::type;
        static_assert(std::is_same<Val_t, typename Map_t::mapped_type>::value, "");

        return req.m_dataArrays;
    }
};

//============================================================================

struct IniAryTrait {
    using Req_t = oes::common::etp12::Protocol::DataArray::PutUninitializedDataArrays;
    using Val_t = oes::common::etp12::Datatypes::DataArrayTypes::PutUninitializedDataArrayType;

    static auto& get_map(Req_t& req) {
        using Map_t = std::remove_cv<decltype(req.m_dataArrays)>::type;
        static_assert(std::is_same<Val_t, typename Map_t::mapped_type>::value, "");

        return req.m_dataArrays;
    }
};

//============================================================================

struct PutSubTrait {
    using Req_t = oes::common::etp12::Protocol::DataArray::PutDataSubarrays;
    using Val_t = oes::common::etp12::Datatypes::DataArrayTypes::PutDataSubarraysType;

    static auto& get_map(Req_t& req) {
        using Map_t = std::remove_cv<decltype(req.m_dataSubarrays)>::type;
        static_assert(std::is_same<Val_t, typename Map_t::mapped_type>::value, "");

        return req.m_dataSubarrays;
    }
};

//============================================================================

enum RpcPhaseType {
    eObjPhase,
    eAryPhase,
    eIniPhase,
};

template <typename TRetRPC, typename TRpcTrait>
struct ImportEpcPhase {
    using REQ_t = typename TRetRPC::REQ_t;
    using RES_t = typename TRetRPC::RES_t;
    using MSG_t = typename TRetRPC::MSG_t;

    TRetRPC& rpc1_;
    TRetRPC& rpc2_;

    TRetRPC* prev_rpc_ = nullptr;
    TRetRPC* curr_rpc_ = nullptr;

    size_t msgs_ = 0;
    size_t count_ = 0;
    size_t msgo_bytes_ = 0;
    size_t data_bytes_ = 0;

    size_t cur_count_ = 0;
    size_t cur_msgo_bytes_ = 0; // o for overhead
    size_t cur_data_bytes_ = 0;

    size_t cur_transaction_bytes_ = 0;

    const RpcPhaseType type_;
    const std::string label_;
    const bool do_batching_;
    const size_t max_byte_size_;
    const size_t max_batch_size_;
    const std::chrono::milliseconds timeout_;
    std::function<bool()> health_check_;

    oes::core::HighResTimer timer_;

    bool send_chunks_ = false;

    ImportEpcPhase(
        TRetRPC& rpc1,
        TRetRPC& rpc2,
        RpcPhaseType type,
        const std::string& label,
        size_t max_byte_size,
        size_t max_batch_size,
        std::chrono::milliseconds timeout,
        std::function<bool()> health_check = nullptr
    );

    ImportEpcPhase& operator=(const ImportEpcPhase&) = delete;

    TRetRPC& get_current_rpc();

    template <typename TVal>
    bool add(
        std::string key, TVal&& val,
        size_t data_bytes, size_t msg_overhead_bytes,
        bool send_chunks = false
    );

    bool send();
    bool recv();
    bool finish(const char* verb, const char* noun);

    double encode_sec() const {
        return rpc1_.encode_sec_ + rpc2_.encode_sec_;
    }
    double waited_sec() const {
        return rpc1_.waited_sec_ + rpc2_.waited_sec_;
    }
};

template <typename TRetRPC, typename TRpcTrait>
ImportEpcPhase<TRetRPC, TRpcTrait>::ImportEpcPhase(
    TRetRPC& rpc1,
    TRetRPC& rpc2,
    RpcPhaseType type,
    const std::string& label,
    size_t max_byte_size,
    size_t max_batch_size,
    std::chrono::milliseconds timeout,
    std::function<bool()> health_check
) : rpc1_(rpc1)
  , rpc2_(rpc2)
  , type_(type)
  , label_(label)
  , do_batching_(max_batch_size != 0) // 0 = no batching
  , max_byte_size_(max_byte_size)
  , max_batch_size_(max_batch_size)
  , timeout_(timeout)
{
    health_check_ = health_check ? health_check : [](){return true;};
}

template <typename TRetRPC, typename TRpcTrait>
TRetRPC& ImportEpcPhase<TRetRPC, TRpcTrait>::get_current_rpc() {
    if (!curr_rpc_) {
        assert(!prev_rpc_);
        curr_rpc_ = &rpc1_;
    }
    return *curr_rpc_;
}

template <typename TRetRPC, typename TRpcTrait>
template <typename TVal>
bool ImportEpcPhase<TRetRPC, TRpcTrait>::add(
    std::string key, TVal&& val,
    size_t data_bytes, size_t msg_overhead_bytes,
    bool send_chunks
) {
    assert(data_bytes <= max_byte_size_);

    cur_transaction_bytes_ += data_bytes;

    send_chunks_ = send_chunks;

    const size_t approx_msg_size =
        key.size() +
        data_bytes +
        msg_overhead_bytes
    ;
    const size_t curr_bytes = cur_msgo_bytes_ + cur_data_bytes_;
    const bool too_big  = ((curr_bytes + approx_msg_size) > max_byte_size_);
    const bool too_many = ((cur_count_ + 1) > max_batch_size_);

    if (do_batching_ && (too_big || too_many || (send_chunks_ && curr_bytes !=0))) {
        if (!send()) { // Current batch (or 1-item call, if not batching)
            return false;
        }
    }

    ++cur_count_;
    cur_msgo_bytes_ += msg_overhead_bytes + key.size();
    cur_data_bytes_ += data_bytes;

    auto& rpc = get_current_rpc();

    if (send_chunks_) {
        rpc.begin_multi_part();
    }

    auto& map = TRpcTrait::get_map(rpc.req_);
    const bool ok = map.emplace(std::move(key), std::forward<TVal>(val)).second;

    if (ok && (!do_batching_ || send_chunks_)) {
        return send();
    }

    return ok;
}

template <typename TRetRPC, typename TRpcTrait>
bool ImportEpcPhase<TRetRPC, TRpcTrait>::recv() {
    // wait for the response of the previous RPC, before sending another one
    if (prev_rpc_) {
        std::ostringstream oss;
        auto recv_max_time = oes::core::getPartialTimeoutValue(max_byte_size_);
        bool retry_logic = timeout_ > recv_max_time;
        if (retry_logic) {
            auto remaining_time = timeout_;
            while (true) {
                auto ok = prev_rpc_->recv(oss, remaining_time < recv_max_time ? remaining_time : recv_max_time);
                if (ok.first && oss.str().empty()) {
                    return true;
                }

                // Fail-fast: server responded with a definitive error (not a timeout).
                // No point retrying — the server already gave its answer.
                if (!ok.first && ok.second == std::future_status::ready && !oss.str().empty()) {
                    OES_ERROR("{}: {}", label_, oss.str());
                    break;
                }

                remaining_time -= recv_max_time;
                if (remaining_time.count() < 0) {
                    OES_ERROR("{}: {}", label_, oss.str());
                    break;
                }
                if (!health_check_()) {
                    OES_ERROR("Client connection in bad shape!");
                    break;
                }
                oss.str("");
                oss.clear();

                if (ok.second == std::future_status::timeout) {
                    OES_INFO("Partial timeout. Retrying...");
                    if (recv_max_time < std::chrono::milliseconds(20000)) {
                        recv_max_time += std::chrono::milliseconds(2000);
                    }
                }
                // try again
            }
            return false;
        } else {
            auto ok = prev_rpc_->recv(oss);

            if (!ok.first || !oss.str().empty()) {
               OES_ERROR("{}: {}", label_, oss.str());
                return false;
            }
        }


    }
    return true;
}

template <typename TRetRPC, typename TRpcTrait>
bool ImportEpcPhase<TRetRPC, TRpcTrait>::send() {
    // wait for the response of the previous RPC, before sending another one
    if (!recv()) {
        return false;
    }
    
    auto& rpc = get_current_rpc();
    auto& map = TRpcTrait::get_map(rpc.req_);
    
    OES_RELEASE_UNUSED(map);
    assert(!map.empty());
    
    rpc.async_send(std::move(rpc.req_), timeout_); // moves req_, so "consumed"
    rpc.req_ = typename TRpcTrait::Req_t(); // in case moved-from state is invalid
    
    if (send_chunks_){
        rpc.reset_multi_part();
    }
    
    // "swap" curr and prev RPCs
    prev_rpc_ = &rpc;
    curr_rpc_ = (prev_rpc_ == &rpc1_)? &rpc2_: &rpc1_;
    
    ++msgs_;
    count_ += cur_count_;
    msgo_bytes_ += cur_msgo_bytes_;
    data_bytes_ += cur_data_bytes_;
    
    cur_count_ = 0;
    cur_msgo_bytes_ = 0;
    cur_data_bytes_ = 0;
    
    return true;
}

template <typename TRetRPC, typename TRpcTrait>
bool ImportEpcPhase<TRetRPC, TRpcTrait>::finish(const char* verb, const char* noun) {
    auto& rpc = get_current_rpc();
    const auto& map = TRpcTrait::get_map(rpc.req_);
    if (!map.empty()) {
        if (!send()) {
            return false;
        }
    }
    if (!recv()) {
        // Or still print INFO below, and return false afterwards?
        return false;
    }

    const double elapsedSec = timer_.elapsedSec();

    // Some RPCs, like PutUninitializedDataArrays, have 0 "data bytes",
    // i.e. are all "overhead" (or meta-data if you prefer). So just in case,
    // avoid the division-by-zero.
    std::string overhead;
    if (data_bytes_ == 0) {
        overhead = "= 100%";
    } else {
        overhead = fmt::format("+ {:.1f}%", 100.0 * msgo_bytes_ / data_bytes_);
    }

    const size_t data_bytes = data_bytes_? data_bytes_: msgo_bytes_;

    OES_INFO(
        "{} {} {} ({} bytes {}, {} messages) in {:.3f}s",
            verb, thou(count_), noun, thou(data_bytes), overhead, thou(msgs_), elapsedSec
    );
    OES_INFO(
        "    ({:.1f} MiB/s, {} peak, {:.3f}s encoding, {:.3f}s waiting)",
            1.0 * data_bytes / elapsedSec / 1024.0 / 1024.0, // throughput
            oes::core::System::getMemoryUsage().peakMB(),
            encode_sec(), waited_sec()
    );

    return true;
}

//============================================================================

using SubAryRPC = oes::client::RetRPC<
    oes::common::etp12::Protocol::IDataArray,
    oes::common::etp12::Protocol::DataArray::PutDataSubarrays,
    oes::common::etp12::Protocol::DataArray::PutDataSubarraysResponse
>;

using SubAryPhase = ImportEpcPhase<SubAryRPC, PutSubTrait>;

bool doReadAndSendAryInPieces1D(
    SubAryPhase& phase,
    ScopedH5Dataset& ds, const HdfAry& ary,
    const PutDataSubarraysType& sub_ary_base,
    std::array<hsize_t, 4>& hdf_starts, std::array<hsize_t, 4>& hdf_counts,
    size_t span, int rank_offset
) {
    // otherwise we would not be here, and would have written the array "whole"
    const size_t max_byte_size = phase.max_byte_size_;
    assert(                   max_byte_size < ary.uncompressed_size);
    assert(0 < span && span < max_byte_size);

    assert(ary.rank >= 1);
    assert(ary.rank >  0 + rank_offset);

    const int64_t last_dim = ary.dims[0 + rank_offset];

    const int64_t last_len = last_dim * span; // #bytes, not #elems
    assert(size_t(last_len) > max_byte_size); // otherwise not here
    OES_RELEASE_UNUSED(last_len);

    const size_t max_elems = max_byte_size / span; // that will fit our max-memory budget
    const size_t chunk_count = (last_dim + max_elems - 1) / max_elems;
    assert(chunk_count);
    const size_t chunk_elems = (last_dim + chunk_count - 1) / chunk_count;
    assert(chunk_elems);
    assert((chunk_count*chunk_elems) >= size_t(last_dim));

    const std::string& path = sub_ary_base.m_uid.m_pathInResource;

    for (size_t i = 0; i < chunk_count; ++i) {
        // call_consuming_req() moves the req, so copy a fresh one every time.
        // the copy is cheap, since the actual bulk-array-data is not there yet.
        PutDataSubarraysType sub_ary(sub_ary_base); // copy sub_ary_base

        auto& count = sub_ary.m_counts[0 + rank_offset] = chunk_elems;
        auto& start = sub_ary.m_starts[0 + rank_offset] = i * chunk_elems;

        if ((start + count) > last_dim) {
            count = (last_dim - start); // last chunk, often truncated
            assert(count > 0);
        }

        hdf_starts[0 + rank_offset] = hsize_t(start);
        hdf_counts[0 + rank_offset] = hsize_t(count);

        hid_t memspace = H5Screate_simple(ary.rank, hdf_counts.data(), /*maxdims*/nullptr);
        if (memspace < 0) {
            OES_ERROR("H5Screate_simple");
            return false;
        }

        herr_t rc = H5Sselect_hyperslab(
            ds.filespace_, H5S_SELECT_SET,
            hdf_starts.data(), /*strides*/nullptr,
            hdf_counts.data(), /*blocks*/nullptr
        );
        if (rc < 0) {
            OES_ERROR("H5Sselect_hyperslab");
            return false;
        }

        // mem_type is the type of the array we're asking HDF5 to write into.
        // When it differs from the actual (stored) type, HDF5 will do an implicit
        // conversion to match to passed-in memory type (of buf).
        herr_t read_rc = 0;
        auto readHdf5Data = [&read_rc, &ds, memspace](void* buf, hid_t mem_type) {
            read_rc = H5Dread(
                ds.dataset_, mem_type, memspace, ds.filespace_, H5P_DEFAULT, buf
            );
            H5Sclose(memspace);
        };

        bool ary_type_ok = true;
        size_t elem_sizeof = 1; // FIXME: Remove
        using namespace oes::common::hdf;
        auto& any_ary = sub_ary.m_data.m_item;

        hsize_t full_size = 1;
        switch (ary.rank) {
        case  4: full_size *= hdf_counts[3]; [[fallthrough]];
        case  3: full_size *= hdf_counts[2]; [[fallthrough]];
        case  2: full_size *= hdf_counts[1]; [[fallthrough]];
        case  1: full_size *= hdf_counts[0]; break;
        default: assert(false); return false; // we checked rank <= 4 above
        }

        if (full_size == 0) {
            return false;
        }

        switch (ary.elem_type) {
        case eAryOfVoid:
            ary_type_ok = false;
            break;

        case eAryOfInt: {
            oes::common::etp12::Datatypes::ArrayOfInt ary_of;
            ary_of.m_values.resize(full_size);
            readHdf5Data(ary_of.m_values.data(), H5T_NATIVE_INT32);
            any_ary = std::move(ary_of);
            elem_sizeof = 4;
        } break;

        case eAryOfDouble: {
            oes::common::etp12::Datatypes::ArrayOfDouble ary_of;
            ary_of.m_values.resize(full_size);
            readHdf5Data(ary_of.m_values.data(), H5T_NATIVE_DOUBLE);
            any_ary = std::move(ary_of);
            elem_sizeof = 8;
        } break;

        case eAryOfFloat: {
            oes::common::etp12::Datatypes::ArrayOfFloat ary_of;
            ary_of.m_values.resize(full_size);
            readHdf5Data(ary_of.m_values.data(), H5T_NATIVE_FLOAT);
            any_ary = std::move(ary_of);
            elem_sizeof = 4;
        } break;

        case eAryOfBytes: {
            std::vector<uint8_t> ary_of;
            ary_of.resize(full_size);
            readHdf5Data(ary_of.data(), H5T_NATIVE_UINT8);
            any_ary = std::move(ary_of);
        } break;

        case eAryOfBoolean: {
            oes::common::etp12::Datatypes::ArrayOfBoolean ary_of;
            std::unique_ptr<bool[]> bools(new bool[full_size]);
            readHdf5Data(bools.get(), H5T_NATIVE_INT8); // no vector<bool>::data() ...
            ary_of.m_values.assign(bools.get(), bools.get() + full_size);
            any_ary = std::move(ary_of);
        } break;

        case eAryOfLong: {
            oes::common::etp12::Datatypes::ArrayOfLong ary_of;
            ary_of.m_values.resize(full_size);
            readHdf5Data(ary_of.m_values.data(), H5T_NATIVE_INT64);
            any_ary = std::move(ary_of);
            elem_sizeof = 8;
        } break;

        case eAryOfString:
            ary_type_ok = false;
            break;

        default:
            ary_type_ok = false;
            break;
        }

        if (!ary_type_ok) {
            OES_ERROR("IDataArray::PutDataSubarrays: Unsupported Array Type");
            return false;
        }
        if (read_rc < 0) {
            OES_ERROR("IDataArray::PutDataSubarrays: Cannot read hyperslab");
            return false;
        }

        const size_t sub_bytes = (full_size * elem_sizeof);
        const size_t msg_overhead_bytes = 0; // No batching for sub-arrays
        if (!phase.add(path, std::move(sub_ary), sub_bytes, msg_overhead_bytes)) {
            return false;
        }
    }

    return true;
};

bool doReadAndSendAryInPieces2D(
    SubAryPhase& phase,
    ScopedH5Dataset& ds, const HdfAry& ary,
    const PutDataSubarraysType& sub_ary_base,
    std::array<hsize_t, 4>& hdf_starts,
    std::array<hsize_t, 4>& hdf_counts,
    size_t span
) {
    assert(ary.rank >= 2);
    assert(ary.uncompressed_size > phase.max_byte_size_); // otherwise not here

    // Below, "last" means 2nd (faster) dimension of a "2D" array.
    // By "2D", we mean it could be a "degenerate" 3D or 4D array,
    // whose faster 3rd and 4th dims are "full".
    const int64_t last_dim = ary.dims[1];
    const int64_t last_len = last_dim * span; // #bytes, not #elems

    if (last_len <= int64_t(phase.max_byte_size_)) {
        PutDataSubarraysType sub_ary(sub_ary_base);

        sub_ary.m_starts[1] = 0;
        sub_ary.m_counts[1] = last_dim;

        hdf_starts[1] = 0;
        hdf_counts[1] = last_dim;

        return doReadAndSendAryInPieces1D(
            phase, ds, ary, sub_ary, // inputs
            hdf_starts, hdf_counts, last_len, 0
        );
    }

    // The last dimension is already larger than max_byte_size, so we cannot
    // degenerate into a lower-dimension (thus with fewer contiguous chunks),
    // and will rely on the 1D code to break it into subparts.
    const int64_t slow_dim = ary.dims[0];

    PutDataSubarraysType sub_ary(sub_ary_base);
    sub_ary.m_counts[0] = 1;
    hdf_counts[0] = 1;

    for (int64_t i = 0; i < slow_dim; ++i) {
        sub_ary.m_starts[0] = i;
        hdf_starts[0] = hsize_t(i);

        const bool read_and_send_1d_ok = doReadAndSendAryInPieces1D(
            phase, ds, ary, sub_ary, // inputs
            hdf_starts, hdf_counts, span, 1
        );
        if (!read_and_send_1d_ok) {
            return false;
        }
    }

    return true;
}

bool doReadAndSendAryInPieces3D(
    SubAryPhase& phase,
    ScopedH5Dataset& ds, const HdfAry& ary,
    const PutDataSubarraysType& sub_ary_base,
    std::array<hsize_t, 4>& hdf_starts,
    std::array<hsize_t, 4>& hdf_counts,
    size_t span
) {
    assert(ary.rank >= 3);
    assert(ary.uncompressed_size > phase.max_byte_size_); // otherwise not here

    // Below, "last" means 3rd (faster) dimension of a "3D" array.
    // By "3D", we mean it could be a "degenerate" 4D array,
    // whose faster 4th dims are "full".
    const int64_t last_dim = ary.dims[2];
    const int64_t last_len = last_dim * span; // #bytes, not #elems

    if (last_len <= int64_t(phase.max_byte_size_)) {
        PutDataSubarraysType sub_ary(sub_ary_base);

        sub_ary.m_starts[2] = 0;
        sub_ary.m_counts[2] = last_dim;

        hdf_starts[2] = 0;
        hdf_counts[2] = last_dim;

        return doReadAndSendAryInPieces2D(
            phase, ds, ary, sub_ary, // inputs
            hdf_starts, hdf_counts, last_len
        );
    }

    // The last dimension is already larger than max_byte_size, so we cannot
    // degenerate into a lower-dimension (thus with fewer contiguous chunks),
    // and will rely on the 1D code to break it into subparts.
    const int64_t slow_dim = ary.dims[0];
    const int64_t fast_dim = ary.dims[1];

    PutDataSubarraysType sub_ary(sub_ary_base);
    sub_ary.m_counts[0] = 1;
    sub_ary.m_counts[1] = 1;
    hdf_counts[0] = 1;
    hdf_counts[1] = 1;

    for (int64_t i = 0; i < slow_dim; ++i) {
        sub_ary.m_starts[0] = i;
        hdf_starts[0] = hsize_t(i);

        for (int64_t j = 0; j < fast_dim; ++j) {
            sub_ary.m_starts[1] = j;
            hdf_starts[1] = hsize_t(j);

            const bool read_and_send_1d_ok = doReadAndSendAryInPieces1D(
                phase, ds, ary, sub_ary, // inputs
                hdf_starts, hdf_counts, span, 2
            );
            if (!read_and_send_1d_ok) {
                return false;
            }
        }
    }

    return true;
}

bool doReadAndSendAryInPieces4D(
    SubAryPhase& phase,
    ScopedH5Dataset& ds, const HdfAry& ary,
    const PutDataSubarraysType& sub_ary_base,
    std::array<hsize_t, 4>& hdf_starts, std::array<hsize_t, 4>& hdf_counts,
    size_t span
) {
    assert(ary.rank >= 4);
    assert(ary.uncompressed_size > phase.max_byte_size_); // otherwise not here

    // Below, "last" means 4th (fastest) dimension of a 4D array.
    const int64_t last_dim = ary.dims[3];
    const int64_t last_len = last_dim * span; // #bytes, not #elems

    if (last_len <= int64_t(phase.max_byte_size_)) {
        PutDataSubarraysType sub_ary(sub_ary_base);

        sub_ary.m_starts[3] = 0;
        sub_ary.m_counts[3] = last_dim;

        hdf_starts[3] = 0;
        hdf_counts[3] = last_dim;

        return doReadAndSendAryInPieces3D(
            phase, ds, ary, sub_ary, // inputs
            hdf_starts, hdf_counts, last_len
        );
    }

    // The last dimension is already larger than max_byte_size, so we cannot
    // degenerate into a lower-dimension (thus with fewer contiguous chunks),
    // and will rely on the 1D code to break it into subparts.
    const int64_t slow_dim = ary.dims[0];
    const int64_t midl_dim = ary.dims[1];
    const int64_t fast_dim = ary.dims[2];

    PutDataSubarraysType sub_ary(sub_ary_base);
    sub_ary.m_counts[0] = 1;
    sub_ary.m_counts[1] = 1;
    sub_ary.m_counts[2] = 1;
    hdf_counts[0] = 1;
    hdf_counts[1] = 1;
    hdf_counts[2] = 1;

    for (int64_t i = 0; i < slow_dim; ++i) {
        sub_ary.m_starts[0] = i;
        hdf_starts[0] = hsize_t(i);

        for (int64_t j = 0; j < midl_dim; ++j) {
            sub_ary.m_starts[1] = j;
            hdf_starts[1] = hsize_t(j);

            for (int64_t k = 0; k < fast_dim; ++k) {
                sub_ary.m_starts[2] = k;
                hdf_starts[2] = hsize_t(k);

                const bool read_and_send_1d_ok = doReadAndSendAryInPieces1D(
                    phase, ds, ary, sub_ary, // inputs
                    hdf_starts, hdf_counts, span, 3
                );
                if (!read_and_send_1d_ok) {
                    return false;
                }
            }
        }
    }

    return true;
}

//============================================================================

struct ImportEpcStats {
    oes::core::HighResTimer timer_;

    double total_encoding_sec_ = 0;
    double total_waiting_sec_ = 0;
    size_t total_msgo_bytes_ = 0;
    size_t total_data_bytes_ = 0;
    size_t total_msgs_ = 0;
    size_t total_objs_ = 0;
    size_t total_arys_ = 0;

    template <typename TRpcPhase>
    void add(const TRpcPhase& phase);

    void print();
};

template <typename TRpcPhase>
void ImportEpcStats::add(const TRpcPhase& phase) {
    total_encoding_sec_ += phase.encode_sec();
    total_waiting_sec_  += phase.waited_sec();

    total_msgo_bytes_ += phase.msgo_bytes_;
    total_data_bytes_ += phase.data_bytes_;
    total_msgs_  += phase.msgs_;

    switch (phase.type_) {
    case eObjPhase: total_objs_  += phase.count_; break;
    case eAryPhase: total_arys_  += phase.count_; break;
    case eIniPhase: break;
    }
}

void ImportEpcStats::print() {
    const double elapsedSec = timer_.elapsedSec();

    if (!total_objs_ || !total_arys_) {
        OES_INFO("Nothing was imported");
        return;
    }

    // Some RPCs, like PutUninitializedDataArrays, have 0 "data bytes",
    // i.e. are all "overhead" (or meta-data if you prefer). So just in case,
    // avoid the division-by-zero.
    std::string overhead;
    if (total_data_bytes_ == 0) {
        overhead = "= 100%";
    } else {
        overhead = fmt::format("+ {:.1f}%", 100.0 * total_msgo_bytes_ / total_data_bytes_);
    }

    const size_t data_bytes = total_data_bytes_? total_data_bytes_: total_msgo_bytes_;

    OES_INFO(
        "PUT {} objects, {} arrays ({} bytes {}, {} messages) in {:.3f}s",
                    thou(total_objs_), thou(total_arys_), thou(data_bytes), overhead,
                    thou(total_msgs_), elapsedSec
    );
    OES_INFO(
        "    ({:.1f} MiB/s, {} peak, {:.3f}s encoding, {:.3f}s waiting)",
                    1.0 * data_bytes / elapsedSec / 1024.0 / 1024.0,
                    oes::core::System::getMemoryUsage().peakMB(),
                    total_encoding_sec_, total_waiting_sec_
    );
}

} // namespace

namespace oes::client {

/*!
 * \brief Extracts the external relationship from a given EPC package and part ID string
 *
 * This method extracts the external relationship from a given EPC package and part ID string. It searches for the corresponding
 * target of the external relationship and returns it as a string. If the target is not found, an empty string is returned.
 *
 * \param inPackage EPC package
 * \param partIdStr ID string of the part with external relationship
 * \return The target of the external relationship as a string, or an empty string if not found
 */
namespace epc = oes::common::epc;

std::string getExternalRel(epc::Package& inPackage, const std::string& partIdStr){
    // split partIdStr into parts separated by /
    std::vector<std::string> parts;
    boost::split(parts, partIdStr, boost::algorithm::is_any_of("/"));
    // insert _rels/ into the vector before the last part
    std::vector<std::string> partsWithRels(parts.begin(), parts.end()-1);
    partsWithRels.push_back("_rels");
    // insert the last part into the vector
    partsWithRels.push_back(parts.back());

    const std::string relPart = inPackage.extractFile(boost::join(partsWithRels,"/") + ".rels");
    const  std::string part = relPart;
    if(part.empty()){
        return std::string();
    }
    size_t firstLim = 0;
    size_t lastLim  = 0;
    while(firstLim!=std::string::npos && lastLim!=std::string::npos){
        // For all <Relationship...... /> entry
        firstLim = part.find("<Relationship", lastLim);
        if(firstLim==0){
            break;
        }
        lastLim = part.find("/>", firstLim);
        if(lastLim==0){
            break;
        }
        // Look for external resource
        const std::string relationship = part.substr(firstLim,lastLim-firstLim);
        size_t first = relationship.find("externalResource");
        if(first==std::string::npos){
            continue;
        }

        // Find corresponding target
        first = relationship.find("Target=\"");
        if(first!=std::string::npos){
            first += 8;
            const size_t last = relationship.find("\"", first);
            if(last!=std::string::npos){
                return relationship.substr(first,last-first);
            }
        }
    }
    return std::string();
}

//============================================================================

using epc::xml::XmlName;

//! The store types an object by its xsi:type when present, so URIs must use it
//! too: the element is e.g. <eml:EpcExternalPartReference xsi:type="eml:obj_...">.
std::string objTypeOf(const XmlName& xname) {
    if (xname.xsi_type_.empty()) {
        return xname.local_name_;
    }
    return xname.xsi_type_.substr(xname.xsi_type_.find(':') + 1);
}

EmlUri makeObjUri(const EmlUri& parent_space_uri, const XmlName& xname, const Guid& uid) {
    EmlUri::ML ml = xmlnsToML(xname.xmlns_uri_,xname.schema_version_);
    return parent_space_uri.child_type(objTypeOf(xname), ml).child_obj(uid);
}

//============================================================================

using BigAry = std::pair<std::string, HdfAry>;
/**
 * @brief Helper struct to import arrays starting from previous phase (objects + uninitialized arrays)
 */
struct ImportArraysContext {
    std::map<std::string, std::string> hdf_filenames;
    std::map<std::string, EmlUri> hdf_uris;
    size_t max_byte_size;
    std::map<std::string,std::vector<BigAry>> big_arrays;

    size_t ini_ary_phase_count;
    double progress;
};

/**
 * @brief Import arrays to server
 * 
 * @param client_config  client configuration
 * @param space_uri_txt  space uri (string)
 * @param epc_filename   epc file name
 * @param context        Context from import objects (previous phease)
 * @param totals         Statistics 
 * @param options        import options
 * @return true          on success
 * @return false         on fail
 */
bool importArrays(
    const oes::client::ClientConfig& client_config,
    const std::string& space_uri_txt,
    const std::string& epc_filename,
    const ImportArraysContext& context,
    ImportEpcStats& totals,
    const ImportEpcOptions& options
) {
    std::unique_ptr<Client> client_ptr { nullptr };
    auto commitTx = [](oes::client::ScopedTransaction& transaction, size_t tx_num) -> bool {
        std::string failure_txt;
        const bool tx_ok = transaction.commit(&failure_txt);
        if (!tx_ok) {
            OES_ERROR("Import arrays (tx {}) failed.", tx_num);
        }

        return tx_ok;
    };

    bool connection_ok = false;
    int retries = options.reconnect_retries < 0 ? 1 : options.reconnect_retries + 1;
    size_t transaction_counter {0};
    size_t sub_totals_count {0};

    int last_progress_value {0};

    auto resume = false;
    auto bigarr_it_checkpoint = context.big_arrays.cbegin();
    auto tx_current_bigarr_it = context.big_arrays.cbegin();
    
    auto subarr_it_checkpoint = bigarr_it_checkpoint->second.cbegin();
    auto tx_current_subarr_it = bigarr_it_checkpoint->second.cbegin();

    auto progress = context.progress;

    while (bigarr_it_checkpoint != context.big_arrays.cend()) {
        if (!connection_ok) {
            auto wait_reconnection_time = std::chrono::seconds(2);
            while (retries > 0) {
                client_ptr.reset(new Client(client_config));
                connection_ok = client_ptr->connect(options.timeout_);
                if (connection_ok || retries <= 0) {
                    break;
                }

                OES_INFO("Trying to (re)connect to the server {} for transaction {}", retries, transaction_counter);
                std::this_thread::sleep_for(wait_reconnection_time);
                wait_reconnection_time *= 2;
                retries--;
            };

            if (retries < 0 || !connection_ok) {
                OES_ERROR("Failed to (re)connect to the server.");
                return false;
            }
        }


        const std::string message = fmt::format(
            "Import EPC Arrays {}: transaction {}", epc_filename, transaction_counter
        );
        oes::client::ScopedTransaction tx(*client_ptr, options.timeout_);
        tx.start(space_uri_txt, message, false, nullptr, options.n_transaction_retries_); //todo check throws
        
        IDataArray *const array_proto = client_ptr->array_proto();

        size_t sub_count = 0;
        auto putsub_rpc1 = rpcFor(array_proto, &IDataArray::PutDataSubarrays);
        auto putsub_rpc2 = rpcFor(array_proto, &IDataArray::PutDataSubarrays);
        ImportEpcPhase<decltype(putsub_rpc1), PutSubTrait> put_sub_phase(
            putsub_rpc1, putsub_rpc2, eAryPhase, "PUT arrays",
            context.max_byte_size, 0/*max_batch_size*/, options.timeout_,
            [&client_ptr](){return client_ptr->isConnOpen();}
        );
        static_assert(std::is_same<SubAryPhase, decltype(put_sub_phase)>::value, "");
        
        bool transaction_limit = false;
        for (tx_current_bigarr_it = bigarr_it_checkpoint; tx_current_bigarr_it != context.big_arrays.cend(); tx_current_bigarr_it++) {
            const H5::FileAccPropList& fapl = H5::FileAccPropList();
            const std::string& hdf_id = tx_current_bigarr_it->first;
            const auto it_file = context.hdf_filenames.find(hdf_id);
            assert(it_file != context.hdf_filenames.end());
            H5::H5File file(it_file->second, H5F_ACC_RDONLY, H5::FileCreatPropList::DEFAULT, fapl);
            hid_t file_id = file.getId();

            const auto it_uri = context.hdf_uris.find(tx_current_bigarr_it->first);
            assert(it_uri != context.hdf_uris.end());
            const std::string& hdf_uri_str = it_uri->second.to_string(); 
            
            for (tx_current_subarr_it = resume ? subarr_it_checkpoint : tx_current_bigarr_it->second.cbegin(); tx_current_subarr_it != tx_current_bigarr_it->second.cend();) {
                const auto& entry = *tx_current_subarr_it;
                const std::string& path = entry.first;
                const HdfAry& ary = entry.second;


                ScopedH5Dataset ds;
                if (!ds.open(file_id, path)) {
                    OES_ERROR("Cannot open HDF5 dataset: {}", path);
                    return false;
                }

                size_t span = 4;
                {
                using namespace oes::common::hdf;
                    switch (ary.elem_type) {
                        case eAryOfDouble: span = 8; break;
                        case eAryOfFloat: span = 4; break;
                        case eAryOfBytes: span = 1; break;
                        case eAryOfBoolean: span = 1; break;
                        case eAryOfLong: span = 8; break;
                    };
                }

                PutDataSubarraysType sub_ary;
                sub_ary.m_uid.m_uri = hdf_uri_str; // NOT the "real owner" of the data, but the EpcExt
                sub_ary.m_uid.m_pathInResource = path;
                sub_ary.m_counts.resize(ary.rank);
                sub_ary.m_starts.resize(ary.rank);

                std::array<hsize_t, 4> hdf_starts {{ 0, 0, 0, 0 }};
                std::array<hsize_t, 4> hdf_counts {{ 1, 1, 1, 1 }};

                bool read_and_send_ok = false;

                switch (ary.rank) {
                case  1:
                    read_and_send_ok = doReadAndSendAryInPieces1D(
                        put_sub_phase, ds, ary, sub_ary, // inputs
                        hdf_starts, hdf_counts, span, 0
                    );
                    break;

                case  2:
                    read_and_send_ok = doReadAndSendAryInPieces2D(
                        put_sub_phase, ds, ary, sub_ary, // inputs
                        hdf_starts, hdf_counts, span
                    );
                    break;

                case  3:
                    read_and_send_ok = doReadAndSendAryInPieces3D(
                        put_sub_phase, ds, ary, sub_ary, // inputs
                        hdf_starts, hdf_counts, span
                    );
                    break;

                case  4:
                    read_and_send_ok = doReadAndSendAryInPieces4D(
                        put_sub_phase, ds, ary, sub_ary, // inputs
                        hdf_starts, hdf_counts, span
                    );
                    break;

                default: assert(false); return false;
                }

                if (!read_and_send_ok) {
                    OES_ERROR("Cannot read or send HDF5 dataset: {}", path);
                    connection_ok = false;
                    break;
                }
                
                ++tx_current_subarr_it;
                ++sub_count;
                
                // if (put_sub_phase.cur_transaction_bytes_ >= 50'000'000) {
                // 1) Check if elapsed time reach the transaction limit in time (if defined: > 0) OR
                // 2) Check if current transaction bytes reach the transaction limit in size (if defined: > 0)
                if ((options.transaction_limit_sec > 0 && put_sub_phase.timer_.elapsedSec() > options.transaction_limit_sec) ||
                    (options.transaction_limit_bytes > 0 && put_sub_phase.cur_transaction_bytes_ > options.transaction_limit_bytes)) {

                    OES_INFO("time {} {}, size {} {}", (options.transaction_limit_sec > 0 && put_sub_phase.timer_.elapsedSec() > options.transaction_limit_sec),
                        put_sub_phase.timer_.elapsedSec(),
                        (options.transaction_limit_bytes > 0 && put_sub_phase.cur_transaction_bytes_ > options.transaction_limit_bytes),
                        put_sub_phase.cur_transaction_bytes_
                    ); //TODO REMOVE

                    transaction_limit = true;
                    break;
                }

                auto temp = progress + static_cast<double>(sub_count)*(97.0-30.0)/static_cast<double>(context.ini_ary_phase_count);
                if (last_progress_value < int(temp)) {
                    last_progress_value = temp;
                    OES_INFO("Send large arrays progress: {}%", static_cast<int>(last_progress_value));
                }
                resume = false;
            } //end subarray loop

            if (!connection_ok) {
                break;
            }

            if (transaction_limit) {
                break;
            }
        } // bigarrays

        put_sub_phase.count_ = sub_count;

        // end smaller transaction
        if (connection_ok) {
            if (!put_sub_phase.finish("PUT", "arrays")) {
                OES_ERROR("Could not finish transaction {}.", transaction_counter);
                continue;
            }
            
            totals.add(put_sub_phase);
            if (commitTx(tx, transaction_counter)) {
                if (tx_current_subarr_it == tx_current_bigarr_it->second.cend()) {
                    ++tx_current_bigarr_it;
                    if (tx_current_bigarr_it != context.big_arrays.cend()) {
                        tx_current_subarr_it = tx_current_bigarr_it->second.cbegin();
                    }
                }
                
                // update big arrays checkpoint iterator
                bigarr_it_checkpoint = tx_current_bigarr_it;
                subarr_it_checkpoint = tx_current_subarr_it;

                sub_totals_count += sub_count;
                progress += static_cast<double>(sub_count)*(97.0-30.0)/static_cast<double>(context.ini_ary_phase_count);

                transaction_counter++;
            } else {
                OES_ERROR("Could not commit transaction {}.", transaction_counter);
                continue;
            }
            resume = true;
        } else {
            OES_ERROR("Connection not ok, transaction {}.", transaction_counter);
            retries--;
            resume = true;
            continue;
        }

    } // main loop

    assert(sub_totals_count == context.ini_ary_phase_count);

    return true;
}

//================================

/*!
 * \brief Imports an EPC file, into an ETP 1.2 server.
 *
 * The three main parameters affecting this functions are:
 * # client.negotiated_max_msg_size()
 * # max_batch_size
 * # transaction-limit-time or transaction-limit-size
 *
 * For efficiency, as many objects or arrays are \em bundled together in a single
 * message (batch request), without exceeding either \em max_msg_size or
 * \a max_batch_size, whichever comes first.
 *
 * And when a single array exceeds \em max_msg_size, instead of sending it \em whole
 * using the PutDataArrays message, it is instead first \em declared to the server
 * using the PutUninitializedDataArrays message, and then written in \em pieces or
 * \em parts, using two or more PutDataSubarrays messages.
 *
 * Memory consumption is proportional to \em max_msg_size, not the EPC's size.
 * 
 * With the transaction-limit-time (and/or -size) is defined then arrays will be
 * imported in multiple transaction (unless the time or size is bigger than all
 * arrays together)
 *
 * \param client Client configuration
 * \param etp_space the destination ETP dataspace path (not URI), for all data
 * \param epc_filename the input EPC filename
 * \param timeout the maximum time the client should wait for a server response,
 *     before giving up and assuming the request as failed. The value to use depends
 *     on the performance of the network (bandwidth and latency), the server itself,
 *     but also \a timeout and the \a client's negotiated_max_msg_size(), since the
 *     larger the request, the more time the server will take to return a response.
 * \param max_batch_size the maximum number of objects or arrays to send per-request.
 *     \em Batching is completely disabled when zero, i.e. all objects and arrays are
 *     sent in their own message (unless the array needs to be split in pieces/parts)
 * \return true on success; false otherwise.
 */
bool importEpc(
    const oes::client::ClientConfig& client_config,
    const std::string& etp_space,
    const std::string& epc_filename,
    const ImportEpcOptions& options
) {
    std::unique_ptr<oes::client::Client> client_ptr{nullptr};
    client_ptr.reset(new oes::client::Client(client_config));

    oes::common::etp12::Datatypes::ErrorInfo err_c;
    if (!client_ptr->connect(options.timeout_, &err_c)) {
        OES_ERROR("Could not connect to server: {}", err_c.m_message);
        return false;
    }
    auto& client = *client_ptr;


    std::ostream* err = options.err_;

    std::ostringstream oss;
    std::ostream& rpc_err = err? *err: oss;
    EpcStatistics stats;

#ifdef OES_DEBUG_BUILD
    auto ping_rpc = rpcFor(client.core_proto(), &oes::client::proto12::ICore::Ping);

    //std::ostringstream oss;
    auto ping_ok = ping_rpc.call(oss, options.timeout_);
    if (!ping_ok.first || ping_ok.second != std::future_status::ready) {
        OES_ERROR("Ping failed");
        return false;
    }
#endif //OES_DEBUG_BUILD

    OES_INFO("Import Options: "
             "timeout = {} [s]; "
             "max_msg_size (negotiated) = {}; "
             "transaction_retries = {}; "
             "reject_duplicated = {}; ",
             std::chrono::duration_cast<std::chrono::seconds>(options.timeout_).count(),
             client.negotiated_max_msg_size(),
             options.n_transaction_retries_,
             options.reject_duplicates_);

    ImportEpcStats totals;

    // Take 20% off the max-msg-size, to leave wiggle room for Avro serialization overhead.
    // A more formal framework to split messages which are too large is a TODO
    const size_t max_byte_size = size_t(client.negotiated_max_msg_size() * 0.8);

    const std::string space_uri_txt = getDataspaceUri(client, etp_space, options.timeout_);
    const EmlUri space_uri = EmlUri::parse(space_uri_txt);
    if(!space_uri.isValid()){
        OES_ERROR("Import EPC {} into ETP space {} failed: Dataspace not found", epc_filename, etp_space);
        return false;
    }
    const std::string message = fmt::format(
        "Import EPC {} into ETP space {}", epc_filename, etp_space
    );

    oes::client::ScopedTransaction tx(client, options.timeout_);
    tx.start(space_uri_txt, message, false, nullptr, options.n_transaction_retries_);

    auto commitTx = [&tx, &totals]() {
        std::string failure_reason;
        const bool tx_ok = tx.commit(&failure_reason);

        if (!tx_ok) {
            OES_ERROR("EPC Import failed: {}", failure_reason);
        }

        return tx_ok;
    };

    double progress{1.0};

    std::map<std::pair<std::string, oes::core::Guid>, int64_t> uuid_map;
    if (options.reject_duplicates_) {
        auto* proto = client.disco_proto();
            if (!proto) {
                return false;
            }

        auto disc_rpc = rpcFor(proto, &proto12::IDiscovery::GetResources);
        disc_rpc.req_.m_context.m_uri = space_uri_txt;
        disc_rpc.req_.m_context.m_depth = 1;
        disc_rpc.req_.m_context.m_dataObjectTypes.clear();
        disc_rpc.req_.m_storeLastWriteFilter.reset();
        disc_rpc.req_.m_scope = oes::common::etp12::Datatypes::Object::ContextScopeKind::eTargets;
        

        OES_INFO("Discovery progress: {}%", static_cast<int>(progress));

        std::vector<Resource> res_vec;
        if (!disc_rpc.callForEachValue(
            rpc_err, options.timeout_,
            [&](auto& res) {
                res_vec.emplace_back(res);
            }
        )) {
            return false;
        }
        
        for (auto& ob : res_vec) {
            const EmlUri uri = EmlUri::parse(ob.m_uri);
            uuid_map.emplace(std::make_pair(uri.to_canonical_type(), uri.guid_), ob.m_lastChanged);
        }    
    }

    progress = 3.0;
    OES_INFO("Prolog progress: {}%", static_cast<int>(progress));

    // Phase 1: Send objects (extracting which arrays they use)
    auto put_obj_rpc1 = rpcFor(client.store_proto(), &oes::client::proto12::IStore::PutDataObjects);
    auto put_obj_rpc2 = rpcFor(client.store_proto(), &oes::client::proto12::IStore::PutDataObjects);
    ImportEpcPhase<decltype(put_obj_rpc1), PutObjTrait> put_obj_phase(
        put_obj_rpc1, put_obj_rpc2, eObjPhase, "PUT objects",
        max_byte_size, options.max_batch_size_, options.timeout_
    );

    epc::Package inpackage;
    inpackage.openForReading(epc_filename);

    typedef epc::FileContentType::ContentTypeMap ContentTypeMap;
    const ContentTypeMap& contentTypes =
        inpackage.getFileContentType().getAllContentType();

    using AryVal = std::tuple<std::string, std::string, EmlUri, std::string>; // hdf-id, XPA, container-uri, locator-url
    std::map<std::string, AryVal> array_map; // PathInHdfFile_ as key

    fs::path hdf_path(epc_filename);
    hdf_path.replace_extension(".h5");

    ImportArraysContext arr_context;
    arr_context.max_byte_size = max_byte_size;

    bool put_obj_ok = true;
    for (const ContentTypeMap::value_type& entry : contentTypes) {
        std::string partIdStr = entry.second.getExtensionOrPartName();
        if (partIdStr[0] == '/') {
            partIdStr.erase(0, 1); // normalize path, if necessary
        }
        if (partIdStr == "rels") {
            continue; // a bit had-hoc
        }
        std::string partIdXml = inpackage.extractFile(partIdStr);
        if (partIdXml.empty()) {
            OES_ERROR("Cannot read part: {}", partIdStr);
            continue;
        }

        using epc::xml::AbstractCitedDataObject;
        using epc::xml::NameValuePair;
        using epc::xml::XmlPathGetter;
        using epc::xml::DataObjectReference;
        using epc::xml::Hdf5Dataset;

        Guid uuid;
        XmlName xname;
        AbstractCitedDataObject cited;
        std::map<std::string,std::string> filename_array_uri;

        {
            using namespace epc;
            const epc::FilePart::ScanRc rc = epc::FilePart::scan(
                partIdXml, // note: copied, since "in-place" parsing
                [&uuid, &xname, &cited](
                    const XmlName& n, const AbstractCitedDataObject& c
                ) {
                    uuid.reparse(c.uuid_.c_str(), c.uuid_.length(), false);
                    xname = n;
                    cited = c;
                    return epc::FilePart::eOK_DONE;
                },
                /*
                ** See above
                [&xdata](const NameValuePair& kv) {
                    xdata.add(kv);
                    return epc::FilePart::eOK_DONE;
                },
                */
                nullptr, // see above, we do not extract xdata
                nullptr, // see above, we no not extract relations
                [&uuid, &xname, &array_map, &space_uri, &filename_array_uri](const XmlPathGetter& xpg, const Hdf5Dataset& h) {
                    std::string hdf_id = h.HdfProxy_.Uuid_.empty() ? h.HdfProxy_.EnergisticsUri_ : h.HdfProxy_.Uuid_ ;

                    std::string path = h.PathInHdfFile_;
                    if (path[0] == '/') {
                        path.erase(0, 1); // normalize path, if necessary
                    }
                    EmlUri uri = EmlUri::parse(h.HdfProxy_.EnergisticsUri_);
                    if(!uri.isValid()){
                        uri = makeObjUri(space_uri, xname, uuid);
                        if(h.HdfProxy_.Uuid_.empty()){
                            filename_array_uri.emplace(hdf_id,uri.to_string());
                        }
                    }
                    std::string xpath = xpg("2.0", nullptr);
                    array_map.emplace(std::move(path), AryVal(hdf_id,std::move(xpath), std::move(uri), h.HdfProxy_.LocatorUrl_));
                    return epc::FilePart::eOK_DONE;
                }
            );
            // Replace all occurrences of filename by hdf_filename in xml
            for(const auto& filename_array : filename_array_uri){
                partIdXml = FilePart::replaceNestedXMLValue(
                    partIdXml, "ExternalDataArrayPart", "URI", filename_array.first, filename_array.second
                );
            }

            if (rc != epc::FilePart::eOK_DONE) {
                OES_ERROR("Cannot XML parse part {}", partIdStr);
                continue;
            }
        }
        if (!isMl(xname.xmlns_uri_)) {
            continue;
        }
        if (!uuid.isValid()) {
            OES_ERROR("Invalid Guid: Part ignored: {}", partIdStr);
            continue;
        }

        std::chrono::microseconds last_changed_usec_dup;
        auto last_changed_valid_dup = oes::core::usecSinceEpochFromExtendedIsoDateTime(
                cited.LastUpdate_, last_changed_usec_dup
            );

        const EmlUri obj_uri = makeObjUri(space_uri, xname, uuid);
        //if (cited.uuid_ in there... and dates match)
        if (options.reject_duplicates_ && last_changed_valid_dup) {
            auto found = uuid_map.find({obj_uri.to_canonical_type(), uuid});
            if (found != uuid_map.end()) {
                if ((*found).second != last_changed_usec_dup.count()) {
                    OES_ERROR("Found duplicated object with different dates. Object with title {} (type {}). Aborting import.", cited.Title_, xname.xsi_type_);
                    return false;
                }
            }
        }

        if ((obj_uri.ml_ == EmlUri::eEML20||obj_uri.ml_ == EmlUri::eEML23) && boost::ends_with(obj_uri.obj_type_,"EpcExternalPartReference")) {
            const std::string rel = getExternalRel(inpackage,partIdStr);
            fs::path local_hdf_path = hdf_path;
            local_hdf_path.replace_filename(rel);
            const std::string hdf_filename = local_hdf_path.string();
            fs::file_status hdf_stat = fs::status(local_hdf_path);
            if (!fs::is_regular_file(hdf_stat)) {
                OES_ERROR("HDF5 file not found: {}", hdf_filename);
                put_obj_ok = false;
                break;
            }
            arr_context.hdf_filenames.emplace(uuid.toXmlString(),std::move(hdf_filename));
            arr_context.hdf_uris.emplace(uuid.toXmlString(),std::move(obj_uri));
        }

        oes::common::etp12::Datatypes::Object::DataObject etp_obj;
        etp_obj.m_format = "xml";
        
        bool send_chunks = false;
        oes::common::etp12::Datatypes::Uuid blobId;
        if(partIdXml.size() >= max_byte_size){
            send_chunks = true;
            oes::common::assignUid(blobId, Guid::make());
            etp_obj.m_blobId = blobId;
        } else {
            OES_UNUSED(blobId);
            etp_obj.m_data.assign(partIdXml.begin(), partIdXml.end());
        }
        
        etp_obj.m_resource.m_name = std::move(cited.Title_);
        etp_obj.m_resource.m_uri = obj_uri.to_string();

        std::chrono::microseconds last_changed_usec;
        auto last_changed_valid = oes::core::usecSinceEpochFromExtendedIsoDateTime(
                cited.lastChanged(), last_changed_usec
            );

        if (last_changed_valid) {
            etp_obj.m_resource.m_lastChanged = last_changed_usec.count();
        } else {
            OES_ERROR("Invalid timestamp: {}", cited.lastChanged());
        }

        const size_t msg_overhead_bytes =
            etp_obj.m_format.size() +
            etp_obj.m_resource.m_name.size() +
            etp_obj.m_resource.m_uri.size()
        ;
        put_obj_ok = put_obj_phase.add(
            std::move(partIdStr), std::move(etp_obj),
            etp_obj.m_data.size(), msg_overhead_bytes,
            send_chunks
        );
        if (!put_obj_ok) {
            break;
        }
        if(send_chunks){
            auto chunk_obj_rpc = rpcFor(client.store_proto(), &oes::client::proto12::IStore::Chunk);
            chunk_obj_rpc.begin_multi_part();
            chunk_obj_rpc.req_.m_blobId = blobId;

            const size_t chunk_total_size = partIdXml.size();

            size_t nb_chunks = std::lround(std::ceil(1.0 * chunk_total_size / (max_byte_size)));
            size_t chunk_size = std::lround(std::ceil(1.0 * chunk_total_size / nb_chunks));
            for(size_t offset=0; offset<chunk_total_size; offset+=chunk_size){
                nb_chunks--;
                size_t size = std::min<>(chunk_size, chunk_total_size - offset);
                chunk_obj_rpc.req_.m_data = std::move(std::vector<uint8_t>(partIdXml.begin()+offset, partIdXml.begin()+offset+size));
                chunk_obj_rpc.req_.m_final = nb_chunks==0;
                if(chunk_obj_rpc.req_.m_final) {
                    chunk_obj_rpc.sending_final_part();
                }
                chunk_obj_rpc.async_send(std::move(chunk_obj_rpc.req_), options.timeout_); 
            }
        }
    }

    if (!put_obj_phase.finish("PUT", "objects")) {
        OES_ERROR("EPC Import failed");
        return false;
    }
    if (!put_obj_ok) {
        OES_ERROR("EPC Import failed");
        return false;
    }
    totals.add(put_obj_phase);

    if (array_map.empty()) {
        return commitTx();
    }

    for( const auto& am : array_map){
        if(!Guid::parse(std::get<0>(am.second)).isValid()){
            fs::path local_hdf_path = hdf_path;
            local_hdf_path.replace_filename(std::get<0>(am.second));
            const std::string hdf_filename = local_hdf_path.string();
            fs::file_status hdf_stat = fs::status(local_hdf_path);
            if (!fs::is_regular_file(hdf_stat)) {
                OES_ERROR("HDF5 file not found: {}", hdf_filename);
                break;
            }
            arr_context.hdf_filenames.emplace(std::get<0>(am.second),std::move(hdf_filename));
        }
    }

    IDataArray*const array_proto = client.array_proto();

    progress = 4.0;
    OES_INFO("Send objects progress: {}%", static_cast<int>(progress));

    // Phase 2: Send small-enough arrays "whole"
    auto put_ary_rpc1 = rpcFor(array_proto, &IDataArray::PutDataArrays);
    auto put_ary_rpc2 = rpcFor(array_proto, &IDataArray::PutDataArrays);
    ImportEpcPhase<decltype(put_ary_rpc1), PutAryTrait> put_ary_phase(
        put_ary_rpc1, put_ary_rpc2, eAryPhase, "PUT arrays",
        max_byte_size, options.max_batch_size_, options.timeout_
    );

    auto readAndSendFullAry = [&put_ary_phase](
        const std::string& path, const HdfAry& ary, AnyArray& any_ary, const EmlUri& hdf_uri
    ) {
        assert(!ary.data()); // read directly into any_ary instead

                             // done in sub_ary_phase otherwise
        assert(ary.uncompressed_size <= put_ary_phase.max_byte_size_);

        oes::client::data12::DataArrayTypes::PutDataArraysType msg;
        msg.m_uid.m_pathInResource = path;
        msg.m_uid.m_uri = hdf_uri.to_string();
        msg.m_array.m_data.m_item = std::move(any_ary);

        msg.m_array.m_dimensions.resize(ary.rank);
        switch (ary.rank) {
        case  4: msg.m_array.m_dimensions[3] = ary.dims[3]; [[fallthrough]];
        case  3: msg.m_array.m_dimensions[2] = ary.dims[2]; [[fallthrough]];
        case  2: msg.m_array.m_dimensions[1] = ary.dims[1]; [[fallthrough]];
        case  1: msg.m_array.m_dimensions[0] = ary.dims[0]; break;
        default: assert(false); return false;
        }

        const size_t msg_overhead_bytes =
            msg.m_uid.m_pathInResource.size() +
            msg.m_uid.m_uri.size() +
            msg.m_array.m_dimensions.size() * 4
        ;
        return put_ary_phase.add(
            path, std::move(msg), ary.uncompressed_size, msg_overhead_bytes
        );
    };

    // using BigAry = std::pair<std::string, HdfAry>;
    // std::map<std::string,std::vector<BigAry>> big_arrays;
    auto& big_arrays = arr_context.big_arrays;

    for(const auto& hdf_filename : arr_context.hdf_filenames){
        const std::string& hdf_filename_id = hdf_filename.first;
        std::vector<BigAry> hdf_big_arrays;
        const H5::FileAccPropList& fapl = H5::FileAccPropList();
        H5::H5File file(hdf_filename.second, H5F_ACC_RDONLY, H5::FileCreatPropList::DEFAULT, fapl);
        hid_t file_id = file.getId();
        for (const auto& entry : array_map) {
            const std::string& ary_path = entry.first;
            const std::string& hdf_id = std::get<0>(entry.second);

            if(hdf_id!=hdf_filename_id){
                continue;
            }

#ifdef OES_DEBUG_BUILD
            const std::string& obj_xpath = std::get<1>(entry.second);
            OES_UNUSED(obj_xpath); // just for info
#endif//OES_DEBUG_BUILD

            HdfAry ary;
            AnyArray any_ary;
            bool too_large = true;
            if (!processDataset(file_id, ary_path, max_byte_size, ary, any_ary, too_large)) {
                OES_ERROR("Invalid or missing HDF5 array: {}", ary_path);
                return false;
            }

            if (too_large) {
                hdf_big_arrays.emplace_back(ary_path, std::move(ary));
                continue; // postponed to next phase
            }

            const auto it = arr_context.hdf_uris.find(hdf_id);
            EmlUri uri;
            if(it != arr_context.hdf_uris.end()){
                uri = it->second;
            }else{
                uri = std::get<2>(entry.second);
            }

            if (!readAndSendFullAry(ary_path, ary, any_ary, uri)) {
                OES_ERROR("Cannot PUT HDF5 array: {}", ary_path);
                return false;
            }
        }
        if(!hdf_big_arrays.empty()){
            big_arrays.emplace(hdf_filename.first,std::move(hdf_big_arrays));
        }

        progress += (24.0 - 4.0)/static_cast<double>(arr_context.hdf_filenames.size());
        OES_INFO("Send small-enough arrays progress: {}%", static_cast<int>(progress));
    }
    if (!put_ary_phase.finish("PUT", "arrays")) {
        return false;
    }
    totals.add(put_ary_phase);

    if (big_arrays.empty()) {
        return commitTx();
    }

    progress = 27.0;
    OES_INFO("Send small-enough arrays progress: {}%", static_cast<int>(progress));

    // Phase 3: Declare (PutUninit) "large" arrays (if any)
    auto uninit_rpc1 = rpcFor(array_proto, &IDataArray::PutUninitializedDataArrays);
    auto uninit_rpc2 = rpcFor(array_proto, &IDataArray::PutUninitializedDataArrays);
    ImportEpcPhase<decltype(uninit_rpc1), IniAryTrait> ini_ary_phase(
        uninit_rpc1, uninit_rpc2, eIniPhase, "INI arrays",
        max_byte_size, options.max_batch_size_, options.timeout_
    );

    auto initAndSendAryDecl = [&ini_ary_phase, &arr_context](
        const std::string& path, const HdfAry& ary, const std::string& hdf_id
    ) {
        const auto it = arr_context.hdf_uris.find(hdf_id);
        assert(it != arr_context.hdf_uris.end());

        oes::client::data12::DataArrayTypes::PutUninitializedDataArrayType msg;
        msg.m_uid.m_uri = it->second.to_string(); // NOT the "real owner" of the data, but the EpcExt
        msg.m_uid.m_pathInResource = path;

        msg.m_metadata.m_dimensions.resize(ary.rank);
        switch (ary.rank) {
        case  4: msg.m_metadata.m_dimensions[3] = ary.dims[3]; [[fallthrough]];
        case  3: msg.m_metadata.m_dimensions[2] = ary.dims[2]; [[fallthrough]];
        case  2: msg.m_metadata.m_dimensions[1] = ary.dims[1]; [[fallthrough]];
        case  1: msg.m_metadata.m_dimensions[0] = ary.dims[0]; break;
        default: assert(false); return false;
        }

        {
            using namespace oes::common::hdf;
            auto& ary_type = msg.m_metadata.m_transportArrayType; // alias

            switch (ary.elem_type) {
            case eAryOfVoid:    assert(false); return false;

            case eAryOfInt:     ary_type = oes::client::data12::AnyArrayType::eArrayOfInt;     break;
            case eAryOfDouble:  ary_type = oes::client::data12::AnyArrayType::eArrayOfDouble;  break;
            case eAryOfFloat:   ary_type = oes::client::data12::AnyArrayType::eArrayOfFloat;   break;
            case eAryOfBytes:   ary_type = oes::client::data12::AnyArrayType::eBytes;          break;
            case eAryOfBoolean: ary_type = oes::client::data12::AnyArrayType::eArrayOfBoolean; break;
            case eAryOfLong:    ary_type = oes::client::data12::AnyArrayType::eArrayOfLong;    break;

            case eAryOfString:  assert(false); return false;
            default:            assert(false); return false;
            }
        }

        const size_t msg_overhead_bytes =
            msg.m_uid.m_pathInResource.size() +
            msg.m_uid.m_uri.size() +
            msg.m_metadata.m_dimensions.size() * 4 +
            4 // array type
        ;
        return ini_ary_phase.add(path, std::move(msg), 0, msg_overhead_bytes);
    };

    for (const auto& ba : big_arrays) {
        const std::string& hdf_uri = ba.first;
        for (const auto& entry : ba.second) {
            const std::string& path = entry.first;
            const HdfAry& ary = entry.second;

            if (!initAndSendAryDecl(path, ary, hdf_uri)) {
                OES_ERROR("Cannot PUT HDF5 uninit-array: {}", path);
                return false;
            }
        }
    }

    if (!ini_ary_phase.finish("INI", "arrays")) {
        return false;
    }
    totals.add(ini_ary_phase);
    progress = 30.0;
    arr_context.ini_ary_phase_count = ini_ary_phase.count_;
    arr_context.progress = progress;

    OES_INFO("Declare large arrays progress: {}%", static_cast<int>(progress));

    bool ok = commitTx();
    if (!ok) {
        OES_ERROR("Error commiting objects+init arrays part");
        return false;
    }

    // destroy this client instance
    client_ptr.reset(nullptr);

    ok = importArrays(client_config, space_uri_txt, epc_filename, arr_context, totals, options);
    if (!ok) {
        OES_ERROR("Could not import arrays.");
        return false;
    }

    totals.print();
    OES_INFO("Total progress: 100%");

    return true;
}

//============================================================================

using AryVal = std::tuple<std::string, std::string, EmlUri, std::string>;
using epc::xml::AbstractCitedDataObject;
using epc::xml::NameValuePair;
using epc::xml::XmlPathGetter;
using epc::xml::DataObjectReference;
using epc::xml::Hdf5Dataset;


bool importXmlStringObjects(
    Client& client,
    const std::string& space_uri_txt,
    std::map<std::string, std::string>&& partIdStr_xmlStr_map,
    const ImportEpcOptions& options
) {

    const size_t max_byte_size = size_t(client.negotiated_max_msg_size() * 0.8);
    const EmlUri space_uri = EmlUri::parse(space_uri_txt);
    
    auto put_obj_rpc1 = rpcFor(client.store_proto(), &oes::client::proto12::IStore::PutDataObjects);
    auto put_obj_rpc2 = rpcFor(client.store_proto(), &oes::client::proto12::IStore::PutDataObjects);
    ImportEpcPhase<decltype(put_obj_rpc1), PutObjTrait> put_obj_phase(
        put_obj_rpc1, put_obj_rpc2, eObjPhase, "PUT objects",
        max_byte_size, options.max_batch_size_, options.timeout_
    );
    
    bool put_obj_ok = true;
    for (auto& entry : partIdStr_xmlStr_map) {

        std::string part_id_str = entry.first;
        auto xml_str = std::move(entry.second);

        Guid uuid;
        XmlName xname;
        AbstractCitedDataObject cited;
        std::map<std::string,std::string> filename_array_uri;

        {
            using namespace epc;
            const epc::FilePart::ScanRc rc = epc::FilePart::scan(
                xml_str,
                [&uuid, &xname, &cited](
                    const XmlName& n, const AbstractCitedDataObject& c
                ) {
                    uuid.reparse(c.uuid_.c_str(), c.uuid_.length(), false);
                    xname = n;
                    cited = c;
                    return epc::FilePart::eOK_DONE;
                },
                nullptr,
                nullptr,
                nullptr
            );
            // Replace all occurrences of filename by hdf_filename in xml
            for(const auto& filename_array : filename_array_uri){
                xml_str = FilePart::replaceNestedXMLValue(
                    xml_str, "ExternalDataArrayPart", "URI", filename_array.first, filename_array.second
                );
            }

            if (rc != epc::FilePart::eOK_DONE) {
                OES_ERROR("Cannot XML parse part {}", part_id_str);
                continue;
            }
        }
        if (!isMl(xname.xmlns_uri_)) {
            return false;
        }
        if (!uuid.isValid()) {
            OES_ERROR("Invalid Guid: Part ignored: {}", part_id_str);
            return false;
        }

        const EmlUri obj_uri = makeObjUri(space_uri, xname, uuid);

        oes::common::etp12::Datatypes::Object::DataObject etp_obj;
        etp_obj.m_format = "xml";
        
        bool send_chunks = false;
        oes::common::etp12::Datatypes::Uuid blobId;
        if(xml_str.size() >= max_byte_size){
            send_chunks = true;
            oes::common::assignUid(blobId, Guid::make());
            etp_obj.m_blobId = blobId;
        } else {
            OES_UNUSED(blobId);
            etp_obj.m_data.assign(xml_str.begin(), xml_str.end());
        }
        
        etp_obj.m_resource.m_name = std::move(cited.Title_);
        etp_obj.m_resource.m_uri = obj_uri.to_string();

        std::chrono::microseconds last_changed_usec;
        auto last_changed_valid = oes::core::usecSinceEpochFromExtendedIsoDateTime(
                cited.lastChanged(), last_changed_usec
            );

        if (last_changed_valid) {
            etp_obj.m_resource.m_lastChanged = last_changed_usec.count();
        } else {
            OES_ERROR("Invalid timestamp: {}", cited.lastChanged());
            return false;
        }

        const size_t msg_overhead_bytes =
            etp_obj.m_format.size() +
            etp_obj.m_resource.m_name.size() +
            etp_obj.m_resource.m_uri.size()
        ;
        
        put_obj_ok = put_obj_phase.add(
            std::move(part_id_str), std::move(etp_obj),
            etp_obj.m_data.size(), msg_overhead_bytes,
            send_chunks
        );
        if (!put_obj_ok) {
            OES_ERROR("Could not put object");
            return false;
        }
        if(send_chunks){
            auto chunk_obj_rpc = rpcFor(client.store_proto(), &oes::client::proto12::IStore::Chunk);
            chunk_obj_rpc.begin_multi_part();
            chunk_obj_rpc.req_.m_blobId = blobId;

            const size_t chunk_total_size = xml_str.size();

            size_t nb_chunks = std::lround(std::ceil(1.0 * chunk_total_size / (max_byte_size)));
            size_t chunk_size = std::lround(std::ceil(1.0 * chunk_total_size / nb_chunks));
            for(size_t offset=0; offset<chunk_total_size; offset+=chunk_size){
                nb_chunks--;
                size_t size = std::min<>(chunk_size, chunk_total_size - offset);
                chunk_obj_rpc.req_.m_data = std::move(std::vector<uint8_t>(xml_str.begin()+offset, xml_str.begin()+offset+size));
                chunk_obj_rpc.req_.m_final = nb_chunks==0;
                if(chunk_obj_rpc.req_.m_final) {
                    chunk_obj_rpc.sending_final_part();
                }
                chunk_obj_rpc.async_send(std::move(chunk_obj_rpc.req_), options.timeout_); 
            }
        }
        
    }
    
    if (!put_obj_phase.finish("PUT", "objects")) {
        OES_ERROR("Could not finish put obejcts");
        return false;
    }

    return true;
}

} // namespace oes::client
