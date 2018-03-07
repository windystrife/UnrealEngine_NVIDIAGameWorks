// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Textures/SlateIcon.h"
#include "EditorUndoClient.h"
#include "IPersonaPreviewScene.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "Engine/WindDirectionalSource.h"

class IEditableSkeleton;
class IPersonaToolkit;
class UAnimSequence;

class FAnimationEditorPreviewScene : public IPersonaPreviewScene, public FEditorUndoClient
{
public:
	FAnimationEditorPreviewScene(const ConstructionValues& CVS, const TSharedRef<class IEditableSkeleton>& InEditableSkeleton, const TSharedRef<class IPersonaToolkit>& InPersonaToolkit);

	~FAnimationEditorPreviewScene();

	/** IPersonaPreviewScene interface */
	virtual TSharedRef<class IPersonaToolkit> GetPersonaToolkit() const override { return PersonaToolkit.Pin().ToSharedRef(); }
	virtual void SetPreviewAnimationAsset(UAnimationAsset* AnimAsset, bool bEnablePreview = true) override;
	virtual UAnimationAsset* GetPreviewAnimationAsset() const override;
	virtual void SetPreviewMesh(USkeletalMesh* NewPreviewMesh) override;
	virtual USkeletalMesh* GetPreviewMesh() const override;
	virtual bool AttachObjectToPreviewComponent(UObject* Object, FName AttachTo) override;
	virtual void RemoveAttachedObjectFromPreviewComponent(UObject* Object, FName AttachedTo) override;
	virtual void InvalidateViews() override;
	virtual void FocusViews() override;
	virtual UDebugSkelMeshComponent* GetPreviewMeshComponent() const override { return SkeletalMeshComponent; }
	virtual void SetPreviewMeshComponent(UDebugSkelMeshComponent* InSkeletalMeshComponent) override { SkeletalMeshComponent = InSkeletalMeshComponent; }
	virtual void SetAdditionalMeshes(class UDataAsset* InAdditionalMeshes) override;
	virtual void RefreshAdditionalMeshes() override;
	virtual void ShowReferencePose(bool bReferencePose) override;
	virtual bool IsShowReferencePoseEnabled() const override;
	virtual void SetSelectedBone(const FName& BoneName) override;
	virtual void ClearSelectedBone() override;
	virtual void SetSelectedSocket(const FSelectedSocketInfo& SocketInfo) override;
	virtual void ClearSelectedSocket() override;
	virtual void SetSelectedActor(AActor* InActor) override;
	virtual void ClearSelectedActor() override;
	virtual void DeselectAll() override;

	virtual void RegisterOnAnimChanged(const FOnAnimChanged& Delegate) override
	{
		OnAnimChanged.Add(Delegate);
	}

	virtual void UnregisterOnAnimChanged(void* Thing) override
	{
		OnAnimChanged.RemoveAll(Thing);
	}

	virtual void RegisterOnPreviewMeshChanged(const FOnPreviewMeshChanged& Delegate) override
	{
		OnPreviewMeshChanged.Add(Delegate);
	}

	virtual void UnregisterOnPreviewMeshChanged(void* Thing) override
	{
		OnPreviewMeshChanged.RemoveAll(Thing);
	}

	virtual void RegisterOnLODChanged(const FSimpleDelegate& Delegate) override
	{
		OnLODChanged.Add(Delegate);
	}

	virtual void UnregisterOnLODChanged(void* Thing) override
	{
		OnLODChanged.RemoveAll(Thing);
	}

	virtual void RegisterOnInvalidateViews(const FSimpleDelegate& Delegate) override
	{
		OnInvalidateViews.Add(Delegate);
	}

	virtual void UnregisterOnInvalidateViews(void* Thing) override
	{
		OnInvalidateViews.RemoveAll(Thing);
	}

	virtual void RegisterOnFocusViews(const FSimpleDelegate& Delegate) override
	{
		OnFocusViews.Add(Delegate);
	}

	virtual void UnregisterOnFocusViews(void* Thing) override
	{
		OnFocusViews.RemoveAll(Thing);
	}

