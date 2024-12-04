// ball_recordjsonformatter.cpp                                       -*-C++-*-
#include <ball_recordjsonformatter.h>

#include <bsls_ident.h>
BSLS_IDENT_RCSID(ball_recordjsonformatter_cpp,"$Id$ $CSID$")

///Implementation Notes
///--------------------
// A format specification is, itself, a JSON string supplied to a formatter
// that defines the sequence and format in which a set of log record fields
// will be published (as JSON).  The format specification is represented as
// JSON array of JSON objects and values.  We decode the format specification
// into a 'bdld::Datum' object and for each object and value in the array, we
// create a formatter object that preserves the format specification for the
// corresponding field in the log record.  When a log record is published,
// these formatters are supplied with to the log record to render it as JSON.
//
///Record JSON Formatter Schema
/// - - - - - - - - - - - - - -
// The following is a JSON schema of the Message Format Specification:
//..
// {
//     "$schema": "http://json-schema.org/draft-07/schema",
//     "$id": "http://recordjsonformatter.json",
//     "type": "array",
//     "title": "The schema of the RecordJsonFormatter format specification",
//     "description":
//         "The schema defines a set of objects and their properties  that are
//          used to customize the output of fixed fields  and user-defined
//          attributes of the log record published in JSON.",
//     "definitions": {
//         "timestamp": {
//             "type": "object",
//             "$id": "#timestamp",
//             "title": "timestamp",
//             "description":
//                 "This object type defines the format in which the fixed
//                  'timestamp' field of the  log record will be displayed as
//                  JSON.",
//             "properties": {
//                 "timestamp": {
//                     "type": "object",
//                     "properties": {
//                         "name": {
//                             "description":
//                                 "Custom name for the 'timestamp' fixed
//                                  field.",
//                             "type": "string"
//                         },
//                         "timeZone": {
//                             "type": "string",
//                             "enum": ["local", "utc"]
//                         },
//                         "fractionalSecPrecision": {
//                             "type": "string",
//                             "enum": [
//                                 "none",
//                                 "milliseconds",
//                                 "microseconds"
//                             ]
//                         },
//                         "format": {
//                             "description":
//                                 "Display the time in either in the
//                                  'bdePrint' or ISO 8601 format.",
//                             "type": "string",
//                             "enum": ["bdePrint", "iso8601"]
//                         }
//                     },
//                     "additionalProperties": false
//                 }
//             },
//             "additionalProperties": false,
//             "required": ["timestamp"]
//         },
//         "threadId": {
//             "type": "object",
//             "$id": "#threadId",
//             "title": "threadId",
//             "description":
//                 "This object type defines the format in which the fixed
//                  'threadId' field of the  log record will be displayed as
//                  JSON.",
//             "properties": {
//                 "threadId": {
//                     "type": "object",
//                     "properties": {
//                         "name": {
//                             "description":
//                                 "Custom name for the 'threadId' fixed
//                                  field.",
//                             "type": "string"
//                         },
//                         "hex": {
//                             "description": "Thread output format.",
//                             "type": "string",
//                             "enum": ["hex", "decimal"]
//                         }
//                     },
//                     "additionalProperties": false
//                 }
//             },
//             "additionalProperties": false,
//             "required": ["threadId"]
//         },
//         "file": {
//             "type": "object",
//             "$id": "#file",
//             "title": "file",
//             "description":
//                 "This object type defines the format in which the fixed
//                  'file' field of the log record will be displayed as JSON.",
//             "properties": {
//                 "file": {
//                     "type": "object",
//                     "properties": {
//                         "name": {
//                             "description":
//                                 "Custom name for the 'file' fixed field.",
//                             "type": "string"
//                         },
//                         "path": {
//                             "type": "string",
//                             "enum": ["full", "file"]
//                         }
//                     },
//                     "additionalProperties": false
//                 }
//             },
//             "additionalProperties": false,
//             "required": ["file"]
//         },
//         "field": {
//             "type": "object",
//             "$id": "#field",
//             "title": "field",
//             "description":
//                 "This object type defines the format in which a fixed record
//                  field or a user-defined attribute will be displayed as
//                  JSON.",
//             "examples": [{
//             {
//                 "processId": {
//                     "description": "Fixed record field",
//                     "name": "pid"
//                 },
//                 "bas.uuid": {
//                     "description": "User-defined attribute",
//                     "name": "BAS.UUID"
//                 }
//             }],
//             "properties": {
//                 "name": {
//                     "type": "string"
//                 }
//             }
//             "additionalProperties": false
//         }
//     },
//     "additionalItems": true,
//     "items": {
//         "anyOf": [{
//                 "$ref": "#/definitions/timestamp"
//             },
//             {
//                 "$ref": "#/definitions/threadId"
//             },
//             {
//                 "$ref": "#/definitions/file"
//             },
//             {
//                 "$ref": "#/definitions/field",
//                 "description":
//                     "This object indicates that a fixed record field or a
//                      user-defined attribute having the specified object name
//                      will be displayed as JSON under the value of the
//                      underlying object 'name'.",
//                 "example": [{"message": {"name": "msg"}}]
//             },
//             {
//                 "type": "string",
//                 "title": "a fixed record field or user-defined attribute
//                           name",
//                 "description":
//                     "This string indicates that a fixed record field or a
//                      user-defined attribute having the name specified as the
//                      value of this string, or all user-defined attributes
//                      (in case of the "attributes" value) will be displayed
//                      in the default format.",
//                 "examples": [ "timestamp",
//                               "processId",
//                               "threadId",
//                               "file",
//                               "line",
//                               "category",
//                               "severity",
//                               "message",
//                               "attributes"
//                             ]
//             }
//         ]
//     }
// }
//..
// TBD: verify the schema in a JSON schema validator

#include <ball_managedattribute.h>
#include <ball_record.h>
#include <ball_recordattributes.h>
#include <ball_severity.h>

#include <baljsn_datumutil.h>
#include <baljsn_simpleformatter.h>

#include <bdld_manageddatum.h>

#include <bdldfp_decimal.h>

#include <bdlf_bind.h>
#include <bdlf_placeholder.h>

#include <bdls_pathutil.h>

#include <bdlsb_fixedmemoutstreambuf.h>

#include <bdlt_datetime.h>
#include <bdlt_currenttime.h>
#include <bdlt_localtimeoffset.h>
#include <bdlt_iso8601util.h>
#include <bdlt_iso8601utilconfiguration.h>

#include <bslim_printer.h>

#include <bslma_allocatorutil.h>
#include <bslma_managedptr.h>

#include <bsls_annotation.h>
#include <bsls_platform.h>
#include <bsls_types.h>

