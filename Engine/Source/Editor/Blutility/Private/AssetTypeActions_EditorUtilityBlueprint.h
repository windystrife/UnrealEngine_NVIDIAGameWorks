// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "EditorUtilityBlueprint.h"
#include "Toolkits/IToolkitHost.h"
#include "AssetTypeActions_Base.h"

class FMenuBuilder;

class FAssetTypeActions_EditorUtilityBlueprint : public FAssetTypeActions_Base
{
public:
	// IAssetTypeActions interface
	virtual FText GetName() const override;
	virtual FColor GetTypeColor() const override;
	virtual UClass* GetSupportedClass() const override;
	virtual bool HasActions(const TArray<UObject*>& InObjects) const override;
	virtual void GetActions(const TArray<UObject*>& InObjects, FMenuBuilder& MenuBuilder) override;
	virtual void OpenAssetEditor(const TArray<UObject*>& InObjects, TSharedPtr<class IToolkitHost> EditWithinLevelEditor = TSharedPtr<IToolkitHost>()) override;
	virtual uint32 GetCategories() override;
	virtual bool CanLocalize() const override { return false; }
	// End of IAssetTypeActions interface

protected:
	typedef TArray< TWeakObjectPtr<class UEditorUtilityBlueprint> > FWeakBlueprintPointerArray;

	void ExecuteEdit(FWeakBlueprintPointerArray Objects);
	void ExecuteEditDefaults(FWeakBlueprintPointerArray Objects);
	void ExecuteNewDerivedBlueprint(TWeakObjectPtr<class UEditorUtilityBlueprint> InObject);
	void ExecuteGlobalBlutility(TWeakObjectPtr<class UEditorUtilityBlueprint> InObject);

	/** Returns the tooltip to display when attempting to derive a Blueprint */
	FText GetNewDerivedBlueprintTooltip(TWeakObjectPtr<class UEditorUtilityBlueprint> InObject);

	/** Returns TRUE if you can derive a Blueprint */
	bool CanExecuteNewDerivedBlueprint(TWeakObjectPtr<class UEditorUtilityBlueprint> InObject);
};
