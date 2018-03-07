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

export LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH
#setting output format .csv, 'pivot' - is pivot table mode, ++ means append
export STAT_FORMAT=pivot-csv++
#check existing files because of apend mode
ls *.csv
rm -i *.csv
#setting a delimiter in txt or csv file
#export STAT_DELIMITER=,
export STAT_RUNINFO1=Host=`hostname -s`
#append a suffix after the filename
#export STAT_SUFFIX=$STAT_RUNINFO1
for ((i=1;i<=${repeat:=100};++i)); do echo $i of $repeat: && STAT_RUNINFO2=Run=$i $* || break; done
