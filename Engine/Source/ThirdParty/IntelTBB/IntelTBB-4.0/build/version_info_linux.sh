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
echo '#N": BUILD_HOST'"\t\t"`hostname -s`" ("`uname -m`")"'" ENDL \'
# find OS name in *-release and issue* files by filtering blank lines and lsb-release content out
echo '#N": BUILD_OS'"\t\t"`lsb_release -sd 2>/dev/null | grep -ih '[a-z] ' - /etc/*release /etc/issue 2>/dev/null | head -1 | sed -e 's/["\\\\]//g'`'" ENDL \'
echo '#N": BUILD_KERNEL'"\t"`uname -srv`'" ENDL \'
echo '#N": BUILD_GCC'"\t\t"`g++ -v </dev/null 2>&1 | grep 'gcc.*version '`'" ENDL \'
[ -z "$COMPILER_VERSION" ] || echo '#N": BUILD_COMPILER'"\t"$COMPILER_VERSION'" ENDL \'
echo '#N": BUILD_LIBC'"\t"`getconf GNU_LIBC_VERSION | grep glibc | sed -e 's/^glibc //'`'" ENDL \'
echo '#N": BUILD_LD'"\t\t"`ld -v 2>&1 | grep 'version'`'" ENDL \'
echo '#N": BUILD_TARGET'"\t$arch on $runtime"'" ENDL \'
echo '#N": BUILD_COMMAND'"\t"$*'" ENDL \'
echo ""
echo "#define __TBB_DATETIME \""`date -u`"\""
