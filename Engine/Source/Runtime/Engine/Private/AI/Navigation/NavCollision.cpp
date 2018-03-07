// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AI/Navigation/NavCollision.h"
#include "Serialization/MemoryWriter.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Interfaces/Interface_CollisionDataProvider.h"
#include "Engine/StaticMesh.h"
#include "SceneManagement.h"
#include "AI/Navigation/NavAreas/NavArea.h"
#include "AI/NavigationModifier.h"
#include "AI/NavigationSystemHelpers.h"
#include "DerivedDataPluginInterface.h"
#include "DerivedDataCacheInterface.h"
#include "PhysicsEngine/BodySetup.h"
#include "ProfilingDebugging/CookStats.h"

#if ENABLE_COOK_STATS
namespace NavCollisionCookStats
{
	static FCookStats::FDDCResourceUsageStats UsageStats;
	static FCookStatsManager::FAutoRegisterCallback RegisterCookStats([](FCookStatsManager::AddStatFuncRef AddStat)
	{
		UsageStats.LogStats(AddStat, TEXT("NavCollision.Usage"), TEXT(""));
	});
}
#endif

static const FName NAVCOLLISION_FORMAT = TEXT("NavCollision_X");

class FNavCollisionDataReader
{
public:
	FNavCollisionConvex& TriMeshCollision;
	FNavCollisionConvex& ConvexCollision;
	TNavStatArray<int32>& ConvexShapeIndices;

	FNavCollisionDataReader(FByteBulkData& InBulkData, FNavCollisionConvex& InTriMeshCollision, FNavCollisionConvex& InConvexCollision, TNavStatArray<int32>& InShapeIndices)
		: TriMeshCollision(InTriMeshCollision)
		, ConvexCollision(InConvexCollision)
		, ConvexShapeIndices(InShapeIndices)
	{
		// Read cooked data
		uint8* DataPtr = (uint8*)InBulkData.Lock( LOCK_READ_ONLY );
		FBufferReader Ar( DataPtr, InBulkData.GetBulkDataSize(), false );

		uint8 bLittleEndian = true;

		Ar << bLittleEndian;
		Ar.SetByteSwapping( PLATFORM_LITTLE_ENDIAN ? !bLittleEndian : !!bLittleEndian );
		Ar << TriMeshCollision.VertexBuffer;
		Ar << TriMeshCollision.IndexBuffer;
		Ar << ConvexCollision.VertexBuffer;
		Ar << ConvexCollision.IndexBuffer;
		Ar << ConvexShapeIndices;

		InBulkData.Unlock();
	}
};

//----------------------------------------------------------------------//
// FDerivedDataNavCollisionCooker
//----------------------------------------------------------------------//
class FDerivedDataNavCollisionCooker : public FDerivedDataPluginInterface
{
private:
	UNavCollision* NavCollisionInstance;
	UObject* CollisionDataProvider;
	FName Format;
	FGuid DataGuid;
	FString MeshId;

public:
	FDerivedDataNavCollisionCooker(FName InFormat, UNavCollision* InInstance);

	virtual const TCHAR* GetPluginName() const override
	{
		return TEXT("NavCollision");
	}

	virtual const TCHAR* GetVersionString() const override
	{
		return TEXT("B89838347A4348138EE337A847529C5C");
	}

	virtual FString GetPluginSpecificCacheKeySuffix() const override
	{
		const uint16 Version = 13;

		return FString::Printf( TEXT("%s_%s_%s_%hu")
			, *Format.ToString()
			, *DataGuid.ToString()
			, *MeshId
			, Version
			);
	}

	virtual bool IsBuildThreadsafe() const override
	{
		return false;
	}

	virtual bool Build( TArray<uint8>& OutData ) override;

	/** Return true if we can build **/
	bool CanBuild()
	{
		return true;
	}
};

FDerivedDataNavCollisionCooker::FDerivedDataNavCollisionCooker(FName InFormat, UNavCollision* InInstance)
	: NavCollisionInstance(InInstance)
	, CollisionDataProvider( NULL )
	, Format( InFormat )
{
	check(NavCollisionInstance != NULL);
	CollisionDataProvider = NavCollisionInstance->GetOuter();
	DataGuid = NavCollisionInstance->GetGuid();
	IInterface_CollisionDataProvider* CDP = Cast<IInterface_CollisionDataProvider>(CollisionDataProvider);
	if (CDP)
	{
		CDP->GetMeshId(MeshId);
	}
}

