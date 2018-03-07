// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GraphActionNode.h"

/*******************************************************************************
 * Static FGraphActionNode Helpers
 ******************************************************************************/

struct FGraphActionNodeImpl
{
	static const int32 DEFAULT_GROUPING = 0;

	/**
	 * Utility sort function. Compares nodes based off of section, grouping, and
	 * type.
	 * 
	 * @param  LhsMenuNodePtr	The node to determine if it should come first.
	 * @param  RhsMenuNodePtr	The node to determine if it should come second.
	 * @return True if LhsMenuNodePtr should come before RhsMenuNodePtr.
	 */
	static bool NodeCompare(TSharedPtr<FGraphActionNode> const& LhsMenuNodePtr, TSharedPtr<FGraphActionNode> const& RhsMenuNodePtr);

	/**
	 * Utility sort function. Compares nodes based off of section, grouping, 
	 * type, and then alphabetically.
	 * 
	 * @param  LhsMenuNodePtr	The node to determine if it should come first.
	 * @param  RhsMenuNodePtr	The node to determine if it should come second.
	 * @return True if LhsMenuNodePtr should come before RhsMenuNodePtr.
	 */
	static bool AlphabeticalNodeCompare(TSharedPtr<FGraphActionNode> const& LhsMenuNodePtr, TSharedPtr<FGraphActionNode> const& RhsMenuNodePtr);
};

//------------------------------------------------------------------------------
bool FGraphActionNodeImpl::NodeCompare(TSharedPtr<FGraphActionNode> const& LhsMenuNodePtr, TSharedPtr<FGraphActionNode> const& RhsMenuNodePtr)
{
	FGraphActionNode* LhsMenuNode = LhsMenuNodePtr.Get();
	FGraphActionNode* RhsMenuNode = RhsMenuNodePtr.Get();

	bool const bLhsIsCategory  = LhsMenuNode->IsCategoryNode();
	bool const bRhsIsCategory  = RhsMenuNode->IsCategoryNode();
	bool const bLhsIsSeparator = LhsMenuNode->IsSeparator();
	bool const bRhsIsSeparator = RhsMenuNode->IsSeparator();
	bool const bLhsIsSectionHeader = LhsMenuNode->IsSectionHeadingNode();
	bool const bRhsIsSectionHeader = RhsMenuNode->IsSectionHeadingNode();

	if (LhsMenuNode->SectionID != RhsMenuNode->SectionID)
	{
		// since we don't add section headers for children that have the same
		// section as their parents (the header is above the parent), we need to
		// organize them first (so they're seemingly under the same header)
		if ((LhsMenuNode->SectionID == LhsMenuNode->ParentNode.Pin()->SectionID) && 
			(LhsMenuNode->SectionID != FGraphActionNode::INVALID_SECTION_ID))
		{
			return true;
		}
		else // otherwise...
		{
			// sections are ordered in ascending order
			return (LhsMenuNode->SectionID < RhsMenuNode->SectionID);
		}		
	}
	else if (bLhsIsSectionHeader != bRhsIsSectionHeader)
	{
		// section headers go to the top of that section
		return bLhsIsSectionHeader;
	}
	else if (LhsMenuNode->Grouping != RhsMenuNode->Grouping)
	{
		// groups are ordered in descending order
		return (LhsMenuNode->Grouping >= RhsMenuNode->Grouping);
	}
	// next, make sure separators are preserved
	else if (bLhsIsSeparator != bRhsIsSeparator)
	{
		// separators with the same grouping go to the bottom of that "group"
		return bRhsIsSeparator;
	}
	// next, categories get listed before action nodes
	else if (bLhsIsCategory != bRhsIsCategory)
	{
		return bLhsIsCategory;
	}
	else
	{
		// both lhs and rhs are seemingly the same, so to keep them menu from 
		// jumping around everytime an entry is added, we sort by the order they
		// were inserted
		return (LhsMenuNode->InsertOrder < RhsMenuNode->InsertOrder);
	}
}

