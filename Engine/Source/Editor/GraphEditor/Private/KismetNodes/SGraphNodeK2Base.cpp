// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "KismetNodes/SGraphNodeK2Base.h"
#include "Engine/Engine.h"
#include "Internationalization/Culture.h"
#include "Modules/ModuleManager.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SToolTip.h"
#include "EngineGlobals.h"
#include "GraphEditorSettings.h"
#include "SCommentBubble.h"
#include "SGraphPin.h"
#include "EdGraphSchema_K2.h"
#include "K2Node.h"
#include "K2Node_Composite.h"
#include "K2Node_MacroInstance.h"
#include "K2Node_Timeline.h"
#include "Engine/Breakpoint.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "KismetNodes/KismetNodeInfoContext.h"
#include "IDocumentation.h"
#include "TutorialMetaData.h"
#include "Widgets/Layout/SBox.h"

#define LOCTEXT_NAMESPACE "SGraphNodeK2Base"

//////////////////////////////////////////////////////////////////////////
// SGraphNodeK2Base

const FLinearColor SGraphNodeK2Base::BreakpointHitColor(0.7f, 0.0f, 0.0f);
const FLinearColor SGraphNodeK2Base::LatentBubbleColor(1.f, 0.5f, 0.25f);
const FLinearColor SGraphNodeK2Base::TimelineBubbleColor(0.7f, 0.5f, 0.5f);
const FLinearColor SGraphNodeK2Base::PinnedWatchColor(0.7f, 0.5f, 0.5f);

void SGraphNodeK2Base::UpdateStandardNode()
{
	SGraphNode::UpdateGraphNode();
	// clear the default tooltip, to make room for our custom "complex" tooltip
	SetToolTip(NULL);
}

