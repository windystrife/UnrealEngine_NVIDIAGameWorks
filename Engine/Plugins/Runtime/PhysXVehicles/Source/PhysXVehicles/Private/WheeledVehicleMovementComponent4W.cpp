// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "WheeledVehicleMovementComponent4W.h"
#include "Components/PrimitiveComponent.h"

#include "PhysXPublic.h"

UWheeledVehicleMovementComponent4W::UWheeledVehicleMovementComponent4W(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	// grab default values from physx
	PxVehicleDifferential4WData DefDifferentialSetup;
	TEnumAsByte<EVehicleDifferential4W::Type> DiffType((uint8)DefDifferentialSetup.mType);
	DifferentialSetup.DifferentialType = DiffType;
	DifferentialSetup.FrontRearSplit = DefDifferentialSetup.mFrontRearSplit;
	DifferentialSetup.FrontLeftRightSplit = DefDifferentialSetup.mFrontLeftRightSplit;
	DifferentialSetup.RearLeftRightSplit = DefDifferentialSetup.mRearLeftRightSplit;
	DifferentialSetup.CentreBias = DefDifferentialSetup.mCentreBias;
	DifferentialSetup.FrontBias = DefDifferentialSetup.mFrontBias;
	DifferentialSetup.RearBias = DefDifferentialSetup.mRearBias;

	PxVehicleEngineData DefEngineData;
	EngineSetup.MOI = DefEngineData.mMOI;
	EngineSetup.MaxRPM = OmegaToRPM(DefEngineData.mMaxOmega);
	EngineSetup.DampingRateFullThrottle = DefEngineData.mDampingRateFullThrottle;
	EngineSetup.DampingRateZeroThrottleClutchEngaged = DefEngineData.mDampingRateZeroThrottleClutchEngaged;
	EngineSetup.DampingRateZeroThrottleClutchDisengaged = DefEngineData.mDampingRateZeroThrottleClutchDisengaged;

	// Convert from PhysX curve to ours
	FRichCurve* TorqueCurveData = EngineSetup.TorqueCurve.GetRichCurve();
	for(PxU32 KeyIdx=0; KeyIdx<DefEngineData.mTorqueCurve.getNbDataPairs(); KeyIdx++)
	{
		float Input = DefEngineData.mTorqueCurve.getX(KeyIdx) * EngineSetup.MaxRPM;
		float Output = DefEngineData.mTorqueCurve.getY(KeyIdx) * DefEngineData.mPeakTorque;
		TorqueCurveData->AddKey(Input, Output);
	}

	PxVehicleClutchData DefClutchData;
	TransmissionSetup.ClutchStrength = DefClutchData.mStrength;

	PxVehicleAckermannGeometryData DefAckermannSetup;
	AckermannAccuracy = DefAckermannSetup.mAccuracy;

	PxVehicleGearsData DefGearSetup;
	TransmissionSetup.GearSwitchTime = DefGearSetup.mSwitchTime;
	TransmissionSetup.ReverseGearRatio = DefGearSetup.mRatios[PxVehicleGearsData::eREVERSE];
	TransmissionSetup.FinalRatio = DefGearSetup.mFinalRatio;

	PxVehicleAutoBoxData DefAutoBoxSetup;
	TransmissionSetup.NeutralGearUpRatio = DefAutoBoxSetup.mUpRatios[PxVehicleGearsData::eNEUTRAL];
	TransmissionSetup.GearAutoBoxLatency = DefAutoBoxSetup.getLatency();
	TransmissionSetup.bUseGearAutoBox = true;

	for (uint32 i = PxVehicleGearsData::eFIRST; i < DefGearSetup.mNbRatios; i++)
	{
		FVehicleGearData GearData;
		GearData.DownRatio = DefAutoBoxSetup.mDownRatios[i];
		GearData.UpRatio = DefAutoBoxSetup.mUpRatios[i];
		GearData.Ratio = DefGearSetup.mRatios[i];
		TransmissionSetup.ForwardGears.Add(GearData);
	}
	
	// Init steering speed curve
	FRichCurve* SteeringCurveData = SteeringCurve.GetRichCurve();
	SteeringCurveData->AddKey(0.f, 1.f);
	SteeringCurveData->AddKey(20.f, 0.9f);
	SteeringCurveData->AddKey(60.f, 0.8f);
	SteeringCurveData->AddKey(120.f, 0.7f);

	// Initialize WheelSetups array with 4 wheels
	WheelSetups.SetNum(4);
}

