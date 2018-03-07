// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AssetTypeActions_Base.h"

class FMenuBuilder;

/** Base asset type actions for any classes with gameplay tagging */
class GAMEPLAYTAGSEDITOR_API FAssetTypeActions_GameplayTagAssetBase : public FAssetTypeActions_Base
{
public:

	/** Constructor */
	FAssetTypeActions_GameplayTagAssetBase(FName InTagPropertyName);

	/** Overridden to specify that the gameplay tag base has actions */
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override;

	/** Overridden to offer the gameplay tagging options */
	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override;

	/** Overridden to specify misc category */
	virtual uint32 GetCategories() override;

private:
	/**
	 * Open the gameplay tag editor
	 * 
	 * @param TagAssets	Assets to open the editor with
	 */
	void OpenGameplayTagEditor(TArray<class UObject*> Objects, TArray<struct FGameplayTagContainer*> Containers);

	/** Name of the property of the owned gameplay tag container */
	FName OwnedGameplayTagPropertyName;
};
