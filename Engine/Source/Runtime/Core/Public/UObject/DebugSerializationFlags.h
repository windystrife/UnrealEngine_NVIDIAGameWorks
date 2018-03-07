// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
DebugSerializationFlags.h: 
Set custom flags on the archive to help track issues while serializing
=============================================================================*/

#pragma once

#include "CoreTypes.h"

// Debug serialization flags
enum EDebugSerializationFlags
{
	/** No special flags */
	DSF_None =					0x00000000,

	DSF_IgnoreDiff =			0x00000001,
	DSF_EnableCookerWarnings =	0x00000002,
};
