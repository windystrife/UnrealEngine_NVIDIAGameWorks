// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// Physics engine integration utilities

#include "CoreMinimal.h"
#include "EngineDefines.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "PhysxUserData.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Components/PrimitiveComponent.h"
#include "Model.h"
#include "PhysicsPublic.h"
#include "PhysXPublic.h"
#include "PhysicsEngine/PhysXSupport.h"
#include "PhysicsEngine/ConvexElem.h"
#include "PhysicsEngine/BodySetup.h"

#if WITH_FLEX
#include "FlexContainerInstance.h"
#include "DrawDebugHelpers.h" // FlushPersistentDebugLines
#endif

/* *********************************************************************** */
/* *********************************************************************** */
/* *********************** MODELTOHULLS  ********************************* */
/* *********************************************************************** */
/* *********************************************************************** */


/** Returns false if ModelToHulls operation should halt because of vertex count overflow. */
static bool AddConvexPrim(FKAggregateGeom* OutGeom, TArray<FPlane> &Planes, UModel* InModel)
{
	// Add Hull.
	FKConvexElem NewConvex;

	// Because of precision, we use the original model verts as 'snap to' verts.
	TArray<FVector> SnapVerts;
	for(int32 k=0; k<InModel->Verts.Num(); k++)
	{
		// Find vertex vector. Bit of  hack - sometimes FVerts are uninitialised.
		const int32 PointIx = InModel->Verts[k].pVertex;
		if(PointIx < 0 || PointIx >= InModel->Points.Num())
		{
			continue;
		}

		SnapVerts.Add(InModel->Points[PointIx]);
	}

	// Create a hull from a set of planes
	bool bSuccess = NewConvex.HullFromPlanes(Planes, SnapVerts);

	// If it failed for some reason, remove from the array
	if(bSuccess && NewConvex.ElemBox.IsValid)
	{
		OutGeom->ConvexElems.Add(NewConvex);
	}

	// Return if we succeeded or not
	return bSuccess;
}

// Worker function for traversing collision mode/blocking volumes BSP.
// At each node, we record, the plane at this node, and carry on traversing.
// We are interested in 'inside' ie solid leafs.
/** Returns false if ModelToHulls operation should halt because of vertex count overflow. */
static bool ModelToHullsWorker(FKAggregateGeom* outGeom,
								UModel* inModel, 
								int32 nodeIx, 
								bool bOutside, 
								TArray<FPlane> &planes)
{
	FBspNode& node = inModel->Nodes[nodeIx];
	// BACK
	if (node.iBack != INDEX_NONE) // If there is a child, recurse into it.
	{
		planes.Add(node.Plane);
		if (!ModelToHullsWorker(outGeom, inModel, node.iBack, node.ChildOutside(0, bOutside), planes))
		{
			return false;
		}
		planes.RemoveAt(planes.Num() - 1);
	}
	else if (!node.ChildOutside(0, bOutside)) // If its a leaf, and solid (inside)
	{
		planes.Add(node.Plane);
		if (!AddConvexPrim(outGeom, planes, inModel))
		{
			return false;
		}
		planes.RemoveAt(planes.Num() - 1);
	}

	// FRONT
	if (node.iFront != INDEX_NONE)
	{
		planes.Add(node.Plane.Flip());
		if (!ModelToHullsWorker(outGeom, inModel, node.iFront, node.ChildOutside(1, bOutside), planes))
		{
			return false;
		}
		planes.RemoveAt(planes.Num() - 1);
	}
	else if (!node.ChildOutside(1, bOutside))
	{
		planes.Add(node.Plane.Flip());
		if (!AddConvexPrim(outGeom, planes, inModel))
		{
			return false;
		}
		planes.RemoveAt(planes.Num() - 1);
	}

	return true;
}

