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

#include "TestingMocks.h"

#include <boost/test/unit_test.hpp>

namespace oes::testing::eml {

using oes::core::Guid;

//============================================================================

std::string EmlObjMock::erase_tag(std::string_view ns_prefix_and_name, std::string& xml) {
    std::string_view tag = ns_prefix_and_name;
    const size_t len = tag.length();

    const size_t tag_beg = xml.find(tag);
    BOOST_REQUIRE(tag_beg != std::string::npos);
    BOOST_REQUIRE(xml[tag_beg - 1] == '<');

    const size_t tag_end = xml.find(tag, tag_beg + len);
    BOOST_REQUIRE(tag_end != std::string::npos);
    BOOST_REQUIRE(xml[tag_end - 1] == '/');
    BOOST_REQUIRE(xml[tag_end + len] == '>');

    const size_t tag_nxt = xml.find(tag, tag_end + len);
    BOOST_REQUIRE(tag_nxt == std::string::npos);

    std::string erased = xml.substr(tag_beg - 1, tag_end + len + 2 - tag_beg);
    xml.erase(tag_beg - 1, tag_end + len + 2 - tag_beg);
    return erased;
}

//============================================================================

size_t EmlAryMock::rank() const {
    return dims_.size();
}

int64_t EmlAryMock::size() const {
    const size_t r = rank();
    int64_t size = dims_.empty()? 0: dims_[0];
    for (size_t i = 1; i < r; ++i) {
        size *= dims_[i];
    }
    return size;
}

etpc::Datatypes::AnyArray_item_v& EmlAryMock::data() {
    if (is_null_array(data_cached_) && init_) {
        init_(data_cached_);
    }
    return data_cached_;
}

const etpc::Datatypes::AnyArray_item_v& EmlAryMock::data() const {
    if (is_null_array(data_cached_) && init_) {
        init_(data_cached_);
    }
    return data_cached_;
}

bool EmlAryMock::equals(const EmlAryMock& rhs, bool ignore_data) const {
    if (std::tie(path_, dims_, type_) != std::tie(rhs.path_, rhs.dims_, rhs.type_)) {
        return false;
    }

    if (ignore_data) {
        return true;
    }
    return equals(data(), rhs.data());
}

bool EmlAryMock::equals(
    const std::string& path,
    const etpc::Datatypes::DataArrayTypes::DataArray& rhs
) const {
    auto rhs_type = type_of(rhs.m_data.m_item);
    if (std::tie(path_, dims_, type_) != std::tie(path, rhs.m_dimensions, rhs_type)) {
        return false;
    }
    return equals(data(), rhs.m_data.m_item);
}

EmlAryMock EmlAryMock::from(
    std::string path,
    const etpc::Datatypes::DataArrayTypes::DataArray& ary, bool copy_data
) {
    EmlAryMock mock;
    mock.path_ = std::move(path);
    mock.dims_ = ary.m_dimensions;
    mock.type_ = type_of(ary.m_data.m_item);

    if (copy_data) {
        mock.data_cached_ = ary.m_data.m_item;
        mock.init_ = nullptr;
    }

    return mock;
}

EmlAryMock EmlAryMock::from(
    const etpc::Datatypes::DataArrayTypes::PutDataArraysType& ary, bool copy_data
) {
    using namespace etpc::Datatypes;

    EmlAryMock mock;
    mock.path_ = ary.m_uid.m_pathInResource;
    mock.dims_ = ary.m_array.m_dimensions;
    mock.type_ = type_of(ary.m_array.m_data.m_item);

    if (copy_data) {
        mock.data_cached_ = ary.m_array.m_data.m_item;
        mock.init_ = nullptr;
    }

    return mock;
}

EmlAryMock EmlAryMock::from(
    const etpc::Datatypes::DataArrayTypes::DataArrayIdentifier& ident,
    const etpc::Datatypes::DataArrayTypes::DataArrayMetadata& meta
) {
    EmlAryMock mock;
    mock.path_ = ident.m_pathInResource;
    mock.dims_ = meta.m_dimensions;
    mock.type_ = meta.m_transportArrayType;
    // has no data
    return mock;
}

//============================================================================

bool EmlAryMock::is_null_array(const etpc::Datatypes::AnyArray_item_v& ary) {
    if (ary.index() != size_t(etpc::Datatypes::AnyArray_item_e::eArrayOfBoolean)) {
        return false;
    }
    try {
        const auto& item = std::get<etpc::Datatypes::ArrayOfBoolean>(ary);
        return item.m_values.empty();
    } catch (...) {
        return true;
    }
}

etpc::Datatypes::AnyArrayType EmlAryMock::type_of(const etpc::Datatypes::AnyArray_item_v& ary) {
    using etpc::Datatypes::AnyArray_item_e;
    using etpc::Datatypes::AnyArrayType;
    switch (ary.index()) {
    case size_t(AnyArray_item_e::eArrayOfBoolean): return AnyArrayType::eArrayOfBoolean;
    case size_t(AnyArray_item_e::eArrayOfInt):     return AnyArrayType::eArrayOfInt;
    case size_t(AnyArray_item_e::eArrayOfLong):    return AnyArrayType::eArrayOfLong;
    case size_t(AnyArray_item_e::eArrayOfFloat):   return AnyArrayType::eArrayOfFloat;
    case size_t(AnyArray_item_e::eArrayOfDouble):  return AnyArrayType::eArrayOfDouble;
    case size_t(AnyArray_item_e::eArrayOfString):  return AnyArrayType::eArrayOfString;
    case size_t(AnyArray_item_e::eBytes):          return AnyArrayType::eBytes;
    default: throw "invalid AnyArray type";
    }
}

bool EmlAryMock::equals(
    const etpc::Datatypes::AnyArray_item_v& lhs,
    const etpc::Datatypes::AnyArray_item_v& rhs
) {
    if (lhs.index() != rhs.index()) {
        return false; // not normal. type_ and .idx() disagree
    }

    using namespace etpc::Datatypes;
    switch (lhs.index()) {
    case size_t(AnyArray_item_e::eArrayOfBoolean): return
        std::get<etpc::Datatypes::ArrayOfBoolean>(lhs).m_values ==
        std::get<etpc::Datatypes::ArrayOfBoolean>(rhs).m_values;

    case size_t(AnyArray_item_e::eArrayOfInt): return
        std::get<etpc::Datatypes::ArrayOfInt>(lhs).m_values ==
        std::get<etpc::Datatypes::ArrayOfInt>(rhs).m_values;

    case size_t(AnyArray_item_e::eArrayOfLong): return
        std::get<etpc::Datatypes::ArrayOfLong>(lhs).m_values ==
        std::get<etpc::Datatypes::ArrayOfLong>(rhs).m_values;

    case size_t(AnyArray_item_e::eArrayOfFloat): return
        std::get<etpc::Datatypes::ArrayOfFloat>(lhs).m_values ==
        std::get<etpc::Datatypes::ArrayOfFloat>(rhs).m_values;

    case size_t(AnyArray_item_e::eArrayOfDouble): return
        std::get<etpc::Datatypes::ArrayOfDouble>(lhs).m_values ==
        std::get<etpc::Datatypes::ArrayOfDouble>(rhs).m_values;

    case size_t(AnyArray_item_e::eArrayOfString): return
        std::get<etpc::Datatypes::ArrayOfString>(lhs).m_values ==
        std::get<etpc::Datatypes::ArrayOfString>(rhs).m_values;

    case size_t(AnyArray_item_e::eBytes): return
        std::get<std::vector<uint8_t>>(lhs) ==
        std::get<std::vector<uint8_t>>(rhs);

    default: assert(false);
    }

    return false;
}

//============================================================================

epc::FilePart::ScanData scanXml(std::string xml, int flags) {
    using epc::FilePart;
    auto ret = FilePart::scanData(std::move(xml), flags);
    BOOST_CHECK_EQUAL(ret.second, FilePart::eOK_DONE);
    return ret.first;
}

epc::xml::AbstractCitedDataObject toCited(std::string xml) {
    return scanXml(std::move(xml), epc::FilePart::eCited).cited_;
}

//============================================================================

const EmlObjMock& epc_ext_mock() {
    static const EmlObjMock mock__{
        "Hdf5File",
        "obj_EpcExternalPartReference",
        Guid::parse("e5e5230e-ecdd-4605-85ad-446926eeb47f"),
        R"xml(<?xml version="1.0" encoding="UTF-8"?>
<eml:EpcExternalPartReference
    xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"
    xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xmlns:xsd="http://www.w3.org/2001/XMLSchema"
    xmlns:gts="http://www.isotc211.org/2005/gts"
    xmlns:gsr="http://www.isotc211.org/2005/gsr"
    xmlns:eml="http://www.energistics.org/energyml/data/commonv2"
    xmlns:gml="http://www.opengis.net/gml/3.2"
    xmlns:xlink="http://www.w3.org/1999/xlink"
    xmlns:gmd="http://www.isotc211.org/2005/gmd"
    xmlns:gco="http://www.isotc211.org/2005/gco"
    xmlns:resqml="http://www.energistics.org/energyml/data/resqmlv2"
    xsi:type="eml:obj_EpcExternalPartReference"
    uuid="e5e5230e-ecdd-4605-85ad-446926eeb47f"
    schemaVersion="2.0.0.20140822"
>
 <eml:Citation xsi:type="eml:Citation">
  <eml:Title xsi:type="eml:DescriptionString">Hdf5File</eml:Title>
  <eml:Originator xsi:type="eml:NameString">current User</eml:Originator>
  <eml:Creation xsi:type="xsd:dateTime">2016-06-13T11:38:11Z</eml:Creation>
  <eml:Format xsi:type="eml:DescriptionString">Paradigm From Petrel Test</eml:Format>
  <eml:VersionString xsi:type="xsd:string">2016-06-13T11:38:11Z</eml:VersionString>
 </eml:Citation>
 <eml:MimeType xsi:type="xsd:string">application/x-hdf5</eml:MimeType>
</eml:EpcExternalPartReference>
)xml",
        {}
    };
    return mock__;
}

