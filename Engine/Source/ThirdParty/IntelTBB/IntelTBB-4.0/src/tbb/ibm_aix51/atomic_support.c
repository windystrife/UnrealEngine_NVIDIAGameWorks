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

#include <stdint.h>
#include <sys/atomic_op.h>

/* This file must be compiled with gcc.  The IBM compiler doesn't seem to
   support inline assembly statements (October 2007). */

#ifdef __GNUC__

int32_t __TBB_machine_cas_32 (volatile void* ptr, int32_t value, int32_t comparand) { 
    __asm__ __volatile__ ("sync\n");  /* memory release operation */
    compare_and_swap ((atomic_p) ptr, &comparand, value);
    __asm__ __volatile__ ("isync\n");  /* memory acquire operation */
    return comparand;
}

int64_t __TBB_machine_cas_64 (volatile void* ptr, int64_t value, int64_t comparand) { 
    __asm__ __volatile__ ("sync\n");  /* memory release operation */
    compare_and_swaplp ((atomic_l) ptr, &comparand, value);
    __asm__ __volatile__ ("isync\n");  /* memory acquire operation */
    return comparand;
}

void __TBB_machine_flush () { 
    __asm__ __volatile__ ("sync\n");
}

void __TBB_machine_lwsync () { 
    __asm__ __volatile__ ("lwsync\n");
}

void __TBB_machine_isync () { 
    __asm__ __volatile__ ("isync\n");
}

#endif /* __GNUC__ */
