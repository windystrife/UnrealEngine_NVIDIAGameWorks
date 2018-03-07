// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "EngineDefines.h"
#include "Components/SkeletalMeshComponent.h"
#include "DebugSkelMeshComponent.generated.h"

class Error;

USTRUCT()
struct FSelectedSocketInfo
{
	GENERATED_USTRUCT_BODY()

	/** Default constructor */
	FSelectedSocketInfo()
		: Socket(nullptr)
		, bSocketIsOnSkeleton( false )
	{

	}

	/** Constructor */
	FSelectedSocketInfo( class USkeletalMeshSocket* InSocket, bool bInSocketIsOnSkeleton )
		: Socket( InSocket )
		, bSocketIsOnSkeleton( bInSocketIsOnSkeleton )
	{

	}

	bool IsValid() const
	{
		return Socket != nullptr;
	}

	void Reset()
	{
		Socket = nullptr;
	}

	/** The socket we have selected */
	class USkeletalMeshSocket* Socket;

	/** true if on skeleton, false if on mesh */
	bool bSocketIsOnSkeleton;
};

/** Different modes for Persona's Turn Table. */
namespace EPersonaTurnTableMode
{
	enum Type
	{
		Stopped,
		Playing,
		Paused
	};
};

//////////////////////////////////////////////////////////////////////////
// FDebugSkelMeshSceneProxy

class UDebugSkelMeshComponent;

class FDebugSkelMeshDynamicData
{
public:

	FDebugSkelMeshDynamicData(UDebugSkelMeshComponent* InComponent);

	bool bDrawMesh;
	bool bDrawNormals;
	bool bDrawTangents;
	bool bDrawBinormals;
	bool bDrawClothPaintPreview;

	bool bFlipNormal;
	bool bCullBackface;

	int32 ClothingSimDataIndexWhenPainting;
	TArray<uint32> ClothingSimIndices;

	TArray<float> ClothingVisiblePropertyValues;
	float PropertyViewMin;
	float PropertyViewMax;

	float ClothMeshOpacity;

	TArray<FVector> SkinnedPositions;
	TArray<FVector> SkinnedNormals;
};

/**
* A skeletal mesh component scene proxy with additional debugging options.
*/
class FDebugSkelMeshSceneProxy : public FSkeletalMeshSceneProxy
{
public:
	/**
	* Constructor.
	* @param	Component - skeletal mesh primitive being added
	*/
	FDebugSkelMeshSceneProxy(const UDebugSkelMeshComponent* InComponent, FSkeletalMeshResource* InSkelMeshResource, const FColor& InWireframeOverlayColor = FColor::White);

	virtual ~FDebugSkelMeshSceneProxy()
	{}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

	FDebugSkelMeshDynamicData* DynamicData;

	uint32 GetAllocatedSize() const
	{
		return FSkeletalMeshSceneProxy::GetAllocatedSize();
	}

	virtual uint32 GetMemoryFootprint() const override
	{
		return sizeof(*this) + GetAllocatedSize();
	}
};

UCLASS(transient)
class UNREALED_API UDebugSkelMeshComponent : public USkeletalMeshComponent
{
	GENERATED_UCLASS_BODY()

	/** If true, render a wireframe skeleton of the mesh animated with the raw (uncompressed) animation data. */
	UPROPERTY()
	uint32 bRenderRawSkeleton:1;

	/** Holds onto the bone color that will be used to render the bones of its skeletal mesh */
	//var Color		BoneColor;
	
	/** If true then the skeletal mesh associated with the component is drawn. */
	UPROPERTY()
	uint32 bDrawMesh:1;

	/** If true then the bone names associated with the skeletal mesh are displayed */
	UPROPERTY()
	uint32 bShowBoneNames:1;

	/** Bone influences viewing */
	UPROPERTY(transient)
	uint32 bDrawBoneInfluences:1;

	/** Morphtarget viewing */
	UPROPERTY(transient)
	uint32 bDrawMorphTargetVerts : 1;

	/** Vertex normal viewing */
	UPROPERTY(transient)
	uint32 bDrawNormals:1;

	/** Vertex tangent viewing */
	UPROPERTY(transient)
	uint32 bDrawTangents:1;

	/** Vertex binormal viewing */
	UPROPERTY(transient)
	uint32 bDrawBinormals:1;

	/** Socket hit points viewing */
	UPROPERTY(transient)
	uint32 bDrawSockets:1;

	/** Skeleton sockets visible? */
	UPROPERTY(transient)
	uint32 bSkeletonSocketsVisible:1;

	/** Mesh sockets visible? */
	UPROPERTY(transient)
	uint32 bMeshSocketsVisible:1;

