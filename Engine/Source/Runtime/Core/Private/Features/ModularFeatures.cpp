// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Features/ModularFeatures.h"


IModularFeatures& IModularFeatures::Get()
{
	// Singleton instance
	static FModularFeatures ModularFeatures;
	return ModularFeatures;
}


int32 FModularFeatures::GetModularFeatureImplementationCount( const FName Type )
{
	return ModularFeaturesMap.Num( Type );
}


IModularFeature* FModularFeatures::GetModularFeatureImplementation( const FName Type, const int32 Index )
{
	IModularFeature* ModularFeature = nullptr;

	int32 CurrentIndex = 0;
	for( TMultiMap< FName, class IModularFeature* >::TConstKeyIterator It( ModularFeaturesMap, Type ); It; ++It )
	{
		if( Index == CurrentIndex )
		{
			ModularFeature = It.Value();
			break;
		}

		++CurrentIndex;
	}

	check( ModularFeature != nullptr );
	return ModularFeature;
}


void FModularFeatures::RegisterModularFeature( const FName Type, IModularFeature* ModularFeature )
{
	ModularFeaturesMap.AddUnique( Type, ModularFeature );
	ModularFeatureRegisteredEvent.Broadcast( Type, ModularFeature );
}


void FModularFeatures::UnregisterModularFeature( const FName Type, IModularFeature* ModularFeature )
{
	ModularFeaturesMap.RemoveSingle( Type, ModularFeature );
	ModularFeatureUnregisteredEvent.Broadcast( Type, ModularFeature );
}

IModularFeatures::FOnModularFeatureRegistered& FModularFeatures::OnModularFeatureRegistered()
{
	return ModularFeatureRegisteredEvent;
}

IModularFeatures::FOnModularFeatureUnregistered& FModularFeatures::OnModularFeatureUnregistered()
{
	return ModularFeatureUnregisteredEvent;
}
