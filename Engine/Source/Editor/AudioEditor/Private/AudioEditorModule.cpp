// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AudioEditorModule.h"
#include "Modules/ModuleManager.h"
#include "Sound/SoundNodeDialoguePlayer.h"
#include "EdGraphUtilities.h"
#include "SoundCueGraphConnectionDrawingPolicy.h"
#include "Factories/SoundFactory.h"
#include "Factories/ReimportSoundFactory.h"
#include "SoundCueGraph/SoundCueGraphNode.h"
#include "SoundCueGraphNodeFactory.h"
#include "Factories/ReimportSoundSurroundFactory.h"
#include "AssetToolsModule.h"
#include "SoundClassEditor.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundWave.h"
#include "Sound/SoundSubmix.h"
#include "Sound/SoundEffectPreset.h"
#include "SoundCueEditor.h"
#include "SoundSubmixEditor.h"
#include "Classes/Sound/AudioSettings.h"
#include "AssetTypeActions/AssetTypeActions_DialogueVoice.h"
#include "AssetTypeActions/AssetTypeActions_DialogueWave.h"
#include "AssetTypeActions/AssetTypeActions_SoundAttenuation.h"
#include "AssetTypeActions/AssetTypeActions_SoundConcurrency.h"
#include "AssetTypeActions/AssetTypeActions_SoundBase.h"
#include "AssetTypeActions/AssetTypeActions_SoundClass.h"
#include "AssetTypeActions/AssetTypeActions_SoundCue.h"
#include "AssetTypeActions/AssetTypeActions_SoundMix.h"
#include "AssetTypeActions/AssetTypeActions_SoundWave.h"
#include "AssetTypeActions/AssetTypeActions_ReverbEffect.h"
#include "AssetTypeActions/AssetTypeActions_SoundSubmix.h"
#include "AssetTypeActions/AssetTypeActions_SoundEffectPreset.h"
#include "AssetTypeActions/AssetTypeActions_SoundSourceBus.h"
#include "Utils.h"
#include "UObject/UObjectIterator.h"

const FName AudioEditorAppIdentifier = FName(TEXT("AudioEditorApp"));

DEFINE_LOG_CATEGORY(LogAudioEditor);

class FAudioEditorModule : public IAudioEditorModule
{
public:
	FAudioEditorModule()
	{
	}

	virtual void StartupModule() override
	{
		SoundClassExtensibility.Init();
		SoundCueExtensibility.Init();
		SoundSubmixExtensibility.Init();

		// Register the sound cue graph connection policy with the graph editor
		SoundCueGraphConnectionFactory = MakeShareable(new FSoundCueGraphConnectionDrawingPolicyFactory);
		FEdGraphUtilities::RegisterVisualPinConnectionFactory(SoundCueGraphConnectionFactory);

		TSharedPtr<FSoundCueGraphNodeFactory> SoundCueGraphNodeFactory = MakeShareable(new FSoundCueGraphNodeFactory());
		FEdGraphUtilities::RegisterVisualNodeFactory(SoundCueGraphNodeFactory);

		// Create reimport handler for sound node waves
		UReimportSoundFactory::StaticClass();

		// Create reimport handler for surround sound waves
		UReimportSoundSurroundFactory::StaticClass();
	}

	virtual void ShutdownModule() override
	{
		SoundClassExtensibility.Reset();
		SoundCueExtensibility.Reset();
		SoundSubmixExtensibility.Reset();

		if (SoundCueGraphConnectionFactory.IsValid())
		{
			FEdGraphUtilities::UnregisterVisualPinConnectionFactory(SoundCueGraphConnectionFactory);
		}
	}

