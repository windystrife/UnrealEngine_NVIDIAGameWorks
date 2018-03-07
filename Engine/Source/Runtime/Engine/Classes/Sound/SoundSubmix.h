// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "SoundEffectSubmix.h"
#include "SoundSubmix.generated.h"

class UEdGraph;
class USoundEffectSubmixPreset;
class USoundSubmix;


// Class used to send audio to submixes from USoundBase
USTRUCT(BlueprintType)
struct ENGINE_API FSoundSubmixSendInfo
{
	GENERATED_USTRUCT_BODY()

	// The amount of audio to send
	UPROPERTY(EditAnywhere, Category = SubmixSend)
	float SendLevel;
	
	// The submix to send the audio to
	UPROPERTY(EditAnywhere, Category = SubmixSend)
	USoundSubmix* SoundSubmix;
};

#if WITH_EDITOR

/** Interface for sound submix graph interaction with the AudioEditor module. */
class ISoundSubmixAudioEditor
{
public:
	virtual ~ISoundSubmixAudioEditor() {}

	/** Refreshes the sound class graph links. */
	virtual void RefreshGraphLinks(UEdGraph* SoundClassGraph) = 0;
};
#endif

UCLASS()
class ENGINE_API USoundSubmix : public UObject
{
	GENERATED_UCLASS_BODY()

	// Child submixes to this sound mix
	UPROPERTY(EditAnywhere, Category = SoundSubmix)
	TArray<USoundSubmix*> ChildSubmixes;

	UPROPERTY()
	USoundSubmix* ParentSubmix;

#if WITH_EDITORONLY_DATA
	/** EdGraph based representation of the SoundSubmix */
	class UEdGraph* SoundSubmixGraph;
#endif

	UPROPERTY(EditAnywhere, Category = SoundSubmix)
	TArray<USoundEffectSubmixPreset*> SubmixEffectChain;

protected:

	//~ Begin UObject Interface.
	virtual FString GetDesc() override;
	virtual void BeginDestroy() override;
	virtual void PostLoad() override;
#if WITH_EDITOR
	virtual void PreEditChange(UProperty* PropertyAboutToChange) override;
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End UObject Interface.

public:

	// Sound Submix Editor functionality
#if WITH_EDITOR

	/**
	* @return true if the child sound class exists in the tree
	*/
	bool RecurseCheckChild(USoundSubmix* ChildSoundSubmix);

	/**
	* Set the parent submix of this SoundSubmix, removing it as a child from its previous owner
	*
	* @param	InParentSubmix	The New Parent Submix of this
	*/
	void SetParentSubmix(USoundSubmix* InParentSubmix);

	/**
	* Add Referenced objects
	*
	* @param	InThis SoundSubmix we are adding references from.
	* @param	Collector Reference Collector
	*/
	static void AddReferencedObjects(UObject* InThis, FReferenceCollector& Collector);

	/**
	* Refresh all EdGraph representations of SoundSubmixes
	*
	* @param	bIgnoreThis	Whether to ignore this SoundSubmix if it's already up to date
	*/
	void RefreshAllGraphs(bool bIgnoreThis);

	/** Sets the sound submix graph editor implementation.* */
	static void SetSoundSubmixAudioEditor(TSharedPtr<ISoundSubmixAudioEditor> InSoundSubmixAudioEditor);

	/** Gets the sound submix graph editor implementation. */
	static TSharedPtr<ISoundSubmixAudioEditor> GetSoundSubmixAudioEditor();

private:

	/** Ptr to interface to sound class editor operations. */
	static TSharedPtr<ISoundSubmixAudioEditor> SoundSubmixAudioEditor;

#endif

};

