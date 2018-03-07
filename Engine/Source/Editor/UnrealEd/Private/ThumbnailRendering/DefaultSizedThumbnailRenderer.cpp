// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ThumbnailRendering/DefaultSizedThumbnailRenderer.h"

UDefaultSizedThumbnailRenderer::UDefaultSizedThumbnailRenderer(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UDefaultSizedThumbnailRenderer::GetThumbnailSize(UObject*, float Zoom, uint32& OutWidth, uint32& OutHeight) const
{
	OutWidth = FMath::TruncToInt(DefaultSizeX * Zoom);
	OutHeight = FMath::TruncToInt(DefaultSizeY * Zoom);
}
