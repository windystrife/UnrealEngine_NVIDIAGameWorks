// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class EInterfaceSpecifier
{
	None = -1

	#define INTERFACE_SPECIFIER(SpecifierName) , SpecifierName
		#include "InterfaceSpecifiers.def"
	#undef INTERFACE_SPECIFIER

	, Max
};

extern const TCHAR* GInterfaceSpecifierStrings[(int32)EInterfaceSpecifier::Max];
