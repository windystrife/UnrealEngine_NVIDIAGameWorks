// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SAnimationOutlinerTreeNode.h"
#include "Fonts/SlateFontInfo.h"
#include "Types/SlateStructs.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SSpacer.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Views/SExpanderArrow.h"
#include "EditorStyleSet.h"
#include "DisplayNodes/SequencerTrackNode.h"
#include "SSequencer.h"
#include "ScopedTransaction.h"
#include "Widgets/Input/SEditableLabel.h"
#include "SSequencerTreeView.h"
#include "Widgets/Colors/SColorPicker.h"
#include "SequencerSectionPainter.h"

#define LOCTEXT_NAMESPACE "AnimationOutliner"

class STrackColorPicker : public SCompoundWidget
{
public:
	static void OnOpen()
	{
		if (!Transaction.IsValid())
		{
			Transaction.Reset(new FScopedTransaction(LOCTEXT("ChangeTrackColor", "Change Track Color")));
		}
	}

	static void OnClose()
	{
		if (!Transaction.IsValid())
		{
			return;
		}

		if (!bMadeChanges)
		{
			Transaction->Cancel();
		}
		Transaction.Reset();
	}

	SLATE_BEGIN_ARGS(STrackColorPicker){}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, UMovieSceneTrack* InTrack)
	{
		bMadeChanges = false;
		Track = InTrack;
		ChildSlot
		[
			SNew(SColorPicker)
			.DisplayInlineVersion(true)
			.TargetColorAttribute(this, &STrackColorPicker::GetTrackColor)
			.OnColorCommitted(this, &STrackColorPicker::SetTrackColor)
		];
	}

	FLinearColor GetTrackColor() const
	{
		return Track->GetColorTint().ReinterpretAsLinear();
	}

	void SetTrackColor(FLinearColor NewColor)
	{
		bMadeChanges = true;
		Track->Modify();
		Track->SetColorTint(NewColor.ToFColor(false));
	}

private:
	UMovieSceneTrack* Track;
	static TUniquePtr<FScopedTransaction> Transaction;
	static bool bMadeChanges;
};

TUniquePtr<FScopedTransaction> STrackColorPicker::Transaction;
bool STrackColorPicker::bMadeChanges = false;

SAnimationOutlinerTreeNode::~SAnimationOutlinerTreeNode()
{
	DisplayNode->OnRenameRequested().RemoveAll(this);
}

