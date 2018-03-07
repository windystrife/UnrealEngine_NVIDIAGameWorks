// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GraphDiffControl.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraph.h"

#define LOCTEXT_NAMESPACE "GraphDiffControl"

//If we are collecting all Diff results, keep going. If we just want to know if there *is* any diffs, we can early out
#define KEEP_GOING_IF_RESULTS() 	bHasResult = true;	\
if(!Results.CanStoreResults())		{ break; }


/*******************************************************************************
* Static helper functions
*******************************************************************************/

/** Diff result when a node was added to the graph */
static void DiffR_NodeAdded( const FGraphDiffControl::FNodeDiffContext& DiffContext, FDiffResults& Results, UEdGraphNode* Node )
{
	FDiffSingleResult Diff;
	Diff.Diff = EDiffType::NODE_ADDED;
	Diff.Node1 = Node;

	// Only bother setting up the display data if we're storing the result
	if(Results.CanStoreResults())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("NodeType"), DiffContext.NodeTypeDisplayName);
		Args.Add(TEXT("NodeTitle"), Node->GetNodeTitle(ENodeTitleType::ListView));
		Diff.ToolTip =  FText::Format(LOCTEXT("DIF_AddNode", "Added {NodeType} '{NodeTitle}'"), Args);
		Diff.DisplayString = Diff.ToolTip;
		Diff.DisplayColor = FLinearColor(0.3f,1.0f,0.4f);
	}
	
	Results.Add(Diff);
}

/** Diff result when a node was removed from the graph */
static void DiffR_NodeRemoved( const FGraphDiffControl::FNodeDiffContext& DiffContext, FDiffResults& Results, UEdGraphNode* Node )
{
	FDiffSingleResult Diff;
	Diff.Diff = EDiffType::NODE_REMOVED;
	Diff.Node1 = Node;

	// Only bother setting up the display data if we're storing the result
	if(Results.CanStoreResults())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("NodeType"), DiffContext.NodeTypeDisplayName);
		Args.Add(TEXT("NodeTitle"), Node->GetNodeTitle(ENodeTitleType::ListView));
		Diff.ToolTip =  FText::Format(LOCTEXT("DIF_RemoveNode", "Removed {NodeType} '{NodeTitle}'"), Args);
		Diff.DisplayString = Diff.ToolTip;
		Diff.DisplayColor = FLinearColor(1.f,0.4f,0.4f);
	}

	Results.Add(Diff);
}

/** Diff result when a node comment was changed */
static void DiffR_NodeCommentChanged(const FGraphDiffControl::FNodeDiffContext& DiffContext, FDiffResults& Results, class UEdGraphNode* Node, class UEdGraphNode* Node2)
{
	FDiffSingleResult Diff;
	Diff.Diff = EDiffType::NODE_COMMENT;
	Diff.Node1 = Node;
	Diff.Node2 = Node2;

	// Only bother setting up the display data if we're storing the result
	if(Results.CanStoreResults())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("NodeType"), DiffContext.NodeTypeDisplayName);
		Args.Add(TEXT("NodeTitle"), Node->GetNodeTitle(ENodeTitleType::ListView));
		Diff.ToolTip =  FText::Format(LOCTEXT("DIF_CommentModified", "Comment Modified {NodeType} '{NodeTitle}'"), Args);
		Diff.DisplayString = Diff.ToolTip;
		Diff.DisplayColor = FLinearColor(0.25f,0.4f,0.5f);
	}

	Results.Add(Diff);
}

/** Diff result when a node was moved on the graph */
static void DiffR_NodeMoved(const FGraphDiffControl::FNodeDiffContext& DiffContext, FDiffResults& Results, UEdGraphNode* Node, class UEdGraphNode* OtherNode)
{
	FDiffSingleResult Diff;
	Diff.Diff = EDiffType::NODE_MOVED;
	Diff.Node1 = Node;
	Diff.Node2 = OtherNode;

	// Only bother setting up the display data if we're storing the result
	if(Results.CanStoreResults())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("NodeType"), DiffContext.NodeTypeDisplayName);
		Args.Add(TEXT("NodeTitle"), Node->GetNodeTitle(ENodeTitleType::ListView));
		Diff.ToolTip = FText::Format(LOCTEXT("DIF_MoveNode", "Moved {NodeType} '{NodeTitle}'"), Args);
		Diff.DisplayString = Diff.ToolTip;
		Diff.DisplayColor = FLinearColor(0.9f, 0.84f, 0.43f);
	}

	Results.Add(Diff);
}

