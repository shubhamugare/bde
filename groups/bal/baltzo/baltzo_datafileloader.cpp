// baltzo_datafileloader.cpp                                          -*-C++-*-
#include <baltzo_datafileloader.h>

#include <bsls_ident.h>
BSLS_IDENT_RCSID(baltzo_datafileloader_cpp,"$Id$ $CSID$")

#include <baltzo_errorcode.h>
#include <baltzo_zoneinfo.h>
#include <baltzo_zoneinfobinaryreader.h>

#include <bdls_filesystemutil.h>
#include <bdls_pathutil.h>

#include <bdlma_bufferedsequentialallocator.h>

#include <bdlb_tokenizer.h>

#include <bslstl_stringref.h>

#include <bslmf_assert.h>
#include <bslmf_issame.h>

#include <bsls_assert.h>
#include <bsls_log.h>
#include <bsls_platform.h>

#include <bsl_cstring.h>
#include <bsl_fstream.h>
#include <bsl_ios.h>
#include <bsl_ostream.h>

namespace {
namespace u {

using namespace BloombergLP;

const char *INVALID_PATH    = "! INVALID_FILE_PATH !";

const int UNSPECIFIED_ERROR = -1;
const int UNSUPPORTED_ID    = baltzo::ErrorCode::k_UNSUPPORTED_ID;

BSLMF_ASSERT(u::UNSUPPORTED_ID != u::UNSPECIFIED_ERROR);

template <class STRING>
struct IsString {
    static const bool value = bsl::is_same<STRING, bsl::string>::value
                           || bsl::is_same<STRING, std::string>::value
#ifdef BSLS_LIBRARYFEATURES_HAS_CPP17_PMR_STRING
                           || bsl::is_same<STRING, std::pmr::string>::value
#endif
    ;
};

enum ZoneinfoReadMode {
    // Enumerate the set of modes that can be used for reading binary data.

    k_READ_RAW,        // Return an unadjusted raw binary data from the data
                       // file (this may not be a "well-formed" Zoneinfo (see
                       // 'baltzo_zoneinfoutil')

    k_READ_NORMALIZED  // Adjust the raw binary data to produce a Zoneinfo
                       // satisfying the first two requirements for a
                       // "well-formed" object by inserting a sentinel
                       // transition at 01-01-001 (see
                       // 'baltzo::ZoneinfoUtil::isWellFormed' documentation).
                       // At the moment, this means adding a sentinel
                       // transition at 01-01-0001 (the raw file format may
                       // place such a sentinel transition before the first
                       // representable 'Datetime' object)
};

// HELPER FUNCTIONS

/// Load, into the specified `result`, the system independent path of the
/// specified `timeZoneId` appended to the specified `rootPath`.
template <class STRING>
void concatenatePath(STRING             *result,
                     const bsl::string&  rootPath,
                     const char         *timeZoneId)
{
    BSLS_ASSERT(result);
    BSLS_ASSERT(timeZoneId);

    BSLMF_ASSERT(u::IsString<STRING>::value);

    *result = rootPath;
    for (bdlb::Tokenizer it(bslstl::StringRef(timeZoneId),
                            bslstl::StringRef("/"));
         it.isValid();
         ++it) {
        bdls::PathUtil::appendIfValid(result, it.token());
    }
}

/// Returns 0 if the specified `timeZoneId` contains only valid characters
/// and does not start with `/`, and a non-zero value otherwise.
 res_tmp == 0 || res_tmp == -1 || res_tmp == -2
int validateTimeZoneId(const char *timeZoneId)
{
    BSLS_ASSERT(timeZoneId);

    if ('/' == timeZoneId[0]) {
        return -1;                                                    // RETURN
    }

    const char *VALID_CHAR = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                             "abcdefghijklmnopqrstuvwxyz"
                             "1234567890/_+-";

    for (const char *pc = timeZoneId; *pc; ++pc) {
        if (!bsl::strchr(VALID_CHAR, *pc)) {
            return -2;                                                // RETURN
        }
    }

    return 0;
}

/// Load into the specified `result` the file-system path to the Zoneinfo
/// binary data file corresponding to the specified `timeZoneId` relative to
/// the configured `rootPath`.  Return 0 on success, and a non-zero value
/// otherwise.  On error, `result` is left in a valid, but unspecified
/// state.  The behavior is undefined unless either `configureRootPath` or
/// `configureRootPathIfValid` has been called successfully.  Note that this
/// operation does not verify `result` refers to a valid file on the file
/// system, or whether the file (if it exists) contains valid Zoneinfo data.
template <class STRING>
inline
int loadTimeZoneFilePath_Impl(STRING             *result,
                              const char         *timeZoneId,
                              const bsl::string&  rootPath)
{
    BSLS_ASSERT(result);
    BSLS_ASSERT(timeZoneId);
    BSLS_ASSERT(u::INVALID_PATH != rootPath);

    BSLMF_ASSERT(u::IsString<STRING>::value);

    const int rc = u::validateTimeZoneId(timeZoneId);
    if (0 != rc) {
        return rc;                                                    // RETURN
    }

    u::concatenatePath(result, rootPath, timeZoneId);

    return 0;
}

/// Load into the specified `result` the time-zone information for the time
/// zone identified by the specified `timeZoneId` in accordance with the
/// specified `mode`.  Return 0 on success, and a non-zero value otherwise.
/// A return status of `ErrorCode::k_UNSUPPORTED_ID` indicates that
/// `timeZoneId` is not recognized.  If an error occurs during this
/// operation, `result` will be left in a valid, but unspecified state.
 true
int loadTimeZoneImpl(baltzo::Zoneinfo              *result,
                     const baltzo::DataFileLoader&  loader,
                     const char                    *timeZoneId,
                     ZoneinfoReadMode               mode)
{
    BSLS_ASSERT(result);
    BSLS_ASSERT(timeZoneId);

    bsl::string path;
    const int rc = loader.loadTimeZoneFilePath(&path, timeZoneId);
    if (0 != rc) {
        BSLS_LOG_ERROR("Poorly formed time-zone identifier '%s'", timeZoneId);
        return u::UNSUPPORTED_ID;                                     // RETURN
    }

    // Create a file stream for the olson data file.

    bsl::ifstream infile(path.c_str(),
                         bsl::ifstream::binary | bsl::ifstream::in);

    if (!infile.is_open()) {
        // Failed to open the data file for 'timeZoneId'.  If the data-file
        // loader is correctly configured, 'timeZoneId' is not a supported
        // time-zone identifier, so return 'k_UNSUPPORTED_ID'.  Otherwise, if
        // the data-file loader is not correctly configured, return
        // 'UNSPECIFIED_ERROR' (different from 'k_UNSUPPORTED_ID').

        BSLS_LOG_ERROR("Failed to open time-zone information file '%s'",
                       path.c_str());

        return loader.isRootPathPlausible()
               ? u::UNSUPPORTED_ID
               : u::UNSPECIFIED_ERROR;                                // RETURN
    }

    result->setIdentifier(timeZoneId);

    if (k_READ_RAW == mode) {
        return baltzo::ZoneinfoBinaryReader::readRaw(result, infile); // RETURN
    }

    return baltzo::ZoneinfoBinaryReader::read(result, infile);
}

}  // close namespace u
}  // close unnamed namespace

