// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "UnrealHeaderTool.h"
#include "CoreMinimal.h"
#include "Misc/AssertionMacros.h"
#include "Misc/CString.h"

#include "CheckedMetadataSpecifiers.h"
#include "FunctionSpecifiers.h"
#include "InterfaceSpecifiers.h"
#include "StructSpecifiers.h"
#include "VariableSpecifiers.h"
#include "Containers/Algo/IsSorted.h"

const TCHAR* GFunctionSpecifierStrings[(int32)EFunctionSpecifier::Max] =
{
	#define FUNCTION_SPECIFIER(SpecifierName) TEXT(#SpecifierName),
		#include "FunctionSpecifiers.def"
	#undef FUNCTION_SPECIFIER
};

const TCHAR* GStructSpecifierStrings[(int32)EStructSpecifier::Max] =
{
	#define STRUCT_SPECIFIER(SpecifierName) TEXT(#SpecifierName),
		#include "StructSpecifiers.def"
	#undef STRUCT_SPECIFIER
};

const TCHAR* GInterfaceSpecifierStrings[(int32)EInterfaceSpecifier::Max] =
{
	#define INTERFACE_SPECIFIER(SpecifierName) TEXT(#SpecifierName),
		#include "InterfaceSpecifiers.def"
	#undef INTERFACE_SPECIFIER
};

const TCHAR* GVariableSpecifierStrings[(int32)EVariableSpecifier::Max] =
{
	#define VARIABLE_SPECIFIER(SpecifierName) TEXT(#SpecifierName),
		#include "VariableSpecifiers.def"
	#undef VARIABLE_SPECIFIER
};

const TCHAR* GCheckedMetadataSpecifierStrings[(int32)ECheckedMetadataSpecifier::Max] =
{
	#define CHECKED_METADATA_SPECIFIER(SpecifierName) TEXT(#SpecifierName),
		#include "CheckedMetadataSpecifiers.def"
	#undef CHECKED_METADATA_SPECIFIER
};

struct FCStringsLessThanCaseInsensitive
{
	FORCEINLINE bool operator()(const TCHAR* Lhs, const TCHAR* Rhs)
	{
		return FCString::Stricmp(Lhs, Rhs) < 0;
	}
};

const bool GIsGFunctionSpecifierStringsSorted        = ensure(Algo::IsSorted(GFunctionSpecifierStrings,        FCStringsLessThanCaseInsensitive()));
const bool GIsGStructSpecifierStringsSorted          = ensure(Algo::IsSorted(GStructSpecifierStrings,          FCStringsLessThanCaseInsensitive()));
const bool GIsGInterfaceSpecifierStringsSorted       = ensure(Algo::IsSorted(GInterfaceSpecifierStrings,       FCStringsLessThanCaseInsensitive()));
const bool GIsGVariableSpecifierStringsSorted        = ensure(Algo::IsSorted(GVariableSpecifierStrings,        FCStringsLessThanCaseInsensitive()));
const bool GIsGCheckedMetadataSpecifierStringsSorted = ensure(Algo::IsSorted(GCheckedMetadataSpecifierStrings, FCStringsLessThanCaseInsensitive()));
