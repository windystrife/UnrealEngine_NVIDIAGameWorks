// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "KismetPins/SGraphPinBool.h"
#include "Widgets/Input/SCheckBox.h"
#include "ScopedTransaction.h"

void SGraphPinBool::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget>	SGraphPinBool::GetDefaultValueWidget()
{
	return SNew(SCheckBox)
		.Style(FEditorStyle::Get(), "Graph.Checkbox")
		.IsChecked(this, &SGraphPinBool::IsDefaultValueChecked)
		.OnCheckStateChanged(this, &SGraphPinBool::OnDefaultValueCheckBoxChanged)
		.Visibility( this, &SGraphPin::GetDefaultValueVisibility );
}

ECheckBoxState SGraphPinBool::IsDefaultValueChecked() const
{
	FString CurrentValue = GraphPinObj->GetDefaultAsString();
	return CurrentValue.ToBool() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SGraphPinBool::OnDefaultValueCheckBoxChanged(ECheckBoxState InIsChecked)
{
	const FString BoolString = (InIsChecked == ECheckBoxState::Checked) ? TEXT("true") : TEXT("false");
	if(GraphPinObj->GetDefaultAsString() != BoolString)
	{
		const FScopedTransaction Transaction( NSLOCTEXT("GraphEditor", "ChangeBoolPinValue", "Change Bool Pin Value" ) );
		GraphPinObj->Modify();

		GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, BoolString);
	}
}
