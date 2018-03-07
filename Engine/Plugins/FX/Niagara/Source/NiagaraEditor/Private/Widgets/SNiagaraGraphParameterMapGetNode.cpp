// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SNiagaraGraphParameterMapGetNode.h"
#include "NiagaraNodeParameterMapGet.h"
#include "SButton.h"
#include "GraphEditorSettings.h"
#include "DrawElements.h"
#include "Widgets/SBoxPanel.h"
#include "SGraphPin.h"

#define LOCTEXT_NAMESPACE "SNiagaraGraphParameterMapGetNode"


void SNiagaraGraphParameterMapGetNode::Construct(const FArguments& InArgs, UEdGraphNode* InGraphNode)
{
	BackgroundBrush = FEditorStyle::GetBrush("Graph.Pin.Background");
	BackgroundHoveredBrush = FEditorStyle::GetBrush("PlainBorder");

	GraphNode = InGraphNode;
	UpdateGraphNode();
}

void SNiagaraGraphParameterMapGetNode::AddPin(const TSharedRef<SGraphPin>& PinToAdd)
{
	PinToAdd->SetOwner(SharedThis(this));

	const UEdGraphPin* PinObj = PinToAdd->GetPinObj();
	const bool bAdvancedParameter = (PinObj != nullptr) && PinObj->bAdvancedView;
	if (bAdvancedParameter)
	{
		PinToAdd->SetVisibility(TAttribute<EVisibility>(PinToAdd, &SGraphPin::IsPinVisibleAsAdvanced));
	}

	// Save the UI building for later...
	if (PinToAdd->GetDirection() == EEdGraphPinDirection::EGPD_Input)
	{
		InputPins.Add(PinToAdd);
	}
	else // Direction == EEdGraphPinDirection::EGPD_Output
	{
		OutputPins.Add(PinToAdd);
	}	
}

TSharedRef<SWidget> SNiagaraGraphParameterMapGetNode::CreateNodeContentArea()
{
	// NODE CONTENT AREA
	return SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("NoBorder"))
		.HAlign(HAlign_Fill)
		.VAlign(VAlign_Fill)
		.Padding(FMargin(0, 3))
		[
			SAssignNew(PinContainerRoot, SVerticalBox)
		];
}

void SNiagaraGraphParameterMapGetNode::CreatePinWidgets()
{
	SGraphNode::CreatePinWidgets();
	
	UNiagaraNodeParameterMapGet* GetNode = Cast<UNiagaraNodeParameterMapGet>(GraphNode);

	// Deferred pin adding to line up input/output pins by name.
	for (int32 i = 0; i < OutputPins.Num() + 1; i++)
	{
		SVerticalBox::FSlot& Slot = PinContainerRoot->AddSlot();
		Slot.AutoHeight();

		// Get nodes have an unequal number of pins. 
		TSharedPtr<SWidget> Widget;
		if (i == 0)
		{

			SAssignNew(Widget, SHorizontalBox)
				.Visibility(EVisibility::Visible)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.FillWidth(1.0f)
				.Padding(Settings->GetInputPinPadding())
				[
					(InputPins.Num() > 0 ? InputPins[0] : SNullWidget::NullWidget)
				]
			+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.Padding(Settings->GetOutputPinPadding())
				[
					SNullWidget::NullWidget
				];
		}
		else
		{
			TSharedRef<SGraphPin> OutputPin = OutputPins[i - 1];
			UEdGraphPin* SrcOutputPin = OutputPin->GetPinObj();

			UEdGraphPin* MatchingInputPin = GetNode->GetDefaultPin(SrcOutputPin);

			TSharedPtr<SWidget> InputPin = SNullWidget::NullWidget;
			for (TSharedRef<SGraphPin> Pin : InputPins)
			{
				UEdGraphPin* SrcInputPin = Pin->GetPinObj();
				if (SrcInputPin == MatchingInputPin)
				{
					InputPin = Pin;
					Pin->SetShowLabel(false);
				}
			}

			SAssignNew(Widget, SHorizontalBox)
				.Visibility(EVisibility::Visible)
				+ SHorizontalBox::Slot()
				.HAlign(HAlign_Left)
				.FillWidth(1.0f)
				.Padding(Settings->GetInputPinPadding())
				[
					InputPin.ToSharedRef()
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Right)
				.Padding(Settings->GetOutputPinPadding())
				[
					OutputPin
				];
		}

		TSharedPtr<SBorder> Border;
		SAssignNew(Border, SBorder)
			.HAlign(HAlign_Fill)
			.VAlign(VAlign_Fill)
			.Padding(FMargin(0, 3))
			[
				Widget.ToSharedRef()
			];
		Border->SetBorderImage(TAttribute<const FSlateBrush*>::Create(TAttribute<const FSlateBrush*>::FGetter::CreateRaw(this, &SNiagaraGraphParameterMapGetNode::GetBackgroundBrush, Widget)));

		Slot.AttachWidget(Border.ToSharedRef());
	}
}


const FSlateBrush* SNiagaraGraphParameterMapGetNode::GetBackgroundBrush(TSharedPtr<SWidget> Border) const
{
	return Border->IsHovered() ? BackgroundHoveredBrush	: BackgroundBrush;
}
#undef LOCTEXT_NAMESPACE
