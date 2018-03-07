// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/MeshMergeCullingVolume.h"

AMeshMergeCullingVolume::AMeshMergeCullingVolume(const FObjectInitializer& ObjectInitializer)
:Super(ObjectInitializer)
{
	bNotForClientOrServer = true;

	bColored = true;
	BrushColor = FColor(45, 225, 45);
}

