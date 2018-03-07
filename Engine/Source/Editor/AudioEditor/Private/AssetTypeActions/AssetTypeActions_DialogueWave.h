// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"
#include "Sound/DialogueWave.h"

class FMenuBuilder;

class FAssetTypeActions_DialogueWave : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_DialogueWave", "Dialogue Wave"); }
	virtual FColor GetTypeColor() const override { return FColor(97, 85, 212); }
	virtual UClass* GetSupportedClass() const override { return UDialogueWave::StaticClass(); }
	virtual uint32 GetCategories() override { return EAssetTypeCategories::Sounds; }
	virtual bool CanFilter() override { return true; }
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override { return true; }
	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override;
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;

private:

	bool CanExecutePlayCommand(TArray<TWeakObjectPtr<UDialogueWave>> Objects) const;

	void ExecutePlaySound(TArray<TWeakObjectPtr<UDialogueWave>> Objects);

	void ExecuteStopSound(TArray<TWeakObjectPtr<UDialogueWave>> Objects);

	void PlaySound(UDialogueWave* DialogueWave);

	void StopSound();

	void ExecuteCreateSoundCue(TArray<TWeakObjectPtr<UDialogueWave>> Objects);

};
