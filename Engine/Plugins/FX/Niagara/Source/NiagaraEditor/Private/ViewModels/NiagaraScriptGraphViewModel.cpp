// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NiagaraScriptGraphViewModel.h"
#include "NiagaraScript.h"
#include "NiagaraScriptSource.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraGraph.h"
#include "NiagaraNodeInput.h"

#include "GenericCommands.h"
#include "ScopedTransaction.h"
#include "EdGraphUtilities.h"
#include "Editor.h"
#include "EditorStyleSet.h"

#include "HAL/PlatformApplicationMisc.h"

#define LOCTEXT_NAMESPACE "NiagaraScriptGraphViewModel"

FNiagaraScriptGraphViewModel::FNiagaraScriptGraphViewModel(UNiagaraScriptSource* InScriptSrc, FText InDisplayName)
	: ScriptSource(InScriptSrc)
	, DisplayName(InDisplayName)
	, Commands(MakeShareable(new FUICommandList()))
	, Selection(MakeShareable(new FNiagaraObjectSelection()))
{
	SetupCommands();
	GEditor->RegisterForUndo(this);
	ErrorColor = FEditorStyle::GetColor("ErrorReporting.BackgroundColor");
}

FNiagaraScriptGraphViewModel::~FNiagaraScriptGraphViewModel()
{
	GEditor->UnregisterForUndo(this);
}

void FNiagaraScriptGraphViewModel::SetScriptSource(UNiagaraScriptSource* InScriptSrc)
{
	Selection->ClearSelectedObjects();
	ScriptSource = InScriptSrc;
	OnGraphChangedDelegate.Broadcast();
}

FText FNiagaraScriptGraphViewModel::GetDisplayName() const
{
	return DisplayName;
}

UNiagaraScriptSource* FNiagaraScriptGraphViewModel::GetScriptSource()
{
	return ScriptSource.Get();
}

UNiagaraGraph* FNiagaraScriptGraphViewModel::GetGraph() const
{
	if (ScriptSource.IsValid() && ScriptSource != nullptr)
	{
		return ScriptSource->NodeGraph;
	}
	return nullptr;
}

TSharedRef<FUICommandList> FNiagaraScriptGraphViewModel::GetCommands()
{
	return Commands;
}

TSharedRef<FNiagaraObjectSelection> FNiagaraScriptGraphViewModel::GetSelection()
{
	return Selection;
}

FNiagaraScriptGraphViewModel::FOnNodesPasted& FNiagaraScriptGraphViewModel::OnNodesPasted()
{
	return OnNodesPastedDelegate;
}

FNiagaraScriptGraphViewModel::FOnGraphChanged& FNiagaraScriptGraphViewModel::OnGraphChanged()
{
	return OnGraphChangedDelegate;
}

EVisibility FNiagaraScriptGraphViewModel::GetGraphErrorTextVisible() const
{
	return ErrorMsg.Len() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
}

FText FNiagaraScriptGraphViewModel::GetGraphErrorText() const
{
	return LOCTEXT("GraphErrorText", "ERROR");
}

FSlateColor FNiagaraScriptGraphViewModel::GetGraphErrorColor() const
{
	return ErrorColor;
}

FText FNiagaraScriptGraphViewModel::GetGraphErrorMsgToolTip() const
{
	return FText::FromString(ErrorMsg);
}

void FNiagaraScriptGraphViewModel::SetErrorTextToolTip(FString ErrorMsgToolTip)
{
	ErrorMsg = ErrorMsgToolTip;
}

void FNiagaraScriptGraphViewModel::PostUndo(bool bSuccess)
{
	// The graph may have been deleted as a result of the undo operation so make sure it's valid
	// before using it.
	UNiagaraGraph* Graph = GetGraph();
	if (Graph != nullptr)
	{
		Graph->NotifyGraphChanged();
	}
}

