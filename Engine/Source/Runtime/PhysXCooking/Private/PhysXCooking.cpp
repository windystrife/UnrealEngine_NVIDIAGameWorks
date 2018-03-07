// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PhysXCooking.h"
#include "Serialization/MemoryWriter.h"
#include "Modules/ModuleManager.h"
#include "Interfaces/Interface_CollisionDataProvider.h"
#include "IPhysXCookingModule.h"
#include "PhysicsPublic.h"
#include "PhysicsEngine/PhysXSupport.h"

static_assert(WITH_PHYSX, "No point in compiling PhysX cooker, if we don't have PhysX.");

static FName NAME_PhysXGeneric(TEXT("PhysXGeneric"));
static FName NAME_PhysXPC(TEXT("PhysXPC"));

bool GetPhysXCooking(FName InFormatName, PxPlatform::Enum& OutFormat)
{
	if ((InFormatName == NAME_PhysXPC) || (InFormatName == NAME_PhysXGeneric))
	{
		OutFormat = PxPlatform::ePC;
	}
	else
	{
		return false;
	}
	return true;
}

/**
* Validates PhysX format name.
*/
bool CheckPhysXCooking(FName InFormatName)
{
	PxPlatform::Enum Format = PxPlatform::ePC;
	return GetPhysXCooking(InFormatName, Format);
}

void UseBVH34IfSupported(FName Format, PxCookingParams& Params)
{
	if((Format == NAME_PhysXPC))
	{
		//TODO: can turn this on once bug is fixed with character movement
		//Params.midphaseDesc = PxMeshMidPhase::eBVH34;
	}
}

FPhysXCooking::FPhysXCooking()
{
#if WITH_PHYSX
	PxTolerancesScale PScale;
	PScale.length = CVarToleranceScaleLength.GetValueOnAnyThread();
	PScale.speed = CVarToleranceScaleSpeed.GetValueOnAnyThread();

	PxCookingParams PCookingParams(PScale);
	PCookingParams.meshWeldTolerance = 0.1f; // Weld to 1mm precision
	PCookingParams.meshPreprocessParams = PxMeshPreprocessingFlags(PxMeshPreprocessingFlag::eWELD_VERTICES);
	// Force any cooking in PhysX or APEX to use older incremental hull method
	// This is because the new 'quick hull' method can generate degenerate geometry in some cases (very thin meshes etc.)
	//PCookingParams.convexMeshCookingType = PxConvexMeshCookingType::eINFLATION_INCREMENTAL_HULL;
	PCookingParams.targetPlatform = PxPlatform::ePC;
	PCookingParams.midphaseDesc = PxMeshMidPhase::eBVH33;
	//PCookingParams.meshCookingHint = PxMeshCookingHint::eCOOKING_PERFORMANCE;
	//PCookingParams.meshSizePerformanceTradeOff = 0.0f;

	PhysXCooking = PxCreateCooking(PX_PHYSICS_VERSION, *GPhysXFoundation, PCookingParams);
#endif
}

physx::PxCooking* FPhysXCooking::GetCooking() const
{
	return PhysXCooking;
}

bool FPhysXCooking::AllowParallelBuild() const
{
	return true;
}

uint16 FPhysXCooking::GetVersion(FName Format) const
{
	check(CheckPhysXCooking(Format));
	return UE_PHYSX_PC_VER;
}


void FPhysXCooking::GetSupportedFormats(TArray<FName>& OutFormats) const
{
	OutFormats.Add(NAME_PhysXPC);
	OutFormats.Add(NAME_PhysXGeneric);
}

/** Utility wrapper for a uint8 TArray for saving into PhysX. */
class FPhysXOutputStream : public PxOutputStream
{
public:
	/** Raw byte data */
	TArray<uint8>			*Data;

	FPhysXOutputStream()
		: Data(NULL)
	{}

	FPhysXOutputStream(TArray<uint8> *InData)
		: Data(InData)
	{}

