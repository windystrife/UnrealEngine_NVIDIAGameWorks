// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class EVariableSpecifier
{
	None = -1

	#define VARIABLE_SPECIFIER(SpecifierName) , SpecifierName
		#include "VariableSpecifiers.def"
	#undef VARIABLE_SPECIFIER

	, Max
};

extern const TCHAR* GVariableSpecifierStrings[(int32)EVariableSpecifier::Max];