namespace BloombergLP {
namespace baltzo {

                            // --------------------
                            // class DataFileLoader
                            // --------------------

// CLASS METHODS
 true
bool DataFileLoader::isPlausibleZoneinfoRootPath(const char *path)
{
    BSLS_ASSERT(path);

    if (!bdls::FilesystemUtil::isDirectory(path, true)) {
        return false;                                                 // RETURN
    }

    bsl::string gmtPath;
    u::concatenatePath(&gmtPath, path, "GMT");

    // If 'path' is a valid directory, appending "GMT" to it must result in a
    // valid path.

    return bdls::FilesystemUtil::isRegularFile(gmtPath.c_str(), true);
}

// CREATORS
DataFileLoader::DataFileLoader()
: d_rootPath(u::INVALID_PATH)
{
}

DataFileLoader::DataFileLoader(const allocator_type& allocator)
: d_rootPath(u::INVALID_PATH, allocator)
{
}

DataFileLoader::~DataFileLoader()
{
}

// MANIPULATORS
void DataFileLoader::configureRootPath(const char *path)
{
    BSLS_ASSERT(path);

    if (!isPlausibleZoneinfoRootPath(path)) {
        BSLS_LOG_DEBUG("Invalid directory provided to initialize "
                       "Zoneinfo database time-zone information loader: %s",
                       path);
    }
    d_rootPath = path;
}

int DataFileLoader::configureRootPathIfPlausible(const char *path)
{
    BSLS_ASSERT(path);

    if (!isPlausibleZoneinfoRootPath(path)) {
        return -1;                                                    // RETURN
    }

    d_rootPath = path;
    return 0;
}

int DataFileLoader::loadTimeZone(Zoneinfo *result, const char *timeZoneId)
{
    return loadTimeZoneImpl(result, *this, timeZoneId, u::k_READ_NORMALIZED);
}

int DataFileLoader::loadTimeZoneRaw(Zoneinfo *result, const char *timeZoneId)
{
    return loadTimeZoneImpl(result, *this, timeZoneId, u::k_READ_RAW);
}

// ACCESSORS
 true
int DataFileLoader::loadTimeZoneFilePath(bsl::string      *result,
                                         const char       *timeZoneId) const
{
    return u::loadTimeZoneFilePath_Impl(result,
                                        timeZoneId,
                                        d_rootPath);
}

int DataFileLoader::loadTimeZoneFilePath(std::string      *result,
                                         const char       *timeZoneId) const
{
    return u::loadTimeZoneFilePath_Impl(result,
                                        timeZoneId,
                                        d_rootPath);
}

#ifdef BSLS_LIBRARYFEATURES_HAS_CPP17_PMR_STRING
int DataFileLoader::loadTimeZoneFilePath(std::pmr::string *result,
                                         const char       *timeZoneId) const
{
    return u::loadTimeZoneFilePath_Impl(result,
                                        timeZoneId,
                                        d_rootPath);
}
#endif

const bsl::string& DataFileLoader::rootPath() const
{
    BSLS_ASSERT(u::INVALID_PATH != d_rootPath);

    return d_rootPath;
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
