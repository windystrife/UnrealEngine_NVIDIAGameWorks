// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/SceneComponent.h"
#include "SceneTypes.h"
#include "ShowFlags.h"
#include "Engine/GameViewportClient.h"
#include "SceneCaptureComponent.generated.h"

class AActor;
class FSceneViewStateInterface;
class UPrimitiveComponent;

/** View state needed to create a scene capture renderer */
struct FSceneCaptureViewInfo
{
	FVector ViewLocation;
	FMatrix ViewRotationMatrix;
	FMatrix ProjectionMatrix;
	FIntRect ViewRect;
	EStereoscopicPass StereoPass;
};

USTRUCT(BlueprintType)
struct FEngineShowFlagsSetting
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SceneCapture)
	FString ShowFlagName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SceneCapture)
	bool Enabled;


	bool operator == (const FEngineShowFlagsSetting& Other) const
	{
		return ShowFlagName == Other.ShowFlagName && Other.Enabled == Enabled;
	}
};

UENUM()
enum class ESceneCapturePrimitiveRenderMode : uint8
{
	/** Legacy */
	PRM_LegacySceneCapture UMETA(DisplayName = "Render Scene Primitives"),
	/** Render primitives in the scene, minus HiddenActors. */
	PRM_RenderScenePrimitives UMETA(DisplayName = "Render Scene Primitives"),
	/** Render only primitives in the ShowOnlyActors list, or components specified with ShowOnlyComponent(). */
	PRM_UseShowOnlyList UMETA(DisplayName = "Use ShowOnly List")
};

	// -> will be exported to EngineDecalClasses.h
UCLASS(hidecategories=(abstract, Collision, Object, Physics, SceneComponent, Mobility))
class ENGINE_API USceneCaptureComponent : public USceneComponent
{
	GENERATED_UCLASS_BODY()

	/** Controls what primitives get rendered into the scene capture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SceneCapture)
	ESceneCapturePrimitiveRenderMode PrimitiveRenderMode;

	/** The components won't rendered by current component.*/
 	UPROPERTY()
 	TArray<TWeakObjectPtr<UPrimitiveComponent> > HiddenComponents;

	/** The actors to hide in the scene capture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SceneCapture)
	TArray<AActor*> HiddenActors;

	/** The only components to be rendered by this scene capture, if PrimitiveRenderMode is set to UseShowOnlyList. */
 	UPROPERTY()
 	TArray<TWeakObjectPtr<UPrimitiveComponent> > ShowOnlyComponents;

	/** The only actors to be rendered by this scene capture, if PrimitiveRenderMode is set to UseShowOnlyList.*/
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SceneCapture)
	TArray<AActor*> ShowOnlyActors;

	/** Whether to update the capture's contents every frame.  If disabled, the component will render once on load and then only when moved. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SceneCapture)
	bool bCaptureEveryFrame;

	/** Whether to update the capture's contents on movement.  Disable if you are going to capture manually from blueprint. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SceneCapture)
	bool bCaptureOnMovement;
	
	/** Whether to persist the rendering state even if bCaptureEveryFrame==false.  This allows velocities for Motion Blur and Temporal AA to be computed. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SceneCapture, meta = (editcondition = "!bCaptureEveryFrame"))
	bool bAlwaysPersistRenderingState;

	/** Scales the distance used by LOD. Set to values greater than 1 to cause the scene capture to use lower LODs than the main view to speed up the scene capture pass. */
	UPROPERTY(EditAnywhere, Category=PlanarReflection, meta=(UIMin = ".1", UIMax = "10"), AdvancedDisplay)
	float LODDistanceFactor;

	/** if > 0, sets a maximum render distance override.  Can be used to cull distant objects from a reflection if the reflecting plane is in an enclosed area like a hallway or room */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=SceneCapture, meta=(UIMin = "100", UIMax = "10000"))
	float MaxViewDistanceOverride;

	/** Capture priority within the frame to sort scene capture on GPU to resolve interdependencies between multiple capture components. Highest come first. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category=SceneCapture)
	int32 CaptureSortPriority;

	/** ShowFlags for the SceneCapture's ViewFamily, to control rendering settings for this view. Hidden but accessible through details customization */
	UPROPERTY(EditAnywhere, interp, Category=SceneCapture)
	TArray<struct FEngineShowFlagsSetting> ShowFlagSettings;

	// NVCHANGE_BEGIN: Add VXGI
	/** Whether to enable VXGI diffuse and specular tracing for this capture. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = SceneCapture)
	bool bEnableVxgi;
	// NVCHANGE_END: Add VXGI

	// TODO: Make this a UStruct to set directly?
	/** Settings stored here read from the strings and int values in the ShowFlagSettings array */
	FEngineShowFlags ShowFlags;
public:
	/** Indicates which stereo pass this component is capturing for, if any */
    EStereoscopicPass CaptureStereoPass;

	//~ Begin UActorComponent Interface
	virtual void OnRegister() override;
	//~ End UActorComponent Interface

	/** Adds the component to our list of hidden components. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|SceneCapture")
	void HideComponent(UPrimitiveComponent* InComponent);

	/** Adds all primitive components in the actor to our list of hidden components. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|SceneCapture")
	void HideActorComponents(AActor* InActor);

	/** Adds the component to our list of show-only components. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|SceneCapture")
	void ShowOnlyComponent(UPrimitiveComponent* InComponent);

	/** Adds all primitive components in the actor to our list of show-only components. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|SceneCapture")
	void ShowOnlyActorComponents(AActor* InActor);

	/** Removes a component from the Show Only list. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|SceneCapture")
	void RemoveShowOnlyComponent(UPrimitiveComponent* InComponent);

	/** Removes a actor's components from the Show Only list. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|SceneCapture")
	void RemoveShowOnlyActorComponents(AActor* InActor);

	/** Clears the Show Only list. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|SceneCapture")
	void ClearShowOnlyComponents(UPrimitiveComponent* InComponent);

	/** Clears the hidden list. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|SceneCapture")
	void ClearHiddenComponents();

	/** Changes the value of TranslucentSortPriority. */
	UFUNCTION(BlueprintCallable, Category = "Rendering|SceneCapture")
	void SetCaptureSortPriority(int32 NewCaptureSortPriority);

	/** Returns the view state, if any, and allocates one if needed. This function can return NULL, e.g. when bCaptureEveryFrame is false. */
	FSceneViewStateInterface* GetViewState(int32 ViewIndex);

#if WITH_EDITOR
	virtual bool CanEditChange(const UProperty* InProperty) const override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif	

	virtual void Serialize(FArchive& Ar);

	virtual void OnUnregister() override;

	/** To leverage a component's bOwnerNoSee/bOnlyOwnerSee properties, the capture view requires an "owner". Override this to set a "ViewActor" for the scene. */
	virtual const AActor* GetViewOwner() const { return nullptr; }

	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	static void UpdateDeferredCaptures(FSceneInterface* Scene);

protected:
	/** Update the show flags from our show flags settings (ideally, you'd be able to set this more directly, but currently unable to make FEngineShowFlags a UStruct to use it as a UProperty...) */
	void UpdateShowFlags();

	virtual void UpdateSceneCaptureContents(FSceneInterface* Scene) {};

	/**
	 * The view state holds persistent scene rendering state and enables occlusion culling in scene captures.
	 * NOTE: This object is used by the rendering thread. When the game thread attempts to destroy it, FDeferredCleanupInterface will keep the object around until the RT is done accessing it.
	 */
	TArray<FSceneViewStateReference> ViewStates;
};

