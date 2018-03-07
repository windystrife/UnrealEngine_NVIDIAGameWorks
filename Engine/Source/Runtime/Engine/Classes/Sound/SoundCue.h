// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Templates/SubclassOf.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundNode.h"
#include "SoundCue.generated.h"

class USoundCue;
class USoundNode;
struct FActiveSound;
struct FSoundParseParameters;

USTRUCT()
struct FSoundNodeEditorData
{
	GENERATED_USTRUCT_BODY()

	int32 NodePosX;

	int32 NodePosY;


	FSoundNodeEditorData()
		: NodePosX(0)
		, NodePosY(0)
	{
	}


	friend FArchive& operator<<(FArchive& Ar,FSoundNodeEditorData& MySoundNodeEditorData)
	{
		return Ar << MySoundNodeEditorData.NodePosX << MySoundNodeEditorData.NodePosY;
	}
};

#if WITH_EDITOR
class USoundCue;

/** Interface for sound cue graph interaction with the AudioEditor module. */
class ISoundCueAudioEditor
{
public:
	virtual ~ISoundCueAudioEditor() {}

	/** Called when creating a new sound cue graph. */
	virtual UEdGraph* CreateNewSoundCueGraph(USoundCue* InSoundCue) = 0;

	/** Sets up a sound node. */
	virtual void SetupSoundNode(UEdGraph* SoundCueGraph, USoundNode* SoundNode, bool bSelectNewNode) = 0;

	/** Links graph nodes from sound nodes. */
	virtual void LinkGraphNodesFromSoundNodes(USoundCue* SoundCue) = 0;

	/** Compiles sound nodes from graph nodes. */
	virtual void CompileSoundNodesFromGraphNodes(USoundCue* SoundCue) = 0;

	/** Removes nodes which are null from the sound cue graph. */
	virtual void RemoveNullNodes(USoundCue* SoundCue) = 0;

	/** Creates an input pin on the given sound cue graph node. */
	virtual void CreateInputPin(UEdGraphNode* SoundCueNode) = 0;

	/** Renames all pins in a sound cue node */
	virtual void RenameNodePins(USoundNode* SoundNode) = 0;
};
#endif

/**
 * The behavior of audio playback is defined within Sound Cues.
 */
UCLASS(hidecategories=object, MinimalAPI, BlueprintType)
class USoundCue : public USoundBase
{
	GENERATED_UCLASS_BODY()

	/* Indicates whether attenuation should use the Attenuation Overrides or the Attenuation Settings asset */
	UPROPERTY(EditAnywhere, Category=Attenuation)
	uint32 bOverrideAttenuation:1;

	UPROPERTY()
	class USoundNode* FirstNode;

	/* Volume multiplier for the Sound Cue */
	UPROPERTY(EditAnywhere, Category=Sound, AssetRegistrySearchable)
	float VolumeMultiplier;

	/* Pitch multiplier for the Sound Cue */
	UPROPERTY(EditAnywhere, Category=Sound, AssetRegistrySearchable)
	float PitchMultiplier;

	/* Attenuation settings to use if Override Attenuation is set to true */
	UPROPERTY(EditAnywhere, Category=AttenuationSettings, meta=(EditCondition="bOverrideAttenuation"))
	FSoundAttenuationSettings AttenuationOverrides;

#if WITH_EDITORONLY_DATA
	UPROPERTY()
	TArray<USoundNode*> AllNodes;

	UPROPERTY()
	class UEdGraph* SoundCueGraph;
#endif

protected:
	// NOTE: Use GetSubtitlePriority() to fetch this value for external use.
	UPROPERTY(EditAnywhere, Category = Subtitles, Meta = (Tooltip = "The priority of the subtitle.  Defaults to 10000.  Higher values will play instead of lower values."))
	float SubtitlePriority;

private:
	float MaxAudibleDistance;

public:

	//~ Begin UObject Interface.
	virtual void GetResourceSizeEx(FResourceSizeEx& CumulativeResourceSize) override;
	virtual FString GetDesc() override;
#if WITH_EDITOR
	virtual void PostInitProperties() override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);