bool FDerivedDataNavCollisionCooker::Build( TArray<uint8>& OutData )
{
	if ((NavCollisionInstance->ConvexShapeIndices.Num() == 0) ||
		(NavCollisionInstance->TriMeshCollision.VertexBuffer.Num() == 0 && NavCollisionInstance->ConvexCollision.VertexBuffer.Num() == 0))
	{
		NavCollisionInstance->GatherCollision();
	}

	FMemoryWriter Ar( OutData );
	uint8 bLittleEndian = PLATFORM_LITTLE_ENDIAN;
	Ar << bLittleEndian;
	int64 CookedMeshInfoOffset = Ar.Tell();

	Ar << NavCollisionInstance->TriMeshCollision.VertexBuffer;
	Ar << NavCollisionInstance->TriMeshCollision.IndexBuffer;
	Ar << NavCollisionInstance->ConvexCollision.VertexBuffer;
	Ar << NavCollisionInstance->ConvexCollision.IndexBuffer;
	Ar << NavCollisionInstance->ConvexShapeIndices;

	// Whatever got cached return true. We want to cache 'failure' too.
	return true;
}

//----------------------------------------------------------------------//
// UNavCollision
//----------------------------------------------------------------------//
UNavCollision::UNavCollision(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{	
	bHasConvexGeometry = false;
	bForceGeometryRebuild = false;
}

FGuid UNavCollision::GetGuid() const
{
	return BodySetupGuid;
}

void UNavCollision::Setup(UBodySetup* BodySetup)
{
	// Create meshes from cooked data if not already done
	if (bHasConvexGeometry || BodySetup == NULL || BodySetupGuid == BodySetup->BodySetupGuid)
	{
		return;
	}

	BodySetupGuid = BodySetup->BodySetupGuid;

	// Make sure all are cleared before we start
	ClearCollision(); 
		
	if (ShouldUseConvexCollision())
	{
		// Find or create cooked navcollision data
		FByteBulkData* FormatData = GetCookedData(NAVCOLLISION_FORMAT);
		if (!bForceGeometryRebuild && FormatData)
		{
			// if it's not being already processed
			if (FormatData->IsLocked() == false)
			{
				// Create physics objects
				FNavCollisionDataReader CookedDataReader(*FormatData, TriMeshCollision, ConvexCollision, ConvexShapeIndices);

				bHasConvexGeometry = true;
			}
		}
		else if (FPlatformProperties::RequiresCookedData() == false)
		{
			GatherCollision();
		}
	}
}

void UNavCollision::GatherCollision()
{
	UStaticMesh* StaticMeshOuter = Cast<UStaticMesh>(GetOuter());
	// get data from owner
	if (StaticMeshOuter && StaticMeshOuter->BodySetup)
	{
		ClearCollision();
		NavigationHelper::GatherCollision(StaticMeshOuter->BodySetup, this);
		bHasConvexGeometry = true;
	}
}

void UNavCollision::ClearCollision()
{
	TriMeshCollision.VertexBuffer.Reset();
	TriMeshCollision.IndexBuffer.Reset();
	ConvexCollision.VertexBuffer.Reset();
	ConvexCollision.IndexBuffer.Reset();
	ConvexShapeIndices.Reset();

	bHasConvexGeometry = false;
}

void UNavCollision::GetNavigationModifier(FCompositeNavModifier& Modifier, const FTransform& LocalToWorld)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_NavCollision_GetNavigationModifier);

	const TSubclassOf<UNavArea> UseAreaClass = AreaClass ? AreaClass : UNavigationSystem::GetDefaultObstacleArea();

	Modifier.ReserveForAdditionalAreas(CylinderCollision.Num() + BoxCollision.Num()
		+ (ConvexCollision.VertexBuffer.Num() > 0 ? ConvexShapeIndices.Num() : 0));

	for (int32 i = 0; i < CylinderCollision.Num(); i++)
	{
		FTransform CylinderToWorld = LocalToWorld;
		const FVector Origin = CylinderToWorld.TransformPosition(CylinderCollision[i].Offset);
		CylinderToWorld.SetTranslation(Origin);

		FAreaNavModifier AreaMod(CylinderCollision[i].Radius, CylinderCollision[i].Height, CylinderToWorld, UseAreaClass);
		AreaMod.SetIncludeAgentHeight(true);
		Modifier.Add(AreaMod);
	}

	for (int32 i = 0; i < BoxCollision.Num(); i++)
	{
		FTransform BoxToWorld = LocalToWorld;
		const FVector Origin = BoxToWorld.TransformPosition(BoxCollision[i].Offset);
		BoxToWorld.SetTranslation(Origin);

		FAreaNavModifier AreaMod(BoxCollision[i].Extent, BoxToWorld, UseAreaClass);
		AreaMod.SetIncludeAgentHeight(true);
		Modifier.Add(AreaMod);
	}

	if (ShouldUseConvexCollision())
	{
		// rebuild collision data if needed
		if (!bHasConvexGeometry)
		{
			GatherCollision();
		}

		if (ConvexCollision.VertexBuffer.Num() > 0)
		{
			int32 LastVertIndex = 0;

			for (int32 i = 0; i < ConvexShapeIndices.Num(); i++)
			{
				int32 FirstVertIndex = LastVertIndex;
				LastVertIndex = ConvexShapeIndices.IsValidIndex(i + 1) ? ConvexShapeIndices[i + 1] : ConvexCollision.VertexBuffer.Num();

				FAreaNavModifier AreaMod(ConvexCollision.VertexBuffer, FirstVertIndex, LastVertIndex, ENavigationCoordSystem::Unreal, LocalToWorld, UseAreaClass);
				AreaMod.SetIncludeAgentHeight(true);
				Modifier.Add(AreaMod);
			}
		}

		if (TriMeshCollision.VertexBuffer.Num() > 0)
		{
			FAreaNavModifier AreaMod(TriMeshCollision.VertexBuffer, 0, TriMeshCollision.VertexBuffer.Num() - 1, ENavigationCoordSystem::Unreal, LocalToWorld, UseAreaClass);
			AreaMod.SetIncludeAgentHeight(true);
			Modifier.Add(AreaMod);
		}
	}
}

