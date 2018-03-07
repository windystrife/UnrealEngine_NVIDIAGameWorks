// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "SGraphActionMenu.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Layout/SScrollBorder.h"
#include "EditorStyleSet.h"
#include "GraphEditorDragDropAction.h"
#include "EdGraphSchema_K2.h"
#include "K2Node.h"
#include "EdGraphSchema_K2_Actions.h"
#include "GraphActionNode.h"
#include "Widgets/SToolTip.h"
#include "IDocumentation.h"
#include "EditorCategoryUtils.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"

#define LOCTEXT_NAMESPACE "GraphActionMenu"

//////////////////////////////////////////////////////////////////////////

template<typename ItemType>
class SCategoryHeaderTableRow : public STableRow < ItemType >
{
public:
	SLATE_BEGIN_ARGS(SCategoryHeaderTableRow)
	{}
		SLATE_DEFAULT_SLOT(typename SCategoryHeaderTableRow::FArguments, Content)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTableView)
	{
		STableRow<ItemType>::ChildSlot
		.Padding(0.0f, 2.0f, 0.0f, 0.0f)
		[
			SAssignNew(ContentBorder, SBorder)
			.BorderImage(this, &SCategoryHeaderTableRow::GetBackgroundImage)
			.Padding(FMargin(0.0f, 3.0f))
			.BorderBackgroundColor(FLinearColor(.6, .6, .6, 1.0f))
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				.Padding(2.0f, 2.0f, 2.0f, 2.0f)
				.AutoWidth()
				[
					SNew(SExpanderArrow, STableRow< ItemType >::SharedThis(this))
				]

				+ SHorizontalBox::Slot()
				.VAlign(VAlign_Center)
				[
					InArgs._Content.Widget
				]
			]
		];

		STableRow < ItemType >::ConstructInternal(
			typename STableRow< ItemType >::FArguments()
			.Style(FEditorStyle::Get(), "DetailsView.TreeView.TableRow")
			.ShowSelection(false),
			InOwnerTableView
			);
	}

	const FSlateBrush* GetBackgroundImage() const
	{
		if ( STableRow<ItemType>::IsHovered() )
		{
			return STableRow<ItemType>::IsItemExpanded() ? FEditorStyle::GetBrush("DetailsView.CategoryTop_Hovered") : FEditorStyle::GetBrush("DetailsView.CollapsedCategory_Hovered");
		}
		else
		{
			return STableRow<ItemType>::IsItemExpanded() ? FEditorStyle::GetBrush("DetailsView.CategoryTop") : FEditorStyle::GetBrush("DetailsView.CollapsedCategory");
		}
	}

	virtual void SetContent(TSharedRef< SWidget > InContent) override
	{
		ContentBorder->SetContent(InContent);
	}

	virtual void SetRowContent(TSharedRef< SWidget > InContent) override
	{
		ContentBorder->SetContent(InContent);
	}

private:
	TSharedPtr<SBorder> ContentBorder;
};

//////////////////////////////////////////////////////////////////////////

namespace GraphActionMenuHelpers
{
	bool ActionMatchesName(const FEdGraphSchemaAction* InGraphAction, const FName& ItemName)
	{
		bool bCheck = false;

		bCheck |= (InGraphAction->GetTypeId() == FEdGraphSchemaAction_K2Var::StaticGetTypeId() &&
			((FEdGraphSchemaAction_K2Var*)InGraphAction)->GetVariableName() == ItemName);
		bCheck |= (InGraphAction->GetTypeId() == FEdGraphSchemaAction_K2LocalVar::StaticGetTypeId() &&
			((FEdGraphSchemaAction_K2LocalVar*)InGraphAction)->GetVariableName() == ItemName);
		bCheck |= (InGraphAction->GetTypeId() == FEdGraphSchemaAction_K2Graph::StaticGetTypeId() &&
			((FEdGraphSchemaAction_K2Graph*)InGraphAction)->EdGraph &&
			((FEdGraphSchemaAction_K2Graph*)InGraphAction)->EdGraph->GetFName() == ItemName);
		bCheck |= (InGraphAction->GetTypeId() == FEdGraphSchemaAction_K2Enum::StaticGetTypeId() &&
			((FEdGraphSchemaAction_K2Enum*)InGraphAction)->GetPathName() == ItemName);
		bCheck |= (InGraphAction->GetTypeId() == FEdGraphSchemaAction_K2Struct::StaticGetTypeId() &&
			((FEdGraphSchemaAction_K2Struct*)InGraphAction)->GetPathName() == ItemName);
		bCheck |= (InGraphAction->GetTypeId() == FEdGraphSchemaAction_K2Delegate::StaticGetTypeId() &&
			((FEdGraphSchemaAction_K2Delegate*)InGraphAction)->GetDelegateName() == ItemName);

		const bool bIsTargetNodeSubclass = (InGraphAction->GetTypeId() == FEdGraphSchemaAction_K2TargetNode::StaticGetTypeId()) ||
			(InGraphAction->GetTypeId() == FEdGraphSchemaAction_K2Event::StaticGetTypeId()) ||
			(InGraphAction->GetTypeId() == FEdGraphSchemaAction_K2InputAction::StaticGetTypeId());
		bCheck |= (bIsTargetNodeSubclass &&
			((FEdGraphSchemaAction_K2TargetNode*)InGraphAction)->NodeTemplate->GetNodeTitle(ENodeTitleType::EditableTitle).ToString() == ItemName.ToString());

		return bCheck;
	}
}

//////////////////////////////////////////////////////////////////////////

void SDefaultGraphActionWidget::Construct(const FArguments& InArgs, const FCreateWidgetForActionData* InCreateData)
{
	ActionPtr = InCreateData->Action;
	MouseButtonDownDelegate = InCreateData->MouseButtonDownDelegate;

	this->ChildSlot
	[
		SNew(SHorizontalBox)
		.ToolTipText(InCreateData->Action->GetTooltipDescription())
		+ SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(STextBlock)
			.Font(FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 9 ))
			.Text(InCreateData->Action->GetMenuDescription())
			.HighlightText(InArgs._HighlightText)
		]
	];
}

