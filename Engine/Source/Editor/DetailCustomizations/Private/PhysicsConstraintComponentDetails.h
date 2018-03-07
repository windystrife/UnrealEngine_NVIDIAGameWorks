// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "IDetailCustomization.h"

class IDetailLayoutBuilder;
class IPropertyHandle;
enum class ECheckBoxState : uint8;

/** 
 * Detail customizer for PhysicsConstraintComponent and PhysicsConstraintTemplate
 */
class FPhysicsConstraintComponentDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;

private:
	struct EPropertyType
	{
		enum Type
		{
			LinearXPositionDrive,
			LinearYPositionDrive,
			LinearZPositionDrive,
			LinearPositionDrive,
			LinearXVelocityDrive,
			LinearYVelocityDrive,
			LinearZVelocityDrive,
			LinearVelocityDrive,
			LinearDrive,
		
			AngularSwingLimit,
			AngularSwing1Limit,
			AngularSwing2Limit,
			AngularTwistLimit,
			AngularAnyLimit,
		};
	};

	bool IsPropertyEnabled(EPropertyType::Type Type) const;

	ECheckBoxState IsLimitRadioChecked(TSharedPtr<IPropertyHandle> Property, uint8 Value) const ;
	void OnLimitRadioChanged(ECheckBoxState CheckType, TSharedPtr<IPropertyHandle> Property, uint8 Value);

	void AddConstraintBehaviorProperties(IDetailLayoutBuilder& DetailBuilder, TSharedPtr<IPropertyHandle> ConstraintInstance, TSharedPtr<IPropertyHandle> ProfileInstance);
	void AddLinearLimits(IDetailLayoutBuilder& DetailBuilder, TSharedPtr<IPropertyHandle> ConstraintInstance, TSharedPtr<IPropertyHandle> ProfileInstance);
	void AddAngularLimits(IDetailLayoutBuilder& DetailBuilder, TSharedPtr<IPropertyHandle> ConstraintInstance, TSharedPtr<IPropertyHandle> ProfileInstance);
	void AddLinearDrive(IDetailLayoutBuilder& DetailBuilder, TSharedPtr<IPropertyHandle> ConstraintInstance, TSharedPtr<IPropertyHandle> ProfileInstance);
	void AddAngularDrive(IDetailLayoutBuilder& DetailBuilder, TSharedPtr<IPropertyHandle> ConstraintInstance, TSharedPtr<IPropertyHandle> ProfileInstance);
private:

	TSharedPtr<IPropertyHandle> LinearXPositionDriveProperty;
	TSharedPtr<IPropertyHandle> LinearYPositionDriveProperty;
	TSharedPtr<IPropertyHandle> LinearZPositionDriveProperty;

	TSharedPtr<IPropertyHandle> LinearXVelocityDriveProperty;
	TSharedPtr<IPropertyHandle> LinearYVelocityDriveProperty;
	TSharedPtr<IPropertyHandle> LinearZVelocityDriveProperty;

	TSharedPtr<IPropertyHandle> AngularSwing1MotionProperty;
	TSharedPtr<IPropertyHandle> AngularSwing2MotionProperty;
	TSharedPtr<IPropertyHandle> AngularTwistMotionProperty;

	TWeakObjectPtr<UObject> ConstraintComponent;
	
	bool bInPhat;
};
