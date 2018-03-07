// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PersonaAssetFamilyManager.h"
#include "AssetData.h"
#include "IAssetFamily.h"
#include "PersonaAssetFamily.h"

FPersonaAssetFamilyManager& FPersonaAssetFamilyManager::Get()
{
	static FPersonaAssetFamilyManager TheManager;
	return TheManager;
}

TSharedRef<IAssetFamily> FPersonaAssetFamilyManager::CreatePersonaAssetFamily(const UObject* InAsset)
{
	// compact any invalid entries
	AssetFamilies.RemoveAll([](const TWeakPtr<class IAssetFamily>& InAssetFamily) { return !InAssetFamily.IsValid(); });

	// look for an existing matching asset family
	FAssetData AssetData(InAsset);
	for (TWeakPtr<class IAssetFamily>& AssetFamily : AssetFamilies)
	{
		if (AssetFamily.Pin()->IsAssetCompatible(AssetData))
		{
			return AssetFamily.Pin().ToSharedRef();
		}
	}

	// not found - make a new one
	TSharedRef<IAssetFamily> NewAssetFamily = MakeShareable(new FPersonaAssetFamily(InAsset));
	AssetFamilies.Add(NewAssetFamily);
	return NewAssetFamily;
}
