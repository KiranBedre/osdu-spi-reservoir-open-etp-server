// ============================================================================
// Copyright 2021 Emerson Paradigm Holding LLC. All rights reserved.
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

#include <cmath>
#include <filesystem>
#include <functional>
#include <optional>
#include <set>
#include <string_view>
#include <sstream>
#include <map>
#include <unordered_map>

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/trim.hpp>
#include <oes/core/utils/config_leave_3rdparty.h>

#include "Hdf5.h"
#include "Client.hpp"
#include "ExportEpc.h"
#include "ImportExportUtils.h"

#include <oes/common/epc/FilePart.h>
#include <oes/common/epc/Package.h>
#include <oes/common/etp12/Protocols.h>
#include <oes/common/etp12/ArrayUtils.h>
#include <oes/common/etp12/EmlUri.h>
#include <oes/core/utils/Assert.h>
#include <oes/core/utils/ChronoUtils.h>
#include <oes/core/utils/Guid.hpp>
#include <oes/core/utils/Logger.h>
#include <oes/core/utils/ThousandSep.h>
#include <oes/core/utils/ThreadUtils.hpp>


namespace fs = std::filesystem;

using oes::core::Guid;
using oes::core::thou;
using oes::core::HighResTimer;

using oes::common::epc::FilePart;
using oes::common::epc::RelType;
using oes::common::epc::Relationship;

using oes::common::etp12::Datatypes::Object::DataObject;
using oes::common::etp12::Datatypes::Object::Resource;
using oes::common::etp12::Protocol::Store::GetDataObjectsResponse;
using oes::common::etp12::EmlUri;
using oes::common::SpscQueue;
namespace Protocol = oes::common::etp12::Protocol;

namespace {

using namespace oes::common::etp12;
using namespace oes::client;

//============================================================================

auto toIndex(const std::string& key, size_t max_size) -> std::pair<size_t, bool> {
    std::pair<size_t, bool> rc(0, false);
    try {
        std::size_t pos;
        auto idx = std::stoll(key, &pos);
        OES_ASSERT(pos == key.length(), return rc;);
        OES_ASSERT(0 <= idx && idx < (long long)(max_size), return rc;);
        rc.first = idx;
        rc.second = true;
        return rc;
    } catch (...) {
        return rc;
    }
}

static const int chunking_threshold = 4096;

void get_chunk(std::vector<hsize_t>& chunking) {
    hsize_t chunk_size = chunking_threshold;
    chunking[chunking.size()-1] = std::min<hsize_t>(chunk_size, chunking[chunking.size()-1]);
    if (chunking.size() > 1) {
        chunk_size = chunk_size / chunking[chunking.size()-1];
        for ( hsize_t i = chunking.size() - 2; i >= 0; i-- ){
            chunking[i] = std::min<hsize_t>(chunk_size, chunking[i]);
            chunk_size /= chunking[i];
            if (chunk_size == 0 || i == 0) {
                break;
            }
        }
    }
}

H5::Group open_or_create_group(H5::H5Location& loc, const std::string& group) {
    if (!H5Lexists(loc.getId(), group.c_str(), H5P_DEFAULT)) {
        return loc.createGroup(group);
    }
    return loc.openGroup(group);
}

H5::Group open_or_create_groups(H5::H5Location& loc, const std::vector<std::string>& steps) {
    switch (steps.size()) {
    case  0: assert(false); return {};
    case  1: return open_or_create_group(loc, steps[0]);
    default: break;
    }

    H5::Group group;
    H5::H5Location* curr_loc = &loc;
    for (const std::string& step : steps) {
        group = open_or_create_group(*curr_loc, step);
        curr_loc = &group;
    }
    return group;
}

//============================================================================

struct HdfDataset {
    H5::DataType data_type;
    H5::DataSpace data_space;
    H5::DataSet dataset;
};
using HdfDataset_uptr = std::unique_ptr<HdfDataset>;

//============================================================================

struct AryParts {
    AryDivision div_;
    std::vector<AryAny> subarys_;

    void write(HdfDataset& ds, size_t idx);
};

struct Ary {
    size_t bytes_ = 0;
    AryIdent ident_; // server-side key, to fetch the array
    AryMeta  meta_;
    std::optional<AryAny> data_;
    std::unique_ptr<AryParts> parts_;

    std::string container_; // see HdfPathKey
    std::string hdf_path_;  // dataset path in the exported HDF5 file

    HdfDataset_uptr write(
        H5::H5File& hdf_file, int hdf5_compression, int hdf5_large_data_compression
    );
};
using AryVec = std::vector<Ary>;

//============================================================================

struct Obj;

struct Rel {
    Guid uid_;
    Obj* target_ = nullptr;
    RelType type_ = RelType::eUnknownType;

    bool operator==(const Rel& other) const {
        return std::tie(type_, target_) == std::tie(other.type_, other.target_);
    }
    bool operator<(const Rel& other) const {
        return std::tie(type_, target_) < std::tie(other.type_, other.target_);
    }
};

constexpr std::string_view epc_ext_part_ref_type1 = "obj_EpcExternalPartReference";
constexpr std::string_view epc_ext_part_ref_type2 = "EpcExternalPartReference"; // just in case

struct ExportCtx;

struct Obj {
    Obj() = default;
    Obj(Obj&&) = default;
    Obj& operator=(Obj&&) = default;

    Obj(const Obj&) = delete;
    Obj& operator=(const Obj&) = delete;

    std::string disco_uri_;
    EmlUri uri_;

    std::vector<Rel> rels_;

    FilePart* part_ = nullptr;

    bool has_datasets_ = false;
    bool skip_ = false; // e.g. OnGoing activities

private:
    char obj_format_[8];
    std::string obj_data_;
    std::unique_ptr<DataObject> obj_; // instead of std::optional, to use less memory

public:
    bool hasObj() const;
    const DataObject& getObj() const;
    const std::string& getObjData() const;
    void moveObjData(std::string& dest);
    void setObj(DataObject obj);
    void resetObj();

    void processRels(ExportCtx& ctx);

    bool isExtPartRef() const;

    void addRel(
        Obj& target,
        RelType type = RelType::eDestinationObject,
        RelType mate = RelType::eSourceObject
    );

    std::string type() const;
    std::string epcPartName() const;
    std::string epcMimeType() const;
};

using ObjVec = std::vector<Obj>;

// ============================================================================

struct ExtPartInfo {
    FilePart* part_ = nullptr;
    size_t obj_refs_ = 0;
    fs::path path_;
};

static constexpr size_t null_idx = size_t(-1);
struct AryAndDiv {
    size_t ary_idx_ = null_idx;
    size_t div_idx_ = null_idx;

    constexpr explicit operator bool() const {
        return ary_idx_ != null_idx;
    }
};

//============================================================================

using namespace oes::common;

//! An object's identity: its ETP type ("resqml20.obj_MdDatum") and its uuid.
using TypedUid = std::pair<std::string, Guid>;

struct ExportCtx {
    const std::string epc_filename_;
    const std::string hdf_filename_;
    const ExportEpcOptions& opts_;

    ObjVec objs_;
    std::map<TypedUid, size_t> objmap_; // used to resolve relations later

    // EpcExternalPartReference is the only HDF proxy type, and its ML prefix
    // varies between writers, so proxies are indexed by guid alone.
    std::unordered_map<Guid, size_t> hdf_proxies_;

    epc::Package epc_;

    size_t epc_ext_part_ref_count_ = 0;
    std::map<Obj*, ExtPartInfo> ext_parts_map_;

    AryVec array_infos_;
    std::map<HdfPathKey, size_t> ary_keys_; // dedups arrays shared by several objects
    std::map<HdfPathKey, std::string> relocated_;

    HighResTimer timer_, global_timer_;
    std::vector<std::tuple<std::string, double, std::string>> phases_;
    void endPhase(const std::string& phase) {
        auto ram = oes::core::System::getMemoryUsage().peakMB();
        phases_.emplace_back(phase, timer_.elapsedSec(), ram);
    };

    EpcStatistics* p_arg_stats_;
    EpcStatistics local_stats_;
    EpcStatistics& stats() { return p_arg_stats_? *p_arg_stats_: local_stats_; }
    void updateStats() {
        stats().elapsed_sec_ = global_timer_.elapsedSec();
    };

    SpscQueue<Obj*> obj_queue_;
    SpscQueue<AryAndDiv> ary_queue_;

    ExportCtx(
        const std::string& epc_filename,
        const std::string& hdf_filename,
        const ExportEpcOptions& opts,
        EpcStatistics* p_stats
    );

    void addResource(Resource& res);
    Obj* setObject(size_t idx, DataObject& etp_obj);
    Obj* resolveObject(const TypedUid& uid);
    Obj* resolveHdfProxy(const Guid& guid);