//------------------------------------------------------------------------------
bool FGraphActionNodeImpl::AlphabeticalNodeCompare(TSharedPtr<FGraphActionNode> const& LhsMenuNodePtr, TSharedPtr<FGraphActionNode> const& RhsMenuNodePtr)
{
	FGraphActionNode* LhsMenuNode = LhsMenuNodePtr.Get();
	FGraphActionNode* RhsMenuNode = RhsMenuNodePtr.Get();

	bool const bLhsIsCategory  = LhsMenuNode->IsCategoryNode();
	bool const bRhsIsCategory  = RhsMenuNode->IsCategoryNode();
	bool const bLhsIsSeparator = LhsMenuNode->IsSeparator();
	bool const bRhsIsSeparator = RhsMenuNode->IsSeparator();
	bool const bLhsIsSectionHeader = LhsMenuNode->IsSectionHeadingNode();
	bool const bRhsIsSectionHeader = RhsMenuNode->IsSectionHeadingNode();

	if (LhsMenuNode->SectionID != RhsMenuNode->SectionID)
	{
		// since we don't add section headers for children that have the same
		// section as their parents (the header is above the parent), we need to
		// organize them first (so they're seemingly under the same header)
		if ((LhsMenuNode->SectionID == LhsMenuNode->ParentNode.Pin()->SectionID) &&
			(LhsMenuNode->SectionID != FGraphActionNode::INVALID_SECTION_ID))
		{
			return true;
		}
		else // otherwise...
		{
			// sections are ordered in ascending order
			return (LhsMenuNode->SectionID < RhsMenuNode->SectionID);
		}
	}
	else if (bLhsIsSectionHeader != bRhsIsSectionHeader)
	{
		// section headers go to the top of that section
		return bLhsIsSectionHeader;
	}
	else if (LhsMenuNode->Grouping != RhsMenuNode->Grouping)
	{
		// groups are ordered in descending order
		return (LhsMenuNode->Grouping >= RhsMenuNode->Grouping);
	}
	// next, make sure separators are preserved
	else if (bLhsIsSeparator != bRhsIsSeparator)
	{
		// separators with the same grouping go to the bottom of that "group"
		return bRhsIsSeparator;
	}
	// next, categories get listed before action nodes
	else if (bLhsIsCategory != bRhsIsCategory)
	{
		return bLhsIsCategory;
	}
	else if (bLhsIsCategory) // if both nodes are category nodes
	{
		// @TODO: Should we be doing localized compares for categories? Probably.
		return (LhsMenuNode->GetDisplayName().ToString() <= RhsMenuNode->GetDisplayName().ToString());
	}
	else // both nodes are action nodes
	{
		return (LhsMenuNode->GetDisplayName().CompareTo(RhsMenuNode->GetDisplayName()) <= 0);
	}
}

/*******************************************************************************
 * FGraphActionNode
 ******************************************************************************/

//------------------------------------------------------------------------------
TSharedPtr<FGraphActionNode> FGraphActionNode::NewRootNode()
{
	// same as a group-divider node, just with an invalid parent
	return MakeShareable(new FGraphActionNode(FGraphActionNodeImpl::DEFAULT_GROUPING, INVALID_SECTION_ID));
}

//------------------------------------------------------------------------------
FGraphActionNode::FGraphActionNode(int32 InGrouping, int32 InSectionID)
	: SectionID(InSectionID)
	, Grouping(InGrouping)
	, bPendingRenameRequest(false)
	, InsertOrder(0)
{
}

//------------------------------------------------------------------------------
FGraphActionNode::FGraphActionNode(TArray< TSharedPtr<FEdGraphSchemaAction> > const& ActionList, int32 InGrouping, int32 InSectionID)
	: SectionID(InSectionID)
	, Grouping(InGrouping)
	, Actions(ActionList)
	, bPendingRenameRequest(false)
	, InsertOrder(0)
{
}

