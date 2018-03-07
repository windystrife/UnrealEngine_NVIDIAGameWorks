// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "KismetPins/SGraphPinKey.h"
#include "SKeySelector.h"
#include "ScopedTransaction.h"

void SGraphPinKey::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	TArray<FKey> KeyList;
	EKeys::GetAllKeys(KeyList);

	SelectedKey = FKey(*InGraphPinObj->GetDefaultAsString());

	if (!SelectedKey.IsValid() && InGraphPinObj->Direction == EEdGraphPinDirection::EGPD_Input)
	{
		// Ensure first valid key is always set by default
		SelectedKey = KeyList[0];
		InGraphPinObj->GetSchema()->TrySetDefaultValue(*InGraphPinObj, SelectedKey.ToString());
	}

	SGraphPin::Construct(SGraphPin::FArguments(), InGraphPinObj);
}

TSharedRef<SWidget>	SGraphPinKey::GetDefaultValueWidget()
{
	return SNew(SKeySelector)
		.Visibility(this, &SGraphPin::GetDefaultValueVisibility)
		.CurrentKey(this, &SGraphPinKey::GetCurrentKey)
		.OnKeyChanged(this, &SGraphPinKey::OnKeyChanged);
}

TOptional<FKey> SGraphPinKey::GetCurrentKey() const
{
	return SelectedKey;
}

void SGraphPinKey::OnKeyChanged(TSharedPtr<FKey> InSelectedKey)
{
	if(SelectedKey != *InSelectedKey.Get())
	{
		const FScopedTransaction Transaction( NSLOCTEXT("GraphEditor", "ChangeKeyPinValue", "Change Key Pin Value" ) );
		GraphPinObj->Modify();

		SelectedKey = *InSelectedKey.Get();
		GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, SelectedKey.ToString());
	}
}