#include <bslstl_stringref.h>

#include <bsl_climits.h>   // for 'INT_MAX'
#include <bsl_cstring.h>   // for 'bsl::strcmp'
#include <bsl_c_stdlib.h>
#include <bsl_c_stdio.h>   // for 'snprintf'

#include <bsl_algorithm.h>
#include <bsl_iomanip.h>
#include <bsl_ostream.h>
#include <bsl_set.h>
#include <bsl_sstream.h>

namespace BloombergLP {
namespace ball {
namespace {

typedef BloombergLP::bslma::AllocatorUtil AllocUtil;

const char *const k_KEY_TIMESTAMP        = "timestamp";
const char *const k_KEY_PROCESS_ID       = "pid";
const char *const k_KEY_THREAD_ID        = "tid";
const char *const k_KEY_SEVERITY         = "severity";
const char *const k_KEY_FILE             = "file";
const char *const k_KEY_LINE             = "line";
const char *const k_KEY_CATEGORY         = "category";
const char *const k_KEY_MESSAGE          = "message";
const char *const k_KEY_ATTRIBUTES       = "attributes";

const char *const k_KEY_NAME             = "name";

const char *const k_KEY_TIME_ZONE        = "timeZone";
const char *const k_KEY_FORMAT           = "format";
const char *const k_KEY_PRECISION        = "fractionalSecPrecision";
const char *const k_KEY_PATH             = "path";

const char *const k_VALUE_LOCAL          = "local";
const char *const k_VALUE_UTC            = "utc";
const char *const k_VALUE_BDE_PRINT      = "bdePrint";
const char *const k_VALUE_ISO_8601       = "iso8601";
const char *const k_VALUE_PRECISION_NONE = "none";
const char *const k_VALUE_PRECISION_MS   = "milliseconds";
const char *const k_VALUE_PRECISION_US   = "microseconds";
const char *const k_VALUE_DECIMAL        = "decimal";
const char *const k_VALUE_HEXADECIMAL    = "hex";
const char *const k_VALUE_FILE           = "file";
const char *const k_VALUE_FULL           = "full";

/// Return the default record JSON format specification.
!__out.empty()
bsl::string_view getDefaultFormat()
{
    const  int  k_BUFFER_SIZE = 256;
    static char buffer[k_BUFFER_SIZE];

    sprintf(buffer,
            "[\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"]",
            k_KEY_TIMESTAMP,
            k_KEY_PROCESS_ID,
            k_KEY_THREAD_ID,
            k_KEY_SEVERITY,
            k_KEY_FILE,
            k_KEY_LINE,
            k_KEY_CATEGORY,
            k_KEY_MESSAGE,
            k_KEY_ATTRIBUTES);

    return buffer;
}

}  // close unnamed namespace within BloombergLP::ball

                   // ========================================
                   // class RecordJsonFormatter_FieldFormatter
                   // ========================================

/// This class defines an abstract protocol for parsing a field from a
/// format specification (using `parse`), and then rendering fields from a
/// `ball::Record` to a stream according to that specification (using
/// `format`).
class RecordJsonFormatter_FieldFormatter {

  public:
    // CREATORS

    /// Destroy this object.
    virtual ~RecordJsonFormatter_FieldFormatter() {};

    // MANIPULATORS

    /// Call the destructor for the most-derived class then deallocate this
    /// object to the specified `allocator`.  Derived-class implementations
    /// of this method should communicate the type of the most-derived
    /// object to the allocator so that the correct size can be computed.
    virtual void deleteSelf(const bsl::allocator<> allocator) = 0;

    /// Format a field of the specified `record` and render it to the
    /// specified `formatter`.  Return 0 on success, and a non-zero value
    /// otherwise.
    virtual
    int format(baljsn::SimpleFormatter *formatter, const Record& record) = 0;

    virtual int parse(bdld::DatumMapRef v) = 0;
       // Parse the specified 'v' datum map and initialize this object with the
       // values retrieved from the map.  Return 0 on success, and a non-zero
       // value otherwise.
};

namespace {

                   // ========================
                   // class TimestampFormatter
                   // ========================

/// This class implements JSON field formatter for the `timestamp` tag.
class TimestampFormatter : public RecordJsonFormatter_FieldFormatter {

    // PRIVATE TYPES
    enum FractionalSecondPrecision {
        // Enumeration used to distinguish among different fractional second
        // precision.
        e_FSP_NONE         = 0,
        e_FSP_MILLISECONDS = 3,
        e_FSP_MICROSECONDS = 6
    };

    enum TimeZone {
        // Enumeration used to distinguish among different time zones.
        e_TZ_LOCAL = 0,
        e_TZ_UTC   = 1
    };

    enum Format {
        // Enumeration used to distinguish among different output formats.
        e_FORMAT_BDE_PRINT = 0,
        e_FORMAT_ISO_8601  = 1
    };

    // DATA
    bsl::string               d_name;
    Format                    d_format;
    TimeZone                  d_timeZone;
    FractionalSecondPrecision d_precision;

  public:
    // TYPES
    typedef bsl::allocator<>  allocator_type;

    // CREATORS

    /// Create the `timestamp` formatter object.  Use the specified
    /// `allocator` (e.g., the address of a `bslma::Allocator` object) to
    /// supply memory.
    explicit TimestampFormatter(const allocator_type& allocator)
    : d_name(k_KEY_TIMESTAMP, allocator)
    , d_format(e_FORMAT_ISO_8601)
    , d_timeZone(e_TZ_UTC)
    , d_precision(e_FSP_MILLISECONDS)
    {}

    // MANIPULATORS

    /// Call the destructor for this object deallocate this object to the
    /// specified `allocator`.
    void deleteSelf(const bsl::allocator<> allocator) BSLS_KEYWORD_OVERRIDE;

    int format(baljsn::SimpleFormatter *formatter, const Record& record)
                                                         BSLS_KEYWORD_OVERRIDE;
        // Format the 'timestamp' field of the specified 'record' and render it
        // to the specified 'formatter'.  Return 0 on success, and a non-zero
        // value otherwise.

    int parse(bdld::DatumMapRef v) BSLS_KEYWORD_OVERRIDE;
       // Parse the specified 'v' datum map and initialize this object with the
       // values retrieved from the map.  Return 0 on success, and a non-zero
       // value otherwise.
};

                   // =======================
                   // class ThreadIdFormatter
                   // =======================

/// This class implements JSON field formatter for the `tid` tag.
class ThreadIdFormatter : public RecordJsonFormatter_FieldFormatter {