//------------------------------------------------------------------------------
TSharedPtr<FGraphActionNode> FGraphActionNode::AddChild(FGraphActionListBuilderBase::ActionGroup const& ActionSet)
{
	const TArray<FString>& CategoryStack = ActionSet.GetCategoryChain();

	TSharedPtr<FGraphActionNode> ActionNode = FGraphActionNode::NewActionNode(ActionSet.Actions);
	AddChildRecursively(CategoryStack, 0, ActionNode);
	
	return ActionNode;
}

//------------------------------------------------------------------------------
TSharedPtr<FGraphActionNode> FGraphActionNode::AddSection(int32 InGrouping, int32 InSectionID)
{
	if ( !ChildSections.Contains(InSectionID) )
	{
		ChildSections.Add(InSectionID);

		TSharedPtr<FGraphActionNode> Section = NewSectionHeadingNode(SharedThis(this), InGrouping, InSectionID);
		InsertChild(Section);

		return Section;
	}

	return nullptr;
}

//------------------------------------------------------------------------------
void FGraphActionNode::SortChildren(bool bAlphabetically/* = true*/, bool bRecursive/* = true*/)
{
	if (bRecursive)
	{
		for (TSharedPtr<FGraphActionNode>& ChildNode : Children)
		{
			ChildNode->SortChildren(bAlphabetically, bRecursive);
		}
	}

	if (bAlphabetically)
	{
		Children.Sort(FGraphActionNodeImpl::AlphabeticalNodeCompare);
	}
	else
	{
		Children.Sort(FGraphActionNodeImpl::NodeCompare);
	}
}

//------------------------------------------------------------------------------
void FGraphActionNode::GetAllNodes(TArray< TSharedPtr<FGraphActionNode> >& OutNodeArray) const
{
	for (TSharedPtr<FGraphActionNode> const& ChildNode : Children)
	{
		OutNodeArray.Add(ChildNode);
		ChildNode->GetAllNodes(OutNodeArray);
	}
}

//------------------------------------------------------------------------------
void FGraphActionNode::GetLeafNodes(TArray< TSharedPtr<FGraphActionNode> >& OutLeafArray) const
{
	for (TSharedPtr<FGraphActionNode> const& ChildNode : Children)
	{
		if (ChildNode->IsCategoryNode())
		{
			ChildNode->GetLeafNodes(OutLeafArray);
		}
		else
		{
			// @TODO: sometimes, certain action nodes can have children as well
			//        (for sub-graphs in the "MyBlueprint" tab)
			OutLeafArray.Add(ChildNode);
		}
	}
}

//------------------------------------------------------------------------------
void FGraphActionNode::ExpandAllChildren(TSharedPtr< STreeView< TSharedPtr<FGraphActionNode> > > TreeView, bool bRecursive/*= true*/)
{
	if (Children.Num() > 0)
	{
		TreeView->SetItemExpansion(this->AsShared(), /*ShouldExpandItem =*/true);
		for (TSharedPtr<FGraphActionNode>& ChildNode : Children)
		{
			if (bRecursive)
			{
				ChildNode->ExpandAllChildren(TreeView);
			}
			else
			{
				TreeView->SetItemExpansion(ChildNode, /*ShouldExpandItem =*/true);
			}
		}
	}
}

//------------------------------------------------------------------------------
void FGraphActionNode::ClearChildren()
{
	Children.Empty();
	CategoryNodes.Empty();
	ChildGroupings.Empty();
	ChildSections.Empty();
}

//------------------------------------------------------------------------------
bool FGraphActionNode::IsRootNode() const
{
	return (!IsActionNode() && !IsCategoryNode() && !ParentNode.IsValid());
}

//------------------------------------------------------------------------------
bool FGraphActionNode::IsSectionHeadingNode() const
{
	return (!IsActionNode() && !IsCategoryNode() && !IsRootNode() && (SectionID != INVALID_SECTION_ID));
}

