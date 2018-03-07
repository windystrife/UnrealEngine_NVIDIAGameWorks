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

REM no LD_PRELOAD under Windows
if "%1"=="-l" (
    echo skip
    exit
)

%*
