// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SAnimTimingPanel.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Layout/WidgetPath.h"
#include "Framework/Application/MenuStack.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/SToolTip.h"
#include "Animation/AnimMontage.h"
#include "Preferences/PersonaOptions.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "STimingTrack.h"

#define LOCTEXT_NAMESPACE "AnimTimingPanel"

namespace AnimTimingConstants
{
	static const float DefaultNodeSize = 18.0f;
	static const int32 FontSize = 10;
}

void SAnimTimingNode::Construct(const FArguments& InArgs)
{
	Element = InArgs._InElement;

	const FSlateBrush* StyleInfo = FEditorStyle::GetBrush("ProgressBar.Background");
	static FSlateFontInfo LabelFont( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), AnimTimingConstants::FontSize);

	UPersonaOptions* EditorOptions = UPersonaOptions::StaticClass()->GetDefaultObject<UPersonaOptions>();
	check(EditorOptions);

	// Pick the colour of the node from the type of the element
	FLinearColor NodeColour = FLinearColor::White;
	switch(Element->GetType())
	{
		case ETimingElementType::QueuedNotify:
		case ETimingElementType::NotifyStateBegin:
		case ETimingElementType::NotifyStateEnd:
		{
			NodeColour = EditorOptions->NotifyTimingNodeColor;
			break;
		}
		case ETimingElementType::BranchPointNotify:
		{
			NodeColour = EditorOptions->BranchingPointTimingNodeColor;
			break;
		}
		case ETimingElementType::Section:
		{
			NodeColour = EditorOptions->SectionTimingNodeColor;
			break;
		}
		default:
			break;
	}

	this->ChildSlot
	[
		SNew(SBorder)
		.BorderImage(StyleInfo)
		.BorderBackgroundColor(NodeColour)
		[
			SNew(STextBlock)
			.Justification(ETextJustify::Center)
			.Text(FText::AsNumber(Element->TriggerIdx))
			.Font(LabelFont)
			.ColorAndOpacity(FSlateColor(FLinearColor::Black))
		]
	];

	if(InArgs._bUseTooltip)
	{
		// Add asset registry tags to a text list; except skeleton as that is implied in Persona
		TSharedRef<SVerticalBox> DescriptionBox = SNew(SVerticalBox);
		TMap<FString, FText> DescriptionItems;
		Element->GetDescriptionItems(DescriptionItems);
		for(TPair<FString, FText> ItemPair : DescriptionItems)
		{
			DescriptionBox->AddSlot()
				.AutoHeight()
				.Padding(0, 0, 3, 0)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(FText::Format(LOCTEXT("Item", "{0} :"), FText::FromString(ItemPair.Key)))
						.ColorAndOpacity(FSlateColor::UseSubduedForeground())
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(ItemPair.Value)
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				];
		}

		// Tooltip
		TSharedRef<SToolTip> NodeToolTip = SNew(SToolTip)
			.TextMargin(1)
			.BorderImage(FEditorStyle::GetBrush("ContentBrowser.TileViewToolTip.ToolTipBorder"))
			[
				SNew(SBorder)
				.Padding(3)
				.BorderImage(FEditorStyle::GetBrush("ContentBrowser.TileViewTooltip.NonContentBorder"))
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0, 0, 0, 3)
					[
						SNew(SBorder)
						.Padding(6)
						.BorderImage(FEditorStyle::GetBrush("ContentBrowser.TileViewTooltip.ContentBorder"))
						[
							SNew(SBox)
							.HAlign(HAlign_Center)
							[
								SNew(STextBlock)
								.Text(FText::FromName(Element->GetTypeName()))
								.Font(FEditorStyle::GetFontStyle("ContentBrowser.TileViewTooltip.NameFont"))
							]
						]
					]

					+ SVerticalBox::Slot()
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(SBorder)
							.Padding(3)
							.BorderImage(FEditorStyle::GetBrush("ContentBrowser.TileViewTooltip.ContentBorder"))
							[
								DescriptionBox
							]
						]
					]
				]
			];
		SetToolTip(NodeToolTip);
	}
}

FVector2D SAnimTimingNode::ComputeDesiredSize(float) const
{
	// Desired height is always the same (a little less than the track height) but the width depends on the text we display
	const TSharedRef< FSlateFontMeasure > FontMeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
	static FSlateFontInfo LabelFont( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 10 );
	float TextWidth = FontMeasureService->Measure(FString::FromInt(Element->TriggerIdx), LabelFont).X;
	return FVector2D(FMath::Max(AnimTimingConstants::DefaultNodeSize, TextWidth), AnimTimingConstants::DefaultNodeSize);
}

