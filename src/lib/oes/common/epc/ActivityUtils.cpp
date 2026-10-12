#include <oes/common/etp12/EmlUri.h>
#include <sstream>

#include "ActivityUtils.h"

#include <oes/core/utils/config_enter_3rdparty.h>
#include <fmt/format.h>
#include <oes/core/utils/config_leave_3rdparty.h>

namespace oes::common::epc {

std::string escapeXml(const std::string& input) {
    std::string output;
    
    for (char c : input) {
        switch (c) {
            case '&': output += "&amp;"; break;
            case '<': output += "&lt;"; break;
            case '>': output += "&gt;"; break;
            case '\'': output += "&apos;"; break;
            case '"': output += "&quot;"; break;
            default: output += c; break;
        }
    }
    return output;
}

AnalyzedObject::AnalyzedObject(
    const std::string& uuid,
    const std::string& ml,
    const std::string& version,
    const std::string& type,
    const std::string& title)
    : uuid_(uuid),
      ml_(ml),
      version_(version),
      type_(type),
      title_(title) {}

// "Header" and "footer"
std::pair<std::string, std::string> get_xml_header_and_footer(
    const std::string& xml_type, // resqml20:ActivityTemplate or resqml20:Activity
    const std::string& obj_type, // resqml20:obj_ActivityTemplate or  resqml20:obj_Activity
    const std::string& obj_uuid
) {
    /*
    <resqml20:Activity xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:gts="http://www.isotc211.org/2005/gts" xmlns:gsr="http://www.isotc211.org/2005/gsr" xmlns:dc="http://purl.org/dc/terms/" xmlns:gml="http://www.opengis.net/gml/3.2" xmlns:xlink="http://www.w3.org/1999/xlink" xmlns:gmd="http://www.isotc211.org/2005/gmd" xmlns:gco="http://www.isotc211.org/2005/gco" xmlns:abstract="http://www.energistics.org/schemas/abstract" xmlns:resqml20="http://www.energistics.org/energyml/data/resqmlv2" xmlns:eml20="http://www.energistics.org/energyml/data/commonv2" xsi:type="resqml20:obj_Activity" uuid="1438e089-b9c9-452c-b533-c1d8ac062804" schemaVersion="2.0">
    
      <resqml20:ExtraMetadata>
    <resqml20:Name>activityStatus</resqml20:Name>
    <resqml20:Value>Done</resqml20:Value>
  </resqml20:ExtraMetadata>
</resqml20:Activity>
    */
   static const std::string xml_template_header = R"xml(
<{} xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:gts="http://www.isotc211.org/2005/gts" xmlns:gsr="http://www.isotc211.org/2005/gsr" xmlns:dc="http://purl.org/dc/terms/" xmlns:gml="http://www.opengis.net/gml/3.2" xmlns:xlink="http://www.w3.org/1999/xlink" xmlns:gmd="http://www.isotc211.org/2005/gmd" xmlns:gco="http://www.isotc211.org/2005/gco" xmlns:abstract="http://www.energistics.org/schemas/abstract" xmlns:resqml20="http://www.energistics.org/energyml/data/resqmlv2" xmlns:eml20="http://www.energistics.org/energyml/data/commonv2" xsi:type="{}" uuid="{}" schemaVersion="2.0">
)xml";
   static const std::string xml_template_footer = R"xml(</{}>)xml";
   return std::make_pair<std::string, std::string>(
        fmt::format(xml_template_header, xml_type, obj_type, obj_uuid),
        fmt::format(xml_template_footer, xml_type)
   );
}

// Citation
std::string get_import_activity_Citation(
    const std::string& title,
    const std::string& originator,
    const std::string& creation_dt,
    const std::string& format
) {
    /*
    <eml20:Citation>
      <eml20:Title>{TITLE}</eml20:Title>
      <eml20:Originator>{ORIGINATOR}</eml20:Originator>
      <eml20:Creation>{CREATION_DT}</eml20:Creation>
      <eml20:Format>{FORMAT}</eml20:Format>
    </eml20:Citation>
    */
   static const std::string citation_template = R"xml(  <eml20:Citation xsi:type="eml20:Citation">
    <eml20:Title xsi:type="eml20:DescriptionString">{}</eml20:Title>
    <eml20:Originator xsi:type="eml20:NameString">{}</eml20:Originator>
    <eml20:Creation xsi:type="xsd:dateTime">{}</eml20:Creation>
    <eml20:Format xsi:type="eml20:DescriptionString">{}</eml20:Format>
  </eml20:Citation>
)xml";

    return fmt::format(
        citation_template,
        title,
        originator,
        creation_dt,
        format
    );
}