void SAnimationOutlinerTreeNode::Construct( const FArguments& InArgs, TSharedRef<FSequencerDisplayNode> Node, const TSharedRef<SSequencerTreeViewRow>& InTableRow )
{
	DisplayNode = Node;
	bIsOuterTopLevelNode = !Node->GetParent().IsValid();
	bIsInnerTopLevelNode = Node->GetType() != ESequencerNode::Folder && Node->GetParent().IsValid() && Node->GetParent()->GetType() == ESequencerNode::Folder;

	if (bIsOuterTopLevelNode)
	{
		ExpandedBackgroundBrush = FEditorStyle::GetBrush( "Sequencer.AnimationOutliner.TopLevelBorder_Expanded" );
		CollapsedBackgroundBrush = FEditorStyle::GetBrush( "Sequencer.AnimationOutliner.TopLevelBorder_Collapsed" );
	}
	else
	{
		ExpandedBackgroundBrush = FEditorStyle::GetBrush( "Sequencer.AnimationOutliner.DefaultBorder" );
		CollapsedBackgroundBrush = FEditorStyle::GetBrush( "Sequencer.AnimationOutliner.DefaultBorder" );
	}

	FMargin InnerNodePadding;
	if ( bIsInnerTopLevelNode )
	{
		InnerBackgroundBrush = FEditorStyle::GetBrush( "Sequencer.AnimationOutliner.TopLevelBorder_Expanded" );
		InnerNodePadding = FMargin(0.f, 1.f);
	}
	else
	{
		InnerBackgroundBrush = FEditorStyle::GetBrush( "Sequencer.AnimationOutliner.TransparentBorder" );
		InnerNodePadding = FMargin(0.f);
	}

	TableRowStyle = &FEditorStyle::Get().GetWidgetStyle<FTableRowStyle>("TableView.Row");

	FSlateFontInfo NodeFont = FEditorStyle::GetFontStyle("Sequencer.AnimationOutliner.RegularFont");

	EditableLabel = SNew(SEditableLabel)
		.CanEdit(this, &SAnimationOutlinerTreeNode::HandleNodeLabelCanEdit)
		.Font(NodeFont)
		.ColorAndOpacity(this, &SAnimationOutlinerTreeNode::GetDisplayNameColor)
		.OnTextChanged(this, &SAnimationOutlinerTreeNode::HandleNodeLabelTextChanged)
		.Text(this, &SAnimationOutlinerTreeNode::GetDisplayName)
		.ToolTipText(this, &SAnimationOutlinerTreeNode::GetDisplayNameToolTipText)
		.Clipping(EWidgetClipping::ClipToBounds);


	Node->OnRenameRequested().AddRaw(this, &SAnimationOutlinerTreeNode::EnterRenameMode);

	auto NodeHeight = [=]() -> FOptionalSize { return DisplayNode->GetNodeHeight(); };

	ForegroundColor.Bind(this, &SAnimationOutlinerTreeNode::GetForegroundBasedOnSelection);

	TSharedRef<SWidget>	FinalWidget = 
		SNew( SBorder )
		.VAlign( VAlign_Center )
		.BorderImage( this, &SAnimationOutlinerTreeNode::GetNodeBorderImage )
		.BorderBackgroundColor( this, &SAnimationOutlinerTreeNode::GetNodeBackgroundTint )
		.Padding(FMargin(0, Node->GetNodePadding().Combined() / 2))
		[
			SNew( SHorizontalBox )

			+ SHorizontalBox::Slot()
			[
				SNew(SBox)
				.HeightOverride_Lambda(NodeHeight)
				.Padding(FMargin(5.0f, 0.0f))
				[
					SNew( SHorizontalBox )

					// Expand track lanes button
					+ SHorizontalBox::Slot()
					.Padding(FMargin(2.f, 0.f, 2.f, 0.f))
					.VAlign( VAlign_Center )
					.AutoWidth()
					[
						SNew(SExpanderArrow, InTableRow).IndentAmount(SequencerLayoutConstants::IndentAmount)
					]

					+ SHorizontalBox::Slot()
					.Padding( InnerNodePadding )
					[
						SNew( SBorder )
						.BorderImage( FEditorStyle::GetBrush( "Sequencer.AnimationOutliner.TopLevelBorder_Collapsed" ) )
						.BorderBackgroundColor( this, &SAnimationOutlinerTreeNode::GetNodeInnerBackgroundTint )
						.Padding( FMargin(0) )
						[
							SNew( SHorizontalBox )
							
							// Icon
							+ SHorizontalBox::Slot()
							.Padding(FMargin(0.f, 0.f, 4.f, 0.f))
							.VAlign(VAlign_Center)
							.AutoWidth()
							[
								SNew(SOverlay)

								+ SOverlay::Slot()
								[
									SNew(SImage)
									.Image(InArgs._IconBrush)
									.ColorAndOpacity(InArgs._IconColor)
								]

								+ SOverlay::Slot()
								.VAlign(VAlign_Top)
								.HAlign(HAlign_Right)
								[
									SNew(SImage)
									.Image(InArgs._IconOverlayBrush)
								]

								+ SOverlay::Slot()
								[
									SNew(SSpacer)
									.Visibility(EVisibility::Visible)
									.ToolTipText(InArgs._IconToolTipText)
								]
							]

							// Label Slot
							+ SHorizontalBox::Slot()
							.VAlign(VAlign_Center)
							.Padding(FMargin(0.f, 0.f, 4.f, 0.f))
							[
								EditableLabel.ToSharedRef()
							]

							// Arbitrary customization slot
							+ SHorizontalBox::Slot()
							.AutoWidth()
							[
								InArgs._CustomContent.Widget
							]
						]
					]
				]
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SComboButton)
				.ContentPadding(0)
				.VAlign(VAlign_Fill)
				.HasDownArrow(false)
				.IsFocusable(false)
				.IsEnabled(!DisplayNode->GetSequencer().IsReadOnly())
				.ButtonStyle(FEditorStyle::Get(), "Sequencer.AnimationOutliner.ColorStrip")
				.OnGetMenuContent(this, &SAnimationOutlinerTreeNode::OnGetColorPicker)
				.OnMenuOpenChanged_Lambda([](bool bIsOpen){
					if (bIsOpen)
					{
						STrackColorPicker::OnOpen();
					}
					else if (!bIsOpen)
					{
						STrackColorPicker::OnClose();
					}
				})
				.ButtonContent()
				[
					SNew(SBox)
					.WidthOverride(6.f)
					[
						SNew(SImage)
						.Image(FEditorStyle::GetBrush("WhiteBrush"))
						.ColorAndOpacity(this, &SAnimationOutlinerTreeNode::GetTrackColorTint)
					]
				]
			]
		];

	ChildSlot
	[
		FinalWidget
	];
}


