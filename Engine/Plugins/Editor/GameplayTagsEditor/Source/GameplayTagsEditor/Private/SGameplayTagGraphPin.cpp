// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SGameplayTagGraphPin.h"
#include "Widgets/Input/SComboButton.h"
#include "K2Node_CallFunction.h"
#include "GameplayTagsModule.h"
#include "Widgets/Layout/SScaleBox.h"

#define LOCTEXT_NAMESPACE "GameplayTagGraphPin"

void SGameplayTagGraphPin::Construct( const FArguments& InArgs, UEdGraphPin* InGraphPinObj )
{
	TagContainer = MakeShareable( new FGameplayTagContainer() );
	SGraphPin::Construct( SGraphPin::FArguments(), InGraphPinObj );
}

TSharedRef<SWidget>	SGameplayTagGraphPin::GetDefaultValueWidget()
{
	ParseDefaultValueData();

	//Create widget
	return SNew(SVerticalBox)
		+SVerticalBox::Slot()
		.AutoHeight()
		[
			SAssignNew( ComboButton, SComboButton )
			.OnGetMenuContent( this, &SGameplayTagGraphPin::GetListContent )
			.ContentPadding( FMargin( 2.0f, 2.0f ) )
			.Visibility( this, &SGraphPin::GetDefaultValueVisibility )
			.MenuPlacement(MenuPlacement_BelowAnchor)
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

void SGameplayTagGraphPin::ParseDefaultValueData()
{
	FString TagString = GraphPinObj->GetDefaultAsString();

	UK2Node_CallFunction* CallFuncNode = Cast<UK2Node_CallFunction>(GraphPinObj->GetOwningNode());
	
	FilterString.Empty();
	if (CallFuncNode)
	{
		UFunction* ThisFunction = CallFuncNode->GetTargetFunction();
		if (ThisFunction)
		{
			FilterString = UGameplayTagsManager::Get().GetCategoriesMetaFromFunction(ThisFunction);
		}
	}

	if (TagString.StartsWith(TEXT("(")) && TagString.EndsWith(TEXT(")")))
	{
		TagString = TagString.LeftChop(1);
		TagString = TagString.RightChop(1);
		TagString.Split("=", NULL, &TagString);
		if (TagString.StartsWith(TEXT("\"")) && TagString.EndsWith(TEXT("\"")))
		{
			TagString = TagString.LeftChop(1);
			TagString = TagString.RightChop(1);
		}
	}

	if (!TagString.IsEmpty())
	{
		FGameplayTag GameplayTag = FGameplayTag::RequestGameplayTag(FName(*TagString));
		TagContainer->AddTag(GameplayTag);
	}
}

TSharedRef<SWidget> SGameplayTagGraphPin::GetListContent()
{
	EditableContainers.Empty();
	EditableContainers.Add( SGameplayTagWidget::FEditableGameplayTagContainerDatum( GraphPinObj->GetOwningNode(), TagContainer.Get() ) );

	return SNew( SVerticalBox )
		+SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight( 400 )
		[
			SNew( SGameplayTagWidget, EditableContainers )
			.OnTagChanged( this, &SGameplayTagGraphPin::RefreshTagList )
			.TagContainerName( TEXT("SGameplayTagGraphPin") )
			.Visibility( this, &SGraphPin::GetDefaultValueVisibility )
			.MultiSelect(false)
			.Filter(FilterString)
		];
}

TSharedRef<SWidget> SGameplayTagGraphPin::SelectedTags()
{
	RefreshTagList();

	SAssignNew( TagListView, SListView<TSharedPtr<FString>> )
		.ListItemsSource(&TagNames)
		.SelectionMode(ESelectionMode::None)
		.OnGenerateRow(this, &SGameplayTagGraphPin::OnGenerateRow);

	return TagListView->AsShared();
}

TSharedRef<ITableRow> SGameplayTagGraphPin::OnGenerateRow(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew( STableRow< TSharedPtr<FString> >, OwnerTable )
		[
			SNew(STextBlock) .Text( FText::FromString(*Item.Get()) )
		];
}

void SGameplayTagGraphPin::RefreshTagList()
{	
	// Clear the list
	TagNames.Empty();

	// Add tags to list
	FString TagName;
	if (TagContainer.IsValid())
	{
		for (auto It = TagContainer->CreateConstIterator(); It; ++It)
		{
			TagName = It->ToString();
			TagNames.Add( MakeShareable( new FString( TagName ) ) );
		}
	}

	// Refresh the slate list
	if( TagListView.IsValid() )
	{
		TagListView->RequestListRefresh();
	}

	// Set Pin Data
	FString TagString;
	if (!TagName.IsEmpty())
	{
		TagString = TEXT("(");
		TagString += TEXT("TagName=\"");
		TagString += TagName;
		TagString += TEXT("\"");
		TagString += TEXT(")");
	}
	FString CurrentDefaultValue = GraphPinObj->GetDefaultAsString();
	if (CurrentDefaultValue.IsEmpty())
	{
		CurrentDefaultValue = FString(TEXT(""));
	}
	if (!CurrentDefaultValue.Equals(TagString))
	{
		GraphPinObj->GetSchema()->TrySetDefaultValue(*GraphPinObj, TagString);
	}
}

#undef LOCTEXT_NAMESPACE
