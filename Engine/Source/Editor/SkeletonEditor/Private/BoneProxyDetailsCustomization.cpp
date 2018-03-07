// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BoneProxyDetailsCustomization.h"
#include "BoneProxy.h"
#include "AnimPreviewInstance.h"
#include "PropertyHandle.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "ScopedTransaction.h"
#include "MultiBoxBuilder.h"
#include "SComboButton.h"
#include "SBox.h"
#include "Algo/Transform.h"

#define LOCTEXT_NAMESPACE "FBoneProxyDetailsCustomization"

namespace BoneProxyCustomizationConstants
{
	static const float ItemWidth = 125.0f;
}

static FText GetTransformFieldText(bool* bValuePtr, FText Label)
{
	return *bValuePtr ? FText::Format(LOCTEXT("Local", "Local {0}"), Label) : FText::Format(LOCTEXT("World", "World {0}"), Label);
}

static void OnSetRelativeTransform(bool* bValuePtr)
{
	*bValuePtr = true;
}

static void OnSetWorldTransform(bool* bValuePtr)
{
	*bValuePtr = false;
}

static bool IsRelativeTransformChecked(bool* bValuePtr)
{
	return *bValuePtr;
}

static bool IsWorldTransformChecked(bool* bValuePtr)
{
	return !*bValuePtr;
}