/** Diff result when a pin type was changed */
static void DiffR_PinTypeChanged(FDiffResults& Results, class UEdGraphPin* Pin2, class UEdGraphPin* Pin1)
{
	FEdGraphPinType Type1 = Pin1->PinType;
	FEdGraphPinType Type2 = Pin2->PinType;

	FDiffSingleResult Diff;

	const UObject* T1Obj = Type1.PinSubCategoryObject.Get();
	const UObject* T2Obj = Type2.PinSubCategoryObject.Get();

	if(Type1.PinCategory != Type2.PinCategory)
	{
		Diff.Diff = EDiffType::PIN_TYPE_CATEGORY;

		// Only bother setting up the display data if we're storing the result
		if(Results.CanStoreResults())
		{
			Diff.ToolTip = FText::Format(LOCTEXT("DIF_PinCategoryToolTipFmt", "Pin '{0}' Category was '{1}', but is now '{2}"), FText::FromString(Pin2->PinName), FText::FromString(Pin1->PinType.PinCategory), FText::FromString(Pin2->PinType.PinCategory));
			Diff.DisplayColor = FLinearColor(0.15f,0.53f,0.15f);
			Diff.DisplayString = FText::Format(LOCTEXT("DIF_PinCategoryFmt", "Pin Category '{0}' ['{1}' -> '{2}']"), FText::FromString(Pin2->PinName), FText::FromString(Pin1->PinType.PinCategory), FText::FromString(Pin2->PinType.PinCategory));
		}
	}
	else if(Type1.PinSubCategory != Type2.PinSubCategory)
	{
		Diff.Diff = EDiffType::PIN_TYPE_SUBCATEGORY;

		// Only bother setting up the display data if we're storing the result
		if(Results.CanStoreResults())
		{
			Diff.ToolTip = FText::Format(LOCTEXT("DIF_PinSubCategoryToolTipFmt", "Pin '{0}' SubCategory was '{1}', but is now '{2}"), FText::FromString(Pin2->PinName), FText::FromString(Pin1->PinType.PinSubCategory), FText::FromString(Pin2->PinType.PinSubCategory));
			Diff.DisplayColor = FLinearColor(0.45f,0.53f,0.65f);
			Diff.DisplayString = FText::Format(LOCTEXT("DIF_PinSubCategoryFmt", "Pin SubCategory '{0}'  ['{1}' -> '{2}']"), FText::FromString(Pin2->PinName), FText::FromString(Pin1->PinType.PinSubCategory), FText::FromString(Pin2->PinType.PinSubCategory));
		}
	}
	else if(T1Obj != T2Obj && (T1Obj && T2Obj ) &&
		(T1Obj->GetFName() != T2Obj->GetFName()))
	{
		Diff.Diff = EDiffType::PIN_TYPE_SUBCATEGORY_OBJECT;

		// Only bother setting up the display data if we're storing the result
		if(Results.CanStoreResults())
		{
			FString Obj1 = T1Obj->GetFName().ToString();
			FString Obj2 = T2Obj->GetFName().ToString();

			Diff.ToolTip = FText::Format(LOCTEXT("DIF_PinSubCategorObjToolTipFmt", "Pin '{0}' was SubCategoryObject '{1}', but is now '{2}"), FText::FromString(Pin2->PinName), FText::FromString(Obj1), FText::FromString(Obj2));
			Diff.DisplayColor = FLinearColor(0.45f,0.13f,0.25f);
			Diff.DisplayString = FText::Format(LOCTEXT("DIF_PinSubCategoryObjFmt", "Pin SubCategoryObject '{0}' ['{1}' -> '{2}']"), FText::FromString(Pin2->PinName), FText::FromString(Obj1), FText::FromString(Obj2));
		}
	}
	else if(Type1.ContainerType != Type2.ContainerType)
	{
		// TODO: Make the messaging correct about the nature of the diff
		Diff.Diff = EDiffType::PIN_TYPE_IS_ARRAY;

		// Only bother setting up the display data if we're storing the result
		if(Results.CanStoreResults())
		{
			FText IsArray1 = Pin1->PinType.IsArray() ? LOCTEXT("true", "true") : LOCTEXT("false", "false");
			FText IsArray2 = Pin2->PinType.IsArray() ? LOCTEXT("true", "true") : LOCTEXT("false", "false");

			Diff.ToolTip = FText::Format(LOCTEXT("DIF_PinIsArrayToolTipFmt", "PinType IsArray for '{0}' modified. Was '{1}', but is now '{2}"), FText::FromString(Pin2->PinName), IsArray1, IsArray2);
			Diff.DisplayString = FText::Format(LOCTEXT("DIF_PinIsArrayFmt", "Pin IsArray '{0}' ['{1}' -> '{2}']"), FText::FromString(Pin2->PinName), IsArray1, IsArray2);
			Diff.DisplayColor = FLinearColor(0.45f,0.33f,0.35f);
		}
	}
	else if(Type1.bIsReference != Type2.bIsReference)
	{
		Diff.Diff = EDiffType::PIN_TYPE_IS_REF;

		// Only bother setting up the display data if we're storing the result
		if(Results.CanStoreResults())
		{
			FText IsRef1 = Pin1->PinType.bIsReference ? LOCTEXT("true", "true") : LOCTEXT("false", "false");
			FText IsRef2 = Pin2->PinType.bIsReference ? LOCTEXT("true", "true") : LOCTEXT("false", "false");

			Diff.ToolTip = FText::Format(LOCTEXT("DIF_PinIsRefToolTipFmt", "PinType IsReference for '{0}' modified. Was '{1}', but is now '{2}"), FText::FromString(Pin2->PinName), IsRef1, IsRef2);
			Diff.DisplayColor = FLinearColor(0.25f,0.43f,0.35f);
			Diff.DisplayString = FText::Format(LOCTEXT("DIF_PinIsRefFmt", "Pin IsReference '{0}' ['{1}' -> '{2}']"), FText::FromString(Pin2->PinName), IsRef1, IsRef2);
		}
	}

	Diff.Pin1 = Pin1;
	Diff.Pin2 = Pin2;
	Results.Add(Diff);
}

