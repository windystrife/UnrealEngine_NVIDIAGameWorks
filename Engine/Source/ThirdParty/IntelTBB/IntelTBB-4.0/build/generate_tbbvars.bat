@echo off
REM
REM Copyright 2005-2012 Intel Corporation.  All Rights Reserved.
REM
REM The source code contained or described herein and all documents related
REM to the source code ("Material") are owned by Intel Corporation or its
REM suppliers or licensors.  Title to the Material remains with Intel
REM Corporation or its suppliers and licensors.  The Material is protected
REM by worldwide copyright laws and treaty provisions.  No part of the
REM Material may be used, copied, reproduced, modified, published, uploaded,
REM posted, transmitted, distributed, or disclosed in any way without
REM Intel's prior express written permission.
REM
REM No license under any patent, copyright, trade secret or other
REM intellectual property right is granted to or conferred upon you by
REM disclosure or delivery of the Materials, either expressly, by
REM implication, inducement, estoppel or otherwise.  Any license under such
REM intellectual property rights must be express and approved by Intel in
REM writing.
REM
setlocal
for %%D in ("%tbb_root%") do set actual_root=%%~fD
set fslash_root=%actual_root:\=/%
set bin_dir=%CD%
set fslash_bin_dir=%bin_dir:\=/%
set _INCLUDE=INCLUDE& set _LIB=LIB
if not x%UNIXMODE%==x set _INCLUDE=CPATH& set _LIB=LIBRARY_PATH

if exist tbbvars.bat goto skipbat
echo Generating local tbbvars.bat
echo @echo off>tbbvars.bat
echo SET TBBROOT=%actual_root%>>tbbvars.bat
echo SET TBB_ARCH_PLATFORM=%arch%\%runtime%>>tbbvars.bat
echo SET TBB_TARGET_ARCH=%arch%>>tbbvars.bat
echo SET %_INCLUDE%=%%TBBROOT%%\include;%%%_INCLUDE%%%>>tbbvars.bat
echo SET %_LIB%=%bin_dir%;%%%_LIB%%%>>tbbvars.bat
echo SET PATH=%bin_dir%;%%PATH%%>>tbbvars.bat
if not x%UNIXMODE%==x echo SET LD_LIBRARY_PATH=%bin_dir%;%%LD_LIBRARY_PATH%%>>tbbvars.bat
:skipbat

if exist tbbvars.sh goto skipsh
echo Generating local tbbvars.sh
echo #!/bin/sh>tbbvars.sh
echo export TBBROOT="%fslash_root%">>tbbvars.sh
echo export TBB_ARCH_PLATFORM="%arch%\%runtime%">>tbbvars.sh
echo export TBB_TARGET_ARCH="%arch%">>tbbvars.sh
echo export %_INCLUDE%="${TBBROOT}/include;$%_INCLUDE%">>tbbvars.sh
echo export %_LIB%="%fslash_bin_dir%;$%_LIB%">>tbbvars.sh
echo export PATH="%fslash_bin_dir%;$PATH">>tbbvars.sh
if not x%UNIXMODE%==x echo export LD_LIBRARY_PATH="%fslash_bin_dir%;$LD_LIBRARY_PATH">>tbbvars.sh
:skipsh

if exist tbbvars.csh goto skipcsh
echo Generating local tbbvars.csh
echo #!/bin/csh>tbbvars.csh
echo setenv TBBROOT "%actual_root%">>tbbvars.csh
echo setenv TBB_ARCH_PLATFORM "%arch%\%runtime%">>tbbvars.csh
echo setenv TBB_TARGET_ARCH "%arch%">>tbbvars.csh
echo setenv %_INCLUDE% "${TBBROOT}\include;$%_INCLUDE%">>tbbvars.csh
echo setenv %_LIB% "%bin_dir%;$%_LIB%">>tbbvars.csh
echo setenv PATH "%bin_dir%;$PATH">>tbbvars.csh
if not x%UNIXMODE%==x echo setenv LD_LIBRARY_PATH "%bin_dir%;$LD_LIBRARY_PATH">>tbbvars.csh
:skipcsh

endlocal
exit
