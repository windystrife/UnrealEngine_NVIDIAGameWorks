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

ifeq ($(tbb_os),mic)
  $(error MIC supports only cross-compilation.  Specify "target=mic" instead.)
endif

ifneq ($(BUILDING_PHASE),1)
  BUILD_PREFIX = mic_icc
  # The same build prefix should be used in offload.inc
  tbb_build_prefix?=$(BUILD_PREFIX)
endif

MAKE_VERSIONS=sh $(tbb_root)/build/version_info_linux.sh $(CPLUS) $(CPLUS_FLAGS) $(INCLUDES) >version_string.ver
MAKE_TBBVARS=sh $(tbb_root)/build/generate_tbbvars.sh
def_prefix=lin64

TEST_LAUNCHER=
run_cmd ?= ../mic.linux.launcher.sh

# detects whether examples are being built.
ifeq ($(BUILDING_PHASE),0)
 export UI = con
 export x64 = 64
endif # examples
