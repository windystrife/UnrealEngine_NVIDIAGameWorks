// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AttenuationSettingsCustomizations.h"
#include "Widgets/Text/STextBlock.h"
#include "PropertyRestriction.h"
#include "Engine/Attenuation.h"
#include "Audio.h"
#include "IDetailChildrenBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailCategoryBuilder.h"
#include "DetailWidgetRow.h"
#include "IDetailPropertyRow.h"
#include "AudioDevice.h"
#include "Classes/Sound/AudioSettings.h"

TSharedRef<IPropertyTypeCustomization> FSoundAttenuationSettingsCustomization::MakeInstance() 
{
	return MakeShareable( new FSoundAttenuationSettingsCustomization );
}

TSharedRef<IPropertyTypeCustomization> FForceFeedbackAttenuationSettingsCustomization::MakeInstance() 
{
	return MakeShareable( new FForceFeedbackAttenuationSettingsCustomization );
}

// Helper to return the bool value of a property
static bool GetValue(TSharedPtr<IPropertyHandle> InProp)
{
	if (InProp.IsValid())
	{
		bool Val;
		InProp->GetValue(Val);
		return Val;
	}
	return true;
}

void FBaseAttenuationSettingsCustomization::CustomizeHeader(TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	// We'll set up reset to default ourselves
	const bool bDisplayResetToDefault = false;
	const FText DisplayNameOverride = FText::GetEmpty();
	const FText DisplayToolTipOverride = FText::GetEmpty();

	HeaderRow
		.NameContent()
		[
			StructPropertyHandle->CreatePropertyNameWidget( DisplayNameOverride, DisplayToolTipOverride, bDisplayResetToDefault )
		];
}

TSharedPtr<IPropertyHandle> FBaseAttenuationSettingsCustomization::GetOverrideAttenuationHandle(TSharedRef<IPropertyHandle> StructPropertyHandle)
{
	// Look for override attenuation handle in parent. 
	// This allows us to disable properties in sound cue/audio component/ambient actor details.
	uint32 NumChildren;
	TSharedPtr< IPropertyHandle > ParentHandle = StructPropertyHandle->GetParentHandle();
	FText DisplayName = ParentHandle->GetPropertyDisplayName();
	if (DisplayName.ToString() == FString(TEXT("AttenuationSettings")))
	{
		ParentHandle = ParentHandle->GetParentHandle();
		ParentHandle->GetNumChildren(NumChildren);
		bool bFoundOverrideProperty = false;

		for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
		{
			TSharedRef<IPropertyHandle> ChildHandle = ParentHandle->GetChildHandle(ChildIndex).ToSharedRef();
			DisplayName = ChildHandle->GetPropertyDisplayName();

			if (DisplayName.ToString() == FString(TEXT("Attenuation")))
			{
				uint32 NumAttenChildren;
				ChildHandle->GetNumChildren(NumAttenChildren);

				for (uint32 i = 0; i < NumAttenChildren; ++i)
				{
					TSharedRef<IPropertyHandle> PropHandle = ChildHandle->GetChildHandle(i).ToSharedRef();
					DisplayName = PropHandle->GetPropertyDisplayName();
					if (DisplayName.ToString() == FString(TEXT("Override Attenuation")))
					{
						return PropHandle;
					}
				}
			}
		}
	}

	// A override attenuation handle wasn't found
	return nullptr;
}

void FBaseAttenuationSettingsCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren( NumChildren );

	TMap<FName, TSharedPtr< IPropertyHandle > > PropertyHandles;
	 
	// Get the override attenuation handle, if it exists
	bOverrideAttenuationHandle = GetOverrideAttenuationHandle(StructPropertyHandle);

	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle( ChildIndex ).ToSharedRef();
		const FName PropertyName = ChildHandle->GetProperty()->GetFName();

		PropertyHandles.Add(PropertyName, ChildHandle);
	}

	AttenuationShapeHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FBaseAttenuationSettings, AttenuationShape));
	DistanceAlgorithmHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FBaseAttenuationSettings, DistanceAlgorithm));

	TSharedRef<IPropertyHandle> AttenuationExtentsHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FBaseAttenuationSettings, AttenuationShapeExtents)).ToSharedRef();

	uint32 NumExtentChildren;
	AttenuationExtentsHandle->GetNumChildren( NumExtentChildren );

	TSharedPtr< IPropertyHandle > ExtentXHandle;
	TSharedPtr< IPropertyHandle > ExtentYHandle;
	TSharedPtr< IPropertyHandle > ExtentZHandle;

	for( uint32 ExtentChildIndex = 0; ExtentChildIndex < NumExtentChildren; ++ExtentChildIndex )
	{
		TSharedRef<IPropertyHandle> ChildHandle = AttenuationExtentsHandle->GetChildHandle( ExtentChildIndex ).ToSharedRef();
		const FName PropertyName = ChildHandle->GetProperty()->GetFName();

		if (PropertyName == GET_MEMBER_NAME_CHECKED(FVector, X))
		{
			ExtentXHandle = ChildHandle;
		}
		else if (PropertyName == GET_MEMBER_NAME_CHECKED(FVector, Y))
		{
			ExtentYHandle = ChildHandle;
		}
		else
		{
			check(PropertyName == GET_MEMBER_NAME_CHECKED(FVector, Z));
			ExtentZHandle = ChildHandle;
		}
	}

	// Get layout build of category so properties can be added to categories
	IDetailLayoutBuilder& LayoutBuilder = ChildBuilder.GetParentCategory().GetParentLayout();

	LayoutBuilder.AddPropertyToCategory(DistanceAlgorithmHandle)
		.EditCondition(GetIsAttenuationEnabledAttribute(), nullptr);

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FBaseAttenuationSettings, CustomAttenuationCurve)))
		.Visibility(TAttribute<EVisibility>(this, &FBaseAttenuationSettingsCustomization::IsCustomCurveSelected))
		.EditCondition(GetIsAttenuationEnabledAttribute(), nullptr);

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FBaseAttenuationSettings, dBAttenuationAtMax)))
		.Visibility(TAttribute<EVisibility>(this, &FBaseAttenuationSettingsCustomization::IsNaturalSoundSelected))
		.EditCondition(GetIsAttenuationEnabledAttribute(), nullptr);

	LayoutBuilder.AddPropertyToCategory(AttenuationShapeHandle)
		.EditCondition(GetIsAttenuationEnabledAttribute(), nullptr);
	
	LayoutBuilder.AddPropertyToCategory(AttenuationExtentsHandle)
		.Visibility(TAttribute<EVisibility>(this, &FBaseAttenuationSettingsCustomization::IsBoxSelected))
		.DisplayName(NSLOCTEXT("AttenuationSettings", "BoxExtentsLabel", "Extents"))
		.ToolTip(NSLOCTEXT("AttenuationSettings", "BoxExtents", "The dimensions of the of the box."))
		.EditCondition(GetIsAttenuationEnabledAttribute(), nullptr);

	// Get the attenuation category directly here otherwise our category is going to be incorrect for the following custom rows (e.g. "Vector" vs "Attenuation")
	FText CategoryText = NSLOCTEXT("AttenuationSettings", "AttenuationDistanceLabel", "AttenuationDistance");
	FName AttenuationCategoryFName(*CategoryText.ToString());
	IDetailCategoryBuilder& AttenuationCategory = LayoutBuilder.EditCategory(AttenuationCategoryFName);

	const FText RadiusLabel(NSLOCTEXT("AttenuationSettings", "RadiusLabel", "Inner Radius"));

	AttenuationCategory.AddCustomRow(RadiusLabel)
		.NameContent()
		[
			SNew(STextBlock)
				.Text(RadiusLabel)
				.ToolTipText(NSLOCTEXT("AttenuationSettings", "RadiusToolTip", "The radius that defines when sound attenuation begins (or when a custom attenuation curve begins). Sounds played at a distance less than this will not be attenuated."))
				.Font(StructCustomizationUtils.GetRegularFont())
				.IsEnabled(GetIsAttenuationEnabledAttribute())
		]
		.ValueContent()
		[
			ExtentXHandle->CreatePropertyValueWidget()
		]
		.Visibility(TAttribute<EVisibility>(this, &FBaseAttenuationSettingsCustomization::IsSphereSelected))
		.IsEnabled(GetIsAttenuationEnabledAttribute());

	AttenuationCategory.AddCustomRow(NSLOCTEXT("AttenuationSettings", "CapsuleHalfHeightLabel", "Capsule Half Height"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("AttenuationSettings", "CapsuleHalfHeightLabel", "Capsule Half Height"))
			.ToolTipText(NSLOCTEXT("AttenuationSettings", "CapsuleHalfHeightToolTip", "The attenuation capsule's half height."))
			.Font(StructCustomizationUtils.GetRegularFont())
		]
		.ValueContent()
		[
			ExtentXHandle->CreatePropertyValueWidget()
		]
		.Visibility(TAttribute<EVisibility>(this, &FBaseAttenuationSettingsCustomization::IsCapsuleSelected))
		.IsEnabled(GetIsAttenuationEnabledAttribute());

	AttenuationCategory.AddCustomRow(NSLOCTEXT("AttenuationSettings", "CapsuleRadiusLabel", "Capsule Radius"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("AttenuationSettings", "CapsuleRadiusLabel", "Capsule Radius"))
			.ToolTipText(NSLOCTEXT("AttenuationSettings", "CapsuleRadiusToolTip", "The attenuation capsule's radius."))
			.Font(StructCustomizationUtils.GetRegularFont())
		]
		.ValueContent()
		[
			ExtentYHandle->CreatePropertyValueWidget()
		]
		.Visibility(TAttribute<EVisibility>(this, &FBaseAttenuationSettingsCustomization::IsCapsuleSelected))
		.IsEnabled(GetIsAttenuationEnabledAttribute());

	AttenuationCategory.AddCustomRow(NSLOCTEXT("AttenuationSettings", "ConeRadiusLabel", "Cone Radius"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("AttenuationSettings", "ConeRadiusLabel", "Cone Radius"))
			.ToolTipText(NSLOCTEXT("AttenuationSettings", "ConeRadiusToolTip", "The attenuation cone's radius."))
			.Font(StructCustomizationUtils.GetRegularFont())
		]
		.ValueContent()
		[
			ExtentXHandle->CreatePropertyValueWidget()
		]
		.Visibility(TAttribute<EVisibility>(this, &FBaseAttenuationSettingsCustomization::IsConeSelected))
		.IsEnabled(GetIsAttenuationEnabledAttribute());

	AttenuationCategory.AddCustomRow(NSLOCTEXT("AttenuationSettings", "ConeAngleLabel", "Cone Angle"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("AttenuationSettings", "ConeAngleLabel", "Cone Angle"))
			.ToolTipText(NSLOCTEXT("AttenuationSettings", "ConeAngleToolTip", "The angle of the inner edge of the attenuation cone's falloff. Inside this angle sounds will be at full volume."))
			.Font(StructCustomizationUtils.GetRegularFont())
		]
		.ValueContent()
		[
			ExtentYHandle->CreatePropertyValueWidget()
		]
		.Visibility(TAttribute<EVisibility>(this, &FBaseAttenuationSettingsCustomization::IsConeSelected))
		.IsEnabled(GetIsAttenuationEnabledAttribute());

	AttenuationCategory.AddCustomRow(NSLOCTEXT("AttenuationSettings", "ConeFalloffAngleLabel", "Cone Falloff Angle"))
		.NameContent()
		[
			SNew(STextBlock)
			.Text(NSLOCTEXT("AttenuationSettings", "ConeFalloffAngleLabel", "Cone Falloff Angle"))
			.ToolTipText(NSLOCTEXT("AttenuationSettings", "ConeFalloffAngleToolTip", "The angle of the outer edge of the attenuation cone's falloff. Outside this angle sounds will be inaudible."))
			.Font(StructCustomizationUtils.GetRegularFont())
		]
		.ValueContent()
		[
			ExtentZHandle->CreatePropertyValueWidget()
		]
		.Visibility(TAttribute<EVisibility>(this, &FBaseAttenuationSettingsCustomization::IsConeSelected))
		.IsEnabled(GetIsAttenuationEnabledAttribute());

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FBaseAttenuationSettings, ConeOffset)))
		.Visibility(TAttribute<EVisibility>(this, &FBaseAttenuationSettingsCustomization::IsConeSelected))
		.EditCondition(GetIsAttenuationEnabledAttribute(), nullptr);	

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FBaseAttenuationSettings, FalloffDistance)))
		.EditCondition(GetIsAttenuationEnabledAttribute(), nullptr);
}

