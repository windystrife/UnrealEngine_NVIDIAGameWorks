// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EdGraph/EdGraphSchema.h"
#include "UObject/MetaData.h"
#include "UObject/UnrealType.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/CoreStyle.h"
#include "Framework/Application/SlateApplication.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "EdGraph/EdGraph.h"
#if WITH_EDITOR
#include "Misc/ConfigCacheIni.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "ScopedTransaction.h"
#include "EditorCategoryUtils.h"
#endif

#define LOCTEXT_NAMESPACE "EdGraph"

void FEdGraphSchemaAction::CosmeticUpdateCategory(FText NewCategory)
{
	Category = MoveTemp(NewCategory);

	Category.ToString().ParseIntoArray(LocalizedFullSearchCategoryArray, TEXT(" "), true);
	Category.BuildSourceString().ParseIntoArray(FullSearchCategoryArray, TEXT(" "), true);

	// Glob search text together, we use the SearchText string for basic filtering:
	UpdateSearchText();
}

void FEdGraphSchemaAction::UpdateSearchText()
{
	SearchText.Reset();

	for (FString& Entry : LocalizedFullSearchTitlesArray)
	{
		Entry.ToLowerInline();
		SearchText += Entry;
	}

	SearchText.Append(LINE_TERMINATOR);

	for (FString& Entry : LocalizedFullSearchKeywordsArray)
	{
		Entry.ToLowerInline();
		SearchText += Entry;
	}

	SearchText.Append(LINE_TERMINATOR);

	for (FString& Entry : LocalizedFullSearchCategoryArray)
	{
		Entry.ToLowerInline();
		SearchText += Entry;
	}

	for (FString& Entry : FullSearchTitlesArray)
	{
		Entry.ToLowerInline();
		SearchText += Entry;
	}

	SearchText.Append(LINE_TERMINATOR);

	for (FString& Entry : FullSearchKeywordsArray)
	{
		Entry.ToLowerInline();
		SearchText += Entry;
	}

	SearchText.Append(LINE_TERMINATOR);

	for (FString& Entry : FullSearchCategoryArray)
	{
		Entry.ToLowerInline();
		SearchText += Entry;
	}
}

void FEdGraphSchemaAction::UpdateSearchData(FText NewMenuDescription, FText NewToolTipDescription, FText NewCategory, FText NewKeywords)
{
	MenuDescription = MoveTemp(NewMenuDescription);
	TooltipDescription = MoveTemp(NewToolTipDescription);
	Category = MoveTemp(NewCategory);
	Keywords = MoveTemp(NewKeywords);

	MenuDescription.ToString().ParseIntoArray(LocalizedMenuDescriptionArray, TEXT(" "), true);
	MenuDescription.BuildSourceString().ParseIntoArray(MenuDescriptionArray, TEXT(" "), true);

	FullSearchTitlesArray = MenuDescriptionArray;
	LocalizedFullSearchTitlesArray = LocalizedMenuDescriptionArray;

	Keywords.ToString().ParseIntoArray(LocalizedFullSearchKeywordsArray, TEXT(" "), true);
	Keywords.BuildSourceString().ParseIntoArray(FullSearchKeywordsArray, TEXT(" "), true);
	
	Category.ToString().ParseIntoArray(LocalizedFullSearchCategoryArray, TEXT(" "), true);
	Category.BuildSourceString().ParseIntoArray(FullSearchCategoryArray, TEXT(" "), true);

	// Glob search text together, we use the SearchText string for basic filtering:
	UpdateSearchText();
}

/////////////////////////////////////////////////////
// FGraphActionListBuilderBase

void FGraphActionListBuilderBase::AddAction( const TSharedPtr<FEdGraphSchemaAction>& NewAction, FString const& Category)
{
	Entries.Add( ActionGroup( NewAction, Category ) );
}

void FGraphActionListBuilderBase::AddActionList( const TArray<TSharedPtr<FEdGraphSchemaAction> >& NewActions, FString const& Category)
{
	Entries.Add( ActionGroup( NewActions, Category ) );
}

