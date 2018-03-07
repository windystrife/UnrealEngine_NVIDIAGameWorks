// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class FMenuBuilder;
class USoundMod;

class FAssetTypeActions_SoundMod: public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override{ return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SoundMod", "Sound Mod"); }
	virtual FColor GetTypeColor() const override{ return FColor(255, 175, 0); }
	virtual UClass* GetSupportedClass() const override;
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override{ return true; }
	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override;
	virtual void AssetsActivated(const TArray<UObject*>& InObjects, EAssetTypeActivationMethod::Type ActivationType) override;
	virtual uint32 GetCategories() override{ return EAssetTypeCategories::Sounds; }
	virtual bool CanFilter() override { return false; }

private:
	/** Handler for when PlaySound is selected */
	void ExecutePlaySound(TArray<TWeakObjectPtr<USoundMod>> Objects);

	/** Handler for when StopSound is selected */
	void ExecuteStopSound(TArray<TWeakObjectPtr<USoundMod>> Objects);

	/** Returns true if only one sound is selected to play */
	bool CanExecutePlayCommand(TArray<TWeakObjectPtr<USoundMod>> Objects) const;

	/** Plays the specified sound wave */
	void PlaySound(USoundMod* Sound);

	/** Stops any currently playing sounds */
	void StopSound();
};
