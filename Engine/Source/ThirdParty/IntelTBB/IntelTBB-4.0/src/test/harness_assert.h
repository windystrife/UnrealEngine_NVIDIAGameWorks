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

// Just the assertion portion of the harness.
// This is useful for writing portions of tests that include
// the minimal number of necessary header files.
//
// The full "harness.h" must be included later.

#ifndef harness_assert_H
#define harness_assert_H

void ReportError( const char* filename, int line, const char* expression, const char* message); 
void ReportWarning( const char* filename, int line, const char* expression, const char* message); 

#define ASSERT(p,message) ((p)?(void)0:ReportError(__FILE__,__LINE__,#p,message))
#define ASSERT_WARNING(p,message) ((p)?(void)0:ReportWarning(__FILE__,__LINE__,#p,message))

//! Compile-time error if x and y have different types
template<typename T>
void AssertSameType( const T& /*x*/, const T& /*y*/ ) {}

#endif /* harness_assert_H */
