// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "KismetNodes/SGraphNodeK2Composite.h"
#include "EdGraph/EdGraph.h"
#include "Widgets/SBoxPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/SToolTip.h"
#include "GraphEditorSettings.h"
#include "SCommentBubble.h"
#include "K2Node_Composite.h"
#include "SGraphPreviewer.h"
#include "IDocumentationPage.h"
#include "IDocumentation.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"

/////////////////////////////////////////////////////
// SGraphNodeK2Composite

void SGraphNodeK2Composite::Construct(const FArguments& InArgs, UK2Node_Composite* InNode)
{
	GraphNode = InNode;

	SetCursor(EMouseCursor::CardinalCross);

	UpdateGraphNode();
}

void SGraphNodeK2Composite::UpdateGraphNode()
{
	InputPins.Empty();
	OutputPins.Empty();
	
	// Reset variables that are going to be exposed, in case we are refreshing an already setup node.
	RightNodeBox.Reset();
	LeftNodeBox.Reset();

	SetupErrorReporting();
	TSharedPtr<SNodeTitle> NodeTitle = SNew(SNodeTitle, GraphNode);

	//
	//             ______________________
	//            |      TITLE AREA      |
	//            +-------+------+-------+
	//            | (>) L |      | R (>) |
	//            | (>) E |      | I (>) |
	//            | (>) F |      | G (>) |
	//            | (>) T |      | H (>) |
	//            |       |      | T (>) |
	//            |_______|______|_______|
	//
	this->ContentScale.Bind( this, &SGraphNode::GetContentScale );
	this->GetOrAddSlot( ENodeZone::Center )
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		[
			SNew(SBorder)
			.BorderImage( FEditorStyle::GetBrush( "Graph.CollapsedNode.Body" ) )
			.Padding(0)
			[
				SNew(SOverlay)
				+SOverlay::Slot()
				[
					SNew(SImage)
					.Image( FEditorStyle::GetBrush("Graph.CollapsedNode.BodyColorSpill") )
					.ColorAndOpacity( this, &SGraphNode::GetNodeTitleColor )
				]
				+SOverlay::Slot()
				[
					SNew(SVerticalBox)
					+SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Top)
					[
						SNew(SOverlay)
						+SOverlay::Slot()
						.HAlign(HAlign_Left)
						.VAlign(VAlign_Center)
						[
							SNew(SBorder)
							.BorderImage( FEditorStyle::GetBrush("NoBorder") )  // Graph.CollapsedNode.ColorSpill
							.Padding( FMargin(10,5,30,3) )
							[
								SNew(SVerticalBox)
								+SVerticalBox::Slot()
								.AutoHeight()
								.HAlign(HAlign_Fill)
								.VAlign(VAlign_Top)
								[
									SNew(SVerticalBox)
									+SVerticalBox::Slot()
										.AutoHeight()
									[
										SAssignNew(InlineEditableText, SInlineEditableTextBlock)
										.Style( FEditorStyle::Get(), "Graph.Node.NodeTitleInlineEditableText" )
										.Text( NodeTitle.Get(), &SNodeTitle::GetHeadTitle )
										.OnVerifyTextChanged(this, &SGraphNodeK2Composite::OnVerifyNameTextChanged)
										.OnTextCommitted(this, &SGraphNodeK2Composite::OnNameTextCommited)
										.IsReadOnly( this, &SGraphNodeK2Composite::IsNameReadOnly )
										.IsSelected(this, &SGraphNodeK2Composite::IsSelectedExclusively)
									]
									+SVerticalBox::Slot()
										.AutoHeight()
									[
										NodeTitle.ToSharedRef()
									]
								]
								+SVerticalBox::Slot()
								.AutoHeight()
								.Padding(1.0f)
								[
									ErrorReporting->AsWidget()
								]
							]
						]
					]
					+SVerticalBox::Slot()
					.AutoHeight()
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Top)
					[
						CreateNodeBody()
					]
				]
			]
		];
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
	.IsGraphNodeHovered( this, &SGraphNode::IsHovered );

	GetOrAddSlot( ENodeZone::TopCenter )
	.SlotOffset( TAttribute<FVector2D>( CommentBubble.Get(), &SCommentBubble::GetOffset ))
	.SlotSize( TAttribute<FVector2D>( CommentBubble.Get(), &SCommentBubble::GetSize ))
	.AllowScaling( TAttribute<bool>( CommentBubble.Get(), &SCommentBubble::IsScalingAllowed ))
	.VAlign( VAlign_Top )
	[
		CommentBubble.ToSharedRef()
	];

	CreatePinWidgets();
}

