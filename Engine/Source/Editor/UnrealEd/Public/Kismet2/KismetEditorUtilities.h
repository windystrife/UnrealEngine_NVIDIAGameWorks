// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/Class.h"
#include "Templates/SubclassOf.h"
#include "GameFramework/Actor.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"

class IBlueprintEditor;
class UEdGraph;
struct Rect;

enum class EBlueprintBytecodeRecompileOptions
{
	None = 0x0,

	// in batch compile mode we don't 'BroadcastCompiled/BroadcastBlueprintCompiled'
	BatchCompile           = 0x1,
	// normally we create a REINST_ version even when doing the bytecode compilation
	// this flag can be used of the blueprints GeneratedClass is being reinstanced by 
	// calling code:
	SkipReinstancing	   = 0x2  
};

ENUM_CLASS_FLAGS(EBlueprintBytecodeRecompileOptions)

enum class EBlueprintCompileOptions
{
	None = 0x0,

	/** This flag has several effects, but its behavior is to 'make things work' when regenerating a blueprint on load */
	IsRegeneratingOnLoad = 0x1, 
	/** Skips garbage collection at the end of compile, useful if caller will collect garbage themselves */
	SkipGarbageCollection = 0x2, 
	/** Prevents intermediate products from being garbage collected, useful for debugging macro/node expansion */
	SaveIntermediateProducts = 0x4, 
	/** Indicates that the skeleton is up to date, and therefore the skeleton compile pass can be skipped */
	SkeletonUpToDate = 0x8, 
	/** Indicates this is a batch compile and that BroadcastCompiled and BroadcastBlueprintCompiled should be skipped */
	BatchCompile = 0x10,
	/** Skips saving blueprints even if save on compile is enabled */
	SkipSave = 0x20,
	/** Skips creating a reinstancer and running reinstancing routines - useful if calling code is performing reinstancing */
	SkipReinstancing = 0x40,
};

ENUM_CLASS_FLAGS(EBlueprintCompileOptions)

//////////////////////////////////////////////////////////////////////////
// FKismetEditorUtilities

class UNREALED_API FKismetEditorUtilities
{
public:
	/** 
	 * Event that's broadcast anytime a Blueprint is created
	 */
	DECLARE_DELEGATE_OneParam(FOnBlueprintCreated, UBlueprint* /*InBlueprint*/);

	/** Manages the TargetClass and EventName to use for spawning default "ghost" nodes in a new Blueprint */
	struct FDefaultEventNodeData
	{
		/** If the new Blueprint is a child of the TargetClass an event will be attempted to be spawned.
		 *	Hiding the category and other things can prevent the event from being placed
		 */
		UClass* TargetClass;

		/** Event Name to spawn a node for */
		FName EventName;
	};

	/** Manages the TargetClass and EventName to use for spawning default "ghost" nodes in a new Blueprint */
	struct FOnBlueprintCreatedData
	{
		/** If the new Blueprint is a child of the TargetClass, the callback will be executed */
		UClass* TargetClass;

		/** Callback to execute */
		FOnBlueprintCreated OnBlueprintCreated;
	};

	/**
	 * Create a new Blueprint and initialize it to a valid state.
	 *
	 * @param ParentClass					the parent class of the new blueprint
	 * @param Outer							the outer object of the new blueprint
	 * @param NewBPName						the name of the new blueprint
	 * @param BlueprintType					the type of the new blueprint (normal, const, etc)
	 * @param BlueprintClassType			the actual class of the blueprint asset (UBlueprint or a derived type)
	 * @param BlueprintGeneratedClassType	the actual generated class of the blueprint asset (UBlueprintGeneratedClass or a derived type)
	 * @param CallingContext				the name of the calling method or module used to identify creation methods to engine analytics/usage stats (default None will be ignored)
	 * @return								the new blueprint
	 */
	static UBlueprint* CreateBlueprint(UClass* ParentClass, UObject* Outer, const FName NewBPName, enum EBlueprintType BlueprintType, TSubclassOf<UBlueprint> BlueprintClassType, TSubclassOf<UBlueprintGeneratedClass> BlueprintGeneratedClassType, FName CallingContext = NAME_None);

	/** 
	 * Event that's broadcast anytime a blueprint is unloaded, and becomes 
	 * invalid (with calls to ReloadBlueprint(), for example).
	 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnBlueprintUnloaded, UBlueprint*);
	static FOnBlueprintUnloaded OnBlueprintUnloaded;

	/** 
	 * Unloads the supplied Blueprint (marking it pending-kill, and removing it 
	 * from its outer package). Then proceeds to reload from disk.
	 *
	 * @param  TargetBlueprint	The Blueprint you want to unload and replace.
	 * @return The freshly loaded Blueprint (replacing the, now invalid, input).
	 */
	static UBlueprint* ReloadBlueprint(UBlueprint* TargetBlueprint);