FReply SDefaultGraphActionWidget::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if( MouseButtonDownDelegate.Execute( ActionPtr ) )
	{
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

//////////////////////////////////////////////////////////////////////////

class SGraphActionCategoryWidget : public SCompoundWidget
{
	SLATE_BEGIN_ARGS( SGraphActionCategoryWidget ) 
	{}
		SLATE_ATTRIBUTE( FText, HighlightText )
		SLATE_EVENT( FOnTextCommitted, OnTextCommitted )
		SLATE_EVENT( FIsSelected, IsSelected )
		SLATE_ATTRIBUTE( bool, IsReadOnly )
	SLATE_END_ARGS()

	TWeakPtr<FGraphActionNode> ActionNode;
	TAttribute<bool> IsReadOnly;
public:
	TWeakPtr<SInlineEditableTextBlock> InlineWidget;

	void Construct( const FArguments& InArgs, TSharedPtr<FGraphActionNode> InActionNode )
	{
		ActionNode = InActionNode;

		FText CategoryTooltip;
		FString CategoryLink, CategoryExcerpt;
		FEditorCategoryUtils::GetCategoryTooltipInfo(*InActionNode->GetDisplayName().ToString(), CategoryTooltip, CategoryLink, CategoryExcerpt);

		TSharedRef<SToolTip> ToolTipWidget = IDocumentation::Get()->CreateToolTip(CategoryTooltip, NULL, CategoryLink, CategoryExcerpt);
		IsReadOnly = InArgs._IsReadOnly;

		this->ChildSlot
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SAssignNew(InlineWidget, SInlineEditableTextBlock)
				.Font( FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 9 )  )
				.Text( FEditorCategoryUtils::GetCategoryDisplayString(InActionNode->GetDisplayName()) )
				.ToolTip( ToolTipWidget )
				.HighlightText( InArgs._HighlightText )
				.OnVerifyTextChanged( this, &SGraphActionCategoryWidget::OnVerifyTextChanged )
				.OnTextCommitted( InArgs._OnTextCommitted )
				.IsSelected( InArgs._IsSelected )
				.IsReadOnly( InArgs._IsReadOnly )
			]
		];
	}

	// SWidget interface
	virtual FReply OnDrop( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override
	{
		TSharedPtr<FGraphEditorDragDropAction> GraphDropOp = DragDropEvent.GetOperationAs<FGraphEditorDragDropAction>();
		if (GraphDropOp.IsValid())
		{
			GraphDropOp->DroppedOnCategory( ActionNode.Pin()->GetCategoryPath() );
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}

	virtual void OnDragEnter( const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent ) override
	{
		TSharedPtr<FGraphEditorDragDropAction> GraphDropOp = DragDropEvent.GetOperationAs<FGraphEditorDragDropAction>();
		if (GraphDropOp.IsValid())
		{
			GraphDropOp->SetHoveredCategoryName( ActionNode.Pin()->GetDisplayName() );
		}
	}

	virtual void OnDragLeave( const FDragDropEvent& DragDropEvent ) override
	{
		TSharedPtr<FGraphEditorDragDropAction> GraphDropOp = DragDropEvent.GetOperationAs<FGraphEditorDragDropAction>();
		if (GraphDropOp.IsValid())
		{
			GraphDropOp->SetHoveredCategoryName( FText::GetEmpty() );
		}
	}

	// End of SWidget interface

	/** Callback for the SInlineEditableTextBlock to verify the text before commit */
	bool OnVerifyTextChanged(const FText& InText, FText& OutErrorMessage)
	{
		if(InText.ToString().Len() > NAME_SIZE)
		{
			OutErrorMessage = LOCTEXT("CategoryNameTooLong_Error", "Name too long!");
			return false;
		}

		return true;
	}
};

//////////////////////////////////////////////////////////////////////////

void SGraphActionMenu::Construct( const FArguments& InArgs, bool bIsReadOnly/* = true*/ )
{
	this->SelectedSuggestion = INDEX_NONE;
	this->bIgnoreUIUpdate = false;
	this->bUseSectionStyling = InArgs._UseSectionStyling;

	this->bAutoExpandActionMenu = InArgs._AutoExpandActionMenu;
	this->bShowFilterTextBox = InArgs._ShowFilterTextBox;
	this->bAlphaSortItems = InArgs._AlphaSortItems;
	this->OnActionSelected = InArgs._OnActionSelected;
	this->OnActionDoubleClicked = InArgs._OnActionDoubleClicked;
	this->OnActionDragged = InArgs._OnActionDragged;
	this->OnCategoryDragged = InArgs._OnCategoryDragged;
	this->OnCreateWidgetForAction = InArgs._OnCreateWidgetForAction;
	this->OnCreateCustomRowExpander = InArgs._OnCreateCustomRowExpander;
	this->OnCollectAllActions = InArgs._OnCollectAllActions;
	this->OnCollectStaticSections = InArgs._OnCollectStaticSections;
	this->OnCategoryTextCommitted = InArgs._OnCategoryTextCommitted;
	this->OnCanRenameSelectedAction = InArgs._OnCanRenameSelectedAction;
	this->OnGetSectionTitle = InArgs._OnGetSectionTitle;
	this->OnGetSectionToolTip = InArgs._OnGetSectionToolTip;
	this->OnGetSectionWidget = InArgs._OnGetSectionWidget;
	this->FilteredRootAction = FGraphActionNode::NewRootNode();
	this->OnActionMatchesName = InArgs._OnActionMatchesName;
	
	// If a delegate for filtering text is passed in, assign it so that it will be used instead of the built-in filter box
	if(InArgs._OnGetFilterText.IsBound())
	{
		this->OnGetFilterText = InArgs._OnGetFilterText;
	}

	TreeView = SNew(STreeView< TSharedPtr<FGraphActionNode> >)
		.ItemHeight(24)
		.TreeItemsSource(&(this->FilteredRootAction->Children))
		.OnGenerateRow(this, &SGraphActionMenu::MakeWidget, bIsReadOnly)
		.OnSelectionChanged(this, &SGraphActionMenu::OnItemSelected)
		.OnMouseButtonDoubleClick(this, &SGraphActionMenu::OnItemDoubleClicked)
		.OnContextMenuOpening(InArgs._OnContextMenuOpening)
		.OnGetChildren(this, &SGraphActionMenu::OnGetChildrenForCategory)
		.SelectionMode(ESelectionMode::Single)
		.OnItemScrolledIntoView(this, &SGraphActionMenu::OnItemScrolledIntoView)
		.OnSetExpansionRecursive(this, &SGraphActionMenu::OnSetExpansionRecursive);


	this->ChildSlot
	[
		SNew(SVerticalBox)

		// FILTER BOX
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew(FilterTextBox, SSearchBox)
			// If there is an external filter delegate, do not display this filter box
			.Visibility(InArgs._OnGetFilterText.IsBound()? EVisibility::Collapsed : EVisibility::Visible)
			.OnTextChanged( this, &SGraphActionMenu::OnFilterTextChanged )
			.OnTextCommitted( this, &SGraphActionMenu::OnFilterTextCommitted )
		]

		// ACTION LIST
		+SVerticalBox::Slot()
		.Padding(FMargin(0.0f, 2.0f, 0.0f, 0.0f))
		.FillHeight(1.f)
		[
			SNew(SScrollBorder, TreeView.ToSharedRef())
			[
				TreeView.ToSharedRef()
			]
		]
	];

	// When the search box has focus, we want first chance handling of any key down events so we can handle the up/down and escape keys the way we want
	FilterTextBox->SetOnKeyDownHandler(FOnKeyDown::CreateSP(this, &SGraphActionMenu::OnKeyDown));

	if (!InArgs._ShowFilterTextBox)
	{
		FilterTextBox->SetVisibility(EVisibility::Collapsed);
	}

	// Get all actions.
	RefreshAllActions(false);
}

