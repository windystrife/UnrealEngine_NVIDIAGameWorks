// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimationStateNodes/SGraphNodeAnimState.h"
#include "AnimStateNodeBase.h"
#include "AnimStateConduitNode.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SToolTip.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimBlueprint.h"
#include "SGraphPreviewer.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "IDocumentation.h"
#include "AnimationStateMachineGraph.h"
#include "Animation/AnimNode_StateMachine.h"
#include "AnimGraphNode_StateMachineBase.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"

/////////////////////////////////////////////////////
// SStateMachineOutputPin

class SStateMachineOutputPin : public SGraphPin
{
public:
	SLATE_BEGIN_ARGS(SStateMachineOutputPin){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UEdGraphPin* InPin);
protected:
	// Begin SGraphPin interface
	virtual TSharedRef<SWidget>	GetDefaultValueWidget() override;
	// End SGraphPin interface

	const FSlateBrush* GetPinBorder() const;
};

void SStateMachineOutputPin::Construct(const FArguments& InArgs, UEdGraphPin* InPin)
{
	this->SetCursor( EMouseCursor::Default );

	typedef SStateMachineOutputPin ThisClass;

	bShowLabel = true;

	GraphPinObj = InPin;
	check(GraphPinObj != NULL);

	const UEdGraphSchema* Schema = GraphPinObj->GetSchema();
	check(Schema);

	// Set up a hover for pins that is tinted the color of the pin.
	SBorder::Construct( SBorder::FArguments()
		.BorderImage( this, &SStateMachineOutputPin::GetPinBorder )
		.BorderBackgroundColor( this, &ThisClass::GetPinColor )
		.OnMouseButtonDown( this, &ThisClass::OnPinMouseDown )
		.Cursor( this, &ThisClass::GetPinCursor )
	);
}

TSharedRef<SWidget>	SStateMachineOutputPin::GetDefaultValueWidget()
{
	return SNew(STextBlock);
}

const FSlateBrush* SStateMachineOutputPin::GetPinBorder() const
{
	return ( IsHovered() )
		? FEditorStyle::GetBrush( TEXT("Graph.StateNode.Pin.BackgroundHovered") )
		: FEditorStyle::GetBrush( TEXT("Graph.StateNode.Pin.Background") );
}

/////////////////////////////////////////////////////
// SGraphNodeAnimState

void SGraphNodeAnimState::Construct(const FArguments& InArgs, UAnimStateNodeBase* InNode)
{
	this->GraphNode = InNode;

	this->SetCursor(EMouseCursor::CardinalCross);

	this->UpdateGraphNode();
}

void SGraphNodeAnimState::GetStateInfoPopup(UEdGraphNode* GraphNode, TArray<FGraphInformationPopupInfo>& Popups)
{
	UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(FBlueprintEditorUtils::FindBlueprintForNodeChecked(GraphNode));
	check(AnimBlueprint);
	UAnimInstance* ActiveObject = Cast<UAnimInstance>(AnimBlueprint->GetObjectBeingDebugged());
	UAnimBlueprintGeneratedClass* Class = AnimBlueprint->GetAnimBlueprintGeneratedClass();

	//FKismetNodeInfoContext* K2Context = (FKismetNodeInfoContext*)Context;
	FLinearColor CurrentStateColor(1.f, 0.5f, 0.25f);

	// Display various types of debug data
	if ((ActiveObject != NULL) && (Class != NULL))
	{
		if (FStateMachineDebugData* DebugInfo = Class->GetAnimBlueprintDebugData().StateMachineDebugData.Find(GraphNode->GetGraph()))
		{
			if (Class->AnimNodeProperties.Num())
			{
				UAnimationStateMachineGraph* TypedGraph = CastChecked<UAnimationStateMachineGraph>(GraphNode->GetGraph());

				if (FAnimNode_StateMachine* CurrentInstance = Class->GetPropertyInstance<FAnimNode_StateMachine>(ActiveObject, TypedGraph->OwnerAnimGraphNode))  
				{
					if (int32* pStateIndex = DebugInfo->NodeToStateIndex.Find(GraphNode))
					{
						const float Weight = CurrentInstance->GetStateWeight(*pStateIndex);
						if (Weight > 0.0f)
						{
							FString StateText = FString::Printf(TEXT("%.1f%%"), Weight * 100.0f);
							if (*pStateIndex == CurrentInstance->GetCurrentState())
							{
								StateText += FString::Printf(TEXT("\nActive for %.2f secs"), CurrentInstance->GetCurrentStateElapsedTime());
							}

							new (Popups) FGraphInformationPopupInfo(NULL, CurrentStateColor, StateText);
						}
					}
				}
			}
		}
	}
}