void UBodySetup::CreateFromModel(UModel* InModel, bool bRemoveExisting)
{
	if ( bRemoveExisting )
	{
		RemoveSimpleCollision();
	}

	const int32 NumHullsAtStart = AggGeom.ConvexElems.Num();
	
	if( InModel != NULL && InModel->Nodes.Num() > 0)
	{
		TArray<FPlane>	Planes;
		bool bSuccess = ModelToHullsWorker(&AggGeom, InModel, 0, InModel->RootOutside, Planes);
		if ( !bSuccess )
		{
			// ModelToHullsWorker failed.  Clear out anything that may have been created.
			AggGeom.ConvexElems.Empty();
		}
	}

	// Create new GUID
	InvalidatePhysicsData();
}

//////////////////////////////////////////////////////////////////////////
// FRigidBodyCollisionInfo

void FRigidBodyCollisionInfo::SetFrom(const FBodyInstance* BodyInst)
{
	if(BodyInst != NULL)
	{
		BodyIndex = BodyInst->InstanceBodyIndex;
		BoneName = BodyInst->BodySetup.IsValid() ? BodyInst->BodySetup->BoneName : NAME_None;

		if(BodyInst->OwnerComponent.IsValid())
		{
			Component = BodyInst->OwnerComponent;
			Actor = Component->GetOwner();
		}
	}
	else
	{
		Component = NULL;
		Actor = NULL;
		BodyIndex = INDEX_NONE;
		BoneName = NAME_None;
	}
}


FBodyInstance* FRigidBodyCollisionInfo::GetBodyInstance() const
{
	FBodyInstance* BodyInst = NULL;
	if(Component.IsValid())
	{
		BodyInst = Component->GetBodyInstance(BoneName);
	}
	return BodyInst;
}

//////////////////////////////////////////////////////////////////////////
// FCollisionNotifyInfo

bool FCollisionNotifyInfo::IsValidForNotify() const
{
	return (Info0.Component.IsValid() && Info1.Component.IsValid());
}

/** Iterate over ContactInfos array and swap order of information */
void FCollisionImpactData::SwapContactOrders()
{
	for(int32 i=0; i<ContactInfos.Num(); i++)
	{
		ContactInfos[i].SwapOrder();
	}
}

/** Swap the order of info in this info  */
void FRigidBodyContactInfo::SwapOrder()
{
	Swap(PhysMaterial[0], PhysMaterial[1]);
	ContactNormal = -ContactNormal;
}

//////////////////////////////////////////////////////////////////////////
// FCollisionResponseContainer

/** Set the status of a particular channel in the structure. */
void FCollisionResponseContainer::SetResponse(ECollisionChannel Channel, ECollisionResponse NewResponse)
{
	if (Channel < ARRAY_COUNT(EnumArray))
	{
		EnumArray[Channel] = NewResponse;
	}
}

/** Set all channels to the specified state */
void FCollisionResponseContainer::SetAllChannels(ECollisionResponse NewResponse)
{
	for(int32 i=0; i<ARRAY_COUNT(EnumArray); i++)
	{
		EnumArray[i] = NewResponse;
	}
}

void FCollisionResponseContainer::ReplaceChannels(ECollisionResponse OldResponse, ECollisionResponse NewResponse)
{
	for (int32 i = 0; i < ARRAY_COUNT(EnumArray); i++)
	{
		if(EnumArray[i] == OldResponse)
		{
			EnumArray[i] = NewResponse;
		}
	}
}

FCollisionResponseContainer FCollisionResponseContainer::CreateMinContainer(const FCollisionResponseContainer& A, const FCollisionResponseContainer& B)
{
	FCollisionResponseContainer Result;
	for(int32 i=0; i<ARRAY_COUNT(Result.EnumArray); i++)
	{
		Result.EnumArray[i] = FMath::Min(A.EnumArray[i], B.EnumArray[i]);
	}
	return Result;
}


/** This constructor will zero out the struct */
FCollisionResponseContainer::FCollisionResponseContainer()
{
	// if this is called before profile is initialized, it will be overwritten by postload code
	// if this is called after profile is initialized, this will have correct values
	*this = FCollisionResponseContainer::DefaultResponseContainer;
}

FCollisionResponseContainer::FCollisionResponseContainer(ECollisionResponse DefaultResponse)
{
	SetAllChannels(DefaultResponse);
}

