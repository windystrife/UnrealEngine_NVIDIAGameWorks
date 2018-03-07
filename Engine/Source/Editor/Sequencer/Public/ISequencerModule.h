// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "ISequencer.h"
#include "Modules/ModuleInterface.h"
#include "AnimatedPropertyKey.h"

class FExtender;
class FExtensibilityManager;
class FMenuBuilder;
class ISequencerTrackEditor;
class ISequencerEditorObjectBinding;
class IToolkitHost;
class UMovieSceneSequence;

namespace SequencerMenuExtensionPoints
{
	static const FName AddTrackMenu_PropertiesSection("AddTrackMenu_PropertiesSection");
}


/** A delegate which will create an auto-key handler. */
DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<ISequencerTrackEditor>, FOnCreateTrackEditor, TSharedRef<ISequencer>);

/** A delegate which will create an object binding handler. */
DECLARE_DELEGATE_RetVal_OneParam(TSharedRef<ISequencerEditorObjectBinding>, FOnCreateEditorObjectBinding, TSharedRef<ISequencer>);

/** A delegate that is executed when adding menu content. */
DECLARE_DELEGATE_TwoParams(FOnGetAddMenuContent, FMenuBuilder& /*MenuBuilder*/, TSharedRef<ISequencer>);

/** A delegate that gets executed then a sequencer is created */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnSequencerCreated, TSharedRef<ISequencer>);

/**
 * Sequencer view parameters.
 */
struct FSequencerViewParams
{
	/** Initial Scrub Position. */
	float InitialScrubPosition;

	FOnGetAddMenuContent OnGetAddMenuContent;

	/** Called when this sequencer has received user focus */
	FSimpleDelegate OnReceivedFocus;

	/** A menu extender for the add menu */
	TSharedPtr<FExtender> AddMenuExtender;

	/** A toolbar extender for the main toolbar */
	TSharedPtr<FExtender> ToolbarExtender;

	/** Unique name for the sequencer. */
	FString UniqueName;

	/** Whether the sequencer is read-only */
	bool bReadOnly;

	FSequencerViewParams(FString InName = FString())
		: InitialScrubPosition(0.0f)
		, UniqueName(MoveTemp(InName))
		, bReadOnly(false)
	{ }
};


/**
 * Sequencer initialization parameters.
 */
struct FSequencerInitParams
{
	/** The root movie scene sequence being edited. */
	UMovieSceneSequence* RootSequence;

	/** The asset editor created for this (if any) */
	TSharedPtr<IToolkitHost> ToolkitHost;

	/** View parameters */
	FSequencerViewParams ViewParams;

	/** Whether or not sequencer should be edited within the level editor */
	bool bEditWithinLevelEditor;

	/** Domain-specific spawn register for the movie scene */
	TSharedPtr<FMovieSceneSpawnRegister> SpawnRegister;

	/** Accessor for event contexts */
	TAttribute<TArray<UObject*>> EventContexts;

	/** Accessor for playback context */
	TAttribute<UObject*> PlaybackContext;

	FSequencerInitParams()
		: RootSequence(nullptr)
		, ToolkitHost(nullptr)
		, bEditWithinLevelEditor(false)
		, SpawnRegister(nullptr)
	{}
};


/**
 * Interface for the Sequencer module.
 */
