// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MeshUtilities.h"
#include "MeshUtilitiesPrivate.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/Material.h"
#include "RawMesh.h"
#include "StaticMeshResources.h"
#include "DistanceFieldAtlas.h"

//@todo - implement required vector intrinsics for other implementations
#if PLATFORM_ENABLE_VECTORINTRINSICS
#include "kDOP.h"
#endif

#if USE_EMBREE
#include <embree2/rtcore.h>
#include <embree2/rtcore_ray.h>
#else
typedef void* RTCDevice;
typedef void* RTCScene;
#endif

//@todo - implement required vector intrinsics for other implementations
#if PLATFORM_ENABLE_VECTORINTRINSICS

class FMeshBuildDataProvider
{
public:

	/** Initialization constructor. */
	FMeshBuildDataProvider(
		const TkDOPTree<const FMeshBuildDataProvider, uint32>& InkDopTree) :
		kDopTree(InkDopTree)
	{}

	// kDOP data provider interface.

	FORCEINLINE const TkDOPTree<const FMeshBuildDataProvider, uint32>& GetkDOPTree(void) const
	{
		return kDopTree;
	}

	FORCEINLINE const FMatrix& GetLocalToWorld(void) const
	{
		return FMatrix::Identity;
	}

	FORCEINLINE const FMatrix& GetWorldToLocal(void) const
	{
		return FMatrix::Identity;
	}

	FORCEINLINE FMatrix GetLocalToWorldTransposeAdjoint(void) const
	{
		return FMatrix::Identity;
	}

	FORCEINLINE float GetDeterminant(void) const
	{
		return 1.0f;
	}

private:

	const TkDOPTree<const FMeshBuildDataProvider, uint32>& kDopTree;
};

/** Generates unit length, stratified and uniformly distributed direction samples in a hemisphere. */
void GenerateStratifiedUniformHemisphereSamples(int32 NumThetaSteps, int32 NumPhiSteps, FRandomStream& RandomStream, TArray<FVector4>& Samples)
{
	Samples.Empty(NumThetaSteps * NumPhiSteps);
	for (int32 ThetaIndex = 0; ThetaIndex < NumThetaSteps; ThetaIndex++)
	{
		for (int32 PhiIndex = 0; PhiIndex < NumPhiSteps; PhiIndex++)
		{
			const float U1 = RandomStream.GetFraction();
			const float U2 = RandomStream.GetFraction();

			const float Fraction1 = (ThetaIndex + U1) / (float)NumThetaSteps;
			const float Fraction2 = (PhiIndex + U2) / (float)NumPhiSteps;

			const float R = FMath::Sqrt(1.0f - Fraction1 * Fraction1);

			const float Phi = 2.0f * (float)PI * Fraction2;
			// Convert to Cartesian
			Samples.Add(FVector4(FMath::Cos(Phi) * R, FMath::Sin(Phi) * R, Fraction1));
		}
	}
}

struct FEmbreeTriangleDesc
{
	int16 ElementIndex;
};

// Mapping between Embree Geometry Id and engine Mesh/LOD Id
struct FEmbreeGeometry
{
	TArray<FEmbreeTriangleDesc> TriangleDescs; // The material ID of each triangle.
};

#if USE_EMBREE

struct FEmbreeRay : public RTCRay
{
	FEmbreeRay() :
		ElementIndex(-1)
	{
		u = v = 0;
		time = 0;
		mask = 0xFFFFFFFF;
		geomID = -1;
		instID = -1;
		primID = -1;
	}

	// Additional Outputs.
	int32 ElementIndex; // Material Index
};

void EmbreeFilterFunc(void* UserPtr, RTCRay& InRay)
{
	FEmbreeGeometry* EmbreeGeometry = (FEmbreeGeometry*)UserPtr;
	FEmbreeRay& EmbreeRay = (FEmbreeRay&)InRay;
	FEmbreeTriangleDesc Desc = EmbreeGeometry->TriangleDescs[InRay.primID];

	EmbreeRay.ElementIndex = Desc.ElementIndex;
}

