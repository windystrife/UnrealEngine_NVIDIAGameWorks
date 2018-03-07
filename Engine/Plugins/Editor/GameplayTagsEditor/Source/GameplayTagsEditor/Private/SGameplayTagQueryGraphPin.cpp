// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SGameplayTagQueryGraphPin.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Layout/SScaleBox.h"

#define LOCTEXT_NAMESPACE "GameplayTagQueryGraphPin"

void SGameplayTagQueryGraphPin::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	TagQuery = MakeShareable(new FGameplayTagQuery());
	SGraphPin::Construct( SGraphPin::FArguments(), InGraphPinObj );
}

TSharedRef<SWidget>	SGameplayTagQueryGraphPin::GetDefaultValueWidget()
{
	ParseDefaultValueData();

	//Create widget
	return SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew( ComboButton, SComboButton )
			.OnGetMenuContent(this, &SGameplayTagQueryGraphPin::GetListContent)
			.ContentPadding( FMargin( 2.0f, 2.0f ) )
			.Visibility( this, &SGraphPin::GetDefaultValueVisibility )
			.ButtonContent()
			[
				SNew( STextBlock )
				.Text( LOCTEXT("GameplayTagQueryWidget_Edit", "Edit") )
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			QueryDesc()
		];
}

void SGameplayTagQueryGraphPin::ParseDefaultValueData()
{
	FString const TagQueryString = GraphPinObj->GetDefaultAsString();

	UProperty* const TQProperty = FindField<UProperty>(UEditableGameplayTagQuery::StaticClass(), TEXT("TagQueryExportText_Helper"));
	if (TQProperty)
	{
		FGameplayTagQuery* const TQ = TagQuery.Get();
		TQProperty->ImportText(*TagQueryString, TQ, 0, nullptr, GLog);
	}
}

TSharedRef<SWidget> SGameplayTagQueryGraphPin::GetListContent()
{
	EditableQueries.Empty();
	EditableQueries.Add(SGameplayTagQueryWidget::FEditableGameplayTagQueryDatum(GraphPinObj->GetOwningNode(), TagQuery.Get(), &TagQueryExportText));

	return SNew( SVerticalBox )
		+SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight( 400 )
		[
			SNew(SScaleBox)
			.HAlign(EHorizontalAlignment::HAlign_Left)
			.VAlign(EVerticalAlignment::VAlign_Top)
			.StretchDirection(EStretchDirection::DownOnly)
			.Stretch(EStretch::ScaleToFit)
			.Content()
			[
				SNew(SGameplayTagQueryWidget, EditableQueries)
				.OnQueryChanged(this, &SGameplayTagQueryGraphPin::OnQueryChanged)
				.Visibility( this, &SGraphPin::GetDefaultValueVisibility )
				.AutoSave(true)
			]
		];
}

void SGameplayTagQueryGraphPin::OnQueryChanged()
{
	// Set Pin Data
	GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, TagQueryExportText);
	QueryDescription = TagQuery->GetDescription();
}

TSharedRef<SWidget> SGameplayTagQueryGraphPin::QueryDesc()
{
	QueryDescription = TagQuery->GetDescription();

	return SNew(STextBlock)
		.Text(this, &SGameplayTagQueryGraphPin::GetQueryDescText)
		.AutoWrapText(true);
}

FText SGameplayTagQueryGraphPin::GetQueryDescText() const
{
	return FText::FromString(QueryDescription);
}

#undef LOCTEXT_NAMESPACE
