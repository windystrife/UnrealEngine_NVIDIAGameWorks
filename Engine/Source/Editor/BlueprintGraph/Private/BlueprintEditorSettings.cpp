// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorSettings.h"
#include "Misc/ConfigCacheIni.h"
#include "Editor/EditorPerProjectUserSettings.h"
#include "Settings/EditorExperimentalSettings.h"
#include "Toolkits/AssetEditorManager.h"
#include "FindInBlueprintManager.h"

UBlueprintEditorSettings::UBlueprintEditorSettings(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	// Style Settings
	, bDrawMidpointArrowsInBlueprints(false)
	, bShowGraphInstructionText(true)
	// Workflow Settings
	, bSplitContextTargetSettings(true)
	, bExposeAllMemberComponentFunctions(true)
	, bShowContextualFavorites(false)
	, bCompactCallOnMemberNodes(false)
	, bFlattenFavoritesMenus(true)
	, bFavorPureCastNodes(false)
	, bAutoCastObjectConnections(false)
	, bShowViewportOnSimulate(false)
	, bShowInheritedVariables(false)
	, bShowEmptySections(true)
	, bSpawnDefaultBlueprintNodes(true)
	, bHideConstructionScriptComponentsInDetailsView(true)
	, bHostFindInBlueprintsInGlobalTab(true)
	// Compiler Settings
	, SaveOnCompile(SoC_Never)
	, bJumpToNodeErrors(false)
	, bAllowExplicitImpureNodeDisabling(false)
	// Developer Settings
	, bShowActionMenuItemSignatures(false)
	// Perf Settings
	, bShowDetailedCompileResults(false)
	, CompileEventDisplayThresholdMs(5)
	, NodeTemplateCacheCapMB(20.f)
{
	// settings that were moved out of experimental...
	UEditorExperimentalSettings const* ExperimentalSettings = GetDefault<UEditorExperimentalSettings>();
	bDrawMidpointArrowsInBlueprints = ExperimentalSettings->bDrawMidpointArrowsInBlueprints;

	// settings that were moved out of editor-user settings...
	UEditorPerProjectUserSettings const* UserSettings = GetDefault<UEditorPerProjectUserSettings>();
	bShowActionMenuItemSignatures = UserSettings->bDisplayActionListItemRefIds;

	FString const ClassConfigKey = GetClass()->GetPathName();

	bool bOldSaveOnCompileVal = false;
	// backwards compatibility: handle the case where users have already switched this on
	if (GConfig->GetBool(*ClassConfigKey, TEXT("bSaveOnCompile"), bOldSaveOnCompileVal, GEditorPerProjectIni) && bOldSaveOnCompileVal)
	{
		SaveOnCompile = SoC_SuccessOnly;
	}
}

void UBlueprintEditorSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = (PropertyChangedEvent.Property != nullptr) ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropertyName == GET_MEMBER_NAME_CHECKED(UBlueprintEditorSettings, bHostFindInBlueprintsInGlobalTab))
	{
		// Close all open Blueprint editors to reset associated FiB states.
		FAssetEditorManager& AssetEditorManager = FAssetEditorManager::Get();
		TArray<UObject*> EditedAssets = AssetEditorManager.GetAllEditedAssets();
		for (UObject* EditedAsset : EditedAssets)
		{
			if (EditedAsset->IsA<UBlueprint>())
			{
				AssetEditorManager.CloseAllEditorsForAsset(EditedAsset);
			}
		}

		// Enable or disable the feature through the FiB manager.
		FFindInBlueprintSearchManager::Get().EnableGlobalFindResults(bHostFindInBlueprintsInGlobalTab);
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}