#if WITH_EDITOR
void UWheeledVehicleMovementComponent4W::PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;

	if (PropertyName == TEXT("DownRatio"))
	{
		for (int32 GearIdx = 0; GearIdx < TransmissionSetup.ForwardGears.Num(); ++GearIdx)
		{
			FVehicleGearData & GearData = TransmissionSetup.ForwardGears[GearIdx];
			GearData.DownRatio = FMath::Min(GearData.DownRatio, GearData.UpRatio);
		}
	}
	else if (PropertyName == TEXT("UpRatio"))
	{
		for (int32 GearIdx = 0; GearIdx < TransmissionSetup.ForwardGears.Num(); ++GearIdx)
		{
			FVehicleGearData & GearData = TransmissionSetup.ForwardGears[GearIdx];
			GearData.UpRatio = FMath::Max(GearData.DownRatio, GearData.UpRatio);
		}
	}
	else if (PropertyName == TEXT("SteeringCurve"))
	{
		//make sure values are capped between 0 and 1
		TArray<FRichCurveKey> SteerKeys = SteeringCurve.GetRichCurve()->GetCopyOfKeys();
		for (int32 KeyIdx = 0; KeyIdx < SteerKeys.Num(); ++KeyIdx)
		{
			float NewValue = FMath::Clamp(SteerKeys[KeyIdx].Value, 0.f, 1.f);
			SteeringCurve.GetRichCurve()->UpdateOrAddKey(SteerKeys[KeyIdx].Time, NewValue);
		}
	}
}
#endif


static void GetVehicleDifferential4WSetup(const FVehicleDifferential4WData& Setup, PxVehicleDifferential4WData& PxSetup)
{
//	PxSetup.mType = Setup.DifferentialType; //TBD make this work again and get rid of the case-switch
	switch(Setup.DifferentialType)
	{
		case EVehicleDifferential4W::LimitedSlip_4W:
			PxSetup.mType = PxVehicleDifferential4WData::eDIFF_TYPE_LS_4WD;
			break;
		case EVehicleDifferential4W::LimitedSlip_FrontDrive:
			PxSetup.mType = PxVehicleDifferential4WData::eDIFF_TYPE_LS_FRONTWD;
			break;
		case EVehicleDifferential4W::LimitedSlip_RearDrive:
				PxSetup.mType = PxVehicleDifferential4WData::eDIFF_TYPE_LS_REARWD;
			break;
		case EVehicleDifferential4W::Open_4W:
				PxSetup.mType = PxVehicleDifferential4WData::eDIFF_TYPE_OPEN_4WD;
			break;
		case EVehicleDifferential4W::Open_FrontDrive:
				PxSetup.mType = PxVehicleDifferential4WData::eDIFF_TYPE_OPEN_FRONTWD;
			break;
		case EVehicleDifferential4W::Open_RearDrive:
				PxSetup.mType = PxVehicleDifferential4WData::eDIFF_TYPE_OPEN_REARWD;
			break;
		default:		
			break;
	}
	PxSetup.mFrontRearSplit = Setup.FrontRearSplit;
	PxSetup.mFrontLeftRightSplit = Setup.FrontLeftRightSplit;
	PxSetup.mRearLeftRightSplit = Setup.RearLeftRightSplit;
	PxSetup.mCentreBias = Setup.CentreBias;
	PxSetup.mFrontBias = Setup.FrontBias;
	PxSetup.mRearBias = Setup.RearBias;
}

float FVehicleEngineData::FindPeakTorque() const
{
	// Find max torque
	float PeakTorque = 0.f;
	TArray<FRichCurveKey> TorqueKeys = TorqueCurve.GetRichCurveConst()->GetCopyOfKeys();
	for (int32 KeyIdx = 0; KeyIdx < TorqueKeys.Num(); KeyIdx++)
	{
		FRichCurveKey& Key = TorqueKeys[KeyIdx];
		PeakTorque = FMath::Max(PeakTorque, Key.Value);
	}
	return PeakTorque;
}

