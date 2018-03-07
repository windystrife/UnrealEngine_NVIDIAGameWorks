// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "InputCoreTypes.h"
#include "Templates/SubclassOf.h"
#include "Templates/Casts.h"
#include "Engine/EngineBaseTypes.h"
#include "UObject/GCObject.h"
#include "EditorWorldExtension.generated.h"

struct FWorldContext;
class UEditorWorldExtensionCollection;
class UEditorWorldExtensionManager;
class UEditorWorldExtension;
class FViewport;
class AActor;
class FEditorViewportClient;

UCLASS()
class UNREALED_API UEditorWorldExtension : public UObject
{
	GENERATED_BODY()

	friend class UEditorWorldExtensionCollection;
public:
	
	/** Default constructor */
	UEditorWorldExtension();

	/** Default destructor */
	virtual ~UEditorWorldExtension();

	/** Initialize extension */
	virtual void Init() {};

	/** Shut down extension when world is destroyed */
	virtual void Shutdown() {};

	/** Give base class the chance to tick */
	virtual void Tick( float DeltaSeconds ) {};

	virtual bool InputKey( FEditorViewportClient* InViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event);
	virtual bool InputAxis( FEditorViewportClient* InViewportClient, FViewport* Viewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime);

	/** Gets the world owning this extension */
	virtual UWorld* GetWorld() const override;

	/**  Spawns a transient actor that we can use in the current world of this extension (templated for convenience) */
	template<class T>
	inline T* SpawnTransientSceneActor(const FString& ActorName, const bool bWithSceneComponent = false, const EObjectFlags InObjectFlags = EObjectFlags::RF_Transient | EObjectFlags::RF_DuplicateTransient )
	{
		return CastChecked<T>(SpawnTransientSceneActor(T::StaticClass(), ActorName, bWithSceneComponent, InObjectFlags));
	}

	/** Spawns a transient actor that we can use in the current world of this extension */
	AActor* SpawnTransientSceneActor(TSubclassOf<AActor> ActorClass, const FString& ActorName, const bool bWithSceneComponent = false, const EObjectFlags InObjectFlags = EObjectFlags::RF_Transient | EObjectFlags::RF_DuplicateTransient );

	/** Destroys a transient actor we created earlier */
	void DestroyTransientActor(AActor* Actor);

	/** Sets if this extension should be ticked. */
	void SetActive(const bool bInActive);

	/** If this extension is currently being ticked. */
	bool IsActive() const;

	/** Get the owning collection of extensions */
	UEditorWorldExtensionCollection* GetOwningCollection();

	/** Executes command */
	bool ExecCommand(const FString& InCommand);

protected:
	
	/** Reparent actors to a new world */
	virtual void TransitionWorld(UWorld* NewWorld);

	/** Give child class a chance to act on entering simulate mode */
	virtual void EnteredSimulateInEditor() {};
	
	/** Give child class a chance to act on leaving simulate mode */
	virtual void LeftSimulateInEditor(UWorld* SimulateWorld) {};

	/** The collection of extensions that is owning this extension */
	UEditorWorldExtensionCollection* OwningExtensionsCollection;

private:

	/** Reparent the actors to a new world. */
	void ReparentActor(AActor* Actor, UWorld* NewWorld);

	/** Let the FEditorWorldExtensionCollection set the world of this extension before init */
	void InitInternal(UEditorWorldExtensionCollection* InOwningExtensionsCollection);

	UPROPERTY()
	TArray<AActor*> ExtensionActors;

	/** If this extension is currently being ticked */
	bool bActive;
};

/**
 * Holds a collection of UEditorExtension
 */
UCLASS()
class UNREALED_API UEditorWorldExtensionCollection : public UObject
{
	GENERATED_BODY()

	friend class UEditorWorldExtensionManager;
public:

	/** Default constructor */
	UEditorWorldExtensionCollection();

	/** Default destructor */
	virtual ~UEditorWorldExtensionCollection();

