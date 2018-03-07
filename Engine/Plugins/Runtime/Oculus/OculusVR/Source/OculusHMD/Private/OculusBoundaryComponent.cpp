// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OculusBoundaryComponent.h"
#include "OculusHMDPrivate.h"
#include "OculusHMD.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS

#include "OculusHMDModule.h" // for IsOVRPluginAvailable()

namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// Static Type Conversion Helpers for OculusBoundaryComponent
//-------------------------------------------------------------------------------------------------

/** Helper that converts ovrpBoundaryType to EBoundaryType */
static EBoundaryType ToEBoundaryType(ovrpBoundaryType Source)
{
	EBoundaryType Destination = EBoundaryType::Boundary_Outer; // Best attempt at initialization

	switch (Source)
	{
		case ovrpBoundary_Outer:
			Destination = EBoundaryType::Boundary_Outer;
			break;
		case ovrpBoundary_PlayArea:
			Destination = EBoundaryType::Boundary_PlayArea;
			break;
		default:
			break;
	}
	return Destination;
}

/** Helper that converts EBoundaryType to ovrpBoundaryType */
static ovrpBoundaryType ToOvrpBoundaryType(EBoundaryType Source)
{
	ovrpBoundaryType Destination = ovrpBoundary_Outer; // Best attempt at initialization

	switch (Source)
	{
		case EBoundaryType::Boundary_Outer:
			Destination = ovrpBoundary_Outer;
			break;
		case EBoundaryType::Boundary_PlayArea:
			Destination = ovrpBoundary_PlayArea;
			break;
		default:
			break;
	}
	return Destination;
}


//-------------------------------------------------------------------------------------------------
// Other Static Helpers (decomposition) for OculusBoundaryComponent
//-------------------------------------------------------------------------------------------------

static FVector PointToWorldSpace(ovrpVector3f ovrPoint)
{
	FOculusHMD* OculusHMD = (FOculusHMD*)(GEngine->XRSystem->GetHMDDevice());
	return OculusHMD->ScaleAndMovePointWithPlayer(ovrPoint);
}

static FVector DimensionsToWorldSpace(ovrpVector3f Dimensions)
{
	FOculusHMD* OculusHMD = (FOculusHMD*)(GEngine->XRSystem->GetHMDDevice());
	FVector WorldLength = OculusHMD->ConvertVector_M2U(Dimensions);
	WorldLength.X = -WorldLength.X;
	return WorldLength;
}

static FVector NormalToWorldSpace(ovrpVector3f Normal)
{
	FVector WorldNormal = ToFVector(Normal);
	float temp = WorldNormal.X;
	WorldNormal.X = -WorldNormal.Z;
	WorldNormal.Z = WorldNormal.Y;
	WorldNormal.Y = temp;
	return WorldNormal;
}

static float DistanceToWorldSpace(float ovrDistance)
{
	FOculusHMD* OculusHMD = (FOculusHMD*)(GEngine->XRSystem->GetHMDDevice());
	return OculusHMD->ConvertFloat_M2U(ovrDistance);
}

/**
 * Helper that checks if DeviceType triggers BoundaryType. If so, stores details about interaction and adds to ResultList!
 * @param ResultList Out-parameter -- a list to add to may or may not be specified. Stores each FBoundaryTestResult corresponding to a device that has triggered boundary system
 * @param Node Specifies device type to test
 * @param BoundaryType Specifies EBoundaryType::Boundary_Outer or EBoundaryType::Boundary_PlayArea to test
 * @return true if DeviceType triggers BoundaryType
 */
static bool AddInteractionPairsToList(TArray<FBoundaryTestResult>* ResultList, ovrpNode Node, ovrpBoundaryType BoundaryType)
{
	if (FOculusHMDModule::Get().IsOVRPluginAvailable())
	{
		ovrpBoundaryTestResult TestResult;

		if (OVRP_FAILURE(ovrp_TestBoundaryNode2(Node, BoundaryType, &TestResult)))
			return false;

		bool IsTriggering = (TestResult.IsTriggering != 0);

		if (IsTriggering && ResultList != NULL)
		{
			FBoundaryTestResult InteractionInfo;
			memset(&InteractionInfo, 0, sizeof(FBoundaryTestResult));

			InteractionInfo.IsTriggering = IsTriggering;
			InteractionInfo.DeviceType = ToETrackedDeviceType(Node);
			InteractionInfo.ClosestDistance = DistanceToWorldSpace(TestResult.ClosestDistance);
			InteractionInfo.ClosestPoint = PointToWorldSpace(TestResult.ClosestPoint);
			InteractionInfo.ClosestPointNormal = NormalToWorldSpace(TestResult.ClosestPointNormal);

			ResultList->Add(InteractionInfo);
		}

		return IsTriggering;
	}
	return false;
}

