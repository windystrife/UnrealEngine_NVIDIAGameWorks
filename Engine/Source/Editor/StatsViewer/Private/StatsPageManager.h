// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IStatsPage.h"

/**
 * A class which manages a the collection of known stats pages           
 */
class FStatsPageManager
{
public:
	virtual ~FStatsPageManager() {}

	/** Gets the instance of this manager */
	static FStatsPageManager& Get();

	/** 
	 * Register a page with the manager 
	 * @param	InPage	The page to register
	 */
	void RegisterPage( TSharedRef<class IStatsPage> InPage );

	/** 
	 * Unregister a page from the manager 
	 * @param	InPage	The page to unregister
	 */
	void UnregisterPage( TSharedRef<class IStatsPage> InPage );

	/** Unregister & delete all registered pages */
	void UnregisterAllPages();

	/** Get the number of stats pages we have */
	int32 NumPages() const;

	/** Get the factory at the specified index */
	TSharedRef<class IStatsPage> GetPage( int32 InPageIndex );

	/** Get the factory with the specified name */
	TSharedPtr<class IStatsPage> GetPage( const FName& InPageName );

private:

	/** The registered pages */
	TArray< TSharedRef<class IStatsPage> > StatsPages;
};