	PxU32 write(const void* Src, PxU32 Count)
	{
		check(Data);
		if (Count)	//PhysX serializer can pass us 0 bytes to write
		{
			check(Src);
			int32 CurrentNum = (*Data).Num();
			(*Data).AddUninitialized(Count);
			FMemory::Memcpy(&(*Data)[CurrentNum], Src, Count);
		}

		return Count;
	}
};

template <bool bUseBuffer>
EPhysXCookingResult FPhysXCooking::CookConvexImp(FName Format, EPhysXMeshCookFlags CookFlags, const TArray<FVector>& SrcBuffer, TArray<uint8>& OutBuffer, PxConvexMesh*& OutConvexMesh) const
{
	EPhysXCookingResult CookResult = EPhysXCookingResult::Failed;
	OutConvexMesh = nullptr;

#if WITH_PHYSX
	PxPlatform::Enum PhysXFormat = PxPlatform::ePC;
	bool bIsPhysXCookingValid = GetPhysXCooking(Format, PhysXFormat);
	check(bIsPhysXCookingValid);

	PxConvexMeshDesc PConvexMeshDesc;
	PConvexMeshDesc.points.data = SrcBuffer.GetData();
	PConvexMeshDesc.points.count = SrcBuffer.Num();
	PConvexMeshDesc.points.stride = sizeof(FVector);
	PConvexMeshDesc.flags = PxConvexFlag::eCOMPUTE_CONVEX | PxConvexFlag::eSHIFT_VERTICES;

	// Set up cooking
	const PxCookingParams CurrentParams = PhysXCooking->getParams();
	PxCookingParams NewParams = CurrentParams;
	NewParams.targetPlatform = PhysXFormat;

	if (!!(CookFlags & EPhysXMeshCookFlags::SuppressFaceRemapTable))
	{
		NewParams.suppressTriangleMeshRemapTable = true;
	}

	if (!!(CookFlags & EPhysXMeshCookFlags::DeformableMesh))
	{
		// Meshes which can be deformed need different cooking parameters to inhibit vertex welding and add an extra skin around the collision mesh for safety.
		// We need to set the meshWeldTolerance to zero, even when disabling 'clean mesh' as PhysX will attempt to perform mesh cleaning anyway according to this meshWeldTolerance
		// if the convex hull is not well formed.
		// Set the skin thickness as a proportion of the overall size of the mesh as PhysX's internal tolerances also use the overall size to calculate the epsilon used.
		const FBox Bounds(SrcBuffer);
		const float MaxExtent = (Bounds.Max - Bounds.Min).Size();

		NewParams.meshPreprocessParams = PxMeshPreprocessingFlags(PxMeshPreprocessingFlag::eDISABLE_CLEAN_MESH);
		NewParams.meshWeldTolerance = 0.0f;
	}
	else
	{
		//For meshes that don't deform we can try to use BVH34
		UseBVH34IfSupported(Format, NewParams);
	}

	// Do we want to do a 'fast' cook on this mesh, may slow down collision performance at runtime
	if (!!(CookFlags & EPhysXMeshCookFlags::FastCook))
	{
		NewParams.meshCookingHint = PxMeshCookingHint::eCOOKING_PERFORMANCE;
	}

	PhysXCooking->setParams(NewParams);

	if (bUseBuffer)
	{
		// Cook the convex mesh to a temp buffer
		TArray<uint8> CookedMeshBuffer;
		FPhysXOutputStream Buffer(&CookedMeshBuffer);
		if (PhysXCooking->cookConvexMesh(PConvexMeshDesc, Buffer))
		{
			CookResult = EPhysXCookingResult::Succeeded;
		}
		else
		{
			if (!(PConvexMeshDesc.flags & PxConvexFlag::eINFLATE_CONVEX))
			{
				// We failed to cook without inflating convex. Let's try again with inflation
				//This is not ideal since it makes the collision less accurate. It's needed if given verts are extremely close.
				PConvexMeshDesc.flags |= PxConvexFlag::eINFLATE_CONVEX;
				if (PhysXCooking->cookConvexMesh(PConvexMeshDesc, Buffer))
				{
					CookResult = EPhysXCookingResult::SucceededWithInflation;
				}
			}
		}

		if (CookedMeshBuffer.Num() == 0)
		{
			CookResult = EPhysXCookingResult::Failed;
		}

		if (CookResult != EPhysXCookingResult::Failed)
		{
			// Append the cooked data into cooked buffer
			OutBuffer.Append(CookedMeshBuffer);
		}
	}
	else
	{
		OutConvexMesh = PhysXCooking->createConvexMesh(PConvexMeshDesc, GPhysXSDK->getPhysicsInsertionCallback()); //NOTE: getPhysicsInsertionCallback probably not thread safe!
		if (OutConvexMesh)
		{
			CookResult = EPhysXCookingResult::Succeeded;
		}
		else
		{
			if (!(PConvexMeshDesc.flags & PxConvexFlag::eINFLATE_CONVEX))
			{
				// We failed to cook without inflating convex. Let's try again with inflation
				//This is not ideal since it makes the collision less accurate. It's needed if given verts are extremely close.
				PConvexMeshDesc.flags |= PxConvexFlag::eINFLATE_CONVEX;
				OutConvexMesh = PhysXCooking->createConvexMesh(PConvexMeshDesc, GPhysXSDK->getPhysicsInsertionCallback()); //NOTE: getPhysicsInsertionCallback probably not thread safe!
				if (OutConvexMesh)
				{
					CookResult = EPhysXCookingResult::SucceededWithInflation;
				}
			}
		}

		if (!OutConvexMesh)
		{
			CookResult = EPhysXCookingResult::Failed;
		}
	}


	// Return default cooking params to normal
	PhysXCooking->setParams(CurrentParams);
#endif		// WITH_PHYSX

	return CookResult;
}