/**
 * Helper that gets geometry (3D points) of outer boundaries or play area (specified by BoundaryType)
 * @param BoundaryType Must be ovrpBoundary_Outer or ovrpBoundary_PlayArea, specifies the type of boundary geometry to retrieve
 * @return Array of 3D points in Unreal world coordinate space corresponding to boundary geometry.
 */
static TArray<FVector> GetBoundaryPoints(ovrpBoundaryType BoundaryType)
{
	TArray<FVector> BoundaryPointList;

	if (FOculusHMDModule::Get().IsOVRPluginAvailable())
	{
		int NumPoints = 0;

		if (OVRP_SUCCESS(ovrp_GetBoundaryGeometry3(BoundaryType, NULL, &NumPoints)))
		{
			//allocate points
			const int BufferSize = NumPoints;
			ovrpVector3f* BoundaryPoints = new ovrpVector3f[BufferSize];

			if (OVRP_SUCCESS(ovrp_GetBoundaryGeometry3(BoundaryType, BoundaryPoints, &NumPoints)))
			{
				NumPoints = FMath::Min(BufferSize, NumPoints);
				check(NumPoints <= BufferSize); // For static analyzer
				BoundaryPointList.Reserve(NumPoints);

				for (int i = 0; i < NumPoints; i++)
				{
					FVector point = PointToWorldSpace(BoundaryPoints[i]);
					BoundaryPointList.Add(point);
				}
			}

			delete[] BoundaryPoints;
		}
	}
	return BoundaryPointList;
}

/**
 * @param BoundaryType must be EBoundaryType::Boundary_Outer or EBoundaryType::Boundary_PlayArea
 * @param Point 3D point in Unreal coordinate space to be tested
 * @return Information about distance from specified boundary, whether the boundary is triggered, etc.
 */
static FBoundaryTestResult CheckPointInBounds(EBoundaryType BoundaryType, const FVector Point)
{
	FBoundaryTestResult InteractionInfo;
	memset(&InteractionInfo, 0, sizeof(FBoundaryTestResult));

	if (FOculusHMDModule::Get().IsOVRPluginAvailable())
	{
		ovrpVector3f OvrpPoint = ToOvrpVector3f(Point);
		ovrpBoundaryType OvrpBoundaryType = ToOvrpBoundaryType(BoundaryType);
		ovrpBoundaryTestResult InteractionResult;

		if (OVRP_SUCCESS(ovrp_TestBoundaryPoint2(OvrpPoint, OvrpBoundaryType, &InteractionResult)))
		{
			InteractionInfo.IsTriggering = (InteractionResult.IsTriggering != 0);
			InteractionInfo.ClosestDistance = DistanceToWorldSpace(InteractionResult.ClosestDistance);
			InteractionInfo.ClosestPoint = PointToWorldSpace(InteractionResult.ClosestPoint);
			InteractionInfo.ClosestPointNormal = NormalToWorldSpace(InteractionResult.ClosestPointNormal);
			InteractionInfo.DeviceType = ETrackedDeviceType::None;
		}
	}

	return InteractionInfo;
}

} // namespace OculusHMD

#endif // OCULUS_HMD_SUPPORTED_PLATFORMS

using namespace OculusHMD;


//-------------------------------------------------------------------------------------------------
// OculusBoundaryComponent Member Functions
//-------------------------------------------------------------------------------------------------

UOculusBoundaryComponent::UOculusBoundaryComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;
	bAutoActivate = true;
	bIsOuterBoundaryTriggered = false;
#endif
}

