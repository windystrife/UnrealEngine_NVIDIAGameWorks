// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Framework/Commands/UICommandList.h"
#include "GraphEditor.h"
#include "Modules/ModuleInterface.h"

class FExtender;
class UEdGraph;

/**
 * Graph editor public interface
 */
class FGraphEditorModule : public IModuleInterface
{

public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

	/** Delegates to be called to extend the graph menus */
	
	DECLARE_DELEGATE_RetVal_FiveParams( TSharedRef<FExtender>, FGraphEditorMenuExtender_SelectedNode, const TSharedRef<FUICommandList>, const UEdGraph*, const UEdGraphNode*, const UEdGraphPin*, bool);
	virtual TArray<FGraphEditorMenuExtender_SelectedNode>& GetAllGraphEditorContextMenuExtender() {return GraphEditorContextMenuExtender;}

private:
	friend class SGraphEditor;

	/**
	 * DO NOT CALL THIS METHOD. Use SNew(SGraphEditor) to make instances of SGraphEditor.
	 *
	 * @return A GraphEditor implementation.
	 */
	virtual TSharedRef<SGraphEditor> PRIVATE_MakeGraphEditor(
		const TSharedPtr<FUICommandList>& InAdditionalCommands,
		const TAttribute<bool>& InIsEditable,
		const TAttribute<bool>& InDisplayAsReadOnly,
		const TAttribute<bool>& InIsEmpty,
		TAttribute<FGraphAppearanceInfo> Appearance,
		TSharedPtr<SWidget> InTitleBar,
		UEdGraph* InGraphToEdit,
		SGraphEditor::FGraphEditorEvents GraphEvents,
		bool InAutoExpandActionMenu,
		UEdGraph* InGraphToDiff,
		FSimpleDelegate InOnNavigateHistoryBack,
		FSimpleDelegate InOnNavigateHistoryForward,
		TAttribute<bool> ShowGraphStateOverlay);

private:
	/** All extender delegates for the graph menus */
	TArray<FGraphEditorMenuExtender_SelectedNode> GraphEditorContextMenuExtender;
};