TAttribute<bool> FBaseAttenuationSettingsCustomization::IsAttenuationOverriddenAttribute() const
{
	TWeakPtr<IPropertyHandle> bOverrideAttenuationPropertyWeakPtr = bOverrideAttenuationHandle;
	auto Lambda = [bOverrideAttenuationPropertyWeakPtr]()
	{
		TSharedPtr<IPropertyHandle> SharedPtrToHandle = bOverrideAttenuationPropertyWeakPtr.Pin();

		return GetValue(SharedPtrToHandle);
	};

	return TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(Lambda));
}

TAttribute<bool> FBaseAttenuationSettingsCustomization::GetIsAttenuationEnabledAttribute() const
{
	TWeakPtr<IPropertyHandle> bOverrideAttenuationPropertyWeakPtr = bOverrideAttenuationHandle;
	TWeakPtr<IPropertyHandle> bIsAttenuatedPropertyWeakPtr = bIsAttenuatedHandle;

	auto Lambda = [bOverrideAttenuationPropertyWeakPtr, bIsAttenuatedPropertyWeakPtr]()
	{
		TSharedPtr<IPropertyHandle> bOverrideAttenuationPropertyWeakHandle = bOverrideAttenuationPropertyWeakPtr.Pin();
		TSharedPtr<IPropertyHandle> bIsAttenuatedPropertyWeakHandle = bIsAttenuatedPropertyWeakPtr.Pin();

		bool Value = GetValue(bOverrideAttenuationPropertyWeakHandle);
		Value &= GetValue(bIsAttenuatedPropertyWeakHandle);
		return Value;
	};

	return TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(Lambda));
}

void FSoundAttenuationSettingsCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Property handle here is the base struct. We are going to hide it since we're showing it's properties directly.
	PropertyHandle->MarkHiddenByCustomization();
}

void FSoundAttenuationSettingsCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren( NumChildren );

	TMap<FName, TSharedPtr< IPropertyHandle > > PropertyHandles;

	for( uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
	{
		TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle( ChildIndex ).ToSharedRef();
		const FName PropertyName = ChildHandle->GetProperty()->GetFName();
		PropertyHandles.Add(PropertyName, ChildHandle);
	}

	bIsOcclusionEnabledHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, bEnableOcclusion)).ToSharedRef();
	bIsSpatializedHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, bSpatialize)).ToSharedRef();
	bIsAirAbsorptionEnabledHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, bAttenuateWithLPF)).ToSharedRef();
	bIsReverbSendEnabledHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, bEnableReverbSend)).ToSharedRef();
	ReverbSendMethodHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, ReverbSendMethod)).ToSharedRef();
	AbsorptionMethodHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, AbsorptionMethod)).ToSharedRef();

	bIsFocusedHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, bEnableListenerFocus)).ToSharedRef();

	// Set protected member so FBaseAttenuationSettingsCustomization knows how to make attenuation settings editable
	bIsAttenuatedHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, bAttenuate)).ToSharedRef();

	// Get handle to layout builder so we can add properties to categories
	IDetailLayoutBuilder& LayoutBuilder = ChildBuilder.GetParentCategory().GetParentLayout();

	FBaseAttenuationSettingsCustomization::CustomizeChildren(StructPropertyHandle, ChildBuilder, StructCustomizationUtils);

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, bAttenuate)))
		.EditCondition(IsAttenuationOverriddenAttribute(), nullptr);

	LayoutBuilder.AddPropertyToCategory(bIsSpatializedHandle)
		.EditCondition(IsAttenuationOverriddenAttribute(), nullptr);

	// Check to see if a spatialization plugin is enabled
	if (IsAudioPluginEnabled(EAudioPlugin::SPATIALIZATION))
	{
		LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, SpatializationAlgorithm)))
			.EditCondition(GetIsSpatializationEnabledAttribute(), nullptr);

		if (DoesAudioPluginHaveCustomSettings(EAudioPlugin::SPATIALIZATION))
		{
			LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, SpatializationPluginSettings)))
				.EditCondition(GetIsSpatializationEnabledAttribute(), nullptr);
		}
	}

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, OmniRadius)))
		.EditCondition(GetIsSpatializationEnabledAttribute(), nullptr);

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, StereoSpread)))
		.EditCondition(GetIsSpatializationEnabledAttribute(), nullptr);

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, bApplyNormalizationToStereoSounds)))
		.EditCondition(GetIsSpatializationEnabledAttribute(), nullptr);

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, bAttenuateWithLPF)))
		.EditCondition(IsAttenuationOverriddenAttribute(), nullptr);

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, bEnableListenerFocus)))
		.EditCondition(IsAttenuationOverriddenAttribute(), nullptr);

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, LPFRadiusMin)))
		.EditCondition(GetIsAirAbsorptionEnabledAttribute(), nullptr);

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, LPFRadiusMax)))
		.EditCondition(GetIsAirAbsorptionEnabledAttribute(), nullptr);

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, LPFFrequencyAtMin)))
		.EditCondition(GetIsAirAbsorptionEnabledAttribute(), nullptr);

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, LPFFrequencyAtMax)))
		.EditCondition(GetIsAirAbsorptionEnabledAttribute(), nullptr);

	if (GetDefault<UAudioSettings>()->IsAudioMixerEnabled())
	{
		LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, HPFFrequencyAtMin)))
			.EditCondition(GetIsAirAbsorptionEnabledAttribute(), nullptr);

		LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, HPFFrequencyAtMax)))
			.EditCondition(GetIsAirAbsorptionEnabledAttribute(), nullptr);
	}

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, bEnableLogFrequencyScaling)))
		.EditCondition(GetIsAirAbsorptionEnabledAttribute(), nullptr);

	LayoutBuilder.AddPropertyToCategory(AbsorptionMethodHandle)
		.EditCondition(GetIsAirAbsorptionEnabledAttribute(), nullptr);

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, CustomLowpassAirAbsorptionCurve)))
		.Visibility(TAttribute<EVisibility>(this, &FSoundAttenuationSettingsCustomization::IsCustomAirAbsorptionCurveSelected))
		.EditCondition(GetIsAirAbsorptionEnabledAttribute(), nullptr);

	if (GetDefault<UAudioSettings>()->IsAudioMixerEnabled())
	{
		LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, CustomHighpassAirAbsorptionCurve)))
			.Visibility(TAttribute<EVisibility>(this, &FSoundAttenuationSettingsCustomization::IsCustomAirAbsorptionCurveSelected))
			.EditCondition(GetIsAirAbsorptionEnabledAttribute(), nullptr);
	}

	// The reverb wet-level mapping is an audio mixer-only feature
	if (GetDefault<UAudioSettings>()->IsAudioMixerEnabled())
	{
		// Add the reverb send enabled handle
		LayoutBuilder.AddPropertyToCategory(bIsReverbSendEnabledHandle)
			.EditCondition(IsAttenuationOverriddenAttribute(), nullptr);

		// Check if a reverb plugin is enabled, otherwise don't show this
		if (DoesAudioPluginHaveCustomSettings(EAudioPlugin::REVERB))
		{
			LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, ReverbPluginSettings)))
				.EditCondition(GetIsReverbSendEnabledAttribute(), nullptr);
		}

		LayoutBuilder.AddPropertyToCategory(ReverbSendMethodHandle)
			.EditCondition(GetIsReverbSendEnabledAttribute(), nullptr);

		LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, ReverbWetLevelMin)))
			.Visibility(TAttribute<EVisibility>(this, &FSoundAttenuationSettingsCustomization::IsLinearMethodSelected))
			.EditCondition(GetIsReverbSendEnabledAttribute(), nullptr);

		LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, ReverbWetLevelMax)))
			.Visibility(TAttribute<EVisibility>(this, &FSoundAttenuationSettingsCustomization::IsLinearMethodSelected))
			.EditCondition(GetIsReverbSendEnabledAttribute(), nullptr);

		LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, CustomReverbSendCurve)))
			.Visibility(TAttribute<EVisibility>(this, &FSoundAttenuationSettingsCustomization::IsCustomReverbSendCurveSelected))
			.EditCondition(GetIsReverbSendEnabledAttribute(), nullptr);

		LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, ReverbDistanceMin)))
			.Visibility(TAttribute<EVisibility>(this, &FSoundAttenuationSettingsCustomization::IsLinearOrCustomReverbMethodSelected))
			.EditCondition(GetIsReverbSendEnabledAttribute(), nullptr);

		LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, ReverbDistanceMax)))
			.Visibility(TAttribute<EVisibility>(this, &FSoundAttenuationSettingsCustomization::IsLinearOrCustomReverbMethodSelected))
			.EditCondition(GetIsReverbSendEnabledAttribute(), nullptr);

		LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, ManualReverbSendLevel)))
			.Visibility(TAttribute<EVisibility>(this, &FSoundAttenuationSettingsCustomization::IsManualReverbSendSelected))
			.EditCondition(GetIsReverbSendEnabledAttribute(), nullptr);
	}

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, FocusAzimuth)))
		.EditCondition(GetIsFocusEnabledAttribute(), nullptr);

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, NonFocusAzimuth)))
		.EditCondition(GetIsFocusEnabledAttribute(), nullptr);

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, FocusDistanceScale)))
		.EditCondition(GetIsFocusEnabledAttribute(), nullptr);

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, NonFocusDistanceScale)))
		.EditCondition(GetIsFocusEnabledAttribute(), nullptr);

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, FocusPriorityScale)))
		.EditCondition(GetIsFocusEnabledAttribute(), nullptr);

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, NonFocusPriorityScale)))
		.EditCondition(GetIsFocusEnabledAttribute(), nullptr);

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, FocusVolumeAttenuation)))
		.EditCondition(GetIsFocusEnabledAttribute(), nullptr);

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, NonFocusVolumeAttenuation)))
		.EditCondition(GetIsFocusEnabledAttribute(), nullptr);

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, bEnableFocusInterpolation)))
		.EditCondition(GetIsFocusEnabledAttribute(), nullptr);

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, FocusAttackInterpSpeed)))
		.EditCondition(GetIsFocusEnabledAttribute(), nullptr);

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, FocusReleaseInterpSpeed)))
		.EditCondition(GetIsFocusEnabledAttribute(), nullptr);

	LayoutBuilder.AddPropertyToCategory(bIsOcclusionEnabledHandle)
		.EditCondition(IsAttenuationOverriddenAttribute(), nullptr);

	// Hide the occlusion plugin settings slot if there's no occlusion plugin loaded.
	// Don't show the built-in occlusion settings if we're using 
	if (GetDefault<UAudioSettings>()->IsAudioMixerEnabled() && DoesAudioPluginHaveCustomSettings(EAudioPlugin::OCCLUSION))
	{
		LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, OcclusionPluginSettings)))
			.EditCondition(GetIsOcclusionEnabledAttribute(), nullptr);
	}

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, OcclusionTraceChannel)))
		.EditCondition(GetIsOcclusionEnabledAttribute(), nullptr);

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, OcclusionLowPassFilterFrequency)))
		.EditCondition(GetIsOcclusionEnabledAttribute(), nullptr);

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, OcclusionVolumeAttenuation)))
		.EditCondition(GetIsOcclusionEnabledAttribute(), nullptr);

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, OcclusionInterpolationTime)))
		.EditCondition(GetIsOcclusionEnabledAttribute(), nullptr);

	LayoutBuilder.AddPropertyToCategory(PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundAttenuationSettings, bUseComplexCollisionForOcclusion)))
		.EditCondition(GetIsOcclusionEnabledAttribute(), nullptr);

	if (PropertyHandles.Num() != 53)
	{
		FString PropertyList;
		for (auto It(PropertyHandles.CreateConstIterator()); It; ++It)
		{
			PropertyList += It.Key().ToString() + TEXT(", ");
		}
		ensureMsgf(false, TEXT("Unexpected property handle(s) customizing FSoundAttenuationSettings: %s"), *PropertyList);
	}
}

void FForceFeedbackAttenuationSettingsCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> PropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	// Property handle here is the base struct. We are going to hide it since we're showing it's properties directly.
	PropertyHandle->MarkHiddenByCustomization();
}

void FForceFeedbackAttenuationSettingsCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	FBaseAttenuationSettingsCustomization::CustomizeChildren(StructPropertyHandle, ChildBuilder, StructCustomizationUtils);

	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren( NumChildren );

	TMap<FName, TSharedPtr< IPropertyHandle > > PropertyHandles;

	for( uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
	{
		TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle( ChildIndex ).ToSharedRef();
		const FName PropertyName = ChildHandle->GetProperty()->GetFName();

		PropertyHandles.Add(PropertyName, ChildHandle);
	}

	TSharedPtr<FPropertyRestriction> EnumRestriction = MakeShareable(new FPropertyRestriction(NSLOCTEXT("AttenuationSettings", "NoNaturalSound", "Natural Sound is only available for Sound Attenuation")));
	const UEnum* const AttenuationDistanceModelEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("EAttenuationDistanceModel"));		
	EnumRestriction->AddHiddenValue(AttenuationDistanceModelEnum->GetNameStringByValue((uint8)EAttenuationDistanceModel::NaturalSound));
	DistanceAlgorithmHandle->AddRestriction(EnumRestriction.ToSharedRef());

	if (PropertyHandles.Num() != 7)
	{
		FString PropertyList;
		for (auto It(PropertyHandles.CreateConstIterator()); It; ++It)
		{
			PropertyList += It.Key().ToString() + TEXT(", ");
		}
		ensureMsgf(false, TEXT("Unexpected property handle(s) customizing FForceFeedbackAttenuationSettings: %s"), *PropertyList);
	}

}