	/** Display raw animation bone transform */
	UPROPERTY(transient)
	uint32 bDisplayRawAnimation:1;

	/** Display non retargeted animation pose */
	UPROPERTY(Transient)
	uint32 bDisplayNonRetargetedPose:1;

	/** Display additive base bone transform */
	UPROPERTY(transient)
	uint32 bDisplayAdditiveBasePose:1;

	/** Display baked animation pose */
	UPROPERTY(Transient)
	uint32 bDisplayBakedAnimation:1;

	/** Display source animation pose */
	UPROPERTY(Transient)
	uint32 bDisplaySourceAnimation:1;

	/** Display Bound **/
	UPROPERTY(transient)
	bool bDisplayBound;

	UPROPERTY(transient)
	bool bDisplayVertexColors;

	UPROPERTY(transient)
	uint32 bPreviewRootMotion:1;

	UPROPERTY(transient)
	uint32 bShowClothData : 1;

	UPROPERTY(transient)
	float MinClothPropertyView;

	UPROPERTY(transient)
	float MaxClothPropertyView;

	UPROPERTY(transient)
	float ClothMeshOpacity;

	UPROPERTY(transient)
	bool bClothFlipNormal;

	UPROPERTY(transient)
	bool bClothCullBackface;

	/* Bounds computed from cloth. */
	FBoxSphereBounds CachedClothBounds;

	/** Non Compressed SpaceBases for when bDisplayRawAnimation == true **/
	TArray<FTransform> UncompressedSpaceBases;

	/** Storage of Additive Base Pose for when bDisplayAdditiveBasePose == true, as they have to be calculated */
	TArray<FTransform> AdditiveBasePoses;

	/** Storage for non retargeted pose. */
	TArray<FTransform> NonRetargetedSpaceBases;

	/** Storage of Baked Animation Pose for when bDisplayBakedAnimation == true, as they have to be calculated */
	TArray<FTransform> BakedAnimationPoses;

	/** Storage of Source Animation Pose for when bDisplaySourceAnimation == true, as they have to be calculated */
	TArray<FTransform> SourceAnimationPoses;
	
	/** Array of bones to render bone weights for */
	UPROPERTY(transient)
	TArray<int32> BonesOfInterest;

	/** Array of morphtargets to render verts for */
	UPROPERTY(transient)
	TArray<class UMorphTarget*> MorphTargetOfInterests;

	/** Array of materials to restore when not rendering blend weights */
	UPROPERTY(transient)
	TArray<class UMaterialInterface*> SkelMaterials;
	
	UPROPERTY(transient, NonTransactional)
	class UAnimPreviewInstance* PreviewInstance;

	UPROPERTY(transient)
	class UAnimInstance* SavedAnimScriptInstance;

	/** Does this component use in game bounds or does it use bounds calculated from bones */
	UPROPERTY(transient)
	bool bIsUsingInGameBounds;
	
	/** Base skel mesh has support for suspending clothing, but single ticks are more of a debug feature when stepping through an animation
	 *  So we control that using this flag
	 */
	UPROPERTY(transient)
	bool bPerformSingleClothingTick;

	UPROPERTY(transient)
	bool bPauseClothingSimulationWithAnim;

	//~ Begin USceneComponent Interface.
	virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
	//~ End USceneComponent Interface.

	//~ Begin UPrimitiveComponent Interface.
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	
	// engine only draw bounds IF selected
	// @todo fix this properly
	// this isn't really the best way to do this, but for now
	// we'll just mark as selected
	virtual bool ShouldRenderSelected() const override;
	//~ End UPrimitiveComponent Interface.

	//~ Begin SkinnedMeshComponent Interface
	virtual bool ShouldCPUSkin() override;
	virtual void PostInitMeshObject(class FSkeletalMeshObject* MeshObject) override;
	virtual void RefreshBoneTransforms(FActorComponentTickFunction* TickFunction = NULL) override;
	virtual int32 GetLODBias() const override { return 0; }
	//~ End SkinnedMeshComponent Interface

	//~ Begin SkeletalMeshComponent Interface
	virtual void InitAnim(bool bForceReinit) override;
	virtual bool IsWindEnabled() const override { return true; }
	//~ End SkeletalMeshComponent Interface
	// Preview.
	// @todo document
	bool IsPreviewOn() const;

	// @todo document
	FString GetPreviewText() const;

	// @todo anim : you still need to give asset, so that we know which one to disable
	// we can disable per asset, so that if some other window disabled before me, I don't accidently turn it off
	void EnablePreview(bool bEnable, class UAnimationAsset * PreviewAsset);

	/**
	 * Update material information depending on color render mode 
	 * Refresh/replace materials 
	 */
	void SetShowBoneWeight(bool bNewShowBoneWeight);