static void GetVehicleEngineSetup(const FVehicleEngineData& Setup, PxVehicleEngineData& PxSetup)
{
	PxSetup.mMOI = M2ToCm2(Setup.MOI);
	PxSetup.mMaxOmega = RPMToOmega(Setup.MaxRPM);
	PxSetup.mDampingRateFullThrottle = M2ToCm2(Setup.DampingRateFullThrottle);
	PxSetup.mDampingRateZeroThrottleClutchEngaged = M2ToCm2(Setup.DampingRateZeroThrottleClutchEngaged);
	PxSetup.mDampingRateZeroThrottleClutchDisengaged = M2ToCm2(Setup.DampingRateZeroThrottleClutchDisengaged);

	float PeakTorque = Setup.FindPeakTorque(); // In Nm
	PxSetup.mPeakTorque = M2ToCm2(PeakTorque);	// convert Nm to (kg cm^2/s^2)

	// Convert from our curve to PhysX
	PxSetup.mTorqueCurve.clear();
	TArray<FRichCurveKey> TorqueKeys = Setup.TorqueCurve.GetRichCurveConst()->GetCopyOfKeys();
	int32 NumTorqueCurveKeys = FMath::Min<int32>(TorqueKeys.Num(), PxVehicleEngineData::eMAX_NB_ENGINE_TORQUE_CURVE_ENTRIES);
	for (int32 KeyIdx = 0; KeyIdx < NumTorqueCurveKeys; KeyIdx++)
	{
		FRichCurveKey& Key = TorqueKeys[KeyIdx];
		const float KeyFloat = FMath::IsNearlyZero(Setup.MaxRPM) ? 0.f : Key.Time / Setup.MaxRPM;
		const float ValueFloat = FMath::IsNearlyZero(PeakTorque) ? 0.f : Key.Value / PeakTorque;
		PxSetup.mTorqueCurve.addPair(FMath::Clamp(KeyFloat, 0.f, 1.f), FMath::Clamp(ValueFloat, 0.f, 1.f)); // Normalize torque to 0-1 range
	}
}

static void GetVehicleGearSetup(const FVehicleTransmissionData& Setup, PxVehicleGearsData& PxSetup)
{
	PxSetup.mSwitchTime = Setup.GearSwitchTime;
	PxSetup.mRatios[PxVehicleGearsData::eREVERSE] = Setup.ReverseGearRatio;
	for (int32 i = 0; i < Setup.ForwardGears.Num(); i++)
	{
		PxSetup.mRatios[i + PxVehicleGearsData::eFIRST] = Setup.ForwardGears[i].Ratio;
	}
	PxSetup.mFinalRatio = Setup.FinalRatio;
	PxSetup.mNbRatios = Setup.ForwardGears.Num() + PxVehicleGearsData::eFIRST;
}

static void GetVehicleAutoBoxSetup(const FVehicleTransmissionData& Setup, PxVehicleAutoBoxData& PxSetup)
{
	for (int32 i = 0; i < Setup.ForwardGears.Num(); i++)
	{
		const FVehicleGearData& GearData = Setup.ForwardGears[i];
		PxSetup.mUpRatios[i + PxVehicleGearsData::eFIRST] = GearData.UpRatio;
		PxSetup.mDownRatios[i + PxVehicleGearsData::eFIRST] = GearData.DownRatio;
	}
	PxSetup.mUpRatios[PxVehicleGearsData::eNEUTRAL] = Setup.NeutralGearUpRatio;
	PxSetup.setLatency(Setup.GearAutoBoxLatency);
}

