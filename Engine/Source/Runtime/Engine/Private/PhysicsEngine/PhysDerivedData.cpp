// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#include "PhysicsEngine/PhysDerivedData.h"
#include "Serialization/MemoryWriter.h"
#include "Modules/ModuleManager.h"
#include "Stats/Stats.h"
#include "Interfaces/Interface_CollisionDataProvider.h"
#include "Components/SplineMeshComponent.h"
#include "PhysicsEngine/PhysicsSettings.h"
#include "PhysicsEngine/BodySetup.h"
#include "Interfaces/ITargetPlatformManagerModule.h"

#if WITH_PHYSX && WITH_EDITOR

#include "IPhysXCookingModule.h"

FDerivedDataPhysXCooker::FDerivedDataPhysXCooker(FName InFormat, EPhysXMeshCookFlags InRuntimeCookFlags, UBodySetup* InBodySetup, bool InIsRuntime)
	: BodySetup( InBodySetup )
	, CollisionDataProvider( NULL )
	, Format( InFormat )
	, RuntimeCookFlags(InRuntimeCookFlags)
	, Cooker( NULL )
	, bIsRuntime(InIsRuntime)
{
	check( BodySetup != NULL );
	CollisionDataProvider = BodySetup->GetOuter();
	DataGuid = BodySetup->BodySetupGuid;
	bGenerateNormalMesh = BodySetup->bGenerateNonMirroredCollision;
	bGenerateMirroredMesh = BodySetup->bGenerateMirroredCollision;
	bGenerateUVInfo = UPhysicsSettings::Get()->bSupportUVFromHitResults;
	BodyComplexity = (int32)BodySetup->GetCollisionTraceFlag();
	IInterface_CollisionDataProvider* CDP = Cast<IInterface_CollisionDataProvider>(CollisionDataProvider);
	if (CDP)
	{
		CDP->GetMeshId(MeshId);
	}
	InitCooker();

	bVerifyDDC = FParse::Param(FCommandLine::Get(), TEXT("VerifyDDC"));
}

void FDerivedDataPhysXCooker::InitCooker()
{
	// static here as an optimization
	static ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	if (TPM)
	{
		Cooker = TPM->FindPhysXCooking( Format );
	}
}

FString FDerivedDataPhysXCooker::GetDebugContextString() const
{
	if (BodySetup)
	{
		UObject* Outer = BodySetup->GetOuter();
		if (Outer)
		{
			return Outer->GetFullName();
		}
	}
	return TEXT("Unknown Context");
}

bool FDerivedDataPhysXCooker::Build( TArray<uint8>& OutData )
{
	SCOPE_CYCLE_COUNTER(STAT_PhysXCooking);

	check(Cooker != NULL);

	if(bIsRuntime && !bVerifyDDC)
	{
		return false;
	}

	FMemoryWriter Ar( OutData );
	uint8 bLittleEndian = PLATFORM_LITTLE_ENDIAN;	//TODO: We should pass the target platform into this function and write it. Then swap the endian on the writer so the reader doesn't have to do it at runtime
	int32 NumConvexElementsCooked = 0;
	int32 NumMirroredElementsCooked = 0;
	int32 NumTriMeshesCooked = 0;
	Ar << bLittleEndian;
	int64 CookedMeshInfoOffset = Ar.Tell();
	Ar << NumConvexElementsCooked;	
	Ar << NumMirroredElementsCooked;
	Ar << NumTriMeshesCooked;

	//TODO: we must save an id with convex and tri meshes for serialization. We must save this here and patch it up at runtime somehow

	bool bSuccess = true;

	FCookBodySetupInfo CookInfo;
	BodySetup->GetCookInfo(CookInfo, RuntimeCookFlags);

	if(CookInfo.bCookNonMirroredConvex)
	{
		bSuccess = BuildConvex(OutData, CookInfo.bConvexDeformableMesh, false, CookInfo.NonMirroredConvexVertices, CookInfo.ConvexCookFlags, NumConvexElementsCooked) && bSuccess;
	}

	if(CookInfo.bCookMirroredConvex)
	{
		bSuccess = BuildConvex(OutData, CookInfo.bConvexDeformableMesh, true, CookInfo.MirroredConvexVertices, CookInfo.ConvexCookFlags, NumMirroredElementsCooked) && bSuccess;
	}

	FBodySetupUVInfo UVInfo;
	if(CookInfo.bCookTriMesh)
	{
		if(!CookInfo.bTriMeshError)
		{
			bSuccess = BuildTriMesh(OutData, CookInfo.TriangleMeshDesc, CookInfo.TriMeshCookFlags, CookInfo.bSupportUVFromHitResults ? &UVInfo : nullptr, NumTriMeshesCooked) && bSuccess;
		}
		else
		{
			bSuccess = false;
		}
	}

	// Seek to end, serialize UV info
	Ar.Seek(OutData.Num());
	Ar << UVInfo;

	// Update info on what actually got cooked
	Ar.Seek( CookedMeshInfoOffset );
	Ar << NumConvexElementsCooked;	
	Ar << NumMirroredElementsCooked;
	Ar << NumTriMeshesCooked;

	return bSuccess;
}

