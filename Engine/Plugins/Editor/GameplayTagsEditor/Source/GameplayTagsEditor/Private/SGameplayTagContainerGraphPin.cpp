// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SGameplayTagContainerGraphPin.h"
#include "Widgets/Input/SComboButton.h"
#include "GameplayTagsModule.h"
#include "Widgets/Layout/SScaleBox.h"

#define LOCTEXT_NAMESPACE "GameplayTagGraphPin"

void SGameplayTagContainerGraphPin::Construct(const FArguments& InArgs, UEdGraphPin* InGraphPinObj)
{
	TagContainer = MakeShareable( new FGameplayTagContainer() );
	SGraphPin::Construct( SGraphPin::FArguments(), InGraphPinObj );
}

TSharedRef<SWidget>	SGameplayTagContainerGraphPin::GetDefaultValueWidget()
{
	ParseDefaultValueData();

	//Create widget
	return SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew( ComboButton, SComboButton )
			.OnGetMenuContent(this, &SGameplayTagContainerGraphPin::GetListContent)
			.ContentPadding( FMargin( 2.0f, 2.0f ) )
			.Visibility( this, &SGraphPin::GetDefaultValueVisibility )
			.ButtonContent()
			[
				SNew( STextBlock )
				.Text( LOCTEXT("GameplayTagWidget_Edit", "Edit") )
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SelectedTags()
		];
}

void SGameplayTagContainerGraphPin::ParseDefaultValueData()
{
	FString TagString = GraphPinObj->GetDefaultAsString();

	if (TagString.StartsWith(TEXT("(")) && TagString.EndsWith(TEXT(")")))
	{
		TagString = TagString.LeftChop(1);
		TagString = TagString.RightChop(1);

		TagString.Split("=", NULL, &TagString);

		TagString = TagString.LeftChop(1);
		TagString = TagString.RightChop(1);

		FString ReadTag;
		FString Remainder;

		while (TagString.Split(TEXT(","), &ReadTag, &Remainder))
		{
			ReadTag.Split("=", NULL, &ReadTag);
			if (ReadTag.EndsWith(TEXT(")")))
			{
				ReadTag = ReadTag.LeftChop(1);
				if (ReadTag.StartsWith(TEXT("\"")) && ReadTag.EndsWith(TEXT("\"")))
				{
					ReadTag = ReadTag.LeftChop(1);
					ReadTag = ReadTag.RightChop(1);
				}
			}
			TagString = Remainder;
			FGameplayTag GameplayTag = FGameplayTag::RequestGameplayTag(FName(*ReadTag));
			TagContainer->AddTag(GameplayTag);
		}
		if (Remainder.IsEmpty())
		{
			Remainder = TagString;
		}
		if (!Remainder.IsEmpty())
		{
			Remainder.Split("=", NULL, &Remainder);
			if (Remainder.EndsWith(TEXT(")")))
			{
				Remainder = Remainder.LeftChop(1);
				if (Remainder.StartsWith(TEXT("\"")) && Remainder.EndsWith(TEXT("\"")))
				{
					Remainder = Remainder.LeftChop(1);
					Remainder = Remainder.RightChop(1);
				}
			}
			FGameplayTag GameplayTag = FGameplayTag::RequestGameplayTag(FName(*Remainder));
			TagContainer->AddTag(GameplayTag);
		}
	}
}

TSharedRef<SWidget> SGameplayTagContainerGraphPin::GetListContent()
{
	EditableContainers.Empty();
	EditableContainers.Add( SGameplayTagWidget::FEditableGameplayTagContainerDatum( GraphPinObj->GetOwningNode(), TagContainer.Get() ) );

	return SNew( SVerticalBox )
		+SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight( 400 )
		[
			SNew( SGameplayTagWidget, EditableContainers )
			.OnTagChanged(this, &SGameplayTagContainerGraphPin::RefreshTagList)
			.TagContainerName( TEXT("SGameplayTagContainerGraphPin") )
			.Visibility( this, &SGraphPin::GetDefaultValueVisibility )
		];
}

TSharedRef<SWidget> SGameplayTagContainerGraphPin::SelectedTags()
{
	RefreshTagList();

	SAssignNew( TagListView, SListView<TSharedPtr<FString>> )
		.ListItemsSource(&TagNames)
		.SelectionMode(ESelectionMode::None)
		.OnGenerateRow(this, &SGameplayTagContainerGraphPin::OnGenerateRow);

	return TagListView->AsShared();
}

TSharedRef<ITableRow> SGameplayTagContainerGraphPin::OnGenerateRow(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew( STableRow< TSharedPtr<FString> >, OwnerTable )
		[
			SNew(STextBlock) .Text( FText::FromString(*Item.Get()) )
		];
}

void SGameplayTagContainerGraphPin::RefreshTagList()
{	
	// Clear the list
	TagNames.Empty();

	// Add tags to list
	if (TagContainer.IsValid())
	{
		for (auto It = TagContainer->CreateConstIterator(); It; ++It)
		{
			FString TagName = It->ToString();
			TagNames.Add( MakeShareable( new FString( TagName ) ) );
		}
	}

	// Refresh the slate list
	if( TagListView.IsValid() )
	{
		TagListView->RequestListRefresh();
	}

	// Set Pin Data
	FString TagContainerString = TagContainer->ToString();
	FString CurrentDefaultValue = GraphPinObj->GetDefaultAsString();
	if (CurrentDefaultValue.IsEmpty())
	{
		CurrentDefaultValue = FString(TEXT("(GameplayTags=)"));
	}
	if (!CurrentDefaultValue.Equals(TagContainerString))
	{
		GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, TagContainerString);
	}
}

#undef LOCTEXT_NAMESPACE