void SGraphActionMenu::RefreshAllActions(bool bPreserveExpansion, bool bHandleOnSelectionEvent/*=true*/)
{
	// Save Selection (of only the first selected thing)
	TArray< TSharedPtr<FGraphActionNode> > SelectedNodes = TreeView->GetSelectedItems();
	TSharedPtr<FGraphActionNode> SelectedAction = SelectedNodes.Num() > 0 ? SelectedNodes[0] : nullptr;

	AllActions.Empty();
	OnCollectAllActions.ExecuteIfBound(AllActions);
	GenerateFilteredItems(bPreserveExpansion);

	// Re-apply selection #0 if possible
	if (SelectedAction.IsValid())
	{
		// Clear the selection, we will be re-selecting the previous action
		TreeView->ClearSelection();

		if(bHandleOnSelectionEvent)
		{
			SelectItemByName(*SelectedAction->GetDisplayName().ToString(), ESelectInfo::OnMouseClick, SelectedAction->SectionID, SelectedNodes[0]->IsCategoryNode());
		}
		else
		{
			// If we do not want to handle the selection, set it directly so it will reselect the item but not handle the event.
			SelectItemByName(*SelectedAction->GetDisplayName().ToString(), ESelectInfo::Direct, SelectedAction->SectionID, SelectedNodes[0]->IsCategoryNode());
		}
	}
}

void SGraphActionMenu::GetSectionExpansion(TMap<int32, bool>& SectionExpansion) const
{

}

void SGraphActionMenu::SetSectionExpansion(const TMap<int32, bool>& InSectionExpansion)
{
	for ( auto& PossibleSection : FilteredRootAction->Children )
	{
		if ( PossibleSection->IsSectionHeadingNode() )
		{
			const bool* IsExpanded = InSectionExpansion.Find(PossibleSection->SectionID);
			if ( IsExpanded != nullptr )
			{
				TreeView->SetItemExpansion(PossibleSection, *IsExpanded);
			}
		}
	}
}

TSharedRef<SEditableTextBox> SGraphActionMenu::GetFilterTextBox()
{
	return FilterTextBox.ToSharedRef();
}

void SGraphActionMenu::GetSelectedActions(TArray< TSharedPtr<FEdGraphSchemaAction> >& OutSelectedActions) const
{
	OutSelectedActions.Empty();

	TArray< TSharedPtr<FGraphActionNode> > SelectedNodes = TreeView->GetSelectedItems();
	if(SelectedNodes.Num() > 0)
	{
		for ( int32 NodeIndex = 0; NodeIndex < SelectedNodes.Num(); NodeIndex++ )
		{
			OutSelectedActions.Append( SelectedNodes[NodeIndex]->Actions );
		}
	}
}

void SGraphActionMenu::OnRequestRenameOnActionNode()
{
	TArray< TSharedPtr<FGraphActionNode> > SelectedNodes = TreeView->GetSelectedItems();
	if(SelectedNodes.Num() > 0)
	{
		if (!SelectedNodes[0]->BroadcastRenameRequest())
		{
			TreeView->RequestScrollIntoView(SelectedNodes[0]);
		}
	}
}

bool SGraphActionMenu::CanRequestRenameOnActionNode() const
{
	TArray< TSharedPtr<FGraphActionNode> > SelectedNodes = TreeView->GetSelectedItems();
	if(SelectedNodes.Num() == 1 && OnCanRenameSelectedAction.IsBound())
	{
		return OnCanRenameSelectedAction.Execute(SelectedNodes[0]);
	}

	return false;
}

FString SGraphActionMenu::GetSelectedCategoryName() const
{
	TArray< TSharedPtr<FGraphActionNode> > SelectedNodes = TreeView->GetSelectedItems();
	return (SelectedNodes.Num() > 0) ? SelectedNodes[0]->GetDisplayName().ToString() : FString();
}

void SGraphActionMenu::GetSelectedCategorySubActions(TArray<TSharedPtr<FEdGraphSchemaAction>>& OutActions) const
{
	TArray< TSharedPtr<FGraphActionNode> > SelectedNodes = TreeView->GetSelectedItems();
	for ( int32 SelectionIndex = 0; SelectionIndex < SelectedNodes.Num(); SelectionIndex++ )
	{
		if ( SelectedNodes[SelectionIndex].IsValid() )
		{
			GetCategorySubActions(SelectedNodes[SelectionIndex], OutActions);
		}
	}
}

void SGraphActionMenu::GetCategorySubActions(TWeakPtr<FGraphActionNode> InAction, TArray<TSharedPtr<FEdGraphSchemaAction>>& OutActions) const
{
	if(InAction.IsValid())
	{
		TSharedPtr<FGraphActionNode> CategoryNode = InAction.Pin();
		TArray<TSharedPtr<FGraphActionNode>> Children;
		CategoryNode->GetLeafNodes(Children);

		for (int32 i = 0; i < Children.Num(); ++i)
		{
			TSharedPtr<FGraphActionNode> CurrentChild = Children[i];

			if (CurrentChild.IsValid() && CurrentChild->IsActionNode())
			{
				for ( int32 ActionIndex = 0; ActionIndex != CurrentChild->Actions.Num(); ActionIndex++ )
				{
					OutActions.Add(CurrentChild->Actions[ActionIndex]);
				}
			}
		}
	}
}

