#!/bin/sh
# Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

cd ..

FLEX=flex
BISON=bison
SRCDIR=../../../Source/ThirdParty/hlslcc/hlslcc/src/hlslcc_lib

if [ -z "$FLEX_PATH" ]; then
	FLEX=bison;
else
	FLEX=$FLEX_PATH;
fi

if [ -z "$BISON_PATH" ]; then
	BISON=bison;
else
	BISON=$BISON_PATH;
fi

#$FLEX --nounistd -o"$SRCDIR/glcpp-lex.inl" "$SRCDIR/glcpp-lex.l"
#$BISON -v -o "$SRCDIR/glcpp-parse.inl" --defines="$SRCDIR/glcpp-parse.h" "$SRCDIR/glcpp-parse.y"

#$FLEX --nounistd -o"$SRCDIR/hlsl_lexer.inl" "$SRCDIR/hlsl_lexer.ll"
$BISON -v -o "$SRCDIR/hlsl_parser.inl" -p "_mesa_hlsl_" --defines="$SRCDIR/hlsl_parser.h" "$SRCDIR/hlsl_parser.yy"