UEdGraph* SGraphNodeK2Composite::GetInnerGraph() const
{
	UK2Node_Composite* CompositeNode = CastChecked<UK2Node_Composite>(GraphNode);
	return CompositeNode->BoundGraph;
}

TSharedPtr<SToolTip> SGraphNodeK2Composite::GetComplexTooltip()
{
	if (UEdGraph* BoundGraph = GetInnerGraph())
	{
		struct LocalUtils
		{
			static bool IsInteractive()
			{
				const FModifierKeysState ModifierKeys = FSlateApplication::Get().GetModifierKeys();
				return ( ModifierKeys.IsAltDown() && ModifierKeys.IsControlDown() );
			}
		};

		TSharedPtr<SToolTip> FinalToolTip = NULL;
		TSharedPtr<SVerticalBox> Container = NULL;
		SAssignNew(FinalToolTip, SToolTip)
		.IsInteractive_Static(&LocalUtils::IsInteractive)
		[
			SAssignNew(Container, SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew( STextBlock )
				.Text(this, &SGraphNodeK2Composite::GetTooltipTextForNode)
				.Font(FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 8))
				.WrapTextAt(160.0f)
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				// Create preview for the tooltip, make sure to disable state overlays to prevent
				// PIE and read-only borders obscuring the graph
				SNew(SGraphPreviewer, BoundGraph)
				.CornerOverlayText(this, &SGraphNodeK2Composite::GetPreviewCornerText)
				.ShowGraphStateOverlay(false)
			]
		];

		// Check to see whether this node has a documentation excerpt. If it does, create a doc box for the tooltip
		TSharedRef<IDocumentationPage> DocPage = IDocumentation::Get()->GetPage(GraphNode->GetDocumentationLink(), NULL);
		if(DocPage->HasExcerpt(GraphNode->GetDocumentationExcerptName()))
		{
			Container->AddSlot()
			.AutoHeight()
			.Padding(FMargin( 0.0f, 5.0f ))
			[
				IDocumentation::Get()->CreateToolTip(FText::FromString("Documentation"), NULL, GraphNode->GetDocumentationLink(), GraphNode->GetDocumentationExcerptName())
			];
		}

		return FinalToolTip;
	}
	else
	{
		return SNew(SToolTip)
			[
				SNew(SVerticalBox)
				+SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew( STextBlock )
					.Text(NSLOCTEXT("CompositeNode", "CompositeNodeInvalidGraphMessage", "ERROR: Invalid Graph"))
					.Font(FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 8))
					.WrapTextAt(160.0f)
				]
			];
	}

}

FText SGraphNodeK2Composite::GetPreviewCornerText() const
{
	UEdGraph* BoundGraph = GetInnerGraph();
	return FText::FromString(BoundGraph->GetName());
}

FText SGraphNodeK2Composite::GetTooltipTextForNode() const
{
	return GraphNode->GetTooltipText();
}

TSharedRef<SWidget> SGraphNodeK2Composite::CreateNodeBody()
{
	if( GraphNode && GraphNode->Pins.Num() > 0 )
	{
		// Create the input and output pin areas if there are pins
		return SNew(SBorder)
			.BorderImage( FEditorStyle::GetBrush("NoBorder") )
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding( FMargin(0,3) )
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.FillWidth(1.0f)
				[
					// LEFT
					SAssignNew(LeftNodeBox, SVerticalBox)
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				[
					// RIGHT
					SAssignNew(RightNodeBox, SVerticalBox)
				]
			];
	}
	else
	{
		// Create a spacer so the node has some body to it
		return SNew(SSpacer)
			.Size(FVector2D(100.f, 50.f));
	}
}