/** Diff result when the # of links to a pin was changed */
static void DiffR_PinLinkCountChanged(FDiffResults& Results, class UEdGraphPin* Pin2, class UEdGraphPin* Pin1)
{
	FDiffSingleResult Diff;
	Diff.Diff = Pin2->LinkedTo.Num() > Pin1->LinkedTo.Num()  ?  EDiffType::PIN_LINKEDTO_NUM_INC : EDiffType::PIN_LINKEDTO_NUM_DEC;
	Diff.Pin2 = Pin2;
	Diff.Pin1 = Pin1;

	// Only bother setting up the display data if we're storing the result
	if(Results.CanStoreResults())
	{
		if(Diff.Diff == EDiffType::PIN_LINKEDTO_NUM_INC)
		{
			Diff.ToolTip = FText::Format(LOCTEXT("DIF_PinLinkCountIncToolTipFmt", "Pin '{0}' has more links (was {1} now {2})"), FText::FromString(Pin1->PinName), FText::AsNumber(Pin1->LinkedTo.Num()), FText::AsNumber(Pin2->LinkedTo.Num()));
			Diff.DisplayColor = FLinearColor(0.5f,0.3f,0.85f);
			Diff.DisplayString = FText::Format(LOCTEXT("DIF_PinLinkCountIncFmt", "Added Link to '{0}'"), FText::FromString(Pin1->PinName));
		}
		else
		{
			Diff.ToolTip = FText::Format(LOCTEXT("DIF_PinLinkCountDecToolTipFmt", "Pin '{0}' has fewer links (was {1} now {2})"), FText::FromString(Pin1->PinName), FText::AsNumber(Pin1->LinkedTo.Num()), FText::AsNumber(Pin2->LinkedTo.Num()));
			Diff.DisplayColor = FLinearColor(0.5f,0.3f,0.85f);
			Diff.DisplayString = FText::Format(LOCTEXT("DIF_PinLinkCountDecFmt", "Removed Link to '{0}'"), FText::FromString(Pin1->PinName));
		}
	}

	Results.Add(Diff);
}

