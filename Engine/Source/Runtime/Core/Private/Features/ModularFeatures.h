// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Containers/Map.h"
#include "UObject/NameTypes.h"
#include "Delegates/Delegate.h"
#include "Features/IModularFeatures.h"

class IModularFeature;

/**
 * Private implementation of modular features interface
 */
class FModularFeatures : public IModularFeatures
{

public:

	/** IModularFeatures interface */
	virtual int32 GetModularFeatureImplementationCount( const FName Type ) override;
	virtual IModularFeature* GetModularFeatureImplementation( const FName Type, const int32 Index ) override;
	virtual void RegisterModularFeature( const FName Type, class IModularFeature* ModularFeature ) override;
	virtual void UnregisterModularFeature( const FName Type, class IModularFeature* ModularFeature ) override;
	DECLARE_DERIVED_EVENT(FModularFeatures, IModularFeatures::FOnModularFeatureRegistered, FOnModularFeatureRegistered);
	virtual IModularFeatures::FOnModularFeatureRegistered& OnModularFeatureRegistered() override;
	DECLARE_DERIVED_EVENT(FModularFeatures, IModularFeatures::FOnModularFeatureUnregistered, FOnModularFeatureUnregistered);
	virtual IModularFeatures::FOnModularFeatureUnregistered& OnModularFeatureUnregistered() override;

private:


private:

	/** Maps each feature type to a list of known providers of that feature */
	TMultiMap< FName, class IModularFeature* > ModularFeaturesMap;

	/** Event used to inform clients that a modular feature has been registered */
	FOnModularFeatureRegistered ModularFeatureRegisteredEvent;

	/** Event used to inform clients that a modular feature has been unregistered */
	FOnModularFeatureUnregistered ModularFeatureUnregisteredEvent;
};
