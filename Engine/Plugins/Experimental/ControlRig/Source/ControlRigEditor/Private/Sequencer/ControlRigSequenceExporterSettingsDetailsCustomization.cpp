// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ControlRigSequenceExporterSettingsDetailsCustomization.h"
#include "DetailLayoutBuilder.h"
#include "AnimSequenceConverterFactory.h"
#include "ControlRigSequenceExporterSettings.h"
#include "PropertyCustomizationHelpers.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/Skeleton.h"
#include "DetailCategoryBuilder.h"
#include "IDetailPropertyRow.h"
#include "Animation/AnimSequence.h"

TSharedRef<IDetailCustomization> FControlRigSequenceExporterSettingsDetailsCustomization::MakeInstance()
{
	return MakeShareable(new FControlRigSequenceExporterSettingsDetailsCustomization);
}

FControlRigSequenceExporterSettingsDetailsCustomization::FControlRigSequenceExporterSettingsDetailsCustomization()
{
	Factory = NewObject<UAnimSequenceConverterFactory>();
	Factory->AddToRoot();
}

FControlRigSequenceExporterSettingsDetailsCustomization::~FControlRigSequenceExporterSettingsDetailsCustomization()
{
	Factory->RemoveFromRoot();
}

void FControlRigSequenceExporterSettingsDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailLayout)
{
	// Customize the anim sequence picker to restrict to the chosen skeletal mesh
	TSharedRef<IPropertyHandle> AnimationSequenceHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UControlRigSequenceExporterSettings, AnimationSequence));
	TSharedRef<IPropertyHandle> SkeletalMeshHandle = DetailLayout.GetProperty(GET_MEMBER_NAME_CHECKED(UControlRigSequenceExporterSettings, SkeletalMesh));

	SkeletalMeshHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateSP(this, &FControlRigSequenceExporterSettingsDetailsCustomization::OnSkeletalMeshChanged, SkeletalMeshHandle));

	DetailLayout.EditCategory("Export Settings")
	.AddProperty(SkeletalMeshHandle);

	DetailLayout.EditCategory("Export Settings")
	.AddProperty(AnimationSequenceHandle)
	.CustomWidget()
	.NameContent()
	[
		AnimationSequenceHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MaxDesiredWidth(600.0f)
	.MinDesiredWidth(600.0f)
	[
		SNew(SObjectPropertyEntryBox)
		.PropertyHandle(AnimationSequenceHandle)
		.NewAssetFactories(TArray<UFactory*>({ Factory }))
		.ThumbnailPool(DetailLayout.GetThumbnailPool())
		.ToolTipText(AnimationSequenceHandle->GetToolTipText())
		.IsEnabled(this, &FControlRigSequenceExporterSettingsDetailsCustomization::IsAnimSequenceEnabled, SkeletalMeshHandle)
		.OnShouldFilterAsset(this, &FControlRigSequenceExporterSettingsDetailsCustomization::HandleShouldFilterAsset, SkeletalMeshHandle)
	];
}

bool FControlRigSequenceExporterSettingsDetailsCustomization::IsAnimSequenceEnabled(TSharedRef<IPropertyHandle> SkeletalMeshHandle) const
{
	UObject* Object;
	SkeletalMeshHandle->GetValue(Object);
	return Object != nullptr;
}

void FControlRigSequenceExporterSettingsDetailsCustomization::OnSkeletalMeshChanged(TSharedRef<IPropertyHandle> SkeletalMeshHandle)
{
	UObject* Object;
	SkeletalMeshHandle->GetValue(Object);
	if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Object))
	{
		Factory->TargetSkeleton = SkeletalMesh->Skeleton;
	}
}

bool FControlRigSequenceExporterSettingsDetailsCustomization::HandleShouldFilterAsset(const FAssetData& InAssetData, TSharedRef<IPropertyHandle> SkeletalMeshHandle)
{
	if (InAssetData.AssetClass == UAnimSequence::StaticClass()->GetFName())
	{
		UObject* Object;
		SkeletalMeshHandle->GetValue(Object);
		if (USkeletalMesh* SkeletalMesh = Cast<USkeletalMesh>(Object))
		{
			FString TagValue;
			if (InAssetData.GetTagValue("Skeleton", TagValue))
			{
				return TagValue != FAssetData(SkeletalMesh->Skeleton).GetExportTextName();
			}
		}
	}

	return true;
}