/** Diff result when a pin to relinked to a different node */
static void DiffR_LinkedToNode(FDiffResults& Results, class UEdGraphPin* Pin1, class UEdGraphPin* Pin2,  class UEdGraphNode* Node1, class UEdGraphNode* Node2)
{
	FDiffSingleResult Diff;
	Diff.Diff = EDiffType::PIN_LINKEDTO_NODE;
	Diff.Pin2 = Pin2;
	Diff.Pin1 = Pin1;
	Diff.Node1 = Node1;
	Diff.Node2 = Node2;

	// Only bother setting up the display data if we're storing the result
	if(Results.CanStoreResults())
	{
		FText Node1Name = Node1->GetNodeTitle(ENodeTitleType::ListView);
		FText Node2Name = Node2->GetNodeTitle(ENodeTitleType::ListView);

		FFormatNamedArguments Args;
		Args.Add(TEXT("PinNameForNode1"), FText::FromString(Pin1->PinName));
		Args.Add(TEXT("NodeName1"), Node1Name);
		Args.Add(TEXT("NodeName2"), Node2Name);
		Diff.ToolTip = FText::Format(LOCTEXT("DIF_PinLinkMovedToolTip", "Pin '{PinNameForNode1}' was linked to Node '{NodeName1}', but is now linked to Node '{NodeName2}'"), Args);

		Diff.DisplayColor = FLinearColor(0.85f,0.71f,0.25f);
		Diff.DisplayString = FText::Format(LOCTEXT("DIF_PinLinkMoved", "Link Moved  '{PinNameForNode1}' ['{NodeName1}' -> '{NodeName2}']"), Args);
	}

	Results.Add(Diff);
}

/** Diff result when a pin default value was changed, and is in use*/
static void DiffR_PinDefaultValueChanged(FDiffResults& Results, class UEdGraphPin* Pin2, class UEdGraphPin* Pin1)
{
	FDiffSingleResult Diff;
	Diff.Diff = EDiffType::PIN_DEFAULT_VALUE;
	Diff.Pin1 = Pin1;
	Diff.Pin2 = Pin2;

	// Only bother setting up the display data if we're storing the result
	if(Results.CanStoreResults())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("PinNameForValue1"), FText::FromString(Pin2->PinName));
		Args.Add(TEXT("PinValue1"), FText::FromString(Pin1->GetDefaultAsString()));
		Args.Add(TEXT("PinValue2"), FText::FromString(Pin2->GetDefaultAsString()));
		Diff.ToolTip = FText::Format(LOCTEXT("DIF_PinDefaultValueToolTip", "Pin '{PinNameForValue1}' Default Value was '{PinValue1}', but is now '{PinValue2}"), Args);
		Diff.DisplayColor = FLinearColor(0.665f,0.13f,0.455f);
		Diff.DisplayString = FText::Format(LOCTEXT("DIF_PinDefaultValue", "Pin Default '{PinNameForValue1}' '{PinValue1}' -> '{PinValue2}']"), Args);
	}

	Results.Add(Diff);
}

/** 
 * Diff result when pin count is not the same.
 *
 * @param	Result	Struct to receive result.
 * @param	Node	Node 1 to which the pins relate.
 * @param	Node2	Node 2 to which the pins relate.
 * @param	Pins1	Array of relevant pins on Node 1.
 * @param	Pins2	Array of relevant pins on Node 2.
 */