void UOculusBoundaryComponent::BeginPlay()
{
	Super::BeginPlay();
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	bIsOuterBoundaryTriggered = AddInteractionPairsToList(NULL, ovrpNode_Head, ovrpBoundary_Outer) ||
								AddInteractionPairsToList(NULL, ovrpNode_HandLeft, ovrpBoundary_Outer) ||
								AddInteractionPairsToList(NULL, ovrpNode_HandRight, ovrpBoundary_Outer);
#endif
}

OculusHMD::FOculusHMD* GetOculusHMD()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	if (GEngine && GEngine->XRSystem.IsValid())
	{
		IHeadMountedDisplay* HMDDevice = GEngine->XRSystem->GetHMDDevice();
		if (HMDDevice)
		{
			EHMDDeviceType::Type HMDDeviceType = HMDDevice->GetHMDDeviceType();
			if (HMDDeviceType == EHMDDeviceType::DT_OculusRift || HMDDeviceType == EHMDDeviceType::DT_GearVR)
			{
				return static_cast<OculusHMD::FOculusHMD*>(HMDDevice);
			}
		}
	}
#endif
	return nullptr;
}

void UOculusBoundaryComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	FOculusHMD* OculusHMD = GetOculusHMD();
	if (OculusHMD && OculusHMD->IsHMDActive())
	{
		OuterBoundsInteractionList.Empty(); // Reset list of FBoundaryTestResults

		// Test each device with outer boundary, adding results to OuterBoundsInteractionList
		bool HMDTriggered = AddInteractionPairsToList(&OuterBoundsInteractionList, ovrpNode_Head, ovrpBoundary_Outer);
		bool LTouchTriggered = AddInteractionPairsToList(&OuterBoundsInteractionList, ovrpNode_HandLeft, ovrpBoundary_Outer);
		bool RTouchTriggered = AddInteractionPairsToList(&OuterBoundsInteractionList, ovrpNode_HandRight, ovrpBoundary_Outer);
		bool OuterBoundsTriggered = HMDTriggered || LTouchTriggered || RTouchTriggered;


		if (OuterBoundsTriggered != bIsOuterBoundaryTriggered) // outer boundary triggered status has changed
		{
			if (OuterBoundsTriggered)
			{
				OnOuterBoundaryTriggered.Broadcast(OuterBoundsInteractionList);
			}
			else
			{
				OnOuterBoundaryReturned.Broadcast();
			}
		}

		bIsOuterBoundaryTriggered = OuterBoundsTriggered;
	}
#endif
}

bool UOculusBoundaryComponent::IsOuterBoundaryDisplayed()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	if (FOculusHMDModule::Get().IsOVRPluginAvailable())
	{
		ovrpBool boundaryVisible;
		return OVRP_SUCCESS(ovrp_GetBoundaryVisible2(&boundaryVisible)) && boundaryVisible;
	}
#endif 
	return false;
}

bool UOculusBoundaryComponent::IsOuterBoundaryTriggered()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	return bIsOuterBoundaryTriggered;
#else
	return false;
#endif
}

bool UOculusBoundaryComponent::SetOuterBoundaryColor(const FColor InBoundaryColor)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	if (FOculusHMDModule::Get().IsOVRPluginAvailable())
	{
		ovrpColorf NewColor = { InBoundaryColor.R / 255.f, InBoundaryColor.G / 255.f, InBoundaryColor.B / 255.f, InBoundaryColor.A / 255.f };
		ovrpBoundaryLookAndFeel BoundaryLookAndFeel;
		BoundaryLookAndFeel.Color = NewColor;

		return OVRP_SUCCESS(ovrp_SetBoundaryLookAndFeel2(BoundaryLookAndFeel));
	}
#endif 
	return true;
}

bool UOculusBoundaryComponent::ResetOuterBoundaryColor()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	if (FOculusHMDModule::Get().IsOVRPluginAvailable())
	{
		return OVRP_SUCCESS(ovrp_ResetBoundaryLookAndFeel2());
	}
#endif
	return true;
}

TArray<FVector> UOculusBoundaryComponent::GetPlayAreaPoints()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	return GetBoundaryPoints(ovrpBoundary_PlayArea);
#else
	return TArray<FVector>();