void FGraphActionListBuilderBase::Append( FGraphActionListBuilderBase& Other )
{
	Entries.Append( MoveTemp(Other.Entries) );
}

int32 FGraphActionListBuilderBase::GetNumActions() const
{
	return Entries.Num();
}

FGraphActionListBuilderBase::ActionGroup& FGraphActionListBuilderBase::GetAction( const int32 Index )
{
	return Entries[Index];
}

void FGraphActionListBuilderBase::Empty()
{
	Entries.Empty();
}

/////////////////////////////////////////////////////
// FGraphActionListBuilderBase::GraphAction

FGraphActionListBuilderBase::ActionGroup::ActionGroup(TSharedPtr<FEdGraphSchemaAction> InAction, FString CategoryPrefix)
	: RootCategory(MoveTemp(CategoryPrefix))
{
	Actions.Add( InAction );
	InitCategoryChain();
}

FGraphActionListBuilderBase::ActionGroup::ActionGroup( const TArray< TSharedPtr<FEdGraphSchemaAction> >& InActions, FString CategoryPrefix)
	: RootCategory(MoveTemp(CategoryPrefix))
{
	Actions = InActions;
	InitCategoryChain();
}

FGraphActionListBuilderBase::ActionGroup::ActionGroup(FGraphActionListBuilderBase::ActionGroup && Other)
{
	Move(Other);
}

FGraphActionListBuilderBase::ActionGroup& FGraphActionListBuilderBase::ActionGroup::operator=(FGraphActionListBuilderBase::ActionGroup && Other)
{
	if (&Other != this)
	{
		Move(Other);
	}
	return *this;
}

FGraphActionListBuilderBase::ActionGroup::ActionGroup(const FGraphActionListBuilderBase::ActionGroup& Other)
{
	Copy(Other);
}

FGraphActionListBuilderBase::ActionGroup& FGraphActionListBuilderBase::ActionGroup::operator=(const FGraphActionListBuilderBase::ActionGroup& Other)
{
	if (&Other != this)
	{
		Copy(Other);
	}
	return *this;
}

FGraphActionListBuilderBase::ActionGroup::~ActionGroup()
{
}

const TArray<FString>& FGraphActionListBuilderBase::ActionGroup::GetCategoryChain() const
{
#if WITH_EDITOR
	return CategoryChain;
#else
	static TArray<FString> Dummy;
	return Dummy;
#endif
}

void FGraphActionListBuilderBase::ActionGroup::PerformAction( class UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location )
{
	for ( int32 ActionIndex = 0; ActionIndex < Actions.Num(); ActionIndex++ )
	{
		TSharedPtr<FEdGraphSchemaAction> CurrentAction = Actions[ActionIndex];
		if ( CurrentAction.IsValid() )
		{
			CurrentAction->PerformAction( ParentGraph, FromPins, Location );
		}
	}
}

void FGraphActionListBuilderBase::ActionGroup::Move(FGraphActionListBuilderBase::ActionGroup& Other)
{
	Actions = MoveTemp(Other.Actions);
	RootCategory = MoveTemp(Other.RootCategory);
	CategoryChain = MoveTemp(Other.CategoryChain);
}

void FGraphActionListBuilderBase::ActionGroup::Copy(const ActionGroup& Other)
{
	Actions = Other.Actions;
	RootCategory = Other.RootCategory;
	CategoryChain = Other.CategoryChain;
}

void FGraphActionListBuilderBase::ActionGroup::InitCategoryChain()
{
#if WITH_EDITOR
	const TCHAR* CategoryDelim = TEXT("|");
	FEditorCategoryUtils::GetCategoryDisplayString(RootCategory).ParseIntoArray(CategoryChain, CategoryDelim, true);

	if (Actions.Num() > 0)
	{
		TArray<FString> SubCategoryChain;

		FString SubCategory = FEditorCategoryUtils::GetCategoryDisplayString(Actions[0]->GetCategory().ToString());
		SubCategory.ParseIntoArray(SubCategoryChain, CategoryDelim, true);

		CategoryChain.Append(SubCategoryChain);
	}

	for (FString& Category : CategoryChain)
	{
		Category.TrimStartInline();
	}
#endif
}