bool SGraphActionMenu::SelectItemByName(const FName& ItemName, ESelectInfo::Type SelectInfo, int32 SectionId/* = INDEX_NONE */, bool bIsCategory/* = false*/)
{
	if (ItemName != NAME_None)
	{
		TSharedPtr<FGraphActionNode> SelectionNode;

		TArray<TSharedPtr<FGraphActionNode>> GraphNodes;
		FilteredRootAction->GetAllNodes(GraphNodes);
		for (int32 i = 0; i < GraphNodes.Num() && !SelectionNode.IsValid(); ++i)
		{
			TSharedPtr<FGraphActionNode> CurrentGraphNode = GraphNodes[i];
			FEdGraphSchemaAction* GraphAction = CurrentGraphNode->GetPrimaryAction().Get();

			// If the user is attempting to select a category, make sure it's a category
			if( CurrentGraphNode->IsCategoryNode() == bIsCategory )
			{
				if(SectionId == INDEX_NONE || CurrentGraphNode->SectionID == SectionId)
				{
					if (GraphAction)
					{
						if ((OnActionMatchesName.IsBound() && OnActionMatchesName.Execute(GraphAction, ItemName)) || GraphActionMenuHelpers::ActionMatchesName(GraphAction, ItemName))
						{
							SelectionNode = GraphNodes[i];

							break;
						}
					}
					
					if (CurrentGraphNode->GetDisplayName().ToString() == FName::NameToDisplayString(ItemName.ToString(), false))
					{
						SelectionNode = CurrentGraphNode;

						break;
					}
				}
			}

			// One of the children may match
			for(int32 ChildIdx = 0; ChildIdx < CurrentGraphNode->Children.Num() && !SelectionNode.IsValid(); ++ChildIdx)
			{
				TSharedPtr<FGraphActionNode> CurrentChildNode = CurrentGraphNode->Children[ChildIdx];

				for ( int32 ActionIndex = 0; ActionIndex < CurrentChildNode->Actions.Num(); ActionIndex++ )
				{
					FEdGraphSchemaAction* ChildGraphAction = CurrentChildNode->Actions[ActionIndex].Get();

					// If the user is attempting to select a category, make sure it's a category
					if( CurrentChildNode->IsCategoryNode() == bIsCategory )
					{
						if(SectionId == INDEX_NONE || CurrentChildNode->SectionID == SectionId)
						{
							if(ChildGraphAction)
							{
								if ((OnActionMatchesName.IsBound() && OnActionMatchesName.Execute(ChildGraphAction, ItemName)) || GraphActionMenuHelpers::ActionMatchesName(ChildGraphAction, ItemName))
								{
									SelectionNode = GraphNodes[i]->Children[ChildIdx];

									break;
								}
							}
							else if (CurrentChildNode->GetDisplayName().ToString() == FName::NameToDisplayString(ItemName.ToString(), false))
							{
								SelectionNode = CurrentChildNode;

								break;
							}
						}
					}
				}
			}
		}

		if(SelectionNode.IsValid())
		{
			// Expand the parent nodes
			for (TSharedPtr<FGraphActionNode> ParentAction = SelectionNode->GetParentNode().Pin(); ParentAction.IsValid(); ParentAction = ParentAction->GetParentNode().Pin())
			{
				TreeView->SetItemExpansion(ParentAction, true);
			}

			// Select the node
			TreeView->SetSelection(SelectionNode,SelectInfo);
			TreeView->RequestScrollIntoView(SelectionNode);
			return true;
		}
	}
	else
	{
		TreeView->ClearSelection();
		return true;
	}
	return false;
}

void SGraphActionMenu::ExpandCategory(const FText& CategoryName)
{
	if (!CategoryName.IsEmpty())
	{
		TArray<TSharedPtr<FGraphActionNode>> GraphNodes;
		FilteredRootAction->GetAllNodes(GraphNodes);
		for (int32 i = 0; i < GraphNodes.Num(); ++i)
		{
			if (GraphNodes[i]->GetDisplayName().EqualTo(CategoryName))
			{
				GraphNodes[i]->ExpandAllChildren(TreeView);
			}
		}
	}
}

static bool CompareGraphActionNode(TSharedPtr<FGraphActionNode> A, TSharedPtr<FGraphActionNode> B)
{
	check(A.IsValid());
	check(B.IsValid());

	// First check grouping is the same
	if (A->GetDisplayName().ToString() != B->GetDisplayName().ToString())
	{
		return false;
	}

	if (A->HasValidAction() && B->HasValidAction())
	{
		return A->GetPrimaryAction()->GetMenuDescription().CompareTo(B->GetPrimaryAction()->GetMenuDescription()) == 0;
	}
	else if(!A->HasValidAction() && !B->HasValidAction())
	{
		return true;
	}
	else
	{
		return false;
	}
}

template<typename ItemType, typename ComparisonType> 
void RestoreExpansionState(TSharedPtr< STreeView<ItemType> > InTree, const TArray<ItemType>& ItemSource, const TSet<ItemType>& OldExpansionState, ComparisonType ComparisonFunction)
{
	check(InTree.IsValid());

	// Iterate over new tree items
	for(int32 ItemIdx=0; ItemIdx<ItemSource.Num(); ItemIdx++)
	{
		ItemType NewItem = ItemSource[ItemIdx];

		// Look through old expansion state
		for (typename TSet<ItemType>::TConstIterator OldExpansionIter(OldExpansionState); OldExpansionIter; ++OldExpansionIter)
		{
			const ItemType OldItem = *OldExpansionIter;
			// See if this matches this new item
			if(ComparisonFunction(OldItem, NewItem))
			{
				// It does, so expand it
				InTree->SetItemExpansion(NewItem, true);
			}
		}
	}
}

