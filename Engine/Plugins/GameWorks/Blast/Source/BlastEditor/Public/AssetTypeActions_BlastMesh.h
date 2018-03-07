#pragma once
#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class UBlastMesh;

class FAssetTypeActions_BlastMesh : public FAssetTypeActions_Base
{
	virtual FText GetName() const override { return NSLOCTEXT("NvBlast", "AssetTypeActions_BlastMesh", "Blast Mesh"); }

	virtual UClass* GetSupportedClass() const override;

	virtual FColor GetTypeColor() const override { return FColor::Emerald; }

	virtual uint32 GetCategories() override { return EAssetTypeCategories::Physics; }



	virtual bool HasActions(const TArray<UObject *>& InObjects) const override { return true; }

	virtual bool IsImportedAsset() const override { return true; }


	virtual void GetActions(const TArray<UObject *>& InObjects, FMenuBuilder& MenuBuilder) override;

	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;

};