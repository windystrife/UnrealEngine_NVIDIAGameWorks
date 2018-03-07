// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CollectionReferenceStructCustomization.h"
#include "Misc/PackageName.h"
#include "Misc/MessageDialog.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboButton.h"
#include "DetailWidgetRow.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "MultiBoxBuilder.h"

#define LOCTEXT_NAMESPACE "CollectionReferenceStructCustomization"

TSharedRef<IPropertyTypeCustomization> FCollectionReferenceStructCustomization::MakeInstance()
{
	return MakeShareable(new FCollectionReferenceStructCustomization());
}

void FCollectionReferenceStructCustomization::CustomizeHeader( TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	TSharedPtr<IPropertyHandle> CollectionNameProperty = StructPropertyHandle->GetChildHandle("CollectionName");

	if(CollectionNameProperty.IsValid())
	{		
		HeaderRow.ValueContent()
		.MinDesiredWidth(125.0f)
		.MaxDesiredWidth(600.0f)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				CollectionNameProperty->CreatePropertyValueWidget()
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
			.VAlign(VAlign_Center)
			[
				SAssignNew(PickerButton, SButton)
				.ButtonStyle(FEditorStyle::Get(), "HoverHintOnly")
				.ToolTipText(LOCTEXT("ComboToolTipText", "Choose a collection"))
				.OnClicked(FOnClicked::CreateSP(this, &FCollectionReferenceStructCustomization::OnPickContent, CollectionNameProperty.ToSharedRef()))
				.ContentPadding(2.0f)
				.ForegroundColor(FSlateColor::UseForeground())
				.IsFocusable(false)
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("PropertyWindow.Button_Ellipsis"))
				.ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
		]
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget()
		];
	}
}

void FCollectionReferenceStructCustomization::CustomizeChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
}

FReply FCollectionReferenceStructCustomization::OnPickContent(TSharedRef<IPropertyHandle> PropertyHandle) 
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
	FCollectionPickerConfig PickerConfig;
	PickerConfig.OnCollectionSelected = FOnCollectionSelected::CreateSP(this, &FCollectionReferenceStructCustomization::OnCollectionPicked, PropertyHandle);
	
	FMenuBuilder MenuBuilder(true, NULL);
	MenuBuilder.AddWidget(SNew(SBox)
		.WidthOverride(300.0f)
		.HeightOverride(300.0f)
		[
			ContentBrowserModule.Get().CreateCollectionPicker(PickerConfig)
		], FText());


	PickerMenu = FSlateApplication::Get().PushMenu(PickerButton.ToSharedRef(),
		FWidgetPath(),
		MenuBuilder.MakeWidget(),
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
		);

	return FReply::Handled();
}

void FCollectionReferenceStructCustomization::OnCollectionPicked(const FCollectionNameType& CollectionType, TSharedRef<IPropertyHandle> PropertyHandle)
{
	if (PickerMenu.IsValid())
	{
		PickerMenu->Dismiss();
		PickerMenu.Reset();
	}

	PropertyHandle->SetValue(CollectionType.Name);
}

#undef LOCTEXT_NAMESPACE