EPhysXCookingResult FPhysXCooking::CookConvex(FName Format, EPhysXMeshCookFlags CookFlags, const TArray<FVector>& SrcBuffer, TArray<uint8>& OutBuffer) const
{
	PxConvexMesh* JunkConvexMesh;
	return CookConvexImp</*bUseBuffer=*/true>(Format, CookFlags, SrcBuffer, OutBuffer, JunkConvexMesh);
}

EPhysXCookingResult FPhysXCooking::CreateConvex(FName Format, EPhysXMeshCookFlags CookFlags, const TArray<FVector>& SrcBuffer, physx::PxConvexMesh*& OutConvexMesh) const
{
	TArray<uint8> JunkBuffer;
	return CookConvexImp</*bUseBuffer=*/false>(Format, CookFlags, SrcBuffer, JunkBuffer, OutConvexMesh);
}

template<bool bUseBuffer>
bool FPhysXCooking::CookTriMeshImp(FName Format, EPhysXMeshCookFlags CookFlags, const TArray<FVector>& SrcVertices, const TArray<FTriIndices>& SrcIndices, const TArray<uint16>& SrcMaterialIndices, const bool FlipNormals, TArray<uint8>& OutBuffer, PxTriangleMesh*& OutTriangleMesh) const
{
	OutTriangleMesh = nullptr;
#if WITH_PHYSX
	PxPlatform::Enum PhysXFormat = PxPlatform::ePC;
	bool bIsPhysXCookingValid = GetPhysXCooking(Format, PhysXFormat);
	check(bIsPhysXCookingValid);

	PxTriangleMeshDesc PTriMeshDesc;
	PTriMeshDesc.points.data = SrcVertices.GetData();
	PTriMeshDesc.points.count = SrcVertices.Num();
	PTriMeshDesc.points.stride = sizeof(FVector);
	PTriMeshDesc.triangles.data = SrcIndices.GetData();
	PTriMeshDesc.triangles.count = SrcIndices.Num();
	PTriMeshDesc.triangles.stride = sizeof(FTriIndices);
	PTriMeshDesc.materialIndices.data = SrcMaterialIndices.GetData();
	PTriMeshDesc.materialIndices.stride = sizeof(PxMaterialTableIndex);
	PTriMeshDesc.flags = FlipNormals ? PxMeshFlag::eFLIPNORMALS : (PxMeshFlags)0;

	// Set up cooking
	const PxCookingParams CurrentParams = PhysXCooking->getParams();
	PxCookingParams NewParams = CurrentParams;
	NewParams.targetPlatform = PhysXFormat;

	if (!!(CookFlags & EPhysXMeshCookFlags::SuppressFaceRemapTable))
	{
		NewParams.suppressTriangleMeshRemapTable = true;
	}

	if (!!(CookFlags & EPhysXMeshCookFlags::DeformableMesh))
	{
		// In the case of a deformable mesh, we have to change the cook params
		NewParams.meshPreprocessParams = PxMeshPreprocessingFlag::eDISABLE_CLEAN_MESH;

		// The default BVH34 midphase does not support refit
		NewParams.midphaseDesc = PxMeshMidPhase::eBVH33;
	}
	else
	{
		if (PhysXCooking->validateTriangleMesh(PTriMeshDesc) == false)
		{
			NewParams.meshPreprocessParams = PxMeshPreprocessingFlag::eDISABLE_CLEAN_MESH;
		}
		//For non deformable meshes we can try to use BVH34
		UseBVH34IfSupported(Format, NewParams);
	}

	PhysXCooking->setParams(NewParams);
	bool bResult = false;

	// Cook TriMesh Data
	if (bUseBuffer)
	{
		FPhysXOutputStream Buffer(&OutBuffer);
		bResult = PhysXCooking->cookTriangleMesh(PTriMeshDesc, Buffer);
	}
	else
	{
		FPhysXOutputStream Buffer(&OutBuffer);
		OutTriangleMesh = PhysXCooking->createTriangleMesh(PTriMeshDesc, GPhysXSDK->getPhysicsInsertionCallback()); //NOTE: getPhysicsInsertionCallback probably not thread safe!
		bResult = !!OutTriangleMesh;
	}


	// Restore cooking params
	PhysXCooking->setParams(CurrentParams);
	return bResult;
#else
	return false;
#endif		// WITH_PHYSX
}

