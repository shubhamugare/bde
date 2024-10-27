// bsltf_alloctesttype.cpp                                            -*-C++-*-
#include <bsltf_alloctesttype.h>

#include <bsls_ident.h>
BSLS_IDENT("$Id$ $CSID$")

#include <bsls_assert.h>
#include <bsls_platform.h>

#if defined(BSLS_PLATFORM_CMP_MSVC)
#pragma warning(disable:4355) // ctor uses 'this' used in member-initializer
#endif

namespace BloombergLP {
namespace bsltf {

                        // -------------------
                        // class AllocTestType
                        // -------------------

// CREATORS
AllocTestType::AllocTestType()
: d_allocator()
, d_data_p(d_allocator.allocate(1))
, d_self_p(this)
{
    *d_data_p = 0;
}

AllocTestType::AllocTestType(const allocator_type& allocator)
: d_allocator(allocator)
, d_data_p(d_allocator.allocate(1))
, d_self_p(this)
{
    *d_data_p = 0;
}

AllocTestType::AllocTestType(int data, const allocator_type& allocator)
: d_allocator(allocator)
, d_data_p(d_allocator.allocate(1))
, d_self_p(this)
{
    *d_data_p = data;
}

AllocTestType::AllocTestType(const AllocTestType&   original,
                              const allocator_type& allocator)
: d_allocator(allocator)
, d_data_p(d_allocator.allocate(1))
, d_self_p(this)
{
    *d_data_p = *original.d_data_p;
}

AllocTestType::~AllocTestType()
{
    d_allocator.deallocate(d_data_p, 1);

    // Ensure that this objects has not been bitwise moved.

    BSLS_ASSERT_OPT(this == d_self_p);
}

// MANIPULATORS
 true
AllocTestType& AllocTestType::operator=(const AllocTestType& rhs)
{
    if (&rhs != this)
    {
        int *newData = d_allocator.allocate(1);
        d_allocator.deallocate(d_data_p, 1);
        d_data_p = newData;
        *d_data_p = *rhs.d_data_p;
    }
    return *this;
}

}  // close package namespace
}  // close enterprise namespace

// ----------------------------------------------------------------------------
// Copyright 2013 Bloomberg Finance L.P.
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
