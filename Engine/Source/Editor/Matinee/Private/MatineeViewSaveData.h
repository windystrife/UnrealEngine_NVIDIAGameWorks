// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

struct FMatineeViewSaveData
{
	FMatineeViewSaveData()
		: ViewIndex( INDEX_NONE )
		, ViewLocation( 0.0f, 0.0f, 0.0f )
		, ViewRotation( 0, 0, 0)
	{
	}

	int32 ViewIndex;
	FVector ViewLocation;
	FRotator ViewRotation;
};
