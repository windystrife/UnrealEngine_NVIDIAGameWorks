// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	PhysXSupport.h: PhysX support
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Stats/Stats.h"
#include "Misc/ScopeLock.h"
#include "EngineDefines.h"
#include "Containers/Queue.h"
#include "Physics/PhysicsFiltering.h"
#include "PhysXPublic.h"

class UBodySetup;
class UPhysicalMaterial;
struct FByteBulkData;
struct FCollisionShape;

#if WITH_PHYSX

// Whether to track PhysX memory allocations
#ifndef PHYSX_MEMORY_VALIDATION
#define PHYSX_MEMORY_VALIDATION		0
#endif

// Whether to track PhysX memory allocations
#ifndef PHYSX_MEMORY_STATS
#define PHYSX_MEMORY_STATS		0 || PHYSX_MEMORY_VALIDATION
#endif

// binary serialization requires 128 byte alignment
#ifndef PHYSX_SERIALIZATION_ALIGNMENT
#define PHYSX_SERIALIZATION_ALIGNMENT 128
#endif

#define PHYSX_MEMORY_STAT_ONLY (0)

#if WITH_APEX
/** Get a pointer to the ApexScene from an SceneIndex (will be NULL if scene already shut down) */
nvidia::apex::Scene* GetApexSceneFromIndex(int32 InSceneIndex);
#endif

//////// GEOM CONVERSION
// we need this helper struct since PhysX needs geoms to be on the stack
struct UCollision2PGeom
{
	UCollision2PGeom(const FCollisionShape& CollisionShape);
	const PxGeometry * GetGeometry() { return (PxGeometry*)Storage; }
private:
	
	union StorageUnion
	{
		char box[sizeof(PxBoxGeometry)];
		char sphere[sizeof(PxSphereGeometry)];
		char capsule[sizeof(PxCapsuleGeometry)];
	};	//we need this because we can't use real union since these structs have non trivial constructors

	char Storage[sizeof(StorageUnion)];
};

/** Thresholds for aggregates  */
const uint32 AggregateMaxSize	   = 128;
const uint32 AggregateBodyShapesThreshold	   = 999999999;


/////// UTILS


/** Perform any deferred cleanup of resources (GPhysXPendingKillConvex etc) */
ENGINE_API void DeferredPhysResourceCleanup();


/** Calculates correct impulse at the body's center of mass and adds the impulse to the body. */
/** Util to see if a PxRigidBody is non-kinematic */
bool IsRigidBodyKinematic_AssumesLocked(const PxRigidBody* PRigidBody);

bool IsRigidBodyKinematicAndInSimulationScene_AssumesLocked(const PxRigidBody* PRigidBody);

/////// GLOBAL POINTERS

/** Pointer to PhysX Foundation singleton */
extern ENGINE_API PxFoundation*			GPhysXFoundation;
/** Pointer to PhysX debugger */
extern PxPvd*					GPhysXVisualDebugger;

extern ENGINE_API TAutoConsoleVariable<float> CVarToleranceScaleLength;

extern ENGINE_API TAutoConsoleVariable<float> CVarToleranceScaleSpeed;
	
#if WITH_APEX
/** 
 *	Map from SceneIndex to actual ApexScene. This indirection allows us to set it to null when we kill the scene, 
 *	and therefore abort trying to destroy PhysX objects after the scene has been destroyed (eg. on game exit). 
 */
extern TMap<int16, nvidia::apex::Scene*>	GPhysXSceneMap;
#else // #if WITH_APEX
/** 
 *	Map from SceneIndex to actual PxScene. This indirection allows us to set it to null when we kill the scene, 
 *	and therefore abort trying to destroy PhysX objects after the scene has been destroyed (eg. on game exit). 
 */
extern TMap<int16, PxScene*>	GPhysXSceneMap;
#endif // #if WITH_APEX

/** Total number of PhysX convex meshes around currently. */
extern int32					GNumPhysXConvexMeshes;

// The following are used for deferred cleanup - object that cannot be destroyed until all uses have been destroyed, but GC guarantees no order.

/** Array of PxConvexMesh objects which are awaiting cleaning up. */
extern TArray<PxConvexMesh*>	GPhysXPendingKillConvex;

/** Array of PxTriangleMesh objects which are awaiting cleaning up. */
extern ENGINE_API TArray<PxTriangleMesh*>	GPhysXPendingKillTriMesh;

/** Array of PxHeightField objects which are awaiting cleaning up. */
extern ENGINE_API TArray<PxHeightField*>	GPhysXPendingKillHeightfield;