	/** 
	 * Unloads the specified Blueprint (marking it pending-kill, and removing it 
	 * from its outer package). Then proceeds to replace all references with a
	 * copy of the one passed.
	 *
	 * @param  Target		The Blueprint you want to unload and replace.
	 * @param  Replacement	The Blueprint you cloned and used to replace Target.
	 * @return The duplicated replacement Blueprint.
	 */
	static UBlueprint* ReplaceBlueprint(UBlueprint* Target, UBlueprint const* Replacement);

	/** 
	 * Determines if the specified blueprint is referenced currently in the undo 
	 * buffer.
	 *
	 * @param  Blueprint	The Blueprint you want to query about.
	 * @return True if the Blueprint is saved in the undo buffer, false if not.
	 */
	static bool IsReferencedByUndoBuffer(UBlueprint* Blueprint);

	/** Create the correct event graphs for this blueprint */
	static void CreateDefaultEventGraphs(UBlueprint* Blueprint);

	/** Tries to compile a blueprint, updating any actors in the editor who are using the old class, etc... */
	static void CompileBlueprint(UBlueprint* BlueprintObj, EBlueprintCompileOptions CompileFlags = EBlueprintCompileOptions::None, class FCompilerResultsLog* pResults = nullptr );

	/** Generates a blueprint skeleton only.  Minimal compile, no notifications will be sent, no GC, etc.  Only successful if there isn't already a skeleton generated */
	static bool GenerateBlueprintSkeleton(UBlueprint* BlueprintObj, bool bForceRegeneration = false);

	/** Recompiles the bytecode of a blueprint only.  Should only be run for recompiling dependencies during compile on load */
	static void RecompileBlueprintBytecode(UBlueprint* BlueprintObj, TArray<UObject*>* ObjLoaded = nullptr, EBlueprintBytecodeRecompileOptions Flags = EBlueprintBytecodeRecompileOptions::None);

	/** Tries to make sure that a data-only blueprint is conformed to its native parent, in case any native class flags have changed */
	static void ConformBlueprintFlagsAndComponents(UBlueprint* BlueprintObj);

	/** @return true is it's possible to create a blueprint from the specified class */
	static bool CanCreateBlueprintOfClass(const UClass* Class);

	/** Take a list of components that belong to a single Actor and add them to a blueprint as SCSNodes */
	static void AddComponentsToBlueprint(UBlueprint* Blueprint, const TArray<UActorComponent*>& Components, bool bHarvesting = false, class USCS_Node* OptionalNewRootNode = nullptr, bool bKeepMobility = false);

	/** 
	 * Take an Actor and generate a blueprint based on it. Uses the Actors type as the parent class. 
	 * @param Path					The path to use when creating the package for the new blueprint
	 * @param Actor					The actor to use as the template for the blueprint
	 * @param bReplaceActor			If true, replace the actor in the scene with one based on the created blueprint
	 * @param bKeepMobility			If true, The mobility of each actor components will be copy
	 * @return The blueprint created from the actor
	 */
	static UBlueprint* CreateBlueprintFromActor(const FString& Path, AActor* Actor, bool bReplaceActor, bool bKeepMobility = false );

	/** 
	 * Take an Actor and generate a blueprint based on it. Uses the Actors type as the parent class. 
	 * @param BlueprintName			The name to use for the Blueprint
	 * @param Outer					The outer object to create the blueprint within
	 * @param Actor					The actor to use as the template for the blueprint
	 * @param bReplaceActor			If true, replace the actor in the scene with one based on the created blueprint
	 * @param bKeepMobility			If true, The mobility of each actor components will be copy
	 * @return The blueprint created from the actor
	 */
	static UBlueprint* CreateBlueprintFromActor(const FName BlueprintName, UObject* Outer, AActor* Actor, bool bReplaceActor, bool bKeepMobility = false);

	/** 
	 * Take a list of Actors and generate a blueprint  by harvesting the components they have. Uses AActor as parent class type as the parent class. 
	 * @param Path					The path to use when creating the package for the new blueprint
	 * @param Actors				The actor list to use as the template for the new blueprint, typically this is the currently selected actors
	 * @param bReplaceInWorld		If true, replace the selected actors in the scene with one based on the created blueprint
	 * @return The blueprint created from the actors
	 */
	static UBlueprint* HarvestBlueprintFromActors(const FString& Path, const TArray<AActor*>& Actors, bool ReplaceInWorld);