    void openEpc();
    void addRelsAndCloseEpc();
};

//============================================================================

HdfDataset_uptr Ary::write(
    H5::H5File& hdf_file, int hdf5_compression, int hdf5_large_data_compression
) {
    Ary& ary = *this;
    std::string name;
    const std::string& path = ary.hdf_path_;

    H5::Group curr_grp;
    std::vector<std::string> steps;
    boost::split(steps, path, boost::algorithm::is_any_of("/"));
    assert(!steps.empty());
    name = std::move(steps.back());
    steps.pop_back();

    const bool has_group = !steps.empty();
    if (has_group) {
        curr_grp = open_or_create_groups(hdf_file, steps);
    }
    H5::H5Location& curr_loc = has_group? curr_grp: hdf_file;

    auto ds = std::make_unique<HdfDataset>();

    H5::DataType& data_type = ds->data_type;
    switch (ary.meta_.m_transportArrayType) {
    case AryType::eArrayOfBoolean:  data_type = H5::PredType::NATIVE_INT8;   break;
    case AryType::eArrayOfInt:      data_type = H5::PredType::NATIVE_INT32;  break;
    case AryType::eArrayOfLong:     data_type = H5::PredType::NATIVE_INT64;  break;
    case AryType::eArrayOfFloat:    data_type = H5::PredType::NATIVE_FLOAT;  break;
    case AryType::eArrayOfDouble:   data_type = H5::PredType::NATIVE_DOUBLE; break;
    case AryType::eArrayOfString:   assert(false); break;
    case AryType::eBytes:           data_type = H5::PredType::NATIVE_UINT8;  break;
    } // no default on purpose

    const auto& dims = ary.meta_.m_dimensions;
    std::vector<hsize_t> hdims(dims.begin(), dims.end());

    H5::DSetCreatPropList plist;
    if((hdf5_compression > 0 && hdf5_large_data_compression > 0) && hdims.size()>0 && hdims[0]>0){
        std::vector<hsize_t> chunking = hdims;
        get_chunk(chunking);
        plist.setChunk(int(chunking.size()), chunking.data());
        plist.setShuffle();
        plist.setDeflate(ary.data_ ? hdf5_compression : hdf5_large_data_compression);
    }

    H5::DataSpace& data_space = ds->data_space;
    data_space.setExtentSimple(int(hdims.size()), hdims.data(), hdims.data());
    
    ds->dataset = curr_loc.createDataSet(
        name.c_str(), data_type, data_space, plist
    );

    if (ary.bytes_ == 0) {
        return nullptr; // nothing to write(but dataset still created...)
    }

    if (ary.data_) {
        OES_ASSERT(aryTypeOf(ary.data_->m_item) == aryTypeOf(ary.meta_), return nullptr;);

        std::vector<int8_t> bools;
        AryMemRef ary_mem = aryMemoryOf(ary.data_->m_item, bools);
        OES_ASSERT(ary_mem, return nullptr;);
        H5::DataSet& dataset = ds->dataset;
        dataset.write(ary_mem.data_, data_type, data_space, data_space);
        return nullptr;
    }

    return ds;
}

void AryParts::write(HdfDataset& ds, size_t idx) {
    const AryDivision& div = div_;

    const AryAny& data = subarys_[idx];

    std::vector<int8_t> bools;
    AryMemRef ary_mem = aryMemoryOf(data.m_item, bools);
    OES_ASSERT(ary_mem, return;);

    std::vector<hsize_t> starts, counts;
    size_t offset = div.initDataSubarraysType(idx, starts, counts);
    OES_UNUSED(offset);

    H5::DataSpace mem_space;
    hsize_t slab_size = productOf(counts);
    mem_space.setExtentSimple(1, &slab_size, &slab_size);

    ds.data_space.selectHyperslab(H5S_SELECT_SET, counts.data(), starts.data());

    ds.dataset.write(ary_mem.data_, ds.data_type, mem_space, ds.data_space);
}

//============================================================================

bool Obj::hasObj() const {
    return obj_.get();
}

const DataObject& Obj::getObj() const {
    assert(hasObj());
    return *obj_;
}

const std::string& Obj::getObjData() const {
    return obj_data_;
}
void Obj::moveObjData(std::string& dest) {
    assert(!obj_data_.empty());
    dest = std::move(obj_data_);
}

void Obj::setObj(DataObject obj) {
    assert(!obj_);
    obj_.reset(new DataObject(std::move(obj)));

    // "move" obj data (i.e. XML or JSON) to a string, the type we need.
    // FIXME: Avoid the need for that copy.
    obj_data_.assign(obj_->m_data.data(), obj_->m_data.data() + obj_->m_data.size());
    obj_->m_data.clear();
    obj_->m_data.shrink_to_fit();

    // Cache the data's format (XML or JSON normally) in a compact way.
    assert(obj_->m_format.size() < 8);
    strncpy(obj_format_, obj_->m_format.data(), sizeof(obj_format_) - 1);
    obj_format_[sizeof(obj_format_) - 1] = 0;
}

void Obj::resetObj() {
    obj_.reset();
    obj_data_.clear();
    obj_data_.shrink_to_fit();
}


bool Obj::isExtPartRef() const {
    return
        (uri_.obj_type_ == epc_ext_part_ref_type1) ||
        (uri_.obj_type_ == epc_ext_part_ref_type2)
    ;
}

void Obj::addRel(Obj& target, RelType type, RelType mate) {
    auto& this_rel = rels_.emplace_back();
    auto& mate_rel = target.rels_.emplace_back();
    this_rel.uid_ = mate_rel.uid_ = Guid::make();
    this_rel.type_ = type;
    mate_rel.type_ = mate;
    this_rel.target_ = &target;
    mate_rel.target_ = this;
}

void Obj::processRels(ExportCtx& ctx) {
    assert(part_);

    std::sort(rels_.begin(), rels_.end());
    rels_.erase(std::unique(rels_.begin(), rels_.end()), rels_.end());
    for (const Rel& obj_rel : rels_) {
        Relationship rel(true);
        rel.setType(obj_rel.type_);
        rel.setTarget(obj_rel.target_->epcPartName());
        rel.setId(fmt::format("_{}_{}", obj_rel.target_->uri_.guid_.toXmlString(), obj_rel.uid_.toXmlString()));
        part_->addRelationship(std::move(rel));

        if (obj_rel.target_->isExtPartRef()) {
            auto& ext_part = ctx.ext_parts_map_[obj_rel.target_];
            ++ext_part.obj_refs_;
        }
    }

    rels_.clear(); // release memory eagerly
}

std::string Obj::type() const {
    if (uri_.ml_ != EmlUri::eEML20) {
        return uri_.obj_type_;
    }
    if (boost::starts_with(uri_.obj_type_, "obj_")) {
        return uri_.obj_type_;
    }
    return "obj_" + uri_.obj_type_;
}

// e.g. "/obj_ContinuousProperty_00008e2a-2735-4479-bd90-9b21a10fc01c.xml"
std::string Obj::epcPartName() const {
    return fmt::format(
        "{}_{}.{}", type(), uri_.guid_.toXmlString(), obj_format_
    );
}

// e.g. "application/x-resqml+xml;version=2.0;type=obj_ContinuousProperty";
std::string Obj::epcMimeType() const {
    const char* ml;
    const char* ver;
    switch (uri_.ml_) {
    case EmlUri::eEML20: ml = "eml"; ver = "2.0"; break;
    case EmlUri::eEML23: ml = "eml"; ver = "2.3"; break;
    case EmlUri::eRESQML20: ml = "resqml"; ver = "2.0"; break;
    case EmlUri::eRESQML22: ml = "resqml"; ver = "2.2"; break;
    case EmlUri::eWITSML21: ml = "witsml"; ver = "2.1"; break;
    case EmlUri::ePRODML22: ml = "prodml"; ver = "2.2"; break;
    case EmlUri::ePRODML23: ml = "prodml"; ver = "2.3"; break;
    default: ml = "energistics"; ver = "1.0"; break;
    }
    return fmt::format(
        "application/x-{}+{};version={};type={}", // See epc/ContentType.h
        ml, obj_format_, ver, type()
    );
}

//============================================================================

//! The container of a dataset, see HdfPathKey. \a owner_uri is the fallback.
std::string containerOf(const epc::xml::DataObjectReference& proxy, const std::string& owner_uri) {
    if (!proxy.EnergisticsUri_.empty() && proxy.LocatorUrl_ == "application/x-hdf5") {
        return proxy.EnergisticsUri_;
    }
    const Guid guid = Guid::parse(proxy.Uuid_);
    return guid.isValid()? guid.toXmlString(): owner_uri;
}

//! Plans the HDF5 path of each array (sets Ary::hdf_path_).
//! \return the relocated arrays, i.e. those not keeping their original path.
std::map<HdfPathKey, std::string> planArrayPaths(AryVec& arys, std::ostream* warn) {
    std::vector<HdfPathKey> keys;
    keys.reserve(arys.size());
    for (const Ary& ary : arys) {
        keys.emplace_back(ary.container_, normalizeHdfPath(ary.ident_.m_pathInResource));
    }

    const auto planned = planHdfPaths(keys);

    std::map<HdfPathKey, std::string> relocated;
    for (size_t i = 0; i < arys.size(); ++i) {
        const std::string& out = planned.at(keys[i]);
        arys[i].hdf_path_ = out;
        if (out != keys[i].second) {
            OES_DEBUG("Dataset {} of {} relocated to {}", keys[i].second, keys[i].first, out);
            relocated.emplace(keys[i], out);
        }
    }

    if (warn && !relocated.empty()) {
        const auto N = relocated.size();
        OES_WARNING("{} dataset{} relocated (same path in different external parts)",
            thou(N), ((N == 1)? "": "s")
        );
    }
    return relocated;
}

//! \return \a xml with the paths of its relocated datasets changed.
std::string relocateDatasets(
    const std::string& xml, const std::string& owner_uri,
    const std::map<HdfPathKey, std::string>& relocated
) {
    return FilePart::rewriteDatasetPaths(
        xml,
        [&](const epc::xml::Hdf5Dataset& hdf) -> std::optional<std::string> {
            auto found = relocated.find({
                containerOf(hdf.HdfProxy_, owner_uri), normalizeHdfPath(hdf.PathInHdfFile_)
            });
            if (found == relocated.end()) {
                return std::nullopt;
            }
            return '/' + found->second;
        }
    );
}

//============================================================================

struct PhaseBase {
    double proc_ = 0;
    double wait_ = 0;
    double total_ = 0;

    std::string peak_ram_;

    size_t item_count_ = 0;

    enum class ExitStatus {
        eDONE = 0, eTIMEOUT, ePROLOG_ERROR, ePROCESS_ERROR, eEXCEPTION
    };

    ExitStatus status_ = ExitStatus::eDONE;
    std::string error_;

    bool prolog() { return true; }  // no-ops, to avoid derived impl
    void epilog(ExitStatus) {}      // having those unless necessary

    std::string toString(const std::string& label, double wall_time) const;
};

template <
    class Self, // i.e. CRTP
    typename Value
>
struct Phase : PhaseBase {
    Phase(Phase&&) = delete;
    Phase(const Phase&) = delete;
    Phase& operator=(Phase&&) = delete;
    Phase& operator=(const Phase&) = delete;

    explicit Phase(SpscQueue<Value>& queue, size_t max_msg_size, std::function<bool()> health_check = nullptr);

    SpscQueue<Value>& queue_;
    size_t max_msg_size_;
    std::function<bool()> health_check_;

    void operator()() {
        processAll();
    }

    void processAll();

    bool finished(const Value& v) const {
        const bool ok(v);
        return !ok;
    }

    void finish(HighResTimer& timer, ExitStatus es);
};

//============================================================================

struct ParseXmlPhase final : Phase<ParseXmlPhase, Obj*> {
    using Base = Phase<ParseXmlPhase, Obj*>;

    ExportCtx& ctx_;

    ParseXmlPhase(ExportCtx& ctx, size_t max_msg_size, SpscQueue<Obj*>& queue);

    bool processOne(Obj* p_obj);
    void epilog(ExitStatus es);
};

struct WriteHdfPhase final : Phase<WriteHdfPhase, AryAndDiv> {
    using Base = Phase<WriteHdfPhase, AryAndDiv>;

    H5::H5File hdf_file_;
    std::string hdf_filename_;

    AryVec& array_infos_;

    // HDF5 compression. valid range: [0 _ 9].
    // 0 none; 1 fast but bigger; 9 best but slower
    int hdf5_compression_ = 3;
    int hdf5_large_data_compression_ = 3;

    size_t prev_ary_idx_ = null_idx;
    HdfDataset_uptr prev_ds_;

    WriteHdfPhase(
        SpscQueue<AryAndDiv>& queue,
        size_t max_msg_size,
        const std::string& hdf_filename,
        AryVec& array_infos,
        int hdf5_compression,
        int hdf5_large_data_compression,
        std::function<bool()> health_check = nullptr
    );

    bool prolog();
    bool processOne(const AryAndDiv& idx);
};

} //namespace