/** PxScene visualization for a particular EPhysicsSceneType */
static bool ExecPxVis(UWorld* InWorld, uint32 SceneType, const TCHAR* Cmd, FOutputDevice* Ar)
{
	check(SceneType < PST_MAX);

#if WITH_PHYSX
	// Get the scene to set flags on
	PxScene* PScene = InWorld->GetPhysicsScene()->GetPhysXScene(SceneType);

	struct { const TCHAR* Name; PxVisualizationParameter::Enum Flag; float Size; } Flags[] =
	{
		// Axes
		{ TEXT("WORLDAXES"),			PxVisualizationParameter::eWORLD_AXES,			1.f },
		{ TEXT("BODYAXES"),				PxVisualizationParameter::eBODY_AXES,			1.f },
		{ TEXT("MASSAXES"),             PxVisualizationParameter::eBODY_MASS_AXES,		1.f },

		// Contacts
		{ TEXT("CONTACTPOINT"),			PxVisualizationParameter::eCONTACT_POINT,		1.f },
		{ TEXT("CONTACTS"),				PxVisualizationParameter::eCONTACT_NORMAL,		1.f },
		{ TEXT("CONTACTERROR"),			PxVisualizationParameter::eCONTACT_ERROR,		100.f },
		{ TEXT("CONTACTFORCE"),			PxVisualizationParameter::eCONTACT_FORCE,		1.f },

		// Joints
		{ TEXT("JOINTLIMITS"),			PxVisualizationParameter::eJOINT_LIMITS,		1.f },
		{ TEXT("JOINTLOCALFRAMES"),		PxVisualizationParameter::eJOINT_LOCAL_FRAMES,	1.f },

		// Collision
		{ TEXT("COLLISION"),			PxVisualizationParameter::eCOLLISION_SHAPES,	1.f },
	};

	SCOPED_SCENE_WRITE_LOCK(PScene);

	bool bDebuggingActive = false;
	bool bFoundFlag = false;
	if ( FParse::Command(&Cmd,TEXT("PHYSX_CLEAR_ALL")) )
	{
		Ar->Logf(TEXT("Clearing all PhysX Debug Flags."));
		for (int32 i = 0; i < ARRAY_COUNT(Flags); i++)
		{
			PScene->setVisualizationParameter(Flags[i].Flag, 0.0f);
			bFoundFlag = true;
		}
	}
	else
	{
		for (int32 i = 0; i < ARRAY_COUNT(Flags); i++)
		{
			// Parse out the command sent in and set only those flags
			if (FParse::Command(&Cmd, Flags[i].Name))
			{
				PxReal Result = PScene->getVisualizationParameter(Flags[i].Flag);
				if (Result == 0.0f)
				{
					PScene->setVisualizationParameter(Flags[i].Flag, Flags[i].Size);
					Ar->Logf(TEXT("Flag set."));
				}
				else
				{
					PScene->setVisualizationParameter(Flags[i].Flag, 0.0f);
					Ar->Logf(TEXT("Flag un-set."));
				}

				bFoundFlag = true;
			}

			// See if any flags are true
			PxReal Result = PScene->getVisualizationParameter(Flags[i].Flag);
			if(Result > 0.f)
			{
				bDebuggingActive = true;
			}
		}
	}

	// If no debugging going on - disable it using NX_VISUALIZATION_SCALE
	if(bDebuggingActive)
	{
		PScene->setVisualizationParameter(PxVisualizationParameter::eSCALE, 20.0f);
	}
	else
	{
		PScene->setVisualizationParameter(PxVisualizationParameter::eSCALE, 0.0f);
	}

	if(!bFoundFlag)
	{
		Ar->Logf(TEXT("Unknown PhysX visualization flag specified."));
	}
#endif	// WITH_PHYSX

	return 1;
}