static void DiffR_NodePinCount(FDiffResults& Results, class UEdGraphNode* Node, class UEdGraphNode* Node2, const TArray<class UEdGraphPin*> &Pins1, const TArray<class UEdGraphPin*> &Pins2)
{
	FText NodeName = Node->GetNodeTitle(ENodeTitleType::ListView);
	int32 OriginalCount = Pins2.Num();
	int32 NewCount = Pins1.Num();
	FDiffSingleResult Diff;
	Diff.Diff = EDiffType::NODE_PIN_COUNT;
	Diff.Node1 = Node;
	Diff.Node2 = Node2;

	// Only bother setting up the display data if we're storing the result
	if(Results.CanStoreResults())
	{
		FFormatNamedArguments Args;
		Args.Add(TEXT("NodeName"), NodeName);
		Args.Add(TEXT("OriginalCount"), OriginalCount);
		Args.Add(TEXT("NewCount"), NewCount);
		Diff.DisplayColor = FLinearColor(0.45f,0.4f,0.4f);

		struct FMatchName
		{
			FMatchName(const FString& InPinName)
				: PinName(InPinName)
			{
			}

			const FString& PinName;

			bool operator()(const UEdGraphPin* Entry )
			{
				return PinName == Entry->PinName;
			}
		};

		FText ListOfPins;
		TArray< FText > RemovedPins;
		for (auto OldPin : Pins1)
		{
			const UEdGraphPin* const* FoundPin = Pins2.FindByPredicate(FMatchName(OldPin->PinName));
			if (FoundPin == nullptr)
			{
				RemovedPins.Add(OldPin->GetDisplayName());
			}
		}

		TArray< FText > AddedPins;
		for (auto OldPin : Pins2)
		{
			const UEdGraphPin* const* FoundPin = Pins1.FindByPredicate(FMatchName(OldPin->PinName));
			if (FoundPin == nullptr)
			{
				AddedPins.Add(OldPin->GetDisplayName());
			}
		}

		if (RemovedPins.Num() > 0 && AddedPins.Num() > 0)
		{
			Diff.DisplayString = FText::Format(LOCTEXT("DIF_NodePinsAddedAndRemoved", "Added and removed Pins from '{NodeName}'"), Args);
		}
		else if (AddedPins.Num() > 0)
		{
			if (AddedPins.Num() == 1)
			{
				Diff.DisplayString = FText::Format(LOCTEXT("DIF_NodePinCountIncreased", "Added Pin to '{NodeName}'"), Args);
			}
			else
			{
				Diff.DisplayString = FText::Format(LOCTEXT("DIF_NodePinCountIncreasedSeveral", "Added Pins to '{NodeName}'"), Args);
			}
		}
		else if (RemovedPins.Num() > 0)
		{
			if (RemovedPins.Num() == 1)
			{
				Diff.DisplayString = FText::Format(LOCTEXT("DIF_NodePinCountDecreased", "Removed Pin from '{NodeName}'"), Args);
			}
			else
			{
				Diff.DisplayString = FText::Format(LOCTEXT("DIF_NodePinCountDecreasedSeveral", "Removed Pins from '{NodeName}'"), Args);
			}
		}

		FTextBuilder Builder;
		Builder.AppendLine(FText::Format(LOCTEXT("DIF_NodePinCountChangedToolTip", "Node '{NodeName}' had {OriginalCount} Pins, now has {NewCount} Pins"), Args));
		if (AddedPins.Num() > 0)
		{
			Builder.AppendLine(LOCTEXT("DIF_PinsAddedList", "Pins Added:"));
			for (const auto& Added : AddedPins)
			{
				Builder.AppendLine(Added);
			}
		}

		if (RemovedPins.Num() > 0)
		{
			Builder.AppendLine(LOCTEXT("DIF_PinsRemovedList", "Pins Removed:"));
			for (const auto& Removed : RemovedPins)
			{
				Builder.AppendLine(Removed);
			}
		}
		Diff.ToolTip = Builder.ToText();
	}

	Results.Add(Diff);
}

/** 
 * Populate an array one of pins from another disregarding irrelevant ones (EG invisible). 
 * 
 * @param	InPins			Pins
 * @param	OutRelevanPins	Output of Relevant pins
 */
static void BuildArrayOfRelevantPins(const TArray<class UEdGraphPin*>& InPins, TArray< UEdGraphPin* >& OutRelevantPins)
{
	for(int32 i = 0;i<InPins.Num();++i)
	{
		UEdGraphPin* EachPin = InPins[i];
		if( EachPin != NULL )
		{
			if( EachPin->bHidden == false )
			{
				OutRelevantPins.Add( EachPin );
			}			
		}
	}
}

static bool IsPinTypeDifferent(const FEdGraphPinType& T1, const FEdGraphPinType& T2) 
{
	bool bIsDifferent =  (T1.PinCategory != T2.PinCategory) 
		|| (T1.PinSubCategory != T2.PinSubCategory) 
		|| (T1.ContainerType != T2.ContainerType) 
		|| (T1.bIsReference != T2.bIsReference);

	const UObject* T1Obj = T1.PinSubCategoryObject.Get();
	const UObject* T2Obj = T2.PinSubCategoryObject.Get();
	//TODO: fix, this code makes no sense
	if((T1Obj != T2Obj) && (T1Obj && T2Obj) && (T1Obj->GetFName() != T2Obj->GetFName())) 
	{
		bIsDifferent |= T1Obj->GetFName() == T2Obj->GetFName();
	}
	return bIsDifferent;
}

