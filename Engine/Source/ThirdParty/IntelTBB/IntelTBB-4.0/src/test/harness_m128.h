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

// Header that sets HAVE_m128/HAVE_m256 if vector types (__m128/__m256) are available

//! Class for testing safety of using vector types.
/** Uses circuitous logic forces compiler to put __m128/__m256 objects on stack while
    executing various methods, and thus tempt it to use aligned loads and stores
    on the stack. */
//  Do not create file-scope objects of the class, because MinGW (as of May 2010)
//  did not always provide proper stack alignment in destructors of such objects.

template<typename __Mvec>
class ClassWithVectorType {
    static const int n = 16;
    static const int F = sizeof(__Mvec)/sizeof(float);
    __Mvec field[n];
    void init( int start );
public:
    ClassWithVectorType() {init(-n);} 
    ClassWithVectorType( int i ) {init(i);}
    void operator=( const ClassWithVectorType& src ) {
        __Mvec stack[n];
        for( int i=0; i<n; ++i )
            stack[i^5] = src.field[i];
        for( int i=0; i<n; ++i )
            field[i^5] = stack[i];
    }
    ~ClassWithVectorType() {init(-2*n);}
    friend bool operator==( const ClassWithVectorType& x, const ClassWithVectorType& y ) {
        for( int i=0; i<F*n; ++i )
            if( ((const float*)x.field)[i]!=((const float*)y.field)[i] )
                return false;
        return true;
    }
    friend bool operator!=( const ClassWithVectorType& x, const ClassWithVectorType& y ) {
        return !(x==y);
    }
};

template<typename __Mvec>
void ClassWithVectorType<__Mvec>::init( int start ) {
    __Mvec stack[n];
    for( int i=0; i<n; ++i ) {
        // Declaring value as a one-element array instead of a scalar quites 
        // gratuitous warnings about possible use of "value" before it was set.
        __Mvec value[1];
        for( int j=0; j<F; ++j )
            ((float*)value)[j] = float(n*start+F*i+j);
        stack[i^5] = value[0];
    }
    for( int i=0; i<n; ++i )
        field[i^5] = stack[i];
}

#if (__AVX__ || (_MSC_VER>=1600 && _M_X64)) && !defined(__sun)
#include <immintrin.h>
#define HAVE_m256 1
typedef ClassWithVectorType<__m256> ClassWithAVX;
#if _MSC_VER
#include <intrin.h> // for __cpuid
#endif
bool have_AVX() {
    bool result = false;
    const int avx_mask = 1<<28;
#if _MSC_VER || __INTEL_COMPILER
    int info[4] = {0,0,0,0};
    const int ECX = 2;
    __cpuid(info, 1);
    result = (info[ECX] & avx_mask)!=0;
#elif __GNUC__
    int ECX;
    __asm__( "cpuid"
             : "=c"(ECX)
             : "a" (1)
             : "ebx", "edx" );
    result = (ECX & avx_mask);
#endif
    return result;
}
#endif /* __AVX__ etc */

#if (__SSE__ || _M_IX86_FP || _M_X64) && !defined(__sun)
#include <xmmintrin.h>
#define HAVE_m128 1
typedef ClassWithVectorType<__m128> ClassWithSSE;
#endif