/** Array of PxMaterial objects which are awaiting cleaning up. */
extern TArray<PxMaterial*>		GPhysXPendingKillMaterial;

#if WITH_FLEX
extern ENGINE_API NvFlexLibrary*				GFlexLib;
#endif

#if WITH_PHYSX
extern const physx::PxQuat U2PSphylBasis;
#endif // WITH_PHYSX

/** Utility class to keep track of shared physics data */
class FPhysxSharedData
{
public:
	static FPhysxSharedData& Get(){ return *Singleton; }
	static void Initialize();
	static void Terminate();

	void Add(PxBase* Obj);
//#nv begin #Blast Multiple UBodySetup handling
	//Check for contains first due to multiple UBodySetups sharing the same ref counted object causing harmless double-frees
	void Remove(PxBase* Obj)	{ if(Obj && SharedObjects->contains(*Obj)) { SharedObjects->remove(*Obj); } }
//nv end

	const PxCollection* GetCollection()	{ return SharedObjects; }

	void DumpSharedMemoryUsage(FOutputDevice* Ar);
private:
	/** Collection of shared physx objects */
	PxCollection* SharedObjects;
	
	static FPhysxSharedData* Singleton;

	FPhysxSharedData()
	{
		SharedObjects = PxCreateCollection();
	}

	~FPhysxSharedData()
	{
		SharedObjects->release();
	}

};

ENGINE_API PxCollection* MakePhysXCollection(const TArray<UPhysicalMaterial*>& PhysicalMaterials, const TArray<UBodySetup*>& BodySetups, uint64 BaseId);

/** Utility wrapper for a uint8 TArray for loading into PhysX. */
class FPhysXInputStream : public PxInputStream
{
public:
	/** Raw byte data */
	const uint8	*Data;
	/** Number of bytes */
	int32				DataSize;
	/** Current read position withing Data buffer */
	mutable uint32			ReadPos;

	FPhysXInputStream()
		: Data(NULL)
		, DataSize(0)
		, ReadPos(0)
	{}

	FPhysXInputStream(const uint8 *InData, const int32 InSize)
		: Data(InData)
		, DataSize( InSize )
		, ReadPos(0)
	{}

	virtual PxU32 read(void* Dest, PxU32 Count) override
	{
		check(Data);
		check(Dest);
		check(Count);
		const uint32 EndPos = ReadPos + Count;
		if( EndPos <= (uint32)DataSize )
		{
			FMemory::Memcpy( Dest, Data + ReadPos, Count );
			ReadPos = EndPos;
			return Count;
		}
		else
		{
			return 0;
		}
	}
};

/** Utility class for reading cooked physics data. */
class FPhysXCookingDataReader
{
public:
	TArray<PxConvexMesh*> ConvexMeshes;
	TArray<PxConvexMesh*> ConvexMeshesNegX;
	TArray<PxTriangleMesh*> TriMeshes;

	FPhysXCookingDataReader( FByteBulkData& InBulkData, struct FBodySetupUVInfo* UVInfo );

private:

	PxConvexMesh* ReadConvexMesh( FBufferReader& Ar, uint8* InBulkDataPtr, int32 InBulkDataSize );
	PxTriangleMesh* ReadTriMesh( FBufferReader& Ar, uint8* InBulkDataPtr, int32 InBulkDataSize );
};

/** PhysX memory allocator wrapper */
class FPhysXAllocator : public PxAllocatorCallback
{
#if PHYSX_MEMORY_STATS
	TMap<FName, size_t> AllocationsByType;

	struct FPhysXAllocationHeader
	{
		FPhysXAllocationHeader(	FName InAllocationTypeName, size_t InAllocationSize )
		:	AllocationTypeName(InAllocationTypeName)
		,	AllocationSize(InAllocationSize)
		{
			static_assert((sizeof(FPhysXAllocationHeader) % 16) == 0, "FPhysXAllocationHeader size must multiple of bytes.");
			MagicPadding();
		}

		void MagicPadding()
		{
			for (uint8 ByteCount = 0; ByteCount < sizeof(Padding); ++ByteCount)
			{
				Padding[ByteCount] = 'A' + ByteCount % 4;
			}
		}

		bool operator==(const FPhysXAllocationHeader& OtherHeader) const
		{
			bool bHeaderSame = AllocationTypeName == OtherHeader.AllocationTypeName && AllocationSize == OtherHeader.AllocationSize;
			for (uint8 ByteCount = 0; ByteCount < sizeof(Padding); ++ByteCount)
			{
				bHeaderSame &= Padding[ByteCount] == OtherHeader.Padding[ByteCount];
			}

			return bHeaderSame;
		}