void DrawCylinderHelper(FPrimitiveDrawInterface* PDI, const FMatrix& ElemTM, const float Radius, const float Height, const FColor Color)
{
	const float	AngleDelta = 2.0f * PI / 16;
	FVector X, Y, Z;

	ElemTM.GetUnitAxes(X, Y, Z);
	FVector	LastVertex = ElemTM.GetOrigin() + X * Radius;

	for(int32 SideIndex = 0;SideIndex < 16;SideIndex++)
	{
		const FVector Vertex = ElemTM.GetOrigin() + (X * FMath::Cos(AngleDelta * (SideIndex + 1)) + Y * FMath::Sin(AngleDelta * (SideIndex + 1))) * Radius;

		PDI->DrawLine(LastVertex,Vertex,Color,SDPG_World);
		PDI->DrawLine(LastVertex + Z * Height,Vertex + Z * Height,Color,SDPG_World);
		PDI->DrawLine(LastVertex,LastVertex + Z * Height,Color,SDPG_World);

		LastVertex = Vertex;
	}
}

void DrawBoxHelper(FPrimitiveDrawInterface* PDI, const FMatrix& ElemTM, const FVector& Extent, const FColor Color)
{
	FVector	B[2], P, Q;

	B[0] = Extent; // max
	B[1] = -1.0f * Extent; // min

	for( int32 i=0; i<2; i++ )
	{
		for( int32 j=0; j<2; j++ )
		{
			P.X=B[i].X; Q.X=B[i].X;
			P.Y=B[j].Y; Q.Y=B[j].Y;
			P.Z=B[0].Z; Q.Z=B[1].Z;
			PDI->DrawLine( ElemTM.TransformPosition(P), ElemTM.TransformPosition(Q), Color, SDPG_World);
			P.Y=B[i].Y; Q.Y=B[i].Y;
			P.Z=B[j].Z; Q.Z=B[j].Z;
			P.X=B[0].X; Q.X=B[1].X;
			PDI->DrawLine( ElemTM.TransformPosition(P), ElemTM.TransformPosition(Q), Color, SDPG_World);
			P.Z=B[i].Z; Q.Z=B[i].Z;
			P.X=B[j].X; Q.X=B[j].X;
			P.Y=B[0].Y; Q.Y=B[1].Y;
			PDI->DrawLine( ElemTM.TransformPosition(P), ElemTM.TransformPosition(Q), Color, SDPG_World);
		}
	}
}

void UNavCollision::DrawSimpleGeom(FPrimitiveDrawInterface* PDI, const FTransform& Transform, const FColor Color)
{
	const FMatrix ParentTM = Transform.ToMatrixWithScale();
	for (int32 i = 0; i < CylinderCollision.Num(); i++)
	{
		FMatrix ElemTM = FTranslationMatrix(CylinderCollision[i].Offset);
		ElemTM *= ParentTM;
		DrawCylinderHelper(PDI, ElemTM, CylinderCollision[i].Radius, CylinderCollision[i].Height, Color);
	}
	
	for (int32 i = 0; i < BoxCollision.Num(); i++)
	{
		FMatrix ElemTM = FTranslationMatrix(BoxCollision[i].Offset);
		ElemTM *= ParentTM;
		DrawBoxHelper(PDI, ElemTM, BoxCollision[i].Extent, Color);
	}
}


#if WITH_EDITOR
void UNavCollision::InvalidatePhysicsData()
{
	ClearCollision();
	CookedFormatData.FlushData();
}
#endif // WITH_EDITOR