/////////////////////////////////////////////////////
// FCategorizedGraphActionListBuilder

static FString ConcatCategories(FString RootCategory, FString const& SubCategory)
{
	FString ConcatedCategory = MoveTemp(RootCategory);
	if (!SubCategory.IsEmpty() && !ConcatedCategory.IsEmpty())
	{
		ConcatedCategory += TEXT("|");
	}
	ConcatedCategory += SubCategory;

	return ConcatedCategory;
}

FCategorizedGraphActionListBuilder::FCategorizedGraphActionListBuilder(FString CategoryIn)
	: Category(MoveTemp(CategoryIn))
{
}

void FCategorizedGraphActionListBuilder::AddAction(TSharedPtr<FEdGraphSchemaAction> const& NewAction, FString const& CategoryIn)
{
	FGraphActionListBuilderBase::AddAction(NewAction, ConcatCategories(Category, CategoryIn));
}

void FCategorizedGraphActionListBuilder::AddActionList(TArray<TSharedPtr<FEdGraphSchemaAction> > const& NewActions, FString const& CategoryIn)
{
	FGraphActionListBuilderBase::AddActionList(NewActions, ConcatCategories(Category, CategoryIn));
}

/////////////////////////////////////////////////////
// FGraphContextMenuBuilder

FGraphContextMenuBuilder::FGraphContextMenuBuilder(const UEdGraph* InGraph) 
	: CurrentGraph(InGraph)
{
	OwnerOfTemporaries =  NewObject<UEdGraph>((UObject*)GetTransientPackage());
}

/////////////////////////////////////////////////////
// FEdGraphSchemaAction_NewNode

#define SNAP_GRID (16) // @todo ensure this is the same as SNodePanel::GetSnapGridSize()

namespace 
{
	// Maximum distance a drag can be off a node edge to require 'push off' from node
	const int32 NodeDistance = 60;
}

UEdGraphNode* FEdGraphSchemaAction_NewNode::CreateNode(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, class UEdGraphNode* InNodeTemplate)
{
	// Duplicate template node to create new node
	UEdGraphNode* ResultNode = nullptr;

#if WITH_EDITOR
	ResultNode = DuplicateObject<UEdGraphNode>(InNodeTemplate, ParentGraph);
	ResultNode->SetFlags(RF_Transactional);

	ParentGraph->AddNode(ResultNode, true);

	ResultNode->CreateNewGuid();
	ResultNode->PostPlacedNewNode();
	ResultNode->AllocateDefaultPins();
	ResultNode->AutowireNewNode(FromPin);

	// For input pins, new node will generally overlap node being dragged off
	// Work out if we want to visually push away from connected node
	int32 XLocation = Location.X;
	if (FromPin && FromPin->Direction == EGPD_Input)
	{
		UEdGraphNode* PinNode = FromPin->GetOwningNode();
		const float XDelta = FMath::Abs(PinNode->NodePosX - Location.X);

		if (XDelta < NodeDistance)
		{
			// Set location to edge of current node minus the max move distance
			// to force node to push off from connect node enough to give selection handle
			XLocation = PinNode->NodePosX - NodeDistance;
		}
	}

	ResultNode->NodePosX = XLocation;
	ResultNode->NodePosY = Location.Y;
	ResultNode->SnapToGrid(SNAP_GRID);
#endif // WITH_EDITOR

	return ResultNode;
}

UEdGraphNode* FEdGraphSchemaAction_NewNode::PerformAction(class UEdGraph* ParentGraph, UEdGraphPin* FromPin, const FVector2D Location, bool bSelectNewNode/* = true*/)
{
	UEdGraphNode* ResultNode = nullptr;

#if WITH_EDITOR
	// If there is a template, we actually use it
	if (NodeTemplate != nullptr)
	{
		const FScopedTransaction Transaction(LOCTEXT("AddNode", "Add Node"));
		ParentGraph->Modify();
		if (FromPin)
		{
			FromPin->Modify();
		}

		ResultNode = CreateNode(ParentGraph, FromPin, Location, NodeTemplate);
	}
#endif // WITH_EDITOR

	return ResultNode;
}

