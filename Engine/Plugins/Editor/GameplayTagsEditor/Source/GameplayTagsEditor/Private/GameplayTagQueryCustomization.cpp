// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GameplayTagQueryCustomization.h"
#include "UObject/UnrealType.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SWindow.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Framework/Docking/TabManager.h"

#include "Editor.h"
#include "PropertyHandle.h"
#include "DetailWidgetRow.h"
#include "IPropertyTypeCustomization.h"
#include "IPropertyUtilities.h"
#include "NotifyHook.h"

#define LOCTEXT_NAMESPACE "GameplayTagQueryCustomization"

void FGameplayTagQueryCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> InStructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	StructPropertyHandle = InStructPropertyHandle;

	PropertyUtilities = StructCustomizationUtils.GetPropertyUtilities();
	
	RefreshQueryDescription(); // will call BuildEditableQueryList();

	bool const bReadOnly = StructPropertyHandle->IsEditConst();

	HeaderRow
	.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MaxDesiredWidth(512)
	[
		SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.Text(this, &FGameplayTagQueryCustomization::GetEditButtonText)
				.OnClicked(this, &FGameplayTagQueryCustomization::OnEditButtonClicked)
			]
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SButton)
				.IsEnabled(!bReadOnly)
				.Text(LOCTEXT("GameplayTagQueryCustomization_Clear", "Clear All"))
				.OnClicked(this, &FGameplayTagQueryCustomization::OnClearAllButtonClicked)
				.Visibility(this, &FGameplayTagQueryCustomization::GetClearAllVisibility)
			]
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		[
			SNew(SBorder)
			.Padding(4.0f)
			.Visibility(this, &FGameplayTagQueryCustomization::GetQueryDescVisibility)
			[
				SNew(STextBlock)
				.Text(this, &FGameplayTagQueryCustomization::GetQueryDescText)
				.AutoWrapText(true)
			]
		]
	];

	GEditor->RegisterForUndo(this);
}

FText FGameplayTagQueryCustomization::GetQueryDescText() const
{
	return FText::FromString(QueryDescription);
}

FText FGameplayTagQueryCustomization::GetEditButtonText() const
{
	if (StructPropertyHandle.IsValid())
	{
		bool const bReadOnly = StructPropertyHandle->IsEditConst();
		return
			bReadOnly
			? LOCTEXT("GameplayTagQueryCustomization_View", "View...")
			: LOCTEXT("GameplayTagQueryCustomization_Edit", "Edit...");
	}

	return FText();
}

FReply FGameplayTagQueryCustomization::OnClearAllButtonClicked()
{
	for (auto& EQ : EditableQueries)
	{
		if (EQ.TagQuery)
		{
			EQ.TagQuery->Clear();
		}
	}

	RefreshQueryDescription();

	return FReply::Handled();
}

EVisibility FGameplayTagQueryCustomization::GetClearAllVisibility() const
{
	bool bAtLeastOneQueryIsNonEmpty = false;

	for (auto& EQ : EditableQueries)
	{
		if (EQ.TagQuery && EQ.TagQuery->IsEmpty() == false)
		{
			bAtLeastOneQueryIsNonEmpty = true;
			break;
		}
	}

	return bAtLeastOneQueryIsNonEmpty ? EVisibility::Visible : EVisibility::Collapsed;
}

EVisibility FGameplayTagQueryCustomization::GetQueryDescVisibility() const
{
	return QueryDescription.IsEmpty() ? EVisibility::Collapsed : EVisibility::Visible;
}

void FGameplayTagQueryCustomization::RefreshQueryDescription()
{
	// Rebuild Editable Containers as container references can become unsafe
	BuildEditableQueryList();

	// Clear the list
	QueryDescription.Empty();

	if (EditableQueries.Num() > 1)
	{
		QueryDescription = TEXT("Multiple Selected");
	}
	else if ( (EditableQueries.Num() == 1) && (EditableQueries[0].TagQuery != nullptr) )
	{
		QueryDescription = EditableQueries[0].TagQuery->GetDescription();
	}
}