void SGraphNodeAnimState::GetNodeInfoPopups(FNodeInfoContext* Context, TArray<FGraphInformationPopupInfo>& Popups) const
{
	GetStateInfoPopup(GraphNode, Popups);
}

FSlateColor SGraphNodeAnimState::GetBorderBackgroundColor() const
{
	UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(FBlueprintEditorUtils::FindBlueprintForNodeChecked(GraphNode));
	check(AnimBlueprint);
	UAnimInstance* ActiveObject = Cast<UAnimInstance>(AnimBlueprint->GetObjectBeingDebugged());
	UAnimBlueprintGeneratedClass* Class = AnimBlueprint->GetAnimBlueprintGeneratedClass();

	//FKismetNodeInfoContext* K2Context = (FKismetNodeInfoContext*)Context;

	FLinearColor InactiveStateColor(0.08f, 0.08f, 0.08f);
	FLinearColor ActiveStateColorDim(0.4f, 0.3f, 0.15f);
	FLinearColor ActiveStateColorBright(1.f, 0.6f, 0.35f);

	// Display various types of debug data
	if ((ActiveObject != NULL) && (Class != NULL))
	{
		if (FStateMachineDebugData* DebugInfo = Class->GetAnimBlueprintDebugData().StateMachineDebugData.Find(GraphNode->GetGraph()))
		{
			if (Class->AnimNodeProperties.Num())
			{
				UAnimationStateMachineGraph* TypedGraph = CastChecked<UAnimationStateMachineGraph>(GraphNode->GetGraph());

				if (FAnimNode_StateMachine* CurrentInstance = Class->GetPropertyInstance<FAnimNode_StateMachine>(ActiveObject, TypedGraph->OwnerAnimGraphNode))
				{
					if (int32* pStateIndex = DebugInfo->NodeToStateIndex.Find(GraphNode))
					{
						const float Weight = CurrentInstance->GetStateWeight(*pStateIndex);
						if (Weight > 0.0f)
						{
							return FMath::Lerp<FLinearColor>(ActiveStateColorDim, ActiveStateColorBright, Weight);
						}
					}
				}
			}
		}
	}

	return InactiveStateColor;
}

void SGraphNodeAnimState::UpdateGraphNode()
{
	InputPins.Empty();
	OutputPins.Empty();
	
	// Reset variables that are going to be exposed, in case we are refreshing an already setup node.
	RightNodeBox.Reset();
	LeftNodeBox.Reset();

	const FSlateBrush* NodeTypeIcon = GetNameIcon();

	FLinearColor TitleShadowColor(0.6f, 0.6f, 0.6f);
	TSharedPtr<SErrorText> ErrorText;
	TSharedPtr<SNodeTitle> NodeTitle = SNew(SNodeTitle, GraphNode);

	this->ContentScale.Bind( this, &SGraphNode::GetContentScale );
	this->GetOrAddSlot( ENodeZone::Center )
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.BorderImage( FEditorStyle::GetBrush( "Graph.StateNode.Body" ) )
			.Padding(0)
			.BorderBackgroundColor( this, &SGraphNodeAnimState::GetBorderBackgroundColor )
			[
				SNew(SOverlay)

				// PIN AREA
				+SOverlay::Slot()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Fill)
				[
					SAssignNew(RightNodeBox, SVerticalBox)
				]

				// STATE NAME AREA
				+SOverlay::Slot()
				.HAlign(HAlign_Center)
				.VAlign(VAlign_Center)
				.Padding(10.0f)
				[
					SNew(SBorder)
					.BorderImage( FEditorStyle::GetBrush("Graph.StateNode.ColorSpill") )
					.BorderBackgroundColor( TitleShadowColor )
					.HAlign(HAlign_Center)
					.VAlign(VAlign_Center)
					.Visibility(EVisibility::SelfHitTestInvisible)
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							// POPUP ERROR MESSAGE
							SAssignNew(ErrorText, SErrorText )
							.BackgroundColor( this, &SGraphNodeAnimState::GetErrorColor )
							.ToolTipText( this, &SGraphNodeAnimState::GetErrorMsgToolTip )
						]
						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SImage)
							.Image(NodeTypeIcon)
						]
						+SHorizontalBox::Slot()
						.Padding(FMargin(4.0f, 0.0f, 4.0f, 0.0f))
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
								.AutoHeight()
							[
								SAssignNew(InlineEditableText, SInlineEditableTextBlock)
								.Style( FEditorStyle::Get(), "Graph.StateNode.NodeTitleInlineEditableText" )
								.Text( NodeTitle.Get(), &SNodeTitle::GetHeadTitle )
								.OnVerifyTextChanged(this, &SGraphNodeAnimState::OnVerifyNameTextChanged)
								.OnTextCommitted(this, &SGraphNodeAnimState::OnNameTextCommited)
								.IsReadOnly( this, &SGraphNodeAnimState::IsNameReadOnly )
								.IsSelected(this, &SGraphNodeAnimState::IsSelectedExclusively)
							]
							+SVerticalBox::Slot()
								.AutoHeight()
							[
								NodeTitle.ToSharedRef()
							]
						]
					]
				]
			]
		];

	ErrorReporting = ErrorText;
	ErrorReporting->SetError(ErrorMsg);
	CreatePinWidgets();
}