bool FPhysXCooking::CookTriMesh(FName Format, EPhysXMeshCookFlags CookFlags, const TArray<FVector>& SrcVertices, const TArray<FTriIndices>& SrcIndices, const TArray<uint16>& SrcMaterialIndices, const bool FlipNormals, TArray<uint8>& OutBuffer) const
{
	PxTriangleMesh* JunkTriangleMesh = nullptr;
	return CookTriMeshImp</*bUseBuffer=*/true>(Format, CookFlags, SrcVertices, SrcIndices, SrcMaterialIndices, FlipNormals, OutBuffer, JunkTriangleMesh);
}

bool FPhysXCooking::CreateTriMesh(FName Format, EPhysXMeshCookFlags CookFlags, const TArray<FVector>& SrcVertices, const TArray<FTriIndices>& SrcIndices, const TArray<uint16>& SrcMaterialIndices, const bool FlipNormals, physx::PxTriangleMesh*& OutTriangleMesh) const
{
	TArray<uint8> JunkBuffer;
	return CookTriMeshImp</*bUseBuffer=*/false>(Format, CookFlags, SrcVertices, SrcIndices, SrcMaterialIndices, FlipNormals, JunkBuffer, OutTriangleMesh);
}

template <bool bUseBuffer>
bool FPhysXCooking::CookHeightFieldImp(FName Format, FIntPoint HFSize, const void* Samples, uint32 SamplesStride, TArray<uint8>& OutBuffer, PxHeightField*& OutHeightField) const
{
	OutHeightField = nullptr;
#if WITH_PHYSX
	PxPlatform::Enum PhysXFormat = PxPlatform::ePC;
	bool bIsPhysXCookingValid = GetPhysXCooking(Format, PhysXFormat);
	check(bIsPhysXCookingValid);

	PxHeightFieldDesc HFDesc;
	HFDesc.format = PxHeightFieldFormat::eS16_TM;
	HFDesc.nbColumns = HFSize.X;
	HFDesc.nbRows = HFSize.Y;
	HFDesc.samples.data = Samples;
	HFDesc.samples.stride = SamplesStride;
	HFDesc.flags = PxHeightFieldFlag::eNO_BOUNDARY_EDGES;

	// Set up cooking
	const PxCookingParams& Params = PhysXCooking->getParams();
	PxCookingParams NewParams = Params;
	NewParams.targetPlatform = PhysXFormat;
	UseBVH34IfSupported(Format, NewParams);

	PhysXCooking->setParams(NewParams);

	// Cook to a temp buffer
	TArray<uint8> CookedBuffer;
	FPhysXOutputStream Buffer(&CookedBuffer);

	if (bUseBuffer)
	{
		if (PhysXCooking->cookHeightField(HFDesc, Buffer) && CookedBuffer.Num() > 0)
		{
			// Append the cooked data into cooked buffer
			OutBuffer.Append(CookedBuffer);
			return true;
		}
	}
	else
	{
		OutHeightField = PhysXCooking->createHeightField(HFDesc, GPhysXSDK->getPhysicsInsertionCallback()); //NOTE: getPhysicsInsertionCallback probably not thread safe!
		if (OutHeightField)
		{
			return true;
		}
	}


	return false;
#else
	return false;
#endif		// WITH_PHYSX
}

