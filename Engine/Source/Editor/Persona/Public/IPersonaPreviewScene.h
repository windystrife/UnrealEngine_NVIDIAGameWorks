// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AdvancedPreviewScene.h"

class UAnimationAsset;
class UDebugSkelMeshComponent;
class USkeletalMesh;
struct FSelectedSocketInfo;
struct HActor;
struct FViewportClick;


// called when animation asset has been changed
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAnimChangedMulticaster, UAnimationAsset*);

// anim changed 
typedef FOnAnimChangedMulticaster::FDelegate FOnAnimChanged;

// Called when the preview mesh has been changed;
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPreviewMeshChangedMulticaster, USkeletalMesh* /*OldPreviewMesh*/, USkeletalMesh* /*NewPreviewMesh*/);

// preview mesh changed 
typedef FOnPreviewMeshChangedMulticaster::FDelegate FOnPreviewMeshChanged;

DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMeshClickMulticaster, HActor*, const FViewportClick&);

typedef FOnMeshClickMulticaster::FDelegate FOnMeshClick;

//The selected LOD changed
DECLARE_MULTICAST_DELEGATE(FOnSelectedLODChangedMulticaster);
typedef FOnSelectedLODChangedMulticaster::FDelegate FOnSelectedLODChanged;

/** Modes that the preview scene defaults to (usually depending on asset editor context) */
enum class EPreviewSceneDefaultAnimationMode : int32
{
	ReferencePose,

	Animation,

	AnimationBlueprint,
};

class IPersonaPreviewScene : public FAdvancedPreviewScene
{
public:
	/** Constructor only here to pass ConstructionValues to base constructor */
	IPersonaPreviewScene(ConstructionValues CVS)
		: FAdvancedPreviewScene(CVS)
	{}

	/** Get the persona toolkit we are associated with */
	virtual TSharedRef<class IPersonaToolkit> GetPersonaToolkit() const = 0;

	/** Flag that we want our views to be updated */
	virtual void InvalidateViews() = 0;

	/** Request our views to focus on the current item */
	virtual void FocusViews() = 0;

	/** Get the skeletal mesh component we are using for preview, if any. */
	virtual UDebugSkelMeshComponent* GetPreviewMeshComponent() const = 0;

	/** Set the skeletal mesh component we are going to preview. */
	virtual void SetPreviewMeshComponent(UDebugSkelMeshComponent* InSkeletalMeshComponent) = 0;

	/** Set the additional meshes used by this preview scene (sets the additional meshes on the skeleton) */
	virtual void SetAdditionalMeshes(class UDataAsset* InAdditionalMeshes) = 0;

	/** Refreshes the additional meshes displayed in this preview scene */
	virtual void RefreshAdditionalMeshes() = 0;

	/** Set the animation asset to preview **/
	virtual void SetPreviewAnimationAsset(UAnimationAsset* AnimAsset, bool bEnablePreview = true) = 0;

	/** Get the animation asset we are previewing */
	virtual UAnimationAsset* GetPreviewAnimationAsset() const = 0;

	/** Set the preview mesh for this scene (does not set the preview mesh on the skeleton/asset) */
	virtual void SetPreviewMesh(USkeletalMesh* NewPreviewMesh) = 0;

	/** Get the preview mesh for this scene (does go via skeleton/asset) */
	virtual USkeletalMesh* GetPreviewMesh() const = 0;

	/** Show the reference pose of the displayed skeletal mesh. Otherwise display the default. */
	virtual void ShowReferencePose(bool bReferencePose) = 0;

	/* Are we currently displaying the ref pose */
	virtual bool IsShowReferencePoseEnabled() const = 0;

	/** Attaches an object to the preview component using the supplied attach name, returning whether it was successfully attached or not */
	virtual bool AttachObjectToPreviewComponent(UObject* Object, FName AttachTo) = 0;

	/** Removes a currently attached object from the preview component */
	virtual void RemoveAttachedObjectFromPreviewComponent(UObject* Object, FName AttachedTo) = 0;

	/** Sets the selected bone on the preview component */
	virtual void SetSelectedBone(const FName& BoneName) = 0;

	/** Clears the selected bone on the preview component */
	virtual void ClearSelectedBone() = 0;

	/** Sets the selected socket on the preview component */
	virtual void SetSelectedSocket(const FSelectedSocketInfo& SocketInfo) = 0;

	/** Clears the selected socket on the preview component */
	virtual void ClearSelectedSocket() = 0;

