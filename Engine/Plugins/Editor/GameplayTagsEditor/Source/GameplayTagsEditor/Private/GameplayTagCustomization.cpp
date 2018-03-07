// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagCustomization.h"
#include "Widgets/Input/SComboButton.h"

#include "Editor.h"
#include "PropertyHandle.h"
#include "DetailWidgetRow.h"
#include "GameplayTagsEditorModule.h"
#include "SHyperlink.h"

#define LOCTEXT_NAMESPACE "GameplayTagCustomization"

TSharedRef<IPropertyTypeCustomization> FGameplayTagCustomizationPublic::MakeInstance()
{
	return MakeShareable(new FGameplayTagCustomization);
}

void FGameplayTagCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	TagContainer = MakeShareable(new FGameplayTagContainer);
	StructPropertyHandle = InStructPropertyHandle;

	FSimpleDelegate OnTagChanged = FSimpleDelegate::CreateSP(this, &FGameplayTagCustomization::OnPropertyValueChanged);
	StructPropertyHandle->SetOnPropertyValueChanged(OnTagChanged);

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
		[
			SNew(SComboButton)
			.OnGetMenuContent(this, &FGameplayTagCustomization::GetListContent)
			.ContentPadding(FMargin(2.0f, 2.0f))
			.MenuPlacement(MenuPlacement_BelowAnchor)
			.ButtonContent()
			[
				SNew(STextBlock)
				.Text(LOCTEXT("GameplayTagCustomization_Edit", "Edit"))
			]
		]
		
		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBorder)
			.Visibility(this, &FGameplayTagCustomization::GetVisibilityForTagTextBlockWidget, true)
			.Padding(4.0f)
			[
				SNew(STextBlock)
				.Text(this, &FGameplayTagCustomization::SelectedTag)
			]
		]

		+ SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBorder)
			.Visibility(this, &FGameplayTagCustomization::GetVisibilityForTagTextBlockWidget, false)
			.Padding(4.0f)
			[
				SNew(SHyperlink)
				.Text(this, &FGameplayTagCustomization::SelectedTag)
				.OnNavigate( this, &FGameplayTagCustomization::OnTagDoubleClicked)
			]
		]
	];

	GEditor->RegisterForUndo(this);
}

void FGameplayTagCustomization::OnTagDoubleClicked()
{
	UGameplayTagsManager::Get().NotifyGameplayTagDoubleClickedEditor(TagName);
}

EVisibility FGameplayTagCustomization::GetVisibilityForTagTextBlockWidget(bool ForTextWidget) const
{
	return (UGameplayTagsManager::Get().ShowGameplayTagAsHyperLinkEditor(TagName) ^ ForTextWidget) ? EVisibility::Visible : EVisibility::Collapsed;
}

TSharedRef<SWidget> FGameplayTagCustomization::GetListContent()
{
	BuildEditableContainerList();
	
	FString Categories = UGameplayTagsManager::Get().GetCategoriesMetaFromPropertyHandle(StructPropertyHandle);

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
			.MultiSelect(false)
			.OnTagChanged(this, &FGameplayTagCustomization::OnTagChanged)
			.PropertyHandle(StructPropertyHandle)
		];
}

void FGameplayTagCustomization::OnPropertyValueChanged()
{
	TagName = TEXT("");
	if (StructPropertyHandle.IsValid() && StructPropertyHandle->GetProperty() && EditableContainers.Num() > 0)
	{
		TArray<void*> RawStructData;
		StructPropertyHandle->AccessRawData(RawStructData);
		if (RawStructData.Num() > 0)
		{
			FGameplayTag* Tag = (FGameplayTag*)(RawStructData[0]);
			FGameplayTagContainer* Container = EditableContainers[0].TagContainer;			
			if (Tag && Container)
			{
				Container->Reset();
				Container->AddTag(*Tag);
				TagName = Tag->ToString();
			}			
		}
	}
}

void FGameplayTagCustomization::OnTagChanged()
{
	TagName = TEXT("");
	if (StructPropertyHandle.IsValid() && StructPropertyHandle->GetProperty() && EditableContainers.Num() > 0)
	{
		TArray<void*> RawStructData;
		StructPropertyHandle->AccessRawData(RawStructData);
		if (RawStructData.Num() > 0)
		{
			FGameplayTag* Tag = (FGameplayTag*)(RawStructData[0]);			

			// Update Tag from the one selected from list
			FGameplayTagContainer* Container = EditableContainers[0].TagContainer;
			if (Tag && Container)
			{
				for (auto It = Container->CreateConstIterator(); It; ++It)
				{
					*Tag = *It;
					TagName = It->ToString();
				}
			}
		}
	}
}

void FGameplayTagCustomization::PostUndo(bool bSuccess)
{
	if (bSuccess && !StructPropertyHandle.IsValid())
	{
		OnTagChanged();
	}
}

void FGameplayTagCustomization::PostRedo(bool bSuccess)
{
	if (bSuccess && !StructPropertyHandle.IsValid())
	{
		OnTagChanged();
	}
}

FGameplayTagCustomization::~FGameplayTagCustomization()
{
	GEditor->UnregisterForUndo(this);
}

void FGameplayTagCustomization::BuildEditableContainerList()
{
	EditableContainers.Empty();

	if(StructPropertyHandle.IsValid() && StructPropertyHandle->GetProperty())
	{
		TArray<void*> RawStructData;
		StructPropertyHandle->AccessRawData(RawStructData);

		if (RawStructData.Num() > 0)
		{
			FGameplayTag* Tag = (FGameplayTag*)(RawStructData[0]);
			if (Tag->IsValid())
			{
				TagName = Tag->ToString();
				TagContainer->AddTag(*Tag);
			}
		}

		EditableContainers.Add(SGameplayTagWidget::FEditableGameplayTagContainerDatum(nullptr, TagContainer.Get()));
	}
}

FText FGameplayTagCustomization::SelectedTag() const
{
	return FText::FromString(*TagName);
}

#undef LOCTEXT_NAMESPACE