TAttribute<bool> FSoundAttenuationSettingsCustomization::GetIsFocusEnabledAttribute() const
{
	TWeakPtr<IPropertyHandle> bOverrideAttenuationPropertyWeakPtr = bOverrideAttenuationHandle;
	TWeakPtr<IPropertyHandle> bIsFocusedPropertyWeakPtr = bIsFocusedHandle;

	auto Lambda = [bOverrideAttenuationPropertyWeakPtr, bIsFocusedPropertyWeakPtr]()
	{
		TSharedPtr<IPropertyHandle> bOverrideAttenuationProperty = bOverrideAttenuationPropertyWeakPtr.Pin();
		TSharedPtr<IPropertyHandle> bIsFocusedProperty = bIsFocusedPropertyWeakPtr.Pin();

		bool Value = GetValue(bOverrideAttenuationProperty);
		Value &= GetValue(bIsFocusedProperty);
		return Value;
	};

	return TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(Lambda));
}

TAttribute<bool> FSoundAttenuationSettingsCustomization::GetIsOcclusionEnabledAttribute() const
{
	TWeakPtr<IPropertyHandle> bOverrideAttenuationPropertyWeakPtr = bOverrideAttenuationHandle;
	TWeakPtr<IPropertyHandle> bIsOcclusionPropertyWeakPtr = bIsOcclusionEnabledHandle;

	auto Lambda = [bOverrideAttenuationPropertyWeakPtr, bIsOcclusionPropertyWeakPtr]()
	{
		TSharedPtr<IPropertyHandle> bOverrideAttenuationProperty = bOverrideAttenuationPropertyWeakPtr.Pin();
		TSharedPtr<IPropertyHandle> bOcclusionProperty = bIsOcclusionPropertyWeakPtr.Pin();

		bool Value = GetValue(bOverrideAttenuationProperty);
		Value &= GetValue(bOcclusionProperty);
		return Value;
	};

	return TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(Lambda));
}