UEdGraphNode* FEdGraphSchemaAction_NewNode::PerformAction(class UEdGraph* ParentGraph, TArray<UEdGraphPin*>& FromPins, const FVector2D Location, bool bSelectNewNode/* = true*/) 
{
	UEdGraphNode* ResultNode = nullptr;

#if WITH_EDITOR
	if (FromPins.Num() > 0)
	{
		ResultNode = PerformAction(ParentGraph, FromPins[0], Location, bSelectNewNode);

		// Try autowiring the rest of the pins
		for (int32 Index = 1; Index < FromPins.Num(); ++Index)
		{
			ResultNode->AutowireNewNode(FromPins[Index]);
		}
	}
	else
	{
		ResultNode = PerformAction(ParentGraph, nullptr, Location, bSelectNewNode);
	}
#endif // WITH_EDITOR

	return ResultNode;
}

void FEdGraphSchemaAction_NewNode::AddReferencedObjects( FReferenceCollector& Collector )
{
	FEdGraphSchemaAction::AddReferencedObjects( Collector );

	// These don't get saved to disk, but we want to make sure the objects don't get GC'd while the action array is around
	Collector.AddReferencedObject( NodeTemplate );
}

/////////////////////////////////////////////////////
// UEdGraphSchema

UEdGraphSchema::UEdGraphSchema(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UEdGraphSchema::TryCreateConnection(UEdGraphPin* PinA, UEdGraphPin* PinB) const
{
	const FPinConnectionResponse Response = CanCreateConnection(PinA, PinB);
	bool bModified = false;

	switch (Response.Response)
	{
	case CONNECT_RESPONSE_MAKE:
		PinA->Modify();
		PinB->Modify();
		PinA->MakeLinkTo(PinB);
		bModified = true;
		break;

	case CONNECT_RESPONSE_BREAK_OTHERS_A:
		PinA->Modify();
		PinB->Modify();
		PinA->BreakAllPinLinks(true);
		PinA->MakeLinkTo(PinB);
		bModified = true;
		break;

	case CONNECT_RESPONSE_BREAK_OTHERS_B:
		PinA->Modify();
		PinB->Modify();
		PinB->BreakAllPinLinks(true);
		PinA->MakeLinkTo(PinB);
		bModified = true;
		break;

	case CONNECT_RESPONSE_BREAK_OTHERS_AB:
		PinA->Modify();
		PinB->Modify();
		PinA->BreakAllPinLinks(true);
		PinB->BreakAllPinLinks(true);
		PinA->MakeLinkTo(PinB);
		bModified = true;
		break;

	case CONNECT_RESPONSE_MAKE_WITH_CONVERSION_NODE:
		bModified = CreateAutomaticConversionNodeAndConnections(PinA, PinB);
		break;
		break;

	case CONNECT_RESPONSE_DISALLOW:
	default:
		break;
	}

#if WITH_EDITOR
	if (bModified)
	{
		PinA->GetOwningNode()->PinConnectionListChanged(PinA);
		PinB->GetOwningNode()->PinConnectionListChanged(PinB);
	}
#endif	//#if WITH_EDITOR

	return bModified;
}

bool UEdGraphSchema::CreateAutomaticConversionNodeAndConnections(UEdGraphPin* PinA, UEdGraphPin* PinB) const
{
	return false;
}

void UEdGraphSchema::TrySetDefaultValue(UEdGraphPin& Pin, const FString& NewDefaultValue) const
{
	Pin.DefaultValue = NewDefaultValue;

#if WITH_EDITOR
	UEdGraphNode* Node = Pin.GetOwningNode();
	check(Node);
	Node->PinDefaultValueChanged(&Pin);
#endif	//#if WITH_EDITOR
}

void UEdGraphSchema::TrySetDefaultObject(UEdGraphPin& Pin, UObject* NewDefaultObject) const
{
	Pin.DefaultObject = NewDefaultObject;

#if WITH_EDITOR
	UEdGraphNode* Node = Pin.GetOwningNode();
	check(Node);
	Node->PinDefaultValueChanged(&Pin);
#endif	//#if WITH_EDITOR
}

void UEdGraphSchema::TrySetDefaultText(UEdGraphPin& InPin, const FText& InNewDefaultText) const
{
	InPin.DefaultTextValue = InNewDefaultText;

#if WITH_EDITOR
	UEdGraphNode* Node = InPin.GetOwningNode();
	check(Node);
	Node->PinDefaultValueChanged(&InPin);
#endif	//#if WITH_EDITOR
}

bool UEdGraphSchema::DoesDefaultValueMatchAutogenerated(const UEdGraphPin& InPin) const
{
	// Same logic as UEdGraphPin::DoesDefaultValueMatchAutogenerated
	// Ignoring case on purpose to match default behavior
	return InPin.GetDefaultAsString().Equals(InPin.AutogeneratedDefaultValue, ESearchCase::IgnoreCase);
}

void UEdGraphSchema::BreakNodeLinks(UEdGraphNode& TargetNode) const
{
#if WITH_EDITOR
	TSet<UEdGraphNode*> NodeList;
	NodeList.Add(&TargetNode);
	
	// Iterate over each pin and break all links
	for (TArray<UEdGraphPin*>::TIterator PinIt(TargetNode.Pins); PinIt; ++PinIt)
	{
		UEdGraphPin* TargetPin = *PinIt;
		if (TargetPin != nullptr && TargetPin->SubPins.Num() == 0)
		{
			// Keep track of which node(s) the pin's connected to
			for (UEdGraphPin*& OtherPin : TargetPin->LinkedTo)
			{
				if (OtherPin)
				{
					UEdGraphNode* OtherNode = OtherPin->GetOwningNode();
					if (OtherNode)
					{
						NodeList.Add(OtherNode);
					}
				}
			}

			BreakPinLinks(*TargetPin, false);
		}
	}
	
	// Send all nodes that lost connections a notification
	for (auto It = NodeList.CreateConstIterator(); It; ++It)
	{
		UEdGraphNode* Node = (*It);
		Node->NodeConnectionListChanged();
	}
#endif	//#if WITH_EDITOR
}

bool UEdGraphSchema::SetNodeMetaData(UEdGraphNode* Node, FName const& KeyValue)
{
	if (UPackage* Package = Node->GetOutermost())
	{
		if (UMetaData* MetaData = Package->GetMetaData())
		{
			MetaData->SetValue(Node, KeyValue, TEXT("true"));
			return true;
		}
	}
	return false;
}

void UEdGraphSchema::BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotification) const
{
#if WITH_EDITOR
	// Copy the old pin links
	TArray<class UEdGraphPin*> OldLinkedTo(TargetPin.LinkedTo);
#endif

	TargetPin.BreakAllPinLinks();

#if WITH_EDITOR
	UEdGraphNode* OwningNode = TargetPin.GetOwningNode();
	TSet<UEdGraphNode*> NodeList;

	// Notify this node
	if (OwningNode != nullptr)
	{
		OwningNode->PinConnectionListChanged(&TargetPin);
		NodeList.Add(OwningNode);
	}

	// As well as all other nodes that were connected
	for (UEdGraphPin* OtherPin : OldLinkedTo)
	{
		if (UEdGraphNode* OtherNode = OtherPin->GetOwningNode())
		{
			OtherNode->PinConnectionListChanged(OtherPin);
			NodeList.Add(OtherNode);
		}
	}

	if (bSendsNodeNotification)
	{
		// Send all nodes that received a new pin connection a notification
		for (UEdGraphNode* Node : NodeList)
		{
			Node->NodeConnectionListChanged();
		}
	}
	
#endif	//#if WITH_EDITOR
}