namespace oes::client {

//============================================================================

/*!
* \brief Imports an EPC file, into an ETP 1.2 server.
*
* The two main parameters affecting this functions are:
* # client.negotiated_max_msg_size()
* # max_batch_size
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
* \param client ETP 1.2 client, already connected and authenticated with the server
* \param etp_space the destination ETP dataspace path (not URI), for all data
* \param epc_filename the input EPC filename
* \param hdf_filename the input HDF5 filename
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
bool exportEpc(
    Client& client,
    const std::string& etp_space,
    const std::string& epc_filename,
    const std::string& hdf_filename,
    const ExportEpcOptions& options,
    EpcStatistics* p_stats
) {
    HighResTimer timer, global_timer;
    std::vector<std::tuple<std::string, double, std::string>> phases;
    auto endPhase = [&timer, &phases](const std::string& phase) {
        auto ram = oes::core::System::getMemoryUsage().peakMB();
        phases.emplace_back(phase, timer.elapsedSec(), ram);
    };

    // close to what ProtocolCapabilityKind::eMaxDataItemCount is,
    // except the doc mentions only the GET-from-store use-case,
    // not the PUT-to-store one of this function
    const size_t max_batch_size = options.max_batch_size_;
    const bool overwrite = options.overwrite_;
    std::chrono::milliseconds timeout = options.timeout_;

    std::ostream* out = options.out_;
    std::ostream* msg = options.msg_;
    std::ostream* warn = options.warn_;
    std::ostream* err = options.err_;

    std::ostringstream oss;
    std::ostream& rpc_err = err? *err: oss;

#ifdef OES_DEBUG_BUILD
    auto ping_rpc = rpcFor(client.core_proto(), &proto12::ICore::Ping);

    auto ping_ok = ping_rpc.call(oss, timeout);
    if (!ping_ok.first || ping_ok.second != std::future_status::ready) {
        OES_ERROR("Ping failed");
        return false;
    }
#endif //OES_DEBUG_BUILD

    double progress_value{1.0};

    if (out) {
        OES_INFO("Export Options: "
                 "overwrite = {}; "
                 "jobs = {}; "
                 "hdf5_comp = {}; "
                 "hdf5_large_comp = {}; "
                 "timeout = {} [s]; "
                 "max_msg_size (negociated) = {}; "
                 "transaction_retries_ = {};",
                 options.overwrite_,
                 options.thread_count_,
                 options.hdf5_compression_,
                 options.hdf5_large_data_compression_,
                 std::chrono::duration_cast<std::chrono::seconds>(options.timeout_).count(),
                 client.negotiated_max_msg_size(),
                 options.n_transaction_retries_);
    }

    EpcStatistics local_stats;
    EpcStatistics& stats = p_stats? *p_stats: local_stats;
    auto updateStats = [&]() {
        stats.elapsed_sec_ = global_timer.elapsedSec();
    };

    // Check output files, before anything
    const fs::path epc_path(epc_filename);
    fs::file_status epc_stat = fs::status(epc_path);
    if (fs::is_regular_file(epc_stat)) {
        if (!overwrite) {
            OES_ERROR("File exists: {}", epc_filename);
            return false;
        }
    } else if (fs::exists(epc_stat)) {
        OES_ERROR("Non-file exists: {}", epc_filename);
        return false;
    }

    const fs::path hdf_path(hdf_filename);
    fs::file_status hdf_stat = fs::status(hdf_path);
    if (fs::is_regular_file(hdf_stat)) {
        if (!overwrite) {
            OES_ERROR("File exists: {}", hdf_filename);
            return false;
        }
    } else if (fs::exists(hdf_stat)) {
        OES_ERROR("Non-file exists: {}", hdf_filename);
        return false;
    }

    const std::string space_uri_txt = getDataspaceUri(client, etp_space, options.timeout_);
    const EmlUri space_uri = EmlUri::parse(space_uri_txt);
    if(!space_uri.isValid()){
        OES_ERROR("Export EPC {} from ETP space {} failed: Dataspace not found", epc_filename, etp_space);
        return false;
    }
    const std::string message = "Export ETP Dataspace " + space_uri_txt;

    ScopedTransaction tx(client, timeout);
    if (!tx.supported()) {
        if (warn) {
            OES_WARNING("No Transaction support. Not read-consistent.");
        }
    } else if (!tx.start(space_uri_txt, message, /*read-only*/true, nullptr, options.n_transaction_retries_)) {
        if (warn) {
            OES_WARNING("Could not start Transaction. Not read-consistent.");
        }
    }
    auto commitTx = [err, &tx]() {
        std::string failure_reason;
        const bool tx_ok = tx.commit(&failure_reason);
        if (!tx_ok && err) {
            OES_ERROR("Cannot end read-only transaction: {}", failure_reason);
        }
        return tx_ok;
    };

    size_t epc_ext_part_ref_count = 0;

    endPhase("Prolog");
    if (out) {
        OES_INFO("Prolog progress: {}%", static_cast<int>(progress_value));
    }

    // First, figure out what's in that space, i.e. the TOC
    // We keep only the object's URI, throwing away the Resource we got,
    // since we'll fetch the DataObject next, which also embeds a Resource.
    ObjVec objs;
    std::map<TypedUid, size_t> objmap; // used to resolve relations later

    // EpcExternalPartReference is the only HDF proxy type, and its ML prefix
    // varies between writers, so proxies are indexed by guid alone.
    std::unordered_map<Guid, size_t> hdf_proxies;
    {
        auto* proto = client.disco_proto();
        if (!proto) {
            if (err) {
                OES_ERROR("Missing required protocol: {}", Protocol::IDiscovery::protocolName());
            }
            return false;
        }

        auto rpc = rpcFor(proto, &proto12::IDiscovery::GetResources);
        rpc.req_.m_context.m_uri = space_uri_txt;
        rpc.req_.m_context.m_depth = 1; // required by PV's server... See iLab#3
        rpc.req_.m_context.m_dataObjectTypes.clear();
        rpc.req_.m_storeLastWriteFilter.reset();
        if (out) {
            OES_INFO("Discovering {}...", rpc.req_.m_context.m_uri);
        }

        // For now, in case of multi-part response,
        // we simply accumulate the multi-part responses,
        // instead of having a proper concurrent pipeline.
        if (!rpc.callForEachValue(
            rpc_err, timeout,
            [&](auto& res) {
                auto& obj = objs.emplace_back();
                obj.uri_ = EmlUri::parse(res.m_uri);
                obj.disco_uri_ = std::move(res.m_uri);
                OES_ASSERT(obj.uri_.type_ == EmlUri::eOBJ_INSTANCE, objs.pop_back(); return;);

                if (obj.isExtPartRef()) {
                    ++epc_ext_part_ref_count;
                }
            }
        )) {
            return false;
        }

        if (objs.empty()) {
            if (out) {
                OES_WARNING("Empty dataspace.");
            }
            updateStats();
            return true;
        }

        // Warning: Makes fetching non-deterministic w/o it
        std::sort(objs.begin(), objs.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.disco_uri_ < rhs.disco_uri_;
        });

        for (size_t i = 0; i < objs.size(); ++i) {
            auto obj_uid_unique = objmap.emplace(
                TypedUid{objs[i].uri_.to_canonical_type(), objs[i].uri_.guid_}, i
            );
            OES_ASSERT(obj_uid_unique.second, objs.pop_back(); continue;);
            if (objs[i].isExtPartRef()) {
                hdf_proxies.emplace(objs[i].uri_.guid_, i);
            }
        }
    }

    if (out) {
        OES_INFO("Got {} resources ({} EpcExternalPartReference).",
            thou(objs.size()), epc_ext_part_ref_count
        );
    }

    endPhase("Discovery");
    if (out) {
        progress_value = 2.0;
        OES_INFO("Discovery progress: {}%", static_cast<int>(progress_value));
    }


    // Second, fetch the objects themselves, respecting the "batching" arg.
    size_t obj_errors = 0;
    {
        auto* proto = client.store_proto();
        if (!proto) {
            if (err) {
                OES_ERROR("Missing required protocol: {}",
                    Protocol::IStore::protocolName()
                );
            }
            return false;
        }

        std::vector<std::map<std::string, std::string>> chunks;
        chunks.reserve((objs.size() + max_batch_size - 1) / max_batch_size);
        auto* curr_chunk = &chunks.emplace_back();
        for (const auto& obj : objs) {
            if (curr_chunk->size() == max_batch_size) {
                curr_chunk = &chunks.emplace_back();
            }
            const size_t key = (chunks.size() - 1) * max_batch_size + curr_chunk->size();
            curr_chunk->emplace(std::to_string(key), obj.disco_uri_);
        }

        auto rpc = rpcFor(proto, &proto12::IStore::GetDataObjects);
        rpc.req_.m_format = "xml";

        const size_t max_size = objs.size();

        for (auto& chunk : chunks) {
            rpc.req_.m_uris = chunk;
            if (!rpc.callForEachValue(
                rpc_err, timeout,
                [&](auto& entry) {
                    auto [idx, obj_idx_ok] = toIndex(entry.first, max_size);
                    OES_ASSERT(obj_idx_ok, ++obj_errors; return;);

                    auto& obj = objs[idx];
                    OES_ASSERT(!obj.hasObj(), ++obj_errors; return;); // no duplicates
                    obj.setObj(std::move(entry.second));

                    OES_ASSERT(obj.getObj().m_resource.m_uri == obj.disco_uri_, ;);
                    ++stats.object_count_;
                    stats.object_bytes_ += obj.getObjData().size();
                }
            )) {
                return false;
            }
        }
    }

    assert(stats.object_count_ == (objs.size() - obj_errors));

    if (out) {
        OES_INFO("Got {} objects ({} XML bytes).",
            thou(stats.object_count_), thou(stats.object_bytes_)
        );
    }

    endPhase("Get Objects");
    if (out) {
        progress_value = 3.0;
        OES_INFO("Get Objects progress: {}%", static_cast<int>(progress_value));
    }
    
    std::set<std::string> external_file_reference;

    // Third, we parse the XMLs we got, to extract references to objects and arrays
    // References to arrays are stored in array_infos vector below.
    AryVec array_infos;
    std::unordered_set<std::string> toSkip ;
    {
        std::map<HdfPathKey, size_t> ary_keys;

        for (auto& obj : objs) {
            OES_ASSERT(obj.hasObj(), continue;);
            std::string xml(obj.getObjData()); // makes a copy
            auto [scan_data, rc] = FilePart::scanData(
                xml, /*FilePart::eCited|*/FilePart::eRefs|FilePart::eHdfs|FilePart::eXData
            );

            if (rc != FilePart::eOK_DONE) {
                if (err) {
                    OES_ERROR("Cannot XML parse part {}",
                        obj.disco_uri_
                    );
                }
                return false;
            }

            // Filter OnGoing activities if any
            if (obj.type() == "obj_Activity") {
                bool skip = false ;
                for( auto& [name, value]: scan_data.xdata_) {
                    if( name == "activityStatus" && value == "OnGoing") {
                        OES_INFO("Skipping OnGoing activity {}({})", obj.type(), obj.uri_.guid_.toXmlString());
                        toSkip.emplace(obj.uri_.to_string()) ;
                        skip = true ;
                        break;
                    }
                }
                if ( skip ) {
                    continue ;
                }
            }
// End filter OnGoing activities

            for (const auto& ref : scan_data.refs_) {
                auto ref_guid = Guid::parse(ref.second.Uuid_);
                OES_ASSERT(ref_guid.isValid(), continue;);
                auto ref_guid_found = objmap.find(
                    {EmlUri::canonicalType(ref.second.QualifiedType_), ref_guid}
                );
                OES_ASSERT(ref_guid_found != objmap.end(), continue;);
                Obj& target = objs[ref_guid_found->second];
                obj.addRel(target);
                ++stats.relation_count_;
            }

            obj.has_datasets_ = !scan_data.hdfs_.empty();
            for (auto& hdf : scan_data.hdfs_) {
                const auto& proxy = hdf.second.HdfProxy_;
                std::string array_uri = obj.disco_uri_;
                if(!proxy.EnergisticsUri_.empty() && proxy.LocatorUrl_=="application/x-hdf5"){
                    external_file_reference.insert(proxy.EnergisticsUri_);
                }else{
                    auto hdf_guid = Guid::parse(proxy.Uuid_);

                    if(hdf_guid.isValid()){
                        auto hdf_guid_found = hdf_proxies.find(hdf_guid);
                        OES_ASSERT(hdf_guid_found != hdf_proxies.end(), continue;);
                        auto& hdf_obj = objs[hdf_guid_found->second];
                        array_uri = hdf_obj.disco_uri_;
                        obj.addRel(
                            hdf_obj,
                            RelType::eMlToExternalPartProxy,
                            RelType::eExternalPartProxyToMl
                        );
                    }
                }

                // Having multiples object referencing the same array is a valid use case
                auto key_ok = ary_keys.emplace(
                    HdfPathKey{
                        containerOf(proxy, obj.disco_uri_),
                        normalizeHdfPath(hdf.second.PathInHdfFile_)
                    },
                    array_infos.size()
                );
                if (!key_ok.second) {
                    continue;
                }

                auto& array_id = array_infos.emplace_back();
                array_id.container_ = key_ok.first->first.first;
                array_id.ident_.m_pathInResource = std::move(hdf.second.PathInHdfFile_);
                array_id.ident_.m_uri = array_uri;
            }
        }
    }

    if (out) {
        OES_INFO("XML references {} objects, and {} arrays.",
            thou(stats.relation_count_), thou(array_infos.size())
        );
    }

    const std::map<HdfPathKey, std::string> relocated = planArrayPaths(array_infos, warn);

    endPhase("XML Parse");
    if (out) {
        progress_value = 5.0;
        OES_INFO("XML Parse progress: {}%", static_cast<int>(progress_value));
    }

    // Fourth, fetch info / meta-data about those arrays, respecting the "batching" arg,
    // so we can group or split fetches to respect max-msg-size
    if (!array_infos.empty()) {
        auto* proto = client.array_proto();
        if (!proto) {
            if (err) {
                OES_ERROR("Missing required protocol: {}", Protocol::IDataArray::protocolName());
            }
            return false;
        }

        using AryIdentMap = std::map<std::string, AryIdent>;
        std::vector<AryIdentMap> chunks;
        chunks.reserve((array_infos.size() + max_batch_size - 1) / max_batch_size);
        auto* curr_chunk = &chunks.emplace_back();
        const size_t max_size = array_infos.size();
        for (const auto& info : array_infos) {
            if (curr_chunk->size() == max_batch_size) {
                curr_chunk = &chunks.emplace_back();
            }
            const size_t key = (chunks.size() - 1) * max_batch_size + curr_chunk->size();
            curr_chunk->emplace(std::to_string(key), info.ident_);
        }
        stats.array_chunks_ += chunks.size();

        auto rpc = rpcFor(proto, &proto12::IDataArray::GetDataArrayMetadata);

        size_t meta_ok = 0;
        for (auto& chunk : chunks) {
            rpc.req_.m_dataArrays = chunk;

            if (!rpc.callForEachValue(
                rpc_err, timeout,
                [&](auto& entry) {
                    auto [idx, idx_ok] = toIndex(entry.first, max_size);
                    OES_ASSERT(idx_ok, return;);

                    auto& info = array_infos[idx];
                    OES_ASSERT(info.bytes_ == 0, return;); // i.e. not init yet
                    info.meta_ = entry.second;
                    info.bytes_ = byteSizeOf(info.meta_);
                    stats.array_bytes_ += info.bytes_;
                    ++stats.array_count_;
                    ++meta_ok;
                }
            )) {
                return false;
            }
        }
        OES_ASSERT(array_infos.size() == meta_ok, ;);
    }

    if (!array_infos.empty()) {
        // Sort arrays, so we can group them in batches
        std::sort(
            array_infos.begin(), array_infos.end(),
            [](const Ary& lhs, const Ary& rhs) {
                return lhs.bytes_ < rhs.bytes_;
            }
        );

        if (out) {
            OES_INFO("{} array bytes (largest {} bytes).",
                thou(stats.array_bytes_), thou(array_infos.back().bytes_)
            );
        }
    }

    endPhase("Get Array Meta");
    if (out) {
        progress_value = 7.0;
        OES_INFO("Get Array Meta progress: {}%", static_cast<int>(progress_value));
    }

    // Take 10% off the max-msg-size, to leave wiggle room for Avro serialization overhead.
    // A more formal framework to split messages which are too large is a TODO
    const size_t max_byte_size = size_t(client.negotiated_max_msg_size() * 0.9);

    // Fifth, fetch actual array data
    AryVec arrays;
    if (!array_infos.empty()) {
        auto* proto = client.array_proto();
        assert(proto); // already tested above

        auto rpc = rpcFor(proto, &proto12::IDataArray::GetDataArrays);
        auto& req_arys = rpc.req_.m_dataArrays;
        auto& res_arys = rpc.res_.m_dataArrays;

        const size_t max_size = array_infos.size();
        auto processArray = [&](auto& entry) {
            auto [idx, idx_ok] = toIndex(entry.first, max_size);
            OES_ASSERT(idx_ok, return;);

            auto& info = array_infos[idx];
            OES_ASSERT(info.meta_.m_transportArrayType == aryTypeOf(entry.second), ;);
            OES_ASSERT(!info.data_, return;); // i.e. no array data yet
            info.data_ = std::move(entry.second.m_data);
        };

        size_t cur_byte_size = 0;
        auto iter = array_infos.begin();
        const int width = 1 + static_cast<int>(std::log10(array_infos.size()));

        for (size_t idx = 0; iter != array_infos.end(); ++iter, ++idx) {
            const Ary& ary = *iter;
            if (ary.bytes_ == 0) {
                continue; // nothing to fetch, empty (yes, can happen)
            }
            if (ary.bytes_ > max_byte_size) {
                break; // no grouping possible anymore
            }

            const bool full_by_size = (cur_byte_size + ary.bytes_) > max_byte_size;
            const bool full_by_count = (req_arys.size() + 1) >= max_batch_size;
            if (full_by_size || full_by_count) {
                if (!rpc.callForEachValue(
                    rpc_err, timeout,
                    [&processArray](auto& entry) {
                        processArray(entry);
                    }
                )) {
                    return false;
                }

                // reset for next batch
                cur_byte_size = 0;
                req_arys.clear();
                res_arys.clear();
            }

            cur_byte_size += ary.bytes_;
            auto rc = req_arys.emplace(fmt::format("{:0{}}", idx, width), ary.ident_);
            OES_RELEASE_UNUSED(rc);
            assert(rc.second);
        }

        // process last batch of "small" arrays, if any
        if (!req_arys.empty()) {
            if (!rpc.callForEachValue(
                rpc_err, timeout,
                [&processArray](auto& entry) {
                    processArray(entry);
                }
            )) {
                return false;
            }
        }

        if (out) {
            progress_value = 9.0;
            OES_INFO("Get Small Array Data progress: {}%", static_cast<int>(progress_value)); // estimation
        }

        size_t current_left_size = array_infos.end() - iter;
        if (current_left_size == 0) {
            current_left_size = 1;
        }

        // Now processing arrays which exceed max_byte_size
        for (; iter != array_infos.end(); ++iter) {
            Ary& ary = *iter;

            // Prepare the resquest, assigning the constant parts
            auto sub_rpc = rpcFor(proto, &proto12::IDataArray::GetDataSubarrays);
            auto rc = sub_rpc.req_.m_dataSubarrays.emplace();
            assert(rc.second);
            auto& sub_ary = rc.first->second;
            sub_ary.m_uid = ary.ident_;

            ary.parts_ = std::make_unique<AryParts>();

            size_t esize = byteSizeOf(ary.meta_.m_transportArrayType);

            // "fixup" esize, to account for worse-case scenario of the
            // Avro varint encoding actually increasing the size of the subarray.
            // E.g. Geolog uses MAX_INT as NdV, and some logs are full of NdVs.
            // So we must be conversative to avoid for example that error:
            // DataArray(9).GetDataSubarrays(3) failed: ETP#8: Could not encode+send
            //     #1537(1530) DataArray(9).GetDataSubarraysResponse(8) F
            //     (-M 1000000): Oversize #0 (key: ) of 1: EOF reached
            switch (ary.meta_.m_transportArrayType) {
            case AryType::eArrayOfInt:  esize =  5; break;
            case AryType::eArrayOfLong: esize = 10; break;
            default: break;
            }

            auto& div = ary.parts_->div_ = AryDivision::make(
                ary.meta_.m_dimensions, esize, max_byte_size
            );
            assert(!div.empty());
            const size_t sub_ary_count = div.size();
            ary.parts_->subarys_.reserve(sub_ary_count);
            for (size_t i = 0; i < sub_ary_count; ++i) {
                size_t offset = div.initDataSubarraysType(i, sub_ary.m_starts, sub_ary.m_counts);
                OES_UNUSED(offset);

                // No rpc.callForEachValue() necessary, cannot split single-part message
                auto max_time = oes::core::getPartialTimeoutValue(max_byte_size);
                bool retry_logic = timeout > max_time;
                if (retry_logic) {
                    auto remaining_time = timeout;
                    while (true) {
                        auto ok = sub_rpc.call(rpc_err, remaining_time < max_time ? remaining_time : max_time);
                        if (ok.first) {
                            break;
                        }

                        remaining_time -= max_time;
                        if (remaining_time.count() < 0) {
                            OES_ERROR("Max timeout reached.");
                            return false;
                        }

                        if (!client.isConnOpen()) {
                            OES_ERROR("Client connection in bad shape!");
                            return false;
                        }
                        rpc_err.clear();

                        if (ok.second == std::future_status::timeout) {
                            OES_INFO("Partial timeout. Retrying...");
                            if (max_time < std::chrono::milliseconds(20000)) {
                                max_time += std::chrono::milliseconds(2000);
                            }
                        }
                    }
                } else {
                    auto ok = sub_rpc.call(rpc_err, timeout); //here
                    if (!ok.first) {
                        return false;
                    }

                }


                auto& subarys = sub_rpc.res_.m_dataSubarrays;
                OES_ASSERT(subarys.size() == 1, return false;);
                ary.parts_->subarys_.emplace_back(std::move(subarys.begin()->second.m_data));
            }
            if (out) {
                progress_value += (45.0 - 9.0)/static_cast<double>(current_left_size);
                OES_INFO("Get Large Array Data progress: {}%", static_cast<int>(progress_value));
            }
        }
    }

    endPhase("Get Array Data");
    if (out) {
        progress_value = 45.0;
        OES_INFO("Get Array Data progress: {}%", static_cast<int>(progress_value));
    }

    const bool commit_ok = commitTx();
    OES_ASSERT(commit_ok, ;); // should never happen (and warning already printed)

    endPhase("Commit");
    if (out) {
        progress_value = 47.0;
        OES_INFO("Commit progress: {}%", static_cast<int>(progress_value));
    }

    epc::Package epc;
    epc.openForWriting(epc_filename);

    // Copy/paste props from XMLOPCProxyImpl::XMLOPCProxyImpl()?
    //epc.getExtendedCoreProperty()["ResqmlSchemaDate"] = "20150817";
    epc.getExtendedCoreProperty()["OSDU-EML"] = "1.0";

    std::map<Obj*, ExtPartInfo> ext_parts_map;
    const std::string local_hdf = hdf_path.filename().string();

    for (auto& obj : objs) {
        OES_ASSERT(obj.hasObj(), continue;);
        if( toSkip.find(obj.uri_.to_string()) != toSkip.end()) {
            continue;
        }

        std::string part_name = obj.epcPartName();
        std::string content_type = obj.epcMimeType();

        std::string xml(obj.getObjData());

        if (obj.has_datasets_ && !relocated.empty()) {
            try {
                xml = relocateDatasets(xml, obj.disco_uri_, relocated);
            } catch (const std::exception& ex) {
                OES_ERROR("Cannot relocate datasets of {}: {}", obj.disco_uri_, ex.what());
                return false;
            }
        }

        if(external_file_reference.size() > 0){
            xml = FilePart::replaceNestedXMLValue(xml, "ExternalDataArrayPart", "URI", ".*", local_hdf);
        }

        FilePart* part = epc.createPart(std::move(xml), part_name);
        OES_ASSERT(part, continue;);

        if (obj.isExtPartRef()) {
            auto& ext_part = ext_parts_map[&obj];
            assert(!ext_part.part_ || ext_part.part_ == part);
            ext_part.part_ = part;
        }

        epc.addContentType(epc::ContentType(false, content_type, part_name));

        std::sort(obj.rels_.begin(), obj.rels_.end());
        obj.rels_.erase(std::unique(obj.rels_.begin(), obj.rels_.end()), obj.rels_.end());
        for (const Rel& obj_rel : obj.rels_) {
            epc::Relationship rel(true);
            rel.setType(obj_rel.type_);
            rel.setTarget(obj_rel.target_->epcPartName());
            rel.setId(fmt::format("_{}_{}", obj_rel.target_->uri_.guid_.toXmlString(), obj_rel.uid_.toXmlString()));
            part->addRelationship(std::move(rel));

            if (obj_rel.target_->isExtPartRef()) {
                auto& ext_part = ext_parts_map[obj_rel.target_];
                ++ext_part.obj_refs_;
            }
        }
    }

    if (ext_parts_map.empty()) {
        if (out) {
            OES_WARNING("Did not create External Part Reference to HDF5.");
        }
    } else {
        // Even when we have several EpcExternalPartReference, we still create
        // a single HDF5 file on disk (datasets sharing a path are relocated).
        for (auto& entry : ext_parts_map) {
            ExtPartInfo& hdf = entry.second;
            hdf.path_ = hdf_path;

            epc::Relationship rel(false); // i.e. external
            rel.setType(RelType::eExternalResource);
            rel.setTarget(hdf.path_.filename().string());
            rel.setId("Hdf5File");

            hdf.part_->addRelationship(std::move(rel));
        }
    }

    // TODO: Set epc::CoreProperty::created to the space's actual timestamp,
    //       instead of defaulting it to the export date in the call below.
    epc.writePackage();

    endPhase("Write EPC");
    if (out) {
        progress_value = 49.0;
        OES_INFO("Write EPC progress: {}%", static_cast<int>(progress_value));
    }

    // Sixth, Write the HDF5

    // Re-sort arrays, this time by output path, to collocate same-group datasets
    std::sort(
        array_infos.begin(), array_infos.end(),
        [](const Ary& lhs, const Ary& rhs) {
            return lhs.hdf_path_ < rhs.hdf_path_;
        }
    );

    H5::Group curr_grp;
    std::string curr_grp_dir;

    H5::FileAccPropList fapl;
    fapl.setLibverBounds(H5F_LIBVER_V18, H5F_LIBVER_V18);
    const auto& fcpl = H5::FileCreatPropList::DEFAULT;
    H5::H5File curr_h5(hdf_filename, H5F_ACC_TRUNC, fcpl, fapl);

    auto out_interval_dt = std::chrono::steady_clock::now();
    for (auto& ary : array_infos) {
        H5::H5Location* curr_loc = nullptr;

        std::string name;
        const std::string& path = ary.hdf_path_;

        const size_t stem_pos = path.find_last_of('/');
        if (stem_pos != std::string::npos) {
            auto grp_dir = path.substr(0, stem_pos);
            assert(!grp_dir.empty());

            if (curr_grp_dir != grp_dir) {
                std::vector<std::string> steps;
                boost::split(steps, path, boost::algorithm::is_any_of("/"));
                assert(steps.size() >= 2);
                name = std::move(steps.back());
                steps.pop_back();
                curr_grp = open_or_create_groups(curr_h5, steps);
                curr_grp_dir = grp_dir;
            } else {
                name = path.substr(stem_pos + 1);
            }
            curr_loc = &curr_grp;
        } else {
            curr_grp = {};
            curr_grp_dir.clear();
            curr_loc = &curr_h5;
            name = path;
        }

        H5::DataType data_type;
        switch (ary.meta_.m_transportArrayType) {
        case AryType::eArrayOfBoolean:  data_type = H5::PredType::NATIVE_INT8;   break;
        case AryType::eArrayOfInt:      data_type = H5::PredType::NATIVE_INT32;  break;
        case AryType::eArrayOfLong:     data_type = H5::PredType::NATIVE_INT64;  break;
        case AryType::eArrayOfFloat:    data_type = H5::PredType::NATIVE_FLOAT;  break;
        case AryType::eArrayOfDouble:   data_type = H5::PredType::NATIVE_DOUBLE; break;
        case AryType::eArrayOfString:   assert(false); break;
        case AryType::eBytes:           data_type = H5::PredType::NATIVE_UINT8;  break;
        } // no default on purpose

        const auto& dims = ary.meta_.m_dimensions;
        std::vector<hsize_t> hdims(dims.begin(), dims.end());

        H5::DSetCreatPropList plist;
        if ((options.hdf5_compression_ > 0 && options.hdf5_large_data_compression_ > 0)
            && hdims.size() > 0 && hdims[0] > 0
        ) {
            std::vector<hsize_t> chunking = hdims;
            get_chunk(chunking);
            plist.setChunk(int(chunking.size()), chunking.data());
            plist.setShuffle();
            plist.setDeflate(ary.data_ ? options.hdf5_compression_ : options.hdf5_large_data_compression_);
        }

        H5::DataSpace data_space;
        data_space.setExtentSimple(int(hdims.size()), hdims.data(), hdims.data());

        H5::DataSet dataset = curr_loc->createDataSet(name.c_str(), data_type, data_space, plist);

        if (ary.bytes_ == 0) {
            continue; // nothing to create (but dataset still created...)
        }

        if (ary.data_) {
            OES_ASSERT(aryTypeOf(ary.data_->m_item) == aryTypeOf(ary.meta_), continue;);

            std::vector<int8_t> bools;
            AryMemRef ary_mem = aryMemoryOf(ary.data_->m_item, bools);
            OES_ASSERT(ary_mem, continue;);
            dataset.write(ary_mem.data_, data_type, data_space, data_space);
            ary.data_.reset();
        } else {
            assert(ary.parts_);
            const AryDivision& div = ary.parts_->div_;
            const size_t sub_ary_count = div.size();
            assert(sub_ary_count == ary.parts_->subarys_.size());
            for (size_t i = 0; i < sub_ary_count; ++i) {
                AryAny& data = ary.parts_->subarys_[i];

                std::vector<int8_t> bools;
                AryMemRef ary_mem = aryMemoryOf(data.m_item, bools);
                OES_ASSERT(ary_mem, continue;);

                std::vector<hsize_t> starts, counts;
                size_t offset = div.initDataSubarraysType(i, starts, counts);
                OES_UNUSED(offset);

                H5::DataSpace mem_space;
                hsize_t slab_size = productOf(counts);
                mem_space.setExtentSimple(1, &slab_size, &slab_size);

                data_space.selectHyperslab(H5S_SELECT_SET, counts.data(), starts.data());

                dataset.write(ary_mem.data_, data_type, mem_space, data_space);

                ary.parts_->subarys_[i].m_item = {};
            }
            ary.parts_->subarys_.clear();
            ary.parts_->subarys_.shrink_to_fit();
        }
        dataset.close();

        progress_value += (97.0 - 49.0)/(static_cast<double>(array_infos.size()));
        if (out && (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - out_interval_dt).count() >= 2000) ) {
            out_interval_dt = std::chrono::steady_clock::now();
            OES_INFO("Write HDF progress: {}%", static_cast<int>(progress_value));
        }
    }

    endPhase("Write HDF");
    if (out) {
        progress_value = 99.0;
        OES_INFO("Write HDF progress: {}%", static_cast<int>(progress_value));
    }

    updateStats();

    if (msg) {
        const auto wall_time = stats.elapsed_sec_;
        const int time_width = (wall_time < 1.0)? 5:
            static_cast<int>(std::log10(int64_t(wall_time))) + 5;

        double prev_elapsed = 0.0;
        for (const auto& [name, elapsed, peak_ram] : phases) {
            const double sec = elapsed - prev_elapsed;
            OES_INFO("{:>24}: {:{}.3f}s {:5.1f}% ({} peak)",
                name, sec, time_width, (sec / wall_time * 100.), peak_ram
            );
            prev_elapsed = elapsed;
        }
        auto peak_ram = oes::core::System::getMemoryUsage().peakMB();
        OES_INFO("{:>24}: {:{}.3f}s        ({} peak)",
            "TOTAL", wall_time, time_width, peak_ram
        );
    }

    return true;
}

} // namespace oes::client

