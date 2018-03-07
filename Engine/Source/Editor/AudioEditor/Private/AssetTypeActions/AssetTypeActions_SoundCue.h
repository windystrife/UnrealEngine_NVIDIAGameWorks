// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions/AssetTypeActions_SoundBase.h"

class FMenuBuilder;
class USoundCue;

class FAssetTypeActions_SoundCue : public FAssetTypeActions_SoundBase
{
public:
	// IAssetTypeActions Implementation
	virtual FText GetName() const override { return NSLOCTEXT("AssetTypeActions", "AssetTypeActions_SoundCue", "Sound Cue"); }
	virtual FColor GetTypeColor() const override { return FColor(0, 175, 255); }
	virtual UClass* GetSupportedClass() const override;
	virtual void GetActions( const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder ) override;
	virtual void OpenAssetEditor( const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>() ) override;
	virtual bool CanFilter() override { return true; }

private:
	/** Take selected SoundCues and combine, as much as possible, them to using shared attenuation settings */
	void ExecuteConsolidateAttenuation(TArray<TWeakObjectPtr<USoundCue>> Objects);

	/** Returns true if more than one cue is selected to consolidate */
	bool CanExecuteConsolidateCommand(TArray<TWeakObjectPtr<USoundCue>> Objects) const;
};