void SetupDriveHelper(const UWheeledVehicleMovementComponent4W* VehicleData, const PxVehicleWheelsSimData* PWheelsSimData, PxVehicleDriveSimData4W& DriveData)
{
	PxVehicleDifferential4WData DifferentialSetup;
	GetVehicleDifferential4WSetup(VehicleData->DifferentialSetup, DifferentialSetup);
	
	DriveData.setDiffData(DifferentialSetup);

	PxVehicleEngineData EngineSetup;
	GetVehicleEngineSetup(VehicleData->EngineSetup, EngineSetup);
	DriveData.setEngineData(EngineSetup);

	PxVehicleClutchData ClutchSetup;
	ClutchSetup.mStrength = M2ToCm2(VehicleData->TransmissionSetup.ClutchStrength);
	DriveData.setClutchData(ClutchSetup);

	FVector WheelCentreOffsets[4];
	WheelCentreOffsets[PxVehicleDrive4WWheelOrder::eFRONT_LEFT] = P2UVector(PWheelsSimData->getWheelCentreOffset(PxVehicleDrive4WWheelOrder::eFRONT_LEFT));
	WheelCentreOffsets[PxVehicleDrive4WWheelOrder::eFRONT_RIGHT] = P2UVector(PWheelsSimData->getWheelCentreOffset(PxVehicleDrive4WWheelOrder::eFRONT_RIGHT));
	WheelCentreOffsets[PxVehicleDrive4WWheelOrder::eREAR_LEFT] = P2UVector(PWheelsSimData->getWheelCentreOffset(PxVehicleDrive4WWheelOrder::eREAR_LEFT));
	WheelCentreOffsets[PxVehicleDrive4WWheelOrder::eREAR_RIGHT] = P2UVector(PWheelsSimData->getWheelCentreOffset(PxVehicleDrive4WWheelOrder::eREAR_RIGHT));

	PxVehicleAckermannGeometryData AckermannSetup;
	AckermannSetup.mAccuracy = VehicleData->AckermannAccuracy;
	AckermannSetup.mAxleSeparation = FMath::Abs(WheelCentreOffsets[PxVehicleDrive4WWheelOrder::eFRONT_LEFT].X - WheelCentreOffsets[PxVehicleDrive4WWheelOrder::eREAR_LEFT].X);
	AckermannSetup.mFrontWidth = FMath::Abs(WheelCentreOffsets[PxVehicleDrive4WWheelOrder::eFRONT_RIGHT].Y - WheelCentreOffsets[PxVehicleDrive4WWheelOrder::eFRONT_LEFT].Y);
	AckermannSetup.mRearWidth = FMath::Abs(WheelCentreOffsets[PxVehicleDrive4WWheelOrder::eREAR_RIGHT].Y - WheelCentreOffsets[PxVehicleDrive4WWheelOrder::eREAR_LEFT].Y);
	DriveData.setAckermannGeometryData(AckermannSetup);

	PxVehicleGearsData GearSetup;
	GetVehicleGearSetup(VehicleData->TransmissionSetup, GearSetup);	
	DriveData.setGearsData(GearSetup);

	PxVehicleAutoBoxData AutoBoxSetup;
	GetVehicleAutoBoxSetup(VehicleData->TransmissionSetup, AutoBoxSetup);
	DriveData.setAutoBoxData(AutoBoxSetup);
}

void UWheeledVehicleMovementComponent4W::SetupVehicleDrive(PxVehicleWheelsSimData* PWheelsSimData)
{
	if(WheelSetups.Num() != 4)
	{
		PVehicle = nullptr;
		PVehicleDrive = nullptr;
		return;
	}

	// Setup drive data
	PxVehicleDriveSimData4W DriveData;
	SetupDriveHelper(this, PWheelsSimData, DriveData);

	// Create the vehicle
	PxVehicleDrive4W* PVehicleDrive4W = PxVehicleDrive4W::allocate(4);
	check(PVehicleDrive4W);

	ExecuteOnPxRigidDynamicReadWrite(UpdatedPrimitive->GetBodyInstance(), [&] (PxRigidDynamic* PRigidDynamic)
	{
		PVehicleDrive4W->setup( GPhysXSDK, PRigidDynamic, *PWheelsSimData, DriveData, 0);
		PVehicleDrive4W->setToRestState();

		// cleanup
		PWheelsSimData->free();
	});

	// cache values
	PVehicle = PVehicleDrive4W;
	PVehicleDrive = PVehicleDrive4W;

	SetUseAutoGears(TransmissionSetup.bUseGearAutoBox);
}

void UWheeledVehicleMovementComponent4W::UpdateSimulation(float DeltaTime)
{
	if (PVehicleDrive == NULL)
		return;

	UpdatedPrimitive->GetBodyInstance()->ExecuteOnPhysicsReadWrite([&]
	{
		PxVehicleDrive4WRawInputData RawInputData;
		RawInputData.setAnalogAccel(ThrottleInput);
		RawInputData.setAnalogSteer(SteeringInput);
		RawInputData.setAnalogBrake(BrakeInput);
		RawInputData.setAnalogHandbrake(HandbrakeInput);

		if (!PVehicleDrive->mDriveDynData.getUseAutoGears())
		{
			RawInputData.setGearUp(bRawGearUpInput);
			RawInputData.setGearDown(bRawGearDownInput);
		}

		// Convert from our curve to PxFixedSizeLookupTable
		PxFixedSizeLookupTable<8> SpeedSteerLookup;
		TArray<FRichCurveKey> SteerKeys = SteeringCurve.GetRichCurve()->GetCopyOfKeys();
		const int32 MaxSteeringSamples = FMath::Min(8, SteerKeys.Num());
		for (int32 KeyIdx = 0; KeyIdx < MaxSteeringSamples; KeyIdx++)
		{
			FRichCurveKey& Key = SteerKeys[KeyIdx];
			SpeedSteerLookup.addPair(KmHToCmS(Key.Time), FMath::Clamp(Key.Value, 0.f, 1.f));
		}

		PxVehiclePadSmoothingData SmoothData = {
			{ ThrottleInputRate.RiseRate, BrakeInputRate.RiseRate, HandbrakeInputRate.RiseRate, SteeringInputRate.RiseRate, SteeringInputRate.RiseRate },
			{ ThrottleInputRate.FallRate, BrakeInputRate.FallRate, HandbrakeInputRate.FallRate, SteeringInputRate.FallRate, SteeringInputRate.FallRate }
		};

		PxVehicleDrive4W* PVehicleDrive4W = (PxVehicleDrive4W*)PVehicleDrive;
		PxVehicleDrive4WSmoothAnalogRawInputsAndSetAnalogInputs(SmoothData, SpeedSteerLookup, RawInputData, DeltaTime, false, *PVehicleDrive4W);
	});
}