void SGraphActionMenu::GenerateFilteredItems(bool bPreserveExpansion)
{
	// First, save off current expansion state
	TSet< TSharedPtr<FGraphActionNode> > OldExpansionState;
	if(bPreserveExpansion)
	{
		TreeView->GetExpandedItems(OldExpansionState);
	}

	// Clear the filtered root action
	FilteredRootAction->ClearChildren();

	// Collect the list of always visible sections if any, and force the creation of those sections.
	if ( OnCollectStaticSections.IsBound() )
	{
		TArray<int32> StaticSectionIDs;
		OnCollectStaticSections.Execute(StaticSectionIDs);

		for ( int32 i = 0; i < StaticSectionIDs.Num(); i++ )
		{
			FilteredRootAction->AddSection(0, StaticSectionIDs[i]);
		}
	}
	
	// Trim and sanitized the filter text (so that it more likely matches the action descriptions)
	FString TrimmedFilterString = FText::TrimPrecedingAndTrailing(GetFilterText()).ToString();

	// Tokenize the search box text into a set of terms; all of them must be present to pass the filter
	TArray<FString> FilterTerms;
	TrimmedFilterString.ParseIntoArray(FilterTerms, TEXT(" "), true);
	for (auto& String : FilterTerms)
	{
		String = String.ToLower();
	}

	// Generate a list of sanitized versions of the strings
	TArray<FString> SanitizedFilterTerms;
	for (int32 iFilters = 0; iFilters < FilterTerms.Num() ; iFilters++)
	{
		FString EachString = FName::NameToDisplayString( FilterTerms[iFilters], false );
		EachString = EachString.Replace( TEXT( " " ), TEXT( "" ) );
		SanitizedFilterTerms.Add( EachString );
	}
	ensure( SanitizedFilterTerms.Num() == FilterTerms.Num() );// Both of these should match !

	const bool bRequiresFiltering = FilterTerms.Num() > 0;
	int32 BestMatchCount = 0;
	int32 BestMatchIndex = INDEX_NONE;		
	for (int32 CurTypeIndex=0; CurTypeIndex < AllActions.GetNumActions(); ++CurTypeIndex)
	{
		FGraphActionListBuilderBase::ActionGroup& CurrentAction = AllActions.GetAction( CurTypeIndex );

		// If we're filtering, search check to see if we need to show this action
		bool bShowAction = true;
		int32 EachWeight = 0;
		if (bRequiresFiltering)
		{
			// Combine the actions string, separate with \n so terms don't run into each other, and remove the spaces (incase the user is searching for a variable)
			// In the case of groups containing multiple actions, they will have been created and added at the same place in the code, using the same description
			// and keywords, so we only need to use the first one for filtering.
			const FString& SearchText = CurrentAction.GetSearchTextForFirstAction();

			FString EachTermSanitized;
			for (int32 FilterIndex = 0; (FilterIndex < FilterTerms.Num()) && bShowAction; ++FilterIndex)
			{
				const bool bMatchesTerm = (SearchText.Contains(FilterTerms[FilterIndex], ESearchCase::CaseSensitive) || (SearchText.Contains(SanitizedFilterTerms[FilterIndex], ESearchCase::CaseSensitive) == true));
				bShowAction = bShowAction && bMatchesTerm;
			}

			// Only if we are going to show the action do we want to generate the weight of the filter text
			if (bShowAction)
			{
				// Get the 'weight' of this in relation to the filter
				EachWeight = GetActionFilteredWeight( CurrentAction, FilterTerms, SanitizedFilterTerms );
			}
		}

		if (bShowAction)
		{
			// If this action has a greater relevance than others, cache its index.
			if( EachWeight > BestMatchCount )
			{
				BestMatchCount = EachWeight;
				BestMatchIndex = CurTypeIndex;
			}								
			FilteredRootAction->AddChild(CurrentAction);
		}
	}
	FilteredRootAction->SortChildren(bAlphaSortItems, /*bRecursive =*/true);

	TreeView->RequestTreeRefresh();

	// Update the filtered list (needs to be done in a separate pass because the list is sorted as items are inserted)
	FilteredActionNodes.Empty();
	FilteredRootAction->GetLeafNodes(FilteredActionNodes);

	// Get _all_ new nodes (flattened tree basically)
	TArray< TSharedPtr<FGraphActionNode> > AllNodes;
	FilteredRootAction->GetAllNodes(AllNodes);

	// If theres a BestMatchIndex find it in the actions nodes and select it (maybe this should check the current selected suggestion first ?)
	if( BestMatchIndex != INDEX_NONE ) 
	{
		FGraphActionListBuilderBase::ActionGroup& FilterSelectAction = AllActions.GetAction( BestMatchIndex );
		if( FilterSelectAction.Actions[0].IsValid() == true )
		{
			for (int32 iNode = 0; iNode < FilteredActionNodes.Num() ; iNode++)
			{
				if( FilteredActionNodes[ iNode ].Get()->GetPrimaryAction() == FilterSelectAction.Actions[ 0 ] )
				{
					SelectedSuggestion = iNode;
				}
			}	
		}	
	}

	// Make sure the selected suggestion stays within the filtered list
	if ((SelectedSuggestion >= 0) && (FilteredActionNodes.Num() > 0))
	{
		//@TODO: Should try to actually maintain the highlight on the same item if it survived the filtering
		SelectedSuggestion = FMath::Clamp<int32>(SelectedSuggestion, 0, FilteredActionNodes.Num() - 1);
		MarkActiveSuggestion();
	}
	else
	{
		SelectedSuggestion = INDEX_NONE;
	}


	if (ShouldExpandNodes())
	{
		// Expand all
		FilteredRootAction->ExpandAllChildren(TreeView);
	}
	else
	{
		// Expand to match the old state
		RestoreExpansionState< TSharedPtr<FGraphActionNode> >(TreeView, AllNodes, OldExpansionState, CompareGraphActionNode);
	}
}

