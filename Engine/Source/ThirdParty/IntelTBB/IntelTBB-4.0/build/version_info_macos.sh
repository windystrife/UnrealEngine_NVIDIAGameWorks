#!/bin/sh
#
# Copyright 2005-2012 Intel Corporation.  All Rights Reserved.
#
# The source code contained or described herein and all documents related
# to the source code ("Material") are owned by Intel Corporation or its
# suppliers or licensors.  Title to the Material remains with Intel
# Corporation or its suppliers and licensors.  The Material is protected
# by worldwide copyright laws and treaty provisions.  No part of the
# Material may be used, copied, reproduced, modified, published, uploaded,
# posted, transmitted, distributed, or disclosed in any way without
# Intel's prior express written permission.
#
# No license under any patent, copyright, trade secret or other
# intellectual property right is granted to or conferred upon you by
# disclosure or delivery of the Materials, either expressly, by
# implication, inducement, estoppel or otherwise.  Any license under such
# intellectual property rights must be express and approved by Intel in
# writing.

# Script used to generate version info string
echo "#define __TBB_VERSION_STRINGS(N) \\"
echo '#N": BUILD_HOST'"\t\t"`hostname -s`" ("`arch`")"'" ENDL \'
echo '#N": BUILD_OS'"\t\t"`sw_vers -productName`" version "`sw_vers -productVersion`'" ENDL \'
echo '#N": BUILD_KERNEL'"\t"`uname -v`'" ENDL \'
echo '#N": BUILD_GCC'"\t\t"`gcc -v </dev/null 2>&1 | grep 'version'`'" ENDL \'
[ -z "$COMPILER_VERSION" ] || echo '#N": BUILD_COMPILER'"\t"$COMPILER_VERSION'" ENDL \'
echo '#N": BUILD_TARGET'"\t$arch on $runtime"'" ENDL \'
echo '#N": BUILD_COMMAND'"\t"$*'" ENDL \'
echo ""
echo "#define __TBB_DATETIME \""`date -u`"\""
