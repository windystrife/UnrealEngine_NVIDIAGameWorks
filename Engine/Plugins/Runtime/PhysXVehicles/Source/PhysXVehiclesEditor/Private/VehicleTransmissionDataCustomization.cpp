// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VehicleTransmissionDataCustomization.h"
#include "WheeledVehicleMovementComponent4W.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "DetailLayoutBuilder.h"
#include "IDetailChildrenBuilder.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"
#include "IDocumentation.h"

#define LOCTEXT_NAMESPACE "VehicleTransmissionDataCustomization"

#define RowWidth_Customization 50
#define GearColumnsWidth (75.f * 3.f)

////////////////////////////////////////////////////////////////

bool FVehicleTransmissionDataCustomization::IsAutomaticEnabled() const
{
	return SelectedTransmission && SelectedTransmission->bUseGearAutoBox;
}

//Helper function so we can make neutral and reverse look the same as forward gears
void FVehicleTransmissionDataCustomization::CreateGearUIHelper(FDetailWidgetRow & GearsSetup, FText Label, TSharedRef<IPropertyHandle> GearHandle, EGearType GearType)
{
	uint32 NumChildren = 0;
	GearHandle->GetNumChildren(NumChildren);	//we use num of children to determine if we are dealing with a full gear that has ratio, down, up - or just a single value

	TSharedRef<SWidget> RatioWidget = (NumChildren > 1 ? GearHandle->GetChildHandle("Ratio")->CreatePropertyValueWidget() : GearHandle->CreatePropertyValueWidget());
	TSharedRef<SWidget> DownRatioWidget = (NumChildren > 1 ? GearHandle->GetChildHandle("DownRatio")->CreatePropertyValueWidget() : GearHandle->CreatePropertyValueWidget());
	TSharedRef<SWidget> UpRatioWidget = (NumChildren > 1 ? GearHandle->GetChildHandle("UpRatio")->CreatePropertyValueWidget() : GearHandle->CreatePropertyValueWidget());
	
	RatioWidget->SetEnabled(GearType != NeutralGear);
	switch (GearType)
	{
	case ForwardGear:
		{
			DownRatioWidget->SetEnabled(TAttribute<bool>(this, &FVehicleTransmissionDataCustomization::IsAutomaticEnabled));
			UpRatioWidget->SetEnabled(TAttribute<bool>(this, &FVehicleTransmissionDataCustomization::IsAutomaticEnabled));
			break;
		}
	case ReverseGear:
		{
			DownRatioWidget->SetEnabled(false);
			UpRatioWidget->SetEnabled(false);
			break;
		}
	case NeutralGear:
		{
			DownRatioWidget->SetEnabled(false);
			UpRatioWidget->SetEnabled(TAttribute<bool>(this, &FVehicleTransmissionDataCustomization::IsAutomaticEnabled));
			break;
		}
	}

	TSharedRef<SWidget> RemoveWidget = PropertyCustomizationHelpers::MakeDeleteButton(FSimpleDelegate::CreateSP(this, &FVehicleTransmissionDataCustomization::RemoveGear, GearHandle), LOCTEXT("RemoveGearToolTip", "Removes gear"));
	RemoveWidget->SetEnabled(NumChildren > 1);
	
	GearsSetup
	.NameContent()
	[
		SNew(STextBlock)
		.Text(Label)
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
		.MaxDesiredWidth(GearColumnsWidth)
		.MinDesiredWidth(GearColumnsWidth)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.3333f)
			[
				RatioWidget
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.3333f)
			.Padding(4.f)
			[
				DownRatioWidget
			]

			+ SHorizontalBox::Slot()
			.FillWidth(0.3333f)
			.Padding(4.f)
			[
				UpRatioWidget
			]

			+ SHorizontalBox::Slot()
			.Padding(4.f)
			.AutoWidth()
			[
				RemoveWidget
			]
		];
}

void FVehicleTransmissionDataCustomization::BuildColumnsHeaderHelper(TSharedRef<class IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& GearsSetup)
{
	
	GearsSetup
	.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("GearSetup", "Gear Setup"))
			.Font(IDetailLayoutBuilder::GetDetailFontBold())
		]
	.ValueContent()
		.MaxDesiredWidth(GearColumnsWidth)
		.MinDesiredWidth(GearColumnsWidth)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.3333f)
			.VAlign(VAlign_Center)
			[
				SNew(SBox)
				.WidthOverride(RowWidth_Customization)
				.HAlign(HAlign_Left)
				.Content()
				[
					SNew(STextBlock)
					.Text(LOCTEXT("RatioLabel", "Gear Ratio"))
					.Font(IDetailLayoutBuilder::GetDetailFontBold())
				]
			]
			+ SHorizontalBox::Slot()
				.FillWidth(0.3333f)
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.HAlign(HAlign_Left)
					.WidthOverride(RowWidth_Customization)
					.Content()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("LowRPMLabel", "Down Ratio"))
						.Font(IDetailLayoutBuilder::GetDetailFontBold())
					]
				]
			+ SHorizontalBox::Slot()
				.FillWidth(0.3333f)
				.VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(LOCTEXT("HighRPMLabel", "Up Ratio"))
					.Font(IDetailLayoutBuilder::GetDetailFontBold())
				]
			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					PropertyCustomizationHelpers::MakeAddButton(FSimpleDelegate::CreateSP(this, &FVehicleTransmissionDataCustomization::AddGear, StructPropertyHandle), LOCTEXT("AddGearToolTip", "Adds a new gear"))
				]
			+ SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					PropertyCustomizationHelpers::MakeEmptyButton(FSimpleDelegate::CreateSP(this, &FVehicleTransmissionDataCustomization::EmptyGears, StructPropertyHandle), LOCTEXT("EmptyGearToolTip", "Removes all gears"))
				]
			
		];
}