	/** Gets the world from the world context */
	virtual UWorld* GetWorld() const override;

	/**
	 * Checks if the passed extension already exists and creates one if it doesn't.
	 * @param EditorExtensionClass the subclass of an extension to create if necessary and add.
	 */
	UEditorWorldExtension* AddExtension(TSubclassOf<UEditorWorldExtension> EditorExtensionClass);

	/** 
	 * Adds an extension to the collection
	 * @param	EditorExtension			The UEditorExtension that will be created, initialized and added to the collection.
	 */
	void AddExtension( UEditorWorldExtension* EditorExtension );
	
	/** 
	 * Removes an extension from the collection and calls Shutdown() on the extension
	 * @param	EditorExtension			The UEditorExtension to remove.  It must already have been added.
	 */
	void RemoveExtension( UEditorWorldExtension* EditorExtension );

	/**
	 * Find an extension based on the class
	 * @param	EditorExtensionClass	The class to find an extension with
	 * @return							The first extension that is found based on class
	 */
	UEditorWorldExtension* FindExtension( TSubclassOf<UEditorWorldExtension> EditorExtensionClass );

	/** Ticks all extensions */
	void Tick( float DeltaSeconds );

	/** Notifies all extensions of keyboard input */
	bool InputKey( FEditorViewportClient* InViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event);

	/** Notifies all extensions of axis movement */
	bool InputAxis( FEditorViewportClient* InViewportClient, FViewport* Viewport, int32 ControllerId, FKey Key, float Delta, float DeltaTime);
	
	/** Show or hide all the actors of extensions that belong to this collection. */
	void ShowAllActors(const bool bShow);

private:

	/** Sets the world for this collection and gives every extension an opportunity to transition */
	void SetWorld(UWorld* World);

	/** Called by the editor after PIE or Simulate is started */
	void PostPIEStarted( bool bIsSimulatingInEditor );

	/** Called just before PIE or Simulate ends */
	void OnPreEndPIE(bool bWasSimulatingInEditor);

	/** Called when PIE or Simulate ends */
	void OnEndPIE( bool bWasSimulatingInEditor );

	/** Called when switching between play and simulate */
	void SwitchPIEAndSIE(bool bIsSimulatingInEditor);

	/** World context */
	TWeakObjectPtr<UWorld> Currentworld;

	/** After entering Simulate, this stores the counterpart editor world to the Simulate world, so that we
        know this collection needs to transition back to editor world after Simulate finishes */
	TWeakObjectPtr<UWorld> EditorWorldOnSimulate;

	/** List of extensions along with their reference count.  Extensions will only be truly removed and Shutdown() after their
	    reference count drops to zero. */
	typedef TTuple<UEditorWorldExtension*, int32> FEditorExtensionTuple;
	TArray<FEditorExtensionTuple> EditorExtensions;
};

/**
 * Holds a map of extension collections paired with worlds
 */
UCLASS()
class UNREALED_API UEditorWorldExtensionManager	: public UObject
{
	GENERATED_BODY()

public:
	/** Default constructor */
	UEditorWorldExtensionManager();

	/** Default destructor */
	virtual ~UEditorWorldExtensionManager();

	/** Gets the editor world wrapper that is found with the world passed.
	 * Adds one for this world if there was non found. */
	UEditorWorldExtensionCollection* GetEditorWorldExtensions(UWorld* InWorld, const bool bCreateIfNeeded = true);

	/** Ticks all the collections */
	void Tick( float DeltaSeconds );

private:

	/** Adds a new editor world wrapper when a new world context was created */
	UEditorWorldExtensionCollection* OnWorldAdd(UWorld* World);

	/** Adds a new editor world wrapper when a new world context was created */
	void OnWorldContextRemove(FWorldContext& InWorldContext);

	UEditorWorldExtensionCollection* FindExtensionCollection(const UWorld* InWorld);

	/** Map of all the editor world maps */
	UPROPERTY()
	TArray<UEditorWorldExtensionCollection*> EditorWorldExtensionCollection;
};