/** PxScene visualization for a particular EPhysicsSceneType */
static bool ExecApexVis(UWorld* InWorld, uint32 SceneType, const TCHAR* Cmd, FOutputDevice* Ar)
{
	check(Cmd);
	check(SceneType < PST_MAX);

#if WITH_PHYSX
#if WITH_APEX
	// Get the scene to set flags on
	apex::Scene* ApexScene = InWorld->GetPhysicsScene()->GetApexScene(SceneType);

	if (ApexScene == NULL)
	{
		return false;
	}

	NvParameterized::Interface* DebugRenderParams = ApexScene->getDebugRenderParams();
	check(DebugRenderParams != NULL);

	// Toggle global flags if there are no further arguments
	const bool bToggle = *Cmd == TCHAR(0);

	// Enable or toggle visualization
	NvParameterized::Handle EnableDebugRenderHandle(*DebugRenderParams, "Enable");
	check(EnableDebugRenderHandle.isValid());
	bool bEnableValue = true;
	if (bToggle)
	{
		EnableDebugRenderHandle.getParamBool(bEnableValue);
		bEnableValue = !bEnableValue;
	}
	EnableDebugRenderHandle.setParamBool(bEnableValue);
	NvParameterized::Handle ScaleDebugRenderHandle(*DebugRenderParams, "Scale");
	check(ScaleDebugRenderHandle.isValid());
	float ScaleValue = 1.0f;
	if (bToggle)
	{
		ScaleDebugRenderHandle.getParamF32(ScaleValue);
		ScaleValue = ScaleValue > 0.0f ? 0.0f : 1.0f;
	}
	ScaleDebugRenderHandle.setParamF32(ScaleValue);

	// See if there's a '/', which means we have a module-specific visualization parameter
	const TCHAR* Slash = Cmd;
	while (*Slash != TCHAR(0) && *Slash != TCHAR('/'))
	{
		++Slash;
	}

	if (*Slash != TCHAR(0))
	{
		FString ModuleName = FString(Cmd).Left((int32)(Slash-Cmd));
		DebugRenderParams = ApexScene->getModuleDebugRenderParams(StringCast<ANSICHAR>(*ModuleName).Get());
	}

	if (DebugRenderParams == NULL)
	{
		Ar->Logf(TEXT("Unknown APEX module requested for apex debug visualization."));
		return false;
	}

	auto FlagNameAnsi = StringCast<ANSICHAR>((*Slash == TCHAR(0) ? Cmd : Slash+1));
	NvParameterized::Handle DebugRenderHandle(*DebugRenderParams, FlagNameAnsi.Get());

	if (!DebugRenderHandle.isValid())
	{
		Ar->Logf(TEXT("Unknown APEX visualization flag specified."));
		return false;
	}

	if (DebugRenderHandle.parameterDefinition()->type() == NvParameterized::TYPE_F32)
	{
		PxF32 Value;
		DebugRenderHandle.getParamF32(Value);
		DebugRenderHandle.setParamF32(Value > 0.0f ? 0.0f : 1.0f);
	}
	else
	if (DebugRenderHandle.parameterDefinition()->type() == NvParameterized::TYPE_U32)
	{
		PxU32 Value;
		DebugRenderHandle.getParamU32(Value);
		DebugRenderHandle.setParamU32(Value > 0 ? 0 : 1);
	}
	else
	if (DebugRenderHandle.parameterDefinition()->type() == NvParameterized::TYPE_BOOL)
	{
		bool bValue;
		DebugRenderHandle.getParamBool(bValue);
		DebugRenderHandle.setParamBool(!bValue);
	}
	else
	{
		Ar->Logf(TEXT("Unknown APEX visualization flag type."));
		return false;
	}

#endif	// WITH_APEX
#endif	// WITH_PHYSX

	return true;
}

#if WITH_PHYSX
void PvdConnect(FString Host, bool bVisualization)
{
	int32	Port = 5425;         // TCP port to connect to, where PVD is listening
	uint32	Timeout = 100;          // timeout in milliseconds to wait for PVD to respond, consoles and remote PCs need a higher timeout.

	PxPvdInstrumentationFlags ConnectionFlags = PxPvdInstrumentationFlag::eALL;

	PxPvdTransport* transport = PxDefaultPvdSocketTransportCreate(TCHAR_TO_ANSI(*Host), Port, Timeout);
	GPhysXVisualDebugger->connect(*transport, ConnectionFlags);

	// per scene properties (e.g. PxPvdSceneFlag::eTRANSMIT_CONSTRAINTS) are 
	// set on the PxPvdSceneClient in PhysScene.cpp, FPhysScene::InitPhysScene
}
#endif