void UNavCollision::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	const int32 VerInitial = 1;
	const int32 VerAreaClass = 2;
	const int32 VerConvexTransforms = 3;
	const int32 VerLatest = VerConvexTransforms;

	// use magic number to determine if serialized stream has version :/
	const int32 MagicNum = 0xA237F237;
	int64 StreamStartPos = Ar.Tell();

	int32 Version = VerLatest;
	int32 MyMagicNum = MagicNum;
	Ar << MyMagicNum;

	if (MyMagicNum != MagicNum)
	{
		Version = VerInitial;
		Ar.Seek(StreamStartPos);
	}
	else
	{
		Ar << Version;
	}

	// loading a dummy GUID to have serialization not break on 
	// packages serialized before switching over UNavCollision to
	// use BodySetup's guid rather than its own one
	// motivation: not creating a new engine version
	// @NOTE could be addressed during next engine version bump
	FGuid Guid;
	Ar << Guid;
	
	bool bCooked = Ar.IsCooking();
	Ar << bCooked;

	if (FPlatformProperties::RequiresCookedData() && !bCooked && Ar.IsLoading())
	{
		UE_LOG(LogNavigation, Fatal, TEXT("This platform requires cooked packages, and NavCollision data was not cooked into %s."), *GetFullName());
	}

	if (bCooked && ShouldUseConvexCollision())
	{
		if (Ar.IsCooking())
		{
			FName Format = NAVCOLLISION_FORMAT;
			GetCookedData(Format); // Get the data from the DDC or build it

			TArray<FName> ActualFormatsToSave;
			ActualFormatsToSave.Add(Format);
			CookedFormatData.Serialize(Ar, this, &ActualFormatsToSave);
		}
		else
		{
			CookedFormatData.Serialize(Ar, this);
		}
	}

	if (Version >= VerAreaClass)
	{
		Ar << AreaClass;
	}

	if (Version < VerConvexTransforms && Ar.IsLoading() && GIsEditor)
	{
		bForceGeometryRebuild = true;
	}
}

void UNavCollision::PostLoad()
{
	Super::PostLoad();

	// Our owner needs to be post-loaded before us else they may not have loaded
	// their data yet.
	UObject* Outer = GetOuter();
	if (Outer)
	{
		Outer->ConditionalPostLoad();

		UStaticMesh* StaticMeshOuter = Cast<UStaticMesh>(Outer);
		if (StaticMeshOuter != NULL)
		{
			Setup(StaticMeshOuter->BodySetup);
		}
	}
}

FByteBulkData* UNavCollision::GetCookedData(FName Format)
{
	if (IsTemplate())
	{
		return NULL;
	}
	
	bool bContainedData = CookedFormatData.Contains(Format);
	FByteBulkData* Result = &CookedFormatData.GetFormat(Format);

	if (!bContainedData)
	{
		if (FPlatformProperties::RequiresCookedData())
		{
			UE_LOG(LogNavigation, Error, TEXT("Attempt to build nav collision data for %s when we are unable to. This platform requires cooked packages."), *GetPathName());
			return nullptr;
		}
		
		TArray<uint8> OutData;
		FDerivedDataNavCollisionCooker* DerivedNavCollisionData = new FDerivedDataNavCollisionCooker(Format, this);
		if (DerivedNavCollisionData->CanBuild())
		{
			bool bDataWasBuilt = false;
			COOK_STAT(auto Timer = NavCollisionCookStats::UsageStats.TimeSyncWork());
			if (GetDerivedDataCacheRef().GetSynchronous(DerivedNavCollisionData, OutData, &bDataWasBuilt))
			{
				COOK_STAT(Timer.AddHitOrMiss(bDataWasBuilt ? FCookStats::CallStats::EHitOrMiss::Miss : FCookStats::CallStats::EHitOrMiss::Hit, OutData.Num()));
				if (OutData.Num())
				{
					Result->Lock(LOCK_READ_WRITE);
					FMemory::Memcpy(Result->Realloc(OutData.Num()), OutData.GetData(), OutData.Num());
					Result->Unlock();
				}
			}
		}
	}

	check(Result);
	return Result->GetBulkDataSize() > 0 ? Result : NULL; // we don't return empty bulk data...but we save it to avoid thrashing the DDC
}

void UNavCollision::GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize)
{
	Super::GetResourceSizeEx(CumulativeResourceSize);
	
	if (CookedFormatData.Contains(NAVCOLLISION_FORMAT))
	{
		const FByteBulkData& FmtData = CookedFormatData.GetFormat(NAVCOLLISION_FORMAT);
		CumulativeResourceSize.AddDedicatedSystemMemoryBytes(FmtData.GetElementSize() * FmtData.GetElementCount());
	}
}

void UNavCollision::CopyUserSettings(const UNavCollision& OtherData)
{
	CylinderCollision = OtherData.CylinderCollision;
	BoxCollision = OtherData.BoxCollision;
	AreaClass = OtherData.AreaClass;
	bIsDynamicObstacle = OtherData.bIsDynamicObstacle;
	bGatherConvexGeometry = OtherData.bGatherConvexGeometry;
}