#endif

class FMeshDistanceFieldAsyncTask : public FNonAbandonableTask
{
public:
	FMeshDistanceFieldAsyncTask(
		TkDOPTree<const FMeshBuildDataProvider, uint32>* InkDopTree,
		bool bInUseEmbree,
		RTCScene InEmbreeScene,
		const TArray<FVector4>* InSampleDirections,
		FBox InVolumeBounds,
		FIntVector InVolumeDimensions,
		float InVolumeMaxDistance,
		int32 InZIndex,
		TArray<float>* DistanceFieldVolume)
		:
		kDopTree(InkDopTree),
		bUseEmbree(bInUseEmbree),
		EmbreeScene(InEmbreeScene),
		SampleDirections(InSampleDirections),
		VolumeBounds(InVolumeBounds),
		VolumeDimensions(InVolumeDimensions),
		VolumeMaxDistance(InVolumeMaxDistance),
		ZIndex(InZIndex),
		OutDistanceFieldVolume(DistanceFieldVolume),
		bNegativeAtBorder(false)
	{}

	void DoWork();

	FORCEINLINE TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FMeshDistanceFieldAsyncTask, STATGROUP_ThreadPoolAsyncTasks);
	}

	bool WasNegativeAtBorder() const
	{
		return bNegativeAtBorder;
	}

private:

	// Readonly inputs
	TkDOPTree<const FMeshBuildDataProvider, uint32>* kDopTree;
	bool bUseEmbree;
	RTCScene EmbreeScene;
	const TArray<FVector4>* SampleDirections;
	FBox VolumeBounds;
	FIntVector VolumeDimensions;
	float VolumeMaxDistance;
	int32 ZIndex;

	// Output
	TArray<float>* OutDistanceFieldVolume;
	bool bNegativeAtBorder;
};