TAttribute<bool> FSoundAttenuationSettingsCustomization::GetIsSpatializationEnabledAttribute() const
{
	TWeakPtr<IPropertyHandle> bOverrideAttenuationPropertyWeakPtr = bOverrideAttenuationHandle;
	TWeakPtr<IPropertyHandle> bIsSpatializedHandleWeakPtr = bIsSpatializedHandle;

	auto Lambda = [bOverrideAttenuationPropertyWeakPtr, bIsSpatializedHandleWeakPtr]()
	{
		TSharedPtr<IPropertyHandle> bOverrideAttenuationProperty = bOverrideAttenuationPropertyWeakPtr.Pin();
		TSharedPtr<IPropertyHandle> bIsSpatializedProperty = bIsSpatializedHandleWeakPtr.Pin();

		bool Value = GetValue(bOverrideAttenuationProperty);
		Value &= GetValue(bIsSpatializedProperty);
		return Value;
	};

	return TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(Lambda));
}

TAttribute<bool> FSoundAttenuationSettingsCustomization::GetIsAirAbsorptionEnabledAttribute() const
{
	TWeakPtr<IPropertyHandle> bOverrideAttenuationPropertyWeakPtr = bOverrideAttenuationHandle;
	TWeakPtr<IPropertyHandle> bIsAirAbsorptionHandleWeakPtr = bIsAirAbsorptionEnabledHandle;

	auto Lambda = [bOverrideAttenuationPropertyWeakPtr, bIsAirAbsorptionHandleWeakPtr]()
	{
		TSharedPtr<IPropertyHandle> bOverrideAttenuationProperty = bOverrideAttenuationPropertyWeakPtr.Pin();
		TSharedPtr<IPropertyHandle> bIsAirAbsorptionProperty = bIsAirAbsorptionHandleWeakPtr.Pin();

		bool Value = GetValue(bOverrideAttenuationProperty);
		Value &= GetValue(bIsAirAbsorptionProperty);
		return Value;
	};

	return TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(Lambda));
}

TAttribute<bool> FSoundAttenuationSettingsCustomization::GetIsReverbSendEnabledAttribute() const
{
	TWeakPtr<IPropertyHandle> bOverrideAttenuationPropertyWeakPtr = bOverrideAttenuationHandle;
	TWeakPtr<IPropertyHandle> bIsReverbSendWeakPtr = bIsReverbSendEnabledHandle;

	auto Lambda = [bOverrideAttenuationPropertyWeakPtr, bIsReverbSendWeakPtr]()
	{
		TSharedPtr<IPropertyHandle> bOverrideAttenuationProperty = bOverrideAttenuationPropertyWeakPtr.Pin();
		TSharedPtr<IPropertyHandle> bIsReverbSendProperty = bIsReverbSendWeakPtr.Pin();

		bool Value = GetValue(bOverrideAttenuationProperty);
		Value &= GetValue(bIsReverbSendProperty);
		return Value;
	};

	return TAttribute<bool>::Create(TAttribute<bool>::FGetter::CreateLambda(Lambda));
}

EVisibility FSoundAttenuationSettingsCustomization::IsLinearMethodSelected() const
{
	uint8 SendMethodValue;
	ReverbSendMethodHandle->GetValue(SendMethodValue);

	const EReverbSendMethod SendMethodType = (EReverbSendMethod)SendMethodValue;

	return (SendMethodType == EReverbSendMethod::Linear ? EVisibility::Visible : EVisibility::Hidden);
}