    // PRIVATE TYPES
    enum Format {
        // Enumeration used to distinguish among different formats.
        e_DECIMAL     = 0,
        e_HEXADECIMAL = 1
    };

    // DATA
    bsl::string d_name;
    Format      d_format;

  public:
    // TYPES
    typedef bsl::allocator<> allocator_type;

    // CREATORS

    /// Create thread id formatter object.  Use the specified
    /// `allocator` (e.g., the address of a `bslma::Allocator` object) to
    /// supply memory.
    explicit
    ThreadIdFormatter(const allocator_type& allocator)
    : d_name(k_KEY_THREAD_ID, allocator)
    , d_format(e_DECIMAL)
    {}

    // MANIPULATORS

    /// Call the destructor for this object deallocate this object to the
    /// specified `allocator`.
    void deleteSelf(const bsl::allocator<> allocator) BSLS_KEYWORD_OVERRIDE;

    int format(baljsn::SimpleFormatter *formatter, const Record& record)
                                                         BSLS_KEYWORD_OVERRIDE;
        // Format the 'tid' field of the specified 'record' and render it to
        // the specified 'formatter'.  Return 0 on success, and a non-zero
        // value otherwise.

    int parse(bdld::DatumMapRef v) BSLS_KEYWORD_OVERRIDE;
       // Parse the specified 'v' datum map and initialize this object with the
       // values retrieved from the map.  Return 0 on success, and a non-zero
       // value otherwise.
};

                   // =========================
                   // class FixedFieldFormatter
                   // =========================

/// This class implements the field formatter protocol for JSON format
/// tag that has a single `name` attribute (`pid`, line', `category`,
/// `message` and a user-defined attribute).
class FixedFieldFormatter : public RecordJsonFormatter_FieldFormatter {

    // DATA
    bsl::string d_name;

  public:
    // TYPES
    typedef bsl::allocator<>  allocator_type;

    // CREATORS

    /// Create fixed field formatter object.  Use the specified
    /// `allocator` (e.g., the address of a `bslma::Allocator` object) to
    /// supply memory.
    FixedFieldFormatter(const bsl::string&    name,
                        const allocator_type& allocator)
    : d_name(name, allocator)
    {}

    // MANIPULATORS
    int parse(bdld::DatumMapRef v) BSLS_KEYWORD_OVERRIDE;
       // Parse the specified 'v' datum map and initialize this object with the
       // values retrieved from the map.  Return 0 on success, and a non-zero
       // value otherwise.

    // ACCESSORS
    const bsl::string& name() const;
      // Return the name of the log record field or attribute.
};

                   // ========================
                   // class ProcessIdFormatter
                   // ========================

/// This class implements JSON field formatter for the `pid` tag.
class ProcessIdFormatter : public FixedFieldFormatter {

  public:
    // TYPES
    typedef bsl::allocator<> allocator_type;

    // CREATORS

    /// Create process id formatter object.  Use the specified
    /// `allocator` (e.g., the address of a `bslma::Allocator` object) to
    /// supply memory.
    explicit ProcessIdFormatter(const allocator_type& allocator)
    : FixedFieldFormatter(k_KEY_PROCESS_ID, allocator)
    {}

    // MANIPULATORS

    /// Call the destructor for this object deallocate this object to the
    /// specified `allocator`.
    void deleteSelf(const bsl::allocator<> allocator) BSLS_KEYWORD_OVERRIDE;

    int format(baljsn::SimpleFormatter *formatter, const Record& record)
                                                         BSLS_KEYWORD_OVERRIDE;
        // Format the 'pid' field of the specified 'record' and render it to
        // the specified 'formatter'.  Return 0 on success, and a non-zero
        // value otherwise.
};

                   // ===================
                   // class LineFormatter
                   // ===================

/// This class implements JSON field formatter for the `line` tag.
class LineFormatter : public FixedFieldFormatter {

  public:
    // TYPES
    typedef bsl::allocator<>  allocator_type;

    // CREATORS

    /// Create the `line` formatter object.  Use the specified
    /// `allocator` (e.g., the address of a `bslma::Allocator` object) to
    /// supply memory.
    explicit LineFormatter(const allocator_type& allocator)
    : FixedFieldFormatter(k_KEY_LINE, allocator)
    {}

    // MANIPULATORS

    /// Call the destructor for this object deallocate this object to the
    /// specified `allocator`.
    void deleteSelf(const bsl::allocator<> allocator) BSLS_KEYWORD_OVERRIDE;

    int format(baljsn::SimpleFormatter *formatter, const Record& record)
                                                         BSLS_KEYWORD_OVERRIDE;
        // Format the 'line' field of the specified 'record' and render it to
        // the specified 'formatter'.  Return 0 on success, and a non-zero
        // value otherwise.
};

                   // =======================
                   // class CategoryFormatter
                   // =======================

/// This class implements JSON field formatter for the `category` tag.
class CategoryFormatter : public FixedFieldFormatter {

  public:
    // TYPES
    typedef bsl::allocator<> allocator_type;

    // CREATORS

    /// Create the `category` formatter object.  Use the specified
    /// `allocator` (e.g., the address of a `bslma::Allocator` object) to
    /// supply memory.
    explicit CategoryFormatter(const allocator_type& allocator)
    : FixedFieldFormatter(k_KEY_CATEGORY, allocator)
    {}

    // MANIPULATORS

    /// Call the destructor for this object deallocate this object to the
    /// specified `allocator`.
    void deleteSelf(const bsl::allocator<> allocator) BSLS_KEYWORD_OVERRIDE;

    int format(baljsn::SimpleFormatter *formatter, const Record& record)
                                                         BSLS_KEYWORD_OVERRIDE;
        // Format the 'category' field of the specified 'record' and render it
        // to the specified 'formatter'.  Return 0 on success, and a non-zero
        // value otherwise.
};

                   // =======================
                   // class SeverityFormatter
                   // =======================

/// This class implements JSON field formatter for the `severity` tag.
class SeverityFormatter : public FixedFieldFormatter {

  public:
    // TYPES
    typedef bsl::allocator<> allocator_type;

    // CREATORS

    /// Create the `severity` formatter object.  Use the specified
    /// `allocator` (e.g., the address of a `bslma::Allocator` object) to
    /// supply memory.
    explicit SeverityFormatter(const allocator_type& allocator)
    : FixedFieldFormatter(k_KEY_SEVERITY, allocator)
    {}

    // MANIPULATORS

    /// Call the destructor for this object deallocate this object to the
    /// specified `allocator`.
    void deleteSelf(const bsl::allocator<> allocator) BSLS_KEYWORD_OVERRIDE;