	/** 
	 * Creates a new blueprint instance and replaces the provided actor list with the new actor
	 * @param Blueprint			The blueprint class to create an actor instance from
	 * @param SelectedActors	The list of currently selected actors in the editor
	 * @param Location			The location of the newly created actor
	 * @param Rotator			The rotation of the newly created actor
	 */
	static AActor* CreateBlueprintInstanceFromSelection(class UBlueprint* Blueprint, TArray<AActor*>& SelectedActors, const FVector& Location, const FRotator& Rotator);

	/** 
	 * Create a new Blueprint from the supplied base class. Pops up window to let user select location and name.
	 *
	 * @param InWindowTitle			The window title
	 * @param InParentClass			The class to create a Blueprint based on
	 */
	static UBlueprint* CreateBlueprintFromClass(FText InWindowTitle, UClass* InParentClass, FString NewNameSuggestion = TEXT(""));

	/** Create a new Actor Blueprint and add the supplied asset to it. */
	static UBlueprint* CreateBlueprintUsingAsset(UObject* Asset, bool bOpenInEditor);

	/** Open a Kismet window, focusing on the specified object (either a node, or a graph).  Prefers existing windows, but will open a new application if required. */
	static void BringKismetToFocusAttentionOnObject(const UObject* ObjectToFocusOn, bool bRequestRename=false);

	/** Open a Kismet window, focusing on the specified pin.  Prefers existing windows, but will open a new application if required. */
	static void BringKismetToFocusAttentionOnPin(const UEdGraphPin* PinToFocusOn );

	/** Open level script kismet window and show any references to the selected actor */
	static void ShowActorReferencesInLevelScript(const AActor* Actor);

	/** Upgrade any cosmetically stale information in a blueprint (done when edited instead of PostLoad to make certain operations easier)
		@returns True if blueprint modified, False otherwise */
	static void UpgradeCosmeticallyStaleBlueprint(UBlueprint* Blueprint);

	/** Create a new event node in the level script blueprint, for the supplied Actor and event (multicast delegate property) name */
	static void CreateNewBoundEventForActor(AActor* Actor, FName EventName);

	/** Create a new event node in the  blueprint, for the supplied component, event name and blueprint */
	static void CreateNewBoundEventForComponent(UObject* Component, FName EventName, UBlueprint* Blueprint, UObjectProperty* ComponentProperty);

	/** Create a new event node in the  blueprint, for the supplied class, event name and blueprint */
	static void CreateNewBoundEventForClass(UClass* Class, FName EventName, UBlueprint* Blueprint, UObjectProperty* ComponentProperty);

	/** Can we paste to this graph? */
	static bool CanPasteNodes(const class UEdGraph* Graph);

	/** Perform paste on graph, at location */
	static void  PasteNodesHere( class UEdGraph* Graph, const FVector2D& Location);

	/** Attempt to get the bounds for currently selected nodes
		@returns false if no nodes are selected */
	static bool GetBoundsForSelectedNodes(const class UBlueprint* Blueprint, class FSlateRect& Rect, float Padding = 0.0f);

	static int32 GetNumberOfSelectedNodes(const class UBlueprint* Blueprint);

	/** Find the event node for this actor with the given event name */
	static const class UK2Node_ActorBoundEvent* FindBoundEventForActor(AActor const* Actor, FName EventName);

	/** Find the event node for the component property with the given event name */
	static const class UK2Node_ComponentBoundEvent* FindBoundEventForComponent(const UBlueprint* Blueprint, FName EventName, FName PropertyName);

	/** Checks to see if a given class implements a blueprint-accesable interface */
	static bool IsClassABlueprintInterface (const UClass* Class);

	/** Checks to see if a blueprint can implement the specified class as an interface */
	static bool CanBlueprintImplementInterface(UBlueprint const* Blueprint, UClass const* Class);

	/** Check to see if a given class is blueprint skeleton class. */
	static bool IsClassABlueprintSkeleton (const UClass* Class);

	/** Check to see if a given class is a blueprint macro library */
	static bool IsClassABlueprintMacroLibrary(const UClass* Class);

	/** Run over the components in the blueprint, and then remove any that fall outside this blueprint's scope (e.g. components brought over after reparenting from another class) */
	static void StripExternalComponents(class UBlueprint* Blueprint);

	/** Whether or not the specified actor is a valid target for bound events */
	static bool IsActorValidForLevelScript(const AActor* Actor);

