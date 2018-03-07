// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CaptureTypeCustomization.h"
#include "Engine/GameViewportClient.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SComboBox.h"
#include "DetailWidgetRow.h"
#include "DetailLayoutBuilder.h"
#include "MovieSceneCaptureModule.h"

#define LOCTEXT_NAMESPACE "CaptureTypeCustomization"

TSharedRef<IPropertyTypeCustomization> FCaptureTypeCustomization::MakeInstance()
{
	return MakeShareable( new FCaptureTypeCustomization );
}

void FCaptureTypeCustomization::CustomizeHeader( TSharedRef<IPropertyHandle> InPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils )
{
	PropertyUtilities = CustomizationUtils.GetPropertyUtilities();
	
	PropertyHandle = InPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FCaptureProtocolID, Identifier));
	FMovieSceneCaptureProtocolRegistry& Registry = IMovieSceneCaptureModule::Get().GetProtocolRegistry();

	Registry.IterateProtocols([&](FCaptureProtocolID ID, const FMovieSceneCaptureProtocolInfo& Info){
		CaptureTypes.Add(MakeShareable(new FCaptureProtocolType{ ID, Info.DisplayName }));
	});

	if (CaptureTypes.Num() == 0)
	{
		return;
	}

	FName CurrentID;
	PropertyHandle->GetValue(CurrentID);

	SetCurrentIndex(CurrentID);

	HeaderRow
	.NameContent()
	[
		InPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MaxDesiredWidth(TOptional<float>())
	[
		SNew(SComboBox<TSharedPtr<FCaptureProtocolType>>)
		.OptionsSource(&CaptureTypes)
		.OnSelectionChanged(this, &FCaptureTypeCustomization::OnPropertyChanged)
		.OnGenerateWidget_Lambda([](TSharedPtr<FCaptureProtocolType> CaptureType){
			return SNew(STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(CaptureType->DisplayName);
		})
		.InitiallySelectedItem(CaptureTypes[CurrentIndex])
		[
			SAssignNew(CurrentText, STextBlock)
			.Font(IDetailLayoutBuilder::GetDetailFont())
			.Text(CaptureTypes[CurrentIndex]->DisplayName)
		]
	];
}

void FCaptureTypeCustomization::OnPropertyChanged(TSharedPtr<FCaptureProtocolType> CaptureType, ESelectInfo::Type)
{
	SetCurrentIndex(CaptureType->ID.Identifier);
	PropertyHandle->SetValue(CaptureType->ID.Identifier);
	CurrentText->SetText(CaptureType->DisplayName);

	PropertyUtilities->RequestRefresh();
}

void FCaptureTypeCustomization::SetCurrentIndex(FName ID)
{
	CurrentIndex = CaptureTypes.IndexOfByPredicate([&](const TSharedPtr<FCaptureProtocolType>& In){
		return In->ID.Identifier == ID;
	});

	if (CurrentIndex == INDEX_NONE)
	{
		CurrentIndex = 0;
	}
}

#undef LOCTEXT_NAMESPACE
