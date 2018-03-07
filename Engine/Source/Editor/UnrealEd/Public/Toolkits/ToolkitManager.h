// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class IToolkit;

/**
 * Singleton that managers instances of editor toolkits
 */
class FToolkitManager
{

public:
	/** Get the singleton instance of the asset editor manager */
	UNREALED_API static FToolkitManager& Get();

	/** Call this to register a newly-created toolkit. */
	UNREALED_API void RegisterNewToolkit( const TSharedRef< class IToolkit > NewToolkit );

	/** Call this to close an existing toolkit */
	UNREALED_API void CloseToolkit( const TSharedRef< class IToolkit > ClosingToolkit );

	/** Called by a toolkit host itself right before it goes away, so that we can make sure the toolkits are destroyed too */
	UNREALED_API void OnToolkitHostDestroyed( class IToolkitHost* HostBeingDestroyed );

	/** Tries to find an open asset editor that's editing the specified asset, and returns the toolkit for that asset editor */
	UNREALED_API TSharedPtr< IToolkit > FindEditorForAsset( const UObject* Asset );

private:
	// Buried constructor since the asset editor manager is a singleton
	FToolkitManager();


private:

	/** List of all currently open toolkits */
	TArray< TSharedPtr< IToolkit > > Toolkits;
};