void SGraphNodeK2Base::UpdateCompactNode()
{
	InputPins.Empty();
	OutputPins.Empty();

	// error handling set-up
	SetupErrorReporting();

	// Reset variables that are going to be exposed, in case we are refreshing an already setup node.
	RightNodeBox.Reset();
	LeftNodeBox.Reset();

 	TSharedPtr< SToolTip > NodeToolTip = SNew( SToolTip );
	if (!GraphNode->GetTooltipText().IsEmpty())
	{
		NodeToolTip = IDocumentation::Get()->CreateToolTip( TAttribute< FText >( this, &SGraphNode::GetNodeTooltip ), NULL, GraphNode->GetDocumentationLink(), GraphNode->GetDocumentationExcerptName() );
	}

	// Setup a meta tag for this node
	FGraphNodeMetaData TagMeta(TEXT("Graphnode"));
	PopulateMetaTag(&TagMeta);
	
	TSharedPtr<SNodeTitle> NodeTitle = SNew(SNodeTitle, GraphNode)
		.Text(this, &SGraphNodeK2Base::GetNodeCompactTitle);

	TSharedRef<SOverlay> NodeOverlay = SNew(SOverlay);
	
	// add optional node specific widget to the overlay:
	TSharedPtr<SWidget> OverlayWidget = GraphNode->CreateNodeImage();
	if(OverlayWidget.IsValid())
	{
		NodeOverlay->AddSlot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew( SBox )
			.WidthOverride( 70.f )
			.HeightOverride( 70.f )
			[
				OverlayWidget.ToSharedRef()
			]
		];
	}

	NodeOverlay->AddSlot()
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.Padding(45.f, 0.f, 45.f, 0.f)
		[
			// MIDDLE
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.HAlign(HAlign_Center)
			.AutoHeight()
			[
				SNew(STextBlock)
					.TextStyle( FEditorStyle::Get(), "Graph.CompactNode.Title" )
					.Text( NodeTitle.Get(), &SNodeTitle::GetHeadTitle )
					.WrapTextAt(128.0f)
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				NodeTitle.ToSharedRef()
			]
		];
	
	NodeOverlay->AddSlot()
		.HAlign(HAlign_Left)
		.VAlign(VAlign_Center)
		.Padding(0.f, 0.f, 55.f, 0.f)
		[
			// LEFT
			SAssignNew(LeftNodeBox, SVerticalBox)
		];

	NodeOverlay->AddSlot()
		.HAlign(HAlign_Right)
		.VAlign(VAlign_Center)
		.Padding(55.f, 0.f, 0.f, 0.f)
		[
			// RIGHT
			SAssignNew(RightNodeBox, SVerticalBox)
		];

	//
	//             ______________________
	//            | (>) L |      | R (>) |
	//            | (>) E |      | I (>) |
	//            | (>) F |   +  | G (>) |
	//            | (>) T |      | H (>) |
	//            |       |      | T (>) |
	//            |_______|______|_______|
	//
	this->ContentScale.Bind( this, &SGraphNode::GetContentScale );
	this->GetOrAddSlot( ENodeZone::Center )
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SNew(SVerticalBox)
		+SVerticalBox::Slot()
		[
			// NODE CONTENT AREA
			SNew( SOverlay)
			+SOverlay::Slot()
			[
				SNew(SImage)
				.Image( FEditorStyle::GetBrush("Graph.VarNode.Body") )
			]
			+ SOverlay::Slot()
			[
				SNew(SImage)
				.Image( FEditorStyle::GetBrush("Graph.VarNode.Gloss") )
			]
			+SOverlay::Slot()
			.Padding( FMargin(0,3) )
			[
				NodeOverlay
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding( FMargin(5.0f, 1.0f) )
		[
			ErrorReporting->AsWidget()
		]
	];

	CreatePinWidgets();

	// Hide pin labels
	for (auto InputPin: this->InputPins)
	{
		if (InputPin->GetPinObj()->ParentPin == nullptr)
		{
			InputPin->SetShowLabel(false);
		}
	}

	for (auto OutputPin : this->OutputPins)
	{
		if (OutputPin->GetPinObj()->ParentPin == nullptr)
		{
			OutputPin->SetShowLabel(false);
		}
	}

	// Create comment bubble
	TSharedPtr<SCommentBubble> CommentBubble;
	const FSlateColor CommentColor = GetDefault<UGraphEditorSettings>()->DefaultCommentNodeTitleColor;

	SAssignNew( CommentBubble, SCommentBubble )
	.GraphNode( GraphNode )
	.Text( this, &SGraphNode::GetNodeComment )
	.OnTextCommitted( this, &SGraphNode::OnCommentTextCommitted )
	.ColorAndOpacity( CommentColor )
	.AllowPinning( true )
	.EnableTitleBarBubble( true )
	.EnableBubbleCtrls( true )
	.GraphLOD( this, &SGraphNode::GetCurrentLOD )
	.IsGraphNodeHovered( this, &SGraphNodeK2Base::IsHovered );

	GetOrAddSlot( ENodeZone::TopCenter )
	.SlotOffset( TAttribute<FVector2D>( CommentBubble.Get(), &SCommentBubble::GetOffset ))
	.SlotSize( TAttribute<FVector2D>( CommentBubble.Get(), &SCommentBubble::GetSize ))
	.AllowScaling( TAttribute<bool>( CommentBubble.Get(), &SCommentBubble::IsScalingAllowed ))
	.VAlign( VAlign_Top )
	[
		CommentBubble.ToSharedRef()
	];

	CreateInputSideAddButton(LeftNodeBox);
	CreateOutputSideAddButton(RightNodeBox);
}

