// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LandscapeInfo.h"
#include "LandscapeLayerInfoObject.h"

LANDSCAPE_API FLandscapeInfoLayerSettings::FLandscapeInfoLayerSettings(ULandscapeLayerInfoObject* InLayerInfo, class ALandscapeProxy* InProxy)
	: LayerInfoObj(InLayerInfo)
	, LayerName((InLayerInfo != NULL) ? InLayerInfo->LayerName : NAME_None)
#if WITH_EDITORONLY_DATA
	, ThumbnailMIC(NULL)
	, Owner(InProxy)
	, DebugColorChannel(0)
	, bValid(false)
#endif
{
}
