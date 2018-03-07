// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "NiagaraTypes.h"
#include "INiagaraCompiler.h"
#include "AssetTypeCategories.h"

class IAssetTools;
class IAssetTypeActions;
struct FNiagaraCompileResults;
class INiagaraEditorTypeUtilities;
class UNiagaraSettings;
class USequencerSettings;
class UNiagaraStackViewModel;

DECLARE_STATS_GROUP(TEXT("Niagara Editor"), STATGROUP_NiagaraEditor, STATCAT_Advanced);

/** Niagara Editor module */
class FNiagaraEditorModule : public IModuleInterface,
	public IHasMenuExtensibility, public IHasToolBarExtensibility, public FGCObject
{
public:
	DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<SWidget>, FOnCreateStackWidget, UNiagaraStackViewModel*);

public:
	FNiagaraEditorModule();

	// IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Compile the specified script. */
	virtual ENiagaraScriptCompileStatus CompileScript(class UNiagaraScript* ScriptToCompile, FString& OutGraphLevelErrorMessages);

	/** Gets the extensibility managers for outside entities to extend static mesh editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() override {return MenuExtensibilityManager;}
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() override {return ToolBarExtensibilityManager;}

	/** Registers niagara editor type utilities for a specific type. */
	void RegisterTypeUtilities(FNiagaraTypeDefinition Type, TSharedRef<INiagaraEditorTypeUtilities> EditorUtilities);

	/** Register/unregister niagara editor settings. */
	void RegisterSettings();
	void UnregisterSettings();

	/** Gets niagara editor type utilities for a specific type if there are any registered. */
	TSharedPtr<INiagaraEditorTypeUtilities> NIAGARAEDITOR_API GetTypeUtilities(const FNiagaraTypeDefinition& Type);

	static EAssetTypeCategories::Type GetAssetCategory() { return NiagaraAssetCategory; }

	TSharedRef<SWidget> CreateStackWidget(UNiagaraStackViewModel* StackViewModel) const;

	FDelegateHandle NIAGARAEDITOR_API SetOnCreateStackWidget(FOnCreateStackWidget InOnCreateStackWidget);
	void NIAGARAEDITOR_API ResetOnCreateStackWidget(FDelegateHandle DelegateHandle);


	/** Niagara Editor app identifier string */
	static const FName NiagaraEditorAppIdentifier;

	/** The tab color scale for niagara editors. */
	static const FLinearColor WorldCentricTabColorScale;

private:
	void RegisterAssetTypeAction(IAssetTools& AssetTools, TSharedRef<IAssetTypeActions> Action);
	void OnNiagaraSettingsChangedEvent(const FString& PropertyName, const UNiagaraSettings* Settings);


	/** FGCObject interface */
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;

private:
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
	TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;

	/** All created asset type actions.  Cached here so that we can unregister it during shutdown. */
	TArray< TSharedPtr<IAssetTypeActions> > CreatedAssetTypeActions;

	TMap<FNiagaraTypeDefinition, TSharedRef<INiagaraEditorTypeUtilities>> TypeToEditorUtilitiesMap;

	static EAssetTypeCategories::Type NiagaraAssetCategory;

	FDelegateHandle CreateEmitterTrackEditorHandle;
	FDelegateHandle CreateSystemTrackEditorHandle;
	FDelegateHandle CreateFloatParameterTrackEditorHandle;
	FDelegateHandle CreateBoolParameterTrackEditorHandle;

	USequencerSettings* SequencerSettings;
	FOnCreateStackWidget OnCreateStackWidget;
};


