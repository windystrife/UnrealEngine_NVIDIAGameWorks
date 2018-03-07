// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "FindInBT.h"
#include "EdGraph/EdGraph.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "EditorStyleSet.h"
#include "BehaviorTreeGraphNode_Decorator.h"
#include "BehaviorTreeGraphNode_Service.h"
#include "Widgets/Input/SSearchBox.h"

#define LOCTEXT_NAMESPACE "FindInBT"

//////////////////////////////////////////////////////////////////////////
// FFindInBTResult

FFindInBTResult::FFindInBTResult(const FString& InValue) 
	: Value(InValue), GraphNode(NULL)
{
}

FFindInBTResult::FFindInBTResult(const FString& InValue, TSharedPtr<FFindInBTResult>& InParent, UEdGraphNode* InNode)
	: Value(InValue), GraphNode(InNode), Parent(InParent)
{
}

void FFindInBTResult::SetNodeHighlight(bool bHighlight)
{
	if (GraphNode.IsValid())
	{
		UBehaviorTreeGraphNode* BTNode = Cast<UBehaviorTreeGraphNode>(GraphNode.Get());
		if (BTNode)
		{
			BTNode->bHighlightInSearchTree = bHighlight;
		}
	}
}

TSharedRef<SWidget> FFindInBTResult::CreateIcon() const
{
	FSlateColor IconColor = FSlateColor::UseForeground();
	const FSlateBrush* Brush = NULL;

	if (GraphNode.IsValid())
	{
		if (Cast<UBehaviorTreeGraphNode_Service>(GraphNode.Get()))
		{
			Brush = FEditorStyle::GetBrush(TEXT("GraphEditor.PinIcon"));
		}
		else if (Cast<UBehaviorTreeGraphNode_Decorator>(GraphNode.Get()))
		{
			Brush = FEditorStyle::GetBrush(TEXT("GraphEditor.RefPinIcon"));
		}
		else
		{
			Brush = FEditorStyle::GetBrush(TEXT("GraphEditor.FIB_Event"));
		}
	}
	
	return SNew(SImage)
		.Image(Brush)
		.ColorAndOpacity(IconColor);
}

FReply FFindInBTResult::OnClick(TWeakPtr<class FBehaviorTreeEditor> BehaviorTreeEditorPtr, TSharedPtr<FFindInBTResult> Root)
{
	if (BehaviorTreeEditorPtr.IsValid() && GraphNode.IsValid())
	{
		if (Parent.IsValid() && Parent.HasSameObject(Root.Get()))
		{
			BehaviorTreeEditorPtr.Pin()->JumpToNode(GraphNode.Get());
		}
		else
		{
			BehaviorTreeEditorPtr.Pin()->JumpToNode(Parent.Pin()->GraphNode.Get());
		}
	}

	return FReply::Handled();
}

FString FFindInBTResult::GetNodeTypeText() const
{
	if (GraphNode.IsValid())
	{
		FString NodeClassName = GraphNode->GetClass()->GetName();
		int32 Pos = NodeClassName.Find("_");
		if (Pos == INDEX_NONE)
		{
			return NodeClassName;
		}
		else
		{
			return NodeClassName.RightChop(Pos + 1);
		}
	}

	return FString();
}

FString FFindInBTResult::GetCommentText() const
{
	if (GraphNode.IsValid())
	{
		return GraphNode->NodeComment;
	}

	return FString();
}

//////////////////////////////////////////////////////////////////////////
// SFindInBT

void SFindInBT::Construct( const FArguments& InArgs, TSharedPtr<FBehaviorTreeEditor> InBehaviorTreeEditor)
{
	BehaviorTreeEditorPtr = InBehaviorTreeEditor;

	this->ChildSlot
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.FillWidth(1)
				[
					SAssignNew(SearchTextField, SSearchBox)
					.HintText(LOCTEXT("BehaviorTreeSearchHint", "Enter text to find nodes..."))
					.OnTextChanged(this, &SFindInBT::OnSearchTextChanged)
					.OnTextCommitted(this, &SFindInBT::OnSearchTextCommitted)
				]
			]
			+SVerticalBox::Slot()
			.FillHeight(1.0f)
			.Padding(0.f, 4.f, 0.f, 0.f)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
				[
					SAssignNew(TreeView, STreeViewType)
					.ItemHeight(24)
					.TreeItemsSource(&ItemsFound)
					.OnGenerateRow(this, &SFindInBT::OnGenerateRow)
					.OnGetChildren(this, &SFindInBT::OnGetChildren)
					.OnSelectionChanged(this, &SFindInBT::OnTreeSelectionChanged)
					.SelectionMode(ESelectionMode::Multi)
				]
			]
		];
}

void SFindInBT::FocusForUse()
{
	// NOTE: Careful, GeneratePathToWidget can be reentrant in that it can call visibility delegates and such
	FWidgetPath FilterTextBoxWidgetPath;
	FSlateApplication::Get().GeneratePathToWidgetUnchecked(SearchTextField.ToSharedRef(), FilterTextBoxWidgetPath);

	// Set keyboard focus directly
	FSlateApplication::Get().SetKeyboardFocus(FilterTextBoxWidgetPath, EFocusCause::SetDirectly);
}

void SFindInBT::OnSearchTextChanged(const FText& Text)
{
	SearchValue = Text.ToString();
	
	InitiateSearch();
}

void SFindInBT::OnSearchTextCommitted(const FText& Text, ETextCommit::Type CommitType)
{
	OnSearchTextChanged(Text);
}