const EmlObjMock& crs_mock() {
    static const EmlObjMock mock__{
        "Default local depth CRS",
        "obj_LocalDepth3dCrs",
        Guid::parse("1c8bb605-c8e7-4da9-af4c-52e5a5e5147b"),
        R"xml(<?xml version="1.0" encoding="UTF-8"?>
<resqml:LocalDepth3dCrs
    xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"
    xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xmlns:xsd="http://www.w3.org/2001/XMLSchema"
    xmlns:gts="http://www.isotc211.org/2005/gts"
    xmlns:gsr="http://www.isotc211.org/2005/gsr"
    xmlns:eml="http://www.energistics.org/energyml/data/commonv2"
    xmlns:gml="http://www.opengis.net/gml/3.2"
    xmlns:xlink="http://www.w3.org/1999/xlink"
    xmlns:gmd="http://www.isotc211.org/2005/gmd"
    xmlns:gco="http://www.isotc211.org/2005/gco"
    xmlns:resqml="http://www.energistics.org/energyml/data/resqmlv2"
    xsi:type="resqml:obj_LocalDepth3dCrs"
    uuid="1c8bb605-c8e7-4da9-af4c-52e5a5e5147b"
    schemaVersion="2.0.0.20140822"
>
	<eml:Citation xsi:type="eml:Citation">
		<eml:Title xsi:type="eml:DescriptionString">Default local depth CRS</eml:Title>
		<eml:Originator xsi:type="eml:NameString">UNKNOWN</eml:Originator>
		<eml:Creation xsi:type="xsd:dateTime">1970-01-01T00:00:00Z</eml:Creation>
		<eml:Format xsi:type="eml:DescriptionString">Paradigm From Petrel Test</eml:Format>
	</eml:Citation>
	<resqml:ExtraMetadata xsi:type="resqml:NameValuePair">
		<resqml:Name xsi:type="xsd:string">pdgm/dx/resqml,/creatorGroup</resqml:Name>
		<resqml:Value xsi:type="xsd:string">UNKNOWN</resqml:Value>
	</resqml:ExtraMetadata>
	<resqml:ExtraMetadata xsi:type="resqml:NameValuePair">
		<resqml:Name xsi:type="xsd:string">pdgm/dx/resqml,/project</resqml:Name>
		<resqml:Value xsi:type="xsd:string">68b47733-261a-484a-99ae-e048bfd05399</resqml:Value>
	</resqml:ExtraMetadata>
	<resqml:YOffset xsi:type="xsd:double">0</resqml:YOffset>
	<resqml:ZOffset xsi:type="xsd:double">0</resqml:ZOffset>
	<resqml:ArealRotation uom="rad" xsi:type="eml:PlaneAngleMeasure">0</resqml:ArealRotation>
	<resqml:ProjectedAxisOrder xsi:type="eml:AxisOrder2d">easting northing</resqml:ProjectedAxisOrder>
	<resqml:ProjectedUom xsi:type="eml:LengthUom">ft[US]</resqml:ProjectedUom>
	<resqml:VerticalUom xsi:type="eml:LengthUom">ft[US]</resqml:VerticalUom>
	<resqml:XOffset xsi:type="xsd:double">0</resqml:XOffset>
	<resqml:ZIncreasingDownward xsi:type="xsd:boolean">true</resqml:ZIncreasingDownward>
	<resqml:VerticalCrs xsi:type="eml:VerticalUnknownCrs">
		<eml:Unknown xsi:type="eml:DescriptionString">NAD 1927</eml:Unknown>
	</resqml:VerticalCrs>
	<resqml:ProjectedCrs xsi:type="eml:ProjectedCrsEpsgCode">
		<eml:EpsgCode xsi:type="xsd:positiveInteger">32056</eml:EpsgCode>
	</resqml:ProjectedCrs>
</resqml:LocalDepth3dCrs>
)xml",
        {}
    };
    return mock__;
}