void FMeshDistanceFieldAsyncTask::DoWork()
{
	FMeshBuildDataProvider kDOPDataProvider(*kDopTree);
	const FVector DistanceFieldVoxelSize(VolumeBounds.GetSize() / FVector(VolumeDimensions.X, VolumeDimensions.Y, VolumeDimensions.Z));
	const float VoxelDiameterSqr = DistanceFieldVoxelSize.SizeSquared();

	for (int32 YIndex = 0; YIndex < VolumeDimensions.Y; YIndex++)
	{
		for (int32 XIndex = 0; XIndex < VolumeDimensions.X; XIndex++)
		{
			const FVector VoxelPosition = FVector(XIndex + .5f, YIndex + .5f, ZIndex + .5f) * DistanceFieldVoxelSize + VolumeBounds.Min;
			const int32 Index = (ZIndex * VolumeDimensions.Y * VolumeDimensions.X + YIndex * VolumeDimensions.X + XIndex);

			float MinDistance = VolumeMaxDistance;
			int32 Hit = 0;
			int32 HitBack = 0;

			for (int32 SampleIndex = 0; SampleIndex < SampleDirections->Num(); SampleIndex++)
			{
				const FVector UnitRayDirection = (*SampleDirections)[SampleIndex];
				const FVector EndPosition = VoxelPosition + UnitRayDirection * VolumeMaxDistance;

				if (FMath::LineBoxIntersection(VolumeBounds, VoxelPosition, EndPosition, UnitRayDirection))
				{
#if USE_EMBREE
					if (bUseEmbree)
					{
						FEmbreeRay EmbreeRay;

						FVector RayDirection = EndPosition - VoxelPosition;
						EmbreeRay.org[0] = VoxelPosition.X;
						EmbreeRay.org[1] = VoxelPosition.Y;
						EmbreeRay.org[2] = VoxelPosition.Z;
						EmbreeRay.dir[0] = RayDirection.X;
						EmbreeRay.dir[1] = RayDirection.Y;
						EmbreeRay.dir[2] = RayDirection.Z;
						EmbreeRay.tnear = 0;
						EmbreeRay.tfar = 1.0f;

						rtcIntersect(EmbreeScene, EmbreeRay);

						if (EmbreeRay.geomID != -1 && EmbreeRay.primID != -1)
						{
							Hit++;

							const FVector HitNormal = FVector(EmbreeRay.Ng[0], EmbreeRay.Ng[1], EmbreeRay.Ng[2]).GetSafeNormal();

							if (FVector::DotProduct(UnitRayDirection, HitNormal) > 0
								// MaterialIndex on the build triangles was set to 1 if two-sided, or 0 if one-sided
								&& EmbreeRay.ElementIndex == 0)
							{
								HitBack++;
							}

							const float CurrentDistance = VolumeMaxDistance * EmbreeRay.tfar;

							if (CurrentDistance < MinDistance)
							{
								MinDistance = CurrentDistance;
							}
						}
					}
					else
#endif
					{
						FkHitResult Result;

						TkDOPLineCollisionCheck<const FMeshBuildDataProvider, uint32> kDOPCheck(
							VoxelPosition,
							EndPosition,
							true,
							kDOPDataProvider,
							&Result);

						bool bHit = kDopTree->LineCheck(kDOPCheck);

						if (bHit)
						{
							Hit++;

							const FVector HitNormal = kDOPCheck.GetHitNormal();

							if (FVector::DotProduct(UnitRayDirection, HitNormal) > 0
								// MaterialIndex on the build triangles was set to 1 if two-sided, or 0 if one-sided
								&& kDOPCheck.Result->Item == 0)
							{
								HitBack++;
							}

							const float CurrentDistance = VolumeMaxDistance * Result.Time;

							if (CurrentDistance < MinDistance)
							{
								MinDistance = CurrentDistance;
							}
						}
					}
				}
			}

			const float UnsignedDistance = MinDistance;

			// Consider this voxel 'inside' an object if more than 50% of the rays hit back faces
			MinDistance *= (Hit == 0 || HitBack < SampleDirections->Num() * .5f) ? 1 : -1;

			// If we are very close to a surface and nearly all of our rays hit backfaces, treat as inside
			// This is important for one sided planes
			if (FMath::Square(UnsignedDistance) < VoxelDiameterSqr && HitBack > .95f * Hit)
			{
				MinDistance = -UnsignedDistance;
			}

			MinDistance = FMath::Min(MinDistance, VolumeMaxDistance);
			const float VolumeSpaceDistance = MinDistance / VolumeBounds.GetExtent().GetMax();

			if (MinDistance < 0 &&
				(XIndex == 0 || XIndex == VolumeDimensions.X - 1 ||
				YIndex == 0 || YIndex == VolumeDimensions.Y - 1 ||
				ZIndex == 0 || ZIndex == VolumeDimensions.Z - 1))
			{
				bNegativeAtBorder = true;
			}

			(*OutDistanceFieldVolume)[Index] = VolumeSpaceDistance;
		}
	}
}