    int format(baljsn::SimpleFormatter *formatter, const Record& record)
                                                         BSLS_KEYWORD_OVERRIDE;
        // Format the 'severity' field of the specified 'record' and render it
        // to the specified 'formatter'.  Return 0 on success, and a non-zero
        // value otherwise.
};

                   // ======================
                   // class MessageFormatter
                   // ======================

/// This class implements JSON field formatter for the `message` tag.
class MessageFormatter : public FixedFieldFormatter {

  public:
    // TYPES
    typedef bsl::allocator<> allocator_type;

    // CREATORS

    /// Create the `message` formatter object.  Use the specified
    /// `allocator` (e.g., the address of a `bslma::Allocator` object) to
    /// supply memory.
    explicit MessageFormatter(const allocator_type& allocator)
    : FixedFieldFormatter(k_KEY_MESSAGE, allocator)
    {}

    // MANIPULATORS

    /// Call the destructor for this object deallocate this object to the
    /// specified `allocator`.
    void deleteSelf(const bsl::allocator<> allocator) BSLS_KEYWORD_OVERRIDE;

    int format(baljsn::SimpleFormatter *formatter, const Record& record)
                                                         BSLS_KEYWORD_OVERRIDE;
        // Format the 'messaged' field of the specified 'record' and render it
        // to the specified 'formatter'.  Return 0 on success, and a non-zero
        // value otherwise.
};

                   // ===================
                   // class FileFormatter
                   // ===================

/// This class implements JSON field formatter for the `file` tag.
class FileFormatter : public RecordJsonFormatter_FieldFormatter {

    // PRIVATE TYPES
    enum Path {
        // Enumeration used to distinguish among different file paths.
        e_FILE = 0,
        e_FULL = 1
    };

    // DATA
    bsl::string d_name;
    Path        d_path;

  public:
    // TYPES
    typedef bsl::allocator<> allocator_type;

    // CREATORS

    /// Create the `file` formatter object.  Use the specified `allocator`
    /// (e.g., the address of a `bslma::Allocator` object) to supply memory.
    explicit FileFormatter(const allocator_type& allocator)
    : d_name(k_KEY_FILE, allocator)
    , d_path(e_FULL)
    {}

    // MANIPULATORS

    /// Call the destructor for this object deallocate this object to the
    /// specified `allocator`.
    void deleteSelf(const bsl::allocator<> allocator) BSLS_KEYWORD_OVERRIDE;

    int format(baljsn::SimpleFormatter *formatter, const Record& record)
                                                         BSLS_KEYWORD_OVERRIDE;
        // Format the 'file' field of the specified 'record' and render it to
        // the specified 'formatter'.  Return 0 on success, and a non-zero
        // value otherwise.

    int parse(bdld::DatumMapRef v) BSLS_KEYWORD_OVERRIDE;
       // Parse the specified 'v' datum map and initialize this object with the
       // values retrieved from the map.  Return 0 on success, and a non-zero
       // value otherwise.
};

                       // ========================
                       // class AttributeFormatter
                       // ========================

/// This class implements JSON field formatter for a user-defined attribute.
class AttributeFormatter : public RecordJsonFormatter_FieldFormatter {

    // PRIVATE TYPES
    enum { k_UNSET = -1 };  // Unspecified index

    // DATA
    bsl::string d_key;    // attribute's key
    int         d_index;  // cached attribute's index

  public:
    // TYPES
    typedef bsl::allocator<> allocator_type;

    // CREATORS
    AttributeFormatter(const bsl::string_view& key,
                       const allocator_type&   allocator)
       // Create an attribute formatter object having the specified 'key' of an
       // attribute to be rendered.  Use the specified 'allocator' (e.g., the
        // address of a 'bslma::Allocator' object) to supply memory.
    : d_key(key, allocator)
    , d_index(k_UNSET)
    {}

    // MANIPULATORS

    /// Call the destructor for this object deallocate this object to the
    /// specified `allocator`.
    void deleteSelf(const bsl::allocator<> allocator) BSLS_KEYWORD_OVERRIDE;

    int format(baljsn::SimpleFormatter *formatter, const Record& record)
                                                         BSLS_KEYWORD_OVERRIDE;
        // Render an attribute having the key supplied at construction of this
        // object and provided by the specified 'record' to the specified
        // 'formatter'.  Return 0 on success, and a non-zero value otherwise.

    int parse(bdld::DatumMapRef v) BSLS_KEYWORD_OVERRIDE;
       // Parse the specified 'v' datum map and initialize this object with the
       // values retrieved from the map.  Return 0 on success, and a non-zero
       // value otherwise.

    // ACCESSORS
    const bsl::string& key() const;
      // Return the key the user-defined attribute.
};

                       // =========================
                       // class AttributesFormatter
                       // =========================

/// This class implements JSON field formatter for the `attributes` tag.
class AttributesFormatter : public RecordJsonFormatter_FieldFormatter {

  public:
    // TYPES

    /// `SkipAttributes` is an alias for a set of names of attributes that
    /// should not be printed as part of the `attributes` format tag,
    /// because they are printed as individual user-defined attributes.
    typedef bsl::set<bsl::string_view>                 SkipAttributes;

    /// Allocator type used to provide memory.
    typedef SkipAttributes::allocator_type             allocator_type;

    /// `SkipAttributesSp` is an alias for the shared pointer to the
    /// `SkipAttributes` type.
    typedef bsl::shared_ptr<SkipAttributes>            SkipAttributesSp;

  private:
    // PRIVATE TYPES

    /// `Attributes` is an alias for the vector of `ball::ManagedAttribute`
    /// objects.
    typedef bsl::vector<ball::ManagedAttribute>        Attributes;

    /// `AttributeCache` is an alias for a vector of pairs of an attribute's
    /// key and a flag indicating whether the attribute should be displayed
    /// or not.
    typedef bsl::vector<bsl::pair<bsl::string, bool> > AttributeCache;

    // DATA
    SkipAttributesSp      d_skipAttributes_sp;
    AttributeCache        d_cache;                // cached attributes

  public:
    // CREATORS

    /// Create an attribute formatter object having the specified
    /// `skipAttributesSp` collection.  Optionally specify an `allocator` to
    /// provide memory.  The behavior is undefined unless `allocator` and
    /// `skipAttributesSp->get_allocator()` compare equal.
    explicit AttributesFormatter(const SkipAttributesSp& skipAttributesSp,
                                 const allocator_type&   allocator =
                                                             allocator_type());

    // MANIPULATORS

    /// Call the destructor for this object deallocate this object to the
    /// specified `allocator`.
    void deleteSelf(const bsl::allocator<> allocator) BSLS_KEYWORD_OVERRIDE;