void SAnimTimingTrackNode::Construct(const FArguments& InArgs)
{
	TAttribute<float> TimeAttr = TAttribute<float>::Create(TAttribute<float>::FGetter::CreateSP(InArgs._Element.ToSharedRef(), &FTimingRelevantElementBase::GetElementTime));

	STrackNode::Construct(STrackNode::FArguments()
		.ViewInputMin(InArgs._ViewInputMin)
		.ViewInputMax(InArgs._ViewInputMax)
		.DataStartPos(TimeAttr)
		.NodeName(InArgs._NodeName)
		.CenterOnPosition(true)
		.AllowDrag(false)
		.OverrideContent()
			[
				SNew(SBox)
				.HAlign(HAlign_Center)
				[
					SNew(SAnimTimingNode)
					.InElement(InArgs._Element)
					.bUseTooltip(InArgs._bUseTooltip)
				]
			]
		);
}

void SAnimTimingPanel::Construct(const FArguments& InArgs, FSimpleMulticastDelegate& OnAnimNotifiesChanged, FSimpleMulticastDelegate& OnSectionsChanged)
{
	SAnimTrackPanel::Construct(SAnimTrackPanel::FArguments()
		.WidgetWidth(InArgs._WidgetWidth)
		.ViewInputMin(InArgs._ViewInputMin)
		.ViewInputMax(InArgs._ViewInputMax)
		.InputMin(InArgs._InputMin)
		.InputMax(InArgs._InputMax)
		.OnSetInputViewRange(InArgs._OnSetInputViewRange));

	AnimSequence = InArgs._InSequence;

	check(AnimSequence);

	this->ChildSlot
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.FillHeight(1)
			[
				SNew(SExpandableArea)
				.AreaTitle(LOCTEXT("ExpandLabel", "Element Timing"))
				.BodyContent()
				[
					SAssignNew(PanelArea, SBorder)
					.BorderImage(FEditorStyle::GetBrush("NoBorder"))
					.Padding(FMargin(2.0f, 2.0f))
					.ColorAndOpacity(FLinearColor::White)
				]
			]
		];

	Update();

	// Register to some delegates to update the interface
	OnAnimNotifiesChanged.Add(FSimpleDelegate::CreateSP(this, &SAnimTimingPanel::RefreshTrackNodes));
	OnSectionsChanged.Add(FSimpleDelegate::CreateSP(this, &SAnimTimingPanel::RefreshTrackNodes));

	// Clear display flags
	FMemory::Memset(bElementNodeDisplayFlags, false, sizeof(bElementNodeDisplayFlags));
}

void SAnimTimingPanel::Update()
{
	check(PanelArea.IsValid());

	TSharedPtr<SVerticalBox> TimingSlots;
	
	PanelArea->SetContent(SAssignNew(TimingSlots, SVerticalBox));

	TSharedRef<S2ColumnWidget> TrackContainer = Create2ColumnWidget(TimingSlots.ToSharedRef());
	TrackContainer->LeftColumn->ClearChildren();
	TrackContainer->LeftColumn->AddSlot()
		.AutoHeight()
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.5f, 0.5f))
		[
			SAssignNew(Track, STimingTrack)
			.ViewInputMin(ViewInputMin)
			.ViewInputMax(ViewInputMax)
			.TrackMinValue(InputMin)
			.TrackMaxValue(InputMax)
			.TrackNumDiscreteValues(AnimSequence->GetNumberOfFrames())
		];

	TrackContainer->RightColumn->ClearChildren();
	TrackContainer->RightColumn->AddSlot()
		.AutoHeight()
		.VAlign(VAlign_Center)
		.Padding(FMargin(0.5f, 0.5f))
		[
			SNew(SBox)
			.HAlign(HAlign_Center)
			[
				SNew(SButton)
				.ToolTipText(LOCTEXT("TrackOptionsToolTip", "Display track options menu"))
				.OnClicked(this, &SAnimTimingPanel::OnContextMenu)
				.Content()
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("ComboButton.Arrow"))
					.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
		];

	RefreshTrackNodes();
}

void SAnimTimingPanel::Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime)
{
	SAnimTrackPanel::Tick(AllottedGeometry, InCurrentTime, InDeltaTime);
}

void SAnimTimingPanel::RefreshTrackNodes()
{
	Elements.Empty();
	GetTimingRelevantElements(AnimSequence, Elements);

	Track->ClearTrack();

	int32 NumElements = Elements.Num();
	for(int32 ElementIdx = 0 ; ElementIdx < NumElements ; ++ElementIdx)
	{
		TSharedPtr<FTimingRelevantElementBase> Element = Elements[ElementIdx];

		Track->AddTrackNode(
			SNew(SAnimTimingTrackNode)
			.ViewInputMin(ViewInputMin)
			.ViewInputMax(ViewInputMax)
			.DataStartPos(Element.ToSharedRef(), &FTimingRelevantElementBase::GetElementTime)
			.NodeName(FString::FromInt(ElementIdx + 1))
			.NodeColor(FLinearColor::Yellow)
			.Element(Element)
			);
	}
}