int32 SGraphActionMenu::GetActionFilteredWeight( const FGraphActionListBuilderBase::ActionGroup& InCurrentAction, const TArray<FString>& InFilterTerms, const TArray<FString>& InSanitizedFilterTerms )
{
	// The overall 'weight'
	int32 TotalWeight = 0;

	// Some simple weight figures to help find the most appropriate match	
	const int32 WholeMatchWeightMultiplier = 2;
	const int32 WholeMatchLocalizedWeightMultiplier = 3;
	const int32 DescriptionWeight = 10;
	const int32 CategoryWeight = 1;
	const int32 NodeTitleWeight = 1;
	const int32 KeywordWeight = 4;

	// Helper array
	struct FArrayWithWeight
	{
		FArrayWithWeight(const TArray< FString >* InArray, int32 InWeight)
			: Array(InArray)
			, Weight(InWeight)
		{
		}

		const TArray< FString >* Array;
		int32			  Weight;
	};

	// Setup an array of arrays so we can do a weighted search			
	TArray< FArrayWithWeight > WeightedArrayList;
	
	int32 Action = 0;
	if( InCurrentAction.Actions[Action].IsValid() == true )
	{
		// Combine the actions string, separate with \n so terms don't run into each other, and remove the spaces (incase the user is searching for a variable)
		// In the case of groups containing multiple actions, they will have been created and added at the same place in the code, using the same description
		// and keywords, so we only need to use the first one for filtering.
		const FString& SearchText = InCurrentAction.GetSearchTextForFirstAction();

		// First the localized keywords
		WeightedArrayList.Add(FArrayWithWeight(&InCurrentAction.GetLocalizedSearchKeywordsArrayForFirstAction(), KeywordWeight));

		// The localized description
		WeightedArrayList.Add(FArrayWithWeight(&InCurrentAction.GetLocalizedMenuDescriptionArrayForFirstAction(), DescriptionWeight));

		// The node search localized title weight
		WeightedArrayList.Add(FArrayWithWeight(&InCurrentAction.GetLocalizedSearchTitleArrayForFirstAction(), NodeTitleWeight));

		// The localized category
		WeightedArrayList.Add(FArrayWithWeight(&InCurrentAction.GetLocalizedSearchCategoryArrayForFirstAction(), CategoryWeight));

		// First the keywords
		int32 NonLocalizedFirstIndex = WeightedArrayList.Add(FArrayWithWeight(&InCurrentAction.GetSearchKeywordsArrayForFirstAction(), KeywordWeight));

		// The description
		WeightedArrayList.Add(FArrayWithWeight(&InCurrentAction.GetMenuDescriptionArrayForFirstAction(), DescriptionWeight));

		// The node search title weight
		WeightedArrayList.Add(FArrayWithWeight(&InCurrentAction.GetSearchTitleArrayForFirstAction(), NodeTitleWeight));

		// The category
		WeightedArrayList.Add(FArrayWithWeight(&InCurrentAction.GetSearchCategoryArrayForFirstAction(), CategoryWeight));

		// Now iterate through all the filter terms and calculate a 'weight' using the values and multipliers
		const FString* EachTerm = nullptr;
		const FString* EachTermSanitized = nullptr;
		for (int32 FilterIndex = 0; FilterIndex < InFilterTerms.Num(); ++FilterIndex)
		{
			EachTerm = &InFilterTerms[FilterIndex];
			EachTermSanitized = &InSanitizedFilterTerms[FilterIndex];
			if( SearchText.Contains( *EachTerm, ESearchCase::CaseSensitive ) )
			{
				TotalWeight += 2;
			}
			else if (SearchText.Contains(*EachTermSanitized, ESearchCase::CaseSensitive))
			{
				TotalWeight++;
			}		
			// Now check the weighted lists	(We could further improve the hit weight by checking consecutive word matches)
			for (int32 iFindCount = 0; iFindCount < WeightedArrayList.Num() ; iFindCount++)
			{
				int32 WeightPerList = 0;
				const TArray<FString>& KeywordArray = *WeightedArrayList[iFindCount].Array;
				int32 EachWeight = WeightedArrayList[iFindCount].Weight;
				int32 WholeMatchCount = 0;
				int32 WholeMatchMultiplier = (iFindCount < NonLocalizedFirstIndex) ? WholeMatchLocalizedWeightMultiplier : WholeMatchWeightMultiplier;

				for (int32 iEachWord = 0; iEachWord < KeywordArray.Num() ; iEachWord++)
				{
					// If we get an exact match weight the find count to get exact matches higher priority
					if (KeywordArray[iEachWord].StartsWith(*EachTerm, ESearchCase::CaseSensitive))
					{
						if (iEachWord == 0)
						{
							WeightPerList += EachWeight * WholeMatchMultiplier;
						}
						else
						{
							WeightPerList += EachWeight;
						}
						WholeMatchCount++;
					}
					else if (KeywordArray[iEachWord].Contains(*EachTerm, ESearchCase::CaseSensitive))
					{
						WeightPerList += EachWeight;
					}
					if (KeywordArray[iEachWord].StartsWith(*EachTermSanitized, ESearchCase::CaseSensitive))
					{
						if (iEachWord == 0)
						{
							WeightPerList += EachWeight * WholeMatchMultiplier;
						}
						else
						{
							WeightPerList += EachWeight;
						}
						WholeMatchCount++;
					}
					else if (KeywordArray[iEachWord].Contains(*EachTermSanitized, ESearchCase::CaseSensitive))
					{
						WeightPerList += EachWeight / 2;
					}
				}
				// Increase the weight if theres a larger % of matches in the keyword list
				if( WholeMatchCount != 0 )
				{
					int32 PercentAdjust = ( 100 / KeywordArray.Num() ) * WholeMatchCount;
					WeightPerList *= PercentAdjust;
				}
				TotalWeight += WeightPerList;
			}
		}
	}
	return TotalWeight;
}

// Returns true if the tree should be autoexpanded
bool SGraphActionMenu::ShouldExpandNodes() const
{
	// Expand all the categories that have filter results, or when there are only a few to show
	const bool bFilterActive = !GetFilterText().IsEmpty();
	const bool bOnlyAFewTotal = AllActions.GetNumActions() < 10;

	return bFilterActive || bOnlyAFewTotal || bAutoExpandActionMenu;
}

bool SGraphActionMenu::CanRenameNode(TWeakPtr<FGraphActionNode> InNode) const
{
	return !OnCanRenameSelectedAction.Execute(InNode);
}

void SGraphActionMenu::OnFilterTextChanged( const FText& InFilterText )
{
	// Reset the selection if the string is empty
	if( InFilterText.IsEmpty() == true )
	{
		SelectedSuggestion = INDEX_NONE;
	}
	GenerateFilteredItems(false);
}

void SGraphActionMenu::OnFilterTextCommitted(const FText& InText, ETextCommit::Type CommitInfo)
{
	if (CommitInfo == ETextCommit::OnEnter)
	{
		TryToSpawnActiveSuggestion();
	}
}

bool SGraphActionMenu::TryToSpawnActiveSuggestion()
{
	TArray< TSharedPtr<FGraphActionNode> > SelectionList = TreeView->GetSelectedItems();

	if (SelectionList.Num() == 1)
	{
		// This isnt really a keypress - its Direct, but its always called from a keypress function. (Maybe pass the selectinfo in ?)
		OnItemSelected( SelectionList[0], ESelectInfo::OnKeyPress );
		return true;
	}
	else if (FilteredActionNodes.Num() == 1)
	{
		OnItemSelected( FilteredActionNodes[0], ESelectInfo::OnKeyPress );
		return true;
	}

	return false;
}

void SGraphActionMenu::OnGetChildrenForCategory( TSharedPtr<FGraphActionNode> InItem, TArray< TSharedPtr<FGraphActionNode> >& OutChildren )
{
	if (InItem->Children.Num())
	{
		OutChildren = InItem->Children;
	}
}

void SGraphActionMenu::OnNameTextCommitted(const FText& NewText, ETextCommit::Type InTextCommit, TWeakPtr< FGraphActionNode > InAction )
{
	if(OnCategoryTextCommitted.IsBound())
	{
		OnCategoryTextCommitted.Execute(NewText, InTextCommit, InAction);
	}
}

void SGraphActionMenu::OnItemScrolledIntoView( TSharedPtr<FGraphActionNode> InActionNode, const TSharedPtr<ITableRow>& InWidget )
{
	if (InActionNode->IsRenameRequestPending())
	{
		InActionNode->BroadcastRenameRequest();
	}
}

