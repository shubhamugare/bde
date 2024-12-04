// balst_stacktraceprintutil.cpp                                      -*-C++-*-
#include <balst_stacktraceprintutil.h>

#include <bsls_ident.h>
BSLS_IDENT_RCSID(balst_stacktraceprintutil_cpp,"$Id$ $CSID$")

#include <balst_stacktrace.h>
#include <balst_stacktraceutil.h>

#include <bdlsb_memoutstreambuf.h>

#include <bdlma_heapbypassallocator.h>

#include <bsls_assert.h>
#include <bsls_bsltestutil.h>
#include <bsls_log.h>
#include <bsls_platform.h>
#include <bsls_stackaddressutil.h>

#include <bsl_algorithm.h>
#include <bsl_iostream.h>

#if defined(BSLS_PLATFORM_OS_WINDOWS) && defined(BDE_BUILD_TARGET_OPT)
#pragma optimize("", off)
#endif

namespace BloombergLP {

namespace balst {
                         // -------------------------
                         // class StackTracePrintUtil
                         // -------------------------

// CLASS METHOD
__out != NULL
bsl::ostream& StackTracePrintUtil::printStackTrace(
                                         bsl::ostream& stream,
                                         int           maxFrames,
                                         bool          demanglingPreferredFlag,
                                         int           additionalIgnoreFrames)
{
    BSLS_ASSERT(0 <= maxFrames || -1 == maxFrames);
    BSLS_ASSERT(0 <= additionalIgnoreFrames);

    enum { k_DEFAULT_MAX_FRAMES = 1024 };

    if (maxFrames < 0) {
        maxFrames = k_DEFAULT_MAX_FRAMES;
    }

    int ignoreFrames = bsls::StackAddressUtil::k_IGNORE_FRAMES + 1 +
                                                        additionalIgnoreFrames;

    // The value 'IGNORE_FRAMES' indicates the number of additional frames to
    // be ignored because they contained function calls within the stack trace
    // facility.

    maxFrames += ignoreFrames;

    StackTrace st;    // defaults to using its own heap bypass allocator

    void **addresses = static_cast<void **>(st.allocator()->allocate(
                                                  maxFrames * sizeof(void *)));
#if !defined(BSLS_PLATFORM_OS_CYGWIN)
    int numAddresses = bsls::StackAddressUtil::getStackAddresses(addresses,
                                                                 maxFrames);
#else
    int numAddresses = 0;
#endif
    if (numAddresses <= 0 || numAddresses > maxFrames) {
        stream << "Stack Trace: Internal Error getting stack addresses\n";
        return stream;                                                // RETURN
    }

    ignoreFrames = bsl::min(numAddresses, ignoreFrames);
    BSLS_ASSERT(0 <= ignoreFrames);

    const int rc = StackTraceUtil::loadStackTraceFromAddressArray(
                                                   &st,
                                                   addresses    + ignoreFrames,
                                                   numAddresses - ignoreFrames,
                                                   demanglingPreferredFlag);
    if (rc) {
        stream << "Stack Trace: Internal Error initializing frames\n";
        return stream;                                                // RETURN
    }

    return StackTraceUtil::printFormatted(stream, st);
}

void StackTracePrintUtil::logExceptionStackTrace(const char *exceptionName,
                                                 const char *message)
{
    enum { k_MAX_STACK_TRACE_DEPTH        = 128,
                // Enough to cover most stack traces, not so deep as to require
                // huge amounts of memory.

           k_LONG_STACK_TRACE_LINE_LENGTH = 256,

           k_STREAMBUF_MEMORY_SIZE        = k_MAX_STACK_TRACE_DEPTH *
                                              k_LONG_STACK_TRACE_LINE_LENGTH };

    // Use this 'streambuf' approach rather than 'bsl::ostringstream' since
    // 'BSLS_LOG_FATAL' requires a 'const char *' and the only way to get that
    // out of an 'ostringsteam' is via '.str().c_str()' which would copy the
    // whole thing using the default allocator.

    bdlma::HeapBypassAllocator alloc;
    bdlsb::MemOutStreamBuf     sb(k_STREAMBUF_MEMORY_SIZE, &alloc);
    bsl::ostream               os(&sb);

    (*bsls::BslTestUtil::makeFunctionCallNonInline(&printStackTrace))(
                                         os, k_MAX_STACK_TRACE_DEPTH, true, 1);
    os << bsl::ends;

    // We have to use bsls_log because balst has no dependency on ball.

    BSLS_LOG_FATAL("About to throw %s, %s\nStack Trace:\n%s",
                   exceptionName,
                   message,
                   sb.data());
}

}  // close package namespace
}  // close enterprise namespace

// ----------------------------------------------------------------------------
// Copyright 2015 Bloomberg Finance L.P.
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
