// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SReferenceNode.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "EdGraphNode_Reference.h"
#include "AssetThumbnail.h"
#include "Widgets/Text/SInlineEditableTextBlock.h"
#include "SCommentBubble.h"

#define LOCTEXT_NAMESPACE "ReferenceViewer"

void SReferenceNode::Construct( const FArguments& InArgs, UEdGraphNode_Reference* InNode )
{
	const int32 ThumbnailSize = 128;

	if (InNode->UsesThumbnail())
	{
		// Create a thumbnail from the graph's thumbnail pool
		TSharedPtr<FAssetThumbnailPool> AssetThumbnailPool = InNode->GetReferenceViewerGraph()->GetAssetThumbnailPool();
		AssetThumbnail = MakeShareable( new FAssetThumbnail( InNode->GetAssetData(), ThumbnailSize, ThumbnailSize, AssetThumbnailPool ) );
	}
	else if (InNode->IsPackage() || InNode->IsCollapsed())
	{
		// Just make a generic thumbnail
		AssetThumbnail = MakeShareable( new FAssetThumbnail( InNode->GetAssetData(), ThumbnailSize, ThumbnailSize, NULL ) );
	}

	GraphNode = InNode;
	SetCursor( EMouseCursor::CardinalCross );
	UpdateGraphNode();
}

void SReferenceNode::UpdateGraphNode()
{
	OutputPins.Empty();

	// Reset variables that are going to be exposed, in case we are refreshing an already setup node.
	RightNodeBox.Reset();
	LeftNodeBox.Reset();

	UpdateErrorInfo();

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
	TSharedPtr<SVerticalBox> MainVerticalBox;
	TSharedPtr<SErrorText> ErrorText;
	TSharedPtr<SNodeTitle> NodeTitle = SNew(SNodeTitle, GraphNode);

	TSharedRef<SWidget> ThumbnailWidget = SNullWidget::NullWidget;
	if ( AssetThumbnail.IsValid() )
	{
		UEdGraphNode_Reference* RefGraphNode = CastChecked<UEdGraphNode_Reference>(GraphNode);

		FAssetThumbnailConfig ThumbnailConfig;
		ThumbnailConfig.bAllowFadeIn = RefGraphNode->UsesThumbnail();
		ThumbnailConfig.bForceGenericThumbnail = !RefGraphNode->UsesThumbnail();

		ThumbnailWidget =
			SNew(SBox)
			.WidthOverride(AssetThumbnail->GetSize().X)
			.HeightOverride(AssetThumbnail->GetSize().Y)
			[
				AssetThumbnail->MakeThumbnailWidget(ThumbnailConfig)
			];
	}

	ContentScale.Bind( this, &SReferenceNode::GetContentScale );
	GetOrAddSlot( ENodeZone::Center )
	.HAlign(HAlign_Center)
	.VAlign(VAlign_Center)
	[
		SAssignNew(MainVerticalBox, SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SBorder)
			.BorderImage( FEditorStyle::GetBrush( "Graph.Node.Body" ) )
			.Padding(0)
			[
				SNew(SVerticalBox)
				. ToolTipText( this, &SReferenceNode::GetNodeTooltip )
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Top)
				[
					SNew(SOverlay)
					+SOverlay::Slot()
					[
						SNew(SImage)
						.Image( FEditorStyle::GetBrush("Graph.Node.TitleGloss") )
					]
					+SOverlay::Slot()
					.HAlign(HAlign_Left)
					.VAlign(VAlign_Center)
					[
						SNew(SBorder)
						.BorderImage( FEditorStyle::GetBrush("Graph.Node.ColorSpill") )
						// The extra margin on the right
						// is for making the color spill stretch well past the node title
						.Padding( FMargin(10,5,30,3) )
						.BorderBackgroundColor( this, &SReferenceNode::GetNodeTitleColor )
						[
							SNew(SVerticalBox)
							+SVerticalBox::Slot()
								.AutoHeight()
							[
								SAssignNew(InlineEditableText, SInlineEditableTextBlock)
								.Style( FEditorStyle::Get(), "Graph.Node.NodeTitleInlineEditableText" )
								.Text( NodeTitle.Get(), &SNodeTitle::GetHeadTitle )
								.OnVerifyTextChanged(this, &SReferenceNode::OnVerifyNameTextChanged)
								.OnTextCommitted(this, &SReferenceNode::OnNameTextCommited)
								.IsReadOnly( this, &SReferenceNode::IsNameReadOnly )
								.IsSelected(this, &SReferenceNode::IsSelectedExclusively)
							]
							+SVerticalBox::Slot()
								.AutoHeight()
							[
								NodeTitle.ToSharedRef()
							]
						]
					]
					+SOverlay::Slot()
					.VAlign(VAlign_Top)
					[
						SNew(SBorder)
						.BorderImage( FEditorStyle::GetBrush( "Graph.Node.TitleHighlight" ) )
						.Visibility(EVisibility::HitTestInvisible)			
						[
							SNew(SSpacer)
							.Size(FVector2D(20,20))
						]
					]
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.Padding(1.0f)
				[
					// POPUP ERROR MESSAGE
					SAssignNew(ErrorText, SErrorText )
					.BackgroundColor( this, &SReferenceNode::GetErrorColor )
					.ToolTipText( this, &SReferenceNode::GetErrorMsgToolTip )
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Fill)
				.VAlign(VAlign_Top)
				[
					// NODE CONTENT AREA
					SNew(SBorder)
					.BorderImage( FEditorStyle::GetBrush("NoBorder") )
					.HAlign(HAlign_Fill)
					.VAlign(VAlign_Fill)
					.Padding( FMargin(0,3) )
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							// LEFT
							SNew(SBox)
							.WidthOverride(40)
							[
								SAssignNew(LeftNodeBox, SVerticalBox)
							]
						]
						
						+SHorizontalBox::Slot()
						.VAlign(VAlign_Center)
						.HAlign(HAlign_Center)
						.FillWidth(1.0f)
						[
							// Thumbnail
							ThumbnailWidget
						]

						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							// RIGHT
							SNew(SBox)
							.WidthOverride(40)
							[
								SAssignNew(RightNodeBox, SVerticalBox)
							]
						]
					]
				]
			]
		]
	];
	// Create comment bubble if comment text is valid
	GetNodeObj()->bCommentBubbleVisible = !GetNodeObj()->NodeComment.IsEmpty();
	if( GetNodeObj()->bCommentBubbleVisible )
	{
		TSharedPtr<SCommentBubble> CommentBubble;

		SAssignNew( CommentBubble, SCommentBubble )
		.GraphNode( GraphNode )
		.Text( this, &SGraphNode::GetNodeComment )
		.ColorAndOpacity( this, &SReferenceNode::GetCommentColor );

		GetOrAddSlot( ENodeZone::TopCenter )
		.SlotOffset( TAttribute<FVector2D>( CommentBubble.Get(), &SCommentBubble::GetOffset ))
		.SlotSize( TAttribute<FVector2D>( CommentBubble.Get(), &SCommentBubble::GetSize ))
		.AllowScaling( TAttribute<bool>( CommentBubble.Get(), &SCommentBubble::IsScalingAllowed ))
		.VAlign( VAlign_Top )
		[
			CommentBubble.ToSharedRef()
		];
	}

	ErrorReporting = ErrorText;
	ErrorReporting->SetError(ErrorMsg);
	CreateBelowWidgetControls(MainVerticalBox);

	CreatePinWidgets();
}

#undef LOCTEXT_NAMESPACE
