// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GraphEditor.h"
#include "ISoundSubmixEditor.h"
#include "EditorUndoClient.h"

//////////////////////////////////////////////////////////////////////////
// FSoundSubmixEditor

class USoundSubmix;

class FSoundSubmixEditor :	public ISoundSubmixEditor, 
							public FGCObject, 
							public FEditorUndoClient
{
public:
	SLATE_BEGIN_ARGS(FSoundSubmixEditor)
	{
	}

	SLATE_END_ARGS()

	virtual void RegisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;
	virtual void UnregisterTabSpawners(const TSharedRef<class FTabManager>& TabManager) override;

	/**
	 * Edits the specified sound submix object
	 *
	 * @param	Mode					Asset editing mode for this editor (standalone or world-centric)
	 * @param	InitToolkitHost			When Mode is WorldCentric, this is the level editor instance to spawn this editor within
	 * @param	ObjectToEdit			The sound submix to edit
	 */
	void InitSoundSubmixEditor( const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UObject* ObjectToEdit );

	FSoundSubmixEditor();
	virtual ~FSoundSubmixEditor();

	/** FGCObject interface */
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;

	/** IToolkit interface */
	virtual FName GetToolkitFName() const override;
	virtual FText GetBaseToolkitName() const override;
	virtual FString GetWorldCentricTabPrefix() const override;

	/** @return Returns the color and opacity to use for the color that appears behind the tab text for this toolkit's tab in world-centric mode. */
	virtual FLinearColor GetWorldCentricTabColorScale() const override;

	//~ Begin ISoundSubmixEditor
	void CreateSoundSubmix(class UEdGraphPin* FromPin, FVector2D Location, const FString& Name) override;
	//~ End ISoundSubmixEditor

	/** FEditorUndoClient Interface */
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override { PostUndo(bSuccess); }

private:
	TSharedRef<SDockTab> SpawnTab_GraphCanvas(const FSpawnTabArgs& Args);
	TSharedRef<SDockTab> SpawnTab_Properties(const FSpawnTabArgs& Args);

private:
	/** Creates all internal widgets for the tabs to point at */
	void CreateInternalWidgets();

	/** Create new graph editor widget */
	TSharedRef<SGraphEditor> CreateGraphEditorWidget();

	/** Called when the selection changes in the GraphEditor */
	void OnSelectedNodesChanged(const TSet<class UObject*>& NewSelection);

	/** Called to create context menu when right-clicking on graph */
	FActionMenuContent OnCreateGraphActionMenu(UEdGraph* InGraph, const FVector2D& InNodePosition, const TArray<UEdGraphPin*>& InDraggedPins, bool bAutoExpand, SGraphEditor::FActionMenuClosed InOnMenuClosed);

	/** Select every node in the graph */
	void SelectAllNodes();
	
	/** Whether we can select every node */
	bool CanSelectAllNodes() const;

	/** Remove the currently selected nodes from editor view*/
	void RemoveSelectedNodes();

	/** Whether we are able to remove the currently selected nodes */
	bool CanRemoveNodes() const;

	/** Called to undo the last action */
	void UndoGraphAction();
	
	/** Called to redo the last undone action */
	void RedoGraphAction();

private:
	/** The SoundSubmix asset being inspected */
	USoundSubmix* SoundSubmix;

	/** List of open tool panels; used to ensure only one exists at any one time */
	TMap< FName, TWeakPtr<class SDockableTab> > SpawnedToolPanels;

	/** Graph Editor */
	TSharedPtr<SGraphEditor> GraphEditor;

	/** Property View */
	TSharedPtr<class IDetailsView> DetailsView;

	/** Command list for this editor */
	TSharedPtr<FUICommandList> GraphEditorCommands;

	/**	The tab ids for all the tabs used */
	static const FName GraphCanvasTabId;
	static const FName PropertiesTabId;
};