namespace {

using namespace oes::common;

std::string PhaseBase::toString(const std::string& label, double wall_time) const {
    const int time_width = (wall_time < 1.0)? 5:
        static_cast<int>(std::log10(int64_t(wall_time))) + 5;

    return fmt::format(
        "{:>24}: {:{}.3f}s {:5.1f}% (WAIT: {:{}.3f}s {:5.1f}%) {} peak\n",
        label,
        proc_, time_width, (proc_ / wall_time * 100.),
        wait_, time_width, (wait_ / wall_time * 100.),
        peak_ram_
    );
}

template <class Self, typename Value>
Phase<Self, Value>::Phase(
    SpscQueue<Value>& queue,
    size_t max_msg_size,
    std::function<bool()> health_check
) : queue_(queue),
    max_msg_size_(max_msg_size)
{
    health_check_ = health_check ? health_check : [](){return true;};
}

template <class Self, typename Value>
void Phase<Self, Value>::processAll() {
    HighResTimer timer;

    Self* self = static_cast<Self*>(this);

    try {
        if (!self->prolog()) {
            return finish(timer, ExitStatus::ePROLOG_ERROR);
        }

        for (;;) {
            const double before_dequeue = timer.elapsedSec();

            Value item;
            auto max_time = oes::core::getPartialTimeoutValue(max_msg_size_);
            const bool retry_logic = queue_.timeout_ > max_time;
            bool dequeue_ok = false;
            if (retry_logic) {
                auto remaining_time = queue_.timeout_;
                while (true) {
                    dequeue_ok = queue_.dequeue(item, remaining_time < max_time ? remaining_time : max_time); // blocking call!
                    if (dequeue_ok) {
                        break;
                    }

                    remaining_time -= max_time;
                    if (remaining_time.count() < 0) {
                        dequeue_ok = false;
                        break;
                    }

                    if (!health_check_()) {
                        throw ClientException("Connection in bad shape.", true);
                    }

                    if (max_time < std::chrono::milliseconds(20000)) {
                        max_time += std::chrono::milliseconds(2000);
                    }
    
                }
            } else {
                dequeue_ok = queue_.dequeue(item);
            }

            const double after_dequeue = timer.elapsedSec();
            wait_ += (after_dequeue - before_dequeue);

            if (!dequeue_ok) {
                return finish(timer, ExitStatus::eTIMEOUT);
            }

            if (self->finished(item)) {
                return finish(timer, ExitStatus::eDONE);
            }

            ++item_count_;
            const bool process_ok = self->processOne(item);

            proc_ += timer.elapsedSec() - after_dequeue;

            if (!process_ok) {
                return finish(timer, ExitStatus::ePROCESS_ERROR);
            }
        }
    } catch (const std::exception& ex) {
        error_ = ex.what();
    } catch (...) {
        error_ = "Unknown exception";
    }

    finish(timer, ExitStatus::eEXCEPTION);
}

template <class Self, typename Value>
void Phase<Self, Value>::finish(HighResTimer& timer, ExitStatus es) {
    status_ = es;
    total_ = timer.elapsedSec();
    peak_ram_ = oes::core::System::getMemoryUsage().peakMB();

    Self* self = static_cast<Self*>(this);
    self->epilog(es);
}

//============================================================================

ExportCtx::ExportCtx(
    const std::string& epc_filename,
    const std::string& hdf_filename,
    const ExportEpcOptions& opts,
    EpcStatistics* p_stats
) : epc_filename_(epc_filename),
    hdf_filename_(hdf_filename),
    opts_(opts),
    p_arg_stats_(p_stats),
    obj_queue_(opts.max_queue_depth_, opts.timeout_),
    ary_queue_(opts.max_queue_depth_, opts.timeout_)
{
    assert(!epc_filename_.empty());
    assert(!hdf_filename_.empty());
}

void ExportCtx::addResource(Resource& res) {
    auto& obj = objs_.emplace_back();
    obj.uri_ = EmlUri::parse(res.m_uri);
    obj.disco_uri_ = std::move(res.m_uri);
    OES_ASSERT(obj.uri_.type_ == EmlUri::eOBJ_INSTANCE, objs_.pop_back(); return;);
    auto obj_uid_unique = objmap_.emplace(TypedUid{obj.uri_.to_canonical_type(), obj.uri_.guid_}, objs_.size() - 1);
    OES_ASSERT(obj_uid_unique.second, objs_.pop_back(); return;);

    if (obj.isExtPartRef()) {
        ++epc_ext_part_ref_count_;
        hdf_proxies_.emplace(obj.uri_.guid_, objs_.size() - 1);
    }
}

Obj* ExportCtx::setObject(size_t idx, DataObject& etp_obj) {
    assert(idx < objs_.size());

    auto& obj = objs_[idx];
    OES_ASSERT(!obj.hasObj(), return nullptr;); // no duplicates
    OES_ASSERT(etp_obj.m_resource.m_uri == obj.disco_uri_, return nullptr;);

    obj.setObj(std::move(etp_obj));

    ++stats().object_count_;
    stats().object_bytes_ += obj.getObjData().size();

    return &obj;
}

Obj* ExportCtx::resolveObject(const TypedUid& uid) {
    auto guid_found = objmap_.find(uid);
    if (guid_found == objmap_.end()) {
        return nullptr;
    }
    Obj& target = objs_[guid_found->second];
    return &target;
}

Obj* ExportCtx::resolveHdfProxy(const Guid& guid) {
    auto guid_found = hdf_proxies_.find(guid);
    if (guid_found == hdf_proxies_.end()) {
        return nullptr;
    }
    Obj& target = objs_[guid_found->second];
    return &target;
}

void ExportCtx::openEpc() {
    epc_.openForWriting(epc_filename_);

    // Copy/paste props from XMLOPCProxyImpl::XMLOPCProxyImpl()?
    //epc_.getExtendedCoreProperty()["ResqmlSchemaDate"] = "20150817";
    epc_.getExtendedCoreProperty()["Emerson-EML"] = "1.0";
}

void ExportCtx::addRelsAndCloseEpc() {
    if (ext_parts_map_.empty()) {
        if (auto* os = opts_.out_) {
            *os << "Did not create any HDF5 file." << std::endl;
        }
    } else {
        // Even when we have several EpcExternalPartReference, we still create
        // a single HDF5 file on disk (datasets sharing a path are relocated).
        const fs::path hdf_path(hdf_filename_);
        for (auto& entry : ext_parts_map_) {
            ExtPartInfo& hdf = entry.second;
            hdf.path_ = hdf_path;

            epc::Relationship rel(false); // i.e. external
            rel.setType(RelType::eExternalResource);
            rel.setTarget(hdf.path_.filename().string());
            rel.setId("Hdf5File");

            hdf.part_->addRelationship(std::move(rel));
        }
    }

    // Write alls relations, rel and obj TOCs, etc...
    epc_.writePackage();
    epc_.close();
}

//============================================================================

ParseXmlPhase::ParseXmlPhase(
    ExportCtx& ctx,
    size_t max_msg_size,
    SpscQueue<Obj*>& queue
) : Base(queue, max_msg_size)
  , ctx_(ctx)
{
}

bool ParseXmlPhase::processOne(Obj* p_obj) {
    assert(p_obj);
    Obj& obj = *p_obj;
    assert(obj.uri_.isValid());

    OES_ASSERT(obj.hasObj(), return false;);

    // A copy, since the EPC part is written later, once all paths are planned
    auto [scan_data, rc] = FilePart::scanData(
        obj.getObjData(), /*FilePart::eCited|*/FilePart::eRefs|FilePart::eHdfs
    );

    if (rc != FilePart::eOK_DONE) {
        error_ = fmt::format(
            "Error: Cannot XML parse part {}\n",
            obj.disco_uri_
        );
        return false;
    }

    for (const auto& ref : scan_data.refs_) {
        auto ref_guid = Guid::parse(ref.second.Uuid_);
        OES_ASSERT(ref_guid.isValid(), continue;);
        Obj* p_target = ctx_.resolveObject(
            {EmlUri::canonicalType(ref.second.QualifiedType_), ref_guid}
        );
        OES_ASSERT(p_target, continue;);
        obj.addRel(*p_target);
        ++ctx_.stats().relation_count_;
    }

    auto& array_infos = ctx_.array_infos_;

    obj.has_datasets_ = !scan_data.hdfs_.empty();
    for (auto& hdf : scan_data.hdfs_) {
        auto hdf_guid = Guid::parse(hdf.second.HdfProxy_.Uuid_);
        OES_ASSERT(hdf_guid.isValid(), continue;);

        Obj* p_hdf_obj = ctx_.resolveHdfProxy(hdf_guid);
        OES_ASSERT(p_hdf_obj, continue;);
        Obj& hdf_obj = *p_hdf_obj;

        obj.addRel(
            hdf_obj,
            RelType::eMlToExternalPartProxy,
            RelType::eExternalPartProxyToMl
        );

        auto key_ok = ctx_.ary_keys_.emplace(
            HdfPathKey{
                containerOf(hdf.second.HdfProxy_, obj.disco_uri_),
                normalizeHdfPath(hdf.second.PathInHdfFile_)
            },
            array_infos.size()
        );
        if (!key_ok.second) {
            continue; // same array, already referenced by another object
        }

        auto& array_id = array_infos.emplace_back();
        array_id.container_ = key_ok.first->first.first;
        array_id.ident_.m_pathInResource = std::move(hdf.second.PathInHdfFile_);
        array_id.ident_.m_uri = hdf_obj.disco_uri_;
    }

    return true;
}

void ParseXmlPhase::epilog(ExitStatus) {
    if (auto* os = ctx_.opts_.out_) {
        OES_UNUSED(os);
        OES_INFO("XML references {} objects, and {} arrays.",
            thou(ctx_.stats().relation_count_), thou(ctx_.array_infos_.size()));
    }
}

//============================================================================

//! Writes all objects and their relations to the EPC, then closes it.
//! Runs concurrently with arrays fetching, once all XML is parsed.
void writeEpc(ExportCtx& ctx, PhaseBase& phase) {
    HighResTimer timer;
    try {
        ctx.openEpc();

        for (Obj& obj : ctx.objs_) {
            if (!obj.hasObj() || obj.skip_) {
                continue;
            }

            std::string xml;
            obj.moveObjData(xml);
            if (obj.has_datasets_ && !ctx.relocated_.empty()) {
                xml = relocateDatasets(xml, obj.disco_uri_, ctx.relocated_);
            }

            const std::string part_name = obj.epcPartName();
            obj.part_ = ctx.epc_.createPart(xml, part_name);
            if (!obj.part_) {
                throw std::runtime_error("Cannot create EPC part " + part_name);
            }

            if (obj.isExtPartRef()) {
                auto& ext_part = ctx.ext_parts_map_[&obj];
                assert(!ext_part.part_ || ext_part.part_ == obj.part_);
                ext_part.part_ = obj.part_;
            }

            ctx.epc_.addContentType(epc::ContentType(false, obj.epcMimeType(), part_name));
        }

        for (Obj& obj : ctx.objs_) {
            if (obj.part_) {
                obj.processRels(ctx);
            }
            obj.resetObj(); // eagerly free memory
        }

        ctx.addRelsAndCloseEpc();
        phase.status_ = PhaseBase::ExitStatus::eDONE;
    } catch (const std::exception& ex) {
        phase.error_ = ex.what();
        phase.status_ = PhaseBase::ExitStatus::eEXCEPTION;
    } catch (...) {
        phase.error_ = "Unknown exception";
        phase.status_ = PhaseBase::ExitStatus::eEXCEPTION;
    }
    phase.proc_ = phase.total_ = timer.elapsedSec();
    phase.peak_ram_ = oes::core::System::getMemoryUsage().peakMB();
}

//============================================================================

WriteHdfPhase::WriteHdfPhase(
    SpscQueue<AryAndDiv>& queue,
    size_t max_msg_size,
    const std::string& hdf_filename,
    AryVec& array_infos,
    int hdf5_compression,
    int hdf5_large_data_compression,
    std::function<bool()> health_check
) : Base(queue, max_msg_size, health_check),
    hdf_filename_(hdf_filename),
    array_infos_(array_infos),
    hdf5_compression_(hdf5_compression),
    hdf5_large_data_compression_(hdf5_large_data_compression)
{
}

bool WriteHdfPhase::prolog() {
    H5::FileAccPropList fapl;
    fapl.setLibverBounds(H5F_LIBVER_V18, H5F_LIBVER_V18);
    const auto& fcpl = H5::FileCreatPropList::DEFAULT;
    hdf_file_ = H5::H5File(hdf_filename_, H5F_ACC_TRUNC, fcpl, fapl);
    return true;
}

bool WriteHdfPhase::processOne(const AryAndDiv& idx) {
    Ary& ary = array_infos_[idx.ary_idx_];
    const bool has_part = (idx.div_idx_ != null_idx);
    const bool is_first_part = (idx.div_idx_ == 0);
    const bool is_empty = (ary.bytes_ == 0);

    if (has_part && (idx.ary_idx_ == prev_ary_idx_) && !is_first_part && !is_empty) {
        assert(prev_ds_);
        assert(ary.parts_);
        ary.parts_->write(*prev_ds_, idx.div_idx_);
        ary.parts_->subarys_[idx.div_idx_].m_item = {}; // free array's memory
        return true;
    }

    HdfDataset_uptr ds = ary.write(hdf_file_, hdf5_compression_, hdf5_large_data_compression_);

    if (is_empty) {
        return ds == nullptr; // we expect to be nullptr!
    }
    if (ds) {
        assert(is_first_part);
        prev_ds_ = std::move(ds);
        prev_ary_idx_ = idx.ary_idx_;

        assert(ary.parts_);
        ary.parts_->write(*prev_ds_, 0);
        ary.parts_->subarys_[0].m_item = {}; // free array's memory
    } else {
        assert(!has_part);
        assert(ary.data_);
        ary.data_.reset(); // free array's memory
    }

    return true;
}

//============================================================================

} // namespace

