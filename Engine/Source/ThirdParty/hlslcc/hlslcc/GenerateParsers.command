#!/bin/sh
# Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#
# Simple wrapper around GenerateParsers.sh using the
# .command extension enables it to be run from the OSX Finder.


setlocal
set FLEX=win_flex.exe
set BISON=win_bison.exe
set SRCDIR=../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib

pushd ..\..\..\..\Extras\NotForLicensees\FlexAndBison\

%FLEX% --nounistd -o%SRCDIR%/glcpp-lex.inl %SRCDIR%/glcpp-lex.l
%BISON% -v -o "%SRCDIR%/glcpp-parse.inl" --defines=%SRCDIR%/glcpp-parse.h %SRCDIR%/glcpp-parse.y

%FLEX% --nounistd -o%SRCDIR%/hlsl_lexer.inl %SRCDIR%/hlsl_lexer.ll
%BISON% -v -o "%SRCDIR%/hlsl_parser.inl" -p "_mesa_hlsl_" --defines=%SRCDIR%/hlsl_parser.h %SRCDIR%/hlsl_parser.yy

pause

popd
endlocal