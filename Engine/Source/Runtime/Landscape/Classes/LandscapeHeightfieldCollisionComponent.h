// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "Templates/RefCounting.h"
#include "EngineDefines.h"
#include "AI/Navigation/NavigationTypes.h"
#include "Components/PrimitiveComponent.h"
#include "Serialization/BulkData.h"
#include "LandscapeHeightfieldCollisionComponent.generated.h"

class ALandscapeProxy;
class FAsyncPreRegisterDDCRequest;
class ULandscapeComponent;
class ULandscapeInfo;
class ULandscapeLayerInfoObject;
class UPhysicalMaterial;
struct FConvexVolume;
struct FEngineShowFlags;
struct FNavigableGeometryExport;

#if WITH_PHYSX
namespace physx
{
	class PxMaterial;
	class PxHeightField;
}
#endif // WITH_PHYSX

UCLASS(MinimalAPI, Within=LandscapeProxy)
class ULandscapeHeightfieldCollisionComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	/** List of layers painted on this component. Matches the WeightmapLayerAllocations array in the LandscapeComponent. */
	UPROPERTY()
	TArray<ULandscapeLayerInfoObject*> ComponentLayerInfos;

	/** Offset of component in landscape quads */
	UPROPERTY()
	int32 SectionBaseX;

	UPROPERTY()
	int32 SectionBaseY;

	/** Size of component in collision quads */
	UPROPERTY()
	int32 CollisionSizeQuads;

	/** Collision scale: (ComponentSizeQuads) / (CollisionSizeQuads) */
	UPROPERTY()
	float CollisionScale;

	/** Size of component's "simple collision" in collision quads */
	UPROPERTY()
	int32 SimpleCollisionSizeQuads;

	/** The flags for each collision quad. See ECollisionQuadFlags. */
	UPROPERTY()
	TArray<uint8> CollisionQuadFlags;

	/** Guid used to share PhysX heightfield objects in the editor */
	UPROPERTY()
	FGuid HeightfieldGuid;

	/** Cached local-space bounding box, created at heightmap update time */
	UPROPERTY()
	FBox CachedLocalBox;

	/** Reference to render component */
	UPROPERTY()
	TLazyObjectPtr<ULandscapeComponent> RenderComponent;

	struct FPhysXHeightfieldRef : public FRefCountedObject
	{
		FGuid Guid;

#if WITH_PHYSX
		/** List of PxMaterials used on this landscape */
		TArray<physx::PxMaterial*> UsedPhysicalMaterialArray;
		physx::PxHeightField* RBHeightfield;
		physx::PxHeightField* RBHeightfieldSimple;
#if WITH_EDITOR
		physx::PxHeightField* RBHeightfieldEd; // Used only by landscape editor, does not have holes in it
#endif	//WITH_EDITOR
#endif	//WITH_PHYSX

		/** tors **/
		FPhysXHeightfieldRef()
#if WITH_PHYSX
			: RBHeightfield(nullptr)
			, RBHeightfieldSimple(nullptr)
#if WITH_EDITOR
			, RBHeightfieldEd(nullptr)
#endif	//WITH_EDITOR
#endif	//WITH_PHYSX
		{
		}

		FPhysXHeightfieldRef(FGuid& InGuid)
			: Guid(InGuid)
#if WITH_PHYSX
			, RBHeightfield(nullptr)
			, RBHeightfieldSimple(nullptr)
#if WITH_EDITOR
			, RBHeightfieldEd(nullptr)
#endif	//WITH_EDITOR
#endif	//WITH_PHYSX
		{
		}

		virtual ~FPhysXHeightfieldRef();
	};
	
#if WITH_EDITORONLY_DATA
	/** The collision height values. Stripped from cooked content */
	FWordBulkData								CollisionHeightData;

	/** Indices into the ComponentLayers array for the per-vertex dominant layer. Stripped from cooked content */
	FByteBulkData								DominantLayerData;

	/*  Cooked editor specific heightfield data, never serialized  */
	TArray<uint8>								CookedCollisionDataEd;

	/** 
	 *	Flag to indicate that the next time we cook data, we should save it to the DDC.
	 *	Used to ensure DDC is populated when loading content for the first time. 
	 *  For editor and full version of collision objects
	 */
	mutable bool								bShouldSaveCookedDataToDDC[2];

	/**
	  * Async DCC load for cooked collision representation. We speculatively
	  * load this to remove hitch when streaming 
	  */
	mutable TSharedPtr<FAsyncPreRegisterDDCRequest>	SpeculativeDDCRequest;

