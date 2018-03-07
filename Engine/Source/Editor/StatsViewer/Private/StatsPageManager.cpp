// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "StatsPageManager.h"


FStatsPageManager& FStatsPageManager::Get()
{
	static FStatsPageManager* Instance = NULL;
	if( Instance == NULL )
	{
		Instance = new FStatsPageManager;
	}
	return *Instance;
}

void FStatsPageManager::RegisterPage( TSharedRef<IStatsPage> InPage )
{
	StatsPages.Add( InPage );
}

void FStatsPageManager::UnregisterPage( TSharedRef<IStatsPage> InPage )
{
	StatsPages.Remove( InPage );
}

void FStatsPageManager::UnregisterAllPages()
{
	StatsPages.Empty();
}

int32 FStatsPageManager::NumPages() const
{
	return StatsPages.Num();
}

TSharedRef<IStatsPage> FStatsPageManager::GetPage( int32 InPageIndex )
{
	check(StatsPages.IsValidIndex(InPageIndex));

	return StatsPages[InPageIndex];
}

TSharedPtr<IStatsPage> FStatsPageManager::GetPage( const FName& InPageName )
{
	for( auto Iter = StatsPages.CreateIterator(); Iter; Iter++ )
	{
		TSharedRef<class IStatsPage> Page  = *Iter;
		if(Page.Get().GetName() == InPageName)
		{
			return Page;
		}
	}

	return NULL;
}