#endif
	virtual void PostLoad() override;
	virtual void Serialize(FArchive& Ar) override;
	//~ End UObject Interface.

	//~ Begin USoundBase Interface.
	virtual bool IsPlayable() const override;
	virtual bool ShouldApplyInteriorVolumes() const override;
	virtual void Parse( class FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances ) override;
	virtual float GetVolumeMultiplier() override;
	virtual float GetPitchMultiplier() override;
	virtual float GetMaxAudibleDistance() override;
	virtual float GetDuration() override;
	virtual const FSoundAttenuationSettings* GetAttenuationSettingsToApply() const override;
	virtual float GetSubtitlePriority() const override;
	//~ End USoundBase Interface.

	/** Construct and initialize a node within this Cue */
	template<class T>
	T* ConstructSoundNode(TSubclassOf<USoundNode> SoundNodeClass = T::StaticClass(), bool bSelectNewNode = true)
	{
		// Set flag to be transactional so it registers with undo system
		T* SoundNode = NewObject<T>(this, SoundNodeClass, NAME_None, RF_Transactional);
#if WITH_EDITOR
		AllNodes.Add(SoundNode);
		SetupSoundNode(SoundNode, bSelectNewNode);
#endif // WITH_EDITORONLY_DATA
		return SoundNode;
	}

	/**
	*	@param		Format		Format to check
	 *
	 *	@return		Sum of the size of waves referenced by this cue for the given platform.
	 */
	virtual int32 GetResourceSizeForFormat(FName Format);

	/**
	 * Recursively finds all Nodes in the Tree
	 */
	ENGINE_API void RecursiveFindAllNodes( USoundNode* Node, TArray<class USoundNode*>& OutNodes );

	/**
	 * Recursively finds sound nodes of type T
	 */
	template<typename T> void RecursiveFindNode(USoundNode* Node, TArray<T*>& OutNodes)
	{
		if (Node)
		{
			// Record the node if it is the desired type
			if (T* FoundNode = Cast<T>(Node))
			{
				OutNodes.AddUnique(FoundNode);
			}

			// Recurse.
			const int32 MaxChildNodes = Node->GetMaxChildNodes();
			for (int32 ChildIndex = 0; ChildIndex < Node->ChildNodes.Num() && ChildIndex < MaxChildNodes; ++ChildIndex)
			{
				RecursiveFindNode<T>(Node->ChildNodes[ChildIndex], OutNodes);
			}
		}
	}

	/** Find the path through the sound cue to a node identified by its hash */
	ENGINE_API bool FindPathToNode(const UPTRINT NodeHashToFind, TArray<USoundNode*>& OutPath) const;

	/** Call when the audio quality has been changed */
	ENGINE_API static void StaticAudioQualityChanged(int32 NewQualityLevel);

	FORCEINLINE static int32 GetCachedQualityLevel() { return CachedQualityLevel; }

protected:
	bool RecursiveFindPathToNode(USoundNode* CurrentNode, const UPTRINT CurrentHash, const UPTRINT NodeHashToFind, TArray<USoundNode*>& OutPath) const;

private:
	void AudioQualityChanged();
	void OnPostEngineInit();
	void EvaluateNodes(bool bAddToRoot);

	FDelegateHandle OnPostEngineInitHandle;
	static int32 CachedQualityLevel;

public:

	/**
	 * Instantiate certain functions to work around a linker issue
	 */
	ENGINE_API void RecursiveFindAttenuation( USoundNode* Node, TArray<class USoundNodeAttenuation*> &OutNodes );

#if WITH_EDITOR
	/** Create the basic sound graph */
	ENGINE_API void CreateGraph();

	/** Clears all nodes from the graph (for old editor's buffer soundcue) */
	ENGINE_API void ClearGraph();

	/** Set up EdGraph parts of a SoundNode */
	ENGINE_API void SetupSoundNode(class USoundNode* InSoundNode, bool bSelectNewNode = true);

	/** Use the SoundCue's children to link EdGraph Nodes together */
	ENGINE_API void LinkGraphNodesFromSoundNodes();

	/** Use the EdGraph representation to compile the SoundCue */
	ENGINE_API void CompileSoundNodesFromGraphNodes();

	/** Get the EdGraph of SoundNodes */
	ENGINE_API class UEdGraph* GetGraph();

	/** Sets the sound cue graph editor implementation.* */
	static ENGINE_API void SetSoundCueAudioEditor(TSharedPtr<ISoundCueAudioEditor> InSoundCueGraphEditor);

	/** Gets the sound cue graph editor implementation. */
	static TSharedPtr<ISoundCueAudioEditor> ENGINE_API GetSoundCueAudioEditor();

private:

	/** Ptr to interface to sound cue editor operations. */
	static ENGINE_API TSharedPtr<ISoundCueAudioEditor> SoundCueAudioEditor;

#endif
};



