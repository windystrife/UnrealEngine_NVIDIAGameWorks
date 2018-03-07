// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SNiagaraGraphPinAdd.h"
#include "NiagaraNodeWithDynamicPins.h"

#include "ScopedTransaction.h"
#include "SComboButton.h"
#include "SBoxPanel.h"
#include "SImage.h"
#include "MultiBoxBuilder.h"
#include "SEditableTextBox.h"
#include "SInlineEditableTextBlock.h"
#include "SBox.h"

#define LOCTEXT_NAMESPACE "NiagaraGraphPinAdd"

void SNiagaraGraphPinAdd::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SetShowLabel(false);
	OwningNode = Cast<UNiagaraNodeWithDynamicPins>(InGraphPinObj->GetOwningNode());
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
	TSharedPtr<SHorizontalBox> PinBox = GetFullPinHorizontalRowWidget().Pin();
	if (PinBox.IsValid())
	{
		if (InGraphPinObj->Direction == EGPD_Input)
		{
			PinBox->AddSlot()
			[
				ConstructAddButton()
			];
		}
		else
		{
			PinBox->InsertSlot(0)
			[
				ConstructAddButton()
			];
		}
	}
}

TSharedRef<SWidget>	SNiagaraGraphPinAdd::ConstructAddButton()
{
	return SNew(SComboButton)
		.HasDownArrow(false)
		.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
		.ForegroundColor(FSlateColor::UseForeground())
		.OnGetMenuContent(this, &SNiagaraGraphPinAdd::OnGetAddButtonMenuContent)
		.ContentPadding(FMargin(2))
		.HAlign(HAlign_Center)
		.VAlign(VAlign_Center)
		.ToolTipText(LOCTEXT("AddPinButtonToolTip", "Connect this pin to add a new typed pin, or choose from the drop-down."))
		.ButtonContent()
		[
			SNew(SImage)
			.ColorAndOpacity(FSlateColor::UseForeground())
			.Image(FEditorStyle::GetBrush("Plus"))
		];
}

TSharedRef<SWidget> SNiagaraGraphPinAdd::OnGetAddButtonMenuContent()
{
	TArray<UEdGraphPin*> Pins = GetPinObj()->GetOwningNode()->GetAllPins();
	EEdGraphPinDirection PinDir = GetPinObj()->Direction;

	int32 FirstPinSameDir = Pins.IndexOfByPredicate([&](UEdGraphPin* InObj) -> bool
	{
		return InObj && (InObj->Direction == PinDir);
	});

	int32 IndexOfPin = Pins.IndexOfByPredicate([&](UEdGraphPin* InObj) -> bool
	{
		return InObj && (InObj == GetPinObj());
	});

	int PinIdx = 0;
	if (FirstPinSameDir != -1 && IndexOfPin != -1)
	{
		PinIdx = IndexOfPin - FirstPinSameDir;
	}

	FString WorkingName = FString::Printf(TEXT("%s%d"), PinDir == EEdGraphPinDirection::EGPD_Input ? TEXT("Input") : TEXT("Output"), PinIdx);

	if (OwningNode != nullptr)
	{
		return OwningNode->GenerateAddPinMenu(WorkingName, this);
	}
	else
	{
		return SNullWidget::NullWidget;
	}

}

void SNiagaraGraphPinAdd::OnAddType(FNiagaraVariable InAdd)
{
	if (OwningNode != nullptr)
	{
		FScopedTransaction AddNewPinTransaction(LOCTEXT("AddNewPinTransaction", "Add pin to node"));
		OwningNode->RequestNewTypedPin(GetPinObj()->Direction, InAdd.GetType(), InAdd.GetName().ToString());
	}
}
#undef LOCTEXT_NAMESPACE
