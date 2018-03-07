// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Views/STableViewBase.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/Views/STreeView.h"
#include "BehaviorTreeEditor.h"

/** Item that matched the search results */
class FFindInBTResult
{
public: 
	/** Create a root (or only text) result */
	FFindInBTResult(const FString& InValue);
	
	/** Create a BT node result */
	FFindInBTResult(const FString& InValue, TSharedPtr<FFindInBTResult>& InParent, UEdGraphNode* InNode);

	/** Called when user clicks on the search item */
	FReply OnClick(TWeakPtr<class FBehaviorTreeEditor> BehaviorTreeEditor,  TSharedPtr<FFindInBTResult> Root);

	/** Create an icon to represent the result */
	TSharedRef<SWidget>	CreateIcon() const;

	/** Gets the comment on this node if any */
	FString GetCommentText() const;

	/** Gets the node type */
	FString GetNodeTypeText() const;

	/** Highlights BT tree nodes */
	void SetNodeHighlight(bool bHighlight);

	/** Any children listed under this BT node (decorators and services) */
	TArray< TSharedPtr<FFindInBTResult> > Children;

	/** The string value for this result */
	FString Value;

	/** The graph node that this search result refers to */
	TWeakObjectPtr<UEdGraphNode> GraphNode;

	/** Search result parent */
	TWeakPtr<FFindInBTResult> Parent;
};

/** Widget for searching for (BT nodes) across focused BehaviorTree */
class SFindInBT : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SFindInBT){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedPtr<class FBehaviorTreeEditor> InBehaviorTreeEditor);

	/** Focuses this widget's search box */
	void FocusForUse();

private:
	typedef TSharedPtr<FFindInBTResult> FSearchResult;
	typedef STreeView<FSearchResult> STreeViewType;

	/** Called when user changes the text they are searching for */
	void OnSearchTextChanged(const FText& Text);

	/** Called when user commits text */
	void OnSearchTextCommitted(const FText& Text, ETextCommit::Type CommitType);

	/** Get the children of a row */
	void OnGetChildren(FSearchResult InItem, TArray<FSearchResult>& OutChildren);

	/** Called when user clicks on a new result */
	void OnTreeSelectionChanged(FSearchResult Item, ESelectInfo::Type SelectInfo);

	/** Called when a new row is being generated */
	TSharedRef<ITableRow> OnGenerateRow(FSearchResult InItem, const TSharedRef<STableViewBase>& OwnerTable);

	/** Begins the search based on the SearchValue */
	void InitiateSearch();
	
	/** Find any results that contain all of the tokens */
	void MatchTokens(const TArray<FString>& Tokens);

	/** Find if child contains all of the tokens and add a result accordingly */
	void MatchTokensInChild(const TArray<FString>& Tokens, UBehaviorTreeGraphNode* Child, FSearchResult ParentNode);
	
	/** Determines if a string matches the search tokens */
	static bool StringMatchesSearchTokens(const TArray<FString>& Tokens, const FString& ComparisonString);

private:
	/** Pointer back to the behavior tree editor that owns us */
	TWeakPtr<class FBehaviorTreeEditor> BehaviorTreeEditorPtr;
	
	/** The tree view displays the results */
	TSharedPtr<STreeViewType> TreeView;

	/** The search text box */
	TSharedPtr<class SSearchBox> SearchTextField;
	
	/** This buffer stores the currently displayed results */
	TArray<FSearchResult> ItemsFound;

	/** we need to keep a handle on the root result, because it won't show up in the tree */
	FSearchResult RootSearchResult;

	/** The string to highlight in the results */
	FText HighlightText;

	/** The string to search for */
	FString	SearchValue;
};