		FName AllocationTypeName;
		size_t	AllocationSize;
		static const int PaddingSize = 8;
		uint8 Padding[PaddingSize];	//physx needs 16 byte alignment. Additionally we fill padding with a pattern to see if there's any memory stomps
		uint8 Padding2[(sizeof(FName) + sizeof(size_t) + PaddingSize) % 16];

		void Validate() const
		{
			bool bValid = true;
			for (uint8 ByteCount = 0; ByteCount < sizeof(Padding); ++ByteCount)
			{
				bValid &= Padding[ByteCount] == 'A' + ByteCount % 4;
			}

			check(bValid);

			FPhysXAllocationHeader* AllocationFooter = (FPhysXAllocationHeader*) (((uint8*)this) + sizeof(FPhysXAllocationHeader)+AllocationSize);
			check(*AllocationFooter == *this);
		}
	};
	
#endif

public:

#if PHYSX_MEMORY_VALIDATION

	/** Iterates over all allocations and checks that they the headers and footers are valid */
	void ValidateHeaders()
	{
		check(IsInGameThread());
		FPhysXAllocationHeader* TmpHeader = nullptr;
		while(NewHeaders.Dequeue(TmpHeader))
		{
			AllocatedHeaders.Add(TmpHeader);
		}

		while (OldHeaders.Dequeue(TmpHeader))
		{
			AllocatedHeaders.Remove(TmpHeader);
		}

		FScopeLock Lock(&ValidationCS);	//this is needed in case another thread is freeing the header
		for (FPhysXAllocationHeader* Header : AllocatedHeaders)
		{
			Header->Validate();
		}
	}
#endif

	FPhysXAllocator()
	{}

	virtual ~FPhysXAllocator() 
	{}

	virtual void* allocate(size_t size, const char* typeName, const char* filename, int line) override
	{
#if PHYSX_MEMORY_STATS
		INC_DWORD_STAT_BY(STAT_MemoryPhysXTotalAllocationSize, size);


		FString AllocationString = FString::Printf(TEXT("%s %s:%d"), ANSI_TO_TCHAR(typeName), ANSI_TO_TCHAR(filename), line);
		FName AllocationName(*AllocationString);

		// Assign header
		FPhysXAllocationHeader* AllocationHeader = (FPhysXAllocationHeader*)FMemory::Malloc(size + sizeof(FPhysXAllocationHeader) * 2, 16);
		AllocationHeader->AllocationTypeName = AllocationName;
		AllocationHeader->AllocationSize = size;
		AllocationHeader->MagicPadding();
		FPhysXAllocationHeader* AllocationFooter = (FPhysXAllocationHeader*) (((uint8*)AllocationHeader) + size + sizeof(FPhysXAllocationHeader));
		AllocationFooter->AllocationTypeName = AllocationName;
		AllocationFooter->AllocationSize = size;
		AllocationFooter->MagicPadding();

		size_t* TotalByType = AllocationsByType.Find(AllocationName);	//TODO: this is not thread safe!
		if( TotalByType )
		{
			*TotalByType += size;
		}
		else
		{
			AllocationsByType.Add(AllocationName, size);
		}

#if PHYSX_MEMORY_VALIDATION
		NewHeaders.Enqueue(AllocationHeader);
#endif

		return (uint8*)AllocationHeader + sizeof(FPhysXAllocationHeader);
#else
		void* ptr = FMemory::Malloc(size, 16);
		#if PHYSX_MEMORY_STAT_ONLY
			INC_DWORD_STAT_BY(STAT_MemoryPhysXTotalAllocationSize, FMemory::GetAllocSize(ptr));
		#endif
		return ptr;
#endif
	}
	 