//------------------------------------------------------------------------------
bool FGraphActionNode::IsCategoryNode() const
{
	return (!IsActionNode() && !DisplayText.IsEmpty());
}

//------------------------------------------------------------------------------
bool FGraphActionNode::IsActionNode() const
{
	return Actions.Num() != 0;
}

//------------------------------------------------------------------------------
bool FGraphActionNode::IsGroupDividerNode() const
{
	return (!IsActionNode() && !IsCategoryNode() && !IsRootNode() && (SectionID == INVALID_SECTION_ID));
}

//------------------------------------------------------------------------------
bool FGraphActionNode::IsSeparator() const
{
	return IsSectionHeadingNode() || IsGroupDividerNode();
}

//------------------------------------------------------------------------------
FText const& FGraphActionNode::GetDisplayName() const
{
	return DisplayText;
}

//------------------------------------------------------------------------------
FText FGraphActionNode::GetCategoryPath() const
{
	FText CategoryPath;
	if (IsCategoryNode())
	{
		CategoryPath = DisplayText;
	}

	TWeakPtr<FGraphActionNode> AncestorNode = ParentNode;
	while (AncestorNode.IsValid())
	{
		const FText& AncestorDisplayText = AncestorNode.Pin()->DisplayText;

		if( !AncestorDisplayText.IsEmpty() )
		{
			CategoryPath = FText::Format(FText::FromString(TEXT("{0}|{1}")), AncestorDisplayText, CategoryPath);
		}
		AncestorNode = AncestorNode.Pin()->GetParentNode();
	}
	return CategoryPath;
}

//------------------------------------------------------------------------------
bool FGraphActionNode::HasValidAction() const
{
	return GetPrimaryAction().IsValid();
}

//------------------------------------------------------------------------------
TSharedPtr<FEdGraphSchemaAction> FGraphActionNode::GetPrimaryAction() const
{
	for (const TSharedPtr<FEdGraphSchemaAction>& NodeAction : Actions)
	{
		if (NodeAction.IsValid())
		{
			return NodeAction;
		}
	}
	return TSharedPtr<FEdGraphSchemaAction>();
}

//------------------------------------------------------------------------------
bool FGraphActionNode::BroadcastRenameRequest()
{
	if (RenameRequestEvent.IsBound())
	{
		RenameRequestEvent.Execute();
		bPendingRenameRequest = false;
	}
	else
	{
		bPendingRenameRequest = true;
	}
	return bPendingRenameRequest;
}

//------------------------------------------------------------------------------
bool FGraphActionNode::IsRenameRequestPending() const
{
	return bPendingRenameRequest;
}

//------------------------------------------------------------------------------
TSharedPtr<FGraphActionNode> FGraphActionNode::NewSectionHeadingNode(TWeakPtr<FGraphActionNode> Parent, int32 Grouping, int32 SectionID)
{
	checkSlow(SectionID != INVALID_SECTION_ID);

	FGraphActionNode* SectionNode = new FGraphActionNode(Grouping, SectionID);
	SectionNode->ParentNode = Parent;
	checkSlow(Parent.IsValid());

	return MakeShareable(SectionNode);
}

//------------------------------------------------------------------------------
TSharedPtr<FGraphActionNode> FGraphActionNode::NewCategoryNode(FString const& Category, int32 Grouping, int32 SectionID)
{
	FGraphActionNode* CategoryNode = new FGraphActionNode(Grouping, SectionID);
	CategoryNode->DisplayText = FText::FromString(Category);

	return MakeShareable(CategoryNode);
}

