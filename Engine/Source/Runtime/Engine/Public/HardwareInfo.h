// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HardwareInfo.h: Declares the hardware info class
=============================================================================*/

#pragma once

#include "CoreMinimal.h"

/** Hardware entry lookups */
static const FName NAME_RHI( "RHI" );
static const FName NAME_TextureFormat( "TextureFormat" );
static const FName NAME_DeviceType( "DeviceType" );


struct ENGINE_API FHardwareInfo
{
	/**
	 * Register with the hardware info a detail which we may want to keep track of
	 *
	 * @param SpecIdentifier - The piece of hardware information we are registering, must match the lookups above
	 * @param HarwareInfo - The information we want to be associated with the input key.
	 */
	static void RegisterHardwareInfo( const FName SpecIdentifier, const FString& HardwareInfo );

	/**
	 * Get the hardware info detail you wanted to keep track of.
	 *
	 * @param SpecIdentifier - The piece of hardware information we are registering, must match the lookups above
	 */
	static FString GetHardwareInfo(const FName SpecIdentifier);

	/**
	 * Get the full details of hardware information which has been registered in string format
	 *
	 * @return The details of the hardware which were registered in a string format.
	 */
	static const FString GetHardwareDetailsString();
};
