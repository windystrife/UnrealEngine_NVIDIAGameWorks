// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectRedirector.h"
#include "AssetTypeActions_Base.h"

class FMenuBuilder;

class FAssetTypeActions_Redirector : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_Redirector", "Redirector"); }
	virtual FColor GetTypeColor() const override { return FColor(128, 128, 128); }
	virtual UClass* GetSupportedClass() const override { return UObjectRedirector::StaticClass(); }
	virtual bool HasActions ( const TArray<UObject*>& InObjects ) const override { return true; }
	virtual void GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder ) override;
	virtual void AssetsActivated( const TArray<UObject*>& InObjects, EAssetTypeActivationMethod::Type ActivationType ) override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
	virtual bool CanLocalize() const override { return false; }

private:
	/** Handler for when FindTarget is selected */
	void ExecuteFindTarget(TArray<TWeakObjectPtr<UObjectRedirector>> Objects);

	/** Handler for when FixUp is selected */
	void ExecuteFixUp(TArray<TWeakObjectPtr<UObjectRedirector>> Objects);

	/** Syncs the content browser to the destination objects for all the supplied redirectors */
	void FindTargets(const TArray<UObjectRedirector*>& Redirectors) const;
};
