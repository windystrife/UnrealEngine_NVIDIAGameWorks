// @third party code - BEGIN HairWorks
#pragma once

#include "AssetTypeActions_Base.h"

class FAssetTypeActions_HairWorks : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override { return FColor(118, 185, 0); }
	virtual UClass* GetSupportedClass() const override;
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override { return true; }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Basic; }
	virtual bool IsImportedAsset() const override { return true; }
	virtual void GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const override;
};
// @third party code - END HairWorks
