#!/bin/bash
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

# Script used to generate tbbvars.[c]sh scripts
bin_dir="$PWD"  # 
cd "$tbb_root"  # keep this comments here
tbb_root="$PWD" # to make it unsensible
cd "$bin_dir"   # to EOL encoding
[ "`uname`" = "Darwin" ] && dll_path="DYLD_LIBRARY_PATH" || dll_path="LD_LIBRARY_PATH" #
[ -f ./tbbvars.sh ] || cat >./tbbvars.sh <<EOF
#!/bin/bash
export TBBROOT="${tbb_root}" #
tbb_bin="${bin_dir}" #
if [ -z "\$CPATH" ]; then #
    export CPATH="\${TBBROOT}/include" #
else #
    export CPATH="\${TBBROOT}/include:\$CPATH" #
fi #
if [ -z "\$LIBRARY_PATH" ]; then #
    export LIBRARY_PATH="\${tbb_bin}" #
else #
    export LIBRARY_PATH="\${tbb_bin}:\$LIBRARY_PATH" #
fi #
if [ -z "\$${dll_path}" ]; then #
    export ${dll_path}="\${tbb_bin}" #
else #
    export ${dll_path}="\${tbb_bin}:\$${dll_path}" #
fi #
${TBB_CUSTOM_VARS_SH} #
EOF
[ -f ./tbbvars.csh ] || cat >./tbbvars.csh <<EOF
#!/bin/csh
setenv TBBROOT "${tbb_root}" #
setenv tbb_bin "${bin_dir}" #
if (! \$?CPATH) then #
    setenv CPATH "\${TBBROOT}/include" #
else #
    setenv CPATH "\${TBBROOT}/include:\$CPATH" #
endif #
if (! \$?LIBRARY_PATH) then #
    setenv LIBRARY_PATH "\${tbb_bin}" #
else #
    setenv LIBRARY_PATH "\${tbb_bin}:\$LIBRARY_PATH" #
endif #
if (! \$?${dll_path}) then #
    setenv ${dll_path} "\${tbb_bin}" #
else #
    setenv ${dll_path} "\${tbb_bin}:\$${dll_path}" #
endif #
${TBB_CUSTOM_VARS_CSH} #
EOF