	virtual void RegisterAssetActions() override
	{
		// Register the audio editor asset type actions
		IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

		AssetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_DialogueVoice));
		AssetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_DialogueWave));
		AssetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_SoundAttenuation));
		AssetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_SoundConcurrency));
		AssetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_SoundBase));
		AssetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_SoundClass));
		AssetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_SoundCue));
		AssetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_SoundMix));
		AssetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_SoundWave));
		AssetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_ReverbEffect));
	}

	virtual void RegisterAudioMixerAssetActions() override
	{
		// Only register asset actions for when audio mixer data is enabled
		if (GetDefault<UAudioSettings>()->IsAudioMixerEnabled())
		{
			IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();
			AssetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_SoundSubmix));
			AssetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_SoundEffectSubmixPreset));
			AssetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_SoundEffectSourcePreset));
			AssetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_SoundEffectSourcePresetChain));
			AssetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_SoundSourceBus));
		}
	}

	virtual void AddSoundWaveActionExtender(TSharedPtr<ISoundWaveAssetActionExtensions> InSoundWaveAssetActionExtender) override
	{
		if (InSoundWaveAssetActionExtender.IsValid())
		{
			SoundWaveAssetActionExtensions.AddUnique(InSoundWaveAssetActionExtender);
		}
	}

	virtual void GetSoundWaveActionExtenders(TArray<TSharedPtr<ISoundWaveAssetActionExtensions>>& OutSoundwaveActionExtensions) override
	{
		OutSoundwaveActionExtensions = SoundWaveAssetActionExtensions;
	}

	virtual void RegisterEffectPresetAssetActions() override
	{
		// Only register asset actions for the case where audio mixer data is enabled
		if (GetDefault<UAudioSettings>()->IsAudioMixerEnabled())
		{
			// Register the audio editor asset type actions
			IAssetTools& AssetTools = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools").Get();

			// Look for any sound effect presets to register
			for (TObjectIterator<UClass> It; It; ++It)
			{
				UClass* ChildClass = *It;
				if (ChildClass->HasAnyClassFlags(CLASS_Abstract))
				{
					continue;
				}

				// Look for submix or source preset classes
				UClass* ParentClass = ChildClass->GetSuperClass();
				if (ParentClass->IsChildOf(USoundEffectSourcePreset::StaticClass()) || ParentClass->IsChildOf(USoundEffectSubmixPreset::StaticClass()))
				{
					USoundEffectPreset* EffectPreset = ChildClass->GetDefaultObject<USoundEffectPreset>();
					if (!RegisteredActions.Contains(EffectPreset) && EffectPreset->HasAssetActions())
					{
						RegisteredActions.Add(EffectPreset);
						AssetTools.RegisterAssetTypeActions(MakeShareable(new FAssetTypeActions_SoundEffectPreset(EffectPreset)));
					}
				}
			}
		}
	}

	virtual TSharedRef<FAssetEditorToolkit> CreateSoundClassEditor( const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, USoundClass* InSoundClass ) override
	{
		TSharedRef<FSoundClassEditor> NewSoundClassEditor(new FSoundClassEditor());
		NewSoundClassEditor->InitSoundClassEditor(Mode, InitToolkitHost, InSoundClass);
		return NewSoundClassEditor;
	}

	virtual TSharedRef<FAssetEditorToolkit> CreateSoundSubmixEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, USoundSubmix* InSoundSubmix) override
	{
		TSharedRef<FSoundSubmixEditor> NewSoundSubmixEditor(new FSoundSubmixEditor());
		NewSoundSubmixEditor->InitSoundSubmixEditor(Mode, InitToolkitHost, InSoundSubmix);
		return NewSoundSubmixEditor;
	}

	virtual TSharedPtr<FExtensibilityManager> GetSoundClassMenuExtensibilityManager() override
	{
		return SoundClassExtensibility.MenuExtensibilityManager;
	}

	virtual TSharedPtr<FExtensibilityManager> GetSoundClassToolBarExtensibilityManager() override
	{
		return SoundClassExtensibility.ToolBarExtensibilityManager;
	}

	virtual TSharedPtr<FExtensibilityManager> GetSoundSubmixMenuExtensibilityManager() override
	{
		return SoundSubmixExtensibility.MenuExtensibilityManager;
	}

	virtual TSharedPtr<FExtensibilityManager> GetSoundSubmixToolBarExtensibilityManager() override
	{
		return SoundSubmixExtensibility.ToolBarExtensibilityManager;
	}

	virtual TSharedRef<ISoundCueEditor> CreateSoundCueEditor(const EToolkitMode::Type Mode, const TSharedPtr< IToolkitHost >& InitToolkitHost, USoundCue* SoundCue) override
	{
		TSharedRef<FSoundCueEditor> NewSoundCueEditor(new FSoundCueEditor());
		NewSoundCueEditor->InitSoundCueEditor(Mode, InitToolkitHost, SoundCue);
		return NewSoundCueEditor;
	}

	virtual TSharedPtr<FExtensibilityManager> GetSoundCueMenuExtensibilityManager() override
	{
		return SoundCueExtensibility.MenuExtensibilityManager;
	}

	virtual TSharedPtr<FExtensibilityManager> GetSoundCueToolBarExtensibilityManager() override
	{
		return SoundCueExtensibility.MenuExtensibilityManager;
	}

	virtual void ReplaceSoundNodesInGraph(USoundCue* SoundCue, UDialogueWave* DialogueWave, TArray<USoundNode*>& NodesToReplace, const FDialogueContextMapping& ContextMapping) override
	{
		// Replace any sound nodes in the graph.
		TArray<USoundCueGraphNode*> GraphNodesToRemove;
		for (USoundNode* const SoundNode : NodesToReplace)
		{
			// Create the new dialogue wave player.
			USoundNodeDialoguePlayer* DialoguePlayer = SoundCue->ConstructSoundNode<USoundNodeDialoguePlayer>();
			DialoguePlayer->SetDialogueWave(DialogueWave);
			DialoguePlayer->DialogueWaveParameter.Context = ContextMapping.Context;

			// We won't need the newly created graph node as we're about to move the dialogue wave player onto the original node.
			GraphNodesToRemove.Add(CastChecked<USoundCueGraphNode>(DialoguePlayer->GetGraphNode()));

			// Swap out the sound wave player in the graph node with the new dialogue wave player.
			USoundCueGraphNode* SoundGraphNode = CastChecked<USoundCueGraphNode>(SoundNode->GetGraphNode());
			SoundGraphNode->SetSoundNode(DialoguePlayer);
		}

		for (USoundCueGraphNode* const SoundGraphNode : GraphNodesToRemove)
		{
			SoundCue->GetGraph()->RemoveNode(SoundGraphNode);
		}

		// Make sure the cue is updated to match its graph.
		SoundCue->CompileSoundNodesFromGraphNodes();

		for (USoundNode* const SoundNode : NodesToReplace)
		{
			// Remove the old node from the list of available nodes.
			SoundCue->AllNodes.Remove(SoundNode);
		}
		SoundCue->MarkPackageDirty();
	}

	USoundWave* ImportSoundWave(UPackage* const SoundWavePackage, const FString& InSoundWaveAssetName, const FString& InWavFilename) override
	{
		USoundFactory* SoundWaveFactory = NewObject<USoundFactory>();

		// Setup sane defaults for importing localized sound waves
		SoundWaveFactory->bAutoCreateCue = false;
		SoundWaveFactory->SuppressImportOverwriteDialog();

		return ImportObject<USoundWave>(SoundWavePackage, *InSoundWaveAssetName, RF_Public | RF_Standalone, *InWavFilename, nullptr, SoundWaveFactory);
	}

private:

	struct FExtensibilityManagers
	{
		TSharedPtr<FExtensibilityManager> MenuExtensibilityManager;
		TSharedPtr<FExtensibilityManager> ToolBarExtensibilityManager;

		void Init()
		{
			MenuExtensibilityManager = MakeShareable(new FExtensibilityManager);
			ToolBarExtensibilityManager = MakeShareable(new FExtensibilityManager);
		}

		void Reset()
		{
			MenuExtensibilityManager.Reset();
			ToolBarExtensibilityManager.Reset();
		}
	};

	FExtensibilityManagers SoundCueExtensibility;
	FExtensibilityManagers SoundClassExtensibility;
	FExtensibilityManagers SoundSubmixExtensibility;
	TArray<TSharedPtr<ISoundWaveAssetActionExtensions>> SoundWaveAssetActionExtensions;
	TSet<USoundEffectPreset*> RegisteredActions;
	TSharedPtr<struct FGraphPanelPinConnectionFactory> SoundCueGraphConnectionFactory;

};

IMPLEMENT_MODULE( FAudioEditorModule, AudioEditor );