TSharedPtr<SToolTip> SGraphNodeK2Base::GetComplexTooltip()
{
	TSharedPtr<SToolTip> NodeToolTip;
	TSharedRef<SToolTip> DefaultToolTip = IDocumentation::Get()->CreateToolTip(TAttribute<FText>(this, &SGraphNode::GetNodeTooltip), NULL, GraphNode->GetDocumentationLink(), GraphNode->GetDocumentationExcerptName());

	struct LocalUtils
	{
		static EVisibility IsToolTipVisible(TSharedRef<SGraphNodeK2Base> const NodeWidget)
		{
			return NodeWidget->GetNodeTooltip().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
		}

		static EVisibility IsToolTipHeadingVisible(TSharedRef<SGraphNodeK2Base> const NodeWidget)
		{
			return NodeWidget->GetToolTipHeading().IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
		}

		static bool IsInteractive()
		{
			const FModifierKeysState ModifierKeys = FSlateApplication::Get().GetModifierKeys();
			return ( ModifierKeys.IsAltDown() && ModifierKeys.IsControlDown() );
		}
	};
	TSharedRef<SGraphNodeK2Base> ThisRef = SharedThis(this);

	TSharedPtr< SVerticalBox > VerticalBoxWidget;
	SAssignNew(NodeToolTip, SToolTip)
		.Visibility_Static(&LocalUtils::IsToolTipVisible, ThisRef)
		.IsInteractive_Static(&LocalUtils::IsInteractive)

		// Emulate text-only tool-tip styling that SToolTip uses when no custom content is supplied.  We want node tool-tips to 
		// be styled just like text-only tool-tips
		.BorderImage( FCoreStyle::Get().GetBrush("ToolTip.BrightBackground") )
		.TextMargin(FMargin(11.0f))
	[
		SAssignNew(VerticalBoxWidget, SVerticalBox)
		// heading container
		+SVerticalBox::Slot()
		[
			SNew(SVerticalBox)
				.Visibility_Static(&LocalUtils::IsToolTipHeadingVisible, ThisRef)
			+SVerticalBox::Slot()
				.AutoHeight()
			[
				SNew(STextBlock)
					.TextStyle( FEditorStyle::Get(), "Documentation.SDocumentationTooltipSubdued")
					.Text(this, &SGraphNodeK2Base::GetToolTipHeading)
			]
			+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(0.f, 2.f, 0.f, 5.f)
			[
				SNew(SBorder)
				// use the border's padding to actually create the horizontal line
				.Padding(1.f)
				.BorderImage(FEditorStyle::GetBrush(TEXT("Menu.Separator")))
			]
		]
		// tooltip body
		+SVerticalBox::Slot()
			.AutoHeight()
		[
			DefaultToolTip->GetContentWidget()
		]
	];

	// English speakers have no real need to know this exists.
	if(FInternationalization::Get().GetCurrentCulture()->GetTwoLetterISOLanguageName() != TEXT("en"))
	{
		struct Local
		{
			static EVisibility GetNativeNodeNameVisibility()
			{
				return FSlateApplication::Get().GetModifierKeys().IsAltDown()? EVisibility::Collapsed : EVisibility::Visible;
			}
		};

		VerticalBoxWidget->AddSlot()
			.AutoHeight()
			.HAlign( HAlign_Right )
			[
				SNew( STextBlock )
				.ColorAndOpacity( FSlateColor::UseSubduedForeground() )
				.Text( LOCTEXT( "NativeNodeName", "hold (Alt) for native node name" ) )
				.TextStyle( &FEditorStyle::GetWidgetStyle<FTextBlockStyle>(TEXT("Documentation.SDocumentationTooltip")) )
				.Visibility_Static(&Local::GetNativeNodeNameVisibility)
			];
	}
	return NodeToolTip;
}

FText SGraphNodeK2Base::GetToolTipHeading() const
{
	if (UK2Node const* K2Node = CastChecked<UK2Node>(GraphNode))
	{
		return K2Node->GetToolTipHeading();
	}
	return FText::GetEmpty();
}

/**
 * Update this GraphNode to match the data that it is observing
 */
void SGraphNodeK2Base::UpdateGraphNode()
{
	UK2Node* K2Node = CastChecked<UK2Node>(GraphNode);
	const bool bCompactMode = K2Node->ShouldDrawCompact();

	if (bCompactMode)	
	{
		UpdateCompactNode();
	}
	else
	{
		UpdateStandardNode();
	}
}

bool SGraphNodeK2Base::RequiresSecondPassLayout() const
{
	UK2Node* K2Node = CastChecked<UK2Node>(GraphNode);
	const bool bBeadMode = K2Node->ShouldDrawAsBead();

	return bBeadMode;
}

