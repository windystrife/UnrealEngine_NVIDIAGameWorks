// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Customization/BlendSpaceDetails.h"

#include "IDetailsView.h"
#include "IDetailGroup.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"

#include "Animation/BlendSpaceBase.h"
#include "Animation/BlendSpace1D.h"

#include "BlendSampleDetails.h"

#include "Widgets/Input/SNumericEntryBox.h"
#include "PropertyCustomizationHelpers.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "BlendSpaceDetails"

FBlendSpaceDetails::FBlendSpaceDetails()
{
	Builder = nullptr;
	BlendSpaceBase = nullptr;
}

FBlendSpaceDetails::~FBlendSpaceDetails()
{
}

void FBlendSpaceDetails::CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder)
{
	TArray< TWeakObjectPtr<UObject> > Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	Builder = &DetailBuilder;
	TWeakObjectPtr<UObject>* WeakPtr = Objects.FindByPredicate([](const TWeakObjectPtr<UObject>& ObjectPtr) { return ObjectPtr->IsA<UBlendSpaceBase>(); });
	if (WeakPtr)
	{
		BlendSpaceBase = Cast<UBlendSpaceBase>(WeakPtr->Get());
		const bool b1DBlendSpace = BlendSpaceBase->IsA<UBlendSpace1D>();

		IDetailCategoryBuilder& CategoryBuilder = DetailBuilder.EditCategory(FName("Axis Settings"));
		IDetailGroup* Groups[2] =
		{
			&CategoryBuilder.AddGroup(FName("Horizontal Axis"), LOCTEXT("HorizontalAxisName", "Horizontal Axis")),
			b1DBlendSpace ? nullptr : &CategoryBuilder.AddGroup(FName("Vertical Axis"), LOCTEXT("VerticalAxisName", "Vertical Axis"))
		};

		// Hide the default blend and interpolation parameters
		TSharedPtr<IPropertyHandle> BlendParameters = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UBlendSpaceBase, BlendParameters), UBlendSpaceBase::StaticClass());
		TSharedPtr<IPropertyHandle> InterpolationParameters = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UBlendSpaceBase, InterpolationParam), UBlendSpaceBase::StaticClass());
		DetailBuilder.HideProperty(BlendParameters);
		DetailBuilder.HideProperty(InterpolationParameters);

		// Add the properties to the corresponding groups created above (third axis will always be hidden since it isn't used)
		int32 HideIndex = b1DBlendSpace ? 1 : 2;
		for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
		{
			TSharedPtr<IPropertyHandle> BlendParameter = BlendParameters->GetChildHandle(AxisIndex);
			TSharedPtr<IPropertyHandle> InterpolationParameter = InterpolationParameters->GetChildHandle(AxisIndex);

			if (AxisIndex < HideIndex)
			{
				Groups[AxisIndex]->AddPropertyRow(BlendParameter.ToSharedRef());
				Groups[AxisIndex]->AddPropertyRow(InterpolationParameter.ToSharedRef());
			}
			else
			{
				DetailBuilder.HideProperty(BlendParameter);
				DetailBuilder.HideProperty(InterpolationParameter);
			}
		}

		IDetailCategoryBuilder& SampleCategoryBuilder = DetailBuilder.EditCategory(FName("BlendSamples"));
		TArray<TSharedRef<IPropertyHandle>> DefaultProperties;
		SampleCategoryBuilder.GetDefaultProperties(DefaultProperties);
		for (TSharedRef<IPropertyHandle> DefaultProperty : DefaultProperties)
		{
			DefaultProperty->MarkHiddenByCustomization();
		}

		FSimpleDelegate RefreshDelegate = FSimpleDelegate::CreateLambda([this]() { Builder->ForceRefreshDetails(); });

		// Retrieve blend samples array
		TSharedPtr<IPropertyHandleArray> BlendSamplesArrayProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UBlendSpaceBase, SampleData), UBlendSpaceBase::StaticClass())->AsArray();
		BlendSamplesArrayProperty->SetOnNumElementsChanged(RefreshDelegate);
		
		uint32 NumBlendSampleEntries = 0;
		BlendSamplesArrayProperty->GetNumElements(NumBlendSampleEntries);
		for (uint32 SampleIndex = 0; SampleIndex < NumBlendSampleEntries; ++SampleIndex)
		{
			TSharedPtr<IPropertyHandle> BlendSampleProperty = BlendSamplesArrayProperty->GetElement(SampleIndex);
			BlendSampleProperty->SetOnChildPropertyValueChanged(RefreshDelegate);
			TSharedPtr<IPropertyHandle> AnimationProperty = BlendSampleProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FBlendSample, Animation));
			TSharedPtr<IPropertyHandle> SampleValueProperty = BlendSampleProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FBlendSample, SampleValue));
			TSharedPtr<IPropertyHandle> RateScaleProperty = BlendSampleProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FBlendSample, RateScale));

			IDetailGroup& Group = SampleCategoryBuilder.AddGroup(FName("GroupName"), FText::GetEmpty());
			Group.HeaderRow()
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(STextBlock)
					.Font(DetailBuilder.GetDetailFont())
					.Text_Lambda([AnimationProperty, SampleIndex]() -> FText
					{
						FAssetData AssetData;
						AnimationProperty->GetValue(AssetData);
						return AssetData.IsValid() ? FText::Format(LOCTEXT("BlendSpaceAnimationNameLabel", "{0} ({1})"), FText::FromString(AssetData.GetAsset()->GetName()), FText::FromString(FString::FromInt(SampleIndex))) : FText::FromString("No Animation");
					})
				]
			];

			FBlendSampleDetails::GenerateBlendSampleWidget([&Group]() -> FDetailWidgetRow& { return Group.AddWidgetRow(); }, FOnSampleMoved::CreateLambda([this](const uint32 Index, const FVector& SampleValue, bool bIsInteractive) 
			{
				if (BlendSpaceBase->IsValidBlendSampleIndex(Index) && BlendSpaceBase->GetBlendSample(Index).SampleValue != SampleValue && !BlendSpaceBase->IsTooCloseToExistingSamplePoint(SampleValue, Index))
				{
					BlendSpaceBase->Modify();

					bool bMoveSuccesful = BlendSpaceBase->EditSampleValue(Index, SampleValue);
					if (bMoveSuccesful)
					{
						BlendSpaceBase->ValidateSampleData();
						FPropertyChangedEvent ChangedEvent(nullptr, bIsInteractive ? EPropertyChangeType::Interactive : EPropertyChangeType::ValueSet);
						BlendSpaceBase->PostEditChangeProperty(ChangedEvent);
					}
				}
			}), BlendSpaceBase, SampleIndex, false);
			FDetailWidgetRow& AnimationRow = Group.AddWidgetRow();
			FBlendSampleDetails::GenerateAnimationWidget(AnimationRow, BlendSpaceBase, AnimationProperty);
			Group.AddPropertyRow(RateScaleProperty.ToSharedRef());
		}
	}
	
}

#undef LOCTEXT_NAMESPACE
