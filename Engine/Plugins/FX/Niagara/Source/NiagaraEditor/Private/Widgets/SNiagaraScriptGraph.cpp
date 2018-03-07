// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SNiagaraScriptGraph.h"
#include "NiagaraScriptGraphViewModel.h"
#include "NiagaraEditorUtilities.h"
#include "NiagaraGraph.h"
#include "NiagaraNode.h"
#include "NiagaraNodeInput.h"
#include "AssetEditorManager.h"
#include "GraphEditor.h"
#include "EditorStyleSet.h"
#include "SBorder.h"
#include "SBoxPanel.h"
#include "TextLayout.h"
#include "SErrorText.h"
#include "STextBlock.h"
#include "NiagaraEditorSettings.h"
#include "EdGraphSchema_Niagara.h"
#include "ScopedTransaction.h"
#include "NiagaraEditorUtilities.h"

#define LOCTEXT_NAMESPACE "NiagaraScriptGraph"

void SNiagaraScriptGraph::Construct(const FArguments& InArgs, TSharedRef<FNiagaraScriptGraphViewModel> InViewModel)
{
	ViewModel = InViewModel;
	ViewModel->GetSelection()->OnSelectedObjectsChanged().AddSP(this, &SNiagaraScriptGraph::ViewModelSelectedNodesChanged);
	ViewModel->OnNodesPasted().AddSP(this, &SNiagaraScriptGraph::NodesPasted);
	ViewModel->OnGraphChanged().AddSP(this, &SNiagaraScriptGraph::GraphChanged);
	bUpdatingGraphSelectionFromViewModel = false;

	GraphTitle = InArgs._GraphTitle;

	GraphEditor = ConstructGraphEditor();

	ChildSlot
	[
		GraphEditor.ToSharedRef()
	];
}

TSharedRef<SGraphEditor> SNiagaraScriptGraph::ConstructGraphEditor()
{
	FGraphAppearanceInfo AppearanceInfo;
	AppearanceInfo.CornerText = LOCTEXT("AppearanceCornerText", "NIAGARA");

	TSharedRef<SWidget> TitleBarWidget =
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush(TEXT("Graph.TitleBackground")))
		.HAlign(HAlign_Fill)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0.0f, 0.0f, 3.0f, 0.0f)
			[
				SNew(SErrorText)
				.Visibility(ViewModel.ToSharedRef(), &FNiagaraScriptGraphViewModel::GetGraphErrorTextVisible)
				.BackgroundColor(ViewModel.ToSharedRef(), &FNiagaraScriptGraphViewModel::GetGraphErrorColor)
				.ToolTipText(ViewModel.ToSharedRef(), &FNiagaraScriptGraphViewModel::GetGraphErrorMsgToolTip)
				.ErrorText(ViewModel->GetGraphErrorText())
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(ViewModel.ToSharedRef(), &FNiagaraScriptGraphViewModel::GetDisplayName)
				.TextStyle(FEditorStyle::Get(), TEXT("GraphBreadcrumbButtonText"))
				.Justification(ETextJustify::Center)
			]
		];

	SGraphEditor::FGraphEditorEvents Events;
	Events.OnSelectionChanged = SGraphEditor::FOnSelectionChanged::CreateSP(this, &SNiagaraScriptGraph::GraphEditorSelectedNodesChanged);
	Events.OnNodeDoubleClicked = FSingleNodeEvent::CreateSP(this, &SNiagaraScriptGraph::OnNodeDoubleClicked);
	Events.OnTextCommitted = FOnNodeTextCommitted::CreateSP(this, &SNiagaraScriptGraph::OnNodeTitleCommitted);
	Events.OnVerifyTextCommit = FOnNodeVerifyTextCommit::CreateSP(this, &SNiagaraScriptGraph::OnVerifyNodeTextCommit);
	Events.OnSpawnNodeByShortcut = SGraphEditor::FOnSpawnNodeByShortcut::CreateSP(this, &SNiagaraScriptGraph::OnSpawnGraphNodeByShortcut);

	return SNew(SGraphEditor)
		.AdditionalCommands(ViewModel->GetCommands())
		.Appearance(AppearanceInfo)
		.TitleBar(TitleBarWidget)
		.GraphToEdit(ViewModel->GetGraph())
		.GraphEvents(Events);
}

