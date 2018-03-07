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

#include "ittnotify_config.h"

#if ITT_PLATFORM==ITT_PLATFORM_WIN

#pragma warning (disable: 593)   /* parameter "XXXX" was set but never used                 */
#pragma warning (disable: 344)   /* typedef name has already been declared (with same type) */
#pragma warning (disable: 174)   /* expression has no effect                                */
#pragma warning (disable: 4127)  /* conditional expression is constant                      */
#pragma warning (disable: 4306)  /* conversion from '?' to '?' of greater size              */

#endif /* ITT_PLATFORM==ITT_PLATFORM_WIN */

#if defined __INTEL_COMPILER

#pragma warning (disable: 869)  /* parameter "XXXXX" was never referenced                  */
#pragma warning (disable: 1418) /* external function definition with no prior declaration  */
#pragma warning (disable: 1419) /* external declaration in primary source file             */

#endif /* __INTEL_COMPILER */