TSharedRef<ITableRow> SGraphActionMenu::MakeWidget( TSharedPtr<FGraphActionNode> InItem, const TSharedRef<STableViewBase>& OwnerTable, bool bIsReadOnly )
{
	TSharedPtr<IToolTip> SectionToolTip;

	if ( InItem->IsSectionHeadingNode() )
	{
		if ( OnGetSectionToolTip.IsBound() )
		{
			SectionToolTip = OnGetSectionToolTip.Execute(InItem->SectionID);
		}
	}

	// In the case of FGraphActionNodes that have multiple actions, all of the actions will
	// have the same text as they will have been created at the same point - only the actual
	// action itself will differ, which is why parts of this function only refer to InItem->Actions[0]
	// rather than iterating over the array

	// Create the widget but do not add any content, the widget is needed to pass the IsSelectedExclusively function down to the potential SInlineEditableTextBlock widget
	TSharedPtr< STableRow< TSharedPtr<FGraphActionNode> > > TableRow;
	
	if ( InItem->IsSectionHeadingNode() )
	{
		TableRow = SNew(SCategoryHeaderTableRow< TSharedPtr<FGraphActionNode> >, OwnerTable)
			.ToolTip(SectionToolTip);
	}
	else
	{
		const FTableRowStyle* Style = bUseSectionStyling ? &FEditorStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.DarkRow") : &FCoreStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row");

		TableRow = SNew(STableRow< TSharedPtr<FGraphActionNode> >, OwnerTable)
			.Style(Style)
			.OnDragDetected(this, &SGraphActionMenu::OnItemDragDetected)
			.ShowSelection(!InItem->IsSeparator());
	}

	TSharedPtr<SHorizontalBox> RowContainer;
	TableRow->SetRowContent
	( 
		SAssignNew(RowContainer, SHorizontalBox)
	);

	TSharedPtr<SWidget> RowContent;
	FMargin RowPadding = FMargin(0, 2);

	if( InItem->IsActionNode() )
	{
		check(InItem->HasValidAction());

		FCreateWidgetForActionData CreateData(&InItem->OnRenameRequest());
		CreateData.Action = InItem->GetPrimaryAction();
		CreateData.HighlightText = TAttribute<FText>(this, &SGraphActionMenu::GetFilterText);
		CreateData.MouseButtonDownDelegate = FCreateWidgetMouseButtonDown::CreateSP( this, &SGraphActionMenu::OnMouseButtonDownEvent );

		if(OnCreateWidgetForAction.IsBound())
		{
			CreateData.IsRowSelectedDelegate = FIsSelected::CreateSP( TableRow.Get(), &STableRow< TSharedPtr<FGraphActionNode> >::IsSelected );
			CreateData.bIsReadOnly = bIsReadOnly;
			CreateData.bHandleMouseButtonDown = false;		//Default to NOT using the delegate. OnCreateWidgetForAction can set to true if we need it
			RowContent = OnCreateWidgetForAction.Execute( &CreateData );
		}
		else
		{
			RowContent = SNew(SDefaultGraphActionWidget, &CreateData);
		}
	}
	else if( InItem->IsCategoryNode() )
	{
		TWeakPtr< FGraphActionNode > WeakItem = InItem;

		// Hook up the delegate for verifying the category action is read only or not
		SGraphActionCategoryWidget::FArguments ReadOnlyArgument;
		if(bIsReadOnly)
		{
			ReadOnlyArgument.IsReadOnly(bIsReadOnly);
		}
		else
		{
			ReadOnlyArgument.IsReadOnly(this, &SGraphActionMenu::CanRenameNode, WeakItem);
		}

		TSharedRef<SGraphActionCategoryWidget> CategoryWidget =
			SNew(SGraphActionCategoryWidget, InItem)
			.HighlightText(this, &SGraphActionMenu::GetFilterText)
			.OnTextCommitted(this, &SGraphActionMenu::OnNameTextCommitted, TWeakPtr< FGraphActionNode >(InItem))
			.IsSelected(TableRow.Get(), &STableRow< TSharedPtr<FGraphActionNode> >::IsSelectedExclusively)
			.IsReadOnly(ReadOnlyArgument._IsReadOnly);

		if(!bIsReadOnly)
		{
			InItem->OnRenameRequest().BindSP( CategoryWidget->InlineWidget.Pin().Get(), &SInlineEditableTextBlock::EnterEditingMode );
		}

		RowContent = CategoryWidget;
	}
	else if( InItem->IsSeparator() )
	{
		RowPadding = FMargin(0);

		FText SectionTitle;
		if( OnGetSectionTitle.IsBound() )
		{
			SectionTitle = OnGetSectionTitle.Execute(InItem->SectionID);
		}

		if( SectionTitle.IsEmpty() )
		{
			RowContent = SNew( SVerticalBox )
			.Visibility(EVisibility::HitTestInvisible)

			+ SVerticalBox::Slot()
			.AutoHeight()
			// Add some empty space before the line, and a tiny bit after it
			.Padding( 0.0f, 5.f, 0.0f, 5.f )
			[
				SNew( SBorder )

				// We'll use the border's padding to actually create the horizontal line
				.Padding(FEditorStyle::GetMargin(TEXT("Menu.Separator.Padding")))

				// Separator graphic
				.BorderImage( FEditorStyle::GetBrush( TEXT( "Menu.Separator" ) ) )
			];
		}
		else
		{
			RowContent = SNew(SHorizontalBox)

			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			[
				SNew(SRichTextBlock)
				.Text(SectionTitle)
				.DecoratorStyleSet(&FEditorStyle::Get())
				.TextStyle(FEditorStyle::Get(), "DetailsView.CategoryTextStyle")
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			.Padding(FMargin(0,0,2,0))
			[
				OnGetSectionWidget.IsBound() ? OnGetSectionWidget.Execute(TableRow.ToSharedRef(), InItem->SectionID) : SNullWidget::NullWidget
			];
		}
	}

	TSharedPtr<SExpanderArrow> ExpanderWidget;
	if (OnCreateCustomRowExpander.IsBound())
	{
		FCustomExpanderData CreateData;
		CreateData.TableRow        = TableRow;
		CreateData.WidgetContainer = RowContainer;

		if (InItem->IsActionNode())
		{
			check(InItem->HasValidAction());
			CreateData.RowAction = InItem->GetPrimaryAction();
		}

		ExpanderWidget = OnCreateCustomRowExpander.Execute(CreateData);
	}
	else 
	{
		ExpanderWidget =
			SNew(SExpanderArrow, TableRow)
			.BaseIndentLevel(1);
	}

	RowContainer->AddSlot()
		.AutoWidth()
		.VAlign(VAlign_Fill)
		.HAlign(HAlign_Right)
	[
		ExpanderWidget.ToSharedRef()
	];

	RowContainer->AddSlot()
	.FillWidth(1.0)
	.Padding(RowPadding)
	[
		RowContent.ToSharedRef()
	];

	return TableRow.ToSharedRef();
}

FText SGraphActionMenu::GetFilterText() const
{
	// If there is an external source for the filter, use that text instead
	if(OnGetFilterText.IsBound())
	{
		return OnGetFilterText.Execute();
	}
	
	return FilterTextBox->GetText();;
}

void SGraphActionMenu::OnItemSelected( TSharedPtr< FGraphActionNode > InSelectedItem, ESelectInfo::Type SelectInfo )
{
	if (!bIgnoreUIUpdate)
	{
		HandleSelection(InSelectedItem, SelectInfo);
	}
}