const EmlObjMock& datum_mock() {
    static const EmlObjMock mock__{
        "49025112030000 location",
        "obj_MdDatum",
        Guid::parse("0ee22f34-1049-5b48-b0e9-c5eb4e4f340f"),
        R"xml(<?xml version="1.0" encoding="UTF-8"?>
<resqml:MdDatum
    xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"
    xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xmlns:xsd="http://www.w3.org/2001/XMLSchema"
    xmlns:gts="http://www.isotc211.org/2005/gts"
    xmlns:gsr="http://www.isotc211.org/2005/gsr"
    xmlns:eml="http://www.energistics.org/energyml/data/commonv2"
    xmlns:gml="http://www.opengis.net/gml/3.2"
    xmlns:xlink="http://www.w3.org/1999/xlink"
    xmlns:gmd="http://www.isotc211.org/2005/gmd"
    xmlns:gco="http://www.isotc211.org/2005/gco"
    xmlns:resqml="http://www.energistics.org/energyml/data/resqmlv2"
    xsi:type="resqml:obj_MdDatum"
    uuid="0ee22f34-1049-5b48-b0e9-c5eb4e4f340f"
    schemaVersion="2.0.0.20140822"
>
	<eml:Citation xsi:type="eml:Citation">
		<eml:Title xsi:type="eml:DescriptionString">49025112030000 location</eml:Title>
		<eml:Originator xsi:type="eml:NameString">UNKNOWN</eml:Originator>
		<eml:Creation xsi:type="xsd:dateTime">1970-01-01T00:00:00Z</eml:Creation>
		<eml:Format xsi:type="eml:DescriptionString">Paradigm From Petrel Test</eml:Format>
		<eml:Editor xsi:type="eml:NameString">UNKNOWN</eml:Editor>
		<eml:LastUpdate xsi:type="xsd:dateTime">2016-06-13T09:36:34Z</eml:LastUpdate>
		<eml:Description xsi:type="eml:CommentString">88-1-AX-33</eml:Description>
	</eml:Citation>
	<resqml:ExtraMetadata xsi:type="resqml:NameValuePair">
		<resqml:Name xsi:type="xsd:string">pdgm/dx/resqml,/creatorGroup</resqml:Name>
		<resqml:Value xsi:type="xsd:string">Unassigned</resqml:Value>
	</resqml:ExtraMetadata>
	<resqml:ExtraMetadata xsi:type="resqml:NameValuePair">
		<resqml:Name xsi:type="xsd:string">pdgm/dx/resqml,/project</resqml:Name>
		<resqml:Value xsi:type="xsd:string">68b47733-261a-484a-99ae-e048bfd05399</resqml:Value>
	</resqml:ExtraMetadata>
	<resqml:ExtraMetadata xsi:type="resqml:NameValuePair">
		<resqml:Name xsi:type="xsd:string">pdgm/dx/resqml,/measuredDepthUom</resqml:Name>
		<resqml:Value xsi:type="xsd:string">ft[US]</resqml:Value>
	</resqml:ExtraMetadata>
	<resqml:ExtraMetadata xsi:type="resqml:NameValuePair">
		<resqml:Name xsi:type="xsd:string">pdgm/dx/resqml,/KBElevation</resqml:Name>
		<resqml:Value xsi:type="xsd:string">5155.9896880037077</resqml:Value>
	</resqml:ExtraMetadata>
	<resqml:Location xsi:type="resqml:Point3d">
		<resqml:Coordinate1 xsi:type="xsd:double">798581.30283477437</resqml:Coordinate1>
		<resqml:Coordinate2 xsi:type="xsd:double">961389.17721849133</resqml:Coordinate2>
		<resqml:Coordinate3 xsi:type="xsd:double">-5155.9896880037077</resqml:Coordinate3>
	</resqml:Location>
	<resqml:MdReference xsi:type="resqml:MdReference">kelly bushing</resqml:MdReference>
	<resqml:LocalCrs xsi:type="eml:DataObjectReference">
		<eml:ContentType xsi:type="xsd:string">application/x-resqml+xml;version=2.0;type=obj_LocalDepth3dCrs</eml:ContentType>
		<eml:Title xsi:type="eml:DescriptionString">Default local depth CRS</eml:Title>
		<eml:UUID xsi:type="eml:UuidString">1c8bb605-c8e7-4da9-af4c-52e5a5e5147b</eml:UUID>
		<eml:UuidAuthority xsi:type="xsd:string">pdgm</eml:UuidAuthority>
		<eml:VersionString xsi:type="eml:NameString">1970-01-01T00:00:00Z</eml:VersionString>
	</resqml:LocalCrs>
</resqml:MdDatum>
)xml",
        {}
    };
    return mock__;
}