#endif
}

TArray<FVector> UOculusBoundaryComponent::GetOuterBoundaryPoints()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	return GetBoundaryPoints(ovrpBoundary_Outer);
#else
	return TArray<FVector>();
#endif
}

FVector UOculusBoundaryComponent::GetPlayAreaDimensions()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	if (FOculusHMDModule::Get().IsOVRPluginAvailable())
	{
		ovrpVector3f Dimensions;

		if (OVRP_FAILURE(ovrp_GetBoundaryDimensions2(ovrpBoundary_PlayArea, &Dimensions)))
			return FVector::ZeroVector;

		return DimensionsToWorldSpace(Dimensions);
	}
#endif
	return FVector::ZeroVector;
}

FVector UOculusBoundaryComponent::GetOuterBoundaryDimensions()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	if (FOculusHMDModule::Get().IsOVRPluginAvailable())
	{
		ovrpVector3f Dimensions;

		if (OVRP_FAILURE(ovrp_GetBoundaryDimensions2(ovrpBoundary_Outer, &Dimensions)))
			return FVector::ZeroVector;

		return DimensionsToWorldSpace(Dimensions);
	}
#endif
	return FVector::ZeroVector;
}

FBoundaryTestResult UOculusBoundaryComponent::CheckIfPointWithinPlayArea(const FVector Point)
{
	FBoundaryTestResult PlayAreaInteractionInfo;
	memset(&PlayAreaInteractionInfo, 0, sizeof(FBoundaryTestResult));

#if OCULUS_HMD_SUPPORTED_PLATFORMS
	PlayAreaInteractionInfo = CheckPointInBounds(EBoundaryType::Boundary_PlayArea, Point);
#endif

	return PlayAreaInteractionInfo;
}

FBoundaryTestResult UOculusBoundaryComponent::CheckIfPointWithinOuterBounds(const FVector Point)
{
	FBoundaryTestResult BoundaryInteractionInfo;
	memset(&BoundaryInteractionInfo, 0, sizeof(FBoundaryTestResult));

#if OCULUS_HMD_SUPPORTED_PLATFORMS
	BoundaryInteractionInfo = CheckPointInBounds(EBoundaryType::Boundary_Outer, Point);
#endif

	return BoundaryInteractionInfo;
}

bool UOculusBoundaryComponent::RequestOuterBoundaryVisible(bool BoundaryVisible)
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	if (FOculusHMDModule::Get().IsOVRPluginAvailable())
	{
		return OVRP_SUCCESS(ovrp_SetBoundaryVisible2(BoundaryVisible));
	}
#endif
	return false;
}

FBoundaryTestResult UOculusBoundaryComponent::GetTriggeredPlayAreaInfo(ETrackedDeviceType DeviceType)
{
	FBoundaryTestResult InteractionInfo;
	memset(&InteractionInfo, 0, sizeof(FBoundaryTestResult));

#if OCULUS_HMD_SUPPORTED_PLATFORMS
	if (FOculusHMDModule::Get().IsOVRPluginAvailable())
	{
		ovrpNode OvrpNode = ToOvrpNode(DeviceType);
		ovrpBoundaryTestResult TestResult;

		if (OVRP_SUCCESS(ovrp_TestBoundaryNode2(OvrpNode, ovrpBoundary_PlayArea, &TestResult)) && TestResult.IsTriggering)
		{
			InteractionInfo.IsTriggering = true;
			InteractionInfo.DeviceType = ToETrackedDeviceType(OvrpNode);
			InteractionInfo.ClosestDistance = DistanceToWorldSpace(TestResult.ClosestDistance);
			InteractionInfo.ClosestPoint = PointToWorldSpace(TestResult.ClosestPoint);
			InteractionInfo.ClosestPointNormal = NormalToWorldSpace(TestResult.ClosestPointNormal);
		}
	}
#endif

	return InteractionInfo;
}

TArray<FBoundaryTestResult> UOculusBoundaryComponent::GetTriggeredOuterBoundaryInfo()
{
#if OCULUS_HMD_SUPPORTED_PLATFORMS
	return OuterBoundsInteractionList;
#else
	return TArray<FBoundaryTestResult>();
#endif
}
