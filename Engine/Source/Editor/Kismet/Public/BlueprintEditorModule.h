// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Engine/Blueprint.h"
#include "Framework/Commands/UICommandList.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "WorkflowOrientedApp/WorkflowCentricApplication.h"

class FSCSEditorTreeNode;
class UUserDefinedEnum;
class UUserDefinedStruct;
struct Rect;

extern const FName BlueprintEditorAppName;

class IBlueprintEditor;
class FBlueprintEditor;
class UUserDefinedEnum;
class UUserDefinedStruct;
class IDetailCustomization;


/** Delegate used to customize variable display */
DECLARE_DELEGATE_RetVal_OneParam(TSharedPtr<IDetailCustomization>, FOnGetVariableCustomizationInstance, TSharedPtr<IBlueprintEditor> /*BlueprintEditor*/);

/** Describes the reason for Refreshing the editor */
namespace ERefreshBlueprintEditorReason
{
	enum Type
	{
		BlueprintCompiled,
		UnknownReason
	};
}

/**
 * Enum editor public interface
 */
class KISMET_API IUserDefinedEnumEditor : public FAssetEditorToolkit
{
};

/**
 * Enum editor public interface
 */
class KISMET_API IUserDefinedStructureEditor : public FAssetEditorToolkit
{
};

/**
 * Blueprint editor public interface
 */
class KISMET_API IBlueprintEditor : public FWorkflowCentricApplication
{
public:
	virtual void JumpToHyperlink(const UObject* ObjectReference, bool bRequestRename) = 0;
	virtual void JumpToPin(const UEdGraphPin* PinToFocusOn) = 0;

	/** Invokes the search UI and sets the mode and search terms optionally */
	virtual void SummonSearchUI(bool bSetFindWithinBlueprint, FString NewSearchTerms = FString(), bool bSelectFirstResult = false) = 0;

	/** Invokes the Find and Replace UI */
	virtual void SummonFindAndReplaceUI() = 0;

	virtual void RefreshEditors(ERefreshBlueprintEditorReason::Type Reason = ERefreshBlueprintEditorReason::UnknownReason) = 0;

	virtual void AddToSelection(UEdGraphNode* InNode) = 0;

	virtual bool CanPasteNodes() const= 0;

	virtual void PasteNodesHere(class UEdGraph* Graph, const FVector2D& Location) = 0;

	virtual bool GetBoundsForSelectedNodes(class FSlateRect& Rect, float Padding ) = 0;

	/** Util to get the currently selected SCS editor tree Nodes */
	virtual TArray<TSharedPtr<class FSCSEditorTreeNode> >  GetSelectedSCSEditorTreeNodes() const = 0;

	/** Get number of currently selected nodes in the SCS editor tree */
	virtual int32 GetNumberOfSelectedNodes() const = 0;

	/** Find and select a specific SCS editor tree node associated with the given component */
	virtual TSharedPtr<class FSCSEditorTreeNode> FindAndSelectSCSEditorTreeNode(const class UActorComponent* InComponent, bool IsCntrlDown) = 0;

	/** Used to track node create/delete events for Analytics */
	virtual void AnalyticsTrackNodeEvent( UBlueprint* Blueprint, UEdGraphNode *GraphNode, bool bNodeDelete = false ) const = 0;

};

DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<class ISCSEditorCustomization>, FSCSEditorCustomizationBuilder, TSharedRef< IBlueprintEditor > /* InBlueprintEditor */);

/**
 * The blueprint editor module provides the blueprint editor application.
 */
