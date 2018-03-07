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

#ifndef __TBB_function_replacement_H
#define __TBB_function_replacement_H

#include <stddef.h> //for ptrdiff_t
typedef enum {
    FRR_OK,     /* Succeeded in replacing the function */
    FRR_NODLL,  /* The requested DLL was not found */
    FRR_NOFUNC, /* The requested function was not found */
    FRR_FAILED, /* The function replacement request failed */
} FRR_TYPE;

typedef enum {
    FRR_FAIL,     /* Required function */
    FRR_IGNORE,   /* optional function */
} FRR_ON_ERROR;

typedef void (*FUNCPTR)();

#ifndef UNICODE
#define ReplaceFunction ReplaceFunctionA
#else
#define ReplaceFunction ReplaceFunctionW
#endif //UNICODE

FRR_TYPE ReplaceFunctionA(const char *dllName, const char *funcName, FUNCPTR newFunc, const char ** opcodes, FUNCPTR* origFunc=NULL);
FRR_TYPE ReplaceFunctionW(const wchar_t *dllName, const char *funcName, FUNCPTR newFunc, const char ** opcodes, FUNCPTR* origFunc=NULL);

// Utilities to convert between ADDRESS and LPVOID
union Int2Ptr {
    UINT_PTR uip;
    LPVOID lpv;
};

inline UINT_PTR Ptr2Addrint(LPVOID ptr);
inline LPVOID Addrint2Ptr(UINT_PTR ptr);

// Use this value as the maximum size the trampoline region
const unsigned MAX_PROBE_SIZE = 32;

// The size of a jump relative instruction "e9 00 00 00 00"
const unsigned SIZE_OF_RELJUMP = 5;

// The size of jump RIP relative indirect "ff 25 00 00 00 00"
const unsigned SIZE_OF_INDJUMP = 6;

// The size of address we put in the location (in Intel64)
const unsigned SIZE_OF_ADDRESS = 8;

// The max distance covered in 32 bits: 2^31 - 1 - C
// where C should not be smaller than the size of a probe.
// The latter is important to correctly handle "backward" jumps.
const __int64 MAX_DISTANCE = (((__int64)1 << 31) - 1) - MAX_PROBE_SIZE;

// The maximum number of distinct buffers in memory
const ptrdiff_t MAX_NUM_BUFFERS = 256;

#endif //__TBB_function_replacement_H
