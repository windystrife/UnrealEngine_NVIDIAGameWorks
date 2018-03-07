// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Textures/SlateIcon.h"
#include "Editor.h"

class FEditorModeTools;
class FEdMode;

// Required forward declarations
class FEdMode;

DECLARE_DELEGATE_RetVal(TSharedRef<FEdMode>, FEditorModeFactoryCallback);

/**
 *	Class responsible for maintaining a list of registered editor mode types.
 *
 *	Example usage:
 *
 *	Register your mode type with:
 *		FEditorModeRegistry::Get().RegisterMode<FMyEditorMode>( FName( TEXT("MyEditorMode") ) );
 *	or:
 *		class FMyEditorModeFactory : public IEditorModeFactory
 *		{
 *			virtual void OnSelectionChanged( FEditorModeTools& Tools, UObject* ItemUndergoingChange ) const override;
 *			virtual FEditorModeInfo GetModeInfo() const override;
 *			virtual TSharedRef<FEdMode> CreateMode() const override;
 *		};
 *		TSharedRef<FMyEditorModeFactory> Factory = MakeShareable( new FMyEditorModeFactory );
 *		FEditorModeRegistry::Get().RegisterMode( FName( TEXT("MyEditorMode") ), Factory );
 *
 *	Unregister your mode when it is no longer available like so (this will prompt the destruction of any existing modes of this type):
 *		FEditorModeRegistry::Get().UnregisterMode( FName( TEXT("MyEditorMode") ) );
 */

struct FEditorModeInfo
{
	/** Default constructor */
	UNREALED_API FEditorModeInfo();

	/** Helper constructor */
	UNREALED_API FEditorModeInfo(
		FEditorModeID InID,
		FText InName = FText(),
		FSlateIcon InIconBrush = FSlateIcon(),
		bool bInIsVisible = false,
		int32 InPriorityOrder = MAX_int32
		);

	/** The mode ID */
	FEditorModeID ID;

	/** Name for the editor to display */
	FText Name;

	/** The mode icon */
	FSlateIcon IconBrush;

	/** Whether or not the mode should be visible in the mode menu */
	bool bVisible;

	/** The priority of this mode which will determine its default order and shift+X command assignment */
	int32 PriorityOrder;
};

struct IEditorModeFactory : public TSharedFromThis<IEditorModeFactory>
{
	/** Virtual destructor */
	virtual ~IEditorModeFactory() {}

	/**
	 * Allows mode factories to handle selection change events, and potentially activate/deactivate modes

	 * @param Tools					The mode tools that are triggering the event
	 * @param ItemUndergoingChange	Either an actor being selected or deselected, or a selection set being modified (typically emptied)
	 */
	virtual void OnSelectionChanged(FEditorModeTools& Tools, UObject* ItemUndergoingChange) const { }

	/**
	 * Gets the information pertaining to the mode type that this factory creates
	 */
	virtual FEditorModeInfo GetModeInfo() const = 0;

	/**
	 * Create a new instance of our mode
	 */
	virtual TSharedRef<FEdMode> CreateMode() const = 0;
};

struct FEditorModeFactory : IEditorModeFactory
{
	FEditorModeFactory(FEditorModeInfo InModeInfo) : ModeInfo(InModeInfo) {}
	virtual ~FEditorModeFactory() {}

	/** Information pertaining to this factory's mode */
	FEditorModeInfo ModeInfo;

	/** Callback used to create an instance of this mode type */
	FEditorModeFactoryCallback FactoryCallback;

	/**
	 * Gets the information pertaining to the mode type that this factory creates
	 */
	virtual FEditorModeInfo GetModeInfo() const override { return ModeInfo; }

	/**
	 * Create a new instance of our mode
	 */
	virtual TSharedRef<FEdMode> CreateMode() const override { return FactoryCallback.Execute(); }
};

/**
 * A registry of editor modes and factories
 */