    int format(baljsn::SimpleFormatter *formatter, const Record& record)
                                                         BSLS_KEYWORD_OVERRIDE;
        // Render all user attributes in the specified 'record' except
        // attributes whose keys are listed in the collection supplied at
        // construction of this object to the specified 'formatter'.  Return 0
        // on success, and a non-zero value otherwise.

    int parse(bdld::DatumMapRef v) BSLS_KEYWORD_OVERRIDE;
       // Return 0, with no other effect.  The specified 'v' map is ignored
       // (and should be empty).  Note that the "attributes" element in the
       // format specification JSON, has no behavioral properties to customize.
};

                    // =================
                    // class DatumParser
                    // =================

/// This class implements a parser that transforms the `bdld::Datum` object
/// into a collection of the `RecordJsonFormatter_FieldFormatter` objects.
class DatumParser {

    // PRIVATE TYPES

    /// `FieldFormatter` is an alias for the
    /// `RecordJsonFormatter_FieldFormatter` type.
    typedef RecordJsonFormatter_FieldFormatter FieldFormatter;

    /// `FieldFormatters` is an alias for the
    /// `RecordJsonFormatter::FieldFormatters` type.
    typedef RecordJsonFormatter::FieldFormatters  FieldFormatters;

    /// `SkipAttributesSp` is an alias for the
    /// `AttributesFormatter::SkipAttributesSp` type.
    typedef AttributesFormatter::SkipAttributesSp SkipAttributesSp;

    /// `SkipAttributesSp` is an alias for the shared pointer to the
    /// `SkipAttributes` type.
    typedef SkipAttributesSp::element_type        SkipAttributes;

    // DATA
    SkipAttributesSp d_skipAttributes_sp;
    bsl::allocator<> d_allocator;

    // CLASS METHODS

    /// Create a field formatter by the specified `v` value.
    RecordJsonFormatter_FieldFormatter *make(const bslstl::StringRef&  v);
    RecordJsonFormatter_FieldFormatter *make(bdld::DatumMapRef         v);

  public:
    // TYPES
    typedef bsl::allocator<> allocator_type;

    // TRAITS
    BSLMF_NESTED_TRAIT_DECLARATION(DatumParser, bslma::UsesBslmaAllocator);

    // CREATORS

    /// Create a datum parser object having the specified `allocator` (e.g.,
    /// the address of a `bslma::Allocator` object) to supply memory.
    explicit DatumParser(const allocator_type& allocator)
    : d_skipAttributes_sp()
    , d_allocator(allocator)
    {}

    /// Parse the specified `v` datum and load the specified `formatters`
    /// with the newly created field formatters supplied with the specified
    /// `skipAttributesPtr` reference to the skip attribute container.
    int parse(FieldFormatters *formatters, const bdld::Datum&  v);
};

                       // ================
                       // class FormatUtil
                       // ================

/// This "struct" provides a namespace for utility functions that format
/// values of various types to JSON.
struct FormatUtil {

    // CLASS METHODS

    /// Add the specified `attribute` having the specified `name` to
    /// `formatter`.
    static int formatAttribute(baljsn::SimpleFormatter *formatter,
                               const ManagedAttribute&  attribute,
                               const bsl::string_view&  name);
};

                        // ===============================
                        // class FieldFormattersDestructor
                        // ===============================

/// This class provides a guard to automatically destroy items of field
/// container supplied at construction of an object of this class.
class FieldFormattersDestructor {

    // DATA
    RecordJsonFormatter::FieldFormatters *d_formatters_p;  // field formatters

  public:
    // CREATORS

    /// Create a field formatter destructor having the specified
    /// `formattersPtr` reference to a container whose elements will be
    /// destroyed when this object is destroyed.
    FieldFormattersDestructor(
                           RecordJsonFormatter::FieldFormatters *formattersPtr)
    : d_formatters_p(formattersPtr)
    {
    }

    /// Destroy this object and destroy all field formatters in the
    /// container supplied to this object at construction.
    ~FieldFormattersDestructor()
    {
        if (d_formatters_p) {
            typedef RecordJsonFormatter::FieldFormatters::iterator iterator;
            for (iterator it  = d_formatters_p->begin();
                          it != d_formatters_p->end();
                        ++it)
            {
                (*it)->deleteSelf(d_formatters_p->get_allocator());
            }
        }
    }
};

                   // ------------------------
                   // class TimestampFormatter
                   // ------------------------

// MANIPULATORS
void TimestampFormatter::deleteSelf(const bsl::allocator<> allocator)
{
    AllocUtil::deleteObject(allocator, this);
}

int TimestampFormatter::format(baljsn::SimpleFormatter *formatter,
                               const Record&            record)
{
    bdlt::DatetimeInterval  offset;

    if (e_TZ_LOCAL == d_timeZone) {
        bsls::Types::Int64 localTimeOffsetInSeconds =
            bdlt::LocalTimeOffset::localTimeOffset(
                              record.fixedFields().timestamp()).totalSeconds();

        offset.setTotalSeconds(localTimeOffsetInSeconds);
    }
    bdlt::DatetimeTz timestamp(record.fixedFields().timestamp() + offset,
                               static_cast<int>(offset.totalMinutes()));

    switch (d_format) {
      case e_FORMAT_ISO_8601: {
        bdlt::Iso8601UtilConfiguration config;

        config.setFractionalSecondPrecision(d_precision);
        config.setUseZAbbreviationForUtc(true);

        char buffer[bdlt::Iso8601Util::k_DATETIMETZ_STRLEN + 1];

        int outputLength = bdlt::Iso8601Util::generateRaw(buffer,
                                                          timestamp,
                                                          config);
        int rc = formatter->addValue(d_name,
                                     bsl::string_view(buffer, outputLength));
        if (rc) {
            return rc;                                                // RETURN
        }
      } break;
      case e_FORMAT_BDE_PRINT: {
        char buffer[32];

        timestamp.localDatetime().printToBuffer(buffer,
                                                sizeof buffer,
                                                d_precision);

        int rc = formatter->addValue(d_name, buffer);
        if (rc) {
            return rc;                                                // RETURN
        }
      } break;
      default: {
          BSLS_ASSERT(0 == "Unexpected timestamp format");
          return -1;                                                  // RETURN
      }
    }

    return 0;
}

int TimestampFormatter::parse(bdld::DatumMapRef v)
{
    for (bdld::Datum::SizeType i = 0; i < v.size(); ++ i) {
        if (false == v[i].value().isString()) {
            return -1;                                                // RETURN
        }
        const bslstl::StringRef& value = v[i].value().theString();
        if (k_KEY_NAME == v[i].key()) {
            d_name = value;
        }
        else if (k_KEY_PRECISION == v[i].key()) {
            if (k_VALUE_PRECISION_NONE == value) {
                d_precision = e_FSP_NONE;
            }
            else if (k_VALUE_PRECISION_MS == value) {
                d_precision = e_FSP_MILLISECONDS;
            }
            else if (k_VALUE_PRECISION_US == value) {
                d_precision = e_FSP_MICROSECONDS;
            }
            else {
                return -1;                                            // RETURN
            }
        }
        else if (k_KEY_TIME_ZONE == v[i].key()) {
            if (k_VALUE_LOCAL == value) {
                d_timeZone = e_TZ_LOCAL;
            }
            else if (k_VALUE_UTC == value) {
                d_timeZone = e_TZ_UTC;
            }
            else {
                return -1;                                            // RETURN
            }
        }
        else if (k_KEY_FORMAT == v[i].key()) {
            if (k_VALUE_BDE_PRINT == value) {
                d_format = e_FORMAT_BDE_PRINT;
            } else if (k_VALUE_ISO_8601 == value) {
                d_format = e_FORMAT_ISO_8601;
            }
            else {
                return -1;                                            // RETURN
            }
        }
    }
    return 0;
}

