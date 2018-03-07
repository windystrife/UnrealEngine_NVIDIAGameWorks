// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

/*-----------------------------------------------------------------------------
	Dependencies.
-----------------------------------------------------------------------------*/

#include "CoreMinimal.h"
#include "Templates/ScopedCallback.h"
#include "Engine/Level.h"
#include "AssetData.h"
#include "Editor/EditorEngine.h"
#include "Engine/StaticMesh.h"



#define CAMERA_ZOOM_DAMPEN			200.f

class AStaticMeshActor;
class FEdMode;
class UFactory;

/** The shorthand identifier used for editor modes */
typedef FName FEditorModeID;

/** Max Unrealed->Editor Exec command string length. */
#define MAX_EDCMD 512

/** The editor object. */
extern UNREALED_API class UEditorEngine* GEditor;

/** Max length of a single folder in the content directory */
#define MAX_CONTENT_FOLDER_NAME_LENGTH 32
/** Max length of an asset name */
#define MAX_ASSET_NAME_LENGTH 64

/**
 * Returns the path to the engine's editor resources directory (e.g. "/../../Engine/Editor/")
 */
UNREALED_API const FString GetEditorResourcesDir();


/** 
 * FEditorDelegates
 * Delegates used by the editor.
 **/
struct UNREALED_API FEditorDelegates
{
	/** delegate type for map change events ( Params: uint32 MapChangeFlags (MapChangeEventFlags) ) */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnMapChanged, uint32);
	/** delegate type for editor mode change events ( Params: FEditorModeID NewMode ) */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnModeChanged, FEditorModeID);
	/** delegate type for editor camera movement */
	DECLARE_MULTICAST_DELEGATE_FourParams(FOnEditorCameraMoved, const FVector&, const FRotator&, ELevelViewportType, int32 );
	/** delegate type for dollying/zooming editor camera movement */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnDollyPerspectiveCamera, const FVector&, int32 );
	/** delegate type for pre save world events ( uint32 SaveFlags, UWorld* World ) */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPreSaveWorld, uint32, class UWorld*);
	/** delegate type for post save world events ( uint32 SaveFlags, UWorld* World, bool bSuccess ) */
	DECLARE_MULTICAST_DELEGATE_ThreeParams(FOnPostSaveWorld, uint32, class UWorld*, bool);
	/** delegate for a PIE event (begin, end, pause/resume, etc) (Params: bool bIsSimulating) */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnPIEEvent, const bool);
	/** delegate for a standalone local play event (Params: uint32 processID) */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnStandaloneLocalPlayEvent, const uint32);
	/** delegate type for beginning or finishing configuration of the properties of a new asset */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnNewAssetCreation, UFactory*);
	/** delegate type fired when new assets are being (re-)imported. Params: UFactory* InFactory, UClass* InClass, UObject* InParent, const FName& Name, const TCHAR* Type */
	DECLARE_MULTICAST_DELEGATE_FiveParams(FOnAssetPreImport, UFactory*, UClass*, UObject*, const FName&, const TCHAR*);
	/** delegate type fired when new assets have been (re-)imported. Note: InCreatedObject can be NULL if import failed. Params: UFactory* InFactory, UObject* InCreatedObject */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAssetPostImport, UFactory*, UObject*);
	/** delegate type fired when new assets have been reimported. Note: InCreatedObject can be NULL if import failed. UObject* InCreatedObject */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnAssetReimport, UObject*);
	/** delegate type for finishing up construction of a new blueprint */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnFinishPickingBlueprintClass, UClass*);
	/** delegate type for triggering when new actors are dropped on to the viewport */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnNewActorsDropped, const TArray<UObject*>&, const TArray<AActor*>&);
	/** delegate type for when attempting to apply an object to an actor */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnApplyObjectToActor, UObject*, AActor*);
	/** delegate type for triggering when grid snapping has changed */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnGridSnappingChanged, bool, float);
	/** delegate type for triggering when focusing on a set of actors */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnFocusViewportOnActors, const TArray<AActor*>&);
	/** delegate type for triggering when a map is opened */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnMapOpened, const FString& /* Filename */, bool /*bAsTemplate*/);
	/** Delegate used for entering or exiting an editor mode */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnEditorModeTransitioned, FEdMode* /*Mode*/);
	/** delegate type for when a user requests to delete certain assets... DOES NOT mean the asset(s) will be deleted (the user could cancel) */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnAssetsPreDelete, const TArray<UObject*>&);
	/** delegate type for when one or more assets have been deleted */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnAssetsDeleted, const TArray<UClass*>& /*DeletedAssetClasses*/);
	/** delegate type for when a user starts dragging something out of content browser (can be multiple assets) */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FOnAssetDragStarted, const TArray<FAssetData>&, class UActorFactory* /*FactoryToUse*/);
	/** delegate type for when a new level is added to the world */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnAddLevelToWorld, ULevel*);
	/** delegate type for when a texture is fit to surface  */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnFitTextureToSurface, UWorld*);

	/** Called when the CurrentLevel is switched to a new level.  Note that this event won't be fired for temporary
		changes to the current level, such as when copying/pasting actors. */
	static FSimpleMulticastDelegate NewCurrentLevel;
	/** Called when the map has changed */
	static FOnMapChanged MapChange;
	/** Called when an actor is added to a layer */
	static FSimpleMulticastDelegate LayerChange;
	/** surfprops changed */
	static FSimpleMulticastDelegate SurfProps;
	/** Sent when requesting to display the properties of selected actors or BSP surfaces */
	static FSimpleMulticastDelegate SelectedProps;
	/** Fits the currently assigned texture to the selected surfaces */
	static FOnFitTextureToSurface FitTextureToSurface;
	/** Called when the editor mode is changed */
	static FOnModeChanged ChangeEditorMode;
	/** Called when properties of an actor have changed */
	static FSimpleMulticastDelegate ActorPropertiesChange;
	/** Called when the editor needs to be refreshed */
	static FSimpleMulticastDelegate RefreshEditor;
	/** called when all browsers need to be refreshed */
	static FSimpleMulticastDelegate RefreshAllBrowsers;
	/** called when the level browser need to be refreshed */
	static FSimpleMulticastDelegate RefreshLevelBrowser;
	/** called when the layer browser need to be refreshed */
	static FSimpleMulticastDelegate RefreshLayerBrowser;
	/** called when the primitive stats browser need to be refreshed */
	static FSimpleMulticastDelegate RefreshPrimitiveStatsBrowser;
	/** Called when an action is performed which interacts with the content browser; 
	 *  load any selected assets which aren't already loaded */
	static FSimpleMulticastDelegate LoadSelectedAssetsIfNeeded;
	/** Called when load errors are about to be displayed */
	static FSimpleMulticastDelegate DisplayLoadErrors;
	/** Called when an editor mode is being entered */
	static FOnEditorModeTransitioned EditorModeEnter;
	/** Called when an editor mode is being exited */
	static FOnEditorModeTransitioned EditorModeExit;
	/** Sent when a PIE session is beginning (before we decide if PIE can run - allows clients to avoid blocking PIE) */
	static FOnPIEEvent PreBeginPIE;
	/** Sent when a PIE session is beginning (but hasn't actually started yet) */
	static FOnPIEEvent BeginPIE;
	/** Sent when a PIE session has fully started and after BeginPlay() has been called */
	static FOnPIEEvent PostPIEStarted;
	/** Sent when a PIE session is ending, before anything else happens */
	static FOnPIEEvent PrePIEEnded;
	/** Sent when a PIE session is ending */
	static FOnPIEEvent EndPIE;
	/** Sent when a PIE session is paused */
	static FOnPIEEvent PausePIE;
	/** Sent when a PIE session is resumed */
	static FOnPIEEvent ResumePIE;
	/** Sent when a PIE session is single-stepped */
	static FOnPIEEvent SingleStepPIE;
	/** Sent just before the user switches between from PIE to SIE, or vice-versa.  Passes in whether we are currently in SIE */
	static FOnPIEEvent OnPreSwitchBeginPIEAndSIE;
	/** Sent after the user switches between from PIE to SIE, or vice-versa.  Passes in whether we are currently in SIE */
	static FOnPIEEvent OnSwitchBeginPIEAndSIE;
	/** Sent when PC local play session is starting */
	static FOnStandaloneLocalPlayEvent BeginStandaloneLocalPlay;
	/** Within a property window, the currently selected item was changed.*/
	static FSimpleMulticastDelegate PropertySelectionChange;
	/** Called after Landscape layer infomap update have completed */
	static FSimpleMulticastDelegate PostLandscapeLayerUpdated;
	/** Called before SaveWorld is processed */
	static FOnPreSaveWorld PreSaveWorld;
	/** Called after SaveWorld is processed */
	static FOnPostSaveWorld PostSaveWorld;
	/** Called when finishing picking a new blueprint class during construction */
	static FOnFinishPickingBlueprintClass OnFinishPickingBlueprintClass;
	/** Called when beginning configuration of a new asset */
	static FOnNewAssetCreation OnConfigureNewAssetProperties;
	/** Called when finishing configuration of a new asset */
	static FOnNewAssetCreation OnNewAssetCreated;
	/** Called when new assets are being (re-)imported. */
	static FOnAssetPreImport OnAssetPreImport;
	/** Called when new assets have been (re-)imported. */
	static FOnAssetPostImport OnAssetPostImport;
	/** Called after an asset has been reimported */
	static FOnAssetReimport OnAssetReimport;
	/** Called when new actors are dropped on to the viewport */
	static FOnNewActorsDropped OnNewActorsDropped;
	/** Called when grid snapping is changed */
	static FOnGridSnappingChanged OnGridSnappingChanged;
	/** Called when a lighting build has started */
	static FSimpleMulticastDelegate OnLightingBuildStarted;
	/** Called when a lighting build has been kept */
	static FSimpleMulticastDelegate OnLightingBuildKept;
	/** Called when a lighting build has failed (maybe called twice if cancelled) */
	static FSimpleMulticastDelegate OnLightingBuildFailed;
	/** Called when a lighting build has succeeded */
	static FSimpleMulticastDelegate OnLightingBuildSucceeded;
	/** Called when when attempting to apply an object to an actor (via drag drop) */
	static FOnApplyObjectToActor OnApplyObjectToActor;
	/** Called when focusing viewport on a set of actors */
	static FOnFocusViewportOnActors OnFocusViewportOnActors;
	/** Called when a map is opened, giving map name, and whether it was a template */
	static FOnMapOpened OnMapOpened;
	/** Called when the editor camera is moved */
	static FOnEditorCameraMoved OnEditorCameraMoved;
	/** Called when the editor camera is moved */
	static FOnDollyPerspectiveCamera OnDollyPerspectiveCamera;
	/** Called on editor shutdown after packages have been successfully saved */
	static FSimpleMulticastDelegate OnShutdownPostPackagesSaved;
	/** Called when the user requests certain assets be deleted (DOES NOT imply that the asset will be deleted... the user could cancel) */
	static FOnAssetsPreDelete OnAssetsPreDelete;
	/** Called when one or more assets have been deleted */
	static FOnAssetsDeleted OnAssetsDeleted;
	/** Called when a user starts dragging something out of content browser (can be multiple assets) */
	static FOnAssetDragStarted OnAssetDragStarted;
	/** Called when Action or Axis mappings have been changed */
	static FSimpleMulticastDelegate OnActionAxisMappingsChanged;
	/** Called from FEditorUtils::AddLevelToWorld after the level is added successfully to the world. */
	static FOnAddLevelToWorld OnAddLevelToWorld;
};

/**
 * Scoped delegate wrapper
 */
 #define DECLARE_SCOPED_DELEGATE( CallbackName, TriggerFunc )						\
	class UNREALED_API FScoped##CallbackName##Impl										\
	{																				\
	public:																			\
		static void FireCallback() { TriggerFunc; }									\
	};																				\
																					\
	typedef TScopedCallback<FScoped##CallbackName##Impl> FScoped##CallbackName;

DECLARE_SCOPED_DELEGATE( ActorPropertiesChange, FEditorDelegates::ActorPropertiesChange.Broadcast() );
DECLARE_SCOPED_DELEGATE( RefreshAllBrowsers, FEditorDelegates::RefreshAllBrowsers.Broadcast() );

#undef DECLARE_SCOPED_DELEGATE


/** Texture alignment. */
enum ETAxis
{
	TAXIS_X                 = 0,
	TAXIS_Y                 = 1,
	TAXIS_Z                 = 2,
	TAXIS_WALLS             = 3,
	TAXIS_AUTO              = 4,
};






/**
 * MapChangeEventFlags defines flags passed to FEditorDelegates::MapChange global events
 */
namespace MapChangeEventFlags
{
	/** MapChangeEventFlags::Type */
	typedef uint32 Type;

	/** Default flags */
	const Type Default = 0;

	/** Set when a new map is created, loaded from disk, imported, etc. */
	const Type NewMap = 1 << 0;

	/** Set when a map rebuild occurred */
	const Type MapRebuild = 1 << 1;

	/** Set when a world was destroyed (torn down) */
	const Type WorldTornDown = 1 << 2;
}

/**
 * This class begins an object movement change when created and ends it when it falls out of scope
 */
class FScopedObjectMovement
{
public:
	/**
	 * Constructor.  Broadcasts a delegate to notify listeners an actor is about to move
	 */
	FScopedObjectMovement( UObject* InObject )
		: Object( InObject )
	{
		if( GEditor && Object.IsValid() )
		{
			GEditor->BroadcastBeginObjectMovement( *Object );
		}
	}

	/**
	 * Constructor.  Broadcasts a delegate to notify listeners an actor has moved
	 */
	~FScopedObjectMovement()
	{
		if( GEditor && Object.IsValid() )
		{
			GEditor->BroadcastEndObjectMovement( *Object );
		}
	}
private:
	/** The object being moved */
	TWeakObjectPtr<UObject> Object;
};

/**
 * Import the entire default properties block for the class specified
 * 
 * @param	Class		the class to import defaults for
 * @param	Text		buffer containing the text to be imported
 * @param	Warn		output device for log messages
 * @param	Depth		current nested subobject depth
 * @param	LineNumber	the starting line number for the defaultproperties block (used for log messages)
 *
 * @return	NULL if the default values couldn't be imported
 */

/**
 * Parameters for ImportObjectProperties
 */
struct FImportObjectParams
{
	/** the location to import the property values to */
	uint8*				DestData;

	/** pointer to a buffer containing the values that should be parsed and imported */
	const TCHAR*		SourceText;

	/** the struct for the data we're importing */
	UStruct*			ObjectStruct;

	/** the original object that ImportObjectProperties was called for.
		if SubobjectOuter is a subobject, corresponds to the first object in SubobjectOuter's Outer chain that is not a subobject itself.
		if SubobjectOuter is not a subobject, should normally be the same value as SubobjectOuter */
	UObject*			SubobjectRoot;

	/** the object corresponding to DestData; this is the object that will used as the outer when creating subobjects from definitions contained in SourceText */
	UObject*			SubobjectOuter;

	/** output device to use for log messages */
	FFeedbackContext*	Warn;

	/** current nesting level */
	int32					Depth;

	/** used when importing defaults during script compilation for tracking which line we're currently for the purposes of printing compile errors */
	int32					LineNumber;

	/** contains the mappings of instanced objects and components to their templates; used when recursively calling ImportObjectProperties; generally
		not necessary to specify a value when calling this function from other code */
	FObjectInstancingGraph* InInstanceGraph;

	/** provides a mapping from an existing actor to a new instance to which it should be remapped */
	const TMap<AActor*, AActor*>* ActorRemapper;

	/** True if we should call PreEditChange/PostEditChange on the object as it's imported.  Pass false here
		if you're going to do that on your own. */
	bool				bShouldCallEditChange;


	/** Constructor */
	FImportObjectParams()
		: DestData( NULL ),
		  SourceText( NULL ),
		  ObjectStruct( NULL ),
		  SubobjectRoot( NULL ),
		  SubobjectOuter( NULL ),
		  Warn( NULL ),
		  Depth( 0 ),
		  LineNumber( INDEX_NONE ),
		  InInstanceGraph( NULL ),
		  ActorRemapper( NULL ),
		  bShouldCallEditChange( true )
	{
	}
};


/**
 * Parse and import text as property values for the object specified.
 * 
 * @param	InParams	Parameters for object import; see declaration of FImportObjectParams.
 *
 * @return	NULL if the default values couldn't be imported
 */

const TCHAR* ImportObjectProperties( FImportObjectParams& InParams );


/**
 * Parse and import text as property values for the object specified.
 * 
 * @param	DestData			the location to import the property values to
 * @param	SourceText			pointer to a buffer containing the values that should be parsed and imported
 * @param	ObjectStruct		the struct for the data we're importing
 * @param	SubobjectRoot		the original object that ImportObjectProperties was called for.
 *								if SubobjectOuter is a subobject, corresponds to the first object in SubobjectOuter's Outer chain that is not a subobject itself.
 *								if SubobjectOuter is not a subobject, should normally be the same value as SubobjectOuter
 * @param	SubobjectOuter		the object corresponding to DestData; this is the object that will used as the outer when creating subobjects from definitions contained in SourceText
 * @param	Warn				output device to use for log messages
 * @param	Depth				current nesting level
 * @param	LineNumber			used when importing defaults during script compilation for tracking which line the defaultproperties block begins on
 * @param	InstanceGraph		contains the mappings of instanced objects and components to their templates; used when recursively calling ImportObjectProperties; generally
 *								not necessary to specify a value when calling this function from other code
 * @param	ActorRemapper		used when duplicating actors to remap references from a source actor to the duplicated actor
 *
 * @return	NULL if the default values couldn't be imported
 */
const TCHAR* ImportObjectProperties(
	uint8*				DestData,
	const TCHAR*		SourceText,
	UStruct*			ObjectStruct,
	UObject*			SubobjectRoot,
	UObject*			SubobjectOuter,
	FFeedbackContext*	Warn,
	int32					Depth,
	int32					LineNumber = INDEX_NONE,
	FObjectInstancingGraph* InstanceGraph = NULL,
	const TMap<AActor*, AActor*>* ActorRemapper = NULL
	);

//
// GBuildStaticMeshCollision - Global control for building static mesh collision on import.
//

extern bool GBuildStaticMeshCollision;

//
// Creating a static mesh from an array of triangles.
//
UStaticMesh* CreateStaticMesh(struct FRawMesh& RawMesh,TArray<FStaticMaterial>& Materials,UObject* Outer,FName Name);

struct FMergeStaticMeshParams
{
	/**
	 *Constructor, setting all values to usable defaults 
	 */
	FMergeStaticMeshParams();

	/** A translation to apply to the verts in SourceMesh */
	FVector Offset;
	/** A rotation to apply to the verts in SourceMesh */
	FRotator Rotation;
	/** A uniform scale to apply to the verts in SourceMesh */
	float ScaleFactor;
	/** A non-uniform scale to apply to the verts in SourceMesh */
	FVector ScaleFactor3D;
	
	/** If true, DestMesh will not be rebuilt */
	bool bDeferBuild;
	
	/** If set, all triangles in SourceMesh will be set to this element index, instead of duplicating SourceMesh's elements into DestMesh's elements */
	int32 OverrideElement;

	/** If true, UVChannelRemap will be used to reroute UV channel values from one channel to another */
	bool bUseUVChannelRemapping;
	/** An array that can remap UV values from one channel to another */
	int32 UVChannelRemap[8];
	
	/* If true, UVScaleBias will be used to modify the UVs (AFTER UVChannelRemap has been applied) */
	bool bUseUVScaleBias;
	/* Scales/Bias's to apply to each UV channel in SourceMesh */
	FVector4 UVScaleBias[8];
};

/**
 * Merges SourceMesh into DestMesh, applying transforms along the way
 *
 * @param DestMesh The static mesh that will have SourceMesh merged into
 * @param SourceMesh The static mesh to merge in to DestMesh
 * @param Params Settings for the merge
 */
void MergeStaticMesh(UStaticMesh* DestMesh, UStaticMesh* SourceMesh, const FMergeStaticMeshParams& Params);

//
// Converting models to static meshes.
//
UNREALED_API void GetBrushMesh(ABrush* Brush,UModel* Model,struct FRawMesh& OutMesh,TArray<FStaticMaterial>& OutMaterials);
UStaticMesh* CreateStaticMeshFromBrush(UObject* Outer,FName Name,ABrush* Brush,UModel* Model);
 
/**
 * Converts a static mesh to a brush.
 *
 * @param	Model					[out] The target brush.  Must be non-NULL.
 * @param	StaticMeshActor			The source static mesh.  Must be non-NULL.
 */
UNREALED_API void CreateModelFromStaticMesh(UModel* Model,AStaticMeshActor* StaticMeshActor);


/**
 * Sets GWorld to the passed in PlayWorld and sets a global flag indicating that we are playing
 * in the Editor.
 *
 * @param	PlayInEditorWorld		PlayWorld
 * @return	the original GWorld
 */
UNREALED_API UWorld* SetPlayInEditorWorld( UWorld* PlayInEditorWorld );

/**
 * Restores GWorld to the passed in one and reset the global flag indicating whether we are a PIE
 * world or not.
 *
 * @param EditorWorld	original world being edited
 */
UNREALED_API void RestoreEditorWorld( UWorld* EditorWorld );


/*-----------------------------------------------------------------------------
	Parameter parsing functions.
-----------------------------------------------------------------------------*/

bool GetFVECTOR( const TCHAR* Stream, const TCHAR* Match, FVector& Value );
bool GetFVECTOR( const TCHAR* Stream, FVector& Value );
const TCHAR* GetFVECTORSpaceDelimited( const TCHAR* Stream, FVector& Value );
bool GetFROTATOR( const TCHAR* Stream, const TCHAR* Match, FRotator& Rotation, int32 ScaleFactor );
bool GetFROTATOR( const TCHAR* Stream, FRotator& Rotation, int ScaleFactor );
const TCHAR* GetFROTATORSpaceDelimited( const TCHAR* Stream, FRotator& Rotation, int32 ScaleFactor );
bool GetBEGIN( const TCHAR** Stream, const TCHAR* Match );
bool GetEND( const TCHAR** Stream, const TCHAR* Match );
bool GetREMOVE( const TCHAR** Stream, const TCHAR* Match );
bool GetSUBSTRING(const TCHAR*	Stream, const TCHAR* Match, TCHAR* Value, int32 MaxLen);
TCHAR* SetFVECTOR( TCHAR* Dest, const FVector* Value );

/**
 * Takes an FName and checks to see that it is unique among all loaded objects.
 *
 * @param	InName		The name to check
 * @param	Outer		The context for validating this object name. Should be a group/package, but could be ANY_PACKAGE if you want to check across the whole system (not recommended)
 * @param	InReason	If the check fails, this string is filled in with the reason why.
 *
 * @return	true if the name is valid
 */

UNREALED_API bool IsUniqueObjectName( const FName& InName, UObject* Outer, FText* InReason=NULL );

/**
 * Takes an FName and checks to see that it is unique among all loaded objects.
 *
 * @param	InName		The name to check
 * @param	Outer		The context for validating this object name. Should be a group/package, but could be ANY_PACKAGE if you want to check across the whole system (not recommended)
 * @param	InReason	If the check fails, this string is filled in with the reason why.
 *
 * @return	true if the name is valid
 */

UNREALED_API bool IsUniqueObjectName( const FName& InName, UObject* Outer, FText& InReason );


/**
 * Provides access to the FEditorModeTools for the level editor
 */
UNREALED_API class FEditorModeTools& GLevelEditorModeTools();