void FNiagaraScriptGraphViewModel::SetupCommands()
{
	Commands->MapAction(
		FGenericCommands::Get().SelectAll,
		FExecuteAction::CreateRaw(this, &FNiagaraScriptGraphViewModel::SelectAllNodes));

	Commands->MapAction(
		FGenericCommands::Get().Delete,
		FExecuteAction::CreateRaw(this, &FNiagaraScriptGraphViewModel::DeleteSelectedNodes),
		FCanExecuteAction::CreateRaw(this, &FNiagaraScriptGraphViewModel::CanDeleteNodes));

	Commands->MapAction(
		FGenericCommands::Get().Copy,
		FExecuteAction::CreateRaw(this, &FNiagaraScriptGraphViewModel::CopySelectedNodes),
		FCanExecuteAction::CreateRaw(this, &FNiagaraScriptGraphViewModel::CanCopyNodes));

	Commands->MapAction(
		FGenericCommands::Get().Cut,
		FExecuteAction::CreateRaw(this, &FNiagaraScriptGraphViewModel::CutSelectedNodes),
		FCanExecuteAction::CreateRaw(this, &FNiagaraScriptGraphViewModel::CanCutNodes));

	Commands->MapAction(
		FGenericCommands::Get().Paste,
		FExecuteAction::CreateRaw(this, &FNiagaraScriptGraphViewModel::PasteNodes),
		FCanExecuteAction::CreateRaw(this, &FNiagaraScriptGraphViewModel::CanPasteNodes));

	Commands->MapAction(
		FGenericCommands::Get().Duplicate,
		FExecuteAction::CreateRaw(this, &FNiagaraScriptGraphViewModel::DuplicateNodes),
		FCanExecuteAction::CreateRaw(this, &FNiagaraScriptGraphViewModel::CanDuplicateNodes));
}

void FNiagaraScriptGraphViewModel::SelectAllNodes()
{
	UNiagaraGraph* Graph = GetGraph();
	if (Graph != nullptr)
	{
		TArray<UObject*> AllNodes;
		Graph->GetNodesOfClass<UObject>(AllNodes);
		TSet<UObject*> AllNodeSet;
		AllNodeSet.Append(AllNodes);
		Selection->SetSelectedObjects(AllNodeSet);
	}
}

void FNiagaraScriptGraphViewModel::DeleteSelectedNodes()
{
	UNiagaraGraph* Graph = GetGraph();
	if (Graph != nullptr)
	{
		const FScopedTransaction Transaction(FGenericCommands::Get().Delete->GetDescription());
		Graph->Modify();

		TArray<UObject*> NodesToDelete = Selection->GetSelectedObjects().Array();
		Selection->ClearSelectedObjects();

		for (UObject* NodeToDelete : NodesToDelete)
		{
			UEdGraphNode* GraphNodeToDelete = Cast<UEdGraphNode>(NodeToDelete);
			if (GraphNodeToDelete != nullptr && GraphNodeToDelete->CanUserDeleteNode())
			{
				GraphNodeToDelete->Modify();
				GraphNodeToDelete->DestroyNode();
			}
		}
	}
}

bool FNiagaraScriptGraphViewModel::CanDeleteNodes() const
{
	UNiagaraGraph* Graph = GetGraph();
	if (Graph != nullptr)
	{
		for (UObject* SelectedNode : Selection->GetSelectedObjects())
		{
			UEdGraphNode* SelectedGraphNode = Cast<UEdGraphNode>(SelectedNode);
			if (SelectedGraphNode != nullptr && SelectedGraphNode->CanUserDeleteNode())
			{
				return true;
			}
		}
	}
	return false;
}

void FNiagaraScriptGraphViewModel::CutSelectedNodes()
{
	// Collect nodes which can not be delete or duplicated so they can be reselected.
	TSet<UObject*> CanBeDuplicatedAndDeleted;
	TSet<UObject*> CanNotBeDuplicatedAndDeleted;
	for (UObject* SelectedNode : Selection->GetSelectedObjects())
	{
		UEdGraphNode* SelectedGraphNode = Cast<UEdGraphNode>(SelectedNode);
		if (SelectedGraphNode != nullptr)
		{
			if (SelectedGraphNode->CanDuplicateNode() && SelectedGraphNode->CanUserDeleteNode())
			{
				CanBeDuplicatedAndDeleted.Add(SelectedNode);
			}
			else
			{
				CanNotBeDuplicatedAndDeleted.Add(SelectedNode);
			}
		}
	}

	// Select the nodes which can be copied and deleted, copy and delete them, and then restore the ones which couldn't be copied or deleted.
	Selection->SetSelectedObjects(CanBeDuplicatedAndDeleted);
	CopySelectedNodes();
	DeleteSelectedNodes();
	Selection->SetSelectedObjects(CanNotBeDuplicatedAndDeleted);
}