FText SGraphNodeK2Base::GetNodeCompactTitle() const
{
	UK2Node* K2Node = CastChecked<UK2Node>(GraphNode);
	return K2Node->GetCompactNodeTitle();
}

/** Populate the brushes array with any overlay brushes to render */
void SGraphNodeK2Base::GetOverlayBrushes(bool bSelected, const FVector2D WidgetSize, TArray<FOverlayBrushInfo>& Brushes) const
{
	UBlueprint* OwnerBlueprint = FBlueprintEditorUtils::FindBlueprintForNode(GraphNode);

	// Search for an enabled or disabled breakpoint on this node
	UBreakpoint* Breakpoint = OwnerBlueprint ? FKismetDebugUtilities::FindBreakpointForNode(OwnerBlueprint, GraphNode) : nullptr;
	if (Breakpoint != NULL)
	{
		FOverlayBrushInfo BreakpointOverlayInfo;

		if (Breakpoint->GetLocation()->IsA<UK2Node_Composite>()
			|| Breakpoint->GetLocation()->IsA<UK2Node_MacroInstance>())
		{
			if (Breakpoint->IsEnabledByUser())
			{
				BreakpointOverlayInfo.Brush = FEditorStyle::GetBrush(FKismetDebugUtilities::IsBreakpointValid(Breakpoint) ? TEXT("Kismet.DebuggerOverlay.Breakpoint.EnabledAndValidCollapsed") : TEXT("Kismet.DebuggerOverlay.Breakpoint.EnabledAndInvalidCollapsed"));
			}
			else
			{
				BreakpointOverlayInfo.Brush = FEditorStyle::GetBrush(TEXT("Kismet.DebuggerOverlay.Breakpoint.DisabledCollapsed"));
			}
		}
		else
		{
			if (Breakpoint->IsEnabledByUser())
			{
				BreakpointOverlayInfo.Brush = FEditorStyle::GetBrush(FKismetDebugUtilities::IsBreakpointValid(Breakpoint) ? TEXT("Kismet.DebuggerOverlay.Breakpoint.EnabledAndValid") : TEXT("Kismet.DebuggerOverlay.Breakpoint.EnabledAndInvalid"));
			}
			else
			{
				BreakpointOverlayInfo.Brush = FEditorStyle::GetBrush(TEXT("Kismet.DebuggerOverlay.Breakpoint.Disabled"));
			}
		}

		if(BreakpointOverlayInfo.Brush != NULL)
		{
			BreakpointOverlayInfo.OverlayOffset -= BreakpointOverlayInfo.Brush->ImageSize/2.f;
		}

		Brushes.Add(BreakpointOverlayInfo);
	}

	// Is this the current instruction?
	if (FKismetDebugUtilities::GetCurrentInstruction() == GraphNode)
	{
		FOverlayBrushInfo IPOverlayInfo;

		// Pick icon depending on whether we are on a hit breakpoint
		const bool bIsOnHitBreakpoint = FKismetDebugUtilities::GetMostRecentBreakpointHit() == GraphNode;
		
		IPOverlayInfo.Brush = FEditorStyle::GetBrush( bIsOnHitBreakpoint ? TEXT("Kismet.DebuggerOverlay.InstructionPointerBreakpoint") : TEXT("Kismet.DebuggerOverlay.InstructionPointer") );

		if (IPOverlayInfo.Brush != NULL)
		{
			float Overlap = 10.f;
			IPOverlayInfo.OverlayOffset.X = (WidgetSize.X/2.f) - (IPOverlayInfo.Brush->ImageSize.X/2.f);
			IPOverlayInfo.OverlayOffset.Y = (Overlap - IPOverlayInfo.Brush->ImageSize.Y);
		}

		IPOverlayInfo.AnimationEnvelope = FVector2D(0.f, 10.f);

		Brushes.Add(IPOverlayInfo);
	}

	// @todo remove if Timeline nodes are rendered in their own slate widget
	if (const UK2Node_Timeline* Timeline = Cast<const UK2Node_Timeline>(GraphNode))
	{
		float Offset = 0.0f;
		if (Timeline && Timeline->bAutoPlay)
		{
			FOverlayBrushInfo IPOverlayInfo;
			IPOverlayInfo.Brush = FEditorStyle::GetBrush( TEXT("Graph.Node.Autoplay") );

			if (IPOverlayInfo.Brush != NULL)
			{
				const float Padding = 2.5f;
				IPOverlayInfo.OverlayOffset.X = WidgetSize.X - IPOverlayInfo.Brush->ImageSize.X - Padding;
				IPOverlayInfo.OverlayOffset.Y = Padding;
				Offset = IPOverlayInfo.Brush->ImageSize.X;
			}
			Brushes.Add(IPOverlayInfo);
		}
		if (Timeline && Timeline->bLoop)
		{
			FOverlayBrushInfo IPOverlayInfo;
			IPOverlayInfo.Brush = FEditorStyle::GetBrush( TEXT("Graph.Node.Loop") );

			if (IPOverlayInfo.Brush != NULL)
			{
				const float Padding = 2.5f;
				IPOverlayInfo.OverlayOffset.X = WidgetSize.X - IPOverlayInfo.Brush->ImageSize.X - Padding - Offset;
				IPOverlayInfo.OverlayOffset.Y = Padding;
			}
			Brushes.Add(IPOverlayInfo);
		}
	}

	// Display an  icon depending on the type of node and it's settings
	if (const UK2Node* K2Node = Cast<const UK2Node>(GraphNode))
	{
		FName ClientIcon = K2Node->GetCornerIcon();
		if ( ClientIcon != NAME_None )
		{
			FOverlayBrushInfo IPOverlayInfo;

			IPOverlayInfo.Brush = FEditorStyle::GetBrush( ClientIcon );

			if (IPOverlayInfo.Brush != NULL)
			{
				IPOverlayInfo.OverlayOffset.X = (WidgetSize.X - (IPOverlayInfo.Brush->ImageSize.X/2.f))-3.f;
				IPOverlayInfo.OverlayOffset.Y = (IPOverlayInfo.Brush->ImageSize.Y/-2.f)+2.f;
			}

			Brushes.Add(IPOverlayInfo);
		}
	}
}