FReply SAnimTimingPanel::OnContextMenu()
{
	FMenuBuilder Builder(true, nullptr);

	Builder.BeginSection("TimingPanelOptions", LOCTEXT("TimingPanelOptionsHeader", "Options"));
	{
		Builder.AddWidget
		(
			SNew(SCheckBox)
			.IsChecked(this, &SAnimTimingPanel::IsElementDisplayChecked, ETimingElementType::Section)
			.OnCheckStateChanged(this, &SAnimTimingPanel::OnElementDisplayEnabledChanged, ETimingElementType::Section)
			.ToolTipText(LOCTEXT("ShowSectionTimingNodes", "Show or hide the timing display for sections on the section name track"))
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ToggleTimingNodes_Sections", "Show Section Timing Nodes"))
			],
			FText()
		);

		Builder.AddWidget
		(
			SNew(SCheckBox)
			.IsChecked(this, &SAnimTimingPanel::IsElementDisplayChecked, ETimingElementType::QueuedNotify)
			.OnCheckStateChanged(this, &SAnimTimingPanel::OnElementDisplayEnabledChanged, ETimingElementType::QueuedNotify)
			.ToolTipText(LOCTEXT("ShowNotifyTimingNodes", "Show or hide the timing display for notifies in the notify panel"))
			.HAlign(HAlign_Left)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("ToggleTimingNodes_Notifies", "Show Notify Timing Nodes"))
			],
			FText()
		);
	}
	Builder.EndSection();

	FSlateApplication::Get().PushMenu(SharedThis(this),
		FWidgetPath(),
		Builder.MakeWidget(),
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu));

	return FReply::Handled();
}

bool SAnimTimingPanel::IsElementDisplayEnabled(ETimingElementType::Type ElementType) const
{
	return bElementNodeDisplayFlags[ElementType];
}

void SAnimTimingPanel::OnElementDisplayEnabledChanged(ECheckBoxState State, ETimingElementType::Type ElementType)
{
	bElementNodeDisplayFlags[ElementType] = State == ECheckBoxState::Checked ? true : false;
}