// From Teaopot originally
const EmlObjMock& grid2d_mock() {
    static const EmlObjMock mock__{
        "Grid",
        "obj_Grid2dRepresentation",
        Guid::parse("09edea9f-dcf7-45a0-bee8-1505fe372253"),
        R"xml(<?xml version="1.0" encoding="UTF-8"?>
<resqml2:Grid2dRepresentation
    xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"
    xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xmlns:xsd="http://www.w3.org/2001/XMLSchema"
    xmlns:gts="http://www.isotc211.org/2005/gts"
    xmlns:gsr="http://www.isotc211.org/2005/gsr"
    xmlns:eml="http://www.energistics.org/energyml/data/commonv2"
    xmlns:gml="http://www.opengis.net/gml/3.2"
    xmlns:xlink="http://www.w3.org/1999/xlink"
    xmlns:gmd="http://www.isotc211.org/2005/gmd"
    xmlns:gco="http://www.isotc211.org/2005/gco"
    xmlns:resqml2="http://www.energistics.org/energyml/data/resqmlv2"
    schemaVersion="2.0"
    uuid="09edea9f-dcf7-45a0-bee8-1505fe372253"
    xsi:type="resqml2:obj_Grid2dRepresentation"
>
	<eml:Citation xsi:type="eml:Citation">
		<eml:Title xsi:type="eml:DescriptionString">Grid</eml:Title>
		<eml:Originator xsi:type="eml:NameString">operator</eml:Originator>
		<eml:Creation xsi:type="xsd:dateTime">2020-05-04T15:12:12Z</eml:Creation>
		<eml:Format xsi:type="eml:DescriptionString">Paradigm StratEarth 21 Alpha 1 Build:20200503-0204 (id: origin/master|52261|f7df0a15a0|20200501-1559) for Win_x64_6.1_v15</eml:Format>
	</eml:Citation>
	<resqml2:ExtraMetadata xsi:type="resqml2:NameValuePair">
		<resqml2:Name xsi:type="xsd:string">pdgm/dx/resqml,/creatorGroup</resqml2:Name>
		<resqml2:Value xsi:type="xsd:string">operator</resqml2:Value>
	</resqml2:ExtraMetadata>
	<resqml2:ExtraMetadata xsi:type="resqml2:NameValuePair">
		<resqml2:Name xsi:type="xsd:string">pdgm/dx/resqml,/project</resqml2:Name>
		<resqml2:Value xsi:type="xsd:string">83e5817e-3e53-4f25-88e2-7ca322c7a0ec</resqml2:Value>
	</resqml2:ExtraMetadata>
    <!-- Commenting out Interp, since not 
	<resqml2:RepresentedInterpretation xsi:type="eml:DataObjectReference">
		<eml:ContentType xsi:type="xsd:string">application/x-resqml+xml;version=2.0;type=obj_HorizonInterpretation</eml:ContentType>
		<eml:Title xsi:type="eml:DescriptionString">demo1</eml:Title>
		<eml:UUID xsi:type="eml:UuidString">a759aa67-d0bc-46d4-95bb-64dcd426e3fb</eml:UUID>
		<eml:UuidAuthority xsi:type="xsd:string">pdgm</eml:UuidAuthority>
		<eml:VersionString xsi:type="eml:NameString">2020-05-04T15:44:17Z</eml:VersionString>
	</resqml2:RepresentedInterpretation>
    -->
	<resqml2:SurfaceRole xsi:type="resqml2:SurfaceRole">map</resqml2:SurfaceRole>
	<resqml2:Grid2dPatch xsi:type="resqml2:Grid2dPatch">
		<resqml2:PatchIndex xsi:type="xsd:nonNegativeInteger">0</resqml2:PatchIndex>
		<resqml2:FastestAxisCount xsi:type="xsd:positiveInteger">329</resqml2:FastestAxisCount>
		<resqml2:SlowestAxisCount xsi:type="xsd:positiveInteger">167</resqml2:SlowestAxisCount>
		<resqml2:Geometry xsi:type="resqml2:PointGeometry">
            <!-- Replacing original CRS with same as MdDatum above
			<resqml2:LocalCrs xsi:type="eml:DataObjectReference">
				<eml:ContentType xsi:type="xsd:string">application/x-resqml+xml;version=2.0;type=obj_LocalDepth3dCrs</eml:ContentType>
				<eml:Title xsi:type="eml:DescriptionString">Default local depth CRS</eml:Title>
				<eml:UUID xsi:type="eml:UuidString">f55e8386-2069-4177-9256-67b936e3593a</eml:UUID>
				<eml:UuidAuthority xsi:type="xsd:string">pdgm</eml:UuidAuthority>
				<eml:VersionString xsi:type="eml:NameString">2020-05-04T15:46:59Z</eml:VersionString>
			</resqml2:LocalCrs>
            -->
			<resqml2:LocalCrs xsi:type="eml:DataObjectReference">
				<eml:ContentType xsi:type="xsd:string">application/x-resqml+xml;version=2.0;type=obj_LocalDepth3dCrs</eml:ContentType>
				<eml:Title xsi:type="eml:DescriptionString">Default local depth CRS</eml:Title>
				<eml:UUID xsi:type="eml:UuidString">1c8bb605-c8e7-4da9-af4c-52e5a5e5147b</eml:UUID>
				<eml:UuidAuthority xsi:type="xsd:string">pdgm</eml:UuidAuthority>
				<eml:VersionString xsi:type="eml:NameString">1970-01-01T00:00:00Z</eml:VersionString>
			</resqml2:LocalCrs>
			<resqml2:Points xsi:type="resqml2:Point3dZValueArray">
                <!-- Getting rid of unwanted relation, not important for unit test
				<resqml2:SupportingGeometry xsi:type="resqml2:Point3dFromRepresentationLatticeArray">
					<resqml2:NodeIndicesOnSupportingRepresentation xsi:type="resqml2:IntegerLatticeArray">
						<resqml2:StartValue xsi:type="xsd:integer">0</resqml2:StartValue>
						<resqml2:Offset xsi:type="resqml2:IntegerConstantArray">
							<resqml2:Value xsi:type="xsd:integer">1</resqml2:Value>
							<resqml2:Count xsi:type="xsd:positiveInteger">166</resqml2:Count>
						</resqml2:Offset>
						<resqml2:Offset xsi:type="resqml2:IntegerConstantArray">
							<resqml2:Value xsi:type="xsd:integer">1</resqml2:Value>
							<resqml2:Count xsi:type="xsd:positiveInteger">328</resqml2:Count>
						</resqml2:Offset>
					</resqml2:NodeIndicesOnSupportingRepresentation>
					<resqml2:SupportingRepresentation xsi:type="eml:DataObjectReference">
						<eml:ContentType xsi:type="xsd:string">application/x-resqml+xml;version=2.0;type=obj_Grid2dRepresentation</eml:ContentType>
						<eml:Title xsi:type="eml:DescriptionString">Full_Teapot_3D</eml:Title>
						<eml:UUID xsi:type="eml:UuidString">1089e21c-c189-45a6-a4ac-970f5e512e20</eml:UUID>
						<eml:UuidAuthority xsi:type="xsd:string">pdgm</eml:UuidAuthority>
						<eml:VersionString xsi:type="eml:NameString">2020-05-04T15:10:36Z</eml:VersionString>
					</resqml2:SupportingRepresentation>
				</resqml2:SupportingGeometry>
                -->
				<resqml2:ZValues xsi:type="resqml2:DoubleHdf5Array">
					<resqml2:Values xsi:type="eml:Hdf5Dataset">
						<eml:PathInHdfFile xsi:type="xsd:string">/RESQML/09edea9f-dcf7-45a0-bee8-1505fe372253/points_patch0</eml:PathInHdfFile>
						<eml:HdfProxy xsi:type="eml:DataObjectReference">
							<eml:ContentType xsi:type="xsd:string">application/x-eml+xml;version=2.0;type=obj_EpcExternalPartReference</eml:ContentType>
							<eml:Title xsi:type="eml:DescriptionString">D:/TESTING/OpenDB/Data/Teapot/AllProj_PNS19.h5</eml:Title>
							<eml:UUID xsi:type="eml:UuidString">54c1304f-fc13-40cd-88cd-3871ed1bf291</eml:UUID>
							<eml:UuidAuthority xsi:type="xsd:string">pdgm</eml:UuidAuthority>
							<eml:VersionString xsi:type="eml:NameString">2020-05-04T17:47:03Z</eml:VersionString>
						</eml:HdfProxy>
					</resqml2:Values>
				</resqml2:ZValues>
			</resqml2:Points>
		</resqml2:Geometry>
	</resqml2:Grid2dPatch>
</resqml2:Grid2dRepresentation>
)xml",
        {
            EmlAryMock{
                "/RESQML/09edea9f-dcf7-45a0-bee8-1505fe372253/points_patch0",
                { 167, 329 }, // slowest first, fastest last
                etpc::Datatypes::AnyArrayType::eArrayOfDouble, {},
                [](etpc::Datatypes::AnyArray_item_v& any_ary) {
                    std::vector<double> values(167 * 329, 1.0);
                    for (int i = 0; i < 167; ++i)   // see <SlowestAxisCount> above
                    for (int j = 0; j < 329; ++j) { // see <FastestAxisCount> above
                        int ij = (i * 329) + j;
                        double val = (j + (i * 1000));
                        values[ij] = val;
                    }
                    etpc::Datatypes::ArrayOfDouble double_ary;
                    double_ary.m_values = std::move(values);
                    any_ary = std::move(double_ary);
                }
            }
        }
    };

    return mock__;
}