                   // -----------------------
                   // class ThreadIdFormatter
                   // -----------------------

// MANIPULATORS
void ThreadIdFormatter::deleteSelf(const bsl::allocator<> allocator)
{
    AllocUtil::deleteObject(allocator, this);
}

int ThreadIdFormatter::format(baljsn::SimpleFormatter *formatter,
                              const Record&            record)
{
    int rc = 0;
    switch (d_format) {
      case e_DECIMAL: {
        rc = formatter->addValue(d_name, record.fixedFields().threadID());
      } break;
      case e_HEXADECIMAL: {
        char buffer[32];

#if defined(BSLS_PLATFORM_CMP_MSVC)
#define snprintf _snprintf
#endif

        snprintf(buffer,
                 sizeof(buffer),
                 "%llX",
                 record.fixedFields().threadID());

#if defined(BSLS_PLATFORM_CMP_MSVC)
#undef snprintf
#endif
        rc = formatter->addValue(d_name, &buffer[0]);
      } break;
      default: {
          BSLS_ASSERT(0 == "Unexpected thread format");
          return -1;                                                  // RETURN
      }
    }

    return rc;
}

int ThreadIdFormatter::parse(bdld::DatumMapRef v)
{
    for (bdld::Datum::SizeType i = 0; i < v.size(); ++i) {
        if (false == v[i].value().isString()) {
            return -1;                                                // RETURN
        }
        const bslstl::StringRef& value = v[i].value().theString();
        if (k_KEY_NAME == v[i].key()) {
            d_name = value;
        }
        else if (k_KEY_FORMAT == v[i].key()) {
            if (k_VALUE_DECIMAL == value) {
                d_format = e_DECIMAL;
            } else if (k_VALUE_HEXADECIMAL == value) {
                d_format = e_HEXADECIMAL;
            }
            else {
                return -1;                                            // RETURN
            }
        }
    }
    return 0;
}

                   // -------------------------
                   // class FixedFieldFormatter
                   // -------------------------

// MANIPULATORS
(__out == 0 ==> std::all_of(v.begin(), v.end(), [](const bdld::DatumPair& p){ return p.value().isString(); })) && (__out == -1 ==> std::any_of(v.begin(), v.end(), [](const bdld::DatumPair& p){ return !p.value().isString(); }))
int FixedFieldFormatter::parse(bdld::DatumMapRef v)
{
    for (bdld::Datum::SizeType i = 0; i < v.size(); ++ i) {
        if (false == v[i].value().isString()) {
            return -1;                                                // RETURN
        }
        if (k_KEY_NAME == v[i].key()) {
            d_name = v[i].value().theString();
        }
    }
    return 0;
}

// ACCESSORS
inline
const bsl::string& FixedFieldFormatter::name() const
{
    return d_name;
}

                   // ------------------------
                   // class ProcessIdFormatter
                   // ------------------------

// MANIPULATORS
void ProcessIdFormatter::deleteSelf(const bsl::allocator<> allocator)
{
    AllocUtil::deleteObject(allocator, this);
}

int ProcessIdFormatter::format(baljsn::SimpleFormatter *formatter,
                               const Record&            record)
{
    return formatter->addValue(name(), record.fixedFields().processID());
}

                   // -------------------
                   // class LineFormatter
                   // -------------------

// MANIPULATORS
void LineFormatter::deleteSelf(const bsl::allocator<> allocator)
{
    AllocUtil::deleteObject(allocator, this);
}

int LineFormatter::format(baljsn::SimpleFormatter *formatter,
                          const Record&            record)
{
    return formatter->addValue(name(), record.fixedFields().lineNumber());
}

                   // -----------------------
                   // class CategoryFormatter
                   // -----------------------

// MANIPULATORS
void CategoryFormatter::deleteSelf(const bsl::allocator<> allocator)
{
    AllocUtil::deleteObject(allocator, this);
}

int CategoryFormatter::format(baljsn::SimpleFormatter *formatter,
                              const Record&            record)
{
    return formatter->addValue(name(), record.fixedFields().category());
}

                   // -----------------------
                   // class SeverityFormatter
                   // -----------------------

void SeverityFormatter::deleteSelf(const bsl::allocator<> allocator)
{
    AllocUtil::deleteObject(allocator, this);
}

int SeverityFormatter::format(baljsn::SimpleFormatter *formatter,
                              const Record&            record)
{
    return formatter->addValue(name(),
                      Severity::toAscii(
                          static_cast<Severity::Level>(
                                            record.fixedFields().severity())));
}

                   // ----------------------
                   // class MessageFormatter
                   // ----------------------

// MANIPULATORS
void MessageFormatter::deleteSelf(const bsl::allocator<> allocator)
{
    AllocUtil::deleteObject(allocator, this);
}

int MessageFormatter::format(baljsn::SimpleFormatter *formatter,
                             const Record&            record)
{
    return formatter->addValue(name(), record.fixedFields().messageRef());
}