	/** 
	 *	if bCouldAddAny is true it returns if any event can be bound in LevelScript for given Actor
	 *	else it returns if there exists any event in level script bound with the actor
	 */
	static bool AnyBoundLevelScriptEventForActor(AActor* Actor, bool bCouldAddAny);

	/** It lists bounded LevelScriptEvents for given actor */
	static void AddLevelScriptEventOptionsForActor(class FMenuBuilder& MenuBuilder, TWeakObjectPtr<AActor> ActorPtr, bool bExistingEvents, bool bNewEvents, bool bOnlyEventName);
	
	/** Return information about the given macro graph */
	static void GetInformationOnMacro(UEdGraph* MacroGraph, /*out*/ class UK2Node_Tunnel*& EntryNode, /*out*/ class UK2Node_Tunnel*& ExitNode, bool& bIsPure);
	
	/** 
	 * Add information about any interfaces that have been implemented to the OutTags array
	 *
	 * @param	Blueprint		Blueprint to harvest interface data from
	 * @param	OutTags			Array to add tags to
	 */
	static void AddInterfaceTags(const UBlueprint* Blueprint, TArray<UObject::FAssetRegistryTag>& OutTags);

	/**
	 * Add a default event node to the graph, this node will also be in a disabled state and will spawn
	 * with a call to it's parent if available
	 *
	 * @param InBlueprint		Blueprint this event will be a part of
	 * @param InGraph			The graph to spawn the event node in
	 * @param InEventName		The name of the event function
	 * @param InEventClass		The class this event can be found in
	 * @param InOutNodePosY		Position to spawn the node at, will return with an offset more suitable to offset the next node
	 * @return					The K2Node_Event will be returned
	 */
	static class UK2Node_Event* AddDefaultEventNode(UBlueprint* InBlueprint, UEdGraph* InGraph, FName InEventName, UClass* InEventClass, int32& InOutNodePosY);

	/**
	 * Will add an event to the list of default event nodes to be auto-generated for the class or a child of the class
	 *
	 * @param InOwner			Method of look-up so these registrations can later be removed when unregistering the Owner.
	 * @param InTargetClass		If a new Blueprint is a child of the target class, the event will attempt to be placed in the main event graph
	 * @param InEventName		Name of event to place
	 */
	static void RegisterAutoGeneratedDefaultEvent(void* InOwner, UClass* InTargetClass, FName InEventName);

	/**
	 * Will add an event to a list of callbacks to occur post Blueprint creation if the Blueprint is a child of the class
	 *
	 * @param InOwner							Method of look-up so these registrations can later be removed when unregistering the Owner.
	 * @param InTargetClass						If a new Blueprint is a child of the target class, the event will attempt to be placed in the main event graph
	 * @param InOnBlueprintCreatedCallback		Callback to call when the Blueprint is created
	 */
	static void RegisterOnBlueprintCreatedCallback(void* InOwner, UClass* InTargetClass, FOnBlueprintCreated InOnBlueprintCreatedCallback);

	/** Unregisters a class from having AutoGeneratedDefaultEvent nodes or callbacks for OnBlueprintCreated */
	static void UnregisterAutoBlueprintNodeCreation(void* InOwner);

	/** Add InNode to selection of editor */
	static void AddToSelection(const class UEdGraph* Graph, UEdGraphNode* InNode);

	/** Get IBlueprintEditor for given object, if it exists */
	static TSharedPtr<class IBlueprintEditor> GetIBlueprintEditorForObject(const UObject* ObjectToFocusOn, bool bOpenEditor);
private:
	/** Stores whether we are already listening for kismet clicks */
	static bool bIsListeningForClicksOnKismetLog;

	/** List of blueprint parent class names cached by IsTrackedBlueprintParent() */
	static TArray<FString> TrackedBlueprintParentList;

	/** Mapping of classes to names of Events that should be automatically spawned */
	static TMultiMap<void*, FDefaultEventNodeData> AutoGeneratedDefaultEventsMap;

	/** Mapping of classes to delegate callbacks when a Blueprint is created, occurs post Event node creation */
	static TMultiMap<void*, FOnBlueprintCreatedData> OnBlueprintCreatedCallbacks;
private:
	
	/**
	 * Attempts to decide whether a blueprint's parent class is suitable for tracking via analytics.
	 *
	 * @param ParentClass	The parent class to check
	 * 
	 * @return	True if the parent class is one we wish to track by reporting creation of children to analytics, otherwise false
	 */
	static bool IsTrackedBlueprintParent(const UClass* ParentClass);

	FKismetEditorUtilities() {}
};