#endif //WITH_EDITORONLY_DATA

	/** 
	 *	Cooked HeightField data. Serialized only with cooked content 
	 *	Stored as array instead of BulkData to take advantage of precaching during async loading
	 */
	TArray<uint8>								CookedCollisionData;
	
	/** This is a list of physical materials that is actually used by a cooked HeightField */
	UPROPERTY()
	TArray<UPhysicalMaterial*>					CookedPhysicalMaterials;
	
	/** Physics engine version of heightfield data. */
	TRefCountPtr<FPhysXHeightfieldRef>	HeightfieldRef;
	
	/** Cached PxHeightFieldSamples values for navmesh generation. Note that it's being used only if navigation octree is set up for lazy geometry exporting */
	int32 HeightfieldRowsCount;
	int32 HeightfieldColumnsCount;
	mutable FNavHeightfieldSamples CachedHeightFieldSamples;

	enum ECollisionQuadFlags : uint8
	{
		QF_PhysicalMaterialMask = 63,	// Mask value for the physical material index, stored in the lower 6 bits.
		QF_EdgeTurned = 64,				// This quad's diagonal has been turned.
		QF_NoCollision = 128,			// This quad has no collision.
	};

	//~ Begin UActorComponent Interface.
protected:
	virtual void OnCreatePhysicsState() override;
public:
	virtual void ApplyWorldOffset(const FVector& InOffset, bool bWorldShift) override;
	//~ End UActorComponent Interface.

	//~ Begin USceneComponent Interface.
	virtual void DestroyComponent(bool bPromoteChildren = false) override;
	virtual FBoxSphereBounds CalcBounds(const FTransform &BoundTransform) const override;

	virtual ECollisionEnabled::Type GetCollisionEnabled() const override;
	virtual ECollisionResponse GetCollisionResponseToChannel(ECollisionChannel Channel) const override;
	virtual ECollisionChannel GetCollisionObjectType() const override;
	virtual const FCollisionResponseContainer& GetCollisionResponseToChannels() const override;
	//~ End USceneComponent Interface.

	//~ Begin UPrimitiveComponent Interface
	virtual bool DoCustomNavigableGeometryExport(FNavigableGeometryExport& GeomExport) const override;
#if WITH_EDITOR
	virtual bool ComponentIsTouchingSelectionBox(const FBox& InSelBBox, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const override;
	virtual bool ComponentIsTouchingSelectionFrustum(const FConvexVolume& InFrustum, const FEngineShowFlags& ShowFlags, const bool bConsiderOnlyBSP, const bool bMustEncompassEntireComponent) const override;
#endif
	//End UPrimitiveComponent interface

	//~ Begin INavRelevantInterface Interface
	virtual bool SupportsGatheringGeometrySlices() const override { return true; }
	virtual void GatherGeometrySlice(FNavigableGeometryExport& GeomExport, const FBox& SliceBox) const override;
	virtual ENavDataGatheringMode GetGeometryGatheringMode() const override;
	virtual void PrepareGeometryExportSync() override;
	//~ End INavRelevantInterface Interface

	//~ Begin UObject Interface.
	virtual void Serialize(FArchive& Ar) override;
	virtual void BeginDestroy() override;
	virtual void PostLoad() override;
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
#if WITH_EDITOR
	virtual void ExportCustomProperties(FOutputDevice& Out, uint32 Indent) override;
	virtual void ImportCustomProperties(const TCHAR* SourceText, FFeedbackContext* Warn) override;
	virtual void PostEditImport() override;
	virtual void PostEditUndo() override;
	//~ End UObject Interface.

	/** Gets the landscape info object for this landscape */
	ULandscapeInfo* GetLandscapeInfo() const;

	/**  We speculatively async load collision object from DDC to remove hitch when streaming */
	void SpeculativelyLoadAsyncDDCCollsionData();

	/** 
	 * Cooks raw height data into collision object binary stream
	 */
	virtual bool CookCollisionData(const FName& Format, bool bUseOnlyDefMaterial, bool bCheckDDC, TArray<uint8>& OutCookedData, TArray<UPhysicalMaterial*>& InOutMaterials) const;

	/** Modify a sub-region of the PhysX heightfield. Note that this does not update the physical material */
	void UpdateHeightfieldRegion(int32 ComponentX1, int32 ComponentY1, int32 ComponentX2, int32 ComponentY2);
#endif

	/** Creates collision object from a cooked collision data */
	virtual void CreateCollisionObject();

	/** Return the landscape actor associated with this component. */
	LANDSCAPE_API ALandscapeProxy* GetLandscapeProxy() const;

	/** @return Component section base as FIntPoint */
	LANDSCAPE_API FIntPoint GetSectionBase() const; 

	/** @param InSectionBase new section base for a component */
	LANDSCAPE_API void SetSectionBase(FIntPoint InSectionBase);

	/** Recreate heightfield and restart physics */
	LANDSCAPE_API virtual void RecreateCollision();

#if WITH_EDITORONLY_DATA
	// Called from editor code to manage foliage instances on landscape.
	LANDSCAPE_API void SnapFoliageInstances(const FBox& InInstanceBox);
#endif
};