void SGraphNodeAnimState::CreatePinWidgets()
{
	UAnimStateNodeBase* StateNode = CastChecked<UAnimStateNodeBase>(GraphNode);

	UEdGraphPin* CurPin = StateNode->GetOutputPin();
	if (!CurPin->bHidden)
	{
		TSharedPtr<SGraphPin> NewPin = SNew(SStateMachineOutputPin, CurPin);

		this->AddPin(NewPin.ToSharedRef());
	}
}

void SGraphNodeAnimState::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
{
	PinToAdd->SetOwner( SharedThis(this) );
	RightNodeBox->AddSlot()
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.FillHeight(1.0f)
		[
			PinToAdd
		];
	OutputPins.Add(PinToAdd);
}

TSharedPtr<SToolTip> SGraphNodeAnimState::GetComplexTooltip()
{
	UAnimStateNodeBase* StateNode = CastChecked<UAnimStateNodeBase>(GraphNode);

	return SNew(SToolTip)
		[
			SNew(SVerticalBox)
	
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				// Create the tooltip preview, ensure to disable state overlays to stop
				// PIE and read-only borders obscuring the graph
				SNew(SGraphPreviewer, StateNode->GetBoundGraph())
				.CornerOverlayText(this, &SGraphNodeAnimState::GetPreviewCornerText)
				.ShowGraphStateOverlay(false)
			]
	
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.0f, 5.0f, 0.0f, 0.0f))
			[
				IDocumentation::Get()->CreateToolTip(FText::FromString("Documentation"), NULL, StateNode->GetDocumentationLink(), StateNode->GetDocumentationExcerptName())
			]

		];
}

FText SGraphNodeAnimState::GetPreviewCornerText() const
{
	UAnimStateNodeBase* StateNode = CastChecked<UAnimStateNodeBase>(GraphNode);

	return FText::Format(NSLOCTEXT("SGraphNodeAnimState", "PreviewCornerStateText", "{0} state"), FText::FromString(StateNode->GetStateName()));
}

const FSlateBrush* SGraphNodeAnimState::GetNameIcon() const
{
	return FEditorStyle::GetBrush( TEXT("Graph.StateNode.Icon") );
}

/////////////////////////////////////////////////////
// SGraphNodeAnimConduit

void SGraphNodeAnimConduit::Construct(const FArguments& InArgs, UAnimStateConduitNode* InNode)
{
	SGraphNodeAnimState::Construct(SGraphNodeAnimState::FArguments(), InNode);
}

void SGraphNodeAnimConduit::GetNodeInfoPopups(FNodeInfoContext* Context, TArray<FGraphInformationPopupInfo>& Popups) const
{
	// Intentionally empty.
}

FText SGraphNodeAnimConduit::GetPreviewCornerText() const
{
	UAnimStateNodeBase* StateNode = CastChecked<UAnimStateNodeBase>(GraphNode);

	return FText::Format(NSLOCTEXT("SGraphNodeAnimState", "PreviewCornerConduitText", "{0} conduit"), FText::FromString(StateNode->GetStateName()));
}

const FSlateBrush* SGraphNodeAnimConduit::GetNameIcon() const
{
	return FEditorStyle::GetBrush( TEXT("Graph.ConduitNode.Icon") );
}