//// EXEC
bool ExecPhysCommands(const TCHAR* Cmd, FOutputDevice* Ar, UWorld* InWorld)
{
#if WITH_PHYSX
	if( FParse::Command(&Cmd, TEXT("PXVIS")) )
	{
		if(InWorld->GetPhysicsScene() == NULL)
		{
			return 1;
		}

		// See which scene type(s) is(are) requested
		bool bVisualizeSync = FParse::Command(&Cmd, TEXT("SYNC"));
		bool bVisualizeAsync = FParse::Command(&Cmd, TEXT("ASYNC")) && InWorld->GetPhysicsScene()->HasAsyncScene();
		if(!bVisualizeSync && !bVisualizeAsync)
		{
			// If none are requested, do both
			bVisualizeSync = true;
			bVisualizeAsync = InWorld->GetPhysicsScene()->HasAsyncScene();
		}

		bool bResult = false;

		// Visualize sync scene if requested
		if(bVisualizeSync)
		{
			if( ExecPxVis(InWorld, PST_Sync, Cmd, Ar) != 0 )
			{
				bResult = 1;
			}
		}

		// Visualize async scene if requested
		if(bVisualizeAsync)
		{
			if( ExecPxVis(InWorld, PST_Async, Cmd, Ar) != 0 )
			{
				bResult = 1;
			}
		}

		return bResult;
	}
	if( FParse::Command(&Cmd, TEXT("APEXVIS")) )
	{
		if(InWorld->GetPhysicsScene() == NULL)
		{
			return 1;
		}

		// See which scene type(s) is(are) requested
		bool bVisualizeSync = FParse::Command(&Cmd, TEXT("SYNC"));
		bool bVisualizeAsync = FParse::Command(&Cmd, TEXT("ASYNC")) && InWorld->GetPhysicsScene()->HasAsyncScene();
		if(!bVisualizeSync && !bVisualizeAsync)
		{
			// If none are requested, do both
			bVisualizeSync = true;
			bVisualizeAsync = InWorld->GetPhysicsScene()->HasAsyncScene();
		}

		bool bResult = false;

		// Visualize sync scene if requested
		if(bVisualizeSync)
		{
			if( ExecApexVis(InWorld, PST_Sync, Cmd, Ar) != 0 )
			{
				bResult = 1;
			}
		}

		// Visualize async scene if requested
		if(bVisualizeAsync)
		{
			if( ExecApexVis(InWorld, PST_Async, Cmd, Ar) != 0 )
			{
				bResult = 1;
			}
		}

		return bResult;
	}
	else if(!IsRunningCommandlet() && GPhysXSDK && FParse::Command(&Cmd, TEXT("PVD")) )
	{
		// check if PvdConnection manager is available on this platform
		if(GPhysXVisualDebugger != NULL)
		{
			if(FParse::Command(&Cmd, TEXT("CONNECT")))
			{
				
				
				const bool bVizualization = !FParse::Command(&Cmd, TEXT("NODEBUG"));

				// setup connection parameters
				FString Host = TEXT("localhost");
				if (*Cmd)
				{
					Host = Cmd;
				}

				

				PvdConnect(Host, bVizualization);

			}
			else if(FParse::Command(&Cmd, TEXT("DISCONNECT")))
			{
				GPhysXVisualDebugger->disconnect();
			}
		}

		return 1;
	}
#if PHYSX_MEMORY_STATS
	else if(GPhysXAllocator && FParse::Command(&Cmd, TEXT("PHYSXALLOC")) )
	{
		GPhysXAllocator->DumpAllocations(Ar);
		return 1;
	}
#endif
	else if(FParse::Command(&Cmd, TEXT("PHYSXSHARED")) )
	{
		FPhysxSharedData::Get().DumpSharedMemoryUsage(Ar);
		return 1;
	}
	else if(FParse::Command(&Cmd, TEXT("PHYSXINFO")))
	{
		Ar->Logf(TEXT("PhysX Info:"));
		Ar->Logf(TEXT("  Version: %d.%d.%d"), PX_PHYSICS_VERSION_MAJOR, PX_PHYSICS_VERSION_MINOR, PX_PHYSICS_VERSION_BUGFIX);
#if UE_BUILD_DEBUG && !defined(NDEBUG)
		Ar->Logf(TEXT("  Configuration: DEBUG"));
#elif WITH_PHYSX_RELEASE
		Ar->Logf(TEXT("  Configuration: RELEASE"));
#else
		Ar->Logf(TEXT("  Configuration: PROFILE"));
#endif
		if(GetPhysXCookingModule())
		{
			Ar->Logf(TEXT("  Cooking Module: TRUE"));
		}
		else
		{
			Ar->Logf(TEXT("  Cooking Module: FALSE"));
		}

		return 1;
	}

#if WITH_FLEX
	if (FParse::Command(&Cmd, TEXT("FLEXVIS")))
	{
		FFlexContainerInstance::sGlobalDebugDraw = !FFlexContainerInstance::sGlobalDebugDraw;

		// if disabling debug draw ensure any persistent lines are flushed
		if (!FFlexContainerInstance::sGlobalDebugDraw)
			FlushPersistentDebugLines(InWorld);
	}
	else if (FParse::Command(&Cmd, TEXT("FLEXSTARTRECORD")))
	{
		FPhysScene* Scene = InWorld->GetPhysicsScene();
		if (Scene)
			Scene->StartFlexRecord();
	}
	else if (FParse::Command(&Cmd, TEXT("FLEXSTOPRECORD")))
	{
		FPhysScene* Scene = InWorld->GetPhysicsScene();
		if (Scene)
			Scene->StopFlexRecord();
	}
#endif

#endif // WITH_PHYSX

	return 0;
}