void UEdGraphSchema::BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin)
{
	SourcePin->BreakLinkTo(TargetPin);

#if WITH_EDITOR
	// get a reference to these now as the following calls can potentially clear the OwningNode (ex: split pins in MakeArray nodes)
	UEdGraphNode* TargetNode = TargetPin->GetOwningNode();
	UEdGraphNode* SourceNode = SourcePin->GetOwningNode();
	
	TargetNode->PinConnectionListChanged(TargetPin);
	SourceNode->PinConnectionListChanged(SourcePin);
	
	TargetNode->NodeConnectionListChanged();
	SourceNode->NodeConnectionListChanged();
#endif	//#if WITH_EDITOR
}

FPinConnectionResponse UEdGraphSchema::MovePinLinks(UEdGraphPin& MoveFromPin, UEdGraphPin& MoveToPin, bool bIsIntermediateMove) const
{
#if WITH_EDITOR
	ensureMsgf(bIsIntermediateMove || !MoveToPin.GetOwningNode()->GetGraph()->HasAnyFlags(RF_Transient),
		TEXT("When moving to an Intermediate pin, use FKismetCompilerContext::MovePinLinksToIntermediate() instead of UEdGraphSchema::MovePinLinks()"));
#endif // #if WITH_EDITOR

	FPinConnectionResponse FinalResponse = FPinConnectionResponse(CONNECT_RESPONSE_MAKE, FText());
	// First copy the current set of links
	TArray<UEdGraphPin*> CurrentLinks = MoveFromPin.LinkedTo;
	// Then break all links at pin we are moving from
	MoveFromPin.BreakAllPinLinks();
	// Try and make each new connection
	for (int32 i=0; i<CurrentLinks.Num(); i++)
	{
		UEdGraphPin* NewLink = CurrentLinks[i];
#if WITH_EDITORONLY_DATA
		// Don't move connections to removed pins
		if (!NewLink->bOrphanedPin)
#endif
		{
			FPinConnectionResponse Response = CanCreateConnection(&MoveToPin, NewLink);
			if (Response.CanSafeConnect())
			{
				MoveToPin.MakeLinkTo(NewLink);
			}
			else
			{
				FinalResponse = Response;
			}
		}
	}
	// Move over the default values
	MoveToPin.DefaultValue = MoveFromPin.DefaultValue;
	MoveToPin.DefaultObject = MoveFromPin.DefaultObject;
	MoveToPin.DefaultTextValue = MoveFromPin.DefaultTextValue;
	return FinalResponse;
}

