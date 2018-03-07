// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class FAssetTypeActions_CSVAssetBase : public FAssetTypeActions_Base
{
public:
	virtual FColor GetTypeColor() const override { return FColor(62, 140, 35); }
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override { return true; }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Misc; }
	virtual bool IsImportedAsset() const override { return true; }

protected:
	/** Handler for opening the source file for this asset */
	void ExecuteFindSourceFileInExplorer(TArray<FString> Filenames, TArray<FString> OverrideExtensions);

	/** Determine whether the find source file in explorer editor command can execute or not */
	bool CanExecuteFindSourceFileInExplorer(TArray<FString> Filenames, TArray<FString> OverrideExtensions) const;

	/** Verify the specified filename exists */
	bool VerifyFileExists(const FString& InFileName) const;
};