class ISequencerModule
	: public IModuleInterface
{
public:

	/**
	 * Create a new instance of a standalone sequencer that can be added to other UIs.
	 *
	 * @param InitParams Initialization parameters.
	 * @return The new sequencer object.
	 */
	virtual TSharedRef<ISequencer> CreateSequencer(const FSequencerInitParams& InitParams) = 0;

	/** 
	 * Registers a delegate that will create an editor for a track in each sequencer.
	 *
	 * @param InOnCreateTrackEditor	Delegate to register.
	 * @return A handle to the newly-added delegate.
	 */
	virtual FDelegateHandle RegisterTrackEditor(FOnCreateTrackEditor InOnCreateTrackEditor, TArrayView<FAnimatedPropertyKey> AnimatedPropertyTypes = TArrayView<FAnimatedPropertyKey>()) = 0;

	/** 
	 * Unregisters a previously registered delegate for creating a track editor
	 *
	 * @param InHandle	Handle to the delegate to unregister
	 */
	virtual void UnRegisterTrackEditor(FDelegateHandle InHandle) = 0;

	/** 
	 * Registers a delegate that will be called when a sequencer is created
	 *
	 * @param InOnSequencerCreated	Delegate to register.
	 * @return A handle to the newly-added delegate.
	 */
	virtual FDelegateHandle RegisterOnSequencerCreated(FOnSequencerCreated::FDelegate InOnSequencerCreated) = 0;

	/** 
	 * Unregisters a previously registered delegate called when a sequencer is created
	 *
	 * @param InHandle	Handle to the delegate to unregister
	 */
	virtual void UnregisterOnSequencerCreated(FDelegateHandle InHandle) = 0;

	/** 
	 * Registers a delegate that will create editor UI for an object binding in sequencer.
	 *
	 * @param InOnCreateEditorObjectBinding	Delegate to register.
	 * @return A handle to the newly-added delegate.
	 */
	virtual FDelegateHandle RegisterEditorObjectBinding(FOnCreateEditorObjectBinding InOnCreateEditorObjectBinding) = 0;

	/** 
	 * Unregisters a previously registered delegate for creating editor UI for an object binding in sequencer.
	 *
	 * @param InHandle	Handle to the delegate to unregister
	 */
	virtual void UnRegisterEditorObjectBinding(FDelegateHandle InHandle) = 0;

	/**
	 * Register that the specified property type can be animated in sequencer
	 */
	virtual void RegisterPropertyAnimator(FAnimatedPropertyKey Key) = 0;

	/**
	 * Unregister that the specified property type can be animated in sequencer
	 */
	virtual void UnRegisterPropertyAnimator(FAnimatedPropertyKey Key) = 0;

	/**
	 * Check whether the specified property type can be animated by sequeuncer
	 */
	virtual bool CanAnimateProperty(FAnimatedPropertyKey Key) = 0;

	/**
	* Get the extensibility manager for menus.
	*
	* @return ObjectBinding Context Menu extensibility manager.
	*/
	virtual TSharedPtr<FExtensibilityManager> GetObjectBindingContextMenuExtensibilityManager() const = 0;

	/**
	 * Get the extensibility manager for menus.
	 *
	 * @return Add Track Menu extensibility manager.
	 */
	virtual TSharedPtr<FExtensibilityManager> GetAddTrackMenuExtensibilityManager() const = 0;

	/**
	 * Get the extensibility manager for toolbars.
	 *
	 * @return Toolbar extensibility manager.
	 */
	virtual TSharedPtr<FExtensibilityManager> GetToolBarExtensibilityManager() const = 0;

public:

	/** 
	 * Helper template for registering property track editors
	 *
	 * @param InOnCreateTrackEditor	Delegate to register.
	 * @return A handle to the newly-added delegate.
	 */
	template<typename PropertyTrackEditorType>
	FDelegateHandle RegisterPropertyTrackEditor()
	{
		auto PropertyTypes = PropertyTrackEditorType::GetAnimatedPropertyTypes();
		return RegisterTrackEditor(FOnCreateTrackEditor::CreateStatic(PropertyTrackEditorType::CreateTrackEditor), PropertyTypes);
	}

public:

	DEPRECATED(4.16, "Please use RegisterTrackEditor")
	FDelegateHandle RegisterTrackEditor_Handle(FOnCreateTrackEditor InOnCreateTrackEditor)
	{
		return RegisterTrackEditor(InOnCreateTrackEditor, TArrayView<FAnimatedPropertyKey>());
	}
	
	DEPRECATED(4.16, "Please use UnRegisterTrackEditor")
	void UnRegisterTrackEditor_Handle(FDelegateHandle InHandle)
	{
		UnRegisterTrackEditor(InHandle);
	}
};