FPinConnectionResponse UEdGraphSchema::CopyPinLinks(UEdGraphPin& CopyFromPin, UEdGraphPin& CopyToPin, bool bIsIntermediateCopy) const
{
#if WITH_EDITOR
	ensureMsgf(bIsIntermediateCopy || !CopyToPin.GetOwningNode()->GetGraph()->HasAnyFlags(RF_Transient),
		TEXT("When copying to an Intermediate pin, use FKismetCompilerContext::CopyPinLinksToIntermediate() instead of UEdGraphSchema::CopyPinLinks()"));
#endif // #if WITH_EDITOR

	FPinConnectionResponse FinalResponse = FPinConnectionResponse(CONNECT_RESPONSE_MAKE, FText());
	for (int32 i=0; i<CopyFromPin.LinkedTo.Num(); i++)
	{
		UEdGraphPin* NewLink = CopyFromPin.LinkedTo[i];
		FPinConnectionResponse Response = CanCreateConnection(&CopyToPin, NewLink);
#if WITH_EDITORONLY_DATA
		// Don't copy connections to removed pins
		if (!NewLink->bOrphanedPin)
#endif
		{
			if (Response.CanSafeConnect())
			{
				CopyToPin.MakeLinkTo(NewLink);
			}
			else
			{
				FinalResponse = Response;
			}
		}
	}

	CopyToPin.DefaultValue = CopyFromPin.DefaultValue;
	CopyToPin.DefaultObject = CopyFromPin.DefaultObject;
	CopyToPin.DefaultTextValue = CopyFromPin.DefaultTextValue;
	return FinalResponse;
}

