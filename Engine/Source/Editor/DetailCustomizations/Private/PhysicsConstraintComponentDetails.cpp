// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PhysicsConstraintComponentDetails.h"
#include "Layout/Visibility.h"
#include "Layout/Margin.h"
#include "Misc/Attribute.h"
#include "Input/Reply.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Widgets/SWidget.h"
#include "Widgets/SBoxPanel.h"
#include "Styling/SlateTypes.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "EditorStyleSet.h"
#include "PhysicsEngine/PhysicsConstraintActor.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailGroup.h"
#include "IDetailPropertyRow.h"
#include "DetailCategoryBuilder.h"
#include "PhysicsEngine/ConstraintTypes.h"
#include "PhysicsEngine/ConstraintDrives.h"
#include "PhysicsEngine/ConstraintInstance.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "ScopedTransaction.h"

#define LOCTEXT_NAMESPACE "PhysicsConstraintComponentDetails"
namespace ConstraintDetails
{
	bool GetBoolProperty(TSharedPtr<IPropertyHandle> Prop)
	{
		bool bIsEnabled = false;

		if (Prop->GetValue(bIsEnabled) == FPropertyAccess::Result::Success)
		{
			return bIsEnabled;
		}
		return false;
	}

	TSharedRef<SWidget> JoinPropertyWidgets(TSharedPtr<IPropertyHandle> TargetProperty, FName TargetChildName, TSharedPtr<IPropertyHandle> ParentProperty, FName CheckPropertyName, TSharedPtr<IPropertyHandle>& StoreCheckProperty)
	{
		StoreCheckProperty = ParentProperty->GetChildHandle(CheckPropertyName);
		StoreCheckProperty->MarkHiddenByCustomization();
		TSharedRef<SWidget> TargetWidget = TargetProperty->GetChildHandle(TargetChildName)->CreatePropertyValueWidget();
		TargetWidget->SetEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda([StoreCheckProperty]()
		{
			bool bSet;
			if (StoreCheckProperty->GetValue(bSet) == FPropertyAccess::Result::Success)
			{
				return bSet;
			}

			return false;
		})));

		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(0, 0, 5, 0)
			[
				StoreCheckProperty->CreatePropertyValueWidget()
			]
		+ SHorizontalBox::Slot()
			[
				TargetWidget
			];
	}

	TSharedRef<SWidget> CreateTriFloatWidget(TSharedPtr<IPropertyHandle> Prop1, TSharedPtr<IPropertyHandle> Prop2, TSharedPtr<IPropertyHandle> Prop3, const FText& TransactionName)
	{
		auto GetMultipleFloats = [Prop1, Prop2, Prop3]()
		{
			// RerunConstructionScripts gets run when the new value is set (if the component
			// is part of a blueprint). This causes the Objects being edited to be cleared,
			// and will cause GetValue to fail. Skip checking the values in that case.
			if (Prop1->GetNumPerObjectValues())
			{
				float Val1, Val2, Val3;

				ensure(Prop1->GetValue(Val1) != FPropertyAccess::Fail);
				ensure(Prop2->GetValue(Val2) != FPropertyAccess::Fail);
				ensure(Prop3->GetValue(Val3) != FPropertyAccess::Fail);

				if (Val1 == Val2 && Val2 == Val3)
				{
					return TOptional<float>(Val1);
				}
			}

			return TOptional<float>();
		};

		auto SetMultipleFloatsCommitted = [Prop1, TransactionName, GetMultipleFloats](float NewValue, ETextCommit::Type)
		{
			TOptional<float> CommonFloat = GetMultipleFloats();
			if(!CommonFloat.IsSet() || CommonFloat.GetValue() != NewValue)	//don't bother doing it twice
			{
				// Only set the first property. Others should be handled in PostEditChangeChainProperty.
				// This prevents an issue where multiple sets fail when using BlueprintComponents
				// due to RerunConstructionScripts destroying the edit list.
				FScopedTransaction Transaction(TransactionName);
				ensure(Prop1->SetValue(NewValue) == FPropertyAccess::Result::Success);
			}
		};

		return SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SNew(SNumericEntryBox<float>)
				.OnValueCommitted_Lambda(SetMultipleFloatsCommitted)
				.Value_Lambda(GetMultipleFloats)
				.MinValue(0.f)
			]
			+ SHorizontalBox::Slot()
			.VAlign(VAlign_Center)
			.AutoWidth()
			[
				SNew(SButton)
				.OnClicked_Lambda([Prop1, Prop2, Prop3, TransactionName]()
				{
					FScopedTransaction Transaction(TransactionName);
					Prop1->ResetToDefault();
					Prop2->ResetToDefault();
					Prop3->ResetToDefault();
					return FReply::Handled();
				} )
				.Visibility_Lambda([Prop1]() { return Prop1->DiffersFromDefault() ? EVisibility::Visible : EVisibility::Collapsed; })
				.ContentPadding(FMargin(5.f, 0.f))
				.ToolTipText(Prop1->GetResetToDefaultLabel())
				.ButtonStyle(FEditorStyle::Get(), "NoBorder")
				.Content()
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("PropertyWindow.DiffersFromDefault"))
				]
			];
	}


	bool IsAngularPropertyEqual(TSharedPtr<IPropertyHandle> Prop, EAngularConstraintMotion CheckMotion)
	{
		uint8 Val;
		if (Prop->GetValue(Val) == FPropertyAccess::Result::Success)
		{
			return Val == CheckMotion;
		}
		return false;
	}

}

TSharedRef<IDetailCustomization> FPhysicsConstraintComponentDetails::MakeInstance()
{
	return MakeShareable(new FPhysicsConstraintComponentDetails());
}