	virtual void deallocate(void* ptr) override
	{
#if PHYSX_MEMORY_STATS
		if( ptr )
		{
			FPhysXAllocationHeader* AllocationHeader = (FPhysXAllocationHeader*)((uint8*)ptr - sizeof(FPhysXAllocationHeader));
#if PHYSX_MEMORY_VALIDATION
			AllocationHeader->Validate();
			OldHeaders.Enqueue(AllocationHeader);
			FScopeLock Lock(&ValidationCS);	//this is needed in case we are in the middle of validating the headers
#endif

			DEC_DWORD_STAT_BY(STAT_MemoryPhysXTotalAllocationSize, AllocationHeader->AllocationSize);
			size_t* TotalByType = AllocationsByType.Find(AllocationHeader->AllocationTypeName);
			*TotalByType -= AllocationHeader->AllocationSize;
			FMemory::Free(AllocationHeader);
		}
#else
		#if PHYSX_MEMORY_STAT_ONLY
			DEC_DWORD_STAT_BY(STAT_MemoryPhysXTotalAllocationSize, FMemory::GetAllocSize(ptr));
		#endif
		FMemory::Free(ptr);
#endif
	}

#if PHYSX_MEMORY_STATS
	void DumpAllocations(FOutputDevice* Ar)
	{
		struct FSortBySize
		{
			FORCEINLINE bool operator()( const size_t& A, const size_t& B ) const 
			{ 
				// Sort descending
				return B < A;
			}
		};
				
		AllocationsByType.ValueSort(FSortBySize());
		for( auto It=AllocationsByType.CreateConstIterator(); It; ++It )
		{
			Ar->Logf(TEXT("%-10d %s"), It.Value(), *It.Key().ToString());
		}
	}
#endif

#if PHYSX_MEMORY_VALIDATION
private:
	FCriticalSection ValidationCS;
	TSet<FPhysXAllocationHeader*> AllocatedHeaders;

	//Since this needs to be thread safe we can't add to the allocated headers set until we're on the game thread
	TQueue<FPhysXAllocationHeader*, EQueueMode::Mpsc> NewHeaders;
	TQueue<FPhysXAllocationHeader*, EQueueMode::Mpsc> OldHeaders;

#endif
};

/** PhysX output stream wrapper */
class FPhysXErrorCallback : public PxErrorCallback
{
public:
	virtual void reportError(PxErrorCode::Enum e, const char* message, const char* file, int line) override;
};

/** 'Shader' used to filter simulation collisions. Could be called on any thread. */
PxFilterFlags PhysXSimFilterShader(	PxFilterObjectAttributes attributes0, PxFilterData filterData0, 
									PxFilterObjectAttributes attributes1, PxFilterData filterData1,
									PxPairFlags& pairFlags, const void* constantBlock, PxU32 constantBlockSize );

class FPhysScene;

/** Event callback used to notify engine about various collision events */
class ENGINE_API FPhysXSimEventCallback : public PxSimulationEventCallback
{
public:
	FPhysXSimEventCallback(FPhysScene* InOwningScene, int32 InSceneType) : OwningScene(InOwningScene), SceneType(InSceneType){}

	virtual void onConstraintBreak(PxConstraintInfo* constraints, PxU32 count) override;
	virtual void onWake(PxActor** actors, PxU32 count) override;
	virtual void onSleep(PxActor** actors, PxU32 count) override;
	virtual void onTrigger(PxTriggerPair* pairs, PxU32 count) override {}
	virtual void onContact(const PxContactPairHeader& PairHeader, const PxContactPair* Pairs, PxU32 NumPairs) override;
	virtual void onAdvance(const PxRigidBody*const* bodyBuffer, const PxTransform* poseBuffer, const PxU32 count) override {}

private:	
	FPhysScene* OwningScene;
	int32 SceneType;
};


#if WITH_APEX
/**
	"Null" render resource manager callback for APEX
	This just gives a trivial implementation of the interface, since we are not using the APEX rendering API
*/
class FApexNullRenderResourceManager : public nvidia::apex::UserRenderResourceManager
{
public:
	// NxUserRenderResourceManager interface.

	virtual nvidia::apex::UserRenderVertexBuffer*	createVertexBuffer(const nvidia::apex::UserRenderVertexBufferDesc&) override
	{
		return NULL;
	}
	virtual nvidia::apex::UserRenderIndexBuffer*	createIndexBuffer(const nvidia::apex::UserRenderIndexBufferDesc&) override
	{
		return NULL;
	}
	virtual nvidia::apex::UserRenderBoneBuffer*		createBoneBuffer(const nvidia::apex::UserRenderBoneBufferDesc&) override
	{
		return NULL;
	}
	virtual nvidia::apex::UserRenderInstanceBuffer*	createInstanceBuffer(const nvidia::apex::UserRenderInstanceBufferDesc&) override
	{
		return NULL;
	}
	virtual nvidia::apex::UserRenderSpriteBuffer*   createSpriteBuffer(const nvidia::apex::UserRenderSpriteBufferDesc&) override
	{
		return NULL;
	}
	