#if WITH_EDITORONLY_DATA
FText UEdGraphSchema::GetPinDisplayName(const UEdGraphPin* Pin) const
{
	FText ResultPinName;
	check(Pin != nullptr);
	if (Pin->PinFriendlyName.IsEmpty())
	{
		ResultPinName = FText::FromString(Pin->PinName);
	}
	else
	{
		ResultPinName = Pin->PinFriendlyName;
		static bool bInitializedShowNodesAndPinsUnlocalized = false;
		static bool bShowNodesAndPinsUnlocalized = false;
		if (!bInitializedShowNodesAndPinsUnlocalized)
		{
			GConfig->GetBool( TEXT("Internationalization"), TEXT("ShowNodesAndPinsUnlocalized"), bShowNodesAndPinsUnlocalized, GEditorSettingsIni );
			bInitializedShowNodesAndPinsUnlocalized =  true;
		}
		if (bShowNodesAndPinsUnlocalized)
		{
			ResultPinName = FText::FromString(ResultPinName.BuildSourceString());
		}
	}
	return ResultPinName;
}
#endif // WITH_EDITORONLY_DATA

void UEdGraphSchema::ConstructBasicPinTooltip(UEdGraphPin const& Pin, FText const& PinDescription, FString& TooltipOut) const
{
	TooltipOut = PinDescription.ToString();
}

void UEdGraphSchema::GetGraphContextActions(FGraphContextMenuBuilder& ContextMenuBuilder) const
{
#if WITH_EDITOR
	// Run thru all nodes and add any menu items they want to add
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* Class = *ClassIt;
		if (Class->IsChildOf(UEdGraphNode::StaticClass()) && !Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated))
		{
			const UEdGraphNode* ClassCDO = Class->GetDefaultObject<UEdGraphNode>();

			if (ClassCDO->CanCreateUnderSpecifiedSchema(this))
			{
				ClassCDO->GetMenuEntries(ContextMenuBuilder);
			}
		}
	}
#endif
}

void UEdGraphSchema::ReconstructNode(UEdGraphNode& TargetNode, bool bIsBatchRequest/*=false*/) const
{
#if WITH_EDITOR
	TargetNode.ReconstructNode();
#endif	//#if WITH_EDITOR
}

void UEdGraphSchema::GetGraphDisplayInformation(const UEdGraph& Graph, /*out*/ FGraphDisplayInfo& DisplayInfo) const
{
	DisplayInfo.PlainName = FText::FromString( Graph.GetName() );
	DisplayInfo.DisplayName = DisplayInfo.PlainName;
}