// Descriptor
std::string get_import_activity_ADescriptor( // dataobject reference
    const std::string& desc_type, // eml20:DataObjectReference
    const std::string& type, // obj_ActivityTemplate
    const std::string& title,
    const std::string& uuid,
    const std::string& authority
    ){ // activity descriptor
    /*
    <resqml20:ActivityDescriptor xsi:type="{DESC_TYPE">
      <eml20:ContentType>application/x-resqml+xml;version=2.0;type={TYPE}</eml20:ContentType>
      <eml20:Title>{TITLE}</eml20:Title>
      <eml20:UUID>{UUID}</eml20:UUID>
      <eml20:UuidAuthority>{AUTHORITY}</eml20:UuidAuthority>
    </resqml20:ActivityDescriptor>
    */
    
    static const std::string dor_template = R"xml(  <resqml20:ActivityDescriptor xsi:type="{}">
    <eml20:ContentType xsi:type="xsd:string">application/x-resqml+xml;version=2.0;type={}</eml20:ContentType>
    <eml20:Title xsi:type="eml20:DescriptionString">{}</eml20:Title>
    <eml20:UUID xsi:type="eml20:UuidString">{}</eml20:UUID>
    <eml20:UuidAuthority xsi:type="xsd:string">{}</eml20:UuidAuthority>
  </resqml20:ActivityDescriptor>
)xml";
    return fmt::format(
        dor_template,
        desc_type,
        type,
        title,
        uuid,
        authority
    );
}

// Parameter
std::string get_import_activity_Parameter( // dataobject parameter
    const std::string& parameter_type, // resqml20:DataObjectParameter
    const std::string& parameter_title,
    const std::string& obj_ml,
    const std::string& obj_version,
    const std::string& obj_type,
    const std::string& obj_title,
    const std::string& obj_uuid,
    const std::string& obj_authority,
    const size_t index
    ) {
    /*
    <resqml20:Parameter xsi:type="resqml20:DataObjectParameter">
      <resqml20:Title>{PARAMETER_TITLE}</resqml20:Title>
      <resqml20:DataObject xsi:type="eml20:DataObjectReference">
        <eml20:ContentType>application/x-resqml+xml;version=2.0;type={OBJ_TYPE}</eml20:ContentType>
        <eml20:Title>{OBJ_TITLE}</eml20:Title>
        <eml20:UUID>{OBJ_UUID}</eml20:UUID>
        <eml20:UuidAuthority>{OBJ_AUTHORITY}</eml20:UuidAuthority>
      </resqml20:DataObject>
      <resqml20:Index>{INDEX}</resqml20:Index>
    </resqml20:Parameter>)
    */
    static const std::string parameter_template = R"xml(  <resqml20:Parameter xsi:type="{}">
    <resqml20:Title xsi:type="xsd:string">{}</resqml20:Title>
    <resqml20:DataObject xsi:type="eml20:DataObjectReference">
      <eml20:ContentType xsi:type="xsd:string">application/x-{}+xml;version={};type={}</eml20:ContentType>
      <eml20:Title xsi:type="eml20:DescriptionString">{}</eml20:Title>
      <eml20:UUID xsi:type="eml20:UuidString">{}</eml20:UUID>
      <eml20:UuidAuthority xsi:type="xsd:string">{}</eml20:UuidAuthority>
    </resqml20:DataObject>
    <resqml20:Index>{}</resqml20:Index>
  </resqml20:Parameter>
)xml";
    return fmt::format(
        parameter_template,
        parameter_type,
        parameter_title,
        obj_ml,
        obj_version,
        escapeXml(obj_type),
        escapeXml(obj_title),
        obj_uuid,
        escapeXml(obj_authority),
        index
    );
}

std::string get_import_activity_Extradata(
    const std::string& name, // activityStatus
    const std::string& status // Done
) {
    /*
    <resqml20:ExtraMetadata>
    <resqml20:Name>activityStatus</resqml20:Name>
    <resqml20:Value>Done</resqml20:Value>
  </resqml20:ExtraMetadata>
    */
    static const std::string extrametadata_template = R"xml(  <resqml20:ExtraMetadata xsi:type="resqml20:NameValuePair">
    <resqml20:Name xsi:type="xsd:string">{}</resqml20:Name>
    <resqml20:Value xsi:type="xsd:string">{}</resqml20:Value>
  </resqml20:ExtraMetadata>
)xml";
    return fmt::format(
        extrametadata_template,
        name,
        status
    );
}