/** Find Pin in array that matches pin */
static class UEdGraphPin* FindOtherLink(TArray<class UEdGraphPin*>& Links2, int32 OriginalIndex, class UEdGraphPin* PinToFind)
{
	//sometimes the order of the pins is different between revisions, although the pins themselves are unchanged, so we have to look at all of them
	UEdGraphNode* Node1 = PinToFind->GetOwningNode();
	for(auto It(Links2.CreateIterator());It;++It)
	{
		UEdGraphPin* Other = *It;
		UEdGraphNode* Node2 = Other->GetOwningNode();
		if(FGraphDiffControl::IsNodeMatch(Node1, Node2))
		{
			return Other;
		}
	}
	return Links2[OriginalIndex];
}

/** Determine if the LinkedTo pins are the same */
static bool LinkedToDifferent(class UEdGraphPin* OriginalPin1, class UEdGraphPin* OriginalPin2, const TArray<class UEdGraphPin*>& Links1, TArray<class UEdGraphPin*>& Links2, FDiffResults& Results)
{
	const int32 Size = Links1.Num();
	bool bHasResult = false;
	for(int32 i = 0;i<Size;++i)
	{
		UEdGraphPin* Pin1 = Links1[i];
		UEdGraphPin* Pin2 = FindOtherLink(Links2, i, Pin1);

		UEdGraphNode* Node1 = Pin1->GetOwningNode();
		UEdGraphNode* Node2 = Pin2->GetOwningNode();
		if(!FGraphDiffControl::IsNodeMatch(Node1, Node2))
		{
			DiffR_LinkedToNode(Results, OriginalPin1, OriginalPin2, Node1, Node2);
			KEEP_GOING_IF_RESULTS()
		}
	}
	return bHasResult;
}

/** 
 * Determine of two Arrays of Pins are different.
 *
 * @param	Pins1	First set of pins to compare.
 * @param	Pins2	Second set of pins to compare.
 * @param	Results Difference results.
 * 
 * returns true if any pins are different and populates the Results array
 */
static bool ArePinsDifferent(const TArray<class UEdGraphPin*>& Pins1, TArray<class UEdGraphPin*>& Pins2, FDiffResults& Results)
{
	const int32 Size = Pins1.Num();
	bool bHasResult = false;
	for(int32 i = 0;i<Size;++i)
	{
		UEdGraphPin* Pin1 = Pins1[i];
		UEdGraphPin* Pin2 = Pins2[i];

		if(IsPinTypeDifferent(Pin1->PinType, Pin2->PinType))
		{
			DiffR_PinTypeChanged(Results, Pin2, Pin1);
			KEEP_GOING_IF_RESULTS()
		}
		if(Pin1->LinkedTo.Num() != Pin2->LinkedTo.Num())
		{
			DiffR_PinLinkCountChanged(Results, Pin2, Pin1);
			KEEP_GOING_IF_RESULTS()
		}
		else if(LinkedToDifferent(Pin1, Pin2,Pin1->LinkedTo, Pin2->LinkedTo, Results))
		{
			KEEP_GOING_IF_RESULTS()
		}

		if(Pin2->LinkedTo.Num() == 0 && (Pin2->GetDefaultAsString() != Pin1->GetDefaultAsString()))
		{
			DiffR_PinDefaultValueChanged(Results, Pin2, Pin1); //note: some issues with how floating point is stored as string format(0.0 vs 0.00) can cause false diffs
			KEEP_GOING_IF_RESULTS()
		}
	}
	return bHasResult;
}

/*******************************************************************************
* FGraphDiffControl::FNodeMatch
*******************************************************************************/

bool FGraphDiffControl::FNodeMatch::IsValid() const
{
	return ((NewNode != NULL) && (OldNode != NULL));
}

