// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagContainerCustomization.h"
#include "Widgets/Input/SComboButton.h"

#include "Widgets/Input/SButton.h"


#include "Editor.h"
#include "PropertyHandle.h"
#include "DetailWidgetRow.h"
#include "ScopedTransaction.h"
#include "SHyperlink.h"

#define LOCTEXT_NAMESPACE "GameplayTagContainerCustomization"

void FGameplayTagContainerCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructPropertyHandle = InStructPropertyHandle;

	FSimpleDelegate OnTagContainerChanged = FSimpleDelegate::CreateSP(this, &FGameplayTagContainerCustomization::RefreshTagList);
	StructPropertyHandle->SetOnPropertyValueChanged(OnTagContainerChanged);

	BuildEditableContainerList();

	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		]
		.ValueContent()
		.MaxDesiredWidth(512)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SVerticalBox)
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SComboButton)
					.OnGetMenuContent(this, &FGameplayTagContainerCustomization::GetListContent)
					.ContentPadding(FMargin(2.0f, 2.0f))
					.MenuPlacement(MenuPlacement_BelowAnchor)
					.ButtonContent()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("GameplayTagContainerCustomization_Edit", "Edit..."))
					]
				]
				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SButton)
					.IsEnabled(!StructPropertyHandle->IsEditConst())
					.Text(LOCTEXT("GameplayTagContainerCustomization_Clear", "Clear All"))
					.OnClicked(this, &FGameplayTagContainerCustomization::OnClearAllButtonClicked)
					.Visibility(this, &FGameplayTagContainerCustomization::GetClearAllVisibility)
				]
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SBorder)
				.Padding(4.0f)
				.Visibility(this, &FGameplayTagContainerCustomization::GetTagsListVisibility)
				[
					ActiveTags()
				]
			]
		];

	GEditor->RegisterForUndo(this);
}

TSharedRef<SWidget> FGameplayTagContainerCustomization::ActiveTags()
{	
	RefreshTagList();
	
	SAssignNew( TagListView, SListView<TSharedPtr<FString>> )
	.ListItemsSource(&TagNames)
	.SelectionMode(ESelectionMode::None)
	.OnGenerateRow(this, &FGameplayTagContainerCustomization::MakeListViewWidget);

	return TagListView->AsShared();
}

void FGameplayTagContainerCustomization::RefreshTagList()
{
	// Rebuild Editable Containers as container references can become unsafe
	BuildEditableContainerList();

	// Clear the list
	TagNames.Empty();

	// Add tags to list
	for (int32 ContainerIdx = 0; ContainerIdx < EditableContainers.Num(); ++ContainerIdx)
	{
		FGameplayTagContainer* Container = EditableContainers[ContainerIdx].TagContainer;
		if (Container)
		{
			for (auto It = Container->CreateConstIterator(); It; ++It)
			{
				TagNames.Add(MakeShareable(new FString(It->ToString())));
			}
		}
	}

	// Refresh the slate list
	if( TagListView.IsValid() )
	{
		TagListView->RequestListRefresh();
	}
}

TSharedRef<ITableRow> FGameplayTagContainerCustomization::MakeListViewWidget(TSharedPtr<FString> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	if (UGameplayTagsManager::Get().ShowGameplayTagAsHyperLinkEditor(*Item.Get()))
	{
		return SNew( STableRow< TSharedPtr<FString> >, OwnerTable )
		[
			SNew(SHyperlink)
			.Text( FText::FromString(*Item.Get()) )
			.OnNavigate( this, &FGameplayTagContainerCustomization::OnTagDoubleClicked, *Item.Get() )
		];

	}


	return SNew( STableRow< TSharedPtr<FString> >, OwnerTable )
	[
		SNew(STextBlock) .Text( FText::FromString(*Item.Get()) )
	];
}

void FGameplayTagContainerCustomization::OnTagDoubleClicked(FString TagName)
{
	UGameplayTagsManager::Get().NotifyGameplayTagDoubleClickedEditor(TagName);
}

TSharedRef<SWidget> FGameplayTagContainerCustomization::GetListContent()
{
	if (!StructPropertyHandle.IsValid() || StructPropertyHandle->GetProperty() == nullptr)
	{
		return SNullWidget::NullWidget;
	}

	FString Categories = UGameplayTagsManager::Get().GetCategoriesMetaFromPropertyHandle(StructPropertyHandle);

	TArray<UObject*> OuterObjects;
	StructPropertyHandle->GetOuterObjects(OuterObjects);

	bool bReadOnly = StructPropertyHandle->IsEditConst();

	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.MaxHeight(400)
		[
			SNew(SGameplayTagWidget, EditableContainers)
			.Filter(Categories)
			.ReadOnly(bReadOnly)
			.TagContainerName(StructPropertyHandle->GetPropertyDisplayName().ToString())
			.OnTagChanged(this, &FGameplayTagContainerCustomization::RefreshTagList)
			.PropertyHandle(StructPropertyHandle)
		];
}

FReply FGameplayTagContainerCustomization::OnClearAllButtonClicked()
{
	FScopedTransaction Transaction(LOCTEXT("GameplayTagContainerCustomization_RemoveAllTags", "Remove All Gameplay Tags"));

	for (int32 ContainerIdx = 0; ContainerIdx < EditableContainers.Num(); ++ContainerIdx)
	{
		FGameplayTagContainer* Container = EditableContainers[ContainerIdx].TagContainer;

		if (Container)
		{
			FGameplayTagContainer EmptyContainer;
			StructPropertyHandle->SetValueFromFormattedString(EmptyContainer.ToString());
			RefreshTagList();
		}
	}
	return FReply::Handled();
}

EVisibility FGameplayTagContainerCustomization::GetClearAllVisibility() const
{
	return TagNames.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FGameplayTagContainerCustomization::GetTagsListVisibility() const
{
	return TagNames.Num() > 0 ? EVisibility::Visible : EVisibility::Collapsed;
}

void FGameplayTagContainerCustomization::PostUndo( bool bSuccess )
{
	if( bSuccess )
	{
		RefreshTagList();
	}
}

void FGameplayTagContainerCustomization::PostRedo( bool bSuccess )
{
	if( bSuccess )
	{
		RefreshTagList();
	}
}

FGameplayTagContainerCustomization::~FGameplayTagContainerCustomization()
{
	GEditor->UnregisterForUndo(this);
}

void FGameplayTagContainerCustomization::BuildEditableContainerList()
{
	EditableContainers.Empty();

	if( StructPropertyHandle.IsValid() )
	{
		TArray<void*> RawStructData;
		StructPropertyHandle->AccessRawData(RawStructData);

		for (int32 ContainerIdx = 0; ContainerIdx < RawStructData.Num(); ++ContainerIdx)
		{
			EditableContainers.Add(SGameplayTagWidget::FEditableGameplayTagContainerDatum(nullptr, (FGameplayTagContainer*)RawStructData[ContainerIdx]));
		}
	}	
}

#undef LOCTEXT_NAMESPACE
