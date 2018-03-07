// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SCompoundWidget.h"
#include "DeclarativeSyntaxSupport.h"
#include "GraphEditor.h"
#include "EdGraph/EdGraphNode.h"

class FNiagaraScriptGraphViewModel;

/** A widget for editing a UNiagaraScript with a graph. */
class SNiagaraScriptGraph : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SNiagaraScriptGraph)
	{}
		/** The text displayed in the title bar of the graph. */
		SLATE_ATTRIBUTE(FText, GraphTitle)

	SLATE_END_ARGS();

	void Construct(const FArguments& InArgs, TSharedRef<FNiagaraScriptGraphViewModel> InViewModel);


private:
	/** Constructs the graph editor widget for the current graph. */
	TSharedRef<SGraphEditor> ConstructGraphEditor();

	/** Called whenever the selected nodes on the script view model changes. */
	void ViewModelSelectedNodesChanged();

	/** Called whenever the selected nodes in the graph editor changes. */
	void GraphEditorSelectedNodesChanged(const TSet<UObject*>& SelectedNodes);

	/** Called when a node is double clicked. */
	void OnNodeDoubleClicked(UEdGraphNode* ClickedNode);

	/** Called when nodes are based in the script view model. */
	void NodesPasted(const TSet<UEdGraphNode*>& PastedNodes);

	/** Sets the position on a group of newly pasted nodes. */
	void PositionPastedNodes(const TSet<UEdGraphNode*>& PastedNodes);

	/** Called whenever the view model's graph changes to a different graph. */
	void GraphChanged();

	/** Called whenever a user edits the name inline of a node.*/
	void OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged);

	/** Called whenever a user left-clicks on the graph with a chord pressed.*/
	FReply OnSpawnGraphNodeByShortcut(FInputChord InChord, const FVector2D& InPosition);

	/** Called whenever a user is trying to edit the name inline of a node and we want to make sure that it is valid.*/
	bool OnVerifyNodeTextCommit(const FText& NewText, UEdGraphNode* NodeBeingChanged, FText& OutErrorMessage);

private:
	/** An attribute for the title text of the graph. */
	TAttribute<FText> GraphTitle;

	/** The view model which exposes the data used by the widget. */
	TSharedPtr<FNiagaraScriptGraphViewModel> ViewModel;

	/** The graph editor which is editing the script graph. */
	TSharedPtr<SGraphEditor> GraphEditor;

	/** Flag to prevent modifying the view model selection when updating the graph
		editor selection due to a view model selection change. */
	bool bUpdatingGraphSelectionFromViewModel;
};