	virtual void RegisterOnMeshClick(const FOnMeshClick& Delegate) override
	{
		OnMeshClick.Add(Delegate);
	}

	virtual void UnregisterOnMeshClick(void* Thing) override
	{
		OnMeshClick.RemoveAll(Thing);
	}

	virtual void SetDefaultAnimationMode(EPreviewSceneDefaultAnimationMode Mode, bool bShowNow) override;
	virtual void ShowDefaultMode() override;
	virtual void EnableWind(bool bEnableWind) override;
	virtual bool IsWindEnabled() const override;
	virtual void SetWindStrength(float InWindStrength) override;
	virtual float GetWindStrength() const override;
	virtual void SetGravityScale(float InGravityScale) override;
	virtual float GetGravityScale() const override;
	virtual AActor* GetSelectedActor() const override;
	virtual FSelectedSocketInfo GetSelectedSocket() const override;
	virtual int32 GetSelectedBoneIndex() const override;
	virtual void TogglePlayback() override;
	virtual AActor* GetActor() const override;
	virtual void SetActor(AActor* InActor) override;
	virtual bool AllowMeshHitProxies() const override;
	virtual void SetAllowMeshHitProxies(bool bState) override;

	virtual void RegisterOnSelectedLODChanged(const FOnSelectedLODChanged &Delegate) override
	{
		OnSelectedLODChanged.Add(Delegate);
	}
	
	virtual void UnRegisterOnSelectedLODChanged(void* Thing) override
	{
		OnSelectedLODChanged.RemoveAll(Thing);
	}
	
	virtual void BroadcastOnSelectedLODChanged() override
	{
		if (OnSelectedLODChanged.IsBound())
		{
			OnSelectedLODChanged.Broadcast();
		}
	}

	/** FPreviewScene interface */
	virtual void Tick(float InDeltaTime) override;
	virtual void AddComponent(class UActorComponent* Component, const FTransform& LocalToWorld, bool bAttachToRoot = false) override;
	virtual void RemoveComponent(class UActorComponent* Component) override;

	/** FEditorUndoClient interface */
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

	/** FGCObject interface */
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;

	/** Validate preview attached assets on skeleton and supplied skeletal mesh, notifying user if any are removed */
	void ValidatePreviewAttachedAssets(USkeletalMesh* PreviewSkeletalMesh);

	/** Get the bounds of the floor */
	const FBoxSphereBounds& GetFloorBounds() const { return FloorBounds; }

	/** Set the floor location */
	void SetFloorLocation(const FVector& InPosition);

	/** Get tooltip text for the preview asset button */
	FText GetPreviewAssetTooltip(bool bEditingAnimBlueprint) const;

	/** Get the preview scene description */
	class UPersonaPreviewSceneDescription* GetPreviewSceneDescription() const { return PreviewSceneDescription; }

	/** Begin recording animation form the preview component **/
	void RecordAnimation();

	/** Check whether recording of the preview component is available */
	bool IsRecordAvailable() const;

	/** Get a status image to display for recording in progress */
	FSlateIcon GetRecordStatusImage() const;

	/** Get a tooltip to display while recording */
	FText GetRecordStatusTooltip() const;

	/** Get a the label to display while recording */
	FText GetRecordStatusLabel() const;

	/** Get a the menu label to display while recording */
	FText GetRecordMenuLabel() const;

	/** Check whether this Persona instance is recording */
	bool IsRecording() const;

	/** Stop recording in this Persona instance */
	void StopRecording();

	/** Get the currently recording animation */
	UAnimSequence* GetCurrentRecording() const;

	/** Get the currently recording animation time */
	float GetCurrentRecordingTime() const;

	/** Get the currently selected actor */
	AActor* GetSelectedActor() { return SelectedActor.Get(); }

	/** Get the currently selected socket */
	FSelectedSocketInfo GetSelectedSocket() { return SelectedSocket; }

	/** Get the currently bone index */
	int32 GetSelectedBoneIndex() { return SelectedBoneIndex; }

