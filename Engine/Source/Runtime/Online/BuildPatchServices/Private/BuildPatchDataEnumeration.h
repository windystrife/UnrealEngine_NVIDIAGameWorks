// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class FBuildDataEnumeration
{
public:
	static bool EnumeratePatchData(const FString& InputFile, const FString& OutputFile, bool bIncludeSizes);
};