void UWheeledVehicleMovementComponent4W::UpdateEngineSetup(const FVehicleEngineData& NewEngineSetup)
{
	if (PVehicleDrive)
	{
		PxVehicleEngineData EngineData;
		GetVehicleEngineSetup(NewEngineSetup, EngineData);

		PxVehicleDrive4W* PVehicleDrive4W = (PxVehicleDrive4W*)PVehicleDrive;
		PVehicleDrive4W->mDriveSimData.setEngineData(EngineData);
	}
}

void UWheeledVehicleMovementComponent4W::UpdateDifferentialSetup(const FVehicleDifferential4WData& NewDifferentialSetup)
{
	if (PVehicleDrive)
	{
		PxVehicleDifferential4WData DifferentialData;
		GetVehicleDifferential4WSetup(NewDifferentialSetup, DifferentialData);

		PxVehicleDrive4W* PVehicleDrive4W = (PxVehicleDrive4W*)PVehicleDrive;
		PVehicleDrive4W->mDriveSimData.setDiffData(DifferentialData);
	}
}

void UWheeledVehicleMovementComponent4W::UpdateTransmissionSetup(const FVehicleTransmissionData& NewTransmissionSetup)
{
	if (PVehicleDrive)
	{
		PxVehicleGearsData GearData;
		GetVehicleGearSetup(NewTransmissionSetup, GearData);

		PxVehicleAutoBoxData AutoBoxData;
		GetVehicleAutoBoxSetup(NewTransmissionSetup, AutoBoxData);

		PxVehicleDrive4W* PVehicleDrive4W = (PxVehicleDrive4W*)PVehicleDrive;
		PVehicleDrive4W->mDriveSimData.setGearsData(GearData);
		PVehicleDrive4W->mDriveSimData.setAutoBoxData(AutoBoxData);
	}
}

void BackwardsConvertCm2ToM2(float& val, float defaultValue)
{
	if (val != defaultValue)
	{
		val = Cm2ToM2(val);
	}
}

void UWheeledVehicleMovementComponent4W::Serialize(FArchive & Ar)
{
	Super::Serialize(Ar);
	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_VEHICLES_UNIT_CHANGE)
	{
		PxVehicleEngineData DefEngineData;
		float DefaultRPM = OmegaToRPM(DefEngineData.mMaxOmega);
		
		//we need to convert from old units to new. This backwards compatable code fails in the rare case that they were using very strange values that are the new defaults in the correct units.
		EngineSetup.MaxRPM = EngineSetup.MaxRPM != DefaultRPM ? OmegaToRPM(EngineSetup.MaxRPM) : DefaultRPM;	//need to convert from rad/s to RPM
	}

	if (Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_VEHICLES_UNIT_CHANGE2)
	{
		PxVehicleEngineData DefEngineData;
		PxVehicleClutchData DefClutchData;

		//we need to convert from old units to new. This backwards compatable code fails in the rare case that they were using very strange values that are the new defaults in the correct units.
		BackwardsConvertCm2ToM2(EngineSetup.DampingRateFullThrottle, DefEngineData.mDampingRateFullThrottle);
		BackwardsConvertCm2ToM2(EngineSetup.DampingRateZeroThrottleClutchDisengaged, DefEngineData.mDampingRateZeroThrottleClutchDisengaged);
		BackwardsConvertCm2ToM2(EngineSetup.DampingRateZeroThrottleClutchEngaged, DefEngineData.mDampingRateZeroThrottleClutchEngaged);
		BackwardsConvertCm2ToM2(EngineSetup.MOI, DefEngineData.mMOI);
		BackwardsConvertCm2ToM2(TransmissionSetup.ClutchStrength, DefClutchData.mStrength);
	}
}

void UWheeledVehicleMovementComponent4W::ComputeConstants()
{
	Super::ComputeConstants();
	MaxEngineRPM = EngineSetup.MaxRPM;
}