class FEditorModeRegistry
{
	typedef TMap<FEditorModeID, TSharedRef<IEditorModeFactory>> FactoryMap;

public:

	/**
	 * Initialize this registry
	 */
	static void Initialize();

	/**
	 * Shutdown this registry
	 */
	static void Shutdown();

	/**
	 * Singleton access
	 */
	UNREALED_API static FEditorModeRegistry& Get();

	/**
	 * Get a list of information for all currently registered modes, sorted by UI priority order
	 */
	UNREALED_API TArray<FEditorModeInfo> GetSortedModeInfo() const;

	/**
	 * Get a currently registered mode information for specified ID
	 */
	UNREALED_API FEditorModeInfo GetModeInfo(FEditorModeID ModeID) const;

	/**
	 * Registers an editor mode. Typically called from a module's StartupModule() routine.
	 *
	 * @param ModeID	ID of the mode to register
	 */
	UNREALED_API void RegisterMode(FEditorModeID ModeID, TSharedRef<IEditorModeFactory> Factory);

	/**
	 * Registers an editor mode type. Typically called from a module's StartupModule() routine.
	 *
	 * @param ModeID	ID of the mode to register
	 */
	template<class T>
	void RegisterMode(FEditorModeID ModeID, FText Name = FText(), FSlateIcon IconBrush = FSlateIcon(), bool bVisible = false, int32 PriorityOrder = MAX_int32)
	{
		TSharedRef<FEditorModeFactory> Factory = MakeShareable( new FEditorModeFactory(FEditorModeInfo(ModeID, Name, IconBrush, bVisible, PriorityOrder)) );

		Factory->FactoryCallback = FEditorModeFactoryCallback::CreateStatic([]() -> TSharedRef<FEdMode>{
			return MakeShareable(new T);
		});
		RegisterMode(ModeID, Factory);
	}

	/**
	 * Unregisters an editor mode. Typically called from a module's ShutdownModule() routine.
	 * Note: will exit the edit mode if it is currently active.
	 *
	 * @param ModeID	ID of the mode to unregister
	 */
	UNREALED_API void UnregisterMode(FEditorModeID ModeID);

	/**
	 * Event that is triggered whenever a mode is registered or unregistered
	 */
	DECLARE_EVENT(FEditorModeRegistry, FRegisteredModesChangedEvent);
	FRegisteredModesChangedEvent& OnRegisteredModesChanged() { return RegisteredModesChanged; }
	
	/**
	 * Event that is triggered whenever a mode is registered
	 */
	DECLARE_EVENT_OneParam(FEditorModeRegistry, FOnModeRegistered, FEditorModeID);
	FOnModeRegistered& OnModeRegistered() { return OnModeRegisteredEvent; }
	
	/**
	 * Event that is triggered whenever a mode is unregistered
	 */
	DECLARE_EVENT_OneParam(FEditorModeRegistry, FOnModeUnregistered, FEditorModeID);
	FOnModeUnregistered& OnModeUnregistered() { return OnModeUnregisteredEvent; }

	/**
	 * Create a new instance of the mode registered under the specified ID
	 */
	TSharedPtr<FEdMode> CreateMode(FEditorModeID ModeID, FEditorModeTools& Owner);

	/**
	 * Const access to the internal factory map
	 */
	const FactoryMap& GetFactoryMap() const { return ModeFactories; }


private:

	/** A map of editor mode IDs to factory callbacks */
	FactoryMap ModeFactories;
	
	/** A list of all modes created */
	TArray<TWeakPtr<FEdMode>> CreatedModes;

	/** Event that is triggered whenever a mode is registered or unregistered */
	FRegisteredModesChangedEvent RegisteredModesChanged;

	/** Event that is triggered whenever a mode is registered */
	FOnModeRegistered OnModeRegisteredEvent;

	/** Event that is triggered whenever a mode is unregistered */
	FOnModeUnregistered OnModeUnregisteredEvent;
};