ECheckBoxState SAnimTimingPanel::IsElementDisplayChecked(ETimingElementType::Type ElementType) const
{
	return bElementNodeDisplayFlags[ElementType] ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

EVisibility SAnimTimingPanel::IsElementDisplayVisible(ETimingElementType::Type ElementType) const
{
	return bElementNodeDisplayFlags[ElementType] ? EVisibility::Visible : EVisibility::Hidden;
}

void SAnimTimingPanel::GetTimingRelevantElements(UAnimSequenceBase* Sequence, TArray<TSharedPtr<FTimingRelevantElementBase>>& Elements)
{
	if(Sequence)
	{
		// Grab notifies
		int32 NumNotifies = Sequence->Notifies.Num();
		for(int32 NotifyIdx = 0 ; NotifyIdx < NumNotifies ; ++NotifyIdx)
		{
			FAnimNotifyEvent& Notify = Sequence->Notifies[NotifyIdx];

			FTimingRelevantElement_Notify* Element = new FTimingRelevantElement_Notify;
			Element->Sequence = Sequence;
			Element->NotifyIndex = NotifyIdx;
			Elements.Add(TSharedPtr<FTimingRelevantElementBase>(Element));

			if(Notify.NotifyStateClass)
			{
				// Add the end marker
				FTimingRelevantElement_NotifyStateEnd* EndElement = new FTimingRelevantElement_NotifyStateEnd;
				EndElement->Sequence = Sequence;
				EndElement->NotifyIndex = NotifyIdx;
				Elements.Add(TSharedPtr<FTimingRelevantElementBase>(EndElement));
			}
		}

		// Check for a montage and extract Montage elements
		if(UAnimMontage* Montage = Cast<UAnimMontage>(Sequence))
		{
			// Add sections
			int32 NumSections = Montage->CompositeSections.Num();

			for(int32 SectionIdx = 0 ; SectionIdx < NumSections ; ++SectionIdx)
			{
				FCompositeSection& Section = Montage->CompositeSections[SectionIdx];

				FTimingRelevantElement_Section* Element = new FTimingRelevantElement_Section;
				Element->Montage = Montage;
				Element->SectionIdx = SectionIdx;
				Elements.Add(TSharedPtr<FTimingRelevantElementBase>(Element));
			}
		}

		// Sort everything and give them trigger orders
		Elements.Sort([](const TSharedPtr<FTimingRelevantElementBase>& A, const TSharedPtr<FTimingRelevantElementBase>& B)
		{
			return A->Compare(*B);
		});

		int32 NumElements = Elements.Num();
		for(int32 Idx = 0 ; Idx < NumElements ; ++Idx)
		{
			Elements[Idx]->TriggerIdx = Idx;
		}
	}
}

FName FTimingRelevantElement_Section::GetTypeName()
{
	return FName("Montage Section");
}

float FTimingRelevantElement_Section::GetElementTime() const
{
	check(Montage)
	if(Montage->CompositeSections.IsValidIndex(SectionIdx))
	{
		return Montage->CompositeSections[SectionIdx].GetTime();
	}
	return -1.0f;
}

ETimingElementType::Type FTimingRelevantElement_Section::GetType()
{
	return ETimingElementType::Section;
}

void FTimingRelevantElement_Section::GetDescriptionItems(TMap<FString, FText>& Items)
{
	check(Montage);
	FCompositeSection& Section = Montage->CompositeSections[SectionIdx];

	FNumberFormattingOptions NumberOptions;
	NumberOptions.MinimumFractionalDigits = 3;

	Items.Add(LOCTEXT("SectionName", "Name").ToString(), FText::FromName(Section.SectionName));
	Items.Add(LOCTEXT("SectionTriggerTime", "Trigger Time").ToString(), FText::Format(LOCTEXT("SectionTriggerTimeValue", "{0}s"), FText::AsNumber(Section.GetTime(), &NumberOptions)));
}

FName FTimingRelevantElement_Notify::GetTypeName()
{
	FName TypeName;
	switch(GetType())
	{
		case ETimingElementType::NotifyStateBegin:
			TypeName = FName("Notify State (Begin)");
			break;
		case ETimingElementType::BranchPointNotify:
			TypeName = FName("Branching Point");
			break;
		default:
			TypeName = FName("Notify");
	}

	return TypeName;
}

float FTimingRelevantElement_Notify::GetElementTime() const
{
	check(Sequence);
	if(Sequence->Notifies.IsValidIndex(NotifyIndex))
	{
		return Sequence->Notifies[NotifyIndex].GetTriggerTime();
	}
	return -1.0f;
}

ETimingElementType::Type FTimingRelevantElement_Notify::GetType()
{
	check(Sequence);
	FAnimNotifyEvent& Event = Sequence->Notifies[NotifyIndex];

	if(Event.IsBranchingPoint())
	{
		return ETimingElementType::BranchPointNotify;
	}
	else
	{
		if(Event.NotifyStateClass)
		{
			return ETimingElementType::NotifyStateBegin;
		}
		else
		{
			return ETimingElementType::QueuedNotify;
		}
	}
}

void FTimingRelevantElement_Notify::GetDescriptionItems(TMap<FString, FText>& Items)
{
	check(Sequence);
	FAnimNotifyEvent& Event = Sequence->Notifies[NotifyIndex];

	FNumberFormattingOptions NumberOptions;
	NumberOptions.MinimumFractionalDigits = 3;

	Items.Add(LOCTEXT("NotifyName", "Name").ToString(), FText::FromName(Event.NotifyName));
	Items.Add(LOCTEXT("NotifyTriggerTime", "Trigger Time").ToString(), FText::Format(LOCTEXT("NotifyTriggerTime_Val", "{0}s"), FText::AsNumber(Event.GetTime(), &NumberOptions)));

	// +1 as we start at 1 when showing tracks to the user
	Items.Add(LOCTEXT("TrackIdx", "Track").ToString(), FText::AsNumber(Event.TrackIndex + 1));

	if(Event.NotifyStateClass)
	{
		Items.Add(LOCTEXT("NotifyDuration", "Duration").ToString(), FText::Format(LOCTEXT("NotifyDuration_Val", "{0}s"), FText::AsNumber(Event.GetDuration(), &NumberOptions)));
	}
}

int32 FTimingRelevantElement_Notify::GetElementSortPriority() const
{
	check(Sequence);
	FAnimNotifyEvent& Event = Sequence->Notifies[NotifyIndex];
	return Event.TrackIndex;
}

FName FTimingRelevantElement_NotifyStateEnd::GetTypeName()
{
	return FName("Notify State (End)");
}

float FTimingRelevantElement_NotifyStateEnd::GetElementTime() const
{
	check(Sequence);
	FAnimNotifyEvent& Event = Sequence->Notifies[NotifyIndex];
	check(Event.NotifyStateClass);
	return Event.GetEndTriggerTime();
}

ETimingElementType::Type FTimingRelevantElement_NotifyStateEnd::GetType()
{
	return ETimingElementType::NotifyStateEnd;
}

#undef LOCTEXT_NAMESPACE