EVisibility FSoundAttenuationSettingsCustomization::IsCustomReverbSendCurveSelected() const
{
	uint8 SendMethodValue;
	ReverbSendMethodHandle->GetValue(SendMethodValue);

	const EReverbSendMethod SendMethodType = (EReverbSendMethod)SendMethodValue;

	return (SendMethodType == EReverbSendMethod::CustomCurve ? EVisibility::Visible : EVisibility::Hidden);
}

EVisibility FSoundAttenuationSettingsCustomization::IsCustomAirAbsorptionCurveSelected() const
{
	uint8 MethodValue;
	AbsorptionMethodHandle->GetValue(MethodValue);

	const EAirAbsorptionMethod MethodType = (EAirAbsorptionMethod)MethodValue;

	return (MethodType == EAirAbsorptionMethod::CustomCurve ? EVisibility::Visible : EVisibility::Hidden);
}

EVisibility FSoundAttenuationSettingsCustomization::IsLinearOrCustomReverbMethodSelected() const
{
	uint8 SendMethodValue;
	ReverbSendMethodHandle->GetValue(SendMethodValue);

	const EReverbSendMethod SendMethodType = (EReverbSendMethod)SendMethodValue;

	return (SendMethodType != EReverbSendMethod::Manual ? EVisibility::Visible : EVisibility::Hidden);
}

EVisibility FSoundAttenuationSettingsCustomization::IsManualReverbSendSelected() const
{
	uint8 SendMethodValue;
	ReverbSendMethodHandle->GetValue(SendMethodValue);

	const EReverbSendMethod SendMethodType = (EReverbSendMethod)SendMethodValue;

	return (SendMethodType == EReverbSendMethod::Manual ? EVisibility::Visible : EVisibility::Hidden);
}


EVisibility FBaseAttenuationSettingsCustomization::IsConeSelected() const
{
	uint8 AttenuationShapeValue;
	AttenuationShapeHandle->GetValue(AttenuationShapeValue);

	const EAttenuationShape::Type AttenuationShape = (EAttenuationShape::Type)AttenuationShapeValue;

	return (AttenuationShape == EAttenuationShape::Cone ? EVisibility::Visible : EVisibility::Hidden);
}

EVisibility FBaseAttenuationSettingsCustomization::IsSphereSelected() const
{
	uint8 AttenuationShapeValue;
	AttenuationShapeHandle->GetValue(AttenuationShapeValue);

	const EAttenuationShape::Type AttenuationShape = (EAttenuationShape::Type)AttenuationShapeValue;

	return (AttenuationShape == EAttenuationShape::Sphere ? EVisibility::Visible : EVisibility::Hidden);
}

EVisibility FBaseAttenuationSettingsCustomization::IsBoxSelected() const
{
	uint8 AttenuationShapeValue;
	AttenuationShapeHandle->GetValue(AttenuationShapeValue);

	const EAttenuationShape::Type AttenuationShape = (EAttenuationShape::Type)AttenuationShapeValue;

	return (AttenuationShape == EAttenuationShape::Box ? EVisibility::Visible : EVisibility::Hidden);
}

EVisibility FBaseAttenuationSettingsCustomization::IsCapsuleSelected() const
{
	uint8 AttenuationShapeValue;
	AttenuationShapeHandle->GetValue(AttenuationShapeValue);

	const EAttenuationShape::Type AttenuationShape = (EAttenuationShape::Type)AttenuationShapeValue;

	return (AttenuationShape == EAttenuationShape::Capsule ? EVisibility::Visible : EVisibility::Hidden);
}

EVisibility FBaseAttenuationSettingsCustomization::IsNaturalSoundSelected() const
{
	uint8 DistanceAlgorithmValue;
	DistanceAlgorithmHandle->GetValue(DistanceAlgorithmValue);

	const EAttenuationDistanceModel DistanceAlgorithm = (EAttenuationDistanceModel)DistanceAlgorithmValue;

	return (DistanceAlgorithm == EAttenuationDistanceModel::NaturalSound ? EVisibility::Visible : EVisibility::Hidden);
}

EVisibility FBaseAttenuationSettingsCustomization::IsCustomCurveSelected() const
{
	uint8 DistanceAlgorithmValue;
	DistanceAlgorithmHandle->GetValue(DistanceAlgorithmValue);

	const EAttenuationDistanceModel DistanceAlgorithm = (EAttenuationDistanceModel)DistanceAlgorithmValue;

	return (DistanceAlgorithm == EAttenuationDistanceModel::Custom ? EVisibility::Visible : EVisibility::Hidden);
}