	/** Broadcasts that a mesh viewport click occurred */
	virtual bool BroadcastMeshClick(HActor* HitProxy, const FViewportClick& Click) {
		OnMeshClick.Broadcast(HitProxy, Click); return OnMeshClick.IsBound(); }

private:
	/** Set preview mesh internal use only. The mesh should be verified by now. */
	void SetPreviewMeshInternal(USkeletalMesh* NewPreviewMesh);

	/** Adds to the viewport all the attached preview objects that the current skeleton and mesh contains */
	void AddPreviewAttachedObjects();

	/** Destroy the supplied component (and its children) */
	void CleanupComponent(USceneComponent* Component);

	/** 
	 * Removes attached components from the preview component. (WARNING: There is a possibility that this function will
	 * remove the wrong component if 2 of the same type (same UObject) are attached at the same location!
	 *
	 * @param bRemovePreviewAttached	Specifies whether all attached components are removed or just the ones
	 *									that haven't come from the target skeleton.
	 */
	void RemoveAttachedComponent(bool bRemovePreviewAttached = true);

	/** Create an actor used to simulate wind (useful for cloth) */
	TWeakObjectPtr<class AWindDirectionalSource> CreateWindActor(UWorld* World);

	TSharedRef<class IEditableSkeleton> GetEditableSkeleton() const { return EditableSkeletonPtr.Pin().ToSharedRef(); }

private:
	/** The one and only actor we have */
	AActor* Actor;

	/** The main preview skeletal mesh component */
	UDebugSkelMeshComponent*			SkeletalMeshComponent;

	/** Array of loaded additional meshes */
	TArray<USkeletalMeshComponent*>		AdditionalMeshes;

	/** The editable skeleton we are viewing/editing */
	TWeakPtr<class IEditableSkeleton> EditableSkeletonPtr;

	/** The persona toolkit we are embedded in */
	TWeakPtr<class IPersonaToolkit> PersonaToolkit;

	/** Cached bounds of the floor mesh */
	FBoxSphereBounds FloorBounds;

	/** Preview asset cached so we can re-apply it when reverting from ref pose */
	TWeakObjectPtr<UObject> CachedPreviewAsset;

	/** Delegate to be called after the preview animation has been changed */
	FOnAnimChangedMulticaster OnAnimChanged;

	/** Broadcasts whenever the preview mesh changes */
	FOnPreviewMeshChangedMulticaster OnPreviewMeshChanged;

	/** Mode that the preview scene defaults to (usually depending on asset editor context) */
	EPreviewSceneDefaultAnimationMode DefaultMode;

	/** Broadcasts whenever the preview mesh is clicked */
	FOnMeshClickMulticaster OnMeshClick;

	/** Configuration object for editing in details panels */
	class UPersonaPreviewSceneDescription* PreviewSceneDescription;

	/** Previous information of a wind actor */
	FVector PrevWindLocation;
	FRotator PrevWindRotation;
	float PrevWindStrength;

	/** Reference to the wind actor */
	TWeakObjectPtr<AWindDirectionalSource> WindSourceActor;

	/** The gravity scale */
	float GravityScale;

	/** The selected actor */
	TWeakObjectPtr<AActor> SelectedActor;

	/** Selected bone */
	int32 SelectedBoneIndex;
	
	/** Selected socket */
	FSelectedSocketInfo SelectedSocket;

	/** LOD index cached & used to check for broadcasting OnLODChanged delegate */
	int32 LastCachedLODForPreviewComponent;

	/** LOD changed delegate */
	FSimpleMulticastDelegate OnLODChanged;

	/** View invalidation delegate */
	FSimpleMulticastDelegate OnInvalidateViews;

	/** View focus delegate */
	FSimpleMulticastDelegate OnFocusViews;

	/** Whether or not mesh section hit proxies should be enabled or not */
	bool bEnableMeshHitProxies;

	/* Selected LOD changed delegate */
	FOnSelectedLODChangedMulticaster OnSelectedLODChanged;
};