void SFindInBT::InitiateSearch()
{
	TArray<FString> Tokens;
	SearchValue.ParseIntoArray(Tokens, TEXT(" "), true);

	for (auto It(ItemsFound.CreateIterator()); It; ++It)
	{
		(*It).Get()->SetNodeHighlight(false); // need to reset highlight
		TreeView->SetItemExpansion(*It, false);
	}
	ItemsFound.Empty();
	if (Tokens.Num() > 0)
	{
		HighlightText = FText::FromString(SearchValue);
		MatchTokens(Tokens);
	}

	// Insert a fake result to inform user if none found
	if (ItemsFound.Num() == 0)
	{
		ItemsFound.Add(FSearchResult(new FFindInBTResult(LOCTEXT("BehaviorTreeSearchNoResults", "No Results found").ToString())));
	}

	TreeView->RequestTreeRefresh();

	for (auto It(ItemsFound.CreateIterator()); It; ++It)
	{
		TreeView->SetItemExpansion(*It, true);
	}
}

void SFindInBT::MatchTokens(const TArray<FString>& Tokens)
{
	RootSearchResult.Reset();

	TWeakPtr<SGraphEditor> FocusedGraphEditor = BehaviorTreeEditorPtr.Pin()->GetFocusedGraphPtr();
	UEdGraph* Graph = NULL;
	if (FocusedGraphEditor.IsValid())
	{
		Graph = FocusedGraphEditor.Pin()->GetCurrentGraph();
	}

	if (Graph == NULL)
	{
		return;
	}

	RootSearchResult = FSearchResult(new FFindInBTResult(FString("BehaviorTreeRoot")));

	for (auto It(Graph->Nodes.CreateConstIterator()); It; ++It)
	{
		UEdGraphNode* Node = *It;
			
		const FString NodeName = Node->GetNodeTitle(ENodeTitleType::ListView).ToString();
		FSearchResult NodeResult(new FFindInBTResult(NodeName, RootSearchResult, Node));

		FString NodeSearchString = NodeName + Node->GetClass()->GetName() + Node->NodeComment;
		NodeSearchString = NodeSearchString.Replace(TEXT(" "), TEXT(""));

		bool bNodeMatchesSearch = StringMatchesSearchTokens(Tokens, NodeSearchString);

		UBehaviorTreeGraphNode* BTNode = Cast<UBehaviorTreeGraphNode>(Node);
		if (BTNode)
		{
			// searching through nodes' decorators
			for (auto DecoratorIt(BTNode->Decorators.CreateConstIterator()); DecoratorIt; ++DecoratorIt)
			{
				UBehaviorTreeGraphNode* Decorator = *DecoratorIt;
				MatchTokensInChild(Tokens, Decorator, NodeResult);
			}

			// searching through nodes' services
			for (auto ServiceIt(BTNode->Services.CreateConstIterator()); ServiceIt; ++ServiceIt)
			{
				UBehaviorTreeGraphNode* Service = *ServiceIt;
				MatchTokensInChild(Tokens, Service, NodeResult);
			}
		}

		if ((NodeResult->Children.Num() > 0) || bNodeMatchesSearch)
		{
			NodeResult->SetNodeHighlight(true);
			ItemsFound.Add(NodeResult);
		}
	}
}

void SFindInBT::MatchTokensInChild(const TArray<FString>& Tokens, UBehaviorTreeGraphNode* Child, FSearchResult ParentNode)
{
	if (Child == NULL)
	{
		return;
	}

	FString ChildName = Child->GetNodeTitle(ENodeTitleType::ListView).ToString();
	FString ChildSearchString = ChildName + Child->GetClass()->GetName() + Child->NodeComment;
	ChildSearchString = ChildSearchString.Replace(TEXT(" "), TEXT(""));
	if (StringMatchesSearchTokens(Tokens, ChildSearchString))
	{
		FSearchResult DecoratorResult(new FFindInBTResult(ChildName, ParentNode, Child));
		ParentNode->Children.Add(DecoratorResult);
	}
}

TSharedRef<ITableRow> SFindInBT::OnGenerateRow( FSearchResult InItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	return SNew(STableRow< TSharedPtr<FFindInBTResult> >, OwnerTable)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SBox)
				.WidthOverride(450.0f)
				[
					SNew(SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					[
						InItem->CreateIcon()
					]
					+SHorizontalBox::Slot()
					.VAlign(VAlign_Center)
					.AutoWidth()
					.Padding(2, 0)
					[
						SNew(STextBlock)
						.Text(FText::FromString(InItem->Value))
						.HighlightText(HighlightText)
					]
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(InItem->GetNodeTypeText()))
				.HighlightText(HighlightText)
			]
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Right)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(FText::FromString(InItem->GetCommentText()))
				.ColorAndOpacity(FLinearColor::Yellow)
				.HighlightText(HighlightText)
			]
		];
}

void SFindInBT::OnGetChildren(FSearchResult InItem, TArray< FSearchResult >& OutChildren)
{
	OutChildren += InItem->Children;
}

void SFindInBT::OnTreeSelectionChanged(FSearchResult Item , ESelectInfo::Type)
{
	if (Item.IsValid())
	{
		Item->OnClick(BehaviorTreeEditorPtr, RootSearchResult);
	}
}

bool SFindInBT::StringMatchesSearchTokens(const TArray<FString>& Tokens, const FString& ComparisonString)
{
	bool bFoundAllTokens = true;

	//search the entry for each token, it must have all of them to pass
	for (auto TokIT(Tokens.CreateConstIterator()); TokIT; ++TokIT)
	{
		const FString& Token = *TokIT;
		if (!ComparisonString.Contains(Token))
		{
			bFoundAllTokens = false;
			break;
		}
	}
	return bFoundAllTokens;
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