void FVehicleTransmissionDataCustomization::CreateGearUIDelegate(TSharedRef<IPropertyHandle> GearProperty, int32 GearIdx, IDetailChildrenBuilder& ChildrenBuilder)
{
	FText Label = FText::Format(LOCTEXT("TransmissionGear", "Gear {0}"), FText::AsNumber(GearIdx + 1));
	CreateGearUIHelper(ChildrenBuilder.AddCustomRow(Label), Label, GearProperty, ForwardGear);
}

void FVehicleTransmissionDataCustomization::CustomizeChildren(TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	// copy all transmision instances I'm accessing right now
	TArray<void*> StructPtrs;
	StructPropertyHandle->AccessRawData(StructPtrs);
	
	if (StructPtrs.Num() == 1)
	{
		SelectedTransmission = (FVehicleTransmissionData *)StructPtrs[0];
	}
	else
	{
		SelectedTransmission = NULL;
	}


	uint32 NumChildren = 0;
	StructPropertyHandle->GetNumChildren(NumChildren);

	FName GearsSetupGroupName = TEXT("GearsSetup");
	FText GearsSetupGroupLabel = LOCTEXT("GearSetupLabel", "Gears Setup");
	IDetailGroup * GearSetupGroup = NULL;
	
	for (uint32 ChildIdx = 0; ChildIdx < NumChildren; ++ChildIdx)
	{
		TSharedRef<IPropertyHandle> ChildProperty = StructPropertyHandle->GetChildHandle(ChildIdx).ToSharedRef();
		const FString PropertyName = ChildProperty->GetProperty() ? ChildProperty->GetProperty()->GetName() : TEXT("");

		if (PropertyName == TEXT("ForwardGears") || PropertyName == TEXT("NeutralGearUpRatio") || PropertyName == TEXT("ReverseGearRatio"))
		{
			if (GearSetupGroup == NULL)
			{
				GearSetupGroup = &StructBuilder.AddGroup(GearsSetupGroupName, GearsSetupGroupLabel);
				BuildColumnsHeaderHelper(StructPropertyHandle, StructBuilder.AddCustomRow(GearsSetupGroupLabel));
			}

			//determine which gear we're showing
			EGearType GearType = ForwardGear;
			if (PropertyName == TEXT("NeutralGearUpRatio"))
			{
				GearType = NeutralGear;
			}
			else if
			(PropertyName == TEXT("ReverseGearRatio"))
			{
				GearType = ReverseGear;
			}
						
			if (GearType == ForwardGear)
			{
				TSharedRef<FDetailArrayBuilder> GearsArrayBuilder = MakeShareable(new FDetailArrayBuilder(ChildProperty, false));
				GearsArrayBuilder->OnGenerateArrayElementWidget(FOnGenerateArrayElementWidget::CreateSP(this, &FVehicleTransmissionDataCustomization::CreateGearUIDelegate));
				StructBuilder.AddCustomBuilder(GearsArrayBuilder);
			}
			else
			{
				const FText PropertyNameText = FText::FromString(PropertyName);
				CreateGearUIHelper(StructBuilder.AddCustomRow(PropertyNameText), PropertyNameText, ChildProperty, GearType);
			}


		}
		else
		{	//Add all other properties
			StructBuilder.AddProperty(ChildProperty);
		}
	}

}

void FVehicleTransmissionDataCustomization::AddGear(TSharedRef<IPropertyHandle> StructPropertyHandle)
{
	TSharedPtr<IPropertyHandle> GearsHandle = StructPropertyHandle->GetChildHandle("ForwardGears");
	if (GearsHandle->IsValidHandle())
	{
		TSharedPtr<IPropertyHandleArray> GearsArray = GearsHandle->AsArray();
		GearsArray->AddItem();
	}
}

void FVehicleTransmissionDataCustomization::EmptyGears(TSharedRef<IPropertyHandle> StructPropertyHandle)
{
	TSharedPtr<IPropertyHandle> GearsHandle = StructPropertyHandle->GetChildHandle("ForwardGears");
	if (GearsHandle->IsValidHandle())
	{
		TSharedPtr<IPropertyHandleArray> GearsArray = GearsHandle->AsArray();
		GearsArray->EmptyArray();
	}
}

void FVehicleTransmissionDataCustomization::RemoveGear(TSharedRef<IPropertyHandle> GearHandle)
{
	if (GearHandle->IsValidHandle())
	{
		const TSharedPtr<IPropertyHandle> ParentHandle = GearHandle->GetParentHandle();
		const TSharedPtr<IPropertyHandleArray> ParentArrayHandle = ParentHandle->AsArray();

		ParentArrayHandle->DeleteItem(GearHandle->GetIndexInArray());
		
	}
}


void FVehicleTransmissionDataCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	HeaderRow.
	NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
		[
			StructPropertyHandle->CreatePropertyValueWidget()
		];
}

#undef LOCTEXT_NAMESPACE
#undef GearColumnsWidth
#undef RowWidth_Customization
