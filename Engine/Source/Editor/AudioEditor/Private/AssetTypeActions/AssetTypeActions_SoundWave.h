// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions/AssetTypeActions_SoundBase.h"

class FMenuBuilder;
class USoundWave;

class FAssetTypeActions_SoundWave : public FAssetTypeActions_SoundBase
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SoundWave", "Sound Wave"); }
	virtual FColor GetTypeColor() const override { return FColor(97, 85, 212); }
	virtual UClass* GetSupportedClass() const override;
	virtual void GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder ) override;
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual bool CanFilter() override { return true; }
	virtual bool IsImportedAsset() const override { return true; }
	virtual void GetResolvedSourceFilePaths(const TArray<UObject*>& TypeAssets, TArray<FString>& OutSourceFilePaths) const override;
	virtual TSharedPtr<SWidget> GetThumbnailOverlay(const FAssetData& AssetData) const override;

private:

	/** Creates a SoundCue of the same name for the sound, if one does not already exist */
	void ExecuteCreateSoundCue(TArray<TWeakObjectPtr<USoundWave>> Objects);

	/** Creates a DialogueWave of the same name for the sound, if one does not already exist */
	void ExecuteCreateDialogueWave(const struct FAssetData& AssetData, TArray<TWeakObjectPtr<USoundWave>> Objects);

	void FillVoiceMenu(FMenuBuilder& MenuBuilder, TArray<TWeakObjectPtr<USoundWave>> Objects);
};
