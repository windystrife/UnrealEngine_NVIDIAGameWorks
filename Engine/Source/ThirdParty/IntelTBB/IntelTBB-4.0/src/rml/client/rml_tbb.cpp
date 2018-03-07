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

#include "../include/rml_tbb.h"
#include "tbb/dynamic_link.h"
#include <assert.h>

namespace tbb {
namespace internal {
namespace rml {

#define MAKE_SERVER(x) DLD(__TBB_make_rml_server,x)
#define GET_INFO(x) DLD(__TBB_call_with_my_server_info,x)
#define SERVER tbb_server 
#define CLIENT tbb_client
#define FACTORY tbb_factory
#include "rml_factory.h"

} // rml
} // internal
} // tbb