bool FGraphDiffControl::FNodeMatch::Diff(const FNodeDiffContext& DiffContext, TArray<FDiffSingleResult>* OptionalDiffsArray /* = NULL*/) const
{
	FDiffResults DiffsOut(OptionalDiffsArray);
	return Diff(DiffContext, DiffsOut);
}

bool FGraphDiffControl::FNodeMatch::Diff(const FNodeDiffContext& DiffContext, FDiffResults& DiffsOut) const
{
	bool bIsDifferent = false;

	if (IsValid())
	{
		//has comment changed?
		if((DiffContext.DiffFlags & EDiffFlags::NodeComment) && NewNode->NodeComment != OldNode->NodeComment)
		{
			DiffR_NodeCommentChanged(DiffContext, DiffsOut, NewNode, OldNode);
			bIsDifferent = true;
		}

		//has it moved?
		if( (DiffContext.DiffFlags & EDiffFlags::NodeMovement) && ( (NewNode->NodePosX != OldNode->NodePosX) || (NewNode->NodePosY != OldNode->NodePosY) ) )
		{
			//same node, different position--
			DiffR_NodeMoved(DiffContext, DiffsOut, NewNode, OldNode);
			bIsDifferent = true;
		}

		if(DiffContext.DiffFlags & EDiffFlags::NodePins)
		{
			// Build arrays of pins that we care about
			TArray< UEdGraphPin* > OldRelevantPins;
			TArray< UEdGraphPin* > RelevantPins;
			BuildArrayOfRelevantPins(OldNode->Pins, OldRelevantPins);
			BuildArrayOfRelevantPins(NewNode->Pins, RelevantPins);

			if(OldRelevantPins.Num() == RelevantPins.Num())
			{
				//checks contents of pins
				bIsDifferent |= ArePinsDifferent(OldRelevantPins, RelevantPins, DiffsOut);
			}
			else//# of pins changed
			{
				DiffR_NodePinCount(DiffsOut, NewNode, OldNode, RelevantPins, OldRelevantPins);
				bIsDifferent = true;
			}
		}

		//Find internal node diffs; skip this if we don't need the result data
		if((DiffContext.DiffFlags & EDiffFlags::NodeSpecificDiffs) && (!bIsDifferent || DiffsOut.CanStoreResults()))
		{
			OldNode->FindDiffs(NewNode, DiffsOut);
			bIsDifferent |= DiffsOut.HasFoundDiffs();
		}
	}
	else if(DiffContext.DiffFlags & EDiffFlags::NodeExistance)
	// one of the nodes is NULL
	{
		bIsDifferent = true;
		switch (DiffContext.DiffMode)
		{
		case EDiffMode::Additive:
			DiffR_NodeAdded(DiffContext, DiffsOut, NewNode);
			break;

		case EDiffMode::Subtractive:
			DiffR_NodeRemoved(DiffContext, DiffsOut, NewNode);
			break;

		default:
			break;
		}
	}

	return bIsDifferent;
}

/*******************************************************************************
* FGraphDiffControl
*******************************************************************************/

FGraphDiffControl::FNodeMatch FGraphDiffControl::FindNodeMatch(class UEdGraph* Graph, class UEdGraphNode* Node, TArray<FNodeMatch> const& PriorMatches)
{
	FNodeMatch Match;
	Match.NewNode = Node;

	if (Graph != NULL)
	{
		// attempt to find a node matching 'Node'
		for (auto NodeIt(Graph->Nodes.CreateIterator()); NodeIt; ++NodeIt) 
		{
			UEdGraphNode* GraphNode = *NodeIt;
			if (GraphNode == NULL)
			{
				continue;
			}

			if (IsNodeMatch(Node, GraphNode, &PriorMatches))
			{
				Match.OldNode = GraphNode;
				break;
			}		
		}
	}	

	return Match;
}