	virtual nvidia::apex::UserRenderSurfaceBuffer*  createSurfaceBuffer(const nvidia::apex::UserRenderSurfaceBufferDesc& desc)   override
	{
		return NULL;
	}
	
	virtual nvidia::apex::UserRenderResource*		createResource(const nvidia::apex::UserRenderResourceDesc&) override
	{
		return NULL;
	}
	virtual void						releaseVertexBuffer(nvidia::apex::UserRenderVertexBuffer&) override {}
	virtual void						releaseIndexBuffer(nvidia::apex::UserRenderIndexBuffer&) override {}
	virtual void						releaseBoneBuffer(nvidia::apex::UserRenderBoneBuffer&) override {}
	virtual void						releaseInstanceBuffer(nvidia::apex::UserRenderInstanceBuffer&) override {}
	virtual void						releaseSpriteBuffer(nvidia::apex::UserRenderSpriteBuffer&) override {}
	virtual void                        releaseSurfaceBuffer(nvidia::apex::UserRenderSurfaceBuffer& buffer) override{}
	virtual void						releaseResource(nvidia::apex::UserRenderResource&) override {}

	virtual physx::PxU32				getMaxBonesForMaterial(void*) override
	{
		return 0;
	}
	virtual bool						getSpriteLayoutData(physx::PxU32 , physx::PxU32 , nvidia::apex::UserRenderSpriteBufferDesc* ) override
	{
		return false;
	}
	virtual bool						getInstanceLayoutData(physx::PxU32 , physx::PxU32 , nvidia::apex::UserRenderInstanceBufferDesc* ) override
	{
		return false;
	}

};
extern FApexNullRenderResourceManager GApexNullRenderResourceManager;

/**
	APEX resource callback
	The resource callback is how APEX asks the application to find assets when it needs them
*/
class FApexResourceCallback : public nvidia::apex::ResourceCallback
{
public:
	// NxResourceCallback interface.

	virtual void* requestResource(const char* NameSpace, const char* Name) override
	{
		// Here a pointer is looked up by name and returned
		(void)NameSpace;
		(void)Name;

		return NULL;
	}

	virtual void  releaseResource(const char* NameSpace, const char* Name, void* Resource) override
	{
		// Here we release a named resource
		(void)NameSpace;
		(void)Name;
		(void)Resource;
	}
};
extern FApexResourceCallback GApexResourceCallback;


#endif // #if WITH_APEX

/** Util to determine whether to use NegX version of mesh, and what transform (rotation) to apply. */
bool CalcMeshNegScaleCompensation(const FVector& InScale3D, PxTransform& POutTransform);

/** Utility wrapper for a PhysX output stream that only counts the memory. */
class FPhysXCountMemoryStream : public PxOutputStream
{
public:
	/** Memory used by the serialized object(s) */
	uint32 UsedMemory;

	FPhysXCountMemoryStream()
		: UsedMemory(0)
	{}

	virtual PxU32 write(const void* Src, PxU32 Count) override
	{
		UsedMemory += Count;
		return Count;
	}
};

/** 
 * Returns the in-memory size of the specified object by serializing it.
 *
 * @param	Obj					Object to determine the memory footprint for
 * @param	SharedCollection	Shared collection of data to ignore
 * @returns						Size of the object in bytes determined by serialization
 **/
ENGINE_API SIZE_T GetPhysxObjectSize(PxBase* Obj, const PxCollection* SharedCollection);
#endif // WITH_PHYSX

/** Helper struct holding physics body filter data during initialisation */
struct FShapeFilterData
{
	PxFilterData SimFilter;
	PxFilterData QuerySimpleFilter;
	PxFilterData QueryComplexFilter;
};

/** Helper object to hold initialisation data for shapes */
struct FShapeData
{
	FShapeData()
		: CollisionEnabled(ECollisionEnabled::NoCollision)
		, SyncShapeFlags(0)
		, AsyncShapeFlags(0)
		, SimpleShapeFlags(0)
		, ComplexShapeFlags(0)
		, SyncBodyFlags(0)
		, AsyncBodyFlags(0)
	{

	}

	TEnumAsByte<ECollisionEnabled::Type> CollisionEnabled;
	FShapeFilterData FilterData;
	PxShapeFlags SyncShapeFlags;
	PxShapeFlags AsyncShapeFlags;
	PxShapeFlags SimpleShapeFlags;
	PxShapeFlags ComplexShapeFlags;
	PxRigidBodyFlags SyncBodyFlags;
	PxRigidBodyFlags AsyncBodyFlags;
};