#if WITH_PHYSX
void ListAwakeRigidBodiesFromScene(bool bIncludeKinematic, PxScene* PhysXScene, int32& totalCount)
{
	check(PhysXScene != NULL);
	
	SCOPED_SCENE_READ_LOCK(PhysXScene);

	PxActor* PhysXActors[2048];
	int32 NumberActors = PhysXScene->getActors(PxActorTypeFlag::eRIGID_DYNAMIC, PhysXActors, 2048);
	for( int32 i = 0; i < NumberActors; ++i )
	{
		PxRigidDynamic* RgActor = (PxRigidDynamic*)PhysXActors[ i ];
		if (!RgActor->isSleeping() && (bIncludeKinematic || RgActor->getRigidBodyFlags() != PxRigidBodyFlag::eKINEMATIC))
		{
			++totalCount;
			FBodyInstance* BodyInst = FPhysxUserData::Get<FBodyInstance>(RgActor->userData);
			if ( BodyInst )
			{
				UE_LOG(LogPhysics, Log, TEXT("BI %s %d"), BodyInst->OwnerComponent.Get() ? *BodyInst->OwnerComponent.Get()->GetPathName() : TEXT("NONE"), BodyInst->InstanceBodyIndex);
			}
			else
			{
				UE_LOG(LogPhysics, Log, TEXT("BI %s"), TEXT("NONE") );
			}
		}
	}
}
#endif // WITH_PHYSX

/** Util to list to log all currently awake rigid bodies */
void ListAwakeRigidBodies(bool bIncludeKinematic, UWorld* world)
{
#if WITH_PHYSX
	if ( world )
	{
		int32 BodyCount = 0;
		UE_LOG(LogPhysics, Log, TEXT("TOTAL: ListAwakeRigidBodies needs fixing."));
		ListAwakeRigidBodiesFromScene(bIncludeKinematic, world->GetPhysicsScene()->GetPhysXScene(PST_Sync), BodyCount);
		if (world->GetPhysicsScene()->HasAsyncScene())
		{
			ListAwakeRigidBodiesFromScene(bIncludeKinematic, world->GetPhysicsScene()->GetPhysXScene(PST_Async) ,BodyCount);
		}
		UE_LOG(LogPhysics, Log, TEXT("TOTAL: %d awake bodies."), BodyCount);
	}
#endif // WITH_PHYSX
}