static TSharedRef<SWidget> BuildTransformFieldLabel(bool* bValuePtr, const FText& Label, bool bMultiSelected)
{
	if (bMultiSelected)
	{
		return SNew(STextBlock)
			.Text(Label)
			.Font(IDetailLayoutBuilder::GetDetailFont());
	}
	else
	{
		FMenuBuilder MenuBuilder(true, nullptr, nullptr);

		FUIAction SetRelativeLocationAction
		(
			FExecuteAction::CreateStatic(&OnSetRelativeTransform, bValuePtr),
			FCanExecuteAction(),
			FIsActionChecked::CreateStatic(&IsRelativeTransformChecked, bValuePtr)
		);

		FUIAction SetWorldLocationAction
		(
			FExecuteAction::CreateStatic(&OnSetWorldTransform, bValuePtr),
			FCanExecuteAction(),
			FIsActionChecked::CreateStatic(&IsWorldTransformChecked, bValuePtr)
		);

		MenuBuilder.BeginSection( TEXT("TransformType"), FText::Format( LOCTEXT("TransformType", "{0} Type"), Label ) );

		MenuBuilder.AddMenuEntry
		(
			FText::Format( LOCTEXT( "LocalLabel", "Local"), Label ),
			FText::Format( LOCTEXT( "LocalLabel_ToolTip", "{0} is relative to its parent"), Label ),
			FSlateIcon(),
			SetRelativeLocationAction,
			NAME_None, 
			EUserInterfaceActionType::RadioButton
		);

		MenuBuilder.AddMenuEntry
		(
			FText::Format( LOCTEXT( "WorldLabel", "World"), Label ),
			FText::Format( LOCTEXT( "WorldLabel_ToolTip", "{0} is relative to the world"), Label ),
			FSlateIcon(),
			SetWorldLocationAction,
			NAME_None,
			EUserInterfaceActionType::RadioButton
		);

		MenuBuilder.EndSection();

		return 
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.HAlign(HAlign_Left)
			[
				SNew(SComboButton)
				.ContentPadding( 0 )
				.ButtonStyle( FEditorStyle::Get(), "NoBorder" )
				.ForegroundColor( FSlateColor::UseForeground() )
				.MenuContent()
				[
					MenuBuilder.MakeWidget()
				]
				.ButtonContent()
				[
					SNew( SBox )
					.Padding( FMargin( 0.0f, 0.0f, 2.0f, 0.0f ) )
					[
						SNew(STextBlock)
						.Text_Static(&GetTransformFieldText, bValuePtr, Label)
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
				]
			];
	}
}

void FBoneProxyDetailsCustomization::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	BoneProxies.Empty();
	Algo::TransformIf(Objects, BoneProxies, [](TWeakObjectPtr<UObject> InItem) { return InItem.IsValid() && InItem.Get()->IsA<UBoneProxy>(); }, [](TWeakObjectPtr<UObject> InItem) { return CastChecked<UBoneProxy>(InItem.Get()); });
	TArrayView<UBoneProxy*> BoneProxiesView(BoneProxies);

	UBoneProxy* FirstBoneProxy = CastChecked<UBoneProxy>(Objects[0].Get());

	bool bIsEditingEnabled = true;
	if (UDebugSkelMeshComponent* Component = FirstBoneProxy->SkelMeshComponent.Get())
	{
		bIsEditingEnabled = (Component->AnimScriptInstance == Component->PreviewInstance);
	}

	TSharedRef<IPropertyHandle> LocationProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UBoneProxy, Location));
	TSharedRef<IPropertyHandle> RotationProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UBoneProxy, Rotation));
	TSharedRef<IPropertyHandle> ScaleProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UBoneProxy, Scale));

	IDetailCategoryBuilder& CategoryBuilder = DetailBuilder.EditCategory(TEXT("Transform"));

	TSharedPtr<SWidget> NameWidget;
	TSharedPtr<SWidget> ValueWidget;

	IDetailPropertyRow& LocationPropertyRow = CategoryBuilder.AddProperty(LocationProperty);
	LocationPropertyRow.GetDefaultWidgets(NameWidget, ValueWidget);
	LocationPropertyRow.OverrideResetToDefault(FResetToDefaultOverride::Create(FIsResetToDefaultVisible::CreateSP(this, &FBoneProxyDetailsCustomization::IsResetLocationVisible, BoneProxiesView), FResetToDefaultHandler::CreateSP(this, &FBoneProxyDetailsCustomization::HandleResetLocation, BoneProxiesView)));

	LocationPropertyRow.CustomWidget()
	.NameContent()
	[
		BuildTransformFieldLabel(&FirstBoneProxy->bLocalLocation, LOCTEXT("Location", "Location"), Objects.Num() > 1)
	]
	.ValueContent()
	.MinDesiredWidth(BoneProxyCustomizationConstants::ItemWidth * 3.0f)
	.MaxDesiredWidth(BoneProxyCustomizationConstants::ItemWidth * 3.0f)
	[
		SNew(SBox)
		.IsEnabled(bIsEditingEnabled)
		[
			ValueWidget.ToSharedRef()
		]
	];

	IDetailPropertyRow& RotationPropertyRow = CategoryBuilder.AddProperty(RotationProperty);
	RotationPropertyRow.GetDefaultWidgets(NameWidget, ValueWidget);
	RotationPropertyRow.OverrideResetToDefault(FResetToDefaultOverride::Create(FIsResetToDefaultVisible::CreateSP(this, &FBoneProxyDetailsCustomization::IsResetRotationVisible, BoneProxiesView), FResetToDefaultHandler::CreateSP(this, &FBoneProxyDetailsCustomization::HandleResetRotation, BoneProxiesView)));

	RotationPropertyRow.CustomWidget()
	.NameContent()
	[
		BuildTransformFieldLabel(&FirstBoneProxy->bLocalRotation, LOCTEXT("Rotation", "Rotation"), Objects.Num() > 1)
	]
	.ValueContent()
	.MinDesiredWidth(BoneProxyCustomizationConstants::ItemWidth * 3.0f)
	.MaxDesiredWidth(BoneProxyCustomizationConstants::ItemWidth * 3.0f)
	[
		SNew(SBox)
		.IsEnabled(bIsEditingEnabled)
		[
			ValueWidget.ToSharedRef()
		]
	];

	IDetailPropertyRow& ScalePropertyRow = CategoryBuilder.AddProperty(ScaleProperty);
	ScalePropertyRow.GetDefaultWidgets(NameWidget, ValueWidget);
	ScalePropertyRow.OverrideResetToDefault(FResetToDefaultOverride::Create(FIsResetToDefaultVisible::CreateSP(this, &FBoneProxyDetailsCustomization::IsResetScaleVisible, BoneProxiesView), FResetToDefaultHandler::CreateSP(this, &FBoneProxyDetailsCustomization::HandleResetScale, BoneProxiesView)));

	ScalePropertyRow.CustomWidget()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(ScaleProperty->GetPropertyDisplayName())
		.Font(DetailBuilder.GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(BoneProxyCustomizationConstants::ItemWidth * 3.0f)
	.MaxDesiredWidth(BoneProxyCustomizationConstants::ItemWidth * 3.0f)
	[
		SNew(SBox)
		.IsEnabled(bIsEditingEnabled)
		[
			ValueWidget.ToSharedRef()
		]
	];
}

bool FBoneProxyDetailsCustomization::IsResetLocationVisible(TSharedPtr<IPropertyHandle> InPropertyHandle, TArrayView<UBoneProxy*> InBoneProxies)
{
	for (UBoneProxy* BoneProxy : InBoneProxies)
	{
		if (UDebugSkelMeshComponent* Component = BoneProxy->SkelMeshComponent.Get())
		{
			if (FAnimNode_ModifyBone* ModifyBone = Component->PreviewInstance->FindModifiedBone(BoneProxy->BoneName))
			{
				if (ModifyBone->Translation != FVector::ZeroVector)
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool FBoneProxyDetailsCustomization::IsResetRotationVisible(TSharedPtr<IPropertyHandle> InPropertyHandle, TArrayView<UBoneProxy*> InBoneProxies)
{
	for (UBoneProxy* BoneProxy : InBoneProxies)
	{
		if (UDebugSkelMeshComponent* Component = BoneProxy->SkelMeshComponent.Get())
		{
			if (FAnimNode_ModifyBone* ModifyBone = Component->PreviewInstance->FindModifiedBone(BoneProxy->BoneName))
			{
				if (ModifyBone->Rotation != FRotator::ZeroRotator)
				{
					return true;
				}
			}
		}
	}

	return false;
}

bool FBoneProxyDetailsCustomization::IsResetScaleVisible(TSharedPtr<IPropertyHandle> InPropertyHandle, TArrayView<UBoneProxy*> InBoneProxies)
{
	for (UBoneProxy* BoneProxy : InBoneProxies)
	{
		if (UDebugSkelMeshComponent* Component = BoneProxy->SkelMeshComponent.Get())
		{
			if (FAnimNode_ModifyBone* ModifyBone = Component->PreviewInstance->FindModifiedBone(BoneProxy->BoneName))
			{
				if (ModifyBone->Scale != FVector(1.0f))
				{
					return true;
				}
			}
		}
	}

	return false;
}

void FBoneProxyDetailsCustomization::HandleResetLocation(TSharedPtr<IPropertyHandle> InPropertyHandle, TArrayView<UBoneProxy*> InBoneProxies)
{
	FScopedTransaction Transaction(LOCTEXT("ResetLocation", "Reset Location"));

	for (UBoneProxy* BoneProxy : InBoneProxies)
	{
		if (UDebugSkelMeshComponent* Component = BoneProxy->SkelMeshComponent.Get())
		{
			BoneProxy->Modify();
			Component->PreviewInstance->Modify();

			FAnimNode_ModifyBone& ModifyBone = Component->PreviewInstance->ModifyBone(BoneProxy->BoneName);
			ModifyBone.Translation = FVector::ZeroVector;

			RemoveUnnecessaryModifications(Component, ModifyBone);
		}
	}
}

void FBoneProxyDetailsCustomization::HandleResetRotation(TSharedPtr<IPropertyHandle> InPropertyHandle, TArrayView<UBoneProxy*> InBoneProxies)
{
	FScopedTransaction Transaction(LOCTEXT("ResetRotation", "Reset Rotation"));

	for (UBoneProxy* BoneProxy : InBoneProxies)
	{
		if (UDebugSkelMeshComponent* Component = BoneProxy->SkelMeshComponent.Get())
		{
			BoneProxy->Modify();
			Component->PreviewInstance->Modify();

			FAnimNode_ModifyBone& ModifyBone = Component->PreviewInstance->ModifyBone(BoneProxy->BoneName);
			ModifyBone.Rotation = FRotator::ZeroRotator;

			RemoveUnnecessaryModifications(Component, ModifyBone);
		}
	}
}

void FBoneProxyDetailsCustomization::HandleResetScale(TSharedPtr<IPropertyHandle> InPropertyHandle, TArrayView<UBoneProxy*> InBoneProxies)
{
	FScopedTransaction Transaction(LOCTEXT("ResetScale", "Reset Scale"));

	for (UBoneProxy* BoneProxy : InBoneProxies)
	{
		if (UDebugSkelMeshComponent* Component = BoneProxy->SkelMeshComponent.Get())
		{
			BoneProxy->Modify();
			Component->PreviewInstance->Modify();

			FAnimNode_ModifyBone& ModifyBone = Component->PreviewInstance->ModifyBone(BoneProxy->BoneName);
			ModifyBone.Scale = FVector(1.0f);

			RemoveUnnecessaryModifications(Component, ModifyBone);
		}
	}
}

void FBoneProxyDetailsCustomization::RemoveUnnecessaryModifications(UDebugSkelMeshComponent* Component, FAnimNode_ModifyBone& ModifyBone)
{
	if (ModifyBone.Translation == FVector::ZeroVector && ModifyBone.Rotation == FRotator::ZeroRotator && ModifyBone.Scale == FVector(1.0f))
	{
		Component->PreviewInstance->RemoveBoneModification(ModifyBone.BoneToModify.BoneName);
	}
}

#undef LOCTEXT_NAMESPACE
