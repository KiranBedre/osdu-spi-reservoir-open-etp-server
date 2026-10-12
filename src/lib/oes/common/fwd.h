#ifndef OES_COMMON_FWD_H
#define OES_COMMON_FWD_H

#include <memory>

//============================================================================

namespace oes::common::epc {

enum class RelMode : unsigned char;
enum class RelType : unsigned char;
enum class PtyType : unsigned char;

class ContentType;
class CoreProperty;
class FileContentType;
class FileCoreProperties;
class FilePart;
class FileRelationship;
class Package;
class Relationship;

} // namespace oes::common::epc

//============================================================================

namespace oes::common::epc::xml {

// epc/FilePart.h
struct AbstractCitedDataObject;
struct EpcExternalPartReference;
struct DataObjectReference;
struct Hdf5Dataset;
struct XmlName;

} // namespace oes::common::epc::xml

//============================================================================

namespace oes::common::auth {
// auth/AuthN.h
enum class AuthType : unsigned char;

struct AuthInfo;
struct AuthDetails;

struct AuthenticatedUser;
using  AuthenticatedUser_ptr = std::shared_ptr<AuthenticatedUser>;
using  AuthenticatedUser_const = std::shared_ptr<const AuthenticatedUser>;

class Entitlement;
using Entitlement_ptr = std::shared_ptr<Entitlement>;

// auth/JWToken.h
struct JwtOutcome;
class JwtObject;

} // namespace oes::common::auth

//============================================================================

namespace oes::common::etp12 {

// etp12/EmlUri.h
struct EmlUri;

// etp12/Tracer.h
struct EtpTracerEventInfo;
struct EtpTraceHeader;
class  EtpTracer12;

} // namespace oes::common::etp12

//============================================================================

namespace oes::common::etp12::Datatypes {

// etp12/Messages.h
struct ErrorInfo;
struct MessageHeader;
struct DataValue;

} // namespace oes::common::etp12::Datatypes

//============================================================================

namespace oes::common::etp12::Datatypes::Object {

struct Resource;

} // namespace oes::common::etp12::Datatypes::Object

//============================================================================

namespace oes::common::etp12::Protocol {

// etp12/Protocols.h
struct EtpResponseStatus;

} // namespace oes::common::etp12::Protocol

#endif //OES_COMMON_FWD_H