	/** Sets the selected actor */
	virtual void SetSelectedActor(AActor* InActor) = 0;

	/** Clears the selected actor */
	virtual void ClearSelectedActor() = 0;

	/** Clears all selection on the preview component */
	virtual void DeselectAll() = 0;

	/** Registers a delegate to be called after the preview animation has been changed */
	virtual void RegisterOnAnimChanged(const FOnAnimChanged& Delegate) = 0;

	/** Unregisters a delegate to be called after the preview animation has been changed */
	virtual void UnregisterOnAnimChanged(void* Thing) = 0;

	/** Registers a delegate to be called when the preview mesh is changed */
	virtual void RegisterOnPreviewMeshChanged(const FOnPreviewMeshChanged& Delegate) = 0;

	/** Unregisters a delegate to be called when the preview mesh is changed */
	virtual void UnregisterOnPreviewMeshChanged(void* Thing) = 0;

	/** Registers a delegate to be called when the preview mesh's LOD has changed */
	virtual void RegisterOnLODChanged(const FSimpleDelegate& Delegate) = 0;

	/** Unregisters a delegate to be called when the preview mesh's LOD has changed */
	virtual void UnregisterOnLODChanged(void* Thing) = 0;

	/** Registers a delegate to be called when the view is invalidated */
	virtual void RegisterOnInvalidateViews(const FSimpleDelegate& Delegate) = 0;

	/** Unregisters a delegate to be called when the view is invalidated */
	virtual void UnregisterOnInvalidateViews(void* Thing) = 0;

	/** Registers a delegate to be called when the view should be focused */
	virtual void RegisterOnFocusViews(const FSimpleDelegate& Delegate) = 0;

	/** Unregisters a delegate to be called when the view should be focused */
	virtual void UnregisterOnFocusViews(void* Thing) = 0;

	/** Registers a delegate to be called when the preview mesh is clicked */
	virtual void RegisterOnMeshClick(const FOnMeshClick& Delegate) = 0;

	/** Unregisters a delegate to be called when the preview mesh is clicked */
	virtual void UnregisterOnMeshClick(void* Thing) = 0;

	/** Broadcasts that the preview mesh was clicked */
	virtual bool BroadcastMeshClick(HActor* HitProxy, const FViewportClick& Click) = 0;

	/** Set the default mode this preview scene appears in. Optionally show the default mode. */
	virtual void SetDefaultAnimationMode(EPreviewSceneDefaultAnimationMode Mode, bool bShowNow = true) = 0;

	/** Show the mode specifed by SetDefaultAnimationMode() */
	virtual void ShowDefaultMode() = 0;

	/** Enable wind. Useful when simulating cloth. */
	virtual void EnableWind(bool bEnableWind) = 0;

	/** Check whether wind is enabled */
	virtual bool IsWindEnabled() const = 0;

	/** Set the wind strength */
	virtual void SetWindStrength(float InWindStrength) = 0;

	/** Get the wind strength */
	virtual float GetWindStrength() const = 0;

	/** Set the gravity scale */
	virtual void SetGravityScale(float InGravityScale) = 0;

	/** Get the gravity scale */
	virtual float GetGravityScale() const = 0;

	/** Get the currently selected actor */
	virtual AActor* GetSelectedActor() const = 0;

	/** Get the currently selected socket */
	virtual FSelectedSocketInfo GetSelectedSocket() const = 0;

	/** Get the currently selected bone index */
	virtual int32 GetSelectedBoneIndex() const = 0;

	/** Toggle the playback of animation, if any */
	virtual void TogglePlayback() = 0;

	/** Get the main actor */
	virtual AActor* GetActor() const = 0;

	/** Set the main actor */
	virtual void SetActor(AActor* InActor) = 0;

	/** Get whether or not to ignore mesh hit proxies */
	virtual bool AllowMeshHitProxies() const = 0;

	/** Set whether or not to ignore mesh hit proxies */
	virtual void SetAllowMeshHitProxies(bool bState) = 0;

	/** Register callback to be able to be notify when the select LOD is change */
	virtual void RegisterOnSelectedLODChanged(const FOnSelectedLODChanged &Delegate) = 0;
	/** Unregister callback to free up the ressources */
	virtual void UnRegisterOnSelectedLODChanged(void* Thing) = 0;
	/** Broadcast select LOD changed */
	virtual void BroadcastOnSelectedLODChanged() = 0;
};