void SGraphActionMenu::OnItemDoubleClicked( TSharedPtr< FGraphActionNode > InClickedItem )
{
	if ( InClickedItem.IsValid() && !bIgnoreUIUpdate )
	{
		if ( InClickedItem->IsActionNode() )
		{
			OnActionDoubleClicked.ExecuteIfBound(InClickedItem->Actions);
		}
		else if (InClickedItem->Children.Num())
		{
			TreeView->SetItemExpansion(InClickedItem, !TreeView->IsItemExpanded(InClickedItem));
		}
	}
}

FReply SGraphActionMenu::OnItemDragDetected( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	// Start a function-call drag event for any entry that can be called by kismet
	if (MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		TArray< TSharedPtr<FGraphActionNode> > SelectedNodes = TreeView->GetSelectedItems();
		if(SelectedNodes.Num() > 0)
		{
			TSharedPtr<FGraphActionNode> Node = SelectedNodes[0];
			// Dragging a ctaegory
			if(Node.IsValid() && Node->IsCategoryNode())
			{
				if(OnCategoryDragged.IsBound())
				{
					return OnCategoryDragged.Execute(Node->GetCategoryPath(), MouseEvent);
				}
			}
			// Dragging an action
			else
			{
				if(OnActionDragged.IsBound())
				{
					TArray< TSharedPtr<FEdGraphSchemaAction> > Actions;
					GetSelectedActions(Actions);
					return OnActionDragged.Execute(Actions, MouseEvent);
				}
			}
		}
	}

	return FReply::Unhandled();
}

bool SGraphActionMenu::OnMouseButtonDownEvent( TWeakPtr<FEdGraphSchemaAction> InAction )
{
	bool bResult = false;
	if( (!bIgnoreUIUpdate) && InAction.IsValid() )
	{
		TArray< TSharedPtr<FGraphActionNode> > SelectionList = TreeView->GetSelectedItems();
		TSharedPtr<FGraphActionNode> SelectedNode;
		if (SelectionList.Num() == 1)
		{	
			SelectedNode = SelectionList[0];			
		}
		else if (FilteredActionNodes.Num() == 1)
		{
			SelectedNode = FilteredActionNodes[0];			
		}
		if (SelectedNode.IsValid() && SelectedNode->HasValidAction())
		{
			if( SelectedNode->GetPrimaryAction().Get() == InAction.Pin().Get() )
			{				
				bResult = HandleSelection( SelectedNode, ESelectInfo::OnMouseClick );
			}
		}
	}
	return bResult;
}

FReply SGraphActionMenu::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& KeyEvent )
{
	int32 SelectionDelta = 0;

	// Escape dismisses the menu without placing a node
	if (KeyEvent.GetKey() == EKeys::Escape)
	{
		FSlateApplication::Get().DismissAllMenus();
		return FReply::Handled();
	}
	else if ((KeyEvent.GetKey() == EKeys::Enter) && !bIgnoreUIUpdate)
	{
		return TryToSpawnActiveSuggestion() ? FReply::Handled() : FReply::Unhandled();
	}
	else if (FilteredActionNodes.Num() > 0 && !FilterTextBox->GetText().IsEmpty())
	{
		// Up and down move thru the filtered node list
		if (KeyEvent.GetKey() == EKeys::Up)
		{
			SelectionDelta = -1;
		}
		else if (KeyEvent.GetKey() == EKeys::Down)
		{
			SelectionDelta = +1;
		}

		if (SelectionDelta != 0)
		{
			// If we have no selected suggestion then we need to use the items in the root to set the selection and set the focus
			if( SelectedSuggestion == INDEX_NONE )
			{
				SelectedSuggestion = (SelectedSuggestion + SelectionDelta + FilteredRootAction->Children.Num()) % FilteredRootAction->Children.Num();
				MarkActiveSuggestion();
				return FReply::Handled();
			}

			//Move up or down one, wrapping around
			SelectedSuggestion = (SelectedSuggestion + SelectionDelta + FilteredActionNodes.Num()) % FilteredActionNodes.Num();

			MarkActiveSuggestion();

			return FReply::Handled();
		}
	}
	else
	{
		// When all else fails, it means we haven't filtered the list and we want to handle it as if we were just scrolling through a normal tree view
		return TreeView->OnKeyDown(FindChildGeometry(MyGeometry, TreeView.ToSharedRef()), KeyEvent);
	}

	return FReply::Unhandled();
}

void SGraphActionMenu::MarkActiveSuggestion()
{
	TGuardValue<bool> PreventSelectionFromTriggeringCommit(bIgnoreUIUpdate, true);

	if (SelectedSuggestion >= 0)
	{
		TSharedPtr<FGraphActionNode>& ActionToSelect = FilteredActionNodes[SelectedSuggestion];

		TreeView->SetSelection(ActionToSelect);
		TreeView->RequestScrollIntoView(ActionToSelect);
	}
	else
	{
		TreeView->ClearSelection();
	}
}

void SGraphActionMenu::AddReferencedObjects( FReferenceCollector& Collector )
{
	for (int32 CurTypeIndex=0; CurTypeIndex < AllActions.GetNumActions(); ++CurTypeIndex)
	{
		FGraphActionListBuilderBase::ActionGroup& Action = AllActions.GetAction( CurTypeIndex );

		for ( int32 ActionIndex = 0; ActionIndex < Action.Actions.Num(); ActionIndex++ )
		{
			Action.Actions[ActionIndex]->AddReferencedObjects(Collector);
		}
	}
}

bool SGraphActionMenu::HandleSelection( TSharedPtr< FGraphActionNode > &InSelectedItem, ESelectInfo::Type InSelectionType )
{
	bool bResult = false;
	if( OnActionSelected.IsBound() )
	{
		if ( InSelectedItem.IsValid() && InSelectedItem->IsActionNode() )
		{
			OnActionSelected.Execute(InSelectedItem->Actions, InSelectionType);
			bResult = true;
		}
		else
		{
			OnActionSelected.Execute(TArray< TSharedPtr<FEdGraphSchemaAction> >(), InSelectionType);
			bResult = true;
		}
	}
	return bResult;
}

void SGraphActionMenu::OnSetExpansionRecursive(TSharedPtr<FGraphActionNode> InTreeNode, bool bInIsItemExpanded)
{
	if (InTreeNode.IsValid() && InTreeNode->Children.Num())
	{
		TreeView->SetItemExpansion(InTreeNode, bInIsItemExpanded);

		for (TSharedPtr<FGraphActionNode> Child : InTreeNode->Children)
		{
			OnSetExpansionRecursive(Child, bInIsItemExpanded);
		}
	}
}

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE
