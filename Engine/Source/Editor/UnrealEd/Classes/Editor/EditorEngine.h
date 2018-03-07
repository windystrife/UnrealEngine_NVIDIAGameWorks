// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"
#include "UObject/ObjectMacros.h"
#include "UObject/UObjectGlobals.h"
#include "InputCoreTypes.h"
#include "Templates/SubclassOf.h"
#include "Engine/EngineTypes.h"
#include "GameFramework/Actor.h"
#include "AssetData.h"
#include "HAL/PlatformProcess.h"
#include "GenericPlatform/GenericApplication.h"
#include "Widgets/SWindow.h"
#include "TimerManager.h"
#include "UObject/UObjectAnnotation.h"
#include "Engine/Brush.h"
#include "Model.h"
#include "Editor/Transactor.h"
#include "Engine/Engine.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Settings/LevelEditorViewportSettings.h"
#include "EditorEngine.generated.h"

class AMatineeActor;
class APlayerStart;
class Error;
class FEditorViewportClient;
class FEditorWorldManager;
class FMessageLog;
class FOutputLogErrorsToMessageLogProxy;
class FPoly;
class FSceneViewport;
class FSceneViewStateInterface;
class FViewport;
class IEngineLoop;
class ILauncherWorker;
class ILayers;
class ILevelViewport;
class ITargetPlatform;
class SViewport;
class UActorFactory;
class UAnimSequence;
class UAudioComponent;
class UBrushBuilder;
class UFoliageType;
class UGameViewportClient;
class ULocalPlayer;
class UNetDriver;
class UPrimitiveComponent;
class USkeleton;
class USoundBase;
class USoundNode;
class UTextureRenderTarget2D;
struct FAnalyticsEventAttribute;
class UEditorWorldExtensionManager;

//
// Things to set in mapSetBrush.
//
UENUM()
enum EMapSetBrushFlags				
{
	/** Set brush color. */
	MSB_BrushColor	= 1,
	/** Set group. */
	MSB_Group		= 2,
	/** Set poly flags. */
	MSB_PolyFlags	= 4,
	/** Set CSG operation. */
	MSB_BrushType	= 8,
};

UENUM()
enum EPasteTo
{
	PT_OriginalLocation	= 0,
	PT_Here				= 1,
	PT_WorldOrigin		= 2
};

USTRUCT()
struct FSlatePlayInEditorInfo
{
	GENERATED_USTRUCT_BODY()

	/** The spawned player for updating viewport location from player when pie closes */
	TWeakObjectPtr<class ULocalPlayer>	EditorPlayer;

	/** The current play in editor SWindow if playing in a floating window */
	TWeakPtr<class SWindow>				SlatePlayInEditorWindow;
	
	/** The current play in editor rendering and I/O viewport if playing in a floating window*/
	TSharedPtr<class FSceneViewport>	SlatePlayInEditorWindowViewport;
	
	/** The slate viewport that should be used for play in viewport */
	TWeakPtr<class ILevelViewport>		DestinationSlateViewport;

	FSlatePlayInEditorInfo()
	: SlatePlayInEditorWindow(NULL), DestinationSlateViewport(NULL)
	{}
};

/**
 * Data structure for storing PIE login credentials
 */
USTRUCT()
struct FPIELoginInfo
{
public:

	GENERATED_USTRUCT_BODY()

	/** Type of account. Needed to identity the auth method to use (epic, internal, facebook, etc) */
	UPROPERTY()
	FString Type;
	/** Id of the user logging in (email, display name, facebook id, etc) */
	UPROPERTY()
	FString Id;
	/** Credentials of the user logging in (password or auth token) */
	UPROPERTY()
	FString Token;
};

/**
 * Holds various data to pass to the post login delegate for PIE logins
 */
struct FPieLoginStruct
{
	/** World context handle for this login */
	FName WorldContextHandle;
	/** Setting index for window positioning */
	int32 SettingsIndex;
	/** X location for window positioning */
	int32 NextX;
	/** Y location for window positioning */
	int32 NextY;
	/** What net mode to run this instance as */
	EPlayNetMode NetMode;
	/** Passthrough condition of blueprint compilation*/
	bool bAnyBlueprintErrors;
	/** Passthrough condition of spectator mode */
	bool bStartInSpectatorMode;
	/** Passthrough start time of PIE */
	double PIEStartTime;

	FPieLoginStruct() :
		WorldContextHandle(NAME_None),
		SettingsIndex(0),
		NextX(0),
		NextY(0),
		NetMode(EPlayNetMode::PIE_Standalone),
		bAnyBlueprintErrors(false),
		bStartInSpectatorMode(false),
		PIEStartTime(0)
	{
	}
};

USTRUCT()
struct FCopySelectedInfo
{
	GENERATED_USTRUCT_BODY()

	/** Do not cache this info, it is only valid after a call to CanCopySelectedActorsToClipboard has been made, and becomes redundant
	when the current selection changes. Used to determine whether a copy can be performed based on the current selection state */
	FCopySelectedInfo()
		: bHasSelectedActors( false )
		, bAllActorsInSameLevel( true )
		, LevelAllActorsAreIn( NULL )
		, bHasSelectedSurfaces( false )
		, LevelWithSelectedSurface( NULL )
	{}


	/** Does the current selection contain actors */
	bool bHasSelectedActors;		

	/** If we have selected actors, are they within the same level */
	bool bAllActorsInSameLevel;

	/** If they are in the same level, what level is it */
	ULevel* LevelAllActorsAreIn;


	/** Does the current selection contain surfaces */
	bool bHasSelectedSurfaces;

	/** If we have selected surfaces, what level is it */
	ULevel* LevelWithSelectedSurface;


	/** Can a quick copy be formed based on the selection information */
	bool CanPerformQuickCopy() const
	{
		// If there are selected actors and BSP surfaces AND all selected actors 
		// and surfaces are in the same level, we can do a quick copy.
		bool bCanPerformQuickCopy = false;
		if( LevelAllActorsAreIn && LevelWithSelectedSurface )
		{
			bCanPerformQuickCopy = ( LevelWithSelectedSurface == LevelAllActorsAreIn );
		}
		// Else, if either we have only selected actors all in one level OR we have 
		// only selected surfaces all in one level, then we can perform a quick copy. 
		else
		{
			bCanPerformQuickCopy = (LevelWithSelectedSurface != NULL && !bHasSelectedActors) || (LevelAllActorsAreIn != NULL && !bHasSelectedSurfaces);
		}
		return bCanPerformQuickCopy;
	}
};

/** A cache of actor labels */
struct FCachedActorLabels
{
	/** Default constructor - does not populate the array */
	UNREALED_API FCachedActorLabels();

	/** Constructor that populates the set of actor names */
	UNREALED_API explicit FCachedActorLabels(UWorld* World, const TSet<AActor*>& IgnoredActors = TSet<AActor*>());

	/** Populate the set of actor names */
	void Populate(UWorld* World, const TSet<AActor*>& IgnoredActors = TSet<AActor*>());

	/** Add a new label to this set */
	FORCEINLINE void Add(const FString& InLabel)
	{
		ActorLabels.Add(InLabel);
	}

	/** Check if the specified label exists */
	FORCEINLINE bool Contains(const FString& InLabel) const
	{
		return ActorLabels.Contains(InLabel);
	}

private:
	TSet<FString> ActorLabels;
};

/** 
 * Represents an actor or a component for use in editor functionality such as snapping which can operate on either type
 */
struct FActorOrComponent
{
	AActor* Actor;
	USceneComponent* Component;

	FActorOrComponent()
		: Actor( nullptr )
		, Component( nullptr )
	{}

	FActorOrComponent( AActor* InActor )
		: Actor( InActor )
		, Component( nullptr )
	{}

	FActorOrComponent( USceneComponent* InComponent )
		: Actor( nullptr )
		, Component( InComponent )
	{}

	UWorld* GetWorld() const
	{
		return Actor ? Actor->GetWorld() : Component->GetWorld();
	}

	bool operator==( const FActorOrComponent& Other ) const
	{
		return Actor == Other.Actor && Component == Other.Component;
	}

	const FBoxSphereBounds& GetBounds() const
	{
		return Actor ? Actor->GetRootComponent()->Bounds : Component->Bounds;
	}

	FVector GetWorldLocation() const
	{
		return Actor ? Actor->GetActorLocation() : Component->GetComponentLocation();
	}

	FRotator GetWorldRotation() const
	{
		return Actor ? Actor->GetActorRotation() : Component->GetComponentRotation();
	}

	void SetWorldLocation( const FVector& NewLocation )
	{
		if( Actor )
		{
			Actor->SetActorLocation( NewLocation );
		}
		else
		{
			Component->SetWorldLocation( NewLocation );
		}
	}

	void SetWorldRotation( const FRotator& NewRotation )
	{
		if(Actor)
		{
			Actor->SetActorRotation(NewRotation);
		}
		else
		{
			Component->SetWorldRotation(NewRotation);
		}
	}

	/** @return true if this is a valid actor or component but not both */
	bool IsValid() const { return (Actor != nullptr) ^ (Component != nullptr);}
};

/**
 * Represents the current selection state of a level (its selected actors and components) from a given point in a time, in a way that can be safely restored later even if the level is reloaded
 */
USTRUCT()
struct FSelectionStateOfLevel
{
	GENERATED_BODY()

	/** Path names of all the selected actors */
	UPROPERTY()
	TArray<FString> SelectedActors;

	/** Path names of all the selected components */
	UPROPERTY()
	TArray<FString> SelectedComponents;
};

/**
 * Engine that drives the Editor.
 * Separate from UGameEngine because it may have much different functionality than desired for an instance of a game itself.
 */
UCLASS(config=Engine, transient)
class UNREALED_API UEditorEngine : public UEngine
{
public:
	GENERATED_BODY()

public:
	UEditorEngine(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());

	// Objects.
	UPROPERTY()
	class UModel* TempModel;

	UPROPERTY()
	class UModel* ConversionTempModel;

	UPROPERTY()
	class UTransactor* Trans;

	// Textures.
	UPROPERTY()
	class UTexture2D* Bad;

	// Font used by Canvas-based editors
	UPROPERTY()
	class UFont* EditorFont;

	// Audio
	UPROPERTY(transient)
	class USoundCue* PreviewSoundCue;

	UPROPERTY(transient)
	class UAudioComponent* PreviewAudioComponent;

	// Used in UnrealEd for showing materials
	UPROPERTY()
	class UStaticMesh* EditorCube;

	UPROPERTY()
	class UStaticMesh* EditorSphere;

	UPROPERTY()
	class UStaticMesh* EditorPlane;

	UPROPERTY()
	class UStaticMesh* EditorCylinder;

	// Toggles.
	UPROPERTY()
	uint32 bFastRebuild:1;

	UPROPERTY()
	uint32 IsImportingT3D:1;

	// Other variables.
	UPROPERTY()
	uint32 ClickFlags;

	UPROPERTY()
	class UPackage* ParentContext;

	UPROPERTY()
	FVector UnsnappedClickLocation;

	UPROPERTY()
	FVector ClickLocation;

	UPROPERTY()
	FPlane ClickPlane;

	UPROPERTY()
	FVector MouseMovement;

	// Setting for the detail mode to show in the editor viewports
	UPROPERTY()
	TEnumAsByte<enum EDetailMode> DetailMode;

	// Advanced.
	UPROPERTY(EditAnywhere, config, Category=Advanced)
	uint32 UseSizingBox:1;

	UPROPERTY(EditAnywhere, config, Category=Advanced)
	uint32 UseAxisIndicator:1;

	UPROPERTY(EditAnywhere, config, Category=Advanced)
	uint32 GodMode:1;

	UPROPERTY(EditAnywhere, config, Category=Advanced)
	FString GameCommandLine;

	/** If true, show translucent marker polygons on the builder brush and volumes. */
	UPROPERTY(EditAnywhere, config, Category=Advanced)
	uint32 bShowBrushMarkerPolys:1;

	/** If true, socket snapping is enabled in the main level viewports. */
	UPROPERTY(EditAnywhere, config, Category=Advanced)
	uint32 bEnableSocketSnapping:1;

	/** If true, same type views will be camera tied, and ortho views will use perspective view for LOD parenting */
	UPROPERTY()
	uint32 bEnableLODLocking:1;

	/** If true, actors can be grouped and grouping rules will be maintained. When deactivated, any currently existing groups will still be preserved.*/
	DEPRECATED(4.17, "bGroupingActive has been deprecated.  Use UActorGroupingUtils::IsGroupingActive instead")
	uint32 bGroupingActive:1;

	UPROPERTY(config)
	FString HeightMapExportClassName;

	/** Array of actor factories created at editor startup and used by context menu etc. */
	UPROPERTY()
	TArray<class UActorFactory*> ActorFactories;

	/** The name of the file currently being opened in the editor. "" if no file is being opened. */
	UPROPERTY()
	FString UserOpenedFile;

	///////////////////////////////
	// "Play From Here" properties
	
	/** Additional per-user/per-game options set in the .ini file. Should be in the form "?option1=X?option2?option3=Y"					*/
	UPROPERTY(EditAnywhere, config, Category=Advanced)
	FString InEditorGameURLOptions;

	/** A pointer to a UWorld that is the duplicated/saved-loaded to be played in with "Play From Here" 								*/
	UPROPERTY()
	class UWorld* PlayWorld;

	/** An optional location for the starting location for "Play From Here"																*/
	UPROPERTY()
	FVector PlayWorldLocation;

	/** An optional rotation for the starting location for "Play From Here"																*/
	UPROPERTY()
	FRotator PlayWorldRotation;

	/** Has a request for "Play From Here" been made?													 								*/
	UPROPERTY()
	uint32 bIsPlayWorldQueued:1;

	/** Has a request to toggle between PIE and SIE been made? */
	UPROPERTY()
	uint32 bIsToggleBetweenPIEandSIEQueued:1;

	/** True if we are requesting to start a simulation-in-editor session */
	UPROPERTY()
	uint32 bIsSimulateInEditorQueued:1;

	/** Allows multipel PIE worlds under a single instance. If false, you can only do multiple UE4 processes for pie networking */
	UPROPERTY(globalconfig)
	uint32 bAllowMultiplePIEWorlds:1;

	/** True if there is a pending end play map queued */
	UPROPERTY()
	uint32 bRequestEndPlayMapQueued:1;

	/** Did the request include the optional location and rotation?										 								*/
	UPROPERTY()
	uint32 bHasPlayWorldPlacement:1;

	/** True to enable mobile preview mode when launching the game from the editor on PC platform */
	UPROPERTY()
	uint32 bUseMobilePreviewForPlayWorld:1;

	/** True to enable VR preview mode when launching the game from the editor on PC platform */
	UPROPERTY()
	uint32 bUseVRPreviewForPlayWorld:1;

	/** True if we're Simulating In Editor, as opposed to Playing In Editor.  In this mode, simulation takes place right the level editing environment */
	UPROPERTY()
	uint32 bIsSimulatingInEditor:1;

	/** True if we should not display notifications about undo/redo */
	UPROPERTY()
	uint32 bSquelchTransactionNotification:1;

	/** The PlayerStart class used when spawning the player at the current camera location. */
	UPROPERTY()
	TSubclassOf<class ANavigationObjectBase>  PlayFromHerePlayerStartClass;

	/** When Simulating In Editor, a pointer to the original (non-simulating) editor world */
	UPROPERTY()
	class UWorld* EditorWorld;
	
	/** When Simulating In Editor, an array of all actors that were selected when it began*/
	UPROPERTY()
	TArray<TWeakObjectPtr<class AActor> > ActorsThatWereSelected;

	/** Where did the person want to play? Where to play the game - -1 means in editor, 0 or more is an index into the GConsoleSupportContainer	*/
	UPROPERTY()
	int32 PlayWorldDestination;

	/** The current play world destination (I.E console).  -1 means no current play world destination, 0 or more is an index into the GConsoleSupportContainer	*/
	UPROPERTY()
	int32 CurrentPlayWorldDestination;

	/** Mobile preview settings for what orientation to default to */
	UPROPERTY(config)
	uint32 bMobilePreviewPortrait:1;

	/** Currently targeted device for mobile previewer. */
	UPROPERTY(config)
	int32 BuildPlayDevice;

	/** Maps world contexts to their slate data */
	TMap<FName, FSlatePlayInEditorInfo>	SlatePlayInEditorMap;

	/** Viewport the next PlaySession was requested to happen on */
	TWeakPtr<class ILevelViewport>		RequestedDestinationSlateViewport;

	/** When set to anything other than -1, indicates a specific In-Editor viewport index that PIE should use */
	UPROPERTY()
	int32 PlayInEditorViewportIndex;

	/** Play world url string edited by a user. */
	UPROPERTY()
	FString UserEditedPlayWorldURL;

	/** Temporary render target that can be used by the editor. */
	UPROPERTY(transient)
	class UTextureRenderTarget2D* ScratchRenderTarget2048;

	UPROPERTY(transient)
	class UTextureRenderTarget2D* ScratchRenderTarget1024;

	UPROPERTY(transient)
	class UTextureRenderTarget2D* ScratchRenderTarget512;

	UPROPERTY(transient)
	class UTextureRenderTarget2D* ScratchRenderTarget256;

	/** A mesh component used to preview in editor without spawning a static mesh actor. */
	UPROPERTY(transient)
	class UStaticMeshComponent* PreviewMeshComp;

	/** The index of the mesh to use from the list of preview meshes. */
	UPROPERTY()
	int32 PreviewMeshIndex;

	/** When true, the preview mesh mode is activated. */
	UPROPERTY()
	uint32 bShowPreviewMesh:1;

	/** If "Camera Align" emitter handling uses a custom zoom or not */
	UPROPERTY(config)
	uint32 bCustomCameraAlignEmitter:1;

	/** The distance to place the camera from an emitter actor when custom zooming is enabled */
	UPROPERTY(config)
	float CustomCameraAlignEmitterDistance;

	/** If true, then draw sockets when socket snapping is enabled in 'g' mode */
	UPROPERTY(config)
	uint32 bDrawSocketsInGMode:1;

	/** If true, then draw particle debug helpers in editor viewports */
	UPROPERTY(transient)
	uint32 bDrawParticleHelpers:1;

	/** Brush builders that have been created in the editor */
	UPROPERTY(transient)
	TArray<class UBrushBuilder*> BrushBuilders;	

	/** Whether or not to recheck the current actor selection for lock actors the next time HasLockActors is called */
	bool bCheckForLockActors;

	/** Cached state of whether or not we have locked actors in the selection*/
	bool bHasLockedActors;

	/** Whether or not to recheck the current actor selection for world settings actors the next time IsWorldSettingsSelected is called */
	bool bCheckForWorldSettingsActors;

	/** Cached state of whether or not we have a worldsettings actor in the selection */
	bool bIsWorldSettingsSelected;

	/** The feature level we should use when loading or creating a new world */
	ERHIFeatureLevel::Type DefaultWorldFeatureLevel;

private:

	/** Manager that holds all extensions paired with a world */
	UPROPERTY()
	UEditorWorldExtensionManager* EditorWorldExtensionsManager;

protected:

	/** Count of how many PIE instances are waiting to log in */
	int32 PIEInstancesToLogInCount;

	/* These are parameters that we need to cache for late joining */
	FString ServerPrefix;
	int32 PIEInstance;
	int32 SettingsIndex;
	bool bStartLateJoinersInSpectatorMode;

private:

	/** Additional launch options requested for the next PlaySession */
	FString RequestedAdditionalStandaloneLaunchOptions;

public:

	/** The "manager" of all the layers for the UWorld currently being edited */
	TSharedPtr< class ILayers >				Layers;

	/** List of all viewport clients */
	TArray<class FEditorViewportClient*>	AllViewportClients;

	/** List of level editor viewport clients for level specific actions */
	TArray<class FLevelEditorViewportClient*> LevelViewportClients;

	/** Annotation to track which PIE/SIE (PlayWorld) UObjects have counterparts in the EditorWorld **/
	class FUObjectAnnotationSparseBool ObjectsThatExistInEditorWorld;

	/** Called prior to a Blueprint compile */
	DECLARE_EVENT_OneParam( UEditorEngine, FBlueprintPreCompileEvent, UBlueprint* );
	FBlueprintPreCompileEvent& OnBlueprintPreCompile() { return BlueprintPreCompileEvent; }

	/** Broadcasts that a Blueprint is about to be compiled */
	void BroadcastBlueprintPreCompile(UBlueprint* BlueprintToCompile) { BlueprintPreCompileEvent.Broadcast(BlueprintToCompile); }

	/** Called when a Blueprint compile is completed. */
	DECLARE_EVENT( UEditorEngine, FBlueprintCompiledEvent );
	FBlueprintCompiledEvent& OnBlueprintCompiled() { return BlueprintCompiledEvent; }

	/**	Broadcasts that a blueprint just finished compiling. THIS SHOULD NOT BE PUBLIC */
	void BroadcastBlueprintCompiled() { BlueprintCompiledEvent.Broadcast(); }

	/** Called by the blueprint compiler after a blueprint has been compiled and all instances replaced, but prior to garbage collection. */
	DECLARE_EVENT(UEditorEngine, FBlueprintReinstanced);
	FBlueprintReinstanced& OnBlueprintReinstanced() { return BlueprintReinstanced; }

	/**	Broadcasts that a blueprint just finished being reinstanced. THIS SHOULD NOT BE PUBLIC */
	void BroadcastBlueprintReinstanced() { BlueprintReinstanced.Broadcast(); }

	/** Called when uobjects have been replaced to allow others a chance to fix their references. */
	typedef TMap<UObject*, UObject*> ReplacementObjectMap;
	DECLARE_EVENT_OneParam( UEditorEngine, FObjectsReplacedEvent, const ReplacementObjectMap& );
	FObjectsReplacedEvent& OnObjectsReplaced() { return ObjectsReplacedEvent; }

	/**	Broadcasts that objects have been replaced*/
	void BroadcastBlueprintCompiled(const TMap<UObject*, UObject*>& ReplacementMap) { ObjectsReplacedEvent.Broadcast(ReplacementMap); }

	/** Called when a package with data-driven classes becomes loaded or unloaded */
	DECLARE_EVENT( UEditorEngine, FClassPackageLoadedOrUnloadedEvent );
	FClassPackageLoadedOrUnloadedEvent& OnClassPackageLoadedOrUnloaded() { return ClassPackageLoadedOrUnloadedEvent; }

	/**	Broadcasts that a class package was just loaded or unloaded. THIS SHOULD NOT BE PUBLIC */
	void BroadcastClassPackageLoadedOrUnloaded() { ClassPackageLoadedOrUnloadedEvent.Broadcast(); }

	/** Called when an object is reimported. */
	DECLARE_EVENT_OneParam( UEditorEngine, FObjectReimported, UObject* );
	FObjectReimported& OnObjectReimported() { return ObjectReimportedEvent; }

	/** Editor-only event triggered before an actor or component is moved, rotated or scaled by an editor system */
	DECLARE_EVENT_OneParam( UEditorEngine, FOnBeginTransformObject, UObject& );
	FOnBeginTransformObject& OnBeginObjectMovement() { return OnBeginObjectTransformEvent; }

	/** Editor-only event triggered after actor or component has moved, rotated or scaled by an editor system */
	DECLARE_EVENT_OneParam( UEditorEngine, FOnEndTransformObject, UObject& );
	FOnEndTransformObject& OnEndObjectMovement() { return OnEndObjectTransformEvent; }

	/** Editor-only event triggered before the camera viewed through the viewport is moved by an editor system */
	DECLARE_EVENT_OneParam( UEditorEngine, FOnBeginTransformCamera, UObject& );
	FOnBeginTransformCamera& OnBeginCameraMovement() { return OnBeginCameraTransformEvent; }

	/** Editor-only event triggered after the camera viewed through the viewport has been moved by an editor system */
	DECLARE_EVENT_OneParam( UEditorEngine, FOnEndTransformCamera, UObject& );
	FOnEndTransformCamera& OnEndCameraMovement() { return OnEndCameraTransformEvent; }

	/** Delegate broadcast by the engine every tick when PIE/SIE is active, to check to see whether we need to
		be able to capture state for simulating actor (for Sequencer recording features).  The single bool parameter
		should be set to true if recording features are needed. */
	DECLARE_EVENT_OneParam( UEditorEngine, FGetActorRecordingState, bool& /* bIsRecordingActive */ );
	FGetActorRecordingState& GetActorRecordingState() { return GetActorRecordingStateEvent; }

	/** Editor-only event triggered when a HLOD Actor is moved between clusters */
	DECLARE_EVENT_TwoParams(UEngine, FHLODActorMovedEvent, const AActor*, const AActor*);
	FHLODActorMovedEvent& OnHLODActorMoved() { return HLODActorMovedEvent; }

	/** Called by internal engine systems after a HLOD Actor is moved between clusters */
	void BroadcastHLODActorMoved(const AActor* InActor, const AActor* ParentActor) { HLODActorMovedEvent.Broadcast(InActor, ParentActor); }

	/** Editor-only event triggered when a HLOD Actor's mesh is build */
	DECLARE_EVENT_OneParam(UEngine, FHLODMeshBuildEvent, const class ALODActor*);
	FHLODMeshBuildEvent& OnHLODMeshBuild() { return HLODMeshBuildEvent; }

	/** Called by internal engine systems after a HLOD Actor's mesh is build */
	void BroadcastHLODMeshBuild(const class ALODActor* InActor) { HLODMeshBuildEvent.Broadcast(InActor); }

	/** Editor-only event triggered when a HLOD Actor is added to a cluster */
	DECLARE_EVENT_TwoParams(UEngine, FHLODActorAddedEvent, const AActor*, const AActor*);
	FHLODActorAddedEvent& OnHLODActorAdded() { return HLODActorAddedEvent; }

	/** Called by internal engine systems after a HLOD Actor is added to a cluster */
	void BroadcastHLODActorAdded(const AActor* InActor, const AActor* ParentActor) { HLODActorAddedEvent.Broadcast(InActor, ParentActor); }

	/** Editor-only event triggered when a HLOD Actor is marked dirty */
	DECLARE_EVENT_OneParam(UEngine, FHLODActorMarkedDirtyEvent, class ALODActor*);
	FHLODActorMarkedDirtyEvent& OnHLODActorMarkedDirty() { return HLODActorMarkedDirtyEvent; }

	/** Called by internal engine systems after a HLOD Actor is marked dirty */
	void BroadcastHLODActorMarkedDirty(class ALODActor* InActor) { HLODActorMarkedDirtyEvent.Broadcast(InActor); }

	/** Editor-only event triggered when a HLOD Actor is marked dirty */
	DECLARE_EVENT(UEngine, FHLODTransitionScreenSizeChangedEvent);
	FHLODTransitionScreenSizeChangedEvent& OnHLODTransitionScreenSizeChanged() { return HLODTransitionScreenSizeChangedEvent; }

	/** Called by internal engine systems after a HLOD Actor is marked dirty */
	void BroadcastHLODTransitionScreenSizeChanged() { HLODTransitionScreenSizeChangedEvent.Broadcast(); }

	/** Editor-only event triggered when a HLOD level is added or removed */
	DECLARE_EVENT(UEngine, FHLODLevelsArrayChangedEvent);
	FHLODLevelsArrayChangedEvent& OnHLODLevelsArrayChanged() { return HLODLevelsArrayChangedEvent; }

	/** Called by internal engine systems after a HLOD Actor is marked dirty */
	void BroadcastHLODLevelsArrayChanged() { HLODLevelsArrayChangedEvent.Broadcast(); }

	DECLARE_EVENT_TwoParams(UEngine, FHLODActorRemovedFromClusterEvent, const AActor*, const AActor*);
	FHLODActorRemovedFromClusterEvent& OnHLODActorRemovedFromCluster() { return HLODActorRemovedFromClusterEvent; }

	/** Called by internal engine systems after an Actor is removed from a cluster */
	void BroadcastHLODActorRemovedFromCluster(const AActor* InActor, const AActor* ParentActor) { HLODActorRemovedFromClusterEvent.Broadcast(InActor, ParentActor); }

	/**
	 * Called before an actor or component is about to be translated, rotated, or scaled by the editor
	 *
	 * @param Object	The actor or component that will be moved
	 */
	void BroadcastBeginObjectMovement(UObject& Object) const { OnBeginObjectTransformEvent.Broadcast(Object); }

	/**
	 * Called when an actor or component has been translated, rotated, or scaled by the editor
	 *
	 * @param Object	The actor or component that moved
	 */
	void BroadcastEndObjectMovement(UObject& Object) const { OnEndObjectTransformEvent.Broadcast(Object); }

	/**
	 * Called before the camera viewed through the viewport is moved by the editor
	 *
	 * @param Object	The camera that will be moved
	 */
	void BroadcastBeginCameraMovement(UObject& Object) const { OnBeginCameraTransformEvent.Broadcast(Object); }

	/**
	 * Called when the camera viewed through the viewport has been moved by the editor
	 *
	 * @param Object	The camera that moved
	 */
	void BroadcastEndCameraMovement(UObject& Object) const { OnEndCameraTransformEvent.Broadcast(Object); }

	/**	Broadcasts that an object has been reimported. THIS SHOULD NOT BE PUBLIC */
	void BroadcastObjectReimported(UObject* InObject);

	//~ Begin UObject Interface.
	virtual void FinishDestroy() override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
	//~ End UObject Interface.

	//~ Begin UEngine Interface.
public:
	virtual void Init(IEngineLoop* InEngineLoop) override;
	virtual float GetMaxTickRate(float DeltaTime, bool bAllowFrameRateSmoothing = true) const override;
	virtual void Tick(float DeltaSeconds, bool bIdleMode) override;
	virtual bool ShouldDrawBrushWireframe(AActor* InActor) override;
	virtual void NotifyToolsOfObjectReplacement(const TMap<UObject*, UObject*>& OldToNewInstanceMap) override;
	virtual bool ShouldThrottleCPUUsage() const override;
	virtual bool GetPropertyColorationColor(class UObject* Object, FColor& OutColor) override;
	virtual bool WorldIsPIEInNewViewport(UWorld* InWorld) override;
	virtual void FocusNextPIEWorld(UWorld* CurrentPieWorld, bool previous = false) override;
	virtual void ResetPIEAudioSetting(UWorld *CurrentPieWorld) override;
	virtual class UGameViewportClient* GetNextPIEViewport(UGameViewportClient* CurrentViewport) override;
	virtual UWorld* CreatePIEWorldByDuplication(FWorldContext &WorldContext, UWorld* InWorld, FString &PlayWorldMapName) override;
	virtual bool GetMapBuildCancelled() const override { return false; }
	virtual void SetMapBuildCancelled(bool InCancelled) override { /* Intentionally empty. */ }
	virtual void HandleNetworkFailure(UWorld *World, UNetDriver *NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString) override;

	FString GetPlayOnTargetPlatformName() const;
protected:
	virtual void InitializeObjectReferences() override;
	virtual void ProcessToggleFreezeCommand(UWorld* InWorld) override;
	virtual void ProcessToggleFreezeStreamingCommand(UWorld* InWorld) override;
	virtual void HandleBrowseToDefaultMapFailure(FWorldContext& Context, const FString& TextURL, const FString& Error) override;
private:
	virtual void RemapGamepadControllerIdForPIE(class UGameViewportClient* GameViewport, int32 &ControllerId) override;
	virtual TSharedPtr<SViewport> GetGameViewportWidget() const override;
	virtual void TriggerStreamingDataRebuild() override;
	virtual bool NetworkRemapPath(UNetDriver* Driver, FString& Str, bool bReading = true) override;
	virtual bool NetworkRemapPath(UPendingNetGame* PendingNetGame, FString& Str, bool bReading = true) override;
	virtual bool AreEditorAnalyticsEnabled() const override;
	virtual void CreateStartupAnalyticsAttributes(TArray<FAnalyticsEventAttribute>& StartSessionAttributes) const override;
	virtual void VerifyLoadMapWorldCleanup() override;

	/** Called during editor init and whenever the vanilla status might have changed, to set the flag on the base class */
	void UpdateIsVanillaProduct();

	/** Called when hotreload adds a new class to create volume factories */
	void CreateVolumeFactoriesForNewClasses(const TArray<UClass*>& NewClasses);

public:
	//~ End UEngine Interface.
	
	//~ Begin FExec Interface
	virtual bool Exec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar=*GLog ) override;
	//~ End FExec Interface

	bool	CommandIsDeprecated( const TCHAR* CommandStr, FOutputDevice& Ar );
	
	/**
	 * Exec command handlers
	 */
	bool	HandleBlueprintifyFunction( const TCHAR* Str , FOutputDevice& Ar );
	bool	HandleCallbackCommand( UWorld* InWorld, const TCHAR* Str, FOutputDevice& Ar );
	bool	HandleTestPropsCommand( const TCHAR* Str, FOutputDevice& Ar );
	bool	HandleMapCommand( const TCHAR* Str, FOutputDevice& Ar, UWorld* InWorld );
	bool	HandleSelectCommand( const TCHAR* Str, FOutputDevice& Ar, UWorld* InWorld );
	bool	HandleDeleteCommand( const TCHAR* Str, FOutputDevice& Ar, UWorld* InWorld );
	bool	HandleLightmassDebugCommand( const TCHAR* Str, FOutputDevice& Ar );
	bool	HandleLightmassStatsCommand( const TCHAR* Str, FOutputDevice& Ar );
	bool	HandleSwarmDistributionCommand( const TCHAR* Str, FOutputDevice& Ar );
	bool	HandleLightmassImmediateImportCommand( const TCHAR* Str, FOutputDevice& Ar );
	bool	HandleLightmassImmediateProcessCommand( const TCHAR* Str, FOutputDevice& Ar );
	bool	HandleLightmassSortCommand( const TCHAR* Str, FOutputDevice& Ar );
	bool	HandleLightmassDebugMaterialCommand( const TCHAR* Str, FOutputDevice& Ar );
	bool	HandleLightmassPaddingCommand( const TCHAR* Str, FOutputDevice& Ar );
	bool	HandleLightmassDebugPaddingCommand( const TCHAR* Str, FOutputDevice& Ar );
	bool	HandleLightmassProfileCommand( const TCHAR* Str, FOutputDevice& Ar );
	bool	HandleSetReplacementCommand( const TCHAR* Str, FOutputDevice& Ar, UWorld* InWorld );
	bool	HandleSelectNameCommand( const TCHAR* Str, FOutputDevice& Ar, UWorld* InWorld  );
	bool	HandleDumpPublicCommand( const TCHAR* Str, FOutputDevice& Ar );
	bool	HandleJumpToCommand( const TCHAR* Str, FOutputDevice& Ar );
	bool	HandleBugItGoCommand( const TCHAR* Str, FOutputDevice& Ar );
	bool	HandleTagSoundsCommand( const TCHAR* Str, FOutputDevice& Ar );
	bool	HandlecheckSoundsCommand( const TCHAR* Str, FOutputDevice& Ar );
	bool	HandleFixupBadAnimNotifiersCommand( const TCHAR* Str, FOutputDevice& Ar );
	bool	HandleSetDetailModeCommand( const TCHAR* Str, FOutputDevice& Ar );
	bool	HandleSetDetailModeViewCommand( const TCHAR* Str, FOutputDevice& Ar, UWorld* InWorld );
	bool	HandleCleanBSPMaterialCommand( const TCHAR* Str, FOutputDevice& Ar, UWorld* InWorld  );
	bool	HandleAutoMergeStaticMeshCommand( const TCHAR* Str, FOutputDevice& Ar );
	bool	HandleAddSelectedCommand( const TCHAR* Str, FOutputDevice& Ar );
	bool	HandleToggleSocketGModeCommand( const TCHAR* Str, FOutputDevice& Ar );
	bool	HandleListMapPackageDependenciesCommand( const TCHAR* Str, FOutputDevice& Ar );
	bool	HandleRebuildVolumesCommand( const TCHAR* Str, FOutputDevice& Ar, UWorld* InWorld );
	bool	HandleRemoveArchtypeFlagCommand( const TCHAR* Str, FOutputDevice& Ar );
	bool	HandleStartMovieCaptureCommand( const TCHAR* Cmd, FOutputDevice& Ar );
	bool	HandleBuildMaterialTextureStreamingData( const TCHAR* Cmd, FOutputDevice& Ar );

	/**
	 * Initializes the Editor.
	 */
	void InitEditor(IEngineLoop* InEngineLoop);

	/**
	 * Constructs a default cube builder brush, this function MUST be called at the AFTER UEditorEngine::Init in order to guarantee builder brush and other required subsystems exist.
	 *
	 * @param		InWorld			World in which to create the builder brush.
	 */
	void InitBuilderBrush( UWorld* InWorld );

	/** Access user setting for audio mute. */
	bool IsRealTimeAudioMuted() const;

	/** Set user setting for audio mute. */
	void MuteRealTimeAudio(bool bMute);

	/** Access user setting for audio level. Fractional volume 0.0->1.0 */
	float GetRealTimeAudioVolume() const;

	/** Set user setting for audio mute. Fractional volume 0.0->1.0 */
	void SetRealTimeAudioVolume(float VolumeLevel);

	/**
	 * Updates a single viewport
	 * @param Viewport - the viewport that we're trying to draw
	 * @param bInAllowNonRealtimeViewportToDraw - whether or not to allow non-realtime viewports to update
	 * @param bLinkedOrthoMovement	True if orthographic viewport movement is linked
	 * @return - Whether a NON-realtime viewport has updated in this call.  Used to help time-slice canvas redraws
	 */
	bool UpdateSingleViewportClient(FEditorViewportClient* InViewportClient, const bool bInAllowNonRealtimeViewportToDraw, bool bLinkedOrthoMovement );

	/** Used for generating status bar text */
	enum EMousePositionType
	{
		MP_None,
		MP_WorldspacePosition,
		MP_Translate,
		MP_Rotate,
		MP_Scale,
		MP_CameraSpeed,
		MP_NoChange
	};


	// Execute a command that is safe for rebuilds.
	virtual bool SafeExec( UWorld* InWorld, const TCHAR* Cmd, FOutputDevice& Ar=*GLog );
	// Process an incoming network message meant for the editor server
	bool Exec_StaticMesh( UWorld* InWorld, const TCHAR* Str, FOutputDevice& Ar );
	bool Exec_Brush( UWorld* InWorld, const TCHAR* Str, FOutputDevice& Ar );
	bool Exec_Poly( UWorld* InWorld, const TCHAR* Str, FOutputDevice& Ar );
	bool Exec_Obj( const TCHAR* Str, FOutputDevice& Ar );
	bool Exec_Camera( const TCHAR* Str, FOutputDevice& Ar );
	bool Exec_Transaction(const TCHAR* Str, FOutputDevice& Ar);
	bool Exec_Particle(const TCHAR* Str, FOutputDevice& Ar);

	/**
	 * Executes each line of text in a file sequentially, as if each were a separate command
	 *
	 * @param InWorld	World context
	 * @param InFilename The name of the file to load and execute
	 * @param Ar Output device
	 */
	void ExecFile( UWorld* InWorld, const TCHAR* InFilename, FOutputDevice& Ar );

	//~ Begin Transaction Interfaces.
	int32 BeginTransaction(const TCHAR* SessionContext, const FText& Description, UObject* PrimaryObject);
	int32 BeginTransaction(const FText& Description);
	int32 EndTransaction();
	void ResetTransaction(const FText& Reason);
	void CancelTransaction(int32 Index);
	bool UndoTransaction(bool bCanRedo = true);
	bool RedoTransaction();
	bool IsTransactionActive();
	FText GetTransactionName() const;
	bool IsObjectInTransactionBuffer( const UObject* Object ) const;

	/**
	 * Rebuilds the map.
	 *
	 * @param bBuildAllVisibleMaps	Whether or not to rebuild all visible maps, if false only the current level will be built.
	 */
	enum EMapRebuildType
	{
		MRT_Current				= 0,
		MRT_AllVisible			= 1,
		MRT_AllDirtyForLighting	= 2,
	};

	/**
	 * Rebuilds the map.
	 *
	 * @param bBuildAllVisibleMaps	Whether or not to rebuild all visible maps, if false only the current level will be built.
	 */
	void RebuildMap(UWorld* InWorld, EMapRebuildType RebuildType);

	/**
	 * Quickly rebuilds a single level (no bounds build, visibility testing or Bsp tree optimization).
	 *
	 * @param Level	The level to be rebuilt.
	 */
	void RebuildLevel(ULevel& Level);
	
	/**
	 * Builds up a model from a set of brushes. Used by RebuildLevel.
	 *
	 * @param Model					The model to be rebuilt.
	 * @param bSelectedBrushesOnly	Use all brushes in the current level or just the selected ones?.
	 * @param bTreatMovableBrushesAsStatic	Treat moveable brushes as static?.
	 */
	void RebuildModelFromBrushes(UModel* Model, bool bSelectedBrushesOnly, bool bTreatMovableBrushesAsStatic = false);

	/**
	 * Rebuilds levels containing currently selected brushes and should be invoked after a brush has been modified
	 */
	void RebuildAlteredBSP();

	/** Helper method for executing the de/intersect CSG operation */
	void BSPIntersectionHelper(UWorld* InWorld, ECsgOper Operation);

	/**
	 * @return	A pointer to the named actor or NULL if not found.
	 */
	AActor* SelectNamedActor(const TCHAR *TargetActorName);


	/**
	 * Moves an actor in front of a camera specified by the camera's origin and direction.
	 * The distance the actor is in front of the camera depends on the actors bounding cylinder
	 * or a default value if no bounding cylinder exists.
	 *
	 * @param InActor			The actor to move
	 * @param InCameraOrigin	The location of the camera in world space
	 * @param InCameraDirection	The normalized direction vector of the camera
	 */
	void MoveActorInFrontOfCamera( AActor& InActor, const FVector& InCameraOrigin, const FVector& InCameraDirection );

	/**
	 * Moves all viewport cameras to the target actor.
	 * @param	Actor					Target actor.
	 * @param	bActiveViewportOnly		If true, move/reorient only the active viewport.
	 */
	void MoveViewportCamerasToActor(AActor& Actor,  bool bActiveViewportOnly);

	/**
	* Moves all viewport cameras to focus on the provided array of actors.
	* @param	Actors					Target actors.

	* @param	bActiveViewportOnly		If true, move/reorient only the active viewport.
	*/
	void MoveViewportCamerasToActor(const TArray<AActor*> &Actors, bool bActiveViewportOnly);

	/**
	* Moves all viewport cameras to focus on the provided array of actors.
	* @param	Actors					Target actors.
	* @param	Components				Target components (used of actors array is empty)
	* @param	bActiveViewportOnly		If true, move/reorient only the active viewport.
	*/
	void MoveViewportCamerasToActor(const TArray<AActor*> &Actors, const TArray<UPrimitiveComponent*>& Components, bool bActiveViewportOnly);

	/**
	* Moves all viewport cameras to focus on the provided component.
	* @param	Component				Target component
	* @param	bActiveViewportOnly		If true, move/reorient only the active viewport.
	*/
	void MoveViewportCamerasToComponent(USceneComponent* Component, bool bActiveViewportOnly);

	/** 
	 * Snaps an actor in a direction.  Optionally will align with the trace normal.
	 * @param InActor			Actor to move to the floor.
	 * @param InAlign			sWhether or not to rotate the actor to align with the trace normal.
	 * @param InUseLineTrace	Whether or not to only trace with a line through the world.
	 * @param InUseBounds		Whether or not to base the line trace off of the bounds.
	 * @param InUsePivot		Whether or not to use the pivot position.
	 * @param InDestination		The destination actor we want to move this actor to, NULL assumes we just want to go towards the floor
	 * @return					Whether or not the actor was moved.
	 */
	bool SnapObjectTo( FActorOrComponent Object, const bool InAlign, const bool InUseLineTrace, const bool InUseBounds, const bool InUsePivot, FActorOrComponent InDestination = FActorOrComponent() );

	/**
	 * Snaps the view of the camera to that of the provided actor.
	 *
	 * @param	Actor	The actor the camera is going to be snapped to.
	 */
	void SnapViewTo(const FActorOrComponent& Object);

	/**
	 * Remove the roll, pitch and/or yaw from the perspective viewports' cameras.
	 *
	 * @param	Roll		If true, the camera roll is reset to zero.
	 * @param	Pitch		If true, the camera pitch is reset to zero.
	 * @param	Yaw			If true, the camera yaw is reset to zero.
	 */
	void RemovePerspectiveViewRotation(bool Roll, bool Pitch, bool Yaw);

	//
	// Pivot handling.
	//

	virtual FVector GetPivotLocation() { return FVector::ZeroVector; }
	/** Sets the editor's pivot location, and optionally the pre-pivots of actors.
	 *
	 * @param	NewPivot				The new pivot location
	 * @param	bSnapPivotToGrid		If true, snap the new pivot location to the grid.
	 * @param	bIgnoreAxis				If true, leave the existing pivot unaffected for components of NewPivot that are 0.
	 * @param	bAssignPivot			If true, assign the given pivot to any valid actors that retain it (defaults to false)
	 */
	virtual void SetPivot(FVector NewPivot, bool bSnapPivotToGrid, bool bIgnoreAxis, bool bAssignPivot=false) {}
	virtual void ResetPivot() {}

	//
	// General functions.
	//

	/**
	 * Cleans up after major events like e.g. map changes.
	 *
	 * @param	ClearSelection	Whether to clear selection
	 * @param	Redraw			Whether to redraw viewports
	 * @param	TransReset		Human readable reason for resetting the transaction system
	 */
	virtual void Cleanse( bool ClearSelection, bool Redraw, const FText& TransReset );
	virtual void FinishAllSnaps() { }

	/**
	 * Redraws all level editing viewport clients.
	 *
	 * @param	bInvalidateHitProxies		[opt] If true (the default), invalidates cached hit proxies too.
	 */
	virtual void RedrawLevelEditingViewports(bool bInvalidateHitProxies=true) {}
		
	/**
	 * Triggers a high res screen shot for current editor viewports.
	 *
	 */
	virtual void TakeHighResScreenShots(){}

	virtual void NoteSelectionChange() { check(0); }

	/**
	 * Adds an actor to the world at the specified location.
	 *
	 * @param	InLevel			Level in which to add the actor
	 * @param	Class			A non-abstract, non-transient, placeable class.  Must be non-NULL.
	 * @param	Transform		The world-space transform to spawn the actor with.
	 * @param	bSilent			If true, suppress logging (optional, defaults to false).
	 * @param	ObjectFlags		The object flags to place on the spawned actor.
	 * @return					A pointer to the newly added actor, or NULL if add failed.
	 */
	virtual AActor* AddActor(ULevel* InLevel, UClass* Class, const FTransform& Transform, bool bSilent = false, EObjectFlags ObjectFlags = RF_Transactional);

	/**
	 * Adds actors to the world at the specified location using export text.
	 *
	 * @param	ExportText		A T3D representation of the actor to create.
	 * @param	bSilent			If true, suppress logging
	 * @param	ObjectFlags		The object flags to place on the spawned actor.
	 * @return					A pointer to the newly added actor, or NULL if add failed.
	 */
	virtual TArray<AActor*> AddExportTextActors(const FString& ExportText, bool bSilent, EObjectFlags ObjectFlags = RF_Transactional);

	virtual void NoteActorMovement() { check(0); }
	virtual UTransactor* CreateTrans();

	/** Plays an editor sound, loading the sound on demand if necessary (if the user has sounds enabled.)  The reference to the sound asset is not retained. */
	void PlayEditorSound( const FString& SoundAssetName );

	/** Plays an editor sound (if the user has sounds enabled.) */
	void PlayEditorSound( USoundBase* InSound );

	/**
	 * Returns true if currently able to play a sound and if the user has sounds enabled.
	 */
	bool CanPlayEditorSound() const;


	/**
	 * Returns the preview audio component
	 */
	UAudioComponent* GetPreviewAudioComponent();

	/**
	 * Returns an audio component linked to the current scene that it is safe to play a sound on
	 *
	 * @param	Sound		A sound to attach to the audio component
	 * @param	SoundNode	A sound node that is attached to the audio component when the sound is NULL
	 */
	UAudioComponent* ResetPreviewAudioComponent( class USoundBase* Sound = NULL, class USoundNode* SoundNode = NULL );

	/**
	 * Plays a preview of a specified sound or node
	 *
	 * @param	Sound		A sound to attach to the audio component
	 * @param	SoundNode	A sound node that is attached to the audio component when the sound is NULL
	 */
	void PlayPreviewSound(USoundBase* Sound, USoundNode* SoundNode = NULL);

	/**
	 * Clean up any world specific editor components so they can be GC correctly
	 */
	void ClearPreviewComponents();

	
	/**
	 * Close all the edit windows for assets that are owned by the world being closed
	 */
	void CloseEditedWorldAssets(UWorld* InWorld);

	/**
	 * Redraws all editor viewport clients.
	 *
	 * @param	bInvalidateHitProxies		[opt] If true (the default), invalidates cached hit proxies too.
	 */
	void RedrawAllViewports(bool bInvalidateHitProxies=true);

	/**
	 * Invalidates all viewports parented to the specified view.
	 *
	 * @param	InParentView				The parent view whose child views should be invalidated.
	 * @param	bInvalidateHitProxies		[opt] If true (the default), invalidates cached hit proxies too.
	 */
	void InvalidateChildViewports(FSceneViewStateInterface* InParentView, bool bInvalidateHitProxies=true);

	/**
	 * Looks for an appropriate actor factory for the specified UClass.
	 *
	 * @param	InClass		The class to find the factory for.
	 * @return				A pointer to the factory to use.  NULL if no factories support this class.
	 */
	UActorFactory* FindActorFactoryForActorClass( const UClass* InClass );

	/**
	 * Looks for an actor factory spawned from the specified class.
	 *
	 * @param	InClass		The factories class to find
	 * @return				A pointer to the factory to use.  NULL if no factories support this class.
	 */
	UActorFactory* FindActorFactoryByClass( const UClass* InClass ) const;

	/**
	* Looks for an actor factory spawned from the specified class, for the specified UClass for an actor.
	*
	* @param	InFactoryClass		The factories class to find
	* @param	InActorClass		The class to find the factory for.
	* @return				A pointer to the factory to use.  NULL if no factories support this class.
	*/
	UActorFactory* FindActorFactoryByClassForActorClass( const UClass* InFactoryClass, const UClass* InActorClass );

	/**
	 * Uses the supplied factory to create an actor at the clicked location and adds to level.
	 *
	 * @param	Factory					The factory to create the actor from.  Must be non-NULL.
	 * @param	ObjectFlags				[opt] The flags to apply to the actor when it is created
	 * @return							A pointer to the new actor, or NULL on fail.
	 */
	AActor* UseActorFactoryOnCurrentSelection( UActorFactory* Factory, const FTransform* InActorTransform, EObjectFlags ObjectFlags = RF_Transactional );

	/**
	 * Uses the supplied factory to create an actor at the clicked location and adds to level.
	 *
	 * @param	Factory					The factory to create the actor from.  Must be non-NULL.
	 * @param	AssetData				The optional asset to base the actor construction on.
	 * @param	ActorLocation			[opt] If null, positions the actor at the mouse location, otherwise specified. Default is null.
	 * @param	bUseSurfaceOrientation	[opt] If true, align new actor's orientation to the underlying surface normal.  Default is false.
	 * @param	ObjectFlags				[opt] The flags to apply to the actor when it is created
	 * @return							A pointer to the new actor, or NULL on fail.
	 */
	AActor* UseActorFactory( UActorFactory* Factory, const FAssetData& AssetData, const FTransform* ActorLocation, EObjectFlags ObjectFlags = RF_Transactional );

	/**
	 * Replaces the selected Actors with the same number of a different kind of Actor using the specified factory to spawn the new Actors
	 * note that only Location, Rotation, Drawscale, Drawscale3D, Tag, and Group are copied from the old Actors
	 * 
	 * @param Factory - the Factory to use to create Actors
	 */
	void ReplaceSelectedActors(UActorFactory* Factory, const FAssetData& AssetData);

	/**
	 * Replaces specified Actors with the same number of a different kind of Actor using the specified factory to spawn the new Actors
	 * note that only Location, Rotation, Drawscale, Drawscale3D, Tag, and Group are copied from the old Actors
	 * 
	 * @param Factory - the Factory to use to create Actors
	 */
	void ReplaceActors(UActorFactory* Factory, const FAssetData& AssetData, const TArray<AActor*>& ActorsToReplace);

	/**
	 * Converts passed in brushes into a single static mesh actor. 
	 * Note: This replaces all the brushes with a single actor. This actor will not be attached to anything unless a single brush was converted.
	 *
	 * @param	InStaticMeshPackageName		The name to save the brushes to.
	 * @param	InBrushesToConvert			A list of brushes being converted.
	 *
	 * @return							Returns the newly created actor with the newly created static mesh.
	 */
	AActor* ConvertBrushesToStaticMesh(const FString& InStaticMeshPackageName, TArray<ABrush*>& InBrushesToConvert, const FVector& InPivotLocation);

	/**
	 * Converts passed in light actors into new actors of another type.
	 * Note: This replaces the old actor with the new actor.
	 * Most properties of the old actor that can be copied are copied to the new actor during this process.
	 * Properties that can be copied are ones found in a common superclass between the actor to convert and the new class.
	 * Common light component properties between the two classes are also copied
	 *
	 * @param	ConvertToClass	The light class we are going to convert to.
	 */
	void ConvertLightActors( UClass* ConvertToClass );

	/**
	 * Converts passed in actors into new actors of the specified type.
	 * Note: This replaces the old actors with brand new actors while attempting to preserve as many properties as possible.
	 * Properties of the actors components are also attempted to be copied for any component names supplied in the third parameter.
	 * If a component name is specified, it is only copied if a component of the specified name exists in the source and destination actors,
	 * as well as in the class default object of the class of the source actor, and that all three of those components share a common base class.
	 * This approach is used instead of simply accepting component classes to copy because some actors could potentially have multiple of the same
	 * component type.
	 *
	 * @param	ActorsToConvert				Array of actors which should be converted to the new class type
	 * @param	ConvertToClass				Class to convert the provided actors to
	 * @param	ComponentsToConsider		Names of components to consider for property copying as well
	 * @param	bUseSpecialCases			If true, looks for classes that can be handled by hardcoded conversions
	 * @param	InStaticMeshPackageName		The name to save the brushes to.
	 */
	void DoConvertActors( const TArray<AActor*>& ActorsToConvert, UClass* ConvertToClass, const TSet<FString>& ComponentsToConsider, bool bUseSpecialCases, const FString& InStaticMeshPackageName );

	/**
	 * Sets up for a potentially deferred ConvertActors call, based on if any brushes are being converted to a static mesh. If one (or more)
	 * are being converted, the user will need to put in a package before the process continues.
	 *
	 * @param	ActorsToConvert			Array of actors which should be converted to the new class type
	 * @param	ConvertToClass			Class to convert the provided actors to
	 * @param	ComponentsToConsider	Names of components to consider for property copying as well
	 * @param	bUseSpecialCases		If true, looks for classes that can be handled by hardcoded conversions
	 */
	void ConvertActors( const TArray<AActor*>& ActorsToConvert, UClass* ConvertToClass, const TSet<FString>& ComponentsToConsider, bool bUseSpecialCases = false );

	/**
	 * Changes the state of preview mesh mode to on or off.
	 *
	 * @param	bState	Enables the preview mesh mode if true; Disables the preview mesh mode if false.
	 */
	void SetPreviewMeshMode( bool bState );

	/**
	 * Updates the position of the preview mesh in the level.
	 */
	void UpdatePreviewMesh();

	/**
	 * Changes the preview mesh to the next one.
	 */
	void CyclePreviewMesh();

	/**
	 * Copy selected actors to the clipboard.
	 *
	 * @param	InWorld					World context
	 * @param	DestinationData			If != NULL, additionally copy data to string
	 */
	virtual void edactCopySelected(UWorld* InWorld, FString* DestinationData = NULL) {}

	/**
	 * Paste selected actors from the clipboard.
	 *
	 * @param	InWorld				World context
	 * @param	bDuplicate			Is this a duplicate operation (as opposed to a real paste)?
	 * @param	bOffsetLocations	Should the actor locations be offset after they are created?
	 * @param	bWarnIfHidden		If true displays a warning if the destination level is hidden
	 * @param	SourceData			If != NULL, use instead of clipboard data
	 */
	virtual void edactPasteSelected(UWorld* InWorld, bool bDuplicate, bool bOffsetLocations, bool bWarnIfHidden, FString* SourceData = NULL) {}

	/**
	 * Duplicates selected actors.
	 *
	 * @param	InLevel			Level to place duplicate
	 * @param	bUseOffset		Should the actor locations be offset after they are created?
	 */
	virtual void edactDuplicateSelected( ULevel* InLevel, bool bOffsetLocations ) {}

	/**
	 * Deletes all selected actors
	 *
	 * @param	InWorld				World context
	 * @param	bVerifyDeletionCanHappen	[opt] If true (default), verify that deletion can be performed.
	 * @param	bWarnAboutReferences		[opt] If true (default), we prompt the user about referenced actors they are about to delete
	 * @param	bWarnAboutSoftReferences	[opt] If true (default), we prompt the user about soft references to actors they are about to delete
	 * @return								true unless the delete operation was aborted.
	 */
	virtual bool edactDeleteSelected(UWorld* InWorld, bool bVerifyDeletionCanHappen=true, bool bWarnAboutReferences = true, bool bWarnAboutSoftReferences = true) { return true; }

	/**
	 * Checks the state of the selected actors and notifies the user of any potentially unknown destructive actions which may occur as
	 * the result of deleting the selected actors.  In some cases, displays a prompt to the user to allow the user to choose whether to
	 * abort the deletion.
	 *
	 * @return								false to allow the selected actors to be deleted, true if the selected actors should not be deleted.
	 */
	virtual bool ShouldAbortActorDeletion() const { return false; }

	/**
	*
	* Rebuild the level's Bsp from the level's CSG brushes.
	*
	* @param InWorld	The world in which the rebuild the CSG brushes for 
	*/
	virtual void csgRebuild( UWorld* InWorld );

	/**
	*
	* Find the Brush EdPoly corresponding to a given Bsp surface.
	*
	* @param InModel	Model to get poly from
	* @param iSurf		surface index
	* @param Poly		
	*
	* returns true if poly not available
	*/
	virtual bool polyFindMaster( UModel* InModel, int32 iSurf, FPoly& Poly );

	/**
	 * Update a the master brush EdPoly corresponding to a newly-changed
	 * poly to reflect its new properties.
	 *
	 * Doesn't do any transaction tracking.
	 */
	virtual void polyUpdateMaster( UModel* Model, int32 iSurf, bool bUpdateTexCoords, bool bOnlyRefreshSurfaceMaterials );

	/**
	 * Populates a list with all polys that are linked to the specified poly.  The
	 * resulting list includes the original poly.
	 */
	virtual void polyGetLinkedPolys( ABrush* InBrush, FPoly* InPoly, TArray<FPoly>* InPolyList );

	/**
	 * Takes a list of polygons and returns a list of the outside edges (edges which are not shared
	 * by other polys in the list).
	 */
	virtual void polyGetOuterEdgeList( TArray<FPoly>* InPolyList, TArray<FEdge>* InEdgeList );

	/**
	 * Takes a list of polygons and creates a new list of polys which have no overlapping edges.  It splits
	 * edges as necessary to achieve this.
	 */
	virtual void polySplitOverlappingEdges( TArray<FPoly>* InPolyList, TArray<FPoly>* InResult );

	/**
	 * Sets and clears all Bsp node flags.  Affects all nodes, even ones that don't
	 * really exist.
	 */
	virtual void polySetAndClearPolyFlags( UModel* Model, uint32 SetBits, uint32 ClearBits, bool SelectedOnly, bool UpdateMaster );

	// Selection.
	virtual void SelectActor(AActor* Actor, bool bInSelected, bool bNotify, bool bSelectEvenIfHidden = false, bool bForceRefresh = false) {}
	virtual bool CanSelectActor(AActor* Actor, bool bInSelected, bool bSelectEvenIfHidden=false, bool bWarnIfLevelLocked=false) const { return true; }
	virtual void SelectGroup(class AGroupActor* InGroupActor, bool bForceSelection=false, bool bInSelected=true, bool bNotify=true) {}
	virtual void SelectComponent(class UActorComponent* Component, bool bInSelected, bool bNotify, bool bSelectEvenIfHidden = false) {}

	/**
	 * Replaces the components in ActorsToReplace with an primitive component in Replacement
	 *
	 * @param ActorsToReplace Primitive components in the actors in this array will have their ReplacementPrimitive set to a component in Replacement
	 * @param Replacement The first usable component in Replacement will be the ReplacementPrimitive for the actors
	 * @param ClassToReplace If this is set, only components will of this class will be used/replaced
	 */
	virtual void AssignReplacementComponentsByActors(TArray<AActor*>& ActorsToReplace, AActor* Replacement, UClass* ClassToReplace=NULL);

	/**
	 * Selects or deselects a BSP surface in the persistent level's UModel.  Does nothing if GEdSelectionLock is true.
	 *
	 * @param	InModel					The model of the surface to select.
	 * @param	iSurf					The index of the surface in the persistent level's UModel to select/deselect.
	 * @param	bSelected				If true, select the surface; if false, deselect the surface.
	 * @param	bNoteSelectionChange	If true, call NoteSelectionChange().
	 */
	virtual void SelectBSPSurf(UModel* InModel, int32 iSurf, bool bSelected, bool bNoteSelectionChange) {}

	/**
	 * Deselect all actors.  Does nothing if GEdSelectionLock is true.
	 *
	 * @param	bNoteSelectionChange		If true, call NoteSelectionChange().
	 * @param	bDeselectBSPSurfs			If true, also deselect all BSP surfaces.
	 */
	virtual void SelectNone(bool bNoteSelectionChange, bool bDeselectBSPSurfs, bool WarnAboutManyActors=true) {}

	/**
	 * Deselect all surfaces.
	 */
	virtual void DeselectAllSurfaces() {}

	// Bsp Poly selection virtuals from EditorCsg.cpp.
	virtual void polySelectAll ( UModel* Model );
	virtual void polySelectMatchingGroups( UModel* Model );
	virtual void polySelectMatchingItems( UModel* Model );
	virtual void polySelectCoplanars( UWorld* InWorld, UModel* Model );
	virtual void polySelectAdjacents( UWorld* InWorld, UModel* Model );
	virtual void polySelectAdjacentWalls( UWorld* InWorld, UModel* Model );
	virtual void polySelectAdjacentFloors( UWorld* InWorld, UModel* Model );
	virtual void polySelectAdjacentSlants( UWorld* InWorld, UModel* Model );
	virtual void polySelectMatchingBrush( UModel* Model );

	/**
	 * Selects surfaces whose material matches that of any selected surfaces.
	 *
	 * @param	bCurrentLevelOnly		If true, select
	 */
	virtual void polySelectMatchingMaterial(UWorld* InWorld, bool bCurrentLevelOnly);

	/**
	 * Selects surfaces whose lightmap resolution matches that of any selected surfaces.
	 *
	 * @param	bCurrentLevelOnly		If true, select
	 */
	virtual void polySelectMatchingResolution(UWorld* InWorld, bool bCurrentLevelOnly);

	virtual void polySelectReverse( UModel* Model );
	virtual void polyMemorizeSet( UModel* Model );
	virtual void polyRememberSet( UModel* Model );
	virtual void polyXorSet( UModel* Model );
	virtual void polyUnionSet( UModel* Model );
	virtual void polyIntersectSet( UModel* Model );
	virtual void polySelectZone( UModel *Model );

	// Pan textures on selected polys.  Doesn't do transaction tracking.
	virtual void polyTexPan( UModel* Model, int32 PanU, int32 PanV, int32 Absolute );

	// Scale textures on selected polys. Doesn't do transaction tracking.
	virtual void polyTexScale( UModel* Model,float UU, float UV, float VU, float VV, bool Absolute );

	// Map brush selection virtuals from EditorCsg.cpp.
	virtual void MapSelectOperation( UWorld* InWorld, EBrushType BrushType );
	virtual void MapSelectFlags( UWorld* InWorld, uint32 Flags );

	// Put the first selected brush into the current Brush.
	virtual void MapBrushGet(UWorld* InWorld);

	// Replace all selected brushes with the current Brush.
	virtual void mapBrushPut();

	// Send all selected brushes in a level to the front of the hierarchy
	virtual void mapSendToFirst(UWorld* InWorld);

	// Send all selected brushes in a level to the back of the hierarchy
	virtual void mapSendToLast(UWorld* InWorld);

	/**
	 * Swaps position in the actor list for the first two selected actors in the current level
	 */
	virtual void mapSendToSwap(UWorld* InWorld);
	virtual void MapSetBrush( UWorld* InWorld, EMapSetBrushFlags PropertiesMask, uint16 BrushColor, FName Group, uint32 SetPolyFlags, uint32 ClearPolyFlags, uint32 BrushType, int32 DrawType );

	// Bsp virtuals from Bsp.cpp.
	virtual void bspRepartition( UWorld* InWorld, int32 iNode );

	/** Convert a Bsp node to an EdPoly.  Returns number of vertices in Bsp node. */
	virtual int32 bspNodeToFPoly( UModel* Model, int32 iNode, FPoly* EdPoly );

	/**
	 * Clean up all nodes after a CSG operation.  Resets temporary bit flags and unlinks
	 * empty leaves.  Removes zero-vertex nodes which have nonzero-vertex coplanars.
	 */
	virtual void bspCleanup( UModel* Model );

	/** 
	 * Build EdPoly list from a model's Bsp. Not transactional.
	 * @param DestArray helps build bsp FPolys in non-main threads. It also allows to perform this action without GUndo 
	 *	interfering. Temporary results will be written to DestArray. Defaults to Model->Polys->Element
	 */
	virtual void bspBuildFPolys( UModel* Model, bool SurfLinks, int32 iNode, TArray<FPoly>* DestArray = NULL );
	virtual void bspMergeCoplanars( UModel* Model, bool RemapLinks, bool MergeDisparateTextures );
	/**
	 * Performs any CSG operation between the brush and the world.
	 *
	 * @param	Actor							The brush actor to apply.
	 * @param	Model							The model to apply the CSG operation to; typically the world's model.
	 * @param	PolyFlags						PolyFlags to set on brush's polys.
	 * @param	BrushType						The type of brush.
	 * @param	CSGOper							The CSG operation to perform.
	 * @param	bBuildBounds					If true, updates bounding volumes on Model for CSG_Add or CSG_Subtract operations.
	 * @param	bMergePolys						If true, coplanar polygons are merged for CSG_Intersect or CSG_Deintersect operations.
	 * @param	bReplaceNULLMaterialRefs		If true, replace NULL material references with a reference to the GB-selected material.
	 * @param	bShowProgressBar				If true, display progress bar for complex brushes
	 * @return									0 if nothing happened, 1 if the operation was error-free, or 1+N if N CSG errors occurred.
	 */
	virtual int32 bspBrushCSG( ABrush* Actor, UModel* Model, uint32 PolyFlags, EBrushType BrushType, ECsgOper CSGOper, bool bBuildBounds, bool bMergePolys, bool bReplaceNULLMaterialRefs, bool bShowProgressBar=true );

	/**
	 * Optimize a level's Bsp, eliminating T-joints where possible, and building side
	 * links.  This does not always do a 100% perfect job, mainly due to imperfect 
	 * levels, however it should never fail or return incorrect results.
	 */
	virtual void bspOptGeom( UModel* Model );
	
	/**
	 * Builds lighting information depending on passed in options.
	 *
	 * @param	Options		Options determining on what and how lighting is built
	 */
	void BuildLighting(const class FLightingBuildOptions& Options);

	/** Updates the asynchronous static light building */
	void UpdateBuildLighting();

	/** Checks to see if the asynchronous lighting build is running or not */
	bool IsLightingBuildCurrentlyRunning() const;

	bool IsLightingBuildCurrentlyExporting() const;

	/** Checks if asynchronous lighting is building, if so, it throws a warning notification and returns true */
	bool WarnIfLightingBuildIsCurrentlyRunning();

	/**
	 * Open a Fbx file with the given name, and import each sequence with the supplied Skeleton.
	 * This is only possible if each track expected in the Skeleton is found in the target file. If not the case, a warning is given and import is aborted.
	 * If Skeleton is empty (ie. TrackBoneNames is empty) then we use this Fbx file to form the track names array.
	 *
	 * @param Skeleton	The skeleton that animation is import into
	 * @param Filename	The FBX filename
	 * @param bImportMorphTracks	true to import any morph curve data.
	 */
	static UAnimSequence * ImportFbxAnimation( USkeleton* Skeleton, UObject* Outer, class UFbxAnimSequenceImportData* ImportData, const TCHAR* InFilename, const TCHAR* AnimName, bool bImportMorphTracks );
	/**
	 * Reimport animation using SourceFilePath and SourceFileStamp 
	 *
	 * @param Skeleton	The skeleton that animation is import into
	 * @param Filename	The FBX filename
	 */
	static bool ReimportFbxAnimation( USkeleton* Skeleton, UAnimSequence* AnimSequence, class UFbxAnimSequenceImportData* ImportData, const TCHAR* InFilename);


	// Object management.
	virtual void RenameObject(UObject* Object,UObject* NewOuter,const TCHAR* NewName, ERenameFlags Flags=REN_None);

	// Level management.
	void AnalyzeLevel(ULevel* Level,FOutputDevice& Ar);

	/**
	 * Updates all components in the current level's scene.
	 */
	void EditorUpdateComponents();

	/** 
	 * Displays a modal message dialog 
	 * @param	InMessage	Type of the message box
	 * @param	InText		Message to display
	 * @param	InTitle		Title for the message box
	 * @return	Returns the result of the modal message box
	 */
	EAppReturnType::Type OnModalMessageDialog(EAppMsgType::Type InMessage, const FText& InText, const FText& InTitle);

	/** 
	 * Returns whether an object should replace an exisiting one or not 
	 * @param	Filename		Filename of the package
	 * @return	Returns whether the objects should replace the already existing ones.
	 */
	bool OnShouldLoadOnTop(const FString& Filename);

	//@todo Slate Editor: Merge PlayMap and RequestSlatePlayMapSession
	/**
	 * Makes a request to start a play from editor session (in editor or on a remote platform)
	 * @param	StartLocation			If specified, this is the location to play from (via a PlayerStartPIE - Play From Here)
	 * @param	StartRotation			If specified, this is the rotation to start playing at
	 * @param	DestinationConsole		Where to play the game - -1 means in editor, 0 or more is an index into the GConsoleSupportContainer
	 * @param	InPlayInViewportIndex	Viewport index to play the game in, or -1 to spawn a standalone PIE window
	 * @param	bUseMobilePreview		True to enable mobile preview mode (PC platform only)
	 */
	virtual void PlayMap( const FVector* StartLocation = NULL, const FRotator* StartRotation = NULL, int32 DestinationConsole = -1, int32 InPlayInViewportIndex = -1, bool bUseMobilePreview = false );



	/**
	 * Can the editor do cook by the book in the editor process space
	 */
	virtual bool CanCookByTheBookInEditor(const FString& PlatformName ) const { return false; }

	/**
	 * Can the editor act as a cook on the fly server
	 */
	virtual bool CanCookOnTheFlyInEditor(const FString& PlatformName) const { return false; }

	/**
	 * Start cook by the book in the editor process space
	 */
	virtual void StartCookByTheBookInEditor( const TArray<ITargetPlatform*> &TargetPlatforms, const TArray<FString> &CookMaps, const TArray<FString> &CookDirectories, const TArray<FString> &CookCultures, const TArray<FString> &IniMapSections ) { }

	/**
	 * Checks if the cook by the book is finished
	 */
	virtual bool IsCookByTheBookInEditorFinished() const { return true; }

	/**
	 * Cancels the current cook by the book in editor
	 */
	virtual void CancelCookByTheBookInEditor() { }


	/**
	 * Makes a request to start a play from a Slate editor session
	 * @param	bAtPlayerStart			Whether or not we would really like to use the game or level's PlayerStart vs the StartLocation
	 * @param	DestinationViewport		Slate Viewport to play the game in, or NULL to spawn a standalone PIE window
	 * @param	bInSimulateInEditor		True to start an in-editor simulation session, or false to kick off a play-in-editor session
	 * @param	StartLocation			If specified, this is the location to play from (via a PlayerStartPIE - Play From Here)
	 * @param	StartRotation			If specified, this is the rotation to start playing at
	 * @param	DestinationConsole		Where to play the game - -1 means in editor, 0 or more is an index into the GConsoleSupportContainer
	 * @param	bUseMobilePreview		True to enable mobile preview mode (PC platform only)
	 * @param	bUseVRPreview			True to enable VR preview mode (PC platform only)
	 */
	void RequestPlaySession( bool bAtPlayerStart, TSharedPtr<class ILevelViewport> DestinationViewport, bool bInSimulateInEditor, const FVector* StartLocation = NULL, const FRotator* StartRotation = NULL, int32 DestinationConsole = -1, bool bUseMobilePreview = false, bool bUseVRPreview = false, bool bUseVulkanPreview = false);

	// @todo gmp: temp hack for Rocket demo
	void RequestPlaySession(const FVector* StartLocation, const FRotator* StartRotation, bool MobilePreview, bool VulkanPreview, const FString& MobilePreviewTargetDevice, FString AdditionalStandaloneLaunchParameters = TEXT(""));

	/** Request to play a game on a remote device */
	void RequestPlaySession( const FString& DeviceId, const FString& DeviceName );

	/** Cancel request to start a play session */
	void CancelRequestPlaySession();

	/** Asks the player to save dirty maps, if this fails it will return false and call CancelRequestPlaySession */
	bool SaveMapsForPlaySession();

	/** Makes a request to start a play from a Slate editor session */
	void RequestToggleBetweenPIEandSIE() { bIsToggleBetweenPIEandSIEQueued = true; }

	/** Called when the debugger has paused the active PIE or SIE session */
	void PlaySessionPaused();

	/** Called when the debugger has resumed the active PIE or SIE session */
	void PlaySessionResumed();

	/** Called when the debugger has single-stepped the active PIE or SIE session */
	void PlaySessionSingleStepped();

	/** Called when game client received input key */
	bool ProcessDebuggerCommands(const FKey InKey, const FModifierKeysState ModifierKeyState, EInputEvent EventType);

	/**
	 * Kicks off a "Play From Here" request that was most likely made during a transaction
	 */
	virtual void StartQueuedPlayMapRequest();

	/**
	 * Request that the current PIE/SIE session should end.  
	 * This should be used to end the game if its not safe to directly call EndPlayMap in your stack frame
	 */
	void RequestEndPlayMap();

	/**
	 * @return true if there is an end play map request queued 
	 */
	bool ShouldEndPlayMap() const { return bRequestEndPlayMapQueued; }

	/**
	 * Request to create a new PIE window and join the currently running PIE session.
	 */
	void RequestLateJoin();

	/**
	 * Builds a URL for game spawned by the editor (not including map name!). 
	 * @param	MapName			The name of the map to put into the URL
	 * @param	bSpecatorMode	If true, the player starts in spectator mode
	 * @param   AdditionalURLOptions	Additional URL Options to append (e.g, ?OptionX?OptionY). This is in addition to InEditorGameURLOptions.
	 *
	 * @return	The URL for the game
	 */
	virtual FString BuildPlayWorldURL(const TCHAR* MapName, bool bSpectatorMode = false, FString AdditionalURLOptions=FString());

	/**
	 * Starts a Play In Editor session
	 *
	 * @param	InWorld		World context
	 * @param	bInSimulateInEditor	True to start an in-editor simulation session, or false to start a play-in-editor session
	 */
	virtual void PlayInEditor( UWorld* InWorld, bool bInSimulateInEditor );

	virtual UGameInstance* CreatePIEGameInstance(int32 InPIEInstance, bool bInSimulateInEditor, bool bAnyBlueprintErrors, bool bStartInSpectatorMode, bool bPlayNetDedicated, float PIEStartTime);

	/**
	 * Kills the Play From Here session
	 */
	virtual void EndPlayMap();

	/** 
	 * Destroy the current play session and perform miscellaneous cleanup
	 */
	virtual void TeardownPlaySession(FWorldContext &PieWorldContext);

	/**
	 * Ends the current play on local pc session.
	 * @todo gmp: temp hack for Rocket demo
	 */
	virtual void EndPlayOnLocalPc();

	/**
	 * Disables any realtime viewports that are currently viewing the level.  This will not disable
	 * things like preview viewports in Cascade, etc. Typically called before running the game.
	 */
	void DisableRealtimeViewports();

	/**
	 * Restores any realtime viewports that have been disabled by DisableRealtimeViewports. This won't
	 * disable viewporst that were realtime when DisableRealtimeViewports has been called and got
	 * latter toggled to be realtime.
	 */
	void RestoreRealtimeViewports();

	/**
	 * Checks to see if any viewport is set to update in realtime.
	 */
	bool IsAnyViewportRealtime();


	/**
	 * @return true if all windows are hidden (including minimized)                                                         
	 */
	bool AreAllWindowsHidden() const;

	/**
	 *	Returns pointer to a temporary render target.
	 *	If it has not already been created, does so here.
	 */
	UTextureRenderTarget2D* GetScratchRenderTarget( const uint32 MinSize );

	/**
	 *  Returns the Editors timer manager instance.
	 */
	TSharedRef<class FTimerManager> GetTimerManager() { return TimerManager.ToSharedRef(); }

	/**
	*  Returns the Editors world manager instance.
	*/
	UEditorWorldExtensionManager* GetEditorWorldExtensionsManager() { return EditorWorldExtensionsManager; }

	// Editor specific

	/**
	* Closes the main editor frame.
	*/
	virtual void CloseEditor() {}
	virtual void GetPackageList( TArray<UPackage*>* InPackages, UClass* InClass ) {}

	/**
	 * Returns the number of currently selected actors.
	 */
	int32 GetSelectedActorCount() const;

	/**
	 * Returns the set of selected actors.
	 */
	class USelection* GetSelectedActors() const;

	/** Returns True is a world info Actor is selected */
	bool IsWorldSettingsSelected();

	/** Function to return unique list of the classes of the assets currently selected in content browser (loaded/not loaded) */
	void GetContentBrowserSelectionClasses( TArray<UClass*>& Selection ) const;

	/** Function to return list of assets currently selected in the content browser */
	void GetContentBrowserSelections(TArray<FAssetData>& Selections) const;

	/**
	 * Returns an FSelectionIterator that iterates over the set of selected actors.
	 */
	class FSelectionIterator GetSelectedActorIterator() const;

	/**
	* Returns an FSelectionIterator that iterates over the set of selected components.
	*/
	class FSelectionIterator GetSelectedComponentIterator() const;

	class FSelectedEditableComponentIterator GetSelectedEditableComponentIterator() const;

	/**
	* Returns the number of currently selected components.
	*/
	int32 GetSelectedComponentCount() const;

	/**
	* @return the set of selected components.
	*/
	class USelection* GetSelectedComponents() const;

	/**
	 * @return the set of selected non-actor objects.
	 */
	class USelection* GetSelectedObjects() const;

	/**
	 * @return the appropriate selection set for the specified object class.
	 */
	class USelection* GetSelectedSet( const UClass* Class ) const;

	/**
	 * @param	RequiredParentClass The parent class this type must implement, or null to accept anything
	 * @return	The first selected class (either a UClass type itself, or the UClass generated by a blueprint), or null if there are no class or blueprint types selected
	 */
	const UClass* GetFirstSelectedClass( const UClass* const RequiredParentClass ) const;

	/**
	 * Get the selection state of the current level (its actors and components) so that it might be restored later.
	 */
	void GetSelectionStateOfLevel(FSelectionStateOfLevel& OutSelectionStateOfLevel) const;

	/**
	 * Restore the selection state of the current level (its actors and components) from a previous state.
	 */
	void SetSelectionStateOfLevel(const FSelectionStateOfLevel& InSelectionStateOfLevel);

	/**
	 * Clears out the current map, if any, and creates a new blank map.
	 *
	 * @return	Pointer to the map that was created.
	 */
	UWorld* NewMap();

	/**
	 * Exports the current map to the specified filename.
	 *
	 * @param	InFilename					Filename to export the map to.
	 * @param	bExportSelectedActorsOnly	If true, export only the selected actors.
	 */
	void ExportMap(UWorld* InWorld, const TCHAR* InFilename, bool bExportSelectedActorsOnly);

	/**
	 * Moves selected actors to the current level.
	 *
	 * @param	InLevel		The destination level.
	 */
	DEPRECATED(4.17, "MoveSelectedActorsToLevel has been deprecated.  Use UEditorLevelUtils::MoveSelectedActorsToLevel instead")
	void MoveSelectedActorsToLevel( ULevel* InLevel );

	/**
	 *	Returns list of all foliage types used in the world
	 * 
	 * @param	InWorld	 The target world.
	 * @return	List of all foliage types used in the world
	 */
	TArray<UFoliageType*> GetFoliageTypesInWorld(UWorld* InWorld);

	/**
	 * Checks to see whether it's possible to perform a copy operation on the selected actors.
	 *
	 * @param InWorld		World to get the selected actors from
	 * @param OutCopySelected	Can be NULL, copies the internal results of the copy check to this struct
	 * @return			true if it's possible to do a copy operation based on the selection
	 */
	bool CanCopySelectedActorsToClipboard( UWorld* InWorld, FCopySelectedInfo* OutCopySelected = NULL );

	/**
	 * Copies selected actors to the clipboard.  Supports copying actors from multiple levels.
	 * NOTE: Doesn't support copying prefab instance actors!
	 *
	 * @param InWorld		World to get the selected actors from
	 * @param bShouldCut If true, deletes the selected actors after copying them to the clipboard
	 * @param bShouldCut If true, this cut is part of a move and the actors will be immediately pasted
	 */
	void CopySelectedActorsToClipboard( UWorld* InWorld, const bool bShouldCut, const bool bIsMove = false );

	/**
	 * Checks to see whether it's possible to perform a paste operation.
	 *
	 * @param InWorld		World to paste into
	 * @return				true if it's possible to do a Paste operation
	 */
	bool CanPasteSelectedActorsFromClipboard( UWorld* InWorld );

	/**
	 * Pastes selected actors from the clipboard.
	 * NOTE: Doesn't support pasting prefab instance actors!
	 *
	 * @param InWorld		World to get the selected actors from
	 * @param PasteTo		Where to paste the content too
	 */
	void PasteSelectedActorsFromClipboard( UWorld* InWorld, const FText& TransDescription, const EPasteTo PasteTo );

	/**
	 * Sets property value and property chain to be used for property-based coloration.
	 *
	 * @param	PropertyValue		The property value to color.
	 * @param	Property			The property to color.
	 * @param	CommonBaseClass		The class of object to color.
	 * @param	PropertyChain		The chain of properties from member to lowest property.
	 */
	virtual void SetPropertyColorationTarget(UWorld* InWorld, const FString& PropertyValue, class UProperty* Property, class UClass* CommonBaseClass, class FEditPropertyChain* PropertyChain);

	/**
	 * Accessor for current property-based coloration settings.
	 *
	 * @param	OutPropertyValue	[out] The property value to color.
	 * @param	OutProperty			[out] The property to color.
	 * @param	OutCommonBaseClass	[out] The class of object to color.
	 * @param	OutPropertyChain	[out] The chain of properties from member to lowest property.
	 */
	virtual void GetPropertyColorationTarget(FString& OutPropertyValue, UProperty*& OutProperty, UClass*& OutCommonBaseClass, FEditPropertyChain*& OutPropertyChain);

	/**
	 * Selects actors that match the property coloration settings.
	 */
	void SelectByPropertyColoration(UWorld* InWorld);

	/**
	 * Warns the user of any hidden levels, and prompts them with a Yes/No dialog
	 * for whether they wish to continue with the operation.  No dialog is presented if all
	 * levels are visible.  The return value is true if no levels are hidden or
	 * the user selects "Yes", or false if the user selects "No".
	 *
	 * @param	InWorld					World context
	 * @param	bIncludePersistentLvl	If true, the persistent level will also be checked for visibility
	 * @return							false if the user selects "No", true otherwise.
	 */
	bool WarnAboutHiddenLevels( UWorld* InWorld, bool bIncludePersistentLvl) const;

	void ApplyDeltaToActor(AActor* InActor, bool bDelta, const FVector* InTranslation, const FRotator* InRotation, const FVector* InScaling, bool bAltDown=false, bool bShiftDown=false, bool bControlDown=false) const;

	void ApplyDeltaToComponent(USceneComponent* InComponent, bool bDelta, const FVector* InTranslation, const FRotator* InRotation, const FVector* InScaling, const FVector& PivotLocation ) const;

	/**
	 * Disable actor/component modification during delta movement.
	 *
	 * @param bDisable True if modification should be disabled; false otherwise.
	 */
	void DisableDeltaModification(bool bDisable) { bDisableDeltaModification = bDisable; }

	/**
	 *	Game-specific function called by Map_Check BEFORE iterating over all actors.
	 *
	 *	@param	Str						The exec command parameters
	 *	@param	Ar						The output archive for logging (?)
	 *	@param	bCheckDeprecatedOnly	If true, only check for deprecated classes
	 */
	virtual bool Game_Map_Check(UWorld* InWorld, const TCHAR* Str, FOutputDevice& Ar, bool bCheckDeprecatedOnly) { return true; }

	/**
	 *	Game-specific function called per-actor by Map_Check
	 *
	 *	@param	Str						The exec command parameters
	 *	@param	Ar						The output archive for logging (?)
	 *	@param	bCheckDeprecatedOnly	If true, only check for deprecated classes
	 */
	virtual bool Game_Map_Check_Actor(const TCHAR* Str, FOutputDevice& Ar, bool bCheckDeprecatedOnly, AActor* InActor) { return true; }

	/**
	 * Auto merge all staticmeshes that are able to be merged
	 */
	void AutoMergeStaticMeshes();

	/**
	 *	Check the InCmdParams for "MAPINISECTION=<name of section>".
	 *	If found, fill in OutMapList with the proper map names.
	 *
	 *	@param	InCmdParams		The cmd line parameters for the application
	 *	@param	OutMapList		The list of maps from the ini section, empty if not found
	 */
	void ParseMapSectionIni(const TCHAR* InCmdParams, TArray<FString>& OutMapList);

	/**
	 *	Load the list of maps from the given section of the Editor.ini file
	 *	Supports sections contains other sections - but DOES NOT HANDLE CIRCULAR REFERENCES!!!
	 *
	 *	@param	InSectionName		The name of the section to load
	 *	@param	OutMapList			The list of maps from that section
	 */
	void LoadMapListFromIni(const FString& InSectionName, TArray<FString>& OutMapList);

	/**
	 *	Check whether the specified package file is a map
	 *
	 *	@param	PackageFilename		The name of the package to check
	 *	@param	OutErrorMsg			if an errors occurs, this is the localized reason
	 *	@return	true if the package is not a map file
	 */
	bool PackageIsAMapFile( const TCHAR* PackageFilename, FText& OutNotMapReason );

	/**
	 *	Searches through the given ULevel for any external references. Prints any found UObjects to the log and informs the user
	 *
	 *	@param	LevelToCheck		ULevel to search through for external objects
	 *	@param	bAddForMapCheck		Optional flag to add any found references to the Map Check dialog (defaults to false)
	 *	@return	true if the given package has external references and the user does not want to ignore the warning (via prompt)
	 */
	bool PackageUsingExternalObjects(ULevel* LevelToCheck, bool bAddForMapCheck=false);

	/**
	 * Synchronizes the content or generic browser's selected objects to the collection specified.
	 *
	 * @param	ObjectSet	the list of objects to sync to
	 */
	void SyncBrowserToObjects( TArray<UObject*>& InObjectsToSync, bool bFocusContentBrowser = true );
	void SyncBrowserToObjects( TArray<struct FAssetData>& InAssetsToSync, bool bFocusContentBrowser = true );

	/**
	 * Syncs the selected actors objects to the content browser
	 */
	void SyncToContentBrowser();

	/**
	 * Syncs the selected actors' levels to the content browser
	 */
	void SyncActorLevelsToContentBrowser();

	/**
	 * Checks if the slected objects contain something to browse to
	 *
	 * @return true if any of the selected objects contains something that can be browsed to
	 */
	bool CanSyncToContentBrowser();

	/**
	 * Checks if the selected objects have levels which can be browsed to
	 *
	 * @return true if any of the selected objects contains something that can be browsed to
	 */
	bool CanSyncActorLevelsToContentBrowser();

	/**
	 * Toggles the movement lock on selected actors so they may or may not be moved with the transform widgets
	 */
	void ToggleSelectedActorMovementLock();

	/**
	 * @return true if there are selected locked actors
	 */
	bool HasLockedActors();

	/**
	 * Opens the objects specialized editor (Same as double clicking on the item in the content browser)
	 *
	 * @param ObjectToEdit	The object to open the editor for 
	 */
	void EditObject( UObject* ObjectToEdit );

	/**
	 * Selects the currently selected actor(s) levels in the level browser
	 *
	 * @param bDeselectOthers	Whether or not to deselect the current level selection first
	 */
	void SelectLevelInLevelBrowser( bool bDeselectOthers );

	/**
	 * Deselects the currently selected actor(s) levels in the level browser
	 */
	void DeselectLevelInLevelBrowser();

	/**
	 * Selects all actors controlled by currently selected MatineeActor
	 */
	void SelectAllActorsControlledByMatinee();

	/**
	 * Selects all actors with the same class as the current selection
	 *
	 * @param bArchetype	true to only select actors of the same class AND same Archetype
	 */
	void SelectAllActorsWithClass( bool bArchetype );

	/**
	 * Finds all references to the currently selected actors, and reports results in a find message log
	 */
	void FindSelectedActorsInLevelScript();

	/** See if any selected actors are referenced in level script */
	bool AreAnySelectedActorsInLevelScript();

	/**
	 * Checks if a provided package is valid to be auto-added to a default changelist based on several
	 * specifications (and if the user has elected to enable the feature). Only brand-new packages will be
	 * allowed.
	 *
	 * @param	InPackage	Package to consider the validity of auto-adding to a default source control changelist
	 * @param	InFilename	Filename of the package after it will be saved (not necessarily the same as the filename of the first param in the event of a rename)
	 *
	 * @return	true if the provided package is valid to be auto-added to a default source control changelist
	 */
	bool IsPackageValidForAutoAdding(UPackage* InPackage, const FString& InFilename);

	/**
	 * Checks if a provided package is valid to be saved. For startup packages, it asks user for confirmation.
	 *
	 * @param	InPackage	Package to consider the validity of auto-adding to a default source control changelist
	 * @param	InFilename	Filename of the package after it will be saved (not necessarily the same as the filename of the first param in the event of a rename)
	 * @param	Error		Error output
	 *
	 * @return	true if the provided package is valid to be saved
	 */
	virtual bool IsPackageOKToSave(UPackage* InPackage, const FString& InFilename, FOutputDevice* Error);

	/** The editor wrapper for UPackage::SavePackage. Auto-adds files to source control when necessary */
	bool SavePackage( UPackage* InOuter, UObject* Base, EObjectFlags TopLevelFlags, const TCHAR* Filename, 
		FOutputDevice* Error=GError, FLinkerLoad* Conform=NULL, bool bForceByteSwapping=false, bool bWarnOfLongFilename=true, 
		uint32 SaveFlags=SAVE_None, const class ITargetPlatform* TargetPlatform = NULL, const FDateTime& FinalTimeStamp = FDateTime::MinValue(), bool bSlowTask = true );

	/** The editor wrapper for UPackage::Save. Auto-adds files to source control when necessary */
	FSavePackageResultStruct Save(UPackage* InOuter, UObject* Base, EObjectFlags TopLevelFlags, const TCHAR* Filename,
		FOutputDevice* Error = GError, FLinkerLoad* Conform = NULL, bool bForceByteSwapping = false, bool bWarnOfLongFilename = true,
		uint32 SaveFlags = SAVE_None, const class ITargetPlatform* TargetPlatform = NULL, const FDateTime& FinalTimeStamp = FDateTime::MinValue(), bool bSlowTask = true);

	/** Invoked before a UWorld is saved to update editor systems */
	virtual void OnPreSaveWorld(uint32 SaveFlags, UWorld* World);

	/** Invoked after a UWorld is saved to update editor systems */
	virtual void OnPostSaveWorld(uint32 SaveFlags, UWorld* World, uint32 OriginalPackageFlags, bool bSuccess);

	/**
	 * Adds provided package to a default changelist
	 *
	 * @param	PackageNames	Filenames of the packages after they will be saved (not necessarily the same as the filename of the first param in the event of a rename)
	 */
	void AddPackagesToDefaultChangelist(TArray<FString>& InPackageNames);

	/**
	 * Delegate used when a source control connection dialog has been closed.
	 * @param	bEnabled	Whether source control was enabled or not
	 */
	void OnSourceControlDialogClosed(bool bEnabled);

	/** 
	 * @return - returns the currently selected positional snap grid setting
	 */
	float GetGridSize();

	/** 
	 * @return - if the grid size is part of the 1,2,4,8,16,.. list or not
	 */
	bool IsGridSizePowerOfTwo() const;

	/** 
	 * Sets the selected positional snap grid setting
	 *
	 * @param InIndex - The index of the selected grid setting
	 */
	void SetGridSize(int32 InIndex);

	/** 
	 * Increase the positional snap grid setting (will not increase passed the maximum value)
	 */
	void GridSizeIncrement();
	
	/** 
	 * Decreased the positional snap grid setting (will not decrease passed the minimum value)
	 */
	void GridSizeDecrement();

	/** 
	 * Accesses the array of snap grid position options
	 */
	const TArray<float>& GetCurrentPositionGridArray() const;
	
	/** 
	 * @return - returns the currently selected rotational snap grid setting
	 */
	FRotator GetRotGridSize();

	/** 
	 * Sets the selected Rotational snap grid setting
	 *
	 * @param InIndex - The index of the selected grid setting
	 * @param InGridMode - Selects which set of grid settings to use
	 */
	void SetRotGridSize(int32 InIndex, enum ERotationGridMode InGridMode);

	/** 
	 * Increase the rotational snap grid setting (will not increase passed the maximum value)
	 */
	void RotGridSizeIncrement();
	
	/** 
	 * Decreased the rotational snap grid setting (will not decrease passed the minimum value)
	 */
	void RotGridSizeDecrement();
	
	/** 
	 * Accesses the array of snap grid rotation options
	 */
	const TArray<float>& GetCurrentRotationGridArray() const;
	
	float GetScaleGridSize();
	void SetScaleGridSize(int32 InIndex);

	float GetGridInterval();

	/**
	 * Access the array of grid interval options
	 */
	const TArray<float>& GetCurrentIntervalGridArray() const;
	
	 /**
	  * Function to convert selected brushes into volumes of the provided class.
	  *
	  * @param	VolumeClass	Class of volume that selected brushes should be converted into
	  */
	void ConvertSelectedBrushesToVolumes( UClass* VolumeClass );

	/**
	 * Called to convert actors of one class type to another
	 *
	 * @param FromClass The class converting from
	 * @param ToClass	The class converting to
	 */
	void ConvertActorsFromClass( UClass* FromClass, UClass* ToClass );

	/**
	 * Gets a delegate that is executed when a matinee is requested to be opened
	 *
	 * The first parameter is the matinee actor
	 *
	 * @return The event delegate.
	 */
	DECLARE_DELEGATE_RetVal_OneParam(bool, FShouldOpenMatineeCallback, AMatineeActor*)
	FShouldOpenMatineeCallback& OnShouldOpenMatinee() { return ShouldOpenMatineeCallback; }

	/** 
	 * Show a (Suppressable) warning dialog to remind the user he is about to lose his undo buffer 
	 *
	 * @param MatineeActor	The actor we wish to check (can be null)
	 * @returns true if the user wishes to proceed
	 */
	bool ShouldOpenMatinee(AMatineeActor* MatineeActor) const;

	/** 
	 * Open the Matinee tool to edit the supplied MatineeActor. Will check that MatineeActor has an InterpData attached.
	 *
	 * @param MatineeActor	The actor we wish to edit
	 * @param bWarnUser		If true, calls ShouldOpenMatinee as part of the open process
	 */
	void OpenMatinee(class AMatineeActor* MatineeActor, bool bWarnUser=true);

	/**
	* Update any outstanding reflection captures
	*/
	void UpdateReflectionCaptures(UWorld* World = GWorld);

	/**
	 * Convenience method for adding a Slate modal window that is parented to the main frame (if it exists)
	 * This function does not return until the modal window is closed.
	 */
	void EditorAddModalWindow( TSharedRef<SWindow> InModalWindow ) const;

	/**
	 * Finds a brush builder of the provided class.  If it doesnt exist one will be created
	 *
	 * @param BrushBuilderClass	The class to get the builder brush from
	 * @return The found or created brush builder
	 */
	UBrushBuilder* FindBrushBuilder( UClass* BrushBuilderClass );

	/**
	 * Parents one actor to another
	 * Check the validity of this call with CanParentActors().
	 *
	 * @param ParentActor	Actor to parent the child to
	 * @param ChildActor	Actor being parented
	 * @param SocketName	An optional socket name to attach to in the parent (use NAME_None when there is no socket)
	 * @param Component		Actual Component included in the ParentActor which is a usually blueprint actor
	 */
	void ParentActors( AActor* ParentActor, AActor* ChildActor, const FName SocketName, USceneComponent* Component=NULL );

	/**
	 * Detaches selected actors from there parents
	 *
	 * @return				true if a detachment occurred
	 */
	bool DetachSelectedActors();	

	/**
	 * Checks the validity of parenting one actor to another
	 *
	 * @param ParentActor	Actor to parent the child to
	 * @param ChildActor	Actor being parented
	 * @param ReasonText	Optional text to receive a description of why the function returned false.
	 * @return				true if the parenting action is valid and will succeed, false if invalid.
	 */
	bool CanParentActors( const AActor* ParentActor, const AActor* ChildActor, FText* ReasonText = NULL);

	/** 
	 * turns all navigable static geometry of ULevel into polygon soup stored in passed Level (ULevel::StaticNavigableGeometry)
	 */
	virtual void RebuildStaticNavigableGeometry(ULevel* Level);

	/**
	 * Gets all objects which can be synced to in content browser for current selection
	 *
	 * @param Objects	Array to be filled with objects which can be browsed to
	 */
	void GetObjectsToSyncToContentBrowser( TArray<UObject*>& Objects );

	/**
	 * Gets all levels which can be synced to in content browser for current selection
	 *
	 * @param Objects	Array to be filled with ULevel objects
	 */
	void GetLevelsToSyncToContentBrowser(TArray<UObject*>& Objects);

	/**
	* Queries for a list of assets that are referenced by the current editor selection (actors, surfaces, etc.)
	*
	* @param	Objects								Array to be filled with asset objects referenced by the current editor selection
	* @param	bIgnoreOtherAssetsIfBPReferenced	If true, and a selected actor has a Blueprint asset, only that will be returned.
	*/
	void GetReferencedAssetsForEditorSelection(TArray<UObject*>& Objects, const bool bIgnoreOtherAssetsIfBPReferenced = false);

	/** Returns the WorldContext for the editor world. For now, there will always be exactly 1 of these in the editor. 
	 *
	 * @param	bEnsureIsGWorld		Temporarily allow callers to validate that this maps to GWorld to help track down any conversion bugs
	 */
	FWorldContext &GetEditorWorldContext(bool bEnsureIsGWorld = false);

	/** Returns the WorldContext for the PIE world.
	*/
	FWorldContext* GetPIEWorldContext();

	/** 
	 * mostly done to check if PIE is being set up, go GWorld is going to change, and it's not really _the_G_World_
	 * NOTE: hope this goes away once PIE and regular game triggering are not that separate code paths
	 */
	virtual bool IsSettingUpPlayWorld() const override { return EditorWorld != NULL && PlayWorld == NULL; }

	/**
	 *	Retrieves the active viewport from the editor.
	 *
	 *	@return		The currently active viewport.
	 */
	FViewport* GetActiveViewport();

	/**
	 *	Retrieves the PIE viewport from the editor.
	 *
	 *	@return		The PIE viewport. NULL if there is no PIE viewport active.
	 */
	FViewport* GetPIEViewport();

	/** 
	 *	Checks for any player starts and returns the first one found.
	 *
	 *	@return		The first player start location found.
	 */
	APlayerStart* CheckForPlayerStart();

	/** Closes the popup created for GenericTextEntryModal or GenericTextEntryModeless*/
	void CloseEntryPopupWindow();

	/**
	 * Prompts the user to save the current map if necessary, then creates a new (blank) map.
	 */
	void CreateNewMapForEditing();

	/**
	 * If a PIE world exists, give the user the option to terminate it.
	 *
	 * @return				true if a PIE session exists and the user refused to end it, false otherwise.
	 */
	bool ShouldAbortBecauseOfPIEWorld() const;

	/**
	 * If an unsaved world exists that would be lost in a map transition, give the user the option to cancel a map load.
	 *
	 * @return				true if an unsaved world exists and the user refused to continue, false otherwise.
	 */
	bool ShouldAbortBecauseOfUnsavedWorld() const;

	/**
	 * Gets the user-friendly, localized (if exists) name of a property
	 *
	 * @param	Property	the property we want to try to et the friendly name of	
	 * @param	OwnerStruct	if specified, uses this class's loc file instead of the property's owner class
	 *						useful for overriding the friendly name given a property inherited from a parent class.
	 *
	 * @return	the friendly name for the property.  localized first, then metadata, then the property's name.
	 */
	static FString GetFriendlyName( const UProperty* Property, UStruct* OwnerStruct = NULL );

	/**
	 * Register a client tool to receive undo events 
	 * @param UndoClient	An object wanting to receive PostUndo/PostRedo events
	 */
	void RegisterForUndo(class FEditorUndoClient* UndoClient );

	/**
	 * Unregister a client from receiving undo events 
	 * @param UndoClient	An object wanting to unsubscribe from PostUndo/PostRedo events
	 */
	void UnregisterForUndo( class FEditorUndoClient* UndoEditor );

	/** 
	 * Are we playing on a local PC session?
	 */
	bool IsPlayingOnLocalPCSession() const { return bPlayOnLocalPcSession && !bIsPlayWorldQueued; }

	/** 
	 * Are we playing via the Launcher?
	 */
	bool IsPlayingViaLauncher() const { return bPlayUsingLauncher && !bIsPlayWorldQueued; }

	/** 
	 * Cancel playing via the Launcher
	 */
	void CancelPlayingViaLauncher();

	/** @return true if the editor is able to launch PIE with online platform support */
	bool SupportsOnlinePIE() const;

	/** @return true if there are active PIE instances logged into an online platform */
	bool IsPlayingWithOnlinePIE() const { return NumOnlinePIEInstances > 0; }

	/**
	 * Ensures the assets specified are loaded and adds them to the global selection set
	 * @param Assets		An array of assets to load and select
	 * @param TypeOfAsset	An optional class to restrict the types of assets loaded & selected
	 */
	void LoadAndSelectAssets( TArray<FAssetData>& Assets, UClass* TypeOfAsset=NULL );

	/**
	 * @return True if percentage based scaling is enabled
	 */
	bool UsePercentageBasedScaling() const;

	DECLARE_DELEGATE(FPIEInstanceWindowSwitch);

	/** Sets the delegate for when the focused PIE window is changed */
	void SetPIEInstanceWindowSwitchDelegate(FPIEInstanceWindowSwitch PIEInstanceWindowSwitchDelegate);

	/**
	 * Returns the actor grouping utility class that performs all grouping related tasks
	 * This will create the class instance if it doesn't exist.
	 */
	class UActorGroupingUtils* GetActorGroupingUtils();

	/**
	 * Query to tell if the editor is currently in a mode where it wants XR HMD
	 * tracking to be used (like in the VR editor or VR PIE preview).
	 */
	bool IsHMDTrackingAllowed() const;

private:
	//
	// Map execs.
	//

	bool Map_Select( UWorld* InWorld, const TCHAR* Str, FOutputDevice& Ar);
	bool Map_Brush( UWorld* InWorld, const TCHAR* Str, FOutputDevice& Ar);
	bool Map_Sendto( UWorld* InWorld, const TCHAR* Str, FOutputDevice& Ar);
	bool Map_Rebuild(UWorld* InWorld, const TCHAR* Str, FOutputDevice& Ar);
	bool Map_Load(const TCHAR* Str, FOutputDevice& Ar);
	bool Map_Import( UWorld* InWorld, const TCHAR* Str, FOutputDevice& Ar );

	struct EMapCheckNotification
	{
		enum Type
		{
			DontDisplayResults,
			DisplayResults,
			NotifyOfResults,
		};
	};

	/**
	 * Checks map for common errors.
	 */
	bool Map_Check(UWorld* InWorld, const TCHAR* Str, FOutputDevice& Ar, bool bCheckDeprecatedOnly, EMapCheckNotification::Type Notification = EMapCheckNotification::DisplayResults, bool bClearLog = true);
	bool Map_Scale( UWorld* InWorld, const TCHAR* Str, FOutputDevice& Ar );
	bool Map_Setbrush( UWorld* InWorld, const TCHAR* Str, FOutputDevice& Ar );

	/**
	 * Attempts to load a preview static mesh from the array preview static meshes at the given index.
	 *
	 * @param	Index	The index of the name in the PlayerPreviewMeshNames array from the editor user settings.
	 * @return	true if a static mesh was loaded; false, otherwise.
	 */
	bool LoadPreviewMesh( int32 Index );

	/**
	 * Login PIE instances with the online platform before actually creating any PIE worlds
	 *
	 * @param bAnyBlueprintErrors passthrough value of blueprint errors encountered during PIE creation
	 * @param bStartInSpectatorMode passthrough value if it is expected that PIE will start in spectator mode
	 * @param PIEStartTime passthrough value of the time that PIE was initiated by the user
	 */
	virtual void LoginPIEInstances(bool bAnyBlueprintErrors, bool bStartInSpectatorMode, double PIEStartTime);

	/**
	 * Delegate called as each PIE instance login is complete, continues creating the PIE world for a single instance
	 * 
	 * @param LocalUserNum local user id, for PIE is going to be 0 (there is no splitscreen)
	 * @param bWasSuccessful was the login successful
	 * @param ErrorString descriptive error when applicable
	 * @param DataStruct data required to continue PIE creation, set at login time
	 */
	virtual void OnLoginPIEComplete(int32 LocalUserNum, bool bWasSuccessful, const FString& ErrorString, FPieLoginStruct DataStruct);

	/** Above function but called a frame later, to stop PIE login from happening from a network callback */
	virtual void OnLoginPIEComplete_Deferred(int32 LocalUserNum, bool bWasSuccessful, FString ErrorString, FPieLoginStruct DataStruct);

	/** Called when all PIE instances have been successfully logged in */
	virtual void OnLoginPIEAllComplete();