void FPhysicsConstraintComponentDetails::AddConstraintBehaviorProperties(IDetailLayoutBuilder& DetailBuilder, TSharedPtr<IPropertyHandle> ConstraintInstance, TSharedPtr<IPropertyHandle> ProfilePropertiesProperty)
{
	IDetailCategoryBuilder& ConstraintCat = DetailBuilder.EditCategory("Constraint Behavior");

	//hide the inner structs that we customize elsewhere
	ProfilePropertiesProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintProfileProperties, LinearLimit))->MarkHiddenByCustomization();
	ProfilePropertiesProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintProfileProperties, ConeLimit))->MarkHiddenByCustomization();
	ProfilePropertiesProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintProfileProperties, TwistLimit))->MarkHiddenByCustomization();
	ProfilePropertiesProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintProfileProperties, LinearDrive))->MarkHiddenByCustomization();
	ProfilePropertiesProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintProfileProperties, AngularDrive))->MarkHiddenByCustomization();
	ProfilePropertiesProperty->MarkHiddenByCustomization();

	//Add properties we want in specific order
	ConstraintCat.AddProperty(ProfilePropertiesProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintProfileProperties, bDisableCollision)));
	ConstraintCat.AddProperty(ProfilePropertiesProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintProfileProperties, bEnableProjection)));


	//Add the rest
	uint32 NumProfileProperties = 0;
	ProfilePropertiesProperty->GetNumChildren(NumProfileProperties);

	for (uint32 ProfileChildIdx = 0; ProfileChildIdx < NumProfileProperties; ++ProfileChildIdx)
	{
		TSharedPtr<IPropertyHandle> ProfileChildProp = ProfilePropertiesProperty->GetChildHandle(ProfileChildIdx);
		if (!ProfileChildProp->IsCustomized())
		{
			ConstraintCat.AddProperty(ProfileChildProp);
		}
	}
}

void FPhysicsConstraintComponentDetails::AddLinearLimits(IDetailLayoutBuilder& DetailBuilder, TSharedPtr<IPropertyHandle> ConstraintInstance, TSharedPtr<IPropertyHandle> ProfilePropertiesProperty)
{
	IDetailCategoryBuilder& LinearLimitCat = DetailBuilder.EditCategory("Linear Limits");
	TSharedPtr<IPropertyHandle> LinearConstraintProperty = ProfilePropertiesProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintProfileProperties, LinearLimit));


	TSharedPtr<IPropertyHandle> LinearXMotionProperty = LinearConstraintProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLinearConstraint, XMotion));
	TSharedPtr<IPropertyHandle> LinearYMotionProperty = LinearConstraintProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLinearConstraint, YMotion));
	TSharedPtr<IPropertyHandle> LinearZMotionProperty = LinearConstraintProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLinearConstraint, ZMotion));

	TArray<TSharedPtr<FString>> LinearLimitOptionNames;
	TArray<FText> LinearLimitOptionTooltips;
	TArray<bool> LinearLimitOptionRestrictItems;

	const int32 ExpectedLinearLimitOptionCount = 3;
	LinearXMotionProperty->GeneratePossibleValues(LinearLimitOptionNames, LinearLimitOptionTooltips, LinearLimitOptionRestrictItems);
	checkf(LinearLimitOptionNames.Num() == ExpectedLinearLimitOptionCount &&
		LinearLimitOptionTooltips.Num() == ExpectedLinearLimitOptionCount &&
		LinearLimitOptionRestrictItems.Num() == ExpectedLinearLimitOptionCount,
		TEXT("It seems the number of enum entries in ELinearConstraintMotion has changed. This must be handled here as well. "));


	uint8 LinearLimitEnum[LCM_MAX] = { LCM_Free, LCM_Limited, LCM_Locked };
	TSharedPtr<IPropertyHandle> LinearLimitProperties[] = { LinearXMotionProperty, LinearYMotionProperty, LinearZMotionProperty };


	for (int32 PropertyIdx = 0; PropertyIdx < 3; ++PropertyIdx)
	{
		TSharedPtr<IPropertyHandle> CurProperty = LinearLimitProperties[PropertyIdx];

		LinearLimitCat.AddProperty(CurProperty).CustomWidget()
			.NameContent()
			[
				SNew(STextBlock)
				.Font(DetailBuilder.GetDetailFont())
				.Text(CurProperty->GetPropertyDisplayName())
				.ToolTipText(CurProperty->GetToolTipText())
			]
		.ValueContent()
			.MinDesiredWidth(125.0f * 3.0f)
			.MaxDesiredWidth(125.0f * 3.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				[
					SNew(SCheckBox)
					.Style(FEditorStyle::Get(), "RadioButton")
					.IsChecked(this, &FPhysicsConstraintComponentDetails::IsLimitRadioChecked, CurProperty, LinearLimitEnum[0])
					.OnCheckStateChanged(this, &FPhysicsConstraintComponentDetails::OnLimitRadioChanged, CurProperty, LinearLimitEnum[0])
					.ToolTipText(LinearLimitOptionTooltips[0])
					[
						SNew(STextBlock)
						.Text(FText::FromString(*LinearLimitOptionNames[0].Get()))
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
				]
				+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					.Padding(5, 0, 0, 0)
					[
						SNew(SCheckBox)
						.Style(FEditorStyle::Get(), "RadioButton")
						.IsChecked(this, &FPhysicsConstraintComponentDetails::IsLimitRadioChecked, CurProperty, LinearLimitEnum[1])
						.OnCheckStateChanged(this, &FPhysicsConstraintComponentDetails::OnLimitRadioChanged, CurProperty, LinearLimitEnum[1])
						.ToolTipText(LinearLimitOptionTooltips[1])
						[
							SNew(STextBlock)
							.Text(FText::FromString(*LinearLimitOptionNames[1].Get()))
							.Font(IDetailLayoutBuilder::GetDetailFont())
						]
					]
				+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					.Padding(5, 0, 0, 0)
					[
						SNew(SCheckBox)
						.Style(FEditorStyle::Get(), "RadioButton")
						.IsChecked(this, &FPhysicsConstraintComponentDetails::IsLimitRadioChecked, CurProperty, LinearLimitEnum[2])
						.OnCheckStateChanged(this, &FPhysicsConstraintComponentDetails::OnLimitRadioChanged, CurProperty, LinearLimitEnum[2])
						.ToolTipText(LinearLimitOptionTooltips[2])
						[
							SNew(STextBlock)
							.Text(FText::FromString(*LinearLimitOptionNames[2].Get()))
							.Font(IDetailLayoutBuilder::GetDetailFont())
						]
					]
			];
	}

	auto IsLinearMotionLimited = [LinearXMotionProperty, LinearYMotionProperty, LinearZMotionProperty]()
	{
		uint8 XMotion, YMotion, ZMotion;
		if (LinearXMotionProperty->GetValue(XMotion) == FPropertyAccess::Result::Success && 
			LinearYMotionProperty->GetValue(YMotion) == FPropertyAccess::Result::Success && 
			LinearZMotionProperty->GetValue(ZMotion) == FPropertyAccess::Result::Success)
		{
			return XMotion == LCM_Limited || YMotion == LCM_Limited || ZMotion == LCM_Limited;
		}

		return false;
	};

	TSharedPtr<IPropertyHandle> SoftProperty = LinearConstraintProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLinearConstraint, bSoftConstraint));

	auto IsRestitutionEnabled = [IsLinearMotionLimited, SoftProperty]()
	{
		return !ConstraintDetails::GetBoolProperty(SoftProperty) && IsLinearMotionLimited();
	};

	LinearLimitCat.AddProperty(LinearConstraintProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLinearConstraint, Limit))).IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(IsLinearMotionLimited)));
	LinearLimitCat.AddProperty(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, bScaleLinearLimits))).IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(IsLinearMotionLimited)));
	LinearLimitCat.AddProperty(LinearConstraintProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLinearConstraint, bSoftConstraint))).IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(IsLinearMotionLimited)));
	LinearLimitCat.AddProperty(LinearConstraintProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLinearConstraint, Stiffness))).IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(IsLinearMotionLimited)));
	LinearLimitCat.AddProperty(LinearConstraintProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLinearConstraint, Damping))).IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(IsLinearMotionLimited)));
	LinearLimitCat.AddProperty(LinearConstraintProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLinearConstraint, Restitution))).IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(IsRestitutionEnabled)));
	LinearLimitCat.AddProperty(LinearConstraintProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLinearConstraint, ContactDistance))).IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(IsLinearMotionLimited)));
	LinearLimitCat.AddProperty(ProfilePropertiesProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintProfileProperties, bLinearBreakable)));
	LinearLimitCat.AddProperty(ProfilePropertiesProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintProfileProperties, LinearBreakThreshold)));
}



void FPhysicsConstraintComponentDetails::AddAngularLimits(IDetailLayoutBuilder& DetailBuilder, TSharedPtr<IPropertyHandle> ConstraintInstance, TSharedPtr<IPropertyHandle> ProfilePropertiesProperty)
{
	IDetailCategoryBuilder& AngularLimitCat = DetailBuilder.EditCategory("Angular Limits");

	TSharedPtr<IPropertyHandle> ConeConstraintProperty = ProfilePropertiesProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintProfileProperties, ConeLimit));
	TSharedPtr<IPropertyHandle> TwistConstraintProperty = ProfilePropertiesProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintProfileProperties, TwistLimit));

	AngularSwing1MotionProperty = ConeConstraintProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConeConstraint, Swing1Motion));
	AngularSwing2MotionProperty = ConeConstraintProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConeConstraint, Swing2Motion));
	AngularTwistMotionProperty = TwistConstraintProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTwistConstraint, TwistMotion));

	TArray<TSharedPtr<FString>> AngularLimitOptionNames;
	TArray<FText> AngularLimitOptionTooltips;
	TArray<bool> AngularLimitOptionRestrictItems;

	const int32 ExpectedAngularLimitOptionCount = 3;
	AngularSwing1MotionProperty->GeneratePossibleValues(AngularLimitOptionNames, AngularLimitOptionTooltips, AngularLimitOptionRestrictItems);
	checkf(AngularLimitOptionNames.Num() == ExpectedAngularLimitOptionCount &&
		AngularLimitOptionTooltips.Num() == ExpectedAngularLimitOptionCount &&
		AngularLimitOptionRestrictItems.Num() == ExpectedAngularLimitOptionCount,
		TEXT("It seems the number of enum entries in EAngularConstraintMotion has changed. This must be handled here as well. "));


	uint8 AngularLimitEnum[LCM_MAX] = { ACM_Free, LCM_Limited, LCM_Locked };
	TSharedPtr<IPropertyHandle> AngularLimitProperties[] = { AngularSwing1MotionProperty, AngularSwing2MotionProperty, AngularTwistMotionProperty };

	const FName AxisStyleNames[3] =
	{
		"PhysicsAssetEditor.RadioButtons.Red",
		"PhysicsAssetEditor.RadioButtons.Red",
		"PhysicsAssetEditor.RadioButtons.Green"
	};

	for (int32 PropertyIdx = 0; PropertyIdx < 3; ++PropertyIdx)
	{
		TSharedPtr<IPropertyHandle> CurProperty = AngularLimitProperties[PropertyIdx];

		AngularLimitCat.AddProperty(CurProperty).CustomWidget()
			.NameContent()
			[
				SNew(STextBlock)
				.Font(DetailBuilder.GetDetailFont())
				.Text(CurProperty->GetPropertyDisplayName())
				.ToolTipText(CurProperty->GetToolTipText())
			]
		.ValueContent()
			.MinDesiredWidth(125.0f * 3.0f)
			.MaxDesiredWidth(125.0f * 3.0f)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.HAlign(HAlign_Left)
				[
					SNew(SCheckBox)
					.Style(FEditorStyle::Get(), AxisStyleNames[PropertyIdx])
					.IsChecked(this, &FPhysicsConstraintComponentDetails::IsLimitRadioChecked, CurProperty, AngularLimitEnum[0])
					.OnCheckStateChanged(this, &FPhysicsConstraintComponentDetails::OnLimitRadioChanged, CurProperty, AngularLimitEnum[0])
					.ToolTipText(AngularLimitOptionTooltips[0])
					[
						SNew(STextBlock)
						.Text(FText::FromString(*AngularLimitOptionNames[0].Get()))
						.Font(IDetailLayoutBuilder::GetDetailFont())
					]
				]
				+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					.Padding(5, 0, 0, 0)
					[
						SNew(SCheckBox)
						.Style(FEditorStyle::Get(), AxisStyleNames[PropertyIdx])
						.IsChecked(this, &FPhysicsConstraintComponentDetails::IsLimitRadioChecked, CurProperty, AngularLimitEnum[1])
						.OnCheckStateChanged(this, &FPhysicsConstraintComponentDetails::OnLimitRadioChanged, CurProperty, AngularLimitEnum[1])
						.ToolTipText(AngularLimitOptionTooltips[1])
						[
							SNew(STextBlock)
							.Text(FText::FromString(*AngularLimitOptionNames[1].Get()))
							.Font(IDetailLayoutBuilder::GetDetailFont())
						]
					]
				+ SHorizontalBox::Slot()
					.AutoWidth()
					.HAlign(HAlign_Left)
					.Padding(5, 0, 0, 0)
					[
						SNew(SCheckBox)
						.Style(FEditorStyle::Get(), AxisStyleNames[PropertyIdx])
						.IsChecked(this, &FPhysicsConstraintComponentDetails::IsLimitRadioChecked, CurProperty, AngularLimitEnum[2])
						.OnCheckStateChanged(this, &FPhysicsConstraintComponentDetails::OnLimitRadioChanged, CurProperty, AngularLimitEnum[2])
						.ToolTipText(AngularLimitOptionTooltips[2])
						[
							SNew(STextBlock)
							.Text(FText::FromString(*AngularLimitOptionNames[2].Get()))
							.Font(IDetailLayoutBuilder::GetDetailFont())
						]
					]
			];
	}

	AngularLimitCat.AddProperty(ConeConstraintProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConeConstraint, Swing1LimitDegrees)).ToSharedRef()).IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::AngularSwing1Limit)));
	AngularLimitCat.AddProperty(ConeConstraintProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConeConstraint, Swing2LimitDegrees)).ToSharedRef()).IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::AngularSwing2Limit)));
	AngularLimitCat.AddProperty(TwistConstraintProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTwistConstraint, TwistLimitDegrees)).ToSharedRef()).IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::AngularTwistLimit)));

	TSharedPtr<IPropertyHandle> SoftSwingProperty = ConeConstraintProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConeConstraint, bSoftConstraint));
	auto SwingRestitutionEnabled = [this, SoftSwingProperty]()
	{
		return !ConstraintDetails::GetBoolProperty(SoftSwingProperty) && (IsPropertyEnabled(EPropertyType::AngularSwing1Limit) || IsPropertyEnabled(EPropertyType::AngularSwing2Limit));
	};

	IDetailGroup& SwingGroup = AngularLimitCat.AddGroup("Swing Limits", LOCTEXT("SwingLimits", "Swing Limits"), true, true);

	SwingGroup.AddPropertyRow(SoftSwingProperty.ToSharedRef()).IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::AngularSwingLimit)));
	SwingGroup.AddPropertyRow(ConeConstraintProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConeConstraint, Stiffness)).ToSharedRef()).IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::AngularSwingLimit)));
	SwingGroup.AddPropertyRow(ConeConstraintProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConeConstraint, Damping)).ToSharedRef()).IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::AngularSwingLimit)));
	SwingGroup.AddPropertyRow(ConeConstraintProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConeConstraint, Restitution)).ToSharedRef()).IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(SwingRestitutionEnabled)));
	SwingGroup.AddPropertyRow(ConeConstraintProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConeConstraint, ContactDistance)).ToSharedRef()).IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::AngularSwingLimit)));

	TSharedPtr<IPropertyHandle> SoftTwistProperty = TwistConstraintProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTwistConstraint, bSoftConstraint));
	auto TwistRestitutionEnabled = [this, SoftTwistProperty]()
	{
		return !ConstraintDetails::GetBoolProperty(SoftTwistProperty) && IsPropertyEnabled(EPropertyType::AngularTwistLimit);
	};

	IDetailGroup& TwistGroup = AngularLimitCat.AddGroup("Twist Limits", LOCTEXT("TwistLimits", "Twist Limits"), true, true);

	TwistGroup.AddPropertyRow(SoftTwistProperty.ToSharedRef()).IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::AngularTwistLimit)));
	TwistGroup.AddPropertyRow(TwistConstraintProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTwistConstraint, Stiffness)).ToSharedRef()).IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::AngularTwistLimit)));
	TwistGroup.AddPropertyRow(TwistConstraintProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTwistConstraint, Damping)).ToSharedRef()).IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::AngularTwistLimit)));
	TwistGroup.AddPropertyRow(TwistConstraintProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTwistConstraint, Restitution)).ToSharedRef()).IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(TwistRestitutionEnabled)));
	TwistGroup.AddPropertyRow(TwistConstraintProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FTwistConstraint, ContactDistance)).ToSharedRef()).IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::AngularTwistLimit)));

	if (bInPhat == false)
	{
	    AngularLimitCat.AddProperty(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, AngularRotationOffset)).ToSharedRef());
	}
	else
	{
		AngularLimitCat.AddProperty(ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, AngularRotationOffset)).ToSharedRef())
			.Visibility(EVisibility::Collapsed);
	}

	AngularLimitCat.AddProperty(ProfilePropertiesProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintProfileProperties, bAngularBreakable)).ToSharedRef());
	AngularLimitCat.AddProperty(ProfilePropertiesProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintProfileProperties, AngularBreakThreshold)).ToSharedRef());
}

void FPhysicsConstraintComponentDetails::AddLinearDrive(IDetailLayoutBuilder& DetailBuilder, TSharedPtr<IPropertyHandle> ConstraintInstance, TSharedPtr<IPropertyHandle> ProfilePropertiesProperty)
{
	IDetailCategoryBuilder& LinearMotorCat = DetailBuilder.EditCategory("LinearMotor");

	TSharedPtr<IPropertyHandle> LinearDriveProperty = ProfilePropertiesProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintProfileProperties, LinearDrive));

	IDetailGroup& PositionGroup = LinearMotorCat.AddGroup("Linear Position Drive", LOCTEXT("LinearPositionDrive", "Linear Position Drive"), false, true);

	TSharedRef<IPropertyHandle> LinearPositionTargetProperty = LinearDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLinearDriveConstraint, PositionTarget)).ToSharedRef();

	TSharedPtr<IPropertyHandle> XDriveProperty = LinearDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLinearDriveConstraint, XDrive));
	TSharedPtr<IPropertyHandle> YDriveProperty = LinearDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLinearDriveConstraint, YDrive));
	TSharedPtr<IPropertyHandle> ZDriveProperty = LinearDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLinearDriveConstraint, ZDrive));

	LinearXPositionDriveProperty = XDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintDrive, bEnablePositionDrive));
	LinearYPositionDriveProperty = YDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintDrive, bEnablePositionDrive));
	LinearZPositionDriveProperty = ZDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintDrive, bEnablePositionDrive));

	TSharedRef<SWidget> LinearPositionXWidget = ConstraintDetails::JoinPropertyWidgets(LinearPositionTargetProperty, FName("X"), XDriveProperty, GET_MEMBER_NAME_CHECKED(FConstraintDrive, bEnablePositionDrive), LinearXPositionDriveProperty);
	TSharedRef<SWidget> LinearPositionYWidget = ConstraintDetails::JoinPropertyWidgets(LinearPositionTargetProperty, FName("Y"), YDriveProperty, GET_MEMBER_NAME_CHECKED(FConstraintDrive, bEnablePositionDrive), LinearYPositionDriveProperty);
	TSharedRef<SWidget> LinearPositionZWidget = ConstraintDetails::JoinPropertyWidgets(LinearPositionTargetProperty, FName("Z"), ZDriveProperty, GET_MEMBER_NAME_CHECKED(FConstraintDrive, bEnablePositionDrive), LinearZPositionDriveProperty);


	FDetailWidgetRow& LinearPositionTargetWidget = PositionGroup.HeaderProperty(LinearPositionTargetProperty).CustomWidget()
	.NameContent()
	[
			LinearPositionTargetProperty->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(125 * 3 + 18 * 3)
	.MaxDesiredWidth(125 * 3 + 18 * 3)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			LinearPositionXWidget
		]
		+ SHorizontalBox::Slot()
		.Padding(5, 0, 0, 0)
		[
			LinearPositionYWidget
		]

		+ SHorizontalBox::Slot()
		.Padding(5, 0, 0, 0)
		[
			LinearPositionZWidget
		]
	];

	TSharedPtr<IPropertyHandle> StiffnessXProperty = XDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintDrive, Stiffness));
	TSharedRef<SWidget> StiffnessWidget = ConstraintDetails::CreateTriFloatWidget(StiffnessXProperty, YDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintDrive, Stiffness)), ZDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintDrive, Stiffness)), LOCTEXT("EditStrength", "Edit Strength"));
	StiffnessWidget->SetEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::LinearPositionDrive)));

	PositionGroup.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("Strength", "Strength"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	[
		StiffnessWidget
	];

	// VELOCITY

	IDetailGroup& VelocityGroup = LinearMotorCat.AddGroup("Linear Velocity Drive", LOCTEXT("LinearVelocityDrive", "Linear Velocity Drive"), false, true);

	TSharedRef<IPropertyHandle> LinearVelocityTargetProperty = LinearDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FLinearDriveConstraint, VelocityTarget)).ToSharedRef();

	LinearXVelocityDriveProperty = XDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintDrive, bEnableVelocityDrive));
	LinearYVelocityDriveProperty = YDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintDrive, bEnableVelocityDrive));
	LinearZVelocityDriveProperty = ZDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintDrive, bEnableVelocityDrive));

	TSharedRef<SWidget> LinearVelocityXWidget = ConstraintDetails::JoinPropertyWidgets(LinearVelocityTargetProperty, FName("X"), XDriveProperty, GET_MEMBER_NAME_CHECKED(FConstraintDrive, bEnableVelocityDrive), LinearXVelocityDriveProperty);
	TSharedRef<SWidget> LinearVelocityYWidget = ConstraintDetails::JoinPropertyWidgets(LinearVelocityTargetProperty, FName("Y"), YDriveProperty, GET_MEMBER_NAME_CHECKED(FConstraintDrive, bEnableVelocityDrive), LinearYVelocityDriveProperty);
	TSharedRef<SWidget> LinearVelocityZWidget = ConstraintDetails::JoinPropertyWidgets(LinearVelocityTargetProperty, FName("Z"), ZDriveProperty, GET_MEMBER_NAME_CHECKED(FConstraintDrive, bEnableVelocityDrive), LinearZVelocityDriveProperty);

	FDetailWidgetRow& LinearVelocityTargetWidget = VelocityGroup.HeaderProperty(LinearVelocityTargetProperty).CustomWidget(true);
	LinearVelocityTargetWidget.NameContent()
		[
			LinearVelocityTargetProperty->CreatePropertyNameWidget()
		];

	LinearVelocityTargetWidget.ValueContent()
		.MinDesiredWidth(125 * 3 + 18 * 3)
		.MaxDesiredWidth(125 * 3 + 18 * 3)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
		[
			LinearVelocityXWidget
		]
	+ SHorizontalBox::Slot()
		.Padding(5, 0, 0, 0)
		[
			LinearVelocityYWidget
		]

	+ SHorizontalBox::Slot()
		.Padding(5, 0, 0, 0)
		[
			LinearVelocityZWidget
		]
		];

	TSharedPtr<IPropertyHandle> XDampingProperty = XDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintDrive, Damping));
	TSharedRef<SWidget> DampingWidget = ConstraintDetails::CreateTriFloatWidget(XDampingProperty, YDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintDrive, Damping)), ZDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintDrive, Damping)), LOCTEXT("EditStrength", "Edit Strength"));
	DampingWidget->SetEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::LinearVelocityDrive)));

	VelocityGroup.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("Strength", "Strength"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	[
		DampingWidget
	];

	// max force limit
	TSharedPtr<IPropertyHandle> MaxForceProperty = XDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintDrive, MaxForce));
	TSharedRef<SWidget> MaxForceWidget = ConstraintDetails::CreateTriFloatWidget(MaxForceProperty, YDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintDrive, MaxForce)), ZDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintDrive, MaxForce)), LOCTEXT("EditMaxForce", "Edit Max Force"));
	MaxForceWidget->SetEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateSP(this, &FPhysicsConstraintComponentDetails::IsPropertyEnabled, EPropertyType::LinearDrive)));

	LinearMotorCat.AddCustomRow(LOCTEXT("MaxForce", "Max Force"), true)
	.NameContent()
	[
		MaxForceProperty->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		MaxForceWidget
	];

}

