// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

enum class ECheckedMetadataSpecifier
{
	None = -1

	#define CHECKED_METADATA_SPECIFIER(SpecifierName) , SpecifierName
		#include "CheckedMetadataSpecifiers.def"
	#undef CHECKED_METADATA_SPECIFIER

	, Max
};

extern const TCHAR* GCheckedMetadataSpecifierStrings[(int32)ECheckedMetadataSpecifier::Max];