void FMeshUtilities::GenerateSignedDistanceFieldVolumeData(
	FString MeshName,
	const FStaticMeshLODResources& LODModel,
	class FQueuedThreadPool& ThreadPool,
	const TArray<EBlendMode>& MaterialBlendModes,
	const FBoxSphereBounds& Bounds,
	float DistanceFieldResolutionScale,
	bool bGenerateAsIfTwoSided,
	FDistanceFieldVolumeData& OutData)
{
	if (DistanceFieldResolutionScale > 0)
	{
		const double StartTime = FPlatformTime::Seconds();
		const FPositionVertexBuffer& PositionVertexBuffer = LODModel.PositionVertexBuffer;
		FIndexArrayView Indices = LODModel.IndexBuffer.GetArrayView();
		TArray<FkDOPBuildCollisionTriangle<uint32> > BuildTriangles;

		RTCDevice EmbreeDevice = NULL;
		RTCScene EmbreeScene = NULL;
		bool bUseEmbree = false;
		
#if USE_EMBREE

		static const auto CVarEmbree = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DistanceFieldBuild.UseEmbree"));
		bUseEmbree = CVarEmbree->GetValueOnAnyThread() != 0;

		if (bUseEmbree)
		{
			EmbreeDevice = rtcNewDevice(NULL);
			
			RTCError ReturnErrorNewDevice = rtcDeviceGetError(EmbreeDevice);
			if (ReturnErrorNewDevice != RTC_NO_ERROR)
			{
				UE_LOG(LogMeshUtilities, Warning, TEXT("GenerateSignedDistanceFieldVolumeData failed for %s. Embree rtcNewDevice failed. Code: %d"), *MeshName, (int32)ReturnErrorNewDevice);
				return;
			}

			EmbreeScene = rtcDeviceNewScene(EmbreeDevice, RTC_SCENE_STATIC, RTC_INTERSECT1);
			
			RTCError ReturnErrorNewScene = rtcDeviceGetError(EmbreeDevice);
			if (ReturnErrorNewScene != RTC_NO_ERROR)
			{
				UE_LOG(LogMeshUtilities, Warning, TEXT("GenerateSignedDistanceFieldVolumeData failed for %s. Embree rtcDeviceNewScene failed. Code: %d"), *MeshName, (int32)ReturnErrorNewScene);
				rtcDeleteDevice(EmbreeDevice);
				return;
			}
		}
#endif

		FVector BoundsSize = Bounds.GetBox().GetExtent() * 2;
		float MaxDimension = FMath::Max(FMath::Max(BoundsSize.X, BoundsSize.Y), BoundsSize.Z);

		// Consider the mesh a plane if it is very flat
		const bool bMeshWasPlane = BoundsSize.Z * 100 < MaxDimension
			// And it lies mostly on the origin
			&& Bounds.Origin.Z - Bounds.BoxExtent.Z < KINDA_SMALL_NUMBER
			&& Bounds.Origin.Z + Bounds.BoxExtent.Z > -KINDA_SMALL_NUMBER;

		TArray<int32> FilteredTriangles;
		FilteredTriangles.Empty(Indices.Num() / 3);

		for (int32 i = 0; i < Indices.Num(); i += 3)
		{
			FVector V0 = PositionVertexBuffer.VertexPosition(Indices[i + 0]);
			FVector V1 = PositionVertexBuffer.VertexPosition(Indices[i + 1]);
			FVector V2 = PositionVertexBuffer.VertexPosition(Indices[i + 2]);

			if (bMeshWasPlane)
			{
				// Flatten out the mesh into an actual plane, this will allow us to manipulate the component's Z scale at runtime without artifacts
				V0.Z = 0;
				V1.Z = 0;
				V2.Z = 0;
			}

			const FVector LocalNormal = ((V1 - V2) ^ (V0 - V2)).GetSafeNormal();

			// No degenerates
			if (LocalNormal.IsUnit())
			{
				bool bTriangleIsOpaqueOrMasked = false;

				for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
				{
					const FStaticMeshSection& Section = LODModel.Sections[SectionIndex];

					if ((uint32)i >= Section.FirstIndex && (uint32)i < Section.FirstIndex + Section.NumTriangles * 3)
					{
						if (MaterialBlendModes.IsValidIndex(Section.MaterialIndex))
						{
							bTriangleIsOpaqueOrMasked = !IsTranslucentBlendMode(MaterialBlendModes[Section.MaterialIndex]);
						}

						break;
					}
				}

				if (bTriangleIsOpaqueOrMasked)
				{
					FilteredTriangles.Add(i / 3);
				}
			}
		}

		FVector4* EmbreeVertices = NULL;
		int32* EmbreeIndices = NULL;
		uint32 GeomID = 0;
		FEmbreeGeometry Geometry;

#if USE_EMBREE

		if (bUseEmbree)
		{
			GeomID = rtcNewTriangleMesh(EmbreeScene, RTC_GEOMETRY_STATIC, FilteredTriangles.Num(), PositionVertexBuffer.GetNumVertices());

			rtcSetIntersectionFilterFunction(EmbreeScene, GeomID, EmbreeFilterFunc);
			rtcSetOcclusionFilterFunction(EmbreeScene, GeomID, EmbreeFilterFunc);
			rtcSetUserData(EmbreeScene, GeomID, &Geometry);

			EmbreeVertices = (FVector4*)rtcMapBuffer(EmbreeScene, GeomID, RTC_VERTEX_BUFFER);
			EmbreeIndices = (int32*)rtcMapBuffer(EmbreeScene, GeomID, RTC_INDEX_BUFFER);

			Geometry.TriangleDescs.Empty(FilteredTriangles.Num());
		}
#endif

		for (int32 TriangleIndex = 0; TriangleIndex < FilteredTriangles.Num(); TriangleIndex++)
		{
			int32 I0 = Indices[TriangleIndex * 3 + 0];
			int32 I1 = Indices[TriangleIndex * 3 + 1];
			int32 I2 = Indices[TriangleIndex * 3 + 2];

			FVector V0 = PositionVertexBuffer.VertexPosition(I0);
			FVector V1 = PositionVertexBuffer.VertexPosition(I1);
			FVector V2 = PositionVertexBuffer.VertexPosition(I2);

			if (bMeshWasPlane)
			{
				// Flatten out the mesh into an actual plane, this will allow us to manipulate the component's Z scale at runtime without artifacts
				V0.Z = 0;
				V1.Z = 0;
				V2.Z = 0;
			}

			if (bUseEmbree)
			{
				EmbreeIndices[TriangleIndex * 3 + 0] = I0;
				EmbreeIndices[TriangleIndex * 3 + 1] = I1;
				EmbreeIndices[TriangleIndex * 3 + 2] = I2;

				EmbreeVertices[I0] = FVector4(V0, 0);
				EmbreeVertices[I1] = FVector4(V1, 0);
				EmbreeVertices[I2] = FVector4(V2, 0);

				FEmbreeTriangleDesc Desc;
				// Store bGenerateAsIfTwoSided in material index
				Desc.ElementIndex = bGenerateAsIfTwoSided ? 1 : 0;
				Geometry.TriangleDescs.Add(Desc);
			}
			else
			{
				BuildTriangles.Add(FkDOPBuildCollisionTriangle<uint32>(
					// Store bGenerateAsIfTwoSided in material index
					bGenerateAsIfTwoSided,
					V0,
					V1,
					V2));
			}
		}

#if USE_EMBREE

		if (bUseEmbree)
		{
			rtcUnmapBuffer(EmbreeScene, GeomID, RTC_VERTEX_BUFFER);
			rtcUnmapBuffer(EmbreeScene, GeomID, RTC_INDEX_BUFFER);

			RTCError ReturnError = rtcDeviceGetError(EmbreeDevice);
			if (ReturnError != RTC_NO_ERROR)
			{
				UE_LOG(LogMeshUtilities, Warning, TEXT("GenerateSignedDistanceFieldVolumeData failed for %s. Embree rtcUnmapBuffer failed. Code: %d"), *MeshName, (int32)ReturnError);
				rtcDeleteScene(EmbreeScene);
				rtcDeleteDevice(EmbreeDevice);
				return;
			}
		}
#endif

		TkDOPTree<const FMeshBuildDataProvider, uint32> kDopTree;

#if USE_EMBREE

		if (bUseEmbree)
		{
			rtcCommit(EmbreeScene);
			RTCError ReturnError = rtcDeviceGetError(EmbreeDevice);
			if (ReturnError != RTC_NO_ERROR)
			{
				UE_LOG(LogMeshUtilities, Warning, TEXT("GenerateSignedDistanceFieldVolumeData failed for %s. Embree rtcCommit failed. Code: %d"), *MeshName, (int32)ReturnError);
				rtcDeleteScene(EmbreeScene);
				rtcDeleteDevice(EmbreeDevice);
				return;
			}
		}
		else
#endif
		{
			kDopTree.Build(BuildTriangles);
		}

		//@todo - project setting
		const int32 NumVoxelDistanceSamples = 1200;
		TArray<FVector4> SampleDirections;
		const int32 NumThetaSteps = FMath::TruncToInt(FMath::Sqrt(NumVoxelDistanceSamples / (2.0f * (float)PI)));
		const int32 NumPhiSteps = FMath::TruncToInt(NumThetaSteps * (float)PI);
		FRandomStream RandomStream(0);
		GenerateStratifiedUniformHemisphereSamples(NumThetaSteps, NumPhiSteps, RandomStream, SampleDirections);
		TArray<FVector4> OtherHemisphereSamples;
		GenerateStratifiedUniformHemisphereSamples(NumThetaSteps, NumPhiSteps, RandomStream, OtherHemisphereSamples);

		for (int32 i = 0; i < OtherHemisphereSamples.Num(); i++)
		{
			FVector4 Sample = OtherHemisphereSamples[i];
			Sample.Z *= -1;
			SampleDirections.Add(Sample);
		}

		static const auto CVar = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DistanceFields.MaxPerMeshResolution"));
		const int32 PerMeshMax = CVar->GetValueOnAnyThread();

		// Meshes with explicit artist-specified scale can go higher
		const int32 MaxNumVoxelsOneDim = DistanceFieldResolutionScale <= 1 ? PerMeshMax / 2 : PerMeshMax;
		const int32 MinNumVoxelsOneDim = 8;

		static const auto CVarDensity = IConsoleManager::Get().FindTConsoleVariableDataFloat(TEXT("r.DistanceFields.DefaultVoxelDensity"));
		const float VoxelDensity = CVarDensity->GetValueOnAnyThread();

		const float NumVoxelsPerLocalSpaceUnit = VoxelDensity * DistanceFieldResolutionScale;
		FBox MeshBounds(Bounds.GetBox());

		static const auto CVarEightBit = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DistanceFieldBuild.EightBit"));

		const bool bEightBitFixedPoint = CVarEightBit->GetValueOnAnyThread() != 0;
		const int32 FormatSize = GPixelFormats[bEightBitFixedPoint ? PF_G8 : PF_R16F].BlockBytes;

		{
			const float MaxOriginalExtent = MeshBounds.GetExtent().GetMax();
			// Expand so that the edges of the volume are guaranteed to be outside of the mesh
			// Any samples outside the bounds will be clamped to the border, so they must be outside
			const FVector NewExtent(MeshBounds.GetExtent() + FVector(.2f * MaxOriginalExtent).ComponentMax(4 * MeshBounds.GetExtent() / MinNumVoxelsOneDim));
			FBox DistanceFieldVolumeBounds = FBox(MeshBounds.GetCenter() - NewExtent, MeshBounds.GetCenter() + NewExtent);
			const float DistanceFieldVolumeMaxDistance = DistanceFieldVolumeBounds.GetExtent().Size();

			const FVector DesiredDimensions(DistanceFieldVolumeBounds.GetSize() * FVector(NumVoxelsPerLocalSpaceUnit));

			const FIntVector VolumeDimensions(
				FMath::Clamp(FMath::TruncToInt(DesiredDimensions.X), MinNumVoxelsOneDim, MaxNumVoxelsOneDim),
				FMath::Clamp(FMath::TruncToInt(DesiredDimensions.Y), MinNumVoxelsOneDim, MaxNumVoxelsOneDim),
				FMath::Clamp(FMath::TruncToInt(DesiredDimensions.Z), MinNumVoxelsOneDim, MaxNumVoxelsOneDim));

			TArray<float> DistanceFieldVolume;
			DistanceFieldVolume.Empty(VolumeDimensions.X * VolumeDimensions.Y * VolumeDimensions.Z);
			DistanceFieldVolume.AddZeroed(VolumeDimensions.X * VolumeDimensions.Y * VolumeDimensions.Z);

			TIndirectArray<FAsyncTask<FMeshDistanceFieldAsyncTask>> AsyncTasks;

			for (int32 ZIndex = 0; ZIndex < VolumeDimensions.Z; ZIndex++)
			{
				FAsyncTask<FMeshDistanceFieldAsyncTask>* Task = new FAsyncTask<class FMeshDistanceFieldAsyncTask>(
					&kDopTree,
					bUseEmbree,
					EmbreeScene,
					&SampleDirections,
					DistanceFieldVolumeBounds,
					VolumeDimensions,
					DistanceFieldVolumeMaxDistance,
					ZIndex,
					&DistanceFieldVolume);

				//Task->StartSynchronousTask();
				Task->StartBackgroundTask(&ThreadPool);

				AsyncTasks.Add(Task);
			}

			bool bNegativeAtBorder = false;

			for (int32 TaskIndex = 0; TaskIndex < AsyncTasks.Num(); TaskIndex++)
			{
				FAsyncTask<FMeshDistanceFieldAsyncTask>& Task = AsyncTasks[TaskIndex];
				Task.EnsureCompletion(false);
				bNegativeAtBorder = bNegativeAtBorder || Task.GetTask().WasNegativeAtBorder();
			}

			TArray<uint8> QuantizedDistanceFieldVolume;
			QuantizedDistanceFieldVolume.Empty(VolumeDimensions.X * VolumeDimensions.Y * VolumeDimensions.Z * FormatSize);
			QuantizedDistanceFieldVolume.AddZeroed(VolumeDimensions.X * VolumeDimensions.Y * VolumeDimensions.Z * FormatSize);

			float MinVolumeDistance = 1.0f;
			float MaxVolumeDistance = -1.0f;

			for (int32 Index = 0; Index < DistanceFieldVolume.Num(); Index++)
			{
				const float VolumeSpaceDistance = DistanceFieldVolume[Index];
				MinVolumeDistance = FMath::Min(MinVolumeDistance, VolumeSpaceDistance);
				MaxVolumeDistance = FMath::Max(MaxVolumeDistance, VolumeSpaceDistance);
			}

			MinVolumeDistance = FMath::Max(MinVolumeDistance, -1.0f);
			MaxVolumeDistance = FMath::Min(MaxVolumeDistance, 1.0f);

			const float InvDistanceRange = 1.0f / (MaxVolumeDistance - MinVolumeDistance);

			for (int32 Index = 0; Index < DistanceFieldVolume.Num(); Index++)
			{
				const float VolumeSpaceDistance = DistanceFieldVolume[Index];

				if (bEightBitFixedPoint)
				{
					check(FormatSize == sizeof(uint8));
					// [MinVolumeDistance, MaxVolumeDistance] -> [0, 1]
					const float RescaledDistance = (VolumeSpaceDistance - MinVolumeDistance) * InvDistanceRange;
					// Encoding based on D3D format conversion rules for float -> UNORM
					const int32 QuantizedDistance = FMath::FloorToInt(RescaledDistance * 255.0f + .5f);
					QuantizedDistanceFieldVolume[Index * FormatSize] = (uint8)FMath::Clamp<int32>(QuantizedDistance, 0, 255);
				}
				else
				{
					check(FormatSize == sizeof(FFloat16));
					FFloat16* OutputPointer = (FFloat16*)&(QuantizedDistanceFieldVolume[Index * FormatSize]);
					*OutputPointer = FFloat16(VolumeSpaceDistance);
				}
			}

			DistanceFieldVolume.Empty();

			OutData.bMeshWasClosed = !bNegativeAtBorder;
			OutData.bBuiltAsIfTwoSided = bGenerateAsIfTwoSided;
			OutData.bMeshWasPlane = bMeshWasPlane;
			OutData.Size = VolumeDimensions;
			OutData.LocalBoundingBox = DistanceFieldVolumeBounds;
			OutData.DistanceMinMax = FVector2D(MinVolumeDistance, MaxVolumeDistance);

			// Toss distance field if mesh was not closed
			if (bNegativeAtBorder)
			{
				OutData.Size = FIntVector(0, 0, 0);
				QuantizedDistanceFieldVolume.Empty();

				UE_LOG(LogMeshUtilities, Log, TEXT("Discarded distance field for %s as mesh was not closed!  Assign a two-sided material to fix."), *MeshName);
			}

			if (QuantizedDistanceFieldVolume.Num() > 0)
			{
				static const auto CVarCompress = IConsoleManager::Get().FindTConsoleVariableDataInt(TEXT("r.DistanceFieldBuild.Compress"));
				const bool bCompress = CVarCompress->GetValueOnAnyThread() != 0;

				if (bCompress)
				{
					const int32 UncompressedSize = QuantizedDistanceFieldVolume.Num() * QuantizedDistanceFieldVolume.GetTypeSize();
					TArray<uint8> TempCompressedMemory;
					// Compressed can be slightly larger than uncompressed
					TempCompressedMemory.Empty(UncompressedSize * 4 / 3);
					TempCompressedMemory.AddUninitialized(UncompressedSize * 4 / 3);
					int32 CompressedSize = TempCompressedMemory.Num() * TempCompressedMemory.GetTypeSize();

					verify(FCompression::CompressMemory(
						(ECompressionFlags)(COMPRESS_ZLIB | COMPRESS_BiasMemory), 
						TempCompressedMemory.GetData(), 
						CompressedSize, 
						QuantizedDistanceFieldVolume.GetData(), 
						UncompressedSize));

					OutData.CompressedDistanceFieldVolume.Empty(CompressedSize);
					OutData.CompressedDistanceFieldVolume.AddUninitialized(CompressedSize);

					FPlatformMemory::Memcpy(OutData.CompressedDistanceFieldVolume.GetData(), TempCompressedMemory.GetData(), CompressedSize);
				}
				else
				{
					int32 CompressedSize = QuantizedDistanceFieldVolume.Num() * QuantizedDistanceFieldVolume.GetTypeSize();

					OutData.CompressedDistanceFieldVolume.Empty(CompressedSize);
					OutData.CompressedDistanceFieldVolume.AddUninitialized(CompressedSize);

					FPlatformMemory::Memcpy(OutData.CompressedDistanceFieldVolume.GetData(), QuantizedDistanceFieldVolume.GetData(), CompressedSize);
				}
			}
			else
			{
				OutData.CompressedDistanceFieldVolume.Empty();
			}

			UE_LOG(LogMeshUtilities, Log, TEXT("Finished distance field build in %.1fs - %ux%ux%u distance field, %u triangles, Range [%.1f, %.1f], %s"),
				(float)(FPlatformTime::Seconds() - StartTime),
				VolumeDimensions.X,
				VolumeDimensions.Y,
				VolumeDimensions.Z,
				Indices.Num() / 3,
				MinVolumeDistance,
				MaxVolumeDistance,
				*MeshName);
		}

#if USE_EMBREE
		if (bUseEmbree)
		{
			rtcDeleteScene(EmbreeScene);
			rtcDeleteDevice(EmbreeDevice);
		}
#endif
	}
}

#else

void FMeshUtilities::GenerateSignedDistanceFieldVolumeData(
	FString MeshName,
	const FStaticMeshLODResources& LODModel,
	class FQueuedThreadPool& ThreadPool,
	const TArray<EBlendMode>& MaterialBlendModes,
	const FBoxSphereBounds& Bounds,
	float DistanceFieldResolutionScale,
	bool bGenerateAsIfTwoSided,
	FDistanceFieldVolumeData& OutData)
{
	if (DistanceFieldResolutionScale > 0)
	{
		UE_LOG(LogMeshUtilities, Error, TEXT("Couldn't generate distance field for mesh, platform is missing required Vector intrinsics."));
	}
}

#endif // PLATFORM_ENABLE_VECTORINTRINSICS
