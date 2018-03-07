// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	TextureLightProfile.cpp: Implementation of UTextureLightProfile.
=============================================================================*/

#include "Engine/TextureLightProfile.h"


/*-----------------------------------------------------------------------------
	UTextureLightProfile
-----------------------------------------------------------------------------*/
UTextureLightProfile::UTextureLightProfile(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	NeverStream = true;
	Brightness = -1;
}

