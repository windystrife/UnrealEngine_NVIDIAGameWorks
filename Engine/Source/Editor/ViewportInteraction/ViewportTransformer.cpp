// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "ViewportTransformer.h"
#include "ViewportWorldInteraction.h"


void UViewportTransformer::Init( UViewportWorldInteraction* InitViewportWorldInteraction )
{
	this->ViewportWorldInteraction = InitViewportWorldInteraction;
}


void UViewportTransformer::Shutdown()
{
	this->ViewportWorldInteraction = nullptr;
}