void SGraphNodeK2Base::GetNodeInfoPopups(FNodeInfoContext* Context, TArray<FGraphInformationPopupInfo>& Popups) const
{
	FKismetNodeInfoContext* K2Context = (FKismetNodeInfoContext*)Context;

	// Display any pending latent actions
	if (UObject* ActiveObject = K2Context->ActiveObjectBeingDebugged)
	{
		TArray<FKismetNodeInfoContext::FObjectUUIDPair>* Pairs = K2Context->NodesWithActiveLatentActions.Find(GraphNode);
		if (Pairs != NULL)
		{
			for (int32 Index = 0; Index < Pairs->Num(); ++Index)
			{
				FKismetNodeInfoContext::FObjectUUIDPair Action = (*Pairs)[Index];

				if (Action.Object == ActiveObject)
				{
					if (UWorld* World = GEngine->GetWorldFromContextObject(Action.Object, EGetWorldErrorMode::ReturnNull))
					{
						FLatentActionManager& LatentActionManager = World->GetLatentActionManager();

						const FString LatentDesc = LatentActionManager.GetDescription(Action.Object, Action.UUID);
						const FString& ActorLabel = Action.GetDisplayName();

						new (Popups) FGraphInformationPopupInfo(NULL, LatentBubbleColor, LatentDesc);
					}
				}
			}
		}

		// Display pinned watches
		if (K2Context->WatchedNodeSet.Contains(GraphNode))
		{
			UBlueprint* Blueprint = K2Context->SourceBlueprint;
			const UEdGraphSchema* Schema = GraphNode->GetSchema();

			FString PinnedWatchText;
			int32 ValidWatchCount = 0;
			for (int32 PinIndex = 0; PinIndex < GraphNode->Pins.Num(); ++PinIndex)
			{
				UEdGraphPin* WatchPin = GraphNode->Pins[PinIndex];
				if (K2Context->WatchedPinSet.Contains(WatchPin))
				{
					if (ValidWatchCount > 0)
					{
						PinnedWatchText += TEXT("\n");
					}

					FString PinName = UEdGraphSchema_K2::TypeToText(WatchPin->PinType).ToString();
					PinName += TEXT(" ");
					PinName += Schema->GetPinDisplayName(WatchPin).ToString();

					FString WatchText;
					const FKismetDebugUtilities::EWatchTextResult WatchStatus = FKismetDebugUtilities::GetWatchText(/*inout*/ WatchText, Blueprint, ActiveObject, WatchPin);

					switch (WatchStatus)
					{
					case FKismetDebugUtilities::EWTR_Valid:
						PinnedWatchText += FString::Printf(*LOCTEXT("WatchingAndValid", "Watching %s\n\t%s").ToString(), *PinName, *WatchText);//@TODO: Print out object being debugged name?
						break;

					case FKismetDebugUtilities::EWTR_NotInScope:
						PinnedWatchText += FString::Printf(*LOCTEXT("WatchingWhenNotInScope", "Watching %s\n\t(not in scope)").ToString(), *PinName);
						break;

					case FKismetDebugUtilities::EWTR_NoProperty:
						PinnedWatchText += FString::Printf(*LOCTEXT("WatchingUnknownProperty", "Watching %s\n\t(no debug data)").ToString(), *PinName);
						break;

					default:
					case FKismetDebugUtilities::EWTR_NoDebugObject:
						PinnedWatchText += FString::Printf(*LOCTEXT("WatchingNoDebugObject", "Watching %s").ToString(), *PinName);
						break;
					}

					ValidWatchCount++;
				}
			}

			if (ValidWatchCount)
			{
				new (Popups) FGraphInformationPopupInfo(NULL, PinnedWatchColor, PinnedWatchText);
			}
		}
	}
}

