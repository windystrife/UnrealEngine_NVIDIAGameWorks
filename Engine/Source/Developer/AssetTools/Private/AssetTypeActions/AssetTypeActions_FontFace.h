// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Engine/FontFace.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"

class FMenuBuilder;

class FAssetTypeActions_FontFace : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_FontFace", "Font Face"); }
	virtual FColor GetTypeColor() const override { return FColor(184,184,112); }
	virtual UClass* GetSupportedClass() const override { return UFontFace::StaticClass(); }
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override { return true; }
	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override;
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual uint32 GetCategories() override { return EAssetTypeCategories::UI; }
	virtual bool CanLocalize() const override { return false; }

private:
	/** Can we execute a reimport for the selected objects? */
	bool CanExecuteReimport(const TArray<TWeakObjectPtr<UFontFace>> Objects) const;

	/** Handler for when Reimport is selected */
	void ExecuteReimport(const TArray<TWeakObjectPtr<UFontFace>> Objects) const;
};