void FPhysicsConstraintComponentDetails::AddAngularDrive(IDetailLayoutBuilder& DetailBuilder, TSharedPtr<IPropertyHandle> ConstraintInstance, TSharedPtr<IPropertyHandle> ProfilePropertiesProperty)
{
	IDetailCategoryBuilder& AngularMotorCat = DetailBuilder.EditCategory("AngularMotor");

	TSharedPtr<IPropertyHandle> AngularDriveProperty = ProfilePropertiesProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintProfileProperties, AngularDrive));
	TSharedPtr<IPropertyHandle> AngularDriveModeProperty = AngularDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAngularDriveConstraint, AngularDriveMode));

	TSharedPtr<IPropertyHandle> SlerpDriveProperty = AngularDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAngularDriveConstraint, SlerpDrive));
	TSharedPtr<IPropertyHandle> SwingDriveProperty = AngularDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAngularDriveConstraint, SwingDrive));
	TSharedPtr<IPropertyHandle> TwistDriveProperty = AngularDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAngularDriveConstraint, TwistDrive));

	TSharedPtr<IPropertyHandle> SlerpPositionDriveProperty = SlerpDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintDrive, bEnablePositionDrive));
	TSharedPtr<IPropertyHandle> SlerpVelocityDriveProperty = SlerpDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintDrive, bEnableVelocityDrive));
	TSharedPtr<IPropertyHandle> SwingPositionDriveProperty = SwingDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintDrive, bEnablePositionDrive));
	TSharedPtr<IPropertyHandle> SwingVelocityDriveProperty = SwingDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintDrive, bEnableVelocityDrive));
	TSharedPtr<IPropertyHandle> TwistPositionDriveProperty = TwistDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintDrive, bEnablePositionDrive));
	TSharedPtr<IPropertyHandle> TwistVelocityDriveProperty = TwistDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintDrive, bEnableVelocityDrive));

	auto IsAngularMode = [AngularDriveModeProperty](EAngularDriveMode::Type CheckMode)
	{
		uint8 DriveMode;
		if (AngularDriveModeProperty->GetValue(DriveMode) == FPropertyAccess::Result::Success)
		{
			return DriveMode == CheckMode;
		}
		return false;
	};

	auto EligibleForSLERP = [this, IsAngularMode]()
	{
		return IsAngularMode(EAngularDriveMode::SLERP) && !ConstraintDetails::IsAngularPropertyEqual(AngularSwing1MotionProperty, ACM_Locked) && !ConstraintDetails::IsAngularPropertyEqual(AngularSwing2MotionProperty, ACM_Locked) && !ConstraintDetails::IsAngularPropertyEqual(AngularTwistMotionProperty, ACM_Locked);
	};

	auto EligibleForTwistAndSwing = [IsAngularMode]()
	{
		return IsAngularMode(EAngularDriveMode::TwistAndSwing);
	};

	auto OrientationEnabled = [EligibleForSLERP, EligibleForTwistAndSwing, TwistPositionDriveProperty, SwingPositionDriveProperty, SlerpPositionDriveProperty]()
	{
		if(EligibleForSLERP())
		{
			return ConstraintDetails::GetBoolProperty(SlerpPositionDriveProperty);
		} else if(EligibleForTwistAndSwing())
		{
			return ConstraintDetails::GetBoolProperty(TwistPositionDriveProperty) || ConstraintDetails::GetBoolProperty(SwingPositionDriveProperty);
		}

		return false;
	};

	auto VelocityEnabled = [EligibleForSLERP, EligibleForTwistAndSwing, TwistVelocityDriveProperty, SwingVelocityDriveProperty, SlerpVelocityDriveProperty]()
	{
		if (EligibleForSLERP())
		{
			return ConstraintDetails::GetBoolProperty(SlerpVelocityDriveProperty);
		}
		else if (EligibleForTwistAndSwing())
		{
			return ConstraintDetails::GetBoolProperty(TwistVelocityDriveProperty) || ConstraintDetails::GetBoolProperty(SwingVelocityDriveProperty);
		}

		return false;
	};

	auto VelocityOrOrientationEnabled = [VelocityEnabled, OrientationEnabled]()
	{
		return VelocityEnabled() || OrientationEnabled();
	};

	AngularMotorCat.AddProperty(AngularDriveModeProperty);

	IDetailGroup& OrientationGroup = AngularMotorCat.AddGroup("Orientation Drive", LOCTEXT("OrientrationDrive", "Orientation Drive"), false, true);
	OrientationGroup.HeaderProperty(AngularDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAngularDriveConstraint, OrientationTarget)).ToSharedRef()).DisplayName(LOCTEXT("TargetOrientation", "Target Orientation")).IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(OrientationEnabled)));


	TSharedRef<SWidget> SlerpPositionWidget = SlerpPositionDriveProperty->CreatePropertyValueWidget();
	TSharedRef<SWidget> SlerpVelocityWidget = SlerpVelocityDriveProperty->CreatePropertyValueWidget();
	SlerpPositionWidget->SetEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(EligibleForSLERP)));
	SlerpVelocityWidget->SetEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(EligibleForSLERP)));

	TSharedRef<SWidget> TwistPositionWidget = TwistPositionDriveProperty->CreatePropertyValueWidget();
	TSharedRef<SWidget> TwistVelocityWidget = TwistVelocityDriveProperty->CreatePropertyValueWidget();
	TwistPositionWidget->SetEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(EligibleForTwistAndSwing)));
	TwistVelocityWidget->SetEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(EligibleForTwistAndSwing)));

	TSharedRef<SWidget> SwingPositionWidget = SwingPositionDriveProperty->CreatePropertyValueWidget();
	TSharedRef<SWidget> SwingVelocityWidget = SwingVelocityDriveProperty->CreatePropertyValueWidget();
	SwingPositionWidget->SetEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(EligibleForTwistAndSwing)));
	SwingVelocityWidget->SetEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(EligibleForTwistAndSwing)));

	OrientationGroup.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("TwistSwingSlerpDrive", "Drives"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(125 * 3 + 18 * 3)
	.MaxDesiredWidth(125 * 3 + 18 * 3)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SlerpDriveProperty->CreatePropertyNameWidget()
			]
			+ SHorizontalBox::Slot()
			[
				SlerpPositionWidget
			]
		]
		+ SHorizontalBox::Slot()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				TwistDriveProperty->CreatePropertyNameWidget()
			]
			+ SHorizontalBox::Slot()
			[
				TwistPositionWidget
			]
		]
		+ SHorizontalBox::Slot()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SwingDriveProperty->CreatePropertyNameWidget()
			]
			+ SHorizontalBox::Slot()
			[
				SwingPositionWidget
			]
		]
	];

	TSharedPtr<IPropertyHandle> StiffnessSlerpProperty = SlerpDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintDrive, Stiffness));
	TSharedRef<SWidget> OrientationStrengthWidget = ConstraintDetails::CreateTriFloatWidget(StiffnessSlerpProperty, TwistDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintDrive, Stiffness)), SwingDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintDrive, Stiffness)), LOCTEXT("EditStrength", "Edit Strength"));
	OrientationStrengthWidget->SetEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(OrientationEnabled)));

	OrientationGroup.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("Strength", "Strength"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	[
		OrientationStrengthWidget
	];

	IDetailGroup& AngularVelocityGroup = AngularMotorCat.AddGroup("Velocity Drive", LOCTEXT("VelocityDrive", "Velocity Drive"), false, true);
	AngularVelocityGroup.HeaderProperty(AngularDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FAngularDriveConstraint, AngularVelocityTarget)).ToSharedRef()).DisplayName(LOCTEXT("TargetVelocity", "Target Velocity")).IsEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(VelocityEnabled)));

	AngularVelocityGroup.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("TwistSwingSlerpDrive", "Drives"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	.MinDesiredWidth(125 * 3 + 18 * 3)
	.MaxDesiredWidth(125 * 3 + 18 * 3)
	[
		SNew(SHorizontalBox)
		+ SHorizontalBox::Slot()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SlerpDriveProperty->CreatePropertyNameWidget()
			]
			+ SHorizontalBox::Slot()
			[
				SlerpVelocityWidget
			]
		]
		+ SHorizontalBox::Slot()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				TwistDriveProperty->CreatePropertyNameWidget()
			]
			+ SHorizontalBox::Slot()
			[
				TwistVelocityWidget
			]
		]
		+ SHorizontalBox::Slot()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			[
				SwingDriveProperty->CreatePropertyNameWidget()
			]
			+ SHorizontalBox::Slot()
			[
				SwingVelocityWidget
			]
		]
	];

	TSharedPtr<IPropertyHandle> DampingSlerpProperty = SlerpDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintDrive, Damping));
	TSharedRef<SWidget> DampingSlerpWidget = ConstraintDetails::CreateTriFloatWidget(DampingSlerpProperty, TwistDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintDrive, Damping)), SwingDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintDrive, Damping)), LOCTEXT("EditStrength", "Edit Strength"));
	DampingSlerpWidget->SetEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(VelocityEnabled)));
	AngularVelocityGroup.AddWidgetRow()
	.NameContent()
	[
		SNew(STextBlock)
		.Text(LOCTEXT("Strength", "Strength"))
		.Font(IDetailLayoutBuilder::GetDetailFont())
	]
	.ValueContent()
	[
		DampingSlerpWidget
	];

	// max force limit
	TSharedPtr<IPropertyHandle> MaxForcePropertySlerp = SlerpDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintDrive, MaxForce));
	TSharedRef<SWidget> MaxForceWidget = ConstraintDetails::CreateTriFloatWidget(MaxForcePropertySlerp, TwistDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintDrive, MaxForce)), SwingDriveProperty->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintDrive, MaxForce)), LOCTEXT("EditMaxForce", "Edit Max Force"));
	MaxForceWidget->SetEnabled(TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(VelocityOrOrientationEnabled)));

	AngularMotorCat.AddCustomRow(LOCTEXT("MaxForce", "Max Force"), true)
	.NameContent()
	[
		MaxForcePropertySlerp->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		MaxForceWidget
	];


}

void FPhysicsConstraintComponentDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	TArray<TWeakObjectPtr<UObject>> Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	TSharedPtr<IPropertyHandle> ConstraintInstance;
	UPhysicsConstraintComponent* ConstraintComp = NULL;
	APhysicsConstraintActor* OwningConstraintActor = NULL;

	bInPhat = false;

	for (int32 i=0; i < Objects.Num(); ++i)
	{
		if (!Objects[i].IsValid()) { continue; }

		if (Objects[i]->IsA(UPhysicsConstraintTemplate::StaticClass()))
		{
			ConstraintInstance = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UPhysicsConstraintTemplate, DefaultInstance));
			bInPhat = true;
			break;
		}
		else if (Objects[i]->IsA(UPhysicsConstraintComponent::StaticClass()))
		{
			ConstraintInstance = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UPhysicsConstraintComponent, ConstraintInstance));
			ConstraintComp = (UPhysicsConstraintComponent*)Objects[i].Get();
			OwningConstraintActor = Cast<APhysicsConstraintActor>(ConstraintComp->GetOwner());
			break;
		}
	}

	DetailBuilder.EditCategory("Constraint");	//Create this category first so it's at the top
	DetailBuilder.EditCategory("Constraint Behavior");	//Create this category first so it's at the top

	TSharedPtr<IPropertyHandle> ProfileInstance = ConstraintInstance->GetChildHandle(GET_MEMBER_NAME_CHECKED(FConstraintInstance, ProfileInstance));
	AddLinearLimits(DetailBuilder, ConstraintInstance, ProfileInstance);
	AddAngularLimits(DetailBuilder, ConstraintInstance, ProfileInstance);
	AddLinearDrive(DetailBuilder, ConstraintInstance, ProfileInstance);
	AddAngularDrive(DetailBuilder, ConstraintInstance, ProfileInstance);

	AddConstraintBehaviorProperties(DetailBuilder, ConstraintInstance, ProfileInstance);	//Now we've added all the complex UI, just dump the rest into Constraint category
}


