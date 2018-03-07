// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IDetailCustomization.h"
#include "Curves/CurveOwnerInterface.h"
#include "SCurveEditor.h"

class IDetailLayoutBuilder;
class UWheeledVehicleMovementComponent4W;

class FWheeledVehicleMovementComponent4WDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;
	
private:

	struct FSteeringCurveEditor : public FCurveOwnerInterface
	{
		FSteeringCurveEditor(UWheeledVehicleMovementComponent4W * InVehicle = NULL);
		/** FCurveOwnerInterface interface */
		virtual TArray<FRichCurveEditInfoConst> GetCurves() const override;
		virtual TArray<FRichCurveEditInfo> GetCurves() override;
		virtual void ModifyOwner() override;
		virtual TArray<const UObject*> GetOwners() const override;
		virtual void MakeTransactional() override;
		virtual void OnCurveChanged( const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos ) override { }
		virtual bool IsValidCurve(FRichCurveEditInfo CurveInfo) override;

	private:
		UWheeledVehicleMovementComponent4W * VehicleComponent;
		UObject* Owner;

	} SteeringCurveEditor;

	struct FTorqueCurveEditor : public FCurveOwnerInterface
	{
		FTorqueCurveEditor(UWheeledVehicleMovementComponent4W * InVehicle = NULL);
		/** FCurveOwnerInterface interface */
		virtual TArray<FRichCurveEditInfoConst> GetCurves() const override;
		virtual TArray<FRichCurveEditInfo> GetCurves() override;
		virtual void ModifyOwner() override;
		virtual TArray<const UObject*> GetOwners() const override;
		virtual void MakeTransactional() override;
		virtual void OnCurveChanged( const TArray<FRichCurveEditInfo>& ChangedCurveEditInfos ) override { }
		virtual bool IsValidCurve( FRichCurveEditInfo CurveInfo ) override;

	private:
		UWheeledVehicleMovementComponent4W * VehicleComponent;
		UObject* Owner;

	} TorqueCurveEditor;

	TArray<TWeakObjectPtr<UObject> > SelectedObjects;	//the objects we're showing details for
	
	/** Steering curve widget */
	TSharedPtr<class SCurveEditor> SteeringCurveWidget;

	/** Torque curve widget */
	TSharedPtr<class SCurveEditor> TorqueCurveWidget;
};