namespace EditorUtilities
{
	/**
	 * Given an actor in a Simulation or PIE world, tries to find a counterpart actor in the editor world
	 *
	 * @param	Actor	The simulation world actor that we want to find a counterpart for
	 *
	 * @return	The found editor world actor, or NULL if we couldn't find a counterpart
	 */
	UNREALED_API AActor* GetEditorWorldCounterpartActor( AActor* Actor );

	/**
	 * Given an actor in the editor world, tries to find a counterpart actor in a Simulation or PIE world
	 *
	 * @param	Actor	The editor world actor that we want to find a counterpart for
	 *
	 * @return	The found Simulation or PIE world actor, or NULL if we couldn't find a counterpart
	 */
	UNREALED_API AActor* GetSimWorldCounterpartActor( AActor* Actor );

	/**
	 * Guiven an actor in the editor world, and SourceComponent from Simulation or PIE world
	 * find the matching component in the Editor World
	 *
	 * @param	SourceComponent	SouceCompoent in SIM world
	 * @param	TargetActor		TargetActor in editor world
	 *
	 * @return	the sound editor component or NULL if we couldn't find
	 */
	UNREALED_API UActorComponent* FindMatchingComponentInstance( UActorComponent* SourceComponent, AActor* TargetActor );

	/** Options for CopyActorProperties */
	namespace ECopyOptions
	{
		enum Type
		{
			/** Default copy options */
			Default = 0,

			/** Set this option to preview the changes and not actually copy anything.  This will count the number of properties that would be copied. */
			PreviewOnly = 1 << 0,

			/** Call PostEditChangeProperty for each modified property */
			CallPostEditChangeProperty = 1 << 1,

			/** Call PostEditMove if we detect that a transform property was changed */
			CallPostEditMove = 1 << 2,

			/** Copy only Edit and Interp properties.  Otherwise we copy all properties by default */
			OnlyCopyEditOrInterpProperties = 1 << 3,

			/** Propagate property changes to archetype instances if the target actor is a CDO */
			PropagateChangesToArchetypeInstances = 1 << 4,

			/** Filters out Blueprint Read-only properties */
			FilterBlueprintReadOnly = 1 << 5,
		};
	}


	/** Copy options structure for CopyActorProperties */
	struct FCopyOptions
	{
		/** Implicit construction for an options enumeration */
		FCopyOptions(const ECopyOptions::Type InFlags) : Flags(InFlags) {}

		/** Check whether we can copy the specified property */
		bool CanCopyProperty(UProperty& Property, UObject& Object) const
		{
			return !PropertyFilter || PropertyFilter(Property, Object);
		}

		/** User-specified flags for the copy */
		ECopyOptions::Type Flags;

		/** User-specified custom property filter predicate */
		TFunction<bool(UProperty&, UObject&)> PropertyFilter;
	};

	/** Helper function for CopyActorProperties(). Copies a single property form a source object to a target object. */
	UNREALED_API void CopySingleProperty(const UObject* const InSourceObject, UObject* const InTargetObject, UProperty* const InProperty);

	/**
	 * Copies properties from one actor to another.  Designed for propagating changes made to PIE actors back to their EditorWorld
	 * counterpart, or pushing spawned actor changes back to a Blueprint CDO object.  You can pass the 'PreviewOnly' option to
	 * count the properties that would be copied instead of actually copying them.
	 *
	 * @param	SourceActor		Actor to copy properties from
	 * @param	TargetActor		Actor that will be modified with properties propagated from the source actor
	 * @param	Options			Optional options for this copy action (see ECopyOptions::Type)
	 *
	 * @return	The number of properties that were copied over (properties that were filtered out, or were already identical, are not counted.)
	 */
	UNREALED_API int32 CopyActorProperties( AActor* SourceActor, AActor* TargetActor, const FCopyOptions& Options = FCopyOptions(ECopyOptions::Default) );
}


extern UNREALED_API class FLevelEditorViewportClient* GCurrentLevelEditingViewportClient;

/** Tracks the last level editing viewport client that received a key press. */
extern UNREALED_API class FLevelEditorViewportClient* GLastKeyLevelEditingViewportClient;