bool FPhysicsConstraintComponentDetails::IsPropertyEnabled( EPropertyType::Type Type ) const
{
	bool bIsVisible = false;
	switch (Type)
	{
		case EPropertyType::LinearXPositionDrive:	return ConstraintDetails::GetBoolProperty(LinearXPositionDriveProperty);
		case EPropertyType::LinearYPositionDrive:	return ConstraintDetails::GetBoolProperty(LinearYPositionDriveProperty);
		case EPropertyType::LinearZPositionDrive:	return ConstraintDetails::GetBoolProperty(LinearZPositionDriveProperty);

		case EPropertyType::LinearXVelocityDrive:	return ConstraintDetails::GetBoolProperty(LinearXVelocityDriveProperty);
		case EPropertyType::LinearYVelocityDrive:	return ConstraintDetails::GetBoolProperty(LinearYVelocityDriveProperty);
		case EPropertyType::LinearZVelocityDrive:	return ConstraintDetails::GetBoolProperty(LinearZVelocityDriveProperty);
		case EPropertyType::LinearPositionDrive:	return ConstraintDetails::GetBoolProperty(LinearXPositionDriveProperty) || ConstraintDetails::GetBoolProperty(LinearYPositionDriveProperty) || ConstraintDetails::GetBoolProperty(LinearZPositionDriveProperty);
		case EPropertyType::LinearVelocityDrive:	return ConstraintDetails::GetBoolProperty(LinearXVelocityDriveProperty) || ConstraintDetails::GetBoolProperty(LinearYVelocityDriveProperty) || ConstraintDetails::GetBoolProperty(LinearZVelocityDriveProperty);
		case EPropertyType::LinearDrive:			return ConstraintDetails::GetBoolProperty(LinearXPositionDriveProperty) || ConstraintDetails::GetBoolProperty(LinearYPositionDriveProperty) || ConstraintDetails::GetBoolProperty(LinearZPositionDriveProperty)
															|| ConstraintDetails::GetBoolProperty(LinearXVelocityDriveProperty) || ConstraintDetails::GetBoolProperty(LinearYVelocityDriveProperty) || ConstraintDetails::GetBoolProperty(LinearZVelocityDriveProperty);
		case EPropertyType::AngularSwing1Limit:		return ConstraintDetails::IsAngularPropertyEqual(AngularSwing1MotionProperty, ACM_Limited);
		case EPropertyType::AngularSwing2Limit:		return ConstraintDetails::IsAngularPropertyEqual(AngularSwing2MotionProperty, ACM_Limited);
		case EPropertyType::AngularSwingLimit:		return ConstraintDetails::IsAngularPropertyEqual(AngularSwing1MotionProperty, ACM_Limited) || ConstraintDetails::IsAngularPropertyEqual(AngularSwing2MotionProperty, ACM_Limited);
		case EPropertyType::AngularTwistLimit:		return ConstraintDetails::IsAngularPropertyEqual(AngularTwistMotionProperty, ACM_Limited);
		case EPropertyType::AngularAnyLimit:		return ConstraintDetails::IsAngularPropertyEqual(AngularSwing1MotionProperty, ACM_Limited) || ConstraintDetails::IsAngularPropertyEqual(AngularSwing2MotionProperty, ACM_Limited) || ConstraintDetails::IsAngularPropertyEqual(AngularTwistMotionProperty, ACM_Limited);
	}

	return bIsVisible;
}

ECheckBoxState FPhysicsConstraintComponentDetails::IsLimitRadioChecked( TSharedPtr<IPropertyHandle> Property, uint8 Value ) const
{
	uint8 PropertyEnumValue = 0;
	if (Property.IsValid() && Property->GetValue(PropertyEnumValue) == FPropertyAccess::Result::Success)
	{
		return PropertyEnumValue == Value ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
	}
	return ECheckBoxState::Unchecked;
}

void FPhysicsConstraintComponentDetails::OnLimitRadioChanged( ECheckBoxState CheckType, TSharedPtr<IPropertyHandle> Property, uint8 Value )
{
	if (Property.IsValid() && CheckType == ECheckBoxState::Checked)
	{
		Property->SetValue(Value);
	}
}

#undef LOCTEXT_NAMESPACE