	/**
	* Update material information depending on color render mode
	* Refresh/replace materials
	*/
	void SetShowMorphTargetVerts(bool bNewShowMorphTargetVerts);

	/**
	 * Does it use in-game bounds or bounds calculated from bones
	 */
	bool IsUsingInGameBounds() const;

	/**
	 * Set to use in-game bounds or bounds calculated from bones
	 */
	void UseInGameBounds(bool bUseInGameBounds);

	/**
	 * Test if in-game bounds are as big as preview bounds
	 */
	bool CheckIfBoundsAreCorrrect();

	/** 
	 * Update components position based on animation root motion
	 */
	void ConsumeRootMotion(const FVector& FloorMin, const FVector& FloorMax);

	/** Sets the flag used to determine whether or not the current active cloth sim mesh should be rendered */
	void SetShowClothProperty(bool bState);

	/** Get whether we should be previewing root motion */
	bool GetPreviewRootMotion() const;

	/** Set whether we should be previewing root motion. Note: disabling root motion preview resets transform. */
	void SetPreviewRootMotion(bool bInPreviewRootMotion);

#if WITH_EDITOR
	//TODO - This is a really poor way to post errors to the user. Work out a better way.
	struct FAnimNotifyErrors
	{
		FAnimNotifyErrors(UObject* InSourceNotify)
		: SourceNotify(InSourceNotify)
		{}
		UObject* SourceNotify;
		TArray<FString> Errors;
	};
	TArray<FAnimNotifyErrors> AnimNotifyErrors;
	virtual void ReportAnimNotifyError(const FText& Error, UObject* InSourceNotify) override;
	virtual void ClearAnimNotifyErrors(UObject* InSourceNotify) override;
#endif

	enum ESectionDisplayMode
	{
		None = -1,
		ShowAll,
		ShowOnlyClothSections,
		HideOnlyClothSections,
		NumSectionDisplayMode
	};
	/** Draw All/ Draw only clothing sections/ Hide only clothing sections */
	int32 SectionsDisplayMode;

	/** 
	 * toggle visibility between cloth sections and non-cloth sections for all LODs
	 * if bShowOnlyClothSections is true, shows only cloth sections. On the other hand, 
	 * if bShowOnlyClothSections is false, hides only cloth sections.
	 */
	void ToggleClothSectionsVisibility(bool bShowOnlyClothSections);
	/** Restore all section visibilities to original states for all LODs */
	void RestoreClothSectionsVisibility();

	/** 
	 * To normal game/runtime code we don't want to expose a non-const pointer to the simulation, so we can only get
	 * one from this editor-only component. Intended for debug options/visualisations/editor-only code to poke the sim
	 */
	IClothingSimulation* GetMutableClothingSimulation();

	int32 FindCurrentSectionDisplayMode();

	/** to avoid clothing reset while modifying properties in Persona */
	virtual void CheckClothTeleport() override;

	/** The currently selected asset guid if we're painting, used to build dynamic mesh to paint sim parameters */
	FGuid SelectedClothingGuidForPainting;

	/** The currently selected LOD for painting */
	int32 SelectedClothingLodForPainting;

	/** The currently selected mask inside the above LOD to be painted */
	int32 SelectedClothingLodMaskForPainting;

	void ToggleMeshSectionForCloth(FGuid InClothGuid);

	// fixes up the disabled flags so clothing is enabled and originals are disabled as
	// ToggleMeshSectionForCloth will make these get out of sync
	void ResetMeshSectionVisibility();

	// Rebuilds the fixed parameter on the mesh to mesh data, to be used if the editor has
	// changed a vert to be fixed or unfixed otherwise the simulation will not work
	void RebuildClothingSectionsFixedVerts();

	TArray<FVector> SkinnedSelectedClothingPositions;
	TArray<FVector> SkinnedSelectedClothingNormals;

private:

private:
	// Helper function to generate space bases for current frame
	void GenSpaceBases(TArray<FTransform>& OutSpaceBases);

	// Helper function to enable overlay material
	void EnableOverlayMaterial(bool bEnable);

	// Rebuilds the cloth bounds for the asset.
	void RebuildCachedClothBounds();
protected:

	// Overridden to support single clothing ticks
	virtual bool ShouldRunClothTick() const override;

	virtual void SendRenderDynamicData_Concurrent() override;

public:
	/** Current turn table mode */
	EPersonaTurnTableMode::Type TurnTableMode;
	/** Current turn table speed scaling */
	float TurnTableSpeedScaling;
	
	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	void RefreshSelectedClothingSkinnedPositions();

	virtual void GetUsedMaterials(TArray<UMaterialInterface *>& OutMaterials, bool bGetDebugMaterials = false) const override;

};
