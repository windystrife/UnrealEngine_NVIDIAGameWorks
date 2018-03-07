// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Layout/Visibility.h"
#include "Input/Reply.h"
#include "IDetailCustomization.h"
#include "PropertyHandle.h"
#include "BodyInstanceCustomization.h"

class FComponentMaterialCategory;
class IDetailLayoutBuilder;

class FPrimitiveComponentDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

private:
	void AddMaterialCategory( IDetailLayoutBuilder& DetailBuilder);
	void AddLightingCategory(IDetailLayoutBuilder& DetailBuilder);
	void AddPhysicsCategory(IDetailLayoutBuilder& DetailBuilder);
	void AddCollisionCategory(IDetailLayoutBuilder& DetailBuilder);

	ECheckBoxState IsMobilityActive(TSharedRef<IPropertyHandle> MobilityHandle, EComponentMobility::Type InMobility) const;

	void OnMobilityChanged(ECheckBoxState InCheckedState, TSharedRef<IPropertyHandle> MobilityHandle, EComponentMobility::Type InMobility);

	void AddAdvancedSubCategory(IDetailLayoutBuilder& DetailBuilder, FName MainCategory, FName SubCategory);

	FReply OnMobilityResetClicked(TSharedRef<IPropertyHandle> MobilityHandle);

	EVisibility GetMobilityResetVisibility(TSharedRef<IPropertyHandle> MobilityHandle) const;
	
	/** Returns whether to enable editing the 'Simulate Physics' checkbox based on the selected objects physics geometry */
	bool IsSimulatePhysicsEditable() const;
	/** Returns whether to enable editing the 'Use Async Scene' checkbox based on the selected objects' mobility and if the project uses an AsyncScene */
	bool IsUseAsyncEditable() const;

	TOptional<float> OnGetBodyMass() const;
	bool IsBodyMassReadOnly() const;
	bool IsBodyMassEnabled() const { return !IsBodyMassReadOnly(); }

private:
	/** Objects being customized so we can update the 'Simulate Physics' state if physics geometry is added/removed */
	TArray< TWeakObjectPtr<UObject> > ObjectsCustomized;

	TSharedPtr<class FComponentMaterialCategory> MaterialCategory;
	TSharedPtr<class FBodyInstanceCustomizationHelper> BodyInstanceCustomizationHelper;

};