const FSlateBrush* SGraphNodeK2Base::GetShadowBrush(bool bSelected) const
{
	const UK2Node* K2Node = CastChecked<UK2Node>(GraphNode);
	const bool bCompactMode = K2Node->ShouldDrawCompact();

	if (bSelected && bCompactMode)
	{
		return FEditorStyle::GetBrush( "Graph.VarNode.ShadowSelected" );
	}
	else
	{
		return SGraphNode::GetShadowBrush(bSelected);
	}
}

void SGraphNodeK2Base::PerformSecondPassLayout(const TMap< UObject*, TSharedRef<SNode> >& NodeToWidgetLookup) const
{
	TSet<UEdGraphNode*> PrevNodes;
	TSet<UEdGraphNode*> NextNodes;

	// Gather predecessor/successor nodes
	for (int32 PinIndex = 0; PinIndex < GraphNode->Pins.Num(); ++PinIndex)
	{
		UEdGraphPin* Pin = GraphNode->Pins[PinIndex];

		if (Pin->LinkedTo.Num() > 0)
		{
			if (Pin->Direction == EGPD_Input)
			{
				for (int32 LinkIndex = 0; LinkIndex < Pin->LinkedTo.Num(); ++LinkIndex)
				{
					PrevNodes.Add(Pin->LinkedTo[LinkIndex]->GetOwningNode());
				}
			}
			
			if (Pin->Direction == EGPD_Output)
			{
				for (int32 LinkIndex = 0; LinkIndex < Pin->LinkedTo.Num(); ++LinkIndex)
				{
					NextNodes.Add(Pin->LinkedTo[LinkIndex]->GetOwningNode());
				}
			}
		}
	}

	// Place this node smack between them
	const float Height = 0.0f;
	PositionThisNodeBetweenOtherNodes(NodeToWidgetLookup, PrevNodes, NextNodes, Height);
}



#undef LOCTEXT_NAMESPACE
