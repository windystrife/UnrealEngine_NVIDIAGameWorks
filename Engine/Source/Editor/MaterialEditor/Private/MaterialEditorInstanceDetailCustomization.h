// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Types/SlateEnums.h"
#include "Layout/Visibility.h"
#include "IDetailCustomization.h"

struct FAssetData;
class IDetailGroup;
class IDetailLayoutBuilder;
class IPropertyHandle;
class UMaterialEditorInstanceConstant;

DECLARE_DELEGATE_OneParam(FGetShowHiddenParameters, bool&);


/*-----------------------------------------------------------------------------
   FMaterialInstanceParameterDetails
-----------------------------------------------------------------------------*/

class FMaterialInstanceParameterDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<class IDetailCustomization> MakeInstance(UMaterialEditorInstanceConstant* MaterialInstance, FGetShowHiddenParameters InShowHiddenDelegate);
	
	/** Constructor */
	FMaterialInstanceParameterDetails(UMaterialEditorInstanceConstant* MaterialInstance, FGetShowHiddenParameters InShowHiddenDelegate);

	/** IDetailCustomization interface */
	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailLayout) override;

	static TOptional<float> OnGetValue(TSharedRef<IPropertyHandle> PropertyHandle);
	static void OnValueCommitted(float NewValue, ETextCommit::Type CommitType, TSharedRef<IPropertyHandle> PropertyHandle);

private:
	/** Builds the custom parameter groups category */
	void CreateGroupsWidget(TSharedRef<IPropertyHandle> ParameterGroupsProperty, class IDetailCategoryBuilder& GroupsCategory);

	/** Builds the widget for an individual parameter group */
	void CreateSingleGroupWidget(struct FEditorParameterGroup& ParameterGroup, TSharedPtr<IPropertyHandle> ParameterGroupProperty, class IDetailGroup& DetailGroup );

	/** These methods generate the custom widgets for the various parameter types */
	void CreateParameterValueWidget(class UDEditorParameterValue* Parameter, TSharedPtr<IPropertyHandle> ParameterProperty, IDetailGroup& DetailGroup );
	void CreateMaskParameterValueWidget(class UDEditorParameterValue* Parameter, TSharedPtr<IPropertyHandle> ParameterProperty, IDetailGroup& DetailGroup );

	/** Returns true if the parameter is in the visible expressions list */
	bool IsVisibleExpression(class UDEditorParameterValue* Parameter);

	/** Returns true if the parameter should be displayed */
	EVisibility ShouldShowExpression(class UDEditorParameterValue* Parameter) const;

	/** Returns true if the parameter is being overridden */
	bool IsOverriddenExpression(class UDEditorParameterValue* Parameter);

	/** Gets the expression description of this parameter from the the base material */
	FText GetParameterExpressionDescription(class UDEditorParameterValue* Parameter) const;

	/**
	 * Called when a parameter is overridden;
	 */
	void OnOverrideParameter(bool NewValue, class UDEditorParameterValue* Parameter);

	/** Called when an asset is set as a parent */
	bool OnShouldSetAsset(const FAssetData& InAssetData) const;

	/** Reset to default implementation.  Resets Parameter to default */
	void ResetToDefault(TSharedPtr<IPropertyHandle> PropertyHandle, class UDEditorParameterValue* Parameter);

	/** If reset to default button should show */
	bool ShouldShowResetToDefault(TSharedPtr<IPropertyHandle> PropertyHandle, class UDEditorParameterValue* Parameter);

	/** Returns true if the refraction options should be displayed */
	EVisibility ShouldShowMaterialRefractionSettings() const;

	/** Returns true if the refraction options should be displayed */
	EVisibility ShouldShowSubsurfaceProfile() const;

	
	//Functions supporting BasePropertyOverrides

	/** Creates all the base property override widgets. */
	void CreateBasePropertyOverrideWidgets(IDetailLayoutBuilder& DetailLayout);

	bool OverrideOpacityClipMaskValueEnabled() const;
	bool OverrideBlendModeEnabled() const;
	bool OverrideShadingModelEnabled() const;
	bool OverrideTwoSidedEnabled() const;
	bool OverrideDitheredLODTransitionEnabled() const;
	void OnOverrideOpacityClipMaskValueChanged(bool NewValue);
	void OnOverrideBlendModeChanged(bool NewValue);
	void OnOverrideShadingModelChanged(bool NewValue);
	void OnOverrideTwoSidedChanged(bool NewValue);
	void OnOverrideDitheredLODTransitionChanged(bool NewValue);

	// NVCHANGE_BEGIN: Add VXGI
	bool OverrideIsVxgiConeTracingEnabled() const { return MaterialEditorInstance->BasePropertyOverrides.bOverride_VxgiConeTracingEnabled; }
	bool OverrideIsUsedWithVxgiVoxelizationEnabled() const { return MaterialEditorInstance->BasePropertyOverrides.bOverride_UsedWithVxgiVoxelization; }
	bool OverrideIsVxgiOmniDirectionalEnabled() const { return MaterialEditorInstance->BasePropertyOverrides.bOverride_VxgiOmniDirectional; }
	bool OverrideIsVxgiProportionalEmittanceEnabled() const { return MaterialEditorInstance->BasePropertyOverrides.bOverride_VxgiProportionalEmittance; }
	bool OverrideGetVxgiAllowTesselationDuringVoxelizationEnabled() const { return MaterialEditorInstance->BasePropertyOverrides.bOverride_VxgiAllowTesselationDuringVoxelization; }
	bool OverrideGetVxgiVoxelizationThicknessEnabled() const { return MaterialEditorInstance->BasePropertyOverrides.bOverride_VxgiVoxelizationThickness; }
	bool OverrideGetVxgiOpacityNoiseScaleBiasEnabled() const { return MaterialEditorInstance->BasePropertyOverrides.bOverride_VxgiOpacityNoiseScaleBias; }
	bool OverrideGetVxgiCoverageSupersamplingEnabled() const { return MaterialEditorInstance->BasePropertyOverrides.bOverride_VxgiCoverageSupersampling; }
	bool OverrideGetVxgiMaterialSamplingRateEnabled() const { return MaterialEditorInstance->BasePropertyOverrides.bOverride_VxgiMaterialSamplingRate; }

	void OnOverrideIsVxgiConeTracingEnabled(bool NewValue);
	void OnOverrideIsUsedWithVxgiVoxelizationEnabled(bool NewValue);
	void OnOverrideIsVxgiOmniDirectionalEnabled(bool NewValue);
	void OnOverrideIsVxgiProportionalEmittanceEnabled(bool NewValue);
	void OnOverrideGetVxgiAllowTesselationDuringVoxelizationEnabled(bool NewValue);
	void OnOverrideGetVxgiVoxelizationThicknessEnabled(bool NewValue);
	void OnOverrideGetVxgiOpacityNoiseScaleBiasEnabled(bool NewValue);
	void OnOverrideGetVxgiCoverageSupersamplingEnabled(bool NewValue);
	void OnOverrideGetVxgiMaterialSamplingRateEnabled(bool NewValue);
	// NVCHANGE_END: Add VXGI

private:
	/** Object that stores all of the possible parameters we can edit */
	UMaterialEditorInstanceConstant* MaterialEditorInstance;

	/** Delegate to call to determine if hidden parameters should be shown */
	FGetShowHiddenParameters ShowHiddenDelegate;
};