// ACTIVITY TEMPLATE
std::string get_activity_template_data(
    const std::string& user_id,
    const std::string& dt_creation
) {
    static const std::string parameter_template = R"xml(
<resqml20:ActivityTemplate xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance" xmlns:xsd="http://www.w3.org/2001/XMLSchema" xmlns:gts="http://www.isotc211.org/2005/gts" xmlns:gsr="http://www.isotc211.org/2005/gsr" xmlns:dc="http://purl.org/dc/terms/" xmlns:gml="http://www.opengis.net/gml/3.2" xmlns:xlink="http://www.w3.org/1999/xlink" xmlns:gmd="http://www.isotc211.org/2005/gmd" xmlns:gco="http://www.isotc211.org/2005/gco" xmlns:abstract="http://www.energistics.org/schemas/abstract" xmlns:resqml20="http://www.energistics.org/energyml/data/resqmlv2" xmlns:eml20="http://www.energistics.org/energyml/data/commonv2" xsi:type="resqml20:obj_ActivityTemplate" uuid="1801db47-12c3-4f8b-8ae3-25baeb528c23" schemaVersion="2.0">
  <eml20:Citation xsi:type="eml20:Citation">
    <eml20:Title xsi:type="eml20:DescriptionString">Import</eml20:Title>
    <eml20:Originator xsi:type="eml20:NameString">{}</eml20:Originator>
    <eml20:Creation xsi:type="xsd:dateTime">{}</eml20:Creation>
    <eml20:Format xsi:type="eml20:DescriptionString">pdgm:portal-import:1</eml20:Format>
  </eml20:Citation>
  <resqml20:Parameter xsi:type="resqml20:ParameterTemplate">
    <resqml20:Title xsi:type="xsd:string">imported</resqml20:Title>
    <resqml20:AllowedKind xsi:type="resqml20:ParameterKind">dataObject</resqml20:AllowedKind>
    <resqml20:IsInput xsi:type="xsd:boolean">false</resqml20:IsInput>
    <resqml20:IsOutput xsi:type="xsd:boolean">true</resqml20:IsOutput>
    <resqml20:MinOccurs>1</resqml20:MinOccurs>
    <resqml20:MaxOccurs>-1</resqml20:MaxOccurs>
  </resqml20:Parameter>
</resqml20:ActivityTemplate>
)xml";

    return fmt::format(
        parameter_template,
        user_id,
        dt_creation
    );
}

std::string extract_string_between(const std::string& xml, const std::string& str_beg, const std::string& str_end) {
    auto beg_index = xml.find(str_beg);
    auto end_index = xml.find(str_end);
    if (beg_index == std::string::npos ||
        end_index == std::string::npos ||
        end_index <= beg_index) {
        return "";
    }

    auto length = end_index - beg_index - str_beg.size();
    return xml.substr(beg_index + str_beg.size(), length);
}

std::set<std::string> extract_activities_template_titles(const std::string& xml, const std::string& prefix) {
    const std::string elem_beg = "<" + prefix + ":Parameter";
    const std::string elem_end = "</" + prefix + ":Parameter>";

    size_t found_param_beg {0};
    size_t found_param_end {0};
    size_t length;
    std::string xml_cpy (xml);

    std::set<std::string> res;

    while (found_param_beg != std::string::npos) {
        found_param_beg = xml_cpy.find(elem_beg);
        if (found_param_beg == std::string::npos){
            break;
        }
        found_param_end = xml_cpy.find(elem_end, found_param_beg);
        if (found_param_end == std::string::npos) {
            break;
        }
        length = found_param_end - found_param_beg;
        auto param_xml = xml_cpy.substr(found_param_beg, length);
        
        auto title = extract_string_between(
            param_xml,
            "<" + prefix + ":Title>",
            "</" + prefix + ":Title>"
        );

        if( title.empty() ) {
          const std::string xsi_str = R"(:Title xsi:type="xsd:string">)";
          title = extract_string_between(
            param_xml,
            "<" + prefix + xsi_str,
            "</" + prefix + ":Title>"
          );
        }

        auto is_output_str = extract_string_between(
            param_xml,
            "<" + prefix + ":IsOutput>",
            "</" + prefix + ":IsOutput>"
        );

        if( is_output_str.empty() ) {
          const std::string xsi_str = R"(:IsOutput xsi:type="xsd:boolean">)";
          is_output_str = extract_string_between(
            param_xml,
            "<" + prefix + xsi_str,
            "</" + prefix + ":IsOutput>"
          );
        }

        if (is_output_str == "true") {
            res.emplace(title);
        }

        xml_cpy = xml_cpy.substr(found_param_end + elem_end.size());
    }

    return res;
}

} // namespace oes::common::epc