void SNiagaraScriptGraph::ViewModelSelectedNodesChanged()
{
	if (FNiagaraEditorUtilities::SetsMatch(GraphEditor->GetSelectedNodes(), ViewModel->GetSelection()->GetSelectedObjects()) == false)
	{
		bUpdatingGraphSelectionFromViewModel = true;
		GraphEditor->ClearSelectionSet();
		for (UObject* SelectedNode : ViewModel->GetSelection()->GetSelectedObjects())
		{
			UEdGraphNode* GraphNode = Cast<UEdGraphNode>(SelectedNode);
			if (GraphNode != nullptr)
			{
				GraphEditor->SetNodeSelection(GraphNode, true);
			}
		}
		bUpdatingGraphSelectionFromViewModel = false;
	}
}

void SNiagaraScriptGraph::GraphEditorSelectedNodesChanged(const TSet<UObject*>& SelectedNodes)
{
	if (bUpdatingGraphSelectionFromViewModel == false)
	{
		ViewModel->GetSelection()->SetSelectedObjects(SelectedNodes);
	}
}

void SNiagaraScriptGraph::OnNodeDoubleClicked(UEdGraphNode* ClickedNode)
{
	UNiagaraNode* NiagaraNode = Cast<UNiagaraNode>(ClickedNode);
	if (NiagaraNode != nullptr)
	{
		UObject* ReferencedAsset = NiagaraNode->GetReferencedAsset();
		if (ReferencedAsset != nullptr)
		{
			FAssetEditorManager::Get().OpenEditorForAsset(ReferencedAsset);
		}
	}
}

void SNiagaraScriptGraph::OnNodeTitleCommitted(const FText& NewText, ETextCommit::Type CommitInfo, UEdGraphNode* NodeBeingChanged)
{
	if (NodeBeingChanged)
	{
		// When you request rename on spawn but accept the value, we want to not add a transaction if they just hit "Enter".
		bool bRename = true;
		if (NodeBeingChanged->IsA(UNiagaraNodeInput::StaticClass()))
		{
			FName CurrentName = Cast<UNiagaraNodeInput>(NodeBeingChanged)->Input.GetName();
			if (CurrentName.ToString().Equals(NewText.ToString(), ESearchCase::CaseSensitive))
			{
				bRename = false;
			}
		}

		if (bRename)
		{
			const FScopedTransaction Transaction(LOCTEXT("RenameNode", "Rename Node"));
			NodeBeingChanged->Modify();
			NodeBeingChanged->OnRenameNode(NewText.ToString());
		}
	}
}

bool SNiagaraScriptGraph::OnVerifyNodeTextCommit(const FText& NewText, UEdGraphNode* NodeBeingChanged, FText& OutErrorMessage)
{
	bool bValid = true;
	if (NodeBeingChanged->IsA(UNiagaraNodeInput::StaticClass()))
	{
		return UNiagaraNodeInput::VerifyNodeRenameTextCommit(NewText, Cast<UNiagaraNodeInput>(NodeBeingChanged), OutErrorMessage);
	}
	return bValid;
}