static const int64_t traj_size = 1673;

const EmlObjMock& traj_mock() {
    static const EmlObjMock mock__{
        "POSITION_OPTIMIZED",
        "obj_WellboreTrajectoryRepresentation",
        Guid::parse("4a6e1e5a-54f3-589a-b197-6e2ce35699f1"),
        R"xml(<?xml version="1.0" encoding="UTF-8"?>
<resqml:WellboreTrajectoryRepresentation
    xmlns:SOAP-ENV="http://schemas.xmlsoap.org/soap/envelope/"
    xmlns:SOAP-ENC="http://schemas.xmlsoap.org/soap/encoding/"
    xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
    xmlns:xsd="http://www.w3.org/2001/XMLSchema"
    xmlns:gts="http://www.isotc211.org/2005/gts"
    xmlns:gsr="http://www.isotc211.org/2005/gsr"
    xmlns:eml="http://www.energistics.org/energyml/data/commonv2"
    xmlns:gml="http://www.opengis.net/gml/3.2"
    xmlns:xlink="http://www.w3.org/1999/xlink"
    xmlns:gmd="http://www.isotc211.org/2005/gmd"
    xmlns:gco="http://www.isotc211.org/2005/gco"
    xmlns:resqml="http://www.energistics.org/energyml/data/resqmlv2"
    xsi:type="resqml:obj_WellboreTrajectoryRepresentation"
    uuid="4a6e1e5a-54f3-589a-b197-6e2ce35699f1"
    schemaVersion="2.0.0.20140822"
>
 <eml:Citation xsi:type="eml:Citation">
  <eml:Title xsi:type="eml:DescriptionString">POSITION_OPTIMIZED</eml:Title>
  <eml:Originator xsi:type="eml:NameString">UNKNOWN</eml:Originator>
  <eml:Creation xsi:type="xsd:dateTime">1970-01-01T00:00:00Z</eml:Creation>
  <eml:Format xsi:type="eml:DescriptionString">Paradigm From Petrel Test</eml:Format>
  <eml:Editor xsi:type="eml:NameString">UNKNOWN</eml:Editor>
  <eml:LastUpdate xsi:type="xsd:dateTime">2016-06-13T09:36:34Z</eml:LastUpdate>
  <eml:Description xsi:type="eml:CommentString">88-1-AX-33</eml:Description>
 </eml:Citation>
 <resqml:ExtraMetadata xsi:type="resqml:NameValuePair">
  <resqml:Name xsi:type="xsd:string">pdgm/dx/resqml,/creatorGroup</resqml:Name>
  <resqml:Value xsi:type="xsd:string">Unassigned</resqml:Value>
 </resqml:ExtraMetadata>
 <resqml:ExtraMetadata xsi:type="resqml:NameValuePair">
  <resqml:Name xsi:type="xsd:string">pdgm/dx/resqml,/project</resqml:Name>
  <resqml:Value xsi:type="xsd:string">68b47733-261a-484a-99ae-e048bfd05399</resqml:Value>
 </resqml:ExtraMetadata>
 <resqml:RepresentedInterpretation xsi:type="eml:DataObjectReference">
  <eml:ContentType xsi:type="xsd:string">application/x-resqml+xml;version=2.0;type=obj_WellboreInterpretation</eml:ContentType>
  <eml:Title xsi:type="eml:DescriptionString">49025112030000</eml:Title>
  <eml:UUID xsi:type="eml:UuidString">ec43b905-4faf-5284-b279-aa7b9b22cd52</eml:UUID>
  <eml:UuidAuthority xsi:type="xsd:string">pdgm</eml:UuidAuthority>
  <eml:VersionString xsi:type="eml:NameString">2016-06-13T09:36:34Z</eml:VersionString>
 </resqml:RepresentedInterpretation>
 <resqml:StartMd xsi:type="xsd:double">0</resqml:StartMd>
 <resqml:FinishMd xsi:type="xsd:double">3153.9936920022683</resqml:FinishMd>
 <resqml:MdUom xsi:type="eml:LengthUom">ft[US]</resqml:MdUom>
 <resqml:Geometry xsi:type="resqml:ParametricLineGeometry">
  <resqml:LocalCrs xsi:type="eml:DataObjectReference">
   <eml:ContentType xsi:type="xsd:string">application/x-resqml+xml;version=2.0;type=obj_LocalDepth3dCrs</eml:ContentType>
   <eml:Title xsi:type="eml:DescriptionString">Default local depth CRS</eml:Title>
   <eml:UUID xsi:type="eml:UuidString">1c8bb605-c8e7-4da9-af4c-52e5a5e5147b</eml:UUID>
   <eml:UuidAuthority xsi:type="xsd:string">pdgm</eml:UuidAuthority>
   <eml:VersionString xsi:type="eml:NameString">1970-01-01T00:00:00Z</eml:VersionString>
  </resqml:LocalCrs>
  <resqml:ControlPointParameters xsi:type="resqml:DoubleHdf5Array">
   <resqml:Values xsi:type="eml:Hdf5Dataset">
    <eml:PathInHdfFile xsi:type="xsd:string">/RESQML/4a6e1e5a-54f3-589a-b197-6e2ce35699f1/controlPointParameters</eml:PathInHdfFile>
    <eml:HdfProxy xsi:type="eml:DataObjectReference">
     <eml:ContentType xsi:type="xsd:string">application/x-resqml+xml;version=2.0;type=EpcExternalPartReference</eml:ContentType>
     <eml:Title xsi:type="eml:DescriptionString">Hdf5File</eml:Title>
     <eml:UUID xsi:type="eml:UuidString">e5e5230e-ecdd-4605-85ad-446926eeb47f</eml:UUID>
     <eml:UuidAuthority xsi:type="xsd:string">pdgm</eml:UuidAuthority>
     <eml:VersionString xsi:type="eml:NameString">2016-06-13T11:38:11Z</eml:VersionString>
    </eml:HdfProxy>
   </resqml:Values>
  </resqml:ControlPointParameters>
  <resqml:ControlPoints xsi:type="resqml:Point3dHdf5Array">
   <resqml:Coordinates xsi:type="eml:Hdf5Dataset">
    <eml:PathInHdfFile xsi:type="xsd:string">/RESQML/4a6e1e5a-54f3-589a-b197-6e2ce35699f1/controlPoints</eml:PathInHdfFile>
    <eml:HdfProxy xsi:type="eml:DataObjectReference">
     <eml:ContentType xsi:type="xsd:string">application/x-resqml+xml;version=2.0;type=EpcExternalPartReference</eml:ContentType>
     <eml:Title xsi:type="eml:DescriptionString">Hdf5File</eml:Title>
     <eml:UUID xsi:type="eml:UuidString">e5e5230e-ecdd-4605-85ad-446926eeb47f</eml:UUID>
     <eml:UuidAuthority xsi:type="xsd:string">pdgm</eml:UuidAuthority>
     <eml:VersionString xsi:type="eml:NameString">2016-06-13T11:38:11Z</eml:VersionString>
    </eml:HdfProxy>
   </resqml:Coordinates>
  </resqml:ControlPoints>
  <resqml:KnotCount xsi:type="xsd:positiveInteger">2</resqml:KnotCount>
  <resqml:LineKindIndex xsi:type="xsd:integer">5</resqml:LineKindIndex>
  <resqml:TangentVectors xsi:type="resqml:Point3dHdf5Array">
   <resqml:Coordinates xsi:type="eml:Hdf5Dataset">
    <eml:PathInHdfFile xsi:type="xsd:string">/RESQML/4a6e1e5a-54f3-589a-b197-6e2ce35699f1/tangentVectors</eml:PathInHdfFile>
    <eml:HdfProxy xsi:type="eml:DataObjectReference">
     <eml:ContentType xsi:type="xsd:string">application/x-resqml+xml;version=2.0;type=EpcExternalPartReference</eml:ContentType>
     <eml:Title xsi:type="eml:DescriptionString">Hdf5File</eml:Title>
     <eml:UUID xsi:type="eml:UuidString">e5e5230e-ecdd-4605-85ad-446926eeb47f</eml:UUID>
     <eml:UuidAuthority xsi:type="xsd:string">pdgm</eml:UuidAuthority>
     <eml:VersionString xsi:type="eml:NameString">2016-06-13T11:38:11Z</eml:VersionString>
    </eml:HdfProxy>
   </resqml:Coordinates>
  </resqml:TangentVectors>
 </resqml:Geometry>
 <resqml:MdDatum xsi:type="eml:DataObjectReference">
  <eml:ContentType xsi:type="xsd:string">application/x-resqml+xml;version=2.0;type=obj_MdDatum</eml:ContentType>
  <eml:Title xsi:type="eml:DescriptionString">49025112030000 location</eml:Title>
  <eml:UUID xsi:type="eml:UuidString">0ee22f34-1049-5b48-b0e9-c5eb4e4f340f</eml:UUID>
  <eml:UuidAuthority xsi:type="xsd:string">pdgm</eml:UuidAuthority>
  <eml:VersionString xsi:type="eml:NameString">2016-06-13T09:36:34Z</eml:VersionString>
 </resqml:MdDatum>
</resqml:WellboreTrajectoryRepresentation>
)xml",
        {
            // The number of "stations" for the well trajectory does not seem to appear
            // anywhere in the trajector's XML, so we'll just choose an arbitrary one.
            // Also, the values themselves don't matter much in this case. We just need
            // arrays to play with, with non-all-zero
            EmlAryMock{
                "/RESQML/4a6e1e5a-54f3-589a-b197-6e2ce35699f1/controlPointParameters",
                { traj_size },
                etpc::Datatypes::AnyArrayType::eArrayOfDouble, {},
                [](etpc::Datatypes::AnyArray_item_v& any_ary) {
                    std::vector<double> values(traj_size, 1.0);
                    for (size_t i = 0; i < values.size(); ++i) {
                        values[i] = static_cast<double>(i);
                    }
                    etpc::Datatypes::ArrayOfDouble double_ary;
                    double_ary.m_values = std::move(values);
                    any_ary = std::move(double_ary);
                }
            },
            EmlAryMock{
                "/RESQML/4a6e1e5a-54f3-589a-b197-6e2ce35699f1/controlPoints",
                { traj_size, 3 },
                etpc::Datatypes::AnyArrayType::eArrayOfDouble, {},
                [](etpc::Datatypes::AnyArray_item_v& any_ary) {
                    const double x = 123.45;
                    const double y = 456.78;
                    const double md_max = 3153.9936920022683;
                    const double md_inc = md_max / (traj_size - 1);
                    std::vector<double> values(traj_size * 3, 1.0);
                    for (size_t i = 0; i < values.size(); i += 3) {
                        values[i + 0] = x;
                        values[i + 1] = y;
                        values[i + 2] = i * md_inc;
                    }
                    values.back() = md_max; // end *precisely* at md_max
                    etpc::Datatypes::ArrayOfDouble double_ary;
                    double_ary.m_values = std::move(values);
                    any_ary = std::move(double_ary);
                }
            },
            EmlAryMock{
                "/RESQML/4a6e1e5a-54f3-589a-b197-6e2ce35699f1/tangentVectors",
                { traj_size, 3 },
                etpc::Datatypes::AnyArrayType::eArrayOfDouble, {},
                [](etpc::Datatypes::AnyArray_item_v& any_ary) {
                    std::vector<double> values(traj_size * 3, 0.0);
                    for (size_t i = 0; i < values.size(); i += 3) {
                        // x, y of vector remain at zero
                        values[i + 2] = 1.0;
                    }
                    etpc::Datatypes::ArrayOfDouble double_ary;
                    double_ary.m_values = std::move(values);
                    any_ary = std::move(double_ary);
                }
            },
        }
    };
    return mock__;
}

//============================================================================

} // namespace oes::testing::eml