bool FDerivedDataPhysXCooker::BuildConvex( TArray<uint8>& OutData, bool bDeformableMesh, bool InMirrored, const TArray<TArray<FVector>>& Elements, EPhysXMeshCookFlags CookFlags, int32& NumConvexCooked )
{	
	bool bSuccess = true;
	for( int32 ElementIndex = 0; ElementIndex < Elements.Num(); ElementIndex++ )
	{
		// Store info on the cooking result (1 byte)
		int32 ResultInfoOffset = OutData.Add(false);

		// Cook and store Result at ResultInfoOffset
		UE_LOG(LogPhysics, Log, TEXT("Cook Convex: %s %d (FlipX:%d)"), *BodySetup->GetOuter()->GetPathName(), ElementIndex, InMirrored);		
		const EPhysXCookingResult Result = Cooker->CookConvex(Format, CookFlags, Elements[ElementIndex], OutData);
		switch (Result)
		{
		case EPhysXCookingResult::Succeeded:
			break;
		case EPhysXCookingResult::Failed:
			UE_LOG(LogPhysics, Warning, TEXT("Failed to cook convex: %s %d (FlipX:%d). The remaining elements will not get cooked."), *BodySetup->GetOuter()->GetPathName(), ElementIndex, InMirrored ? 1 : 0);
			bSuccess = false;
			break;
		case EPhysXCookingResult::SucceededWithInflation:
			if (!bDeformableMesh)
			{
				bSuccess = false;
				UE_LOG(LogPhysics, Warning, TEXT("Cook convex: %s %d (FlipX:%d) failed but succeeded with inflation.  The mesh should be looked at."), *BodySetup->GetOuter()->GetPathName(), ElementIndex, InMirrored ? 1 : 0);
			}
			else
			{
				UE_LOG(LogPhysics, Log, TEXT("Cook convex: %s %d (FlipX:%d) required inflation. You may wish to adjust the mesh so this is not necessary."), *BodySetup->GetOuter()->GetPathName(), ElementIndex, InMirrored ? 1 : 0);
			}
			break;
		default:
			// Unknown/unsupported enum value
			bSuccess = false;
			check(false);
		}
		OutData[ ResultInfoOffset ] = (Result != EPhysXCookingResult::Failed) ? 1 : 0;
	}

	NumConvexCooked = Elements.Num();
	return bSuccess;
}

bool FDerivedDataPhysXCooker::ShouldGenerateTriMeshData(bool InUseAllTriData)
{
	check(Cooker != NULL);

	IInterface_CollisionDataProvider* CDP = Cast<IInterface_CollisionDataProvider>(CollisionDataProvider);
	const bool bPerformCook = ( CDP != NULL ) ? CDP->ContainsPhysicsTriMeshData(InUseAllTriData) : false;
	return bPerformCook;
}

bool FDerivedDataPhysXCooker::BuildTriMesh( TArray<uint8>& OutData,  const FTriMeshCollisionData& TriangleMeshDesc, EPhysXMeshCookFlags CookFlags, FBodySetupUVInfo* UVInfo, int32& NumTrimeshCooked)
{
	check(Cooker != NULL);

	bool bError = false;
	bool bResult = false;

	UE_LOG(LogPhysics, Log, TEXT("Cook TriMesh: %s"), *CollisionDataProvider->GetPathName());
	bResult = Cooker->CookTriMesh( Format, CookFlags, TriangleMeshDesc.Vertices, TriangleMeshDesc.Indices, TriangleMeshDesc.MaterialIndices, TriangleMeshDesc.bFlipNormals, OutData);
	if( !bResult )
	{
		bError = true;
		UE_LOG(LogPhysics, Warning, TEXT("Failed to cook TriMesh: %s."), *CollisionDataProvider->GetPathName());
	}

	// If we want UV info, copy that now
	if (bResult && UVInfo)
	{
		UVInfo->FillFromTriMesh(TriangleMeshDesc);
	}

	NumTrimeshCooked = bResult == true ? 1 : 0;	//the cooker only generates 1 or 0 trimeshes. We save an int because we support multiple trimeshes for welding and we might want to do this per static mesh in the future.
	return !bError;	//use error instead of bResult because we do not warn if the trimesh data is simply empty, and in that case we don't consider it to be a failure
}

FDerivedDataPhysXBinarySerializer::FDerivedDataPhysXBinarySerializer(FName InFormat, const TArray<FBodyInstance*>& InBodies, const TArray<class UBodySetup*>& InBodySetups, const TArray<class UPhysicalMaterial*>& InPhysicalMaterials, const FGuid& InGuid)
: Bodies(InBodies)
, BodySetups(InBodySetups)
, PhysicalMaterials(InPhysicalMaterials)
, Format(InFormat)
, DataGuid(InGuid)
{
	InitSerializer();
}

bool FDerivedDataPhysXBinarySerializer::Build(TArray<uint8>& OutData)
{
	FMemoryWriter Ar(OutData);
	
	//do not serialize anything before the physx data. This is important because physx requires specific alignment. For that to work the physx data must come first in the archive
	SerializeRigidActors(OutData);
	
	// Whatever got cached return true. We want to cache 'failure' too.
	return true;
}

void FDerivedDataPhysXBinarySerializer::SerializeRigidActors(TArray<uint8>& OutData)
{
	Serializer->SerializeActors(Format, Bodies, BodySetups, PhysicalMaterials, OutData);
}

void FDerivedDataPhysXBinarySerializer::InitSerializer()
{
	// static here as an optimization
	static ITargetPlatformManagerModule* TPM = GetTargetPlatformManager();
	if (TPM)
	{
		Serializer = TPM->FindPhysXCooking(Format);
	}
}


#endif	//WITH_PHYSX && WITH_EDITOR
