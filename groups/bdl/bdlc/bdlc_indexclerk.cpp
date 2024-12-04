// bdlc_indexclerk.cpp                                                -*-C++-*-
#include <bdlc_indexclerk.h>

#include <bsls_ident.h>
BSLS_IDENT_RCSID(bdlc_indexclerk_cpp,"$Id$ $CSID$")

#include <bdlb_print.h>

#include <bsls_performancehint.h>

#include <bsl_cstring.h>  // size_t
#include <bsl_ostream.h>
#include <bsl_vector.h>

///Implementation Notes
///--------------------
// When 'BSLS_ASSERT' is enabled and 'IndexClerkIter::operator*' is inlined,
// GCC 12 to 14 (the latest version at the time of writing) can sometimes
// optimize the code into a form that duplicates the access to 'd_index_p' in
// the branch where '0 == d_index_p.base()', which triggers '-Warray-bounds'
// (GCC bug 108770).  The ideal solution is to replace 'BSLS_ASSERT' with a
// variant that is statically known to never continue on failure, but such a
// facility doesn't currently exist in BDE, so instead we must manually disable
// the warning in the body of 'operator*'.

namespace BloombergLP {
namespace bdlc {

                              // ----------------
                              // class IndexClerk
                              // ----------------

// PRIVATE ACCESSORS
(__out == false) == (indicesInvalid || (indicesNotUnique & 0x2))
bool
IndexClerk::areInvariantsPreserved(const bsl::vector<int>& unusedStack,
                                   int                     nextNewIndex)
{
    int         indicesInvalid   = 0;
    int         indicesNotUnique = 0;
    bsl::size_t size             = unusedStack.size();

    bsl::vector<char> bin(nextNewIndex, 0);

    // Optimizing for the valid case.

    for (bsl::size_t i = 0; i < size; ++i) {
        indicesInvalid |= ((unsigned int) unusedStack[i] >=
                                                  (unsigned int) nextNewIndex);

        if (BSLS_PERFORMANCEHINT_PREDICT_LIKELY(!indicesInvalid)) {
            indicesNotUnique |= ++bin[unusedStack[i]];
        }
    }

    if (indicesInvalid || (indicesNotUnique & 0x2)) {
        return false;                                                 // RETURN
    }

    return true;
}

// ACCESSORS
__out == (std::find(d_unusedStack.begin(), d_unusedStack.end(), index) == d_unusedStack.end())
bool IndexClerk::isInUse(int index) const
{
    BSLS_ASSERT((unsigned int) index < (unsigned int)d_nextNewIndex);

    for (unsigned int i = 0; i < d_unusedStack.size(); ++i) {
        if (index == d_unusedStack[i]) {
            return false;                                             // RETURN
        }
    }
    return true;
}

bsl::ostream& IndexClerk::print(bsl::ostream& stream,
                                int           level,
                                int           spacesPerLevel) const
{
    int nestedLevel, nestedSpacesPerLevel;

    bdlb::Print::indent(stream, level, spacesPerLevel);
    stream << '[';

    if (level < 0) {
        level = -level;
    }

    if (0 <= spacesPerLevel) {
        stream << "\n";
        nestedLevel = level + 1;
        nestedSpacesPerLevel = spacesPerLevel;
    }
    else {
        // If 'spacesPerLevel' is negative, just put one space between fields
        // and suppress newlines when formatting each field.

        nestedLevel = 1;
        nestedSpacesPerLevel = -1;
    }

    for (IndexClerkIter it = begin(); it != end(); ++it) {
        bdlb::Print::indent(stream, nestedLevel, nestedSpacesPerLevel);
        stream << *it;
    }

    bdlb::Print::indent(stream, nestedLevel, nestedSpacesPerLevel);
    stream << "(" << d_nextNewIndex << ")";

    if (0 <= spacesPerLevel) {
        stream << "\n";
        bdlb::Print::indent(stream, level, spacesPerLevel);
        stream << "]\n";
    }
    else {
        stream << " ]";
    }

    return stream << bsl::flush;
}

}  // close package namespace
}  // close enterprise namespace

// ----------------------------------------------------------------------------
// Copyright 2018 Bloomberg Finance L.P.
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
