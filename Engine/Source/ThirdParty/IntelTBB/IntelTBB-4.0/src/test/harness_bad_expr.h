/*
    Copyright 2005-2012 Intel Corporation.  All Rights Reserved.

    The source code contained or described herein and all documents related
    to the source code ("Material") are owned by Intel Corporation or its
    suppliers or licensors.  Title to the Material remains with Intel
    Corporation or its suppliers and licensors.  The Material is protected
    by worldwide copyright laws and treaty provisions.  No part of the
    Material may be used, copied, reproduced, modified, published, uploaded,
    posted, transmitted, distributed, or disclosed in any way without
    Intel's prior express written permission.

    No license under any patent, copyright, trade secret or other
    intellectual property right is granted to or conferred upon you by
    disclosure or delivery of the Materials, either expressly, by
    implication, inducement, estoppel or otherwise.  Any license under such
    intellectual property rights must be express and approved by Intel in
    writing.
*/

// Declarations for checking __TBB_ASSERT checks inside TBB.
// This header is an optional part of the test harness.
// It assumes that "harness.h" has already been included.

#define TRY_BAD_EXPR_ENABLED (TBB_USE_ASSERT && TBB_USE_EXCEPTIONS && !__TBB_THROW_ACROSS_MODULE_BOUNDARY_BROKEN)

#if TRY_BAD_EXPR_ENABLED

//! Check that expression x raises assertion failure with message containing given substring.
/** Assumes that tbb::set_assertion_handler( AssertionFailureHandler ) was called earlier. */
#define TRY_BAD_EXPR(x,substr)          \
    {                                   \
        const char* message = NULL;     \
        bool okay = false;              \
        try {                           \
            x;                          \
        } catch( AssertionFailure a ) { \
            okay = true;                \
            message = a.message;        \
        }                               \
        CheckAssertionFailure(__LINE__,#x,okay,message,substr); \
    }

//! Exception object that holds a message.
struct AssertionFailure {
    const char* message;
    AssertionFailure( const char* filename, int line, const char* expression, const char* comment );
};

AssertionFailure::AssertionFailure( const char* filename, int line, const char* expression, const char* comment ) : 
    message(comment) 
{
    ASSERT(filename,"missing filename");
    ASSERT(0<line,"line number must be positive");
    // All of our current files have fewer than 4000 lines.
    ASSERT(line<5000,"dubiously high line number");
    ASSERT(expression,"missing expression");
}

void AssertionFailureHandler( const char* filename, int line, const char* expression, const char* comment ) {
    throw AssertionFailure(filename,line,expression,comment);
}

void CheckAssertionFailure( int line, const char* expression, bool okay, const char* message, const char* substr ) {
    if( !okay ) {
        REPORT("Line %d, %s failed to fail\n", line, expression );
        abort();
    } else if( !message ) {
        REPORT("Line %d, %s failed without a message\n", line, expression );
        abort();
    } else if( strstr(message,substr)==0 ) {                            
        REPORT("Line %d, %s failed with message '%s' missing substring '%s'\n", __LINE__, expression, message, substr );
        abort();
    }
}

#endif /* TRY_BAD_EXPR_ENABLED */