class FBlueprintEditorModule : public IModuleInterface,
	public IHasMenuExtensibility
{
public:
	// IModuleInterface interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End of IModuleInterface interface

	/**
	 * Creates an instance of a Kismet editor object.  Only virtual so that it can be called across the DLL boundary.
	 *
	 * Note: This function should not be called directly, use one of the following instead:
	 *	- FKismetEditorUtilities::BringKismetToFocusAttentionOnObject
	 *  - FAssetEditorManager::Get().OpenEditorForAsset
	 *
	 * @param	Mode					Mode that this editor should operate in
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	Blueprint				The blueprint object to start editing
	 * @param	bShouldOpenInDefaultsMode	If true, the editor will open in defaults editing mode
	 *
	 * @return	Interface to the new Blueprint editor
	 */
	virtual TSharedRef<IBlueprintEditor> CreateBlueprintEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, UBlueprint* Blueprint, bool bShouldOpenInDefaultsMode = false);
	virtual TSharedRef<IBlueprintEditor> CreateBlueprintEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, const TArray< UBlueprint* >& BlueprintsToEdit );

	/**
	 * Creates an instance of a Enum editor object.
	 *
	 * @param	Mode					Mode that this editor should operate in
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	UDEnum					The user-defined Enum to start editing
	 *
	 * @return	Interface to the new Enum editor
	 */
	virtual TSharedRef<IUserDefinedEnumEditor> CreateUserDefinedEnumEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UUserDefinedEnum* UDEnum);

	/**
	 * Creates an instance of a Structure editor object.
	 *
	 * @param	Mode					Mode that this editor should operate in
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	UDEnum					The user-defined structure to start editing
	 *
	 * @return	Interface to the new Struct editor
	 */
	virtual TSharedRef<IUserDefinedStructureEditor> CreateUserDefinedStructEditor(const EToolkitMode::Type Mode, const TSharedPtr<IToolkitHost>& InitToolkitHost, UUserDefinedStruct* UDStruct);

	/** Gets the extensibility managers for outside entities to extend blueprint editor's menus and toolbars */
	virtual TSharedPtr<FExtensibilityManager> GetMenuExtensibilityManager() override { return MenuExtensibilityManager; }

	/**  */
	DECLARE_EVENT_TwoParams(FBlueprintEditorModule, FBlueprintMenuExtensionEvent, TSharedPtr<FExtender>, UBlueprint*);
	FBlueprintMenuExtensionEvent& OnGatherBlueprintMenuExtensions() { return GatherBlueprintMenuExtensions; }

	DECLARE_EVENT_ThreeParams(IBlueprintEditor, FOnRegisterTabs, FWorkflowAllowedTabSet&, FName /** ModeName */, TSharedPtr<FBlueprintEditor>);
	FOnRegisterTabs& OnRegisterTabsForEditor() { return RegisterTabsForEditor; }

	DECLARE_EVENT_OneParam(IBlueprintEditor, FOnRegisterLayoutExtensions, FLayoutExtender&);
	FOnRegisterLayoutExtensions& OnRegisterLayoutExtensions() { return RegisterLayoutExtensions; }

	/** 
	 * Register a customization for interacting with the SCS editor 
	 * @param	InComponentName			The name of the component to customize behavior for
	 * @param	InCustomizationBuilder	The delegate used to create customization instances
	 */
	virtual void RegisterSCSEditorCustomization(const FName& InComponentName, FSCSEditorCustomizationBuilder InCustomizationBuilder);

	/** 
	 * Unregister a previously registered customization for interacting with the SCS editor 
	 * @param	InComponentName			The name of the component to customize behavior for
	 */
	virtual void UnregisterSCSEditorCustomization(const FName& InComponentName);

	/** 
	 * Register a customization for for Blueprint variables
	 * @param	InStruct				The type of the variable to create the customization for
	 * @param	InOnGetDetailCustomization	The delegate used to create customization instances
	 */
	virtual void RegisterVariableCustomization(UStruct* InStruct, FOnGetVariableCustomizationInstance InOnGetVariableCustomization);

	/** 
	 * Unregister a previously registered customization for BP variables
	 * @param	InStruct				The type to create the customization for
	 */
	virtual void UnregisterVariableCustomization(UStruct* InStruct);

	/** 
	 * Build a set of details customizations for the passed-in type, if possible.
	 * @param	InStruct				The type to create the customization for
	 * @param	InBlueprintEditor		The Blueprint Editor the customization will be created for
	 */
	virtual TArray<TSharedPtr<IDetailCustomization>> CustomizeVariable(UStruct* InStruct, TSharedPtr<IBlueprintEditor> InBlueprintEditor);

	/** Delegate for binding functions to be called when the blueprint editor finishes getting created */
	DECLARE_EVENT_OneParam( FBlueprintEditorModule, FBlueprintEditorOpenedEvent, EBlueprintType );
	FBlueprintEditorOpenedEvent& OnBlueprintEditorOpened() { return BlueprintEditorOpened; }

	/** 
	 * Exposes a way for other modules to fold in their own Blueprint editor 
	 * commands (folded in with other BP editor commands, when the editor is 
	 * first opened).
	 */
	virtual const TSharedRef<FUICommandList> GetsSharedBlueprintEditorCommands() const { return SharedBlueprintEditorCommands.ToSharedRef(); }

private:
	/** Loads from ini a list of all events that should be auto created for Blueprints of a specific class */
	void PrepareAutoGeneratedDefaultEvents();

private:
	TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;

	//
	FBlueprintMenuExtensionEvent GatherBlueprintMenuExtensions;

	/** Event called to allow external clients to register additional tabs for the specified editor */
	FOnRegisterTabs RegisterTabsForEditor;
	FOnRegisterLayoutExtensions RegisterLayoutExtensions;

	// Event to be called when the blueprint editor is opened
	FBlueprintEditorOpenedEvent BlueprintEditorOpened;

	/** Customizations for the SCS editor */
	TMap<FName, FSCSEditorCustomizationBuilder> SCSEditorCustomizations;

	/** Customizations for Blueprint variables */
	TMap<UStruct*, FOnGetVariableCustomizationInstance> VariableCustomizations;

	/** 
	 * A command list that can be passed around and isn't bound to an instance 
	 * of the blueprint editor. 
	 */
	TSharedPtr<FUICommandList> SharedBlueprintEditorCommands;

	/** Handle to a registered LevelViewportContextMenuBlueprintExtender delegate */
	FDelegateHandle LevelViewportContextMenuBlueprintExtenderDelegateHandle;

	/** Reference to keep our custom configuration panel alive */
	TSharedPtr<SWidget> ConfigurationPanel;
};
