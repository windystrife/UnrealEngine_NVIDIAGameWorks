#pragma once

#include "AssetTypeActions_Base.h"
#include "AssetToolsModule.h"
#include "AssetRegistryModule.h"

// NvFlow begin
class FAssetTypeActions_FlowMaterial : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_FlowMaterial", "Flow Material"); }
	virtual FColor GetTypeColor() const override { return FColor(255,192,128); }
	virtual UClass* GetSupportedClass() const override { return UFlowMaterial::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Physics; }
};
// NvFlow end