public:
	/** Creates a pie world from the default entry map, used by clients that connect to a PIE server */
	UWorld* CreatePIEWorldFromEntry(FWorldContext &WorldContext, UWorld* InWorld, FString &PlayWorldMapName);

	/**
	 * Continue the creation of a single PIE world after a login was successful
	 *
	 * @param PieWorldContext world context for this PIE instance
	 * @param PlayNetMode mode to create this PIE world in (as server, client, etc)
	 * @param DataStruct data required to continue PIE creation, set at login time
	 * @return	true if world created successfully
	 */
	bool CreatePIEWorldFromLogin(FWorldContext& PieWorldContext, EPlayNetMode PlayNetMode, FPieLoginStruct& DataStruct);

	/*
	 * Handler for when viewport close request is made. 
	 *
	 * @param InViewport the viewport being closed.
	 */
	void OnViewportCloseRequested(FViewport* InViewport);

private:
	/** Gets the scene viewport for a viewport client */
	const FSceneViewport* GetGameSceneViewport(UGameViewportClient* ViewportClient) const;

	/**
	 * Non Online PIE creation flow, creates all instances of PIE at once when online isn't requested/required
	 *
	 * @param bAnyBlueprintErrors passthrough value of blueprint errors encountered during PIE creation
	 * @param bStartInSpectatorMode passthrough value if it is expected that PIE will start in spectator mode
	 */
	void SpawnIntraProcessPIEWorlds(bool bAnyBlueprintErrors, bool bStartInSpectatorMode);

	/** Common init shared by CreatePIEWorldByDuplication and CreatePIEWorldBySavingToTemp */
	void PostCreatePIEWorld(UWorld *InWorld);

	/**
	 * Toggles PIE to SIE or vice-versa
	 */
	void ToggleBetweenPIEandSIE( bool bNewSession = false);

	/**
	 * Hack to switch worlds for the PIE window before and after a slate event
	 *
	 * @param WorldID	The id of the world to restore or -1 if no world
	 * @return The ID of the world to restore later or -1 if no world to restore
	 */
	int32 OnSwitchWorldForSlatePieWindow( int32 WorldID );

	/**
	 * Called via a delegate to toggle between the editor and pie world
	 */
	void OnSwitchWorldsForPIE( bool bSwitchToPieWorld );

	/**
	 * Gives focus to the server or first PIE client viewport
	 */
	void GiveFocusToFirstClientPIEViewport();

public:
	/**
	 * Spawns a PlayFromHere playerstart in the given world
	 * @param	World		The World to spawn in (for PIE this may not be GWorld)
	 * @param	PlayerStartPIE	A reference to the resulting PlayerStartPIE actor
	 *
	 * @return	true if spawn succeeded, false if failed
	 */
	bool SpawnPlayFromHereStart(UWorld* World, AActor*& PlayerStartPIE, const FVector& StartLocation, const FRotator& StartRotation );


private:
	/**
	 * Delegate definition for the execute function that follows
	 */
	DECLARE_DELEGATE_OneParam( FSelectCommand, UModel* );
	DECLARE_DELEGATE_TwoParams( FSelectInWorldCommand, UWorld*, UModel* );
	
	/**
	 * Utility method call a select command for each level model in the worlds level list.
	 *
	 * @param InWorld			World containing the levels
	 * @param InSelectCommand	Select command delegate
	 */
	void ExecuteCommandForAllLevelModels( UWorld* InWorld, FSelectCommand InSelectCommand, const FText& TransDesription );
	void ExecuteCommandForAllLevelModels( UWorld* InWorld, FSelectInWorldCommand InSelectCommand, const FText& TransDesription );
	
public:

	/**
	 * Utility method call ModifySelectedSurfs for each level model in the worlds level list.
	 *
	 * @param InWorld			World containing the levels
	 */
	 void FlagModifyAllSelectedSurfacesInLevels( UWorld* InWorld );
	 
