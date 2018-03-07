// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SGameplayAttributeGraphPin.h"
#include "Widgets/SBoxPanel.h"
#include "AttributeSet.h"
#include "SGameplayAttributeWidget.h"

#define LOCTEXT_NAMESPACE "K2Node"

void SGameplayAttributeGraphPin::Construct( const FArguments& InArgs, UEdGraphPin* InGraphPinObj )
{
	SGraphPin::Construct( SGraphPin::FArguments(), InGraphPinObj );
	LastSelectedProperty = NULL;

}

TSharedRef<SWidget>	SGameplayAttributeGraphPin::GetDefaultValueWidget()
{
	// Parse out current default value
	// It will be in the form (Attribute=/Script/<PackageName>.<ObjectName>:<PropertyName>)
	
	FString DefaultString = GraphPinObj->GetDefaultAsString();
	FGameplayAttribute DefaultAttribute;

	if (DefaultString.StartsWith(TEXT("(")) && DefaultString.EndsWith(TEXT(")")))
	{
		DefaultString = DefaultString.LeftChop(1);
		DefaultString = DefaultString.RightChop(1);

		DefaultString.Split("=", NULL, &DefaultString);

		DefaultString = DefaultString.LeftChop(1);
		DefaultString = DefaultString.RightChop(1);

		DefaultAttribute.SetUProperty(FindObject<UProperty>(ANY_PACKAGE, *DefaultString));
	}

	//Create widget
	return SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SNew(SGameplayAttributeWidget)
			.OnAttributeChanged(this, &SGameplayAttributeGraphPin::OnAttributeChanged)
			.DefaultProperty(DefaultAttribute.GetUProperty())
		];
}

void SGameplayAttributeGraphPin::OnAttributeChanged(UProperty* SelectedAttribute)
{
	FString FinalValue;

	if (SelectedAttribute == nullptr)
	{
		FinalValue = FString(TEXT("()"));
	}
	else
	{
		FinalValue = FString::Printf(TEXT("(Attribute=\"%s\")"), *SelectedAttribute->GetPathName());
	}

	GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, FinalValue);


	FString DefTagString = GraphPinObj->GetDefaultAsString();

	LastSelectedProperty = SelectedAttribute;

}

#undef LOCTEXT_NAMESPACE