FReply FGameplayTagQueryCustomization::OnEditButtonClicked()
{
	if (GameplayTagQueryWidgetWindow.IsValid())
	{
		// already open, just show it
		GameplayTagQueryWidgetWindow->BringToFront(true);
	}
	else
	{
		TArray<UObject*> OuterObjects;
		StructPropertyHandle->GetOuterObjects(OuterObjects);

		bool const bReadOnly = StructPropertyHandle->IsEditConst();

		FText Title;
		if (OuterObjects.Num() > 1)
		{
			FText const AssetName = FText::Format(LOCTEXT("GameplayTagDetailsBase_MultipleAssets", "{0} Assets"), FText::AsNumber(OuterObjects.Num()));
			FText const PropertyName = StructPropertyHandle->GetPropertyDisplayName();
			Title = FText::Format(LOCTEXT("GameplayTagQueryCustomization_BaseWidgetTitle", "Tag Editor: {0} {1}"), PropertyName, AssetName);
		}
		else if (OuterObjects.Num() > 0 && OuterObjects[0])
		{
			FText const AssetName = FText::FromString(OuterObjects[0]->GetName());
			FText const PropertyName = StructPropertyHandle->GetPropertyDisplayName();
			Title = FText::Format(LOCTEXT("GameplayTagQueryCustomization_BaseWidgetTitle", "Tag Editor: {0} {1}"), PropertyName, AssetName);
		}


		GameplayTagQueryWidgetWindow = SNew(SWindow)
			.Title(Title)
			.HasCloseButton(false)
			.ClientSize(FVector2D(600, 400))
			[
				SNew(SGameplayTagQueryWidget, EditableQueries)
				.OnSaveAndClose(this, &FGameplayTagQueryCustomization::CloseWidgetWindow, false)
				.OnCancel(this, &FGameplayTagQueryCustomization::CloseWidgetWindow, true)
				.ReadOnly(bReadOnly)
			];

		if (FGlobalTabmanager::Get()->GetRootWindow().IsValid())
		{
			FSlateApplication::Get().AddWindowAsNativeChild(GameplayTagQueryWidgetWindow.ToSharedRef(), FGlobalTabmanager::Get()->GetRootWindow().ToSharedRef());
		}
		else
		{
			FSlateApplication::Get().AddWindow(GameplayTagQueryWidgetWindow.ToSharedRef());
		}
	}

	return FReply::Handled();
}

FGameplayTagQueryCustomization::~FGameplayTagQueryCustomization()
{
	if( GameplayTagQueryWidgetWindow.IsValid() )
	{
		GameplayTagQueryWidgetWindow->RequestDestroyWindow();
	}
	GEditor->UnregisterForUndo(this);
}

void FGameplayTagQueryCustomization::BuildEditableQueryList()
{	
	EditableQueries.Empty();

	if( StructPropertyHandle.IsValid() )
	{
		TArray<void*> RawStructData;
		StructPropertyHandle->AccessRawData(RawStructData);

		TArray<UObject*> OuterObjects;
		StructPropertyHandle->GetOuterObjects(OuterObjects);
		
		for (int32 Idx = 0; Idx < RawStructData.Num(); ++Idx)
		{
			// Null outer objects may mean that we are inside a UDataTable. This is ok though. We can still dirty the data table via FNotify Hook. (see ::CloseWidgetWindow). However undo will not work.
			UObject* Obj = OuterObjects.IsValidIndex(Idx) ? OuterObjects[Idx] : nullptr;

			EditableQueries.Add(SGameplayTagQueryWidget::FEditableGameplayTagQueryDatum(Obj, (FGameplayTagQuery*)RawStructData[Idx]));
		}
	}	
}

void FGameplayTagQueryCustomization::CloseWidgetWindow(bool WasCancelled)
{
	// Notify change. This is required for these to work inside of UDataTables
	if (!WasCancelled && PropertyUtilities.IsValid())
	{
		UProperty* TheProperty = StructPropertyHandle->GetProperty();

		FEditPropertyChain PropertyChain;
		PropertyChain.AddHead(TheProperty);
		PropertyChain.SetActivePropertyNode(TheProperty);

		FPropertyChangedEvent ChangeEvent(StructPropertyHandle->GetProperty(), EPropertyChangeType::ValueSet, nullptr);
		FNotifyHook* NotifyHook = PropertyUtilities->GetNotifyHook();
		if (NotifyHook != nullptr)
		{
			NotifyHook->NotifyPostChange(ChangeEvent, &PropertyChain);
		}
	}

 	if( GameplayTagQueryWidgetWindow.IsValid() )
 	{
 		GameplayTagQueryWidgetWindow->RequestDestroyWindow();
		GameplayTagQueryWidgetWindow = nullptr;

		RefreshQueryDescription();
 	}
}

#undef LOCTEXT_NAMESPACE