                   // -------------------
                   // class FileFormatter
                   // -------------------

// MANIPULATORS
void FileFormatter::deleteSelf(const bsl::allocator<> allocator)
{
    AllocUtil::deleteObject(allocator, this);
}

int FileFormatter::format(baljsn::SimpleFormatter *formatter,
                          const Record&            record)
{
    switch (d_path) {
      case e_FULL: {
        if (0 != formatter->addValue(d_name, record.fixedFields().fileName()))
        {
            return -1;                                                // RETURN
        }
      } break;
      case e_FILE: {
        const bsl::string_view filename(record.fixedFields().fileName());
        bsl::string basename;
        int rc = bdls::PathUtil::getBasename(&basename, filename);

        if (formatter->addValue(d_name, 0 == rc ? basename : filename))
        {
            return -1;                                                // RETURN
        }
      } break;
      default: {
          BSLS_ASSERT(0 == "Unexpected file format");
      }
    }
    return 0;
}

int FileFormatter::parse(bdld::DatumMapRef v)
{
    for (bdld::Datum::SizeType i = 0; i < v.size(); ++i) {
        if (false == v[i].value().isString()) {
            return -1;                                                // RETURN
        }
        const bslstl::StringRef& value = v[i].value().theString();
        if (k_KEY_NAME == v[i].key()) {
            d_name = value;
        }
        else if (k_KEY_PATH == v[i].key()) {
            if (k_VALUE_FULL == value) {
                d_path = e_FULL;
            } else if (k_VALUE_FILE == value) {
                d_path = e_FILE;
            }
            else {
                return -1;                                            // RETURN
            }
        }
    }
    return 0;
}
                       // ------------------------
                       // class AttributeFormatter
                       // ------------------------

// MANIPULATORS
void AttributeFormatter::deleteSelf(const bsl::allocator<> allocator)
{
    AllocUtil::deleteObject(allocator, this);
}

int AttributeFormatter::format(baljsn::SimpleFormatter *formatter,
                               const Record&            record)
{
    typedef bsl::vector<ball::ManagedAttribute> Attributes;

    const Attributes& attributes = record.attributes();
    if (k_UNSET == d_index ||
        d_index >= static_cast<int>(attributes.size()) ||
        d_key   != attributes[d_index].key())
    {
        d_index = k_UNSET;
        for (Attributes::const_iterator i = attributes.begin();
             i != attributes.end();
             ++i)
        {
            if (d_key == i->key()) {
                d_index = static_cast<int>(bsl::distance(attributes.begin(),
                                                         i));
                break;                                                 // BREAK
            }
        }
        if (k_UNSET == d_index) {
            return formatter->addValue(d_key, "N/A");                 // RETURN
        }
    }

    return FormatUtil::formatAttribute(formatter,
                                       attributes.at(d_index),
                                       d_key);
}

int AttributeFormatter::parse(bdld::DatumMapRef v)
{
    for (bdld::Datum::SizeType i = 0; i < v.size(); ++ i) {
        if (false == v[i].value().isString()) {
            return -1;                                                // RETURN
        }
        if (k_KEY_NAME == v[i].key()) {
            d_key = v[i].value().theString();
        }
    }
    return 0;
}

// ACCESSORS
&__out == &d_key
const bsl::string& AttributeFormatter::key() const
{
    return d_key;
}

                       // -------------------------
                       // class AttributesFormatter
                       // -------------------------

// CREATORS
inline
AttributesFormatter::AttributesFormatter(
                                      const SkipAttributesSp& skipAttributesSp,
                                      const allocator_type&   allocator)
    : d_skipAttributes_sp(skipAttributesSp)
    , d_cache(allocator)
{
    BSLS_ASSERT(allocator == skipAttributesSp->get_allocator());
}

// MANIPULATORS
inline
void AttributesFormatter::deleteSelf(const bsl::allocator<> allocator)
{
    AllocUtil::deleteObject(allocator, this);
}

int AttributesFormatter::format(baljsn::SimpleFormatter *formatter,
                                const Record&            record)
{
    const Attributes& attributes = record.attributes();

    for (Attributes::size_type i = 0; i < attributes.size(); ++i) {
        const ManagedAttribute& a = attributes[i];

        if (i < d_cache.size()) {
            if (d_cache[i].first != a.key()) {
                d_cache[i].first  = a.key();
                d_cache[i].second = d_skipAttributes_sp->end() ==
                                    d_skipAttributes_sp->find(a.key());
            }
        }
        else {
            d_cache.emplace_back(
                bsl::make_pair(a.key(),
                               d_skipAttributes_sp->end() ==
                               d_skipAttributes_sp->find(a.key())));
        }
        if (d_cache[i].second) {
            FormatUtil::formatAttribute(formatter, a, a.key());
        }
    }
    return 0;
}

inline
int AttributesFormatter::parse(bdld::DatumMapRef v)
{
    (void) v;
    BSLS_ASSERT(0 == v.size());
    return 0;
}

                       // -----------------
                       // class DatumParser
                       // -----------------

// CLASS METHODS
__out != NULL
RecordJsonFormatter_FieldFormatter *
DatumParser::make(const bslstl::StringRef& v)
{
    RecordJsonFormatter_FieldFormatter *formatter = 0;

    if (k_KEY_TIMESTAMP       == v) {
        formatter = AllocUtil::newObject<TimestampFormatter>(d_allocator);
    }
    else if (k_KEY_PROCESS_ID == v) {
        formatter = AllocUtil::newObject<ProcessIdFormatter>(d_allocator);
    }
    else if (k_KEY_THREAD_ID  == v) {
        formatter = AllocUtil::newObject<ThreadIdFormatter>(d_allocator);
    }
    else if (k_KEY_SEVERITY   == v) {
        formatter = AllocUtil::newObject<SeverityFormatter>(d_allocator);
    }
    else if (k_KEY_FILE       == v) {
        formatter = AllocUtil::newObject<FileFormatter>(d_allocator);
    }
    else if (k_KEY_LINE       == v) {
        formatter = AllocUtil::newObject<LineFormatter>(d_allocator);
    }
    else if (k_KEY_CATEGORY   == v) {
        formatter = AllocUtil::newObject<CategoryFormatter>(d_allocator);
    }
    else if (k_KEY_MESSAGE    == v) {
        formatter = AllocUtil::newObject<MessageFormatter>(d_allocator);
    }
    else if (k_KEY_ATTRIBUTES == v) {
        if (!d_skipAttributes_sp) {
            d_skipAttributes_sp = bsl::allocate_shared<SkipAttributes>(
                                                                  d_allocator);
        }
        formatter = AllocUtil::newObject<AttributesFormatter>(d_allocator,
                                                          d_skipAttributes_sp);
    }
    else {
        if (!d_skipAttributes_sp) {
            d_skipAttributes_sp = bsl::allocate_shared<SkipAttributes>(
                                                                  d_allocator);
        }

        bslma::ManagedPtr<AttributeFormatter> formatter_mp =
            bslma::ManagedPtrUtil::allocateManaged<AttributeFormatter>(
                                                   d_allocator.mechanism(), v);

        if (d_skipAttributes_sp->end() == d_skipAttributes_sp->find(v)) {
            d_skipAttributes_sp->emplace(formatter_mp->key());
        }

        return formatter_mp.release().first;                          // RETURN
    }

    return formatter;
}

RecordJsonFormatter_FieldFormatter *DatumParser::make(bdld::DatumMapRef  v)
{
    bslma::ManagedPtr<FieldFormatter> formatter_mp;

    if (1 == v.size() && v[0].value().isMap()) {
        formatter_mp.load(make(v[0].key()), d_allocator.mechanism());
        if (formatter_mp && 0 != formatter_mp->parse(v[0].value().theMap())) {
            formatter_mp.reset();
        }
    }
    return formatter_mp.release().first;
}

int DatumParser::parse(FieldFormatters *formatters, const bdld::Datum&  v)
{

    if (false == v.isArray()) {
        return -1;                                                    // RETURN
    }

    bdld::DatumArrayRef array = v.theArray();

    for (bdld::Datum::SizeType i = 0; i < array.length(); ++i) {
        const bdld::Datum&                v = array[i];
        bslma::ManagedPtr<FieldFormatter> formatter_mp;

        if (v.isString()) {
            formatter_mp.load(make(v.theString()), d_allocator.mechanism());
        }
        else if (v.isMap()) {
            formatter_mp.load(make(v.theMap()), d_allocator.mechanism());
        }
        else {
            return -1;                                                // RETURN
        }

        if (formatter_mp) {
            formatters->push_back(formatter_mp.get());
            formatter_mp.release();
        }
    }
    return 0;
}