bool FPhysXCooking::CookHeightField(FName Format, FIntPoint HFSize, const void* Samples, uint32 SamplesStride, TArray<uint8>& OutBuffer) const
{
	physx::PxHeightField* JunkHeightField = nullptr;
	return CookHeightFieldImp</*bUseBuffer=*/true>(Format, HFSize, Samples, SamplesStride, OutBuffer, JunkHeightField);
}

bool FPhysXCooking::CreateHeightField(FName Format, FIntPoint HFSize, const void* Samples, uint32 SamplesStride, physx::PxHeightField*& OutHeightField) const
{
	TArray<uint8> JunkBuffer;
	return CookHeightFieldImp</*bUseBuffer=*/false>(Format, HFSize, Samples, SamplesStride, JunkBuffer, OutHeightField);
}

bool FPhysXCooking::SerializeActors(FName Format, const TArray<FBodyInstance*>& Bodies, const TArray<UBodySetup*>& BodySetups, const TArray<UPhysicalMaterial*>& PhysicalMaterials, TArray<uint8>& OutBuffer) const
{
#if WITH_PHYSX
	PxSerializationRegistry* PRegistry = PxSerialization::createSerializationRegistry(*GPhysXSDK);
	PxCollection* PCollection = PxCreateCollection();

	PxBase* PLastObject = nullptr;

	for (FBodyInstance* BodyInstance : Bodies)
	{
		if (BodyInstance->RigidActorSync)
		{
			PCollection->add(*BodyInstance->RigidActorSync, BodyInstance->RigidActorSyncId);
			PLastObject = BodyInstance->RigidActorSync;
		}

		if (BodyInstance->RigidActorAsync)
		{
			PCollection->add(*BodyInstance->RigidActorAsync, BodyInstance->RigidActorAsyncId);
			PLastObject = BodyInstance->RigidActorAsync;
		}
	}

	PxSerialization::createSerialObjectIds(*PCollection, PxSerialObjectId(1));	//we get physx to assign an id for each actor

																				//Note that rigid bodies may have assigned ids. It's important to let them go first because we rely on that id for deserialization.
																				//One this is done we must find out the next available ID, and use that for naming the shared resources. We have to save this for deserialization
	uint64 BaseId = PLastObject ? (PCollection->getId(*PLastObject) + 1) : 1;

	PxCollection* PExceptFor = MakePhysXCollection(PhysicalMaterials, BodySetups, BaseId);

	for (FBodyInstance* BodyInstance : Bodies)	//and then we mark that id back into the bodyinstance so we can pair the two later
	{
		if (BodyInstance->RigidActorSync)
		{
			BodyInstance->RigidActorSyncId = PCollection->getId(*BodyInstance->RigidActorSync);
		}

		if (BodyInstance->RigidActorAsync)
		{
			BodyInstance->RigidActorAsyncId = PCollection->getId(*BodyInstance->RigidActorAsync);
		}
	}

	//We must store the BaseId for shared resources.
	FMemoryWriter Ar(OutBuffer);
	uint8 bIsLittleEndian = PLATFORM_LITTLE_ENDIAN; //TODO: We should pass the target platform into this function and write it. Then swap the endian on the writer so the reader doesn't have to do it at runtime
	Ar << bIsLittleEndian;
	Ar << BaseId;
	//Note that PhysX expects the binary data to be 128-byte aligned. Because of this we must pad
	int32 BytesToPad = PHYSX_SERIALIZATION_ALIGNMENT - (Ar.Tell() % PHYSX_SERIALIZATION_ALIGNMENT);
	OutBuffer.AddZeroed(BytesToPad);

	FPhysXOutputStream Buffer(&OutBuffer);
	PxSerialization::complete(*PCollection, *PRegistry, PExceptFor);
	PxSerialization::serializeCollectionToBinary(Buffer, *PCollection, *PRegistry, PExceptFor);

#if PHYSX_MEMORY_VALIDATION
	GPhysXAllocator->ValidateHeaders();
#endif
	PCollection->release();
	PExceptFor->release();
	PRegistry->release();

#if PHYSX_MEMORY_VALIDATION
	GPhysXAllocator->ValidateHeaders();
#endif
	return true;
#endif
	return false;
}


