// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Toolkits/AssetEditorToolkit.h"
#include "ISoundClassEditor.h"
#include "ISoundSubmixEditor.h"
#include "ISoundCueEditor.h"

class UDialogueWave;
class USoundClass;
class USoundCue;
class USoundSubmix;
class USoundNode;
class USoundWave;
struct FDialogueContextMapping;

AUDIOEDITOR_API DECLARE_LOG_CATEGORY_EXTERN(LogAudioEditor, Log, All);

extern const FName AudioEditorAppIdentifier;

/** Interface which can be implemented in plugin to extend sound wave asset actions. */
class ISoundWaveAssetActionExtensions
{
public:
	virtual void GetExtendedActions(const TArray<TWeakObjectPtr<USoundWave>>& InObjects, FMenuBuilder& MenuBuilder) = 0;
};

/** Sound class editor module interface */
class IAudioEditorModule :	public IModuleInterface
{
public:
	/** Registers audio editor asset actions. */
	virtual void RegisterAssetActions() = 0;

	/** Registers audio editor asset actions specific to audio mixer functionality. */
	virtual void RegisterAudioMixerAssetActions() = 0;

	/** Registers effect preset asset actions. */
	virtual void RegisterEffectPresetAssetActions() {}

	/** Adds an a sound wave asset action extender class to allow extending asset wave actions to plugins. */
	virtual void AddSoundWaveActionExtender(TSharedPtr<ISoundWaveAssetActionExtensions> InSoundWaveAssetActionExtender) = 0;

	/** Returns all added sound wave action extenders currently added. */
	virtual void GetSoundWaveActionExtenders(TArray<TSharedPtr<ISoundWaveAssetActionExtensions>>& OutSoundwaveActionExtensions) = 0;

	/** Creates a new sound class editor for a sound class object. */
	virtual TSharedRef<FAssetEditorToolkit> CreateSoundClassEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, USoundClass* InSoundClass ) = 0;

	/** Creates a new sound submix editor for a sound submix object. */
	virtual TSharedRef<FAssetEditorToolkit> CreateSoundSubmixEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, USoundSubmix* InSoundSubmix) = 0;

	/** Returns the menu extensibility manager for the given audio editor type. */
	virtual TSharedPtr<FExtensibilityManager> GetSoundClassMenuExtensibilityManager() = 0;

	/** Returns the toolbar extensibility manager for the given audio editor type. */
	virtual TSharedPtr<FExtensibilityManager> GetSoundClassToolBarExtensibilityManager() = 0;

	/** Returns the menu extensibility manager for the given audio editor type. */
	virtual TSharedPtr<FExtensibilityManager> GetSoundSubmixMenuExtensibilityManager() = 0;

	/** Returns the toolbar extensibility manager for the given audio editor type. */
	virtual TSharedPtr<FExtensibilityManager> GetSoundSubmixToolBarExtensibilityManager() = 0;

	/** Creates a new material editor, either for a material or a material function. */
	virtual TSharedRef<ISoundCueEditor> CreateSoundCueEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, USoundCue* SoundCue) = 0;

	/** Returns the menu extensibility manager for the given audio editor type. */
	virtual TSharedPtr<FExtensibilityManager> GetSoundCueMenuExtensibilityManager() = 0;

	/** Returns the toolbar extensibility manager for the given audio editor type. */
	virtual TSharedPtr<FExtensibilityManager> GetSoundCueToolBarExtensibilityManager() = 0;

	/** Replaces sound cue nodes in the graph. */
	virtual void ReplaceSoundNodesInGraph(USoundCue* SoundCue, UDialogueWave* DialogueWave, TArray<USoundNode*>& NodesToReplace, const FDialogueContextMapping& ContextMapping) = 0;

	/** Imports a sound wave from the given package. */
	virtual USoundWave* ImportSoundWave(UPackage* const SoundWavePackage, const FString& InSoundWaveAssetName, const FString& InWavFilename) = 0;
};