bool FGraphDiffControl::IsNodeMatch(class UEdGraphNode* Node1, class UEdGraphNode* Node2, TArray<FGraphDiffControl::FNodeMatch> const* Exclusions)
{
	if(Node2->GetClass() != Node1->GetClass()) 
	{
		return false;
	}

	if(Node1->NodeGuid == Node2->NodeGuid)
	{
		return true;
	}

	// we could be diffing two completely separate assets, this makes sure both 
	// nodes historically belong to the same graph
	bool bIsIntraAssetDiff = (Node1->GetGraph()->GraphGuid == Node2->GetGraph()->GraphGuid);

	// if both nodes are from the same graph
	if (bIsIntraAssetDiff)
	{
		return (Node1->GetFName() == Node2->GetFName());
	}

	if (Exclusions != NULL)
	{
		// have to see if this node has already been matched with another
		for (auto MatchIt(Exclusions->CreateConstIterator()); MatchIt; ++MatchIt)
		{
			FGraphDiffControl::FNodeMatch const& PriorMatch = *MatchIt;
			if (!PriorMatch.IsValid())
			{
				continue;
			}

			// if one of these nodes has already been matched to a different node
			if (((PriorMatch.OldNode == Node1) && (PriorMatch.NewNode != Node2)) ||
				((PriorMatch.OldNode == Node2) && (PriorMatch.NewNode != Node1)) ||
				((PriorMatch.NewNode == Node1) && (PriorMatch.OldNode != Node2)) ||
				((PriorMatch.NewNode == Node2) && (PriorMatch.OldNode != Node1)))
			{
				return false;
			}
		}
	}

	// the name hashes won't match for nodes from separate graph assets, so we 
	// need to look for some kind of semblance between the two... title? (what's displayed to the user)
	FText Title1 = Node1->GetNodeTitle(ENodeTitleType::FullTitle);
	FText Title2 = Node2->GetNodeTitle(ENodeTitleType::FullTitle);

	return Title1.CompareTo(Title2) == 0;
}

bool FGraphDiffControl::DiffGraphs(UEdGraph* const LhsGraph, UEdGraph* const RhsGraph, TArray<FDiffSingleResult>& DiffsOut)
{
	bool bFoundDifferences = false;

	if ((LhsGraph != NULL) && (RhsGraph != NULL))
	{
		TArray<FGraphDiffControl::FNodeMatch> NodeMatches;
		TSet<UEdGraphNode const*> MatchedRhsNodes;

		FGraphDiffControl::FNodeDiffContext AdditiveDiffContext;
		AdditiveDiffContext.NodeTypeDisplayName = LOCTEXT("NodeDiffDisplayName", "Node");

		// march through the all the nodes in the rhs graph and look for matches 
		for (auto NodeIt(RhsGraph->Nodes.CreateConstIterator()); NodeIt; ++NodeIt)
		{
			UEdGraphNode* const RhsNode = *NodeIt;
			if (RhsNode == NULL)
			{
				continue;
			}

			FGraphDiffControl::FNodeMatch NodeMatch = FGraphDiffControl::FindNodeMatch(LhsGraph, RhsNode, NodeMatches);
			// if we found a corresponding node in the lhs graph, track it (so we
			// can prevent future matches with the same nodes)
			if (NodeMatch.IsValid())
			{
				NodeMatches.Add(NodeMatch);
				MatchedRhsNodes.Add(NodeMatch.OldNode);
			}

			bFoundDifferences |= NodeMatch.Diff(AdditiveDiffContext, &DiffsOut);
		}

		FGraphDiffControl::FNodeDiffContext SubtractiveDiffContext = AdditiveDiffContext;
		SubtractiveDiffContext.DiffMode = EDiffMode::Subtractive;
		SubtractiveDiffContext.DiffFlags = EDiffFlags::NodeExistance;

		// go through the lhs nodes to catch ones that may have been missing from the rhs graph
		for (auto NodeIt(LhsGraph->Nodes.CreateConstIterator()); NodeIt; ++NodeIt)
		{
			UEdGraphNode* const LhsNode = *NodeIt;
			// if this node has already been matched, move on
			if ((LhsNode == NULL) || MatchedRhsNodes.Find(LhsNode))
			{
				continue;
			}

			FGraphDiffControl::FNodeMatch NodeMatch = FGraphDiffControl::FindNodeMatch(RhsGraph, LhsNode, NodeMatches);
			bFoundDifferences |= NodeMatch.Diff(SubtractiveDiffContext, &DiffsOut);
		}
	}

	// storing the graph name for all diff entries:
	FName GraphName = LhsGraph ? LhsGraph->GetFName() : RhsGraph->GetFName();
	for( auto& Entry : DiffsOut )
	{
		Entry.OwningGraph = GraphName;
	}

	return bFoundDifferences;
}

#undef LOCTEXT_NAMESPACE
