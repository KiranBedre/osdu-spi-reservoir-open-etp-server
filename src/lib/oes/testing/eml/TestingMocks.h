#pragma once
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

#include "export.h"

#include <oes/common/epc/FilePart.h>
#include <oes/common/etp12/Messages.h>
#include <oes/eml/etp12/Extensions.h>
#include <oes/core/utils/Guid.h>

#include <string>
#include <vector>

namespace oes::testing::eml {

using namespace oes::common;
namespace etpc = oes::common::etp12;

//============================================================================

struct OES_TESTING_EML_EXPORT EmlAryMock {
    std::string path_;
    std::vector<int64_t> dims_;
    etpc::Datatypes::AnyArrayType type_;

    mutable etpc::Datatypes::AnyArray_item_v data_cached_;
    std::function<void(etpc::Datatypes::AnyArray_item_v&)> init_;

    size_t rank() const;
    int64_t size() const;

    etpc::Datatypes::AnyArray_item_v& data();
    const etpc::Datatypes::AnyArray_item_v& data() const;

    bool equals(const EmlAryMock& rhs, bool ignore_data) const;
    bool equals(
        const std::string& path,
        const etpc::Datatypes::DataArrayTypes::DataArray& rhs
    ) const;

    static EmlAryMock from(
        std::string path,
        const etpc::Datatypes::DataArrayTypes::DataArray&,
        bool copy_data
    );
    static EmlAryMock from(
        const etpc::Datatypes::DataArrayTypes::PutDataArraysType&,
        bool copy_data
    );
    static EmlAryMock from(
        const etpc::Datatypes::DataArrayTypes::DataArrayIdentifier& ident,
        const etpc::Datatypes::DataArrayTypes::DataArrayMetadata& meta
    );

    static EmlAryMock from(
        const etpc::Datatypes::DataArrayTypes::PutUninitializedDataArrayType& ary
    ) {
        return from(ary.m_uid, ary.m_metadata);
    }

    static EmlAryMock from(const etp12::AryInfo& ary) {
        return from(ary.ident_, ary.meta_);
    }

    static bool is_null_array(
        const etpc::Datatypes::AnyArray_item_v&
    );
    static etpc::Datatypes::AnyArrayType type_of(
        const etpc::Datatypes::AnyArray_item_v&
    );
    static bool equals(
        const etpc::Datatypes::AnyArray_item_v& lhs,
        const etpc::Datatypes::AnyArray_item_v& rhs
    );
};

inline bool operator==(const EmlAryMock& lhs, const EmlAryMock& rhs) {
    return lhs.equals(rhs, false);
}

inline bool operator==(
    const EmlAryMock& lhs,
    const etpc::Datatypes::DataArrayTypes::PutDataArraysType& rhs
) {
    EmlAryMock rhs_mock = EmlAryMock::from(rhs, true);
    return lhs.equals(rhs_mock, false);
}

inline bool operator==(
    const EmlAryMock& lhs,
    const etpc::Datatypes::DataArrayTypes::PutUninitializedDataArrayType& rhs
) {
    EmlAryMock rhs_mock = EmlAryMock::from(rhs);
    return lhs.equals(rhs_mock, true);
}

inline bool operator==(const EmlAryMock& lhs, const etp12::AryInfo& rhs) {
    EmlAryMock rhs_mock = EmlAryMock::from(rhs);
    return lhs.equals(rhs_mock, true);
}

//============================================================================

struct OES_TESTING_EML_EXPORT EmlObjMock {
    std::string name_;
    std::string type_;
    oes::core::Guid guid_;

    std::string xml_;
    std::vector<EmlAryMock> arrays_;

    static std::string erase_tag(
        std::string_view ns_prefix_and_name, std::string& xml
    );
};

//============================================================================

const std::string xmlns_eml   ("http://www.energistics.org/energyml/data/commonv2");
const std::string xmlns_resqml("http://www.energistics.org/energyml/data/resqmlv2");

//============================================================================

OES_TESTING_EML_EXPORT
epc::FilePart::ScanData scanXml(std::string xml, int flags = 0xFF);

OES_TESTING_EML_EXPORT
epc::xml::AbstractCitedDataObject toCited(std::string xml);

//============================================================================

OES_TESTING_EML_EXPORT
const EmlObjMock& epc_ext_mock();   // eml2:EpcExternalPartReference

OES_TESTING_EML_EXPORT
const EmlObjMock& crs_mock();       // resqml2:LocalDepth3dCrs

OES_TESTING_EML_EXPORT
const EmlObjMock& datum_mock();     // resqml2:MdDatum

OES_TESTING_EML_EXPORT
const EmlObjMock& grid2d_mock();    // resqml2:Grid2dRepresentation

OES_TESTING_EML_EXPORT
const EmlObjMock& traj_mock();      // resqml2:WellboreTrajectoryRepresentation

//! RESQML 2.2 property with one array, whose <URI> is a file name as in an EPC
OES_TESTING_EML_EXPORT
EmlObjMock prop22_mock(const oes::core::Guid& uid);

//============================================================================

} // namespace oes::testing::eml