void SAnimationOutlinerTreeNode::EnterRenameMode()
{
	EditableLabel->EnterTextMode();
}


void SAnimationOutlinerTreeNode::GetAllDescendantNodes(TSharedPtr<FSequencerDisplayNode> RootNode, TArray<TSharedRef<FSequencerDisplayNode> >& AllNodes)
{
	if (!RootNode.IsValid())
	{
		return;
	}

	AllNodes.Add(RootNode.ToSharedRef());

	const FSequencerDisplayNode* RootNodeC = RootNode.Get();

	for (TSharedRef<FSequencerDisplayNode> ChildNode : RootNodeC->GetChildNodes())
	{
		AllNodes.Add(ChildNode);
		GetAllDescendantNodes(ChildNode, AllNodes);
	}
}

void SAnimationOutlinerTreeNode::OnMouseEnter(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
{
	DisplayNode->GetParentTree().SetHoveredNode(DisplayNode);
	SWidget::OnMouseEnter(MyGeometry, MouseEvent);
}

void SAnimationOutlinerTreeNode::OnMouseLeave(const FPointerEvent& MouseEvent)
{
	DisplayNode->GetParentTree().SetHoveredNode(nullptr);
	SWidget::OnMouseLeave(MouseEvent);
}

const FSlateBrush* SAnimationOutlinerTreeNode::GetNodeBorderImage() const
{
	return DisplayNode->IsExpanded() ? ExpandedBackgroundBrush : CollapsedBackgroundBrush;
}

FSlateColor SAnimationOutlinerTreeNode::GetNodeBackgroundTint() const
{
	FSequencer& Sequencer = DisplayNode->GetSequencer();
	const bool bIsSelected = Sequencer.GetSelection().IsSelected(DisplayNode.ToSharedRef());

	if (bIsSelected)
	{
		return FEditorStyle::GetSlateColor("SelectionColor_Pressed");
	}
	else if (Sequencer.GetSelection().NodeHasSelectedKeysOrSections(DisplayNode.ToSharedRef()))
	{
		return FLinearColor(FColor(115, 115, 115, 255));
	}	
	else if (DisplayNode->IsHovered())
	{
		return bIsOuterTopLevelNode ? FLinearColor(FColor(52, 52, 52, 255)) : FLinearColor(FColor(72, 72, 72, 255));
	}
	else
	{
		return bIsOuterTopLevelNode ? FLinearColor(FColor(48, 48, 48, 255)) : FLinearColor(FColor(62, 62, 62, 255));
	}
}

FSlateColor SAnimationOutlinerTreeNode::GetNodeInnerBackgroundTint() const
{
	if ( bIsInnerTopLevelNode )
	{
		FSequencer& Sequencer = DisplayNode->GetSequencer();
		const bool bIsSelected = Sequencer.GetSelection().IsSelected( DisplayNode.ToSharedRef() );

		if ( bIsSelected )
		{
			return FEditorStyle::GetSlateColor( "SelectionColor_Pressed" );
		}
		else if ( Sequencer.GetSelection().NodeHasSelectedKeysOrSections( DisplayNode.ToSharedRef() ) )
		{
			return FLinearColor( FColor( 115, 115, 115, 255 ) );
		}
		else if ( DisplayNode->IsHovered() )
		{
			return FLinearColor( FColor( 52, 52, 52, 255 ) );
		}
		else
		{
			return FLinearColor( FColor( 48, 48, 48, 255 ) );
		}
	}
	else
	{
		return FLinearColor( 0.f, 0.f, 0.f, 0.f );
	}
}

TSharedRef<SWidget> SAnimationOutlinerTreeNode::OnGetColorPicker() const
{
	UMovieSceneTrack* Track = nullptr;

	FSequencerDisplayNode* Current = DisplayNode.Get();
	while (Current && Current->GetType() != ESequencerNode::Object)
	{
		if (Current->GetType() == ESequencerNode::Track)
		{
			Track = static_cast<FSequencerTrackNode*>(Current)->GetTrack();
			if (Track)
			{
				break;
			}
		}
		Current = Current->GetParent().Get();
	}

	if (!Track)
	{
		return SNullWidget::NullWidget;
	}

	return SNew(STrackColorPicker, Track);
}

FSlateColor SAnimationOutlinerTreeNode::GetTrackColorTint() const
{
	FSequencerDisplayNode* Current = DisplayNode.Get();
	while (Current && Current->GetType() != ESequencerNode::Object)
	{
		if (Current->GetType() == ESequencerNode::Track)
		{
			UMovieSceneTrack* Track = static_cast<FSequencerTrackNode*>(Current)->GetTrack();
			if (Track)
			{
				return FSequencerSectionPainter::BlendColor(Track->GetColorTint());
			}
		}
		Current = Current->GetParent().Get();
	}

	return FLinearColor::Transparent;
}

FSlateColor SAnimationOutlinerTreeNode::GetForegroundBasedOnSelection() const
{
	FSequencer& Sequencer = DisplayNode->GetSequencer();
	const bool bIsSelected = Sequencer.GetSelection().IsSelected(DisplayNode.ToSharedRef());

	return bIsSelected ? TableRowStyle->SelectedTextColor : TableRowStyle->TextColor;
}


EVisibility SAnimationOutlinerTreeNode::GetExpanderVisibility() const
{
	return DisplayNode->GetNumChildren() > 0 ? EVisibility::Visible : EVisibility::Hidden;
}


FSlateColor SAnimationOutlinerTreeNode::GetDisplayNameColor() const
{
	return DisplayNode->GetDisplayNameColor();
}


FText SAnimationOutlinerTreeNode::GetDisplayNameToolTipText() const
{
	return DisplayNode->GetDisplayNameToolTipText();
}


FText SAnimationOutlinerTreeNode::GetDisplayName() const
{
	return DisplayNode->GetDisplayName();
}


bool SAnimationOutlinerTreeNode::HandleNodeLabelCanEdit() const
{
	return !DisplayNode->GetSequencer().IsReadOnly() && DisplayNode->CanRenameNode();
}


void SAnimationOutlinerTreeNode::HandleNodeLabelTextChanged(const FText& NewLabel)
{
	DisplayNode->SetDisplayName(NewLabel);
}


#undef LOCTEXT_NAMESPACE