private:
	/** Checks for UWorld garbage collection leaks and reports any that are found */
	void CheckForWorldGCLeaks( UWorld* NewWorld, UPackage* WorldPackage );

	/**
	 * This destroys the given world.
	 * It also does editor specific tasks (EG clears selections from that world etc)
	 *
	 * @param	Context		The world to destroy
	 * @param	CleanseText	Reason for the destruction
	 * @param	NewWorld	An optional new world to keep in memory after destroying the world referenced in the Context.
	 *						This world and it's sublevels will remain in memory.
	 */
	void EditorDestroyWorld( FWorldContext & Context, const FText& CleanseText, UWorld* NewWorld = nullptr );

	ULevel* CreateTransLevelMoveBuffer( UWorld* InWorld );

	/**	Broadcasts that an undo has just occurred. */
	void BroadcastPostUndo(const FString& UndoContext, UObject* PrimaryObject, bool bUndoSuccess);
	
	/**	Broadcasts that an redo has just occurred. */
	void BroadcastPostRedo(const FString& RedoContext, UObject* PrimaryObject, bool bRedoSuccess);

	/** Helper function to show undo/redo notifications */
	void ShowUndoRedoNotification(const FText& NotificationText, bool bSuccess);

	/**	Broadcasts that the supplied objects have been replaced (map of old object to new object */
	void BroadcastObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap) { ObjectsReplacedEvent.Broadcast(ReplacementMap); }

	/** Internal helper functions */
	virtual void PostUndo (bool bSuccess);

	/** Delegate callback: the world origin is going to be moved. */
	void PreWorldOriginOffset(UWorld* InWorld, FIntVector InSrcOrigin, FIntVector InDstOrigin);

	/** Delegate callback for when a streaming level is added to world. */
	void OnLevelAddedToWorld(ULevel* InLevel, UWorld* InWorld);

	/** Delegate callback for when a streaming level is removed from world. */
	void OnLevelRemovedFromWorld(ULevel* InLevel, UWorld* InWorld);

	/** Delegate callback for when a streamed out levels going to be removed by GC. */
	void OnGCStreamedOutLevels();

	/** Puts the currently loaded project file at the top of the recents list and trims and files that fell off the list */
	void UpdateRecentlyLoadedProjectFiles();

	/** Updates the project file to auto load and initializes the bLoadTheMostRecentlyLoadedProjectAtStartup flag */
	void UpdateAutoLoadProject();

	/** Handles user setting changes. */
	void HandleSettingChanged( FName Name );

	/** Callback for handling undo and redo transactions before they happen. */
	void HandleTransactorBeforeRedoUndo( FUndoSessionContext SessionContext );

	/** Callback for finished redo transactions. */
	void HandleTransactorRedo( FUndoSessionContext SessionContext, bool Succeeded );

	/** Callback for finished undo transactions. */
	void HandleTransactorUndo( FUndoSessionContext SessionContext, bool Succeeded );

private:

	/** Delegate broadcast just before a blueprint is compiled */
	FBlueprintPreCompileEvent BlueprintPreCompileEvent;

	/** Delegate broadcast when blueprint is compiled */
	FBlueprintCompiledEvent BlueprintCompiledEvent;

	/** Delegate broadcast when blueprint is reinstanced */
	FBlueprintReinstanced BlueprintReinstanced;

	/** Delegate broadcast when objects have been replaced (e.g on blueprint compile) */
	FObjectsReplacedEvent ObjectsReplacedEvent;

	/** Delegate broadcast when a package has been loaded or unloaded */
	FClassPackageLoadedOrUnloadedEvent ClassPackageLoadedOrUnloadedEvent;

	/** Delegate broadcast when an object has been reimported */
	FObjectReimported ObjectReimportedEvent;

	/** Delegate broadcast when an actor or component is about to be moved, rotated, or scaled  */
	FOnBeginTransformObject OnBeginObjectTransformEvent;

	/** Delegate broadcast when an actor or component has been moved, rotated, or scaled */
	FOnEndTransformObject OnEndObjectTransformEvent;

	/** Delegate broadcast when the camera viewed through the viewport is about to be moved */
	FOnBeginTransformCamera OnBeginCameraTransformEvent;

	/** Delegate broadcast when the camera viewed through the viewport has been moved */
	FOnEndTransformCamera OnEndCameraTransformEvent;
	
	/** Broadcasts after an HLOD actor has been moved between clusters */
	FHLODActorMovedEvent HLODActorMovedEvent;

	/** Broadcasts after an HLOD actor's mesh is build*/
	FHLODMeshBuildEvent HLODMeshBuildEvent;

	/** Broadcasts after an HLOD actor has added to a cluster */
	FHLODActorAddedEvent HLODActorAddedEvent;

	/** Broadcasts after an HLOD actor has been marked dirty */
	FHLODActorMarkedDirtyEvent HLODActorMarkedDirtyEvent;

	/** Broadcasts after a Draw distance value (World settings) is changed */
	FHLODTransitionScreenSizeChangedEvent HLODTransitionScreenSizeChangedEvent;

	/** Broadcasts after the HLOD levels array is changed */
	FHLODLevelsArrayChangedEvent HLODLevelsArrayChangedEvent;

	/** Broadcasts after an Actor is removed from a cluster */
	FHLODActorRemovedFromClusterEvent HLODActorRemovedFromClusterEvent;

	/** Delegate broadcast by the engine every tick when PIE/SIE is active, to check to see whether we need to
		be able to capture state for simulating actor (for Sequencer recording features) */
	FGetActorRecordingState GetActorRecordingStateEvent;

	/** Delegate to be called when a matinee is requested to be opened */
	FShouldOpenMatineeCallback ShouldOpenMatineeCallback;

	/** Reference to owner of the current popup */
	TWeakPtr<class SWindow> PopupWindow;

	/** True if we should disable actor/component modification on delta movement */
	bool bDisableDeltaModification;

	/** List of editors who want to receive undo/redo events */
	TSet< class FEditorUndoClient* > UndoClients;

	/** List of actors that were selected before Undo/redo */
	TArray<AActor*> OldSelectedActors;

	/** List of components that were selected before Undo/redo */
	TArray<UActorComponent*> OldSelectedComponents;

	/** The notification item to use for undo/redo */
	TSharedPtr<class SNotificationItem> UndoRedoNotificationItem;

	/** The Timer manager for all timer delegates */
	TSharedPtr<class FTimerManager> TimerManager;

	/** The output log -> message log redirector for use during PIE */
	TSharedPtr<class FOutputLogErrorsToMessageLogProxy> OutputLogErrorsToMessageLogProxyPtr;

	struct FPlayOnPCInfo
	{
		FProcHandle ProcessHandle;
	};

	TArray<FPlayOnPCInfo> PlayOnLocalPCSessions;

	bool bPlayOnLocalPcSession;

	/** True if we are using the "Play On Device" launcher mode (ie UAT) */
	// @todo: This should be an enum along with bPlayOnLocalPcSession, or completely refactored
	bool bPlayUsingLauncher;
	bool bPlayUsingMobilePreview;
	bool bPlayUsingVulkanPreview;
	FString PlayUsingMobilePreviewTargetDevice;

	/** The platform to run on (as selected in dreop down) */
	FString PlayUsingLauncherDeviceId;
	FString PlayUsingLauncherDeviceName;
	bool bPlayUsingLauncherHasCode;
	bool bPlayUsingLauncherBuild;

	/** Used to prevent reentrant calls to EndPlayMap(). */
	bool bIsEndingPlay;

	/** List of files we are deferring adding to source control */
	TArray<FString> DeferredFilesToAddToSourceControl;

	FPIEInstanceWindowSwitch PIEInstanceWindowSwitchDelegate;

	/** Number of currently running instances logged into an online platform */
	int32 NumOnlinePIEInstances;

	/** Cached version of the view location at the point the PIE session was ended */
	FVector LastViewLocation;
	
	/** Cached version of the view location at the point the PIE session was ended */
	FRotator LastViewRotation;
	
	/** Are the lastview/rotation variables valid */
	bool bLastViewAndLocationValid;

protected:

	/**
	 * Launch a standalone instance on this PC.
	 *
	 * @param	MapNameOverride		Map name override
	 * @param	WindowPos			Position we want to put the window we create.
	 * @param	PIENum				PIE instance count
	 * @param	bIsServer			Is this instance a server.
	 */
	void PlayStandaloneLocalPc(FString MapNameOverride = FString(), FIntPoint* WindowPos = NULL, int32 PIENum = 0, bool bIsServer = false);

	void PlayUsingLauncher();

	/** Called when Matinee is opened */
	virtual void OnOpenMatinee(){};

	/**
	 * Invalidates all editor viewports and hit proxies, used when global changes like Undo/Redo may have invalidated state everywhere
	 */
	void InvalidateAllViewportsAndHitProxies();

	/** Initialize Portal RPC. */
	void InitializePortal();

	/** Destroy any online subsystems generated by PIE */
	void CleanupPIEOnlineSessions(TArray<FName> OnlineIdentifiers);

	// launch on callbacks
	void HandleStageStarted(const FString& InStage, TWeakPtr<SNotificationItem> NotificationItemPtr);
	void HandleStageCompleted(const FString& InStage, double StageTime, bool bHasCode, TWeakPtr<SNotificationItem> NotificationItemPtr);
	void HandleLaunchCanceled(double TotalTime, bool bHasCode, TWeakPtr<SNotificationItem> NotificationItemPtr);
	void HandleLaunchCompleted(bool Succeeded, double TotalTime, int32 ErrorCode, bool bHasCode, TWeakPtr<SNotificationItem> NotificationItemPtr, TSharedPtr<class FMessageLog> MessageLog);

	// Handle requests from slate application to open assets.
	bool HandleOpenAsset(UObject* Asset);

	// Handles a package being reloaded.
	void HandlePackageReloaded(const EPackageReloadPhase InPackageReloadPhase, FPackageReloadedEvent* InPackageReloadedEvent);

public:
	DEPRECATED(4.17, "IsUsingWorldAssets is now always true, remove any code that assumes it could be false")
	static bool IsUsingWorldAssets() { return true; }

private:
	/** Handler for when any asset is loaded in the editor */
	void OnAssetLoaded( UObject* Asset );

	/** Handler for when an asset is created (used to detect world duplication after PostLoad) */
	void OnAssetCreated( UObject* Asset );

	/** Handler for when a world is duplicated in the editor */
	void InitializeNewlyCreatedInactiveWorld(UWorld* World);

	/** Gets the init values for worlds opened via Map_Load in the editor */
	UWorld::InitializationValues GetEditorWorldInitializationValues() const;

	/**
	* Moves all viewport cameras to focus on the provided bounding box.
	* @param	BoundingBox				Target box
	* @param	bActiveViewportOnly		If true, move/reorient only the active viewport.
	*/
	void MoveViewportCamerasToBox(const FBox& BoundingBox, bool bActiveViewportOnly) const;

public:
	// Launcher Worker
	TSharedPtr<class ILauncherWorker> LauncherWorker;
	
	/** Function to run the Play On command for automation testing. */
	void AutomationPlayUsingLauncher(const FString& InLauncherDeviceId);	

	virtual void HandleTravelFailure(UWorld* InWorld, ETravelFailure::Type FailureType, const FString& ErrorString);

	void AutomationLoadMap(const FString& MapName, FString* OutError);

protected:

	UPROPERTY(EditAnywhere, config, Category = Advanced, meta = (MetaClass = "ActorGroupingUtils"))
	FSoftClassPath ActorGroupingUtilsClassName;

	UPROPERTY()
	class UActorGroupingUtils* ActorGroupingUtils;
private:
	FTimerHandle CleanupPIEOnlineSessionsTimerHandle;

	/** Delegate handle for game viewport close requests in PIE sessions. */
	FDelegateHandle ViewportCloseRequestedDelegateHandle;
};

//////////////////////////////////////////////////////////////////////////
// FActorLabelUtilities

struct UNREALED_API FActorLabelUtilities
{
public:
	/**
	 * Given a label, attempts to split this into its alpha/numeric parts.
	 *
	 * @param	InOutLabel	The label to start with, this will only be modified if it ends in a number.
	 * @param	OutIdx		The number which the string ends with, if any.
	 *
	 * @return	true if the label ends with a number.
	 */
	static bool SplitActorLabel(FString& InOutLabel, int32& OutIdx);

	/**
	 * Assigns a new label to an actor. If the name exists it will be appended with a number to make it unique. Actor labels are only available in development builds.
	 *
	 * @param	Actor					The actor to change the label of
	 * @param	NewActorLabel			The new label string to assign to the actor.  If empty, the actor will have a default label.
	 * @param	InExistingActorLabels	(optional) Pointer to a set of actor labels that are currently in use
	 */
	static void SetActorLabelUnique(AActor* Actor, const FString& NewActorLabel, const FCachedActorLabels* InExistingActorLabels = nullptr);

	/** 
	 * Does an explicit actor rename. In addition to changing the label this will also fix any soft references pointing to it 
	 * 
	 * @param	Actor					The actor to change the label of
	 * @param	NewActorLabel			The new label string to assign to the actor.  If empty, the actor will have a default label.
	 * @param	bMakeUnique				If true, it will call SetActorLabelUnique, if false it will use the exact label specified
	 */
	static void RenameExistingActor(AActor* Actor, const FString& NewActorLabel, bool bMakeUnique = false);

private:
	FActorLabelUtilities() {}
};