void UEdGraphSchema::GetContextMenuActions(const UEdGraph* CurrentGraph, const UEdGraphNode* InGraphNode, const UEdGraphPin* InGraphPin, class FMenuBuilder* MenuBuilder, bool bIsDebugging) const
{
#if WITH_EDITOR
	FGraphNodeContextMenuBuilder Context(CurrentGraph, InGraphNode, InGraphPin, MenuBuilder, bIsDebugging);

	if (Context.Node)
	{
		Context.Node->GetContextMenuActions(Context);
	}

	if (InGraphNode)
	{
		// Helper to do the node comment editing
		struct Local
		{
			// Called by the EditableText widget to get the current comment for the node
			static FString GetNodeComment(TWeakObjectPtr<UEdGraphNode> NodeWeakPtr)
			{
				if (UEdGraphNode* SelectedNode = NodeWeakPtr.Get())
				{
					return SelectedNode->NodeComment;
				}
				return FString();
			}

			// Called by the EditableText widget when the user types a new comment for the selected node
			static void OnNodeCommentTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo, TWeakObjectPtr<UEdGraphNode> NodeWeakPtr)
			{
				// Apply the change to the selected actor
				UEdGraphNode* SelectedNode = NodeWeakPtr.Get();
				FString NewString = NewText.ToString();
				if (SelectedNode && !SelectedNode->NodeComment.Equals( NewString, ESearchCase::CaseSensitive ))
				{
					// send property changed events
					const FScopedTransaction Transaction( LOCTEXT("EditNodeComment", "Change Node Comment") );
					SelectedNode->Modify();
					UProperty* NodeCommentProperty = FindField<UProperty>(SelectedNode->GetClass(), "NodeComment");
					if(NodeCommentProperty != nullptr)
					{
						SelectedNode->PreEditChange(NodeCommentProperty);

						SelectedNode->NodeComment = NewString;
						SelectedNode->SetMakeCommentBubbleVisible(true);

						FPropertyChangedEvent NodeCommentPropertyChangedEvent(NodeCommentProperty);
						SelectedNode->PostEditChangeProperty(NodeCommentPropertyChangedEvent);
					}
				}

				// Only dismiss all menus if the text was committed via some user action.
				// ETextCommit::Default implies that focus was switched by some other means. If this is because a submenu was opened, we don't want to close all the menus as a consequence.
				if (CommitInfo != ETextCommit::Default)
				{
					FSlateApplication::Get().DismissAllMenus();
				}
			}
		};

		if( !InGraphPin )
		{
			int32 SelectionCount = GetNodeSelectionCount(CurrentGraph);
		
			if( SelectionCount == 1 )
			{
				// Node comment area
				TSharedRef<SHorizontalBox> NodeCommentBox = SNew(SHorizontalBox);

				MenuBuilder->BeginSection("GraphNodeComment", LOCTEXT("NodeCommentMenuHeader", "Node Comment"));
				{
					MenuBuilder->AddWidget(NodeCommentBox, FText::GetEmpty() );
				}
				TWeakObjectPtr<UEdGraphNode> SelectedNodeWeakPtr = InGraphNode;

				FText NodeCommentText;
				if ( UEdGraphNode* SelectedNode = SelectedNodeWeakPtr.Get() )
				{
					NodeCommentText = FText::FromString( SelectedNode->NodeComment );
				}

			const FSlateBrush* NodeIcon = FCoreStyle::Get().GetDefaultBrush();//@TODO: FActorIconFinder::FindIconForActor(SelectedActors(0).Get());


				// Comment label
				NodeCommentBox->AddSlot()
				.VAlign(VAlign_Center)
				.FillWidth( 1.0f )
				[
					SNew(SMultiLineEditableTextBox)
					.Text( NodeCommentText )
					.ToolTipText(LOCTEXT("NodeComment_ToolTip", "Comment for this node"))
					.OnTextCommitted_Static(&Local::OnNodeCommentTextCommitted, SelectedNodeWeakPtr)
					.SelectAllTextWhenFocused( true )
					.RevertTextOnEscape( true )
					.ModiferKeyForNewLine( EModifierKey::Control )
				];
				MenuBuilder->EndSection();
			}
			else if( SelectionCount > 1 )
			{
				struct SCommentUtility
				{
					static void CreateComment(const UEdGraphSchema* Schema, UEdGraph* Graph)
					{
						if( Schema && Graph )
						{
							TSharedPtr<FEdGraphSchemaAction> Action = Schema->GetCreateCommentAction();

							if( Action.IsValid() )
							{
								Action->PerformAction(Graph, nullptr, FVector2D());
							}
						}
					}
				};

				MenuBuilder->BeginSection("SchemaActionComment", LOCTEXT("MultiCommentHeader", "Comment Group"));
				MenuBuilder->AddMenuEntry(	LOCTEXT("MultiCommentDesc", "Create Comment from Selection"),
											LOCTEXT("CommentToolTip", "Create a resizable comment box around selection."),
											FSlateIcon(), 
											FUIAction( FExecuteAction::CreateStatic( SCommentUtility::CreateComment, this, const_cast<UEdGraph*>( CurrentGraph ))));
				MenuBuilder->EndSection();
			}
		}
	}
#endif
}

FString UEdGraphSchema::IsCurrentPinDefaultValid(const UEdGraphPin* Pin) const
{
	return IsPinDefaultValid(Pin, Pin->DefaultValue, Pin->DefaultObject, Pin->DefaultTextValue);
}

#undef LOCTEXT_NAMESPACE
