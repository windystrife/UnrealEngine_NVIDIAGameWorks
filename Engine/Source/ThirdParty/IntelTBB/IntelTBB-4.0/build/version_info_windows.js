// Copyright 2005-2012 Intel Corporation.  All Rights Reserved.
//
// The source code contained or described herein and all documents related
// to the source code ("Material") are owned by Intel Corporation or its
// suppliers or licensors.  Title to the Material remains with Intel
// Corporation or its suppliers and licensors.  The Material is protected
// by worldwide copyright laws and treaty provisions.  No part of the
// Material may be used, copied, reproduced, modified, published, uploaded,
// posted, transmitted, distributed, or disclosed in any way without
// Intel's prior express written permission.
//
// No license under any patent, copyright, trade secret or other
// intellectual property right is granted to or conferred upon you by
// disclosure or delivery of the Materials, either expressly, by
// implication, inducement, estoppel or otherwise.  Any license under such
// intellectual property rights must be express and approved by Intel in
// writing.

var WshShell = WScript.CreateObject("WScript.Shell");

var tmpExec;

WScript.Echo("#define __TBB_VERSION_STRINGS(N) \\");

//Getting BUILD_HOST
WScript.echo( "#N \": BUILD_HOST\\t\\t" + 
              WshShell.ExpandEnvironmentStrings("%COMPUTERNAME%") +
              "\" ENDL \\" );

//Getting BUILD_OS
tmpExec = WshShell.Exec("cmd /c ver");
while ( tmpExec.Status == 0 ) {
    WScript.Sleep(100);
}
tmpExec.StdOut.ReadLine();

WScript.echo( "#N \": BUILD_OS\\t\\t" + 
              tmpExec.StdOut.ReadLine() +
              "\" ENDL \\" );

if ( WScript.Arguments(0).toLowerCase().match("gcc") ) {
    tmpExec = WshShell.Exec("gcc --version");
    WScript.echo( "#N \": BUILD_COMPILER\\t" + 
                  tmpExec.StdOut.ReadLine() + 
                  "\" ENDL \\" );

} else { // MS / Intel compilers
    //Getting BUILD_CL
    tmpExec = WshShell.Exec("cmd /c echo #define 0 0>empty.cpp");
    tmpExec = WshShell.Exec("cl -c empty.cpp ");
    while ( tmpExec.Status == 0 ) {
        WScript.Sleep(100);
    }
    var clVersion = tmpExec.StdErr.ReadLine();
    WScript.echo( "#N \": BUILD_CL\\t\\t" + 
                  clVersion +
                  "\" ENDL \\" );

    //Getting BUILD_COMPILER
    if ( WScript.Arguments(0).toLowerCase().match("icl") ) {
        tmpExec = WshShell.Exec("icl -c empty.cpp ");
        while ( tmpExec.Status == 0 ) {
            WScript.Sleep(100);
        }
        WScript.echo( "#N \": BUILD_COMPILER\\t" + 
                      tmpExec.StdErr.ReadLine() + 
                      "\" ENDL \\" );
    } else {
        WScript.echo( "#N \": BUILD_COMPILER\\t\\t" + 
                      clVersion +
                      "\" ENDL \\" );
    }
    tmpExec = WshShell.Exec("cmd /c del /F /Q empty.obj empty.cpp");
}

//Getting BUILD_TARGET
WScript.echo( "#N \": BUILD_TARGET\\t" + 
              WScript.Arguments(1) + 
              "\" ENDL \\" );

//Getting BUILD_COMMAND
WScript.echo( "#N \": BUILD_COMMAND\\t" + WScript.Arguments(2) + "\" ENDL" );

//Getting __TBB_DATETIME and __TBB_VERSION_YMD
var date = new Date();
WScript.echo( "#define __TBB_DATETIME \"" + date.toUTCString() + "\"" );
WScript.echo( "#define __TBB_VERSION_YMD " + date.getUTCFullYear() + ", " + 
              (date.getUTCMonth() > 8 ? (date.getUTCMonth()+1):("0"+(date.getUTCMonth()+1))) + 
              (date.getUTCDate() > 9 ? date.getUTCDate():("0"+date.getUTCDate())) );


/*

Original strings

#define __TBB_VERSION_STRINGS \
"TBB: BUILD_HOST\t\tvpolin-mobl1 (ia32)" ENDL \
"TBB: BUILD_OS\t\tMicrosoft Windows XP [Version 5.1.2600]" ENDL \
"TBB: BUILD_CL\t\tMicrosoft (R) 32-bit C/C++ Optimizing Compiler Version 13.10.3077 for 80x86" ENDL \
"TBB: BUILD_COMPILER\tIntel(R) C++ Compiler for 32-bit applications, Version 9.1 Build 20070109Z Package ID: W_CC_C_9.1.034 " ENDL \
"TBB: BUILD_TARGET\t" ENDL \
"TBB: BUILD_COMMAND\t" ENDL \

#define __TBB_DATETIME "Mon Jun 4 10:16:07 UTC 2007"
#define __TBB_VERSION_YMD 2007, 0604



# The script must be run from two directory levels below this level.
x='"TBB: '
y='" ENDL \'
echo "#define __TBB_VERSION_STRINGS \\"
echo $x "BUILD_HOST\t\t"`hostname`" ("`../../arch.exe`")"$y
echo $x "BUILD_OS\t\t"`../../win_version.bat|grep -i 'Version'`$y
echo >empty.cpp
echo $x "BUILD_CL\t\t"`cl -c empty.cpp 2>&1 | grep -i Version`$y
echo $x "BUILD_COMPILER\t"`icl -c empty.cpp 2>&1 | grep -i Version`$y
echo $x "BUILD_TARGET\t"$TBB_ARCH$y
echo $x "BUILD_COMMAND\t"$*$y
echo ""
# A workaround for MKS 8.6 where `date -u` crashes.
date -u > date.tmp
echo "#define __TBB_DATETIME \""`cat date.tmp`"\""
echo "#define __TBB_VERSION_YMD "`date '+%Y, %m%d'`
rm empty.cpp
rm empty.obj
rm date.tmp
*/