                       // ----------------
                       // class FormatUtil
                       // ----------------

int FormatUtil::formatAttribute(baljsn::SimpleFormatter *formatter,
                                const ManagedAttribute&  attribute,
                                const bsl::string_view&  name)
{
    if (attribute.value().is<bsl::string>()) {
        return formatter->addValue(name, attribute.value().the<bsl::string>());
                                                                      // RETURN
    }
    else if (attribute.value().is<int>()) {
        return formatter->addValue(name, attribute.value().the<int>());
                                                                      // RETURN
    }
    else if (attribute.value().is<long>()) {
        return formatter->addValue(name,
                                   static_cast<long long>(
                                       attribute.value().the<long>()));
                                                                      // RETURN
    }
    else if (attribute.value().is<long long>()) {
        return formatter->addValue(name,
                                   attribute.value().the<long long>());
                                                                      // RETURN
    }
    else if (attribute.value().is<unsigned int>()) {
        return formatter->addValue(name,
                                   attribute.value().the<unsigned int>());
                                                                      // RETURN
    }
    else if (attribute.value().is<unsigned long>()) {
        return formatter->addValue(name,
                                   static_cast<unsigned long long>(
                                      attribute.value().the<unsigned long>()));
                                                                      // RETURN
    }
    else if (attribute.value().is<unsigned long long>()) {
        return formatter->addValue(
                                  name,
                                  attribute.value().the<unsigned long long>());
                                                                      // RETURN
    }
    else if (attribute.value().is<const void *>()) {

        const int                   k_STORAGE_SIZE = 32;
        char                        storage[k_STORAGE_SIZE] = { 0 };
        bdlsb::FixedMemOutStreamBuf buffer(storage, k_STORAGE_SIZE - 1);
        bsl::ostream                stream(&buffer);
        bslim::Printer              printer(&stream, 0, -1);

        printer.printHexAddr(attribute.value().the<const void *>(), 0);

        return formatter->addValue(name, &storage[1]);                // RETURN
    }
    return -1;
}

}  // close unnamed namespace within BloombergLP::ball

                        // -------------------------
                        // class RecordJsonFormatter
                        // -------------------------

// PRIVATE MANIPULATORS
void RecordJsonFormatter::releaseFieldFormatters(
                                                FieldFormatters *formattersPtr)
{
    FieldFormattersDestructor destructor(formattersPtr);
}

// CREATORS
RecordJsonFormatter::RecordJsonFormatter(const allocator_type& allocator)
: d_formatSpec(allocator)
, d_recordSeparator("\n")
, d_fieldFormatters(allocator)
{
    static const bsl::string_view k_DEFAULT_FORMAT_SPEC = getDefaultFormat();

    int rc = setFormat(k_DEFAULT_FORMAT_SPEC);
    (void) rc;
    BSLS_ASSERT(0 == rc);
}

RecordJsonFormatter::~RecordJsonFormatter()
{
    releaseFieldFormatters(&d_fieldFormatters);
}

// MANIPULATORS
(format.empty() ==> __out == -1)
int RecordJsonFormatter::setFormat(const bsl::string_view& format)
{
    if (format.empty()) {
        return -1;                                                    // RETURN
    }

    bdld::ManagedDatum datum;
    int rc = baljsn::DatumUtil::decode(&datum, format);
    if (0 != rc) {
        return rc;                                                    // RETURN
    }

    DatumParser               parser(get_allocator());
    FieldFormatters           formatters(get_allocator());
    FieldFormattersDestructor destructor(&formatters);

    rc = parser.parse(&formatters, datum.datum());
    if (0 == rc) {
        d_formatSpec = format;
        d_fieldFormatters.swap(formatters);
    }

    return rc;
}

RecordJsonFormatter& RecordJsonFormatter::operator=(
                                                const RecordJsonFormatter& rhs)
{
    if (this != &rhs) {
        int rc = setFormat(rhs.d_formatSpec);
        (void) rc;
        BSLS_ASSERT(0 == rc);
        d_recordSeparator = rhs.d_recordSeparator;
    }

    return *this;
}

// ACCESSORS
void RecordJsonFormatter::operator()(bsl::ostream& stream,
                                     const Record& record) const
{
    baljsn::SimpleFormatter formatter(stream);
    int rc;
    formatter.openObject();

    for (FieldFormatters::const_iterator it = d_fieldFormatters.cbegin();
         it != d_fieldFormatters.cend();
         ++it)
    {
        rc = (*it)->format(&formatter, record);
        if (rc) {
            stream << "Error: JSON encoding failure.";
            break;                                                     // BREAK
        }
    }

    formatter.closeObject();
    stream << d_recordSeparator;
    stream.flush();

    return;
}

}  // close package namespace
}  // close enterprise namespace

// ----------------------------------------------------------------------------
// Copyright 2020 Bloomberg Finance L.P.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ----------------------------- END-OF-FILE ----------------------------------