//------------------------------------------------------------------------------
TSharedPtr<FGraphActionNode> FGraphActionNode::NewActionNode(TArray< TSharedPtr<FEdGraphSchemaAction> > const& ActionList)
{
	int32 Grouping  = FGraphActionNodeImpl::DEFAULT_GROUPING;
	int32 SectionID = INVALID_SECTION_ID;

	for (TSharedPtr<FEdGraphSchemaAction> const& Action : ActionList)
	{
		Grouping = FMath::Max(Grouping, Action->GetGrouping());
		if (SectionID == INVALID_SECTION_ID)
		{
			// take the first non-zero section ID
			SectionID = Action->GetSectionID();
		}
	}

	FGraphActionNode* ActionNode = new FGraphActionNode(ActionList, Grouping, SectionID);
	TSharedPtr<FEdGraphSchemaAction> PrimeAction = ActionNode->GetPrimaryAction();
	checkSlow(PrimeAction.IsValid());
	ActionNode->DisplayText = PrimeAction->GetMenuDescription();

	return MakeShareable(ActionNode);
}

//------------------------------------------------------------------------------
TSharedPtr<FGraphActionNode> FGraphActionNode::NewGroupDividerNode(TWeakPtr<FGraphActionNode> Parent, int32 Grouping)
{
	FGraphActionNode* DividerNode = new FGraphActionNode(Grouping, INVALID_SECTION_ID);
	DividerNode->ParentNode = Parent;
	checkSlow(Parent.IsValid());

	return MakeShareable(DividerNode);
}

//------------------------------------------------------------------------------
void FGraphActionNode::AddChildRecursively(const TArray<FString>& CategoryStack, int32 Idx, TSharedPtr<FGraphActionNode> NodeToAdd)
{
	if (NodeToAdd->SectionID != INVALID_SECTION_ID)
	{
		TSharedPtr<FGraphActionNode> FoundSectionNode;
		for ( TSharedPtr<FGraphActionNode> const& ChildNode : Children )
		{
			if ( NodeToAdd->SectionID == ChildNode->SectionID && ChildNode->IsSectionHeadingNode() )
			{
				FoundSectionNode = ChildNode;
				break;
			}
		}

		if ( FoundSectionNode.IsValid() )
		{
			FoundSectionNode->AddChildRecursively(CategoryStack, Idx, NodeToAdd);
			return;
		}
	}

	if ( Idx < CategoryStack.Num() )
	{
		const FString& CategorySection = CategoryStack[Idx];
		++Idx;

		// make sure we don't already have a child that this can nest under
		TSharedPtr<FGraphActionNode> ExistingNode = FindMatchingParent(CategorySection, NodeToAdd);
		if ( ExistingNode.IsValid() )
		{
			ExistingNode->AddChildRecursively(CategoryStack, Idx, NodeToAdd);
		}
		else
		{
			TSharedPtr<FGraphActionNode> CategoryNode = NewCategoryNode(CategorySection, NodeToAdd->Grouping, NodeToAdd->SectionID);
			InsertChild(CategoryNode);
			CategoryNode->AddChildRecursively(CategoryStack, Idx, NodeToAdd);
		}
	}
	else
	{
		InsertChild(NodeToAdd);
	}
}

//------------------------------------------------------------------------------
TSharedPtr<FGraphActionNode> FGraphActionNode::FindMatchingParent(FString const& ParentName, TSharedPtr<FGraphActionNode> NodeToAdd)
{
	TSharedPtr<FGraphActionNode> FoundCategoryNode;

	// for the "MyBlueprint" tab, sub-graph actions can be nested under graph
	// actions (meaning that action node can have children).
	bool const bCanNestUnderActionNodes = NodeToAdd->IsActionNode() && NodeToAdd->GetPrimaryAction()->IsParentable();

	if (bCanNestUnderActionNodes)
	{
		// slow path, not commonly used:
		for (TSharedPtr<FGraphActionNode> const& ChildNode : Children)
		{
			if (ChildNode->IsCategoryNode())
			{
				if ((NodeToAdd->SectionID == ChildNode->SectionID) &&
					(ParentName == ChildNode->DisplayText.ToString()))
				{
					FoundCategoryNode = ChildNode;
					break;
				}
			}
			else if (bCanNestUnderActionNodes && ChildNode->IsActionNode())
			{
				// make the action's name into a display name, all categories are 
				// set as such (to ensure that the action name best matches the 
				// category ParentName)
				FString ChildNodeName = FName::NameToDisplayString(ChildNode->DisplayText.ToString(), /*bIsBool =*/false);

				// @TODO: should we be matching section/grouping as well?
				if (ChildNodeName == ParentName)
				{
					FoundCategoryNode = ChildNode;
					break;
				}
			}
		}
	}
	else
	{
		// fast path, just look up in category map:
		TSharedPtr<FGraphActionNode>* PotentialCategoryNode = CategoryNodes.Find(ParentName);
		if (PotentialCategoryNode && (*PotentialCategoryNode)->SectionID == NodeToAdd->SectionID)
		{
			FoundCategoryNode = *PotentialCategoryNode;
		}
	}

	return FoundCategoryNode;
}