FReply SNiagaraScriptGraph::OnSpawnGraphNodeByShortcut(FInputChord InChord, const FVector2D& InPosition)
{
	if (ViewModel.Get() == nullptr)
	{
		return FReply::Unhandled();
	}
	UNiagaraGraph* Graph = ViewModel->GetGraph();
	if (Graph == nullptr)
	{
		return FReply::Unhandled();
	}

	const UNiagaraEditorSettings* Settings = GetDefault<UNiagaraEditorSettings>();
	if (Settings == nullptr)
	{
		return FReply::Unhandled();
	}

	for (int32 i = 0; i < Settings->GraphCreationShortcuts.Num(); i++)
	{
		if (Settings->GraphCreationShortcuts[i].Input.GetRelationship(InChord) == FInputChord::Same)
		{
			const UEdGraphSchema_Niagara* Schema = GetDefault<UEdGraphSchema_Niagara>();

			UEdGraph* OwnerOfTemporaries = NewObject<UEdGraph>((UObject*)GetTransientPackage());
			TArray<UObject*> SelectedObjects;
			TArray<TSharedPtr<FNiagaraSchemaAction_NewNode> > Actions = Schema->GetGraphContextActions(Graph, SelectedObjects, nullptr, OwnerOfTemporaries);

			for (int32 ActionIdx = 0; ActionIdx < Actions.Num(); ActionIdx++)
			{
				TSharedPtr<FNiagaraSchemaAction_NewNode> NiagaraAction = Actions[ActionIdx];
				if (!NiagaraAction.IsValid())
				{
					continue;
				}

				bool bMatch = false;
				if (NiagaraAction->InternalName.ToString().Equals(Settings->GraphCreationShortcuts[i].Name, ESearchCase::IgnoreCase))
				{
					bMatch = true;
				}
				if (!bMatch && NiagaraAction->GetMenuDescription().ToString().Equals(Settings->GraphCreationShortcuts[i].Name, ESearchCase::IgnoreCase))
				{
					bMatch = true;
				}
				if (bMatch)
				{
					FScopedTransaction Transaction(LOCTEXT("AddNode", "Add Node"));
					TArray<UEdGraphPin*> Pins;
					NiagaraAction->PerformAction(Graph, Pins, InPosition);
					return FReply::Handled();
				}					
			}
		}
	}

	return FReply::Unhandled();

	/*
	if (Graph == nullptr)
	{
	return FReply::Handled();
	}

	FScopedTransaction Transaction(LOCTEXT("AddNode", "Add Node"));

	TArray<UEdGraphNode*> OutNodes;
	FVector2D NodeSpawnPos = InPosition;
	FBlueprintSpawnNodeCommands::Get().GetGraphActionByChord(InChord, InGraph, NodeSpawnPos, OutNodes);

	TSet<const UEdGraphNode*> NodesToSelect;

	for (UEdGraphNode* CurrentNode : OutNodes)
	{
	NodesToSelect.Add(CurrentNode);
	}

	// Do not change node selection if no actions were performed
	if(OutNodes.Num() > 0)
	{
	Graph->SelectNodeSet(NodesToSelect, true);
}
	else
	{
		Transaction.Cancel();
	}

	return FReply::Handled();
	*/
}

void SNiagaraScriptGraph::NodesPasted(const TSet<UEdGraphNode*>& PastedNodes)
{
	if (PastedNodes.Num() != 0)
	{
		PositionPastedNodes(PastedNodes);
		GraphEditor->NotifyGraphChanged();
	}
}

void SNiagaraScriptGraph::PositionPastedNodes(const TSet<UEdGraphNode*>& PastedNodes)
{
	FVector2D AvgNodePosition(0.0f, 0.0f);

	for (UEdGraphNode* PastedNode : PastedNodes)
	{
		AvgNodePosition.X += PastedNode->NodePosX;
		AvgNodePosition.Y += PastedNode->NodePosY;
	}

	float InvNumNodes = 1.0f / float(PastedNodes.Num());
	AvgNodePosition.X *= InvNumNodes;
	AvgNodePosition.Y *= InvNumNodes;

	FVector2D PasteLocation = GraphEditor->GetPasteLocation();
	for (UEdGraphNode* PastedNode : PastedNodes)
	{
		PastedNode->NodePosX = (PastedNode->NodePosX - AvgNodePosition.X) + PasteLocation.X;
		PastedNode->NodePosY = (PastedNode->NodePosY - AvgNodePosition.Y) + PasteLocation.Y;

		PastedNode->SnapToGrid(16);
	}
}

void SNiagaraScriptGraph::GraphChanged()
{
	TSharedPtr<SGraphEditor> NewGraphEditor;
	TSharedPtr<SWidget> NewChildWidget;

	if (ViewModel->GetGraph() != nullptr)
	{
		NewGraphEditor = ConstructGraphEditor();
		NewChildWidget = NewGraphEditor;
	}
	else
	{
		NewGraphEditor = nullptr;
		NewChildWidget = SNullWidget::NullWidget;
	}
	
	GraphEditor = NewGraphEditor;
	ChildSlot
	[
		NewChildWidget.ToSharedRef()
	];
}

#undef LOCTEXT_NAMESPACE // "NiagaraScriptGraph"