namespace oes::client {

bool exportEpcMt(
    Client& client,
    const std::string& etp_space,
    const std::string& epc_filename,
    const std::string& hdf_filename,
    const ExportEpcOptions& options,
    EpcStatistics* p_stats
) {
    ExportCtx ctx(epc_filename, hdf_filename, options, p_stats);

    auto endPhase = [&ctx](std::string phase) {
        ctx.endPhase(std::move(phase));
    };

    // close to what ProtocolCapabilityKind::eMaxDataItemCount is,
    // except the doc mentions only the GET-from-store use-case,
    // not the PUT-to-store one of this function
    const size_t max_batch_size = options.max_batch_size_;
    const bool overwrite = options.overwrite_;
    std::chrono::milliseconds timeout = options.timeout_;

    std::ostream* out = options.out_;
    std::ostream* msg = options.msg_;
    std::ostream* warn = options.warn_;
    std::ostream* err = options.err_;

    std::ostringstream oss;
    std::ostream& rpc_err = err? *err: oss;

#ifdef OES_DEBUG_BUILD
    auto ping_rpc = rpcFor(client.core_proto(), &proto12::ICore::Ping);

    auto ping_ok = ping_rpc.call(rpc_err, timeout);
    if (!ping_ok.first || ping_ok.second != std::future_status::ready) {
        OES_ERROR("Ping failed");
        return false;
    }
#endif //OES_DEBUG_BUILD

    if (out) {
        OES_INFO("Export Options: "
                 "overwrite = {}; "
                 "jobs = {}; "
                 "hdf5_comp = {}; "
                 "hdf5_large_comp = {}; "
                 "timeout = {} [s]; "
                 "max_msg_size (negociated) = {}; "
                 "transaction_retries_ = {}; ",
                 options.overwrite_,
                 options.thread_count_,
                 options.hdf5_compression_,
                 options.hdf5_large_data_compression_,
                 std::chrono::duration_cast<std::chrono::seconds>(options.timeout_).count(),
                 client.negotiated_max_msg_size(),
                 options.n_transaction_retries_);
    }

    double progress_value{1.0};

    auto& mutable_stats = ctx.stats();
    const auto& stats = mutable_stats;
    auto updateStats = [&ctx]() {
        ctx.updateStats();
    };

    // Check output files, before anything
    const fs::path epc_path(epc_filename);
    fs::file_status epc_stat = fs::status(epc_path);
    if (fs::is_regular_file(epc_stat)) {
        if (!overwrite) {
            OES_ERROR("File exists: {}", epc_filename);
            return false;
        }
    } else if (fs::exists(epc_stat)) {
        OES_ERROR("Non-file exists: {}", epc_filename);
        return false;
    }

    const fs::path hdf_path(hdf_filename);
    fs::file_status hdf_stat = fs::status(hdf_path);
    if (fs::is_regular_file(hdf_stat)) {
        if (!overwrite) {
            OES_ERROR("File exists: {}", hdf_filename);
            return false;
        }
    } else if (fs::exists(hdf_stat)) {
        OES_ERROR("Non-file exists: {}\n", hdf_filename);
        return false;
    }

    const std::string space_uri_txt = getDataspaceUri(client, etp_space, timeout);
    const EmlUri space_uri = EmlUri::parse(space_uri_txt);
    const std::string message = "Export ETP Dataspace " + space_uri_txt;

    ScopedTransaction tx(client, timeout);
    if (!tx.supported()) {
        if (warn) {
            OES_WARNING("No Transaction support. Not read-consistent.");
        }
    } else if (!tx.start(space_uri_txt, message, /*read-only*/true, nullptr, options.n_transaction_retries_)) {
        if (warn) {
            OES_WARNING("Could not start Transaction. Not read-consistent.");
        }
    }
    auto commitTx = [err, &tx]() {
        std::string failure_reason;
        const bool tx_ok = tx.commit(&failure_reason);
        if (!tx_ok && err) {
            OES_ERROR("Cannot end read-only transaction: {}", failure_reason);
        }
        return tx_ok;
    };

    endPhase("Prolog");
    if (out) {
        OES_INFO("Prolog progress: {}%", static_cast<int>(progress_value));
    }

    // First, figure out what's in that space, i.e. the TOC
    // We keep only the object's URI, throwing away the Resource we got,
    // since we'll fetch the DataObject next, which also embeds a Resource.
    {
        auto* proto = client.disco_proto();
        if (!proto) {
            if (err) {
                OES_ERROR("Missing required protocol: {}", Protocol::IDiscovery::protocolName());
            }
            return false;
        }

        auto rpc = rpcFor(proto, &proto12::IDiscovery::GetResources);
        rpc.req_.m_context.m_uri = space_uri_txt;
        rpc.req_.m_context.m_depth = 1; // required by PV's server... See iLab#3
        rpc.req_.m_context.m_dataObjectTypes.clear();
        rpc.req_.m_storeLastWriteFilter.reset();
        if (out) {
            OES_INFO("Discovering {}...", rpc.req_.m_context.m_uri);
        }

        if (!rpc.callForEachValue(
            rpc_err, timeout,
            [&](auto& res) {
                ctx.addResource(res);
            }
        )) {
            return false;
        }
    }

    // Warning: Unlike in Single-Threaded's (ST) case, we do not sort by URI.
    // This means the order we process things in, is non-deterministic, and
    // depends on the order the server uses (which could be arbitrary).

    ObjVec& objs = ctx.objs_;
    if (objs.empty()) {
        if (out) {
            OES_WARNING("Empty dataspace.");
        }
        updateStats();
        return true;
    }

    if (out) {
        OES_INFO(
            "Got {} resources ({} EpcExternalPartReference).", thou(objs.size()), ctx.epc_ext_part_ref_count_
        );
    }

    endPhase("Discovery");
    if (out) {
        progress_value = 3.0;
        OES_INFO("Discovery progress: {}%", static_cast<int>(progress_value));
    }

    ParseXmlPhase xml_phase(ctx, client.negotiated_max_msg_size(), ctx.obj_queue_);
    PhaseBase epc_writer;

    oes::core::ThreadJoiner thread_joiner(options.thread_count_);
    thread_joiner.threads_.emplace_back(std::ref(xml_phase));

    // Second, fetch the objects themselves, respecting the "batching" arg.

    // Note that now that OpenKV properly splits oversize responses, we could
    // maybe send "mega" requests, which the server would need to split. But
    // for that to be efficient (especially time-to-first-response, TTFR), that
    // implies the server itself does some form of batching/"streaming",
    // instead of waiting for all rows, and then only sending/splitting that.
    size_t obj_errors = 0;
    {
        auto* proto = client.store_proto();
        if (!proto) {
            if (err) {
                OES_ERROR("Missing required protocol: {}", Protocol::IStore::protocolName());
            }
            return false;
        }

        std::vector<std::map<std::string, std::string>> chunks;
        chunks.reserve((objs.size() + max_batch_size - 1) / max_batch_size);
        auto* curr_chunk = &chunks.emplace_back();
        for (const auto& obj : objs) {
            if (curr_chunk->size() == max_batch_size) {
                curr_chunk = &chunks.emplace_back();
            }
            const size_t key = (chunks.size() - 1) * max_batch_size + curr_chunk->size();
            curr_chunk->emplace(std::to_string(key), obj.disco_uri_);
        }

        auto rpc = rpcFor(proto, &proto12::IStore::GetDataObjects);
        rpc.req_.m_format = "xml";

        const size_t max_size = objs.size();

        for (auto& chunk : chunks) {
            rpc.req_.m_uris = chunk;

            if (!rpc.callForEachValue(
                rpc_err, timeout,
                [&](auto& entry) {
                    auto [idx, obj_idx_ok] = toIndex(entry.first, max_size);
                    OES_ASSERT(obj_idx_ok, ++obj_errors; return;);

                    auto* p_obj = ctx.setObject(idx, entry.second);
                    OES_ASSERT(p_obj, ++obj_errors; return;);
                    auto& obj = *p_obj;


                    // Filter OnGoing activities if any
                    bool skip = false ;
                    if (obj.type() == "obj_Activity") {
                        // We need to parse the xml extraMetadata to check the activity status.
                        std::string xml(obj.getObjData()); // makes a copy
                        auto [scan_data, rc] = FilePart::scanData(
                            xml, FilePart::eXData
                        );
                        for( auto& [name, value]: scan_data.xdata_) {
                            if( name == "activityStatus" && value == "OnGoing") {
                                OES_INFO("Skipping OnGoing activity {}({})", obj.type(), obj.uri_.guid_.toXmlString());
                                skip = true ;
                                break;
                            }
                        }
                    }
                    if( !skip ) {
                        const bool obj_enqueued = ctx.obj_queue_.enqueue(&obj);
                        OES_ASSERT(obj_enqueued, throw std::runtime_error("Cannot enqueue object for XML parsing"););
                    } else {
                        obj.skip_ = true;
                    }
                }
            )) {
                return false;
            }
        }
    }

    if (!ctx.obj_queue_.enqueue(nullptr)) {
        return false;
    }

    assert(stats.object_count_ == (objs.size() - obj_errors));

    if (out) {
        OES_INFO("Got {} objects ({} XML bytes).", thou(stats.object_count_), thou(stats.object_bytes_));
    }

    // Waits on the XML thread, before starting arrays
    thread_joiner.join_all();
    if (xml_phase.status_ != PhaseBase::ExitStatus::eDONE) {
        OES_ERROR("XML parsing failed: {}", xml_phase.error_);
        return false;
    }

    // All dataset paths are known now, so the EPC can be written (concurrently with arrays)
    ctx.relocated_ = planArrayPaths(ctx.array_infos_, warn);
    thread_joiner.threads_.emplace_back([&ctx, &epc_writer]() {
        writeEpc(ctx, epc_writer);
    });

    endPhase("Get+Parse Objects");
    if (out) {
        progress_value = 7;
        OES_INFO("Get+Parse Objects progress: {}%", static_cast<int>(progress_value));
    }

    // Fourth, fetch info / meta-data about those arrays, respecting the "batching" arg,
    // so we can group or split fetches to respect max-msg-size
    AryVec& array_infos = ctx.array_infos_;

    if (!array_infos.empty()) {
        auto* proto = client.array_proto();
        if (!proto) {
            if (err) {
                OES_ERROR("Missing required protocol: {}", Protocol::IDataArray::protocolName());
            }
            return false;
        }

        using AryIdentMap = std::map<std::string, AryIdent>;
        std::vector<AryIdentMap> chunks;
        chunks.reserve((array_infos.size() + max_batch_size - 1) / max_batch_size);
        auto* curr_chunk = &chunks.emplace_back();
        const size_t max_size = array_infos.size();
        for (const auto& info : array_infos) {
            if (curr_chunk->size() == max_batch_size) {
                curr_chunk = &chunks.emplace_back();
            }
            const size_t key = (chunks.size() - 1) * max_batch_size + curr_chunk->size();
            curr_chunk->emplace(std::to_string(key), info.ident_);
        }
        mutable_stats.array_chunks_ += chunks.size();

        auto rpc = rpcFor(proto, &proto12::IDataArray::GetDataArrayMetadata);

        size_t meta_ok = 0;
        for (auto& chunk : chunks) {
            rpc.req_.m_dataArrays = chunk;

            if (!rpc.callForEachValue(
                rpc_err, timeout,
                [&](auto& entry) {
                    auto [idx, idx_ok] = toIndex(entry.first, max_size);
                    OES_ASSERT(idx_ok, return;);

                    auto& info = array_infos[idx];
                    OES_ASSERT(info.bytes_ == 0, return;); // i.e. not init yet
                    info.meta_ = entry.second;
                    info.bytes_ = byteSizeOf(info.meta_);
                    mutable_stats.array_bytes_ += info.bytes_;
                    ++mutable_stats.array_count_;
                    ++meta_ok;
                }
            )) {
                return false;
            }
        }
        OES_ASSERT(array_infos.size() == meta_ok, ;);
    }

    if (!array_infos.empty()) {
        // Sort arrays, so we can group them in batches
        std::sort(
            array_infos.begin(), array_infos.end(),
            [](const Ary& lhs, const Ary& rhs) {
                return lhs.bytes_ < rhs.bytes_;
            }
        );

        if (out) {
            OES_INFO(
                "{} array bytes (largest {} bytes).",thou(stats.array_bytes_), thou(array_infos.back().bytes_)
            );
        }
    }

    endPhase("Get Array Meta");
    if (out) {
        progress_value = 14.0;
        OES_INFO("Get Array Meta progress: {}%", static_cast<int>(progress_value));
    }

    // Take 10% off the max-msg-size, to leave wiggle room for Avro serialization overhead.
    const size_t max_byte_size = size_t(client.negotiated_max_msg_size() * 0.9);

    SpscQueue<AryAndDiv> ary_queue(options.max_queue_depth_, timeout);
    WriteHdfPhase hdf_phase(
        ary_queue, client.negotiated_max_msg_size(), hdf_filename, array_infos,
        options.hdf5_compression_, options.hdf5_large_data_compression_,
        [&client](){return client.isConnOpen();}
    );
    thread_joiner.threads_.emplace_back(std::ref(hdf_phase));

    // Fifth, fetch actual array data
    AryVec arrays;
    if (!array_infos.empty()) {
        auto* proto = client.array_proto();
        assert(proto); // already tested above

        auto rpc = rpcFor(proto, &proto12::IDataArray::GetDataArrays);
        auto& req_arys = rpc.req_.m_dataArrays;
        auto& res_arys = rpc.res_.m_dataArrays;

        const size_t max_size = array_infos.size();
        auto processArray = [&](auto& entry) {
            auto [idx, idx_ok] = toIndex(entry.first, max_size);
            OES_ASSERT(idx_ok, return;);

            auto& info = array_infos[idx];
            OES_ASSERT(info.meta_.m_transportArrayType == aryTypeOf(entry.second), ;);
            OES_ASSERT(!info.data_, return;); // i.e. no array data yet
            info.data_ = std::move(entry.second.m_data);

            const bool ary_enqueued = ary_queue.enqueue(AryAndDiv{ idx, null_idx });
            OES_ASSERT(ary_enqueued, throw std::runtime_error("Cannot enqueue array"););
        };

        size_t cur_byte_size = 0;
        auto iter = array_infos.begin();
        const int width = 1 + static_cast<int>(std::log10(max_size));

        size_t idx = 0;
        for (; iter != array_infos.end(); ++iter, ++idx) {
            const Ary& ary = *iter;
            if (ary.bytes_ == 0) {
                const bool ary_enqueued = ary_queue.enqueue(AryAndDiv{idx, null_idx});
                OES_ASSERT(ary_enqueued, throw std::runtime_error("Cannot enqueue array"););
                continue; // nothing to fetch, empty (yes, can happen)
            }
            if (ary.bytes_ > max_byte_size) {
                break; // no grouping possible anymore
            }

            const bool full_by_size = (cur_byte_size + ary.bytes_) > max_byte_size;
            const bool full_by_count = (req_arys.size() + 1) >= max_batch_size;
            if (full_by_size || full_by_count) {
                if (!rpc.callForEachValue(
                    rpc_err, timeout,
                    [&processArray](auto& entry) {
                        processArray(entry);
                    }
                )) {
                    return false;
                }

                // FIXME: No equivalent to this in rpc.callForEachValue()
                //OES_ASSERT(req_arys.size() == res_arys.size(), ;); // i.e. no errors

                // reset for next batch
                cur_byte_size = 0;
                req_arys.clear();
                res_arys.clear();
            }

            cur_byte_size += ary.bytes_;
            auto rc = req_arys.emplace(fmt::format("{:0{}}", idx, width), ary.ident_);
            OES_RELEASE_UNUSED(rc);
            assert(rc.second);
        }

        // process last batch of "small" arrays, if any
        if (!req_arys.empty()) {
            if (!rpc.callForEachValue(
                rpc_err, timeout,
                [&processArray](auto& entry) {
                    processArray(entry);
                }
            )) {
                return false;
            }
        }
        
        if (out) {
            progress_value = 21.0;
            OES_INFO("Get+Write Small Array Data progress: {}%", static_cast<int>(progress_value));
        }

        size_t current_left_size = array_infos.size() - idx;
        if (current_left_size == 0) {
            current_left_size = 1;
        }

        // Now processing arrays which exceed max_byte_size
        for (; iter != array_infos.end(); ++iter, ++idx) {
            Ary& ary = *iter;

            // Prepare the resquest, assigning the constant parts
            auto sub_rpc = rpcFor(proto, &proto12::IDataArray::GetDataSubarrays);
            auto rc = sub_rpc.req_.m_dataSubarrays.emplace();
            assert(rc.second);
            auto& sub_ary = rc.first->second;
            sub_ary.m_uid = ary.ident_;

            ary.parts_ = std::make_unique<AryParts>();

            size_t esize = byteSizeOf(ary.meta_.m_transportArrayType);

            // "fixup" esize, to account for worse-case scenario of the
            // Avro varint encoding actually increasing the size of the subarray.
            // E.g. Geolog uses MAX_INT as NdV, and some logs are full of NdVs.
            // So we must be conversative to avoid for example that error:
            // DataArray(9).GetDataSubarrays(3) failed: ETP#8: Could not encode+send
            //     #1537(1530) DataArray(9).GetDataSubarraysResponse(8) F
            //     (-M 1000000): Oversize #0 (key: ) of 1: EOF reached
            switch (ary.meta_.m_transportArrayType) {
            case AryType::eArrayOfInt:  esize =  5; break;
            case AryType::eArrayOfLong: esize = 10; break;
            default: break;
            }

            auto& div = ary.parts_->div_ = AryDivision::make(
                ary.meta_.m_dimensions, esize, max_byte_size
            );
            assert(!div.empty());
            const size_t sub_ary_count = div.size();
            ary.parts_->subarys_.reserve(sub_ary_count);
            for (size_t i = 0; i < sub_ary_count; ++i) {
                size_t offset = div.initDataSubarraysType(i, sub_ary.m_starts, sub_ary.m_counts);
                OES_UNUSED(offset);

                // No rpc.callForEachValue() necessary, cannot split single-part message
                auto recv_max_time = oes::core::getPartialTimeoutValue(max_byte_size);
                const bool retry_logic = timeout > recv_max_time;
                if (retry_logic) {
                    auto remaining_time = timeout;
                    while (true) {
                        auto ok = sub_rpc.call(rpc_err, remaining_time < recv_max_time ? remaining_time : recv_max_time);
                        if (ok.first) {
                            break;
                        }

                        remaining_time -= recv_max_time;
                        if (remaining_time.count() < 0) {
                            OES_ERROR("Client timeout.");
                            return false;
                        }

                        if (!client.isConnOpen()) {
                            OES_ERROR("Client connection in bad shape!");
                            throw ClientException("Connection in bad shape.", true);
                        }
                        rpc_err.clear();

                        if (ok.second == std::future_status::timeout) {
                            OES_INFO("Partial timeout. Retrying...");

                            if (recv_max_time < std::chrono::milliseconds(20000)) {
                                recv_max_time += std::chrono::milliseconds(2000);
                            }
                        }

                    }

                } else {
                    auto ok = sub_rpc.call(rpc_err, timeout);
                    if (!ok.first) {
                        return false;
                    }
                }

                auto& subarys = sub_rpc.res_.m_dataSubarrays;
                OES_ASSERT(subarys.size() == 1, return false;);
                ary.parts_->subarys_.emplace_back(std::move(subarys.begin()->second.m_data));

                if (!ary_queue.enqueue(AryAndDiv{ idx, i })) {
                    return false;
                }

            }
            if (out) {
                progress_value += (93.0 - 21.0)/static_cast<double>(current_left_size);
                OES_INFO("Get+Write Large Array Data progress: {}%", static_cast<int>(progress_value));
            }
        }

        if (!ary_queue.enqueue(AryAndDiv{})) {
            return false;
        }
    }

    // Waits on HDF and EPC threads
    thread_joiner.join_all();
    if (epc_writer.status_ != PhaseBase::ExitStatus::eDONE) {
        OES_ERROR("Cannot write EPC {}: {}", epc_filename, epc_writer.error_);
        return false;
    }
    endPhase("Get+Write Array Data");
    if (out) {
        progress_value = 97.0;
        OES_INFO("Get+Write Array Data progress: {}%", static_cast<int>(progress_value));
    }

    const bool commit_ok = commitTx();
    OES_ASSERT(commit_ok, ;); // should never happen (and warning already printed)

    endPhase("Commit");
    if (out) {
        progress_value = 99.0;
        OES_INFO("Commit progress: {}%", static_cast<int>(progress_value));
    }

    updateStats();

    if (msg) {
        const auto wall_time = stats.elapsed_sec_;
        const int time_width = (wall_time < 1.0)? 5:
            static_cast<int>(std::log10(int64_t(wall_time))) + 5;

        double prev_elapsed = 0.0;
        for (const auto& [name, elapsed, peak_ram] : ctx.phases_) {
            const double sec = elapsed - prev_elapsed;
            OES_INFO("{:>24}: {:{}.3f}s {:5.1f}% ({} peak)",
                name, sec, time_width, (sec / wall_time * 100.), peak_ram
            );
            prev_elapsed = elapsed;
        }

        auto peak_ram = oes::core::System::getMemoryUsage().peakMB();
        OES_INFO("{:>24}: {:{}.3f}s        ({} peak)\n",
            "TOTAL", wall_time, time_width, peak_ram
        );

        // Writes are async, so cannot be computed like the others
        if (epc_writer.total_) {
            auto s = epc_writer.toString("Async Write EPC", wall_time);
            msg->write(s.data(), s.size());
        }
        if (xml_phase.total_) {
            auto s = xml_phase.toString("Async Parse XML", wall_time);
            msg->write(s.data(), s.size());
        }
        if (hdf_phase.total_) {
            auto s = hdf_phase.toString("Async Write HDF", wall_time);
            msg->write(s.data(), s.size());
        }
    }

    return true;
}

} // namespace oes::client
