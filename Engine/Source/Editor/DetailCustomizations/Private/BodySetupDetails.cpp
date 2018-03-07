// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BodySetupDetails.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Input/SButton.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"
#include "ScopedTransaction.h"
#include "ObjectEditorUtils.h"
#include "PhysicsEngine/BodySetup.h"
#include "PhysicsEngine/PhysicsAsset.h"

#define LOCTEXT_NAMESPACE "BodySetupDetails"

TSharedRef<IDetailCustomization> FBodySetupDetails::MakeInstance()
{
	return MakeShareable( new FBodySetupDetails );
}

void FBodySetupDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	// Customize collision section
	{
		if ( DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UBodySetup, DefaultInstance))->IsValidHandle() )
		{
			DetailBuilder.GetObjectsBeingCustomized(ObjectsCustomized);
			TSharedPtr<IPropertyHandle> BodyInstanceHandler = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UBodySetup, DefaultInstance));

			const bool bInPhat = ObjectsCustomized.Num() && (Cast<USkeletalBodySetup>(ObjectsCustomized[0].Get()) != nullptr);
			if (bInPhat)
			{
				TSharedRef<IPropertyHandle> AsyncEnabled = BodyInstanceHandler->GetChildHandle(GET_MEMBER_NAME_CHECKED(FBodyInstance, bUseAsyncScene)).ToSharedRef();
				AsyncEnabled->MarkHiddenByCustomization();
			}

			BodyInstanceCustomizationHelper = MakeShareable(new FBodyInstanceCustomizationHelper(ObjectsCustomized));
			BodyInstanceCustomizationHelper->CustomizeDetails(DetailBuilder, DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UBodySetup, DefaultInstance)));

			IDetailCategoryBuilder& CollisionCategory = DetailBuilder.EditCategory("Collision");
			DetailBuilder.HideProperty(BodyInstanceHandler);

			TSharedPtr<IPropertyHandle> CollisionTraceHandler = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UBodySetup, CollisionTraceFlag));
			DetailBuilder.HideProperty(CollisionTraceHandler);

			// add physics properties to physics category
			uint32 NumChildren = 0;
			BodyInstanceHandler->GetNumChildren(NumChildren);

			static const FName CollisionCategoryName(TEXT("Collision"));

			// add all properties of this now - after adding 
			for (uint32 ChildIndex=0; ChildIndex < NumChildren; ++ChildIndex)
			{
				TSharedPtr<IPropertyHandle> ChildProperty = BodyInstanceHandler->GetChildHandle(ChildIndex);
				FName CategoryName = FObjectEditorUtils::GetCategoryFName(ChildProperty->GetProperty());
				if (CategoryName == CollisionCategoryName)
				{
					CollisionCategory.AddProperty(ChildProperty);
				}
			}
		}
	}
}

TSharedRef<IDetailCustomization> FSkeletalBodySetupDetails::MakeInstance()
{
	return MakeShareable(new FSkeletalBodySetupDetails);
}


void FSkeletalBodySetupDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	DetailBuilder.GetObjectsBeingCustomized(ObjectsCustomized);

	IDetailCategoryBuilder& Cat = DetailBuilder.EditCategory(TEXT("PhysicalAnimation"));

	const TArray<TWeakObjectPtr<UObject>>& ObjectsCustomizedLocal = ObjectsCustomized;
	auto PhysAnimEditable = [ObjectsCustomizedLocal]() -> bool
	{
		bool bVisible = ObjectsCustomizedLocal.Num() > 0;
		for (TWeakObjectPtr<UObject> WeakObj : ObjectsCustomizedLocal)
		{
			if (USkeletalBodySetup* BS = Cast<USkeletalBodySetup>(WeakObj.Get()))
			{
				if (!BS->FindPhysicalAnimationProfile(BS->GetCurrentPhysicalAnimationProfileName()))
				{
					bVisible = false;
					break;
				}
			}
			else
			{
				bVisible = false;
				break;
			}
		}

		return bVisible;
	};

	TAttribute<EVisibility> PhysAnimVisible = TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda([PhysAnimEditable]()
	{
		return PhysAnimEditable() == true ? EVisibility::Visible : EVisibility::Collapsed;
	}));

	TAttribute<EVisibility> NewPhysAnimButtonVisible = TAttribute<EVisibility>::Create(TAttribute<EVisibility>::FGetter::CreateLambda([PhysAnimEditable]()
	{
		return PhysAnimEditable() == true ? EVisibility::Collapsed : EVisibility::Visible;
	}));

	Cat.AddCustomRow(LOCTEXT("Profile", "Physical Animation Profile"))
	[
		SNew(SBorder)
		.BorderImage(FEditorStyle::Get().GetBrush("ToolPanel.DarkGroupBorder"))
		.Padding(4.0f)
		[
			SNew(SRichTextBlock)
			.DecoratorStyleSet(&FEditorStyle::Get())
			.Text_Lambda([ObjectsCustomizedLocal]()
			{
				if (ObjectsCustomizedLocal.Num() > 0)
				{
					if (USkeletalBodySetup* BS = Cast<USkeletalBodySetup>(ObjectsCustomizedLocal[0].Get()))
					{
						return FText::Format(LOCTEXT("ProfileFormat", "Current Profile: <RichTextBlock.Bold>{0}</>"), FText::FromName(BS->GetCurrentPhysicalAnimationProfileName()));
					}
				}

				return FText();
			})
		]
	];

	TSharedPtr<IPropertyHandle> PhysicalAnimationProfile = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(USkeletalBodySetup, CurrentPhysicalAnimationProfile));
	PhysicalAnimationProfile->MarkHiddenByCustomization();

	uint32 NumChildren = 0;
	TSharedPtr<IPropertyHandle> ProfileData = PhysicalAnimationProfile->GetChildHandle(GET_MEMBER_NAME_CHECKED(FPhysicalAnimationProfile, PhysicalAnimationData));
	ProfileData->GetNumChildren(NumChildren);
	for(uint32 ChildIdx = 0; ChildIdx < NumChildren; ++ChildIdx)
	{
		TSharedPtr<IPropertyHandle> Child = ProfileData->GetChildHandle(ChildIdx);
		if(!Child->IsCustomized())
		{
			Cat.AddProperty(Child)
			.Visibility(PhysAnimVisible);
		}
	}
}

#undef LOCTEXT_NAMESPACE

