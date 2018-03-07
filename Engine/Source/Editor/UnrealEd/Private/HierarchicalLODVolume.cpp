// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "HierarchicalLODVolume.h"

AHierarchicalLODVolume::AHierarchicalLODVolume(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{
	bColored = true;
	BrushColor.R = 255;
	BrushColor.G = 100;
	BrushColor.B = 255;
	BrushColor.A = 255;
}