bool FNiagaraScriptGraphViewModel::CanCutNodes() const
{
	return CanCopyNodes() && CanDeleteNodes();
}

void FNiagaraScriptGraphViewModel::CopySelectedNodes()
{
	TSet<UObject*> NodesToCopy;
	for (UObject* SelectedNode : Selection->GetSelectedObjects())
	{
		UEdGraphNode* SelectedGraphNode = Cast<UEdGraphNode>(SelectedNode);
		if (SelectedGraphNode != nullptr)
		{
			if (SelectedGraphNode->CanDuplicateNode())
			{
				SelectedGraphNode->PrepareForCopying();
				NodesToCopy.Add(SelectedNode);
			}
		}
	}

	FString ExportedText;
	FEdGraphUtilities::ExportNodesToText(NodesToCopy, ExportedText);
	FPlatformApplicationMisc::ClipboardCopy(*ExportedText);
}

bool FNiagaraScriptGraphViewModel::CanCopyNodes() const
{
	UNiagaraGraph* Graph = GetGraph();
	if (Graph != nullptr)
	{
		for (UObject* SelectedNode : Selection->GetSelectedObjects())
		{
			UEdGraphNode* SelectedGraphNode = Cast<UEdGraphNode>(SelectedNode);
			if (SelectedGraphNode != nullptr && SelectedGraphNode->CanDuplicateNode())
			{
				return true;
			}
		}
	}
	return false;
}


void FNiagaraScriptGraphViewModel::PasteNodes()
{
	UEdGraph* Graph = GetGraph();
	if (Graph != nullptr)
	{
		const FScopedTransaction Transaction(FGenericCommands::Get().Paste->GetDescription());;
		Graph->Modify();

		Selection->ClearSelectedObjects();

		// Grab the text to paste from the clipboard.
		FString TextToImport;
		FPlatformApplicationMisc::ClipboardPaste(TextToImport);

		// Import the nodes
		TSet<UEdGraphNode*> PastedNodes;
		FEdGraphUtilities::ImportNodesFromText(Graph, TextToImport, PastedNodes);

		for (UEdGraphNode* PastedNode : PastedNodes)
		{
			PastedNode->CreateNewGuid();
		}

		FNiagaraEditorUtilities::FixUpPastedInputNodes(Graph, PastedNodes);

		OnNodesPastedDelegate.Broadcast(PastedNodes);

		TSet<UObject*> PastedObjects;
		for (UEdGraphNode* PastedNode : PastedNodes)
		{
			PastedObjects.Add(PastedNode);
		}

		Selection->SetSelectedObjects(PastedObjects);
		CastChecked<UNiagaraGraph>(Graph)->NotifyGraphNeedsRecompile();
	}
}

bool FNiagaraScriptGraphViewModel::CanPasteNodes() const
{
	UNiagaraGraph* Graph = GetGraph();
	if (Graph == nullptr)
	{
		return false;
	}

	FString ClipboardContent;
	FPlatformApplicationMisc::ClipboardPaste(ClipboardContent);

	return FEdGraphUtilities::CanImportNodesFromText(Graph, ClipboardContent);
}

void FNiagaraScriptGraphViewModel::DuplicateNodes()
{
	CopySelectedNodes();
	PasteNodes();
}

bool FNiagaraScriptGraphViewModel::CanDuplicateNodes() const
{
	return CanCopyNodes();
}


#undef LOCTEXT_NAMESPACE // NiagaraScriptGraphViewModel