//------------------------------------------------------------------------------
void FGraphActionNode::InsertChild(TSharedPtr<FGraphActionNode> NodeToAdd)
{
	ensure(!NodeToAdd->IsRootNode());
	//ensure(!IsSeparator());

	NodeToAdd->ParentNode = this->AsShared();

	if (NodeToAdd->SectionID != INVALID_SECTION_ID)
	{
		// don't need a section heading if the parent is under the same section
		bool const bAddSectionHeading = (NodeToAdd->SectionID != SectionID) && 
			// make sure we already haven't already added a heading for this section
			!ChildSections.Contains(NodeToAdd->SectionID) &&
			// if this node also has a category, use that over a section heading
			(!NodeToAdd->IsActionNode() || NodeToAdd->GetPrimaryAction()->GetCategory().IsEmpty());

		if (bAddSectionHeading)
		{
			ChildSections.Add(NodeToAdd->SectionID); // to avoid recursion, add before we insert
			//InsertChild(NewSectionHeadingNode(NodeToAdd->ParentNode, NodeToAdd->Grouping, NodeToAdd->SectionID));

			TSharedPtr<FGraphActionNode> NewSection = NewSectionHeadingNode(NodeToAdd->ParentNode, NodeToAdd->Grouping, NodeToAdd->SectionID);
			InsertChild(NewSection);

			NodeToAdd->InsertOrder = NewSection->Children.Num();
			NewSection->Children.Add(NodeToAdd);
			if (NodeToAdd->IsCategoryNode())
			{
				CategoryNodes.Add(NodeToAdd->DisplayText.ToString(), NodeToAdd);
			}
			return;
		}
	}
	// we don't use group-dividers inside of sections (we use groups to more to
	// hardcode the order), but if this isn't in a section...
	else if (!ChildGroupings.Contains(NodeToAdd->Grouping))
	{
		// don't need a divider if this is the first group
		if (ChildGroupings.Num() > 0)
		{
			int32 LowestGrouping = MAX_int32;
			for (int32 Group : ChildGroupings)
			{
				LowestGrouping = FMath::Min(LowestGrouping, Group);
			}
			// dividers come at the end of a menu group, so it would be 
			// undesirable to add it for NodeToAdd->Grouping if that group is 
			// lower than all the others (the lowest group should not have a 
			// divider associated with it)
			int32 DividerGrouping = FMath::Max(LowestGrouping, NodeToAdd->Grouping);

			ChildGroupings.Add(NodeToAdd->Grouping); // to avoid recursion, add before we insert
			InsertChild(NewGroupDividerNode(NodeToAdd->ParentNode, DividerGrouping));
		}
		else
		{
			ChildGroupings.Add(NodeToAdd->Grouping);
		}
	}

	NodeToAdd->InsertOrder = Children.Num();
	Children.Add(NodeToAdd);
	if (NodeToAdd->IsCategoryNode())
	{
		CategoryNodes.Add(NodeToAdd->DisplayText.ToString(), NodeToAdd);
	}
}