FPhysXPlatformModule::FPhysXPlatformModule()
{
	PhysXCookerTLS = FPlatformTLS::AllocTlsSlot();
}

FPhysXPlatformModule::~FPhysXPlatformModule()
{
		//NOTE: we don't bother cleaning up TLS, this is only closed during real shutdown
}

IPhysXCooking* FPhysXPlatformModule::GetPhysXCooking()
{
	FPhysXCooking* PhysXCooking = (FPhysXCooking*)FPlatformTLS::GetTlsValue(PhysXCookerTLS);
	if (!PhysXCooking)
	{
		InitPhysXCooking();
		PhysXCooking = new FPhysXCooking();
		FPlatformTLS::SetTlsValue(PhysXCookerTLS, PhysXCooking);
	}

	return PhysXCooking;
}

physx::PxCooking* FPhysXPlatformModule::CreatePhysXCooker(uint32 version, physx::PxFoundation& foundation, const physx::PxCookingParams& params)
{
#if WITH_PHYSX
	return PxCreateCooking(PX_PHYSICS_VERSION, foundation, params);
#endif
	return nullptr;
}

void FPhysXPlatformModule::Terminate()
{
	ShutdownPhysXCooking();
}

/**
*	Load the required modules for PhysX
*/
void FPhysXPlatformModule::InitPhysXCooking()
{
	if (IsInGameThread())
	{
		// Make sure PhysX libs are loaded
		PhysDLLHelper::LoadPhysXModules(/*bLoadCookingModule=*/true);
	}
}

void FPhysXPlatformModule::ShutdownPhysXCooking()
{
#if WITH_PHYSX
	if (PxCooking* PhysXCooking = GetPhysXCooking()->GetCooking())
	{
		PhysXCooking->release();
		PhysXCooking = nullptr;
	}

	//TODO: is it worth killing all the TLS objects?
#endif
}

IMPLEMENT_MODULE(FPhysXPlatformModule, PhysXCooking);
