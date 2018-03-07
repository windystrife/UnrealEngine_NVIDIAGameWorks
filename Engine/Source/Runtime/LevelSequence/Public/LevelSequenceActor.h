// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "UObject/SoftObjectPath.h"
#include "GameFramework/Actor.h"
#include "UObject/SoftObjectPath.h"
#include "LevelSequencePlayer.h"
#include "MovieSceneBindingOwnerInterface.h"
#include "MovieSceneBindingOverrides.h"
#include "LevelSequenceActor.generated.h"

class ULevelSequenceBurnIn;

UCLASS(Blueprintable, DefaultToInstanced)
class LEVELSEQUENCE_API ULevelSequenceBurnInInitSettings : public UObject
{
	GENERATED_BODY()
};

UCLASS(DefaultToInstanced)
class LEVELSEQUENCE_API ULevelSequenceBurnInOptions : public UObject
{
public:

	GENERATED_BODY()
	ULevelSequenceBurnInOptions(const FObjectInitializer& Init);

	/** Ensure the settings object is up-to-date */
	void ResetSettings();

public:

	UPROPERTY(EditAnywhere, Category="General")
	bool bUseBurnIn;

	UPROPERTY(EditAnywhere, Category="General", meta=(EditCondition=bUseBurnIn, MetaClass="LevelSequenceBurnIn"))
	FSoftClassPath BurnInClass;

	UPROPERTY(Instanced, EditAnywhere, Category="General", meta=(EditCondition=bUseBurnIn))
	ULevelSequenceBurnInInitSettings* Settings;

protected:

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif //WITH_EDITOR
};

/**
 * Actor responsible for controlling a specific level sequence in the world.
 */
UCLASS(hideCategories=(Rendering, Physics, LOD, Activation))
class LEVELSEQUENCE_API ALevelSequenceActor
	: public AActor
	, public IMovieSceneBindingOwnerInterface
{
public:

	GENERATED_BODY()

	/** Create and initialize a new instance. */
	ALevelSequenceActor(const FObjectInitializer& Init);

public:

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Playback")
	bool bAutoPlay;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Playback", meta=(ShowOnlyInnerProperties))
	FMovieSceneSequencePlaybackSettings PlaybackSettings;

	UPROPERTY(transient, BlueprintReadOnly, Category="Playback", meta=(ExposeFunctionCategories="Game|Cinematic"))
	ULevelSequencePlayer* SequencePlayer;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="General", meta=(AllowedClasses="LevelSequence"))
	FSoftObjectPath LevelSequence;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, AdvancedDisplay, Category="General")
	TArray<AActor*> AdditionalEventReceivers;

	UPROPERTY(Instanced, VisibleAnywhere, AdvancedDisplay, BlueprintReadOnly, Category="General")
	ULevelSequenceBurnInOptions* BurnInOptions;

	/** Mapping of actors to override the sequence bindings with */
	UPROPERTY(Instanced, VisibleAnywhere, AdvancedDisplay, BlueprintReadOnly, Category="General")
	UMovieSceneBindingOverrides* BindingOverrides;

public:

	/**
	 * Get the level sequence being played by this actor.
	 *
	 * @param bLoad Whether to load the sequence object if it is not already in memory.
	 * @param bInitializePlayer Whether to initialize the player when the sequence has been loaded.
	 * @return Level sequence, or nullptr if not assigned or if it cannot be loaded.
	 * @see SetSequence
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	ULevelSequence* GetSequence(bool bLoad = false, bool bInitializePlayer = false) const;

	/**
	 * Set the level sequence being played by this actor.
	 *
	 * @param InSequence The sequence object to set.
	 * @see GetSequence
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void SetSequence(ULevelSequence* InSequence);

	/**
	 * Set an array of additional actors that will receive events triggerd from this sequence actor
	 *
	 * @param AdditionalReceivers An array of actors to receive events
	 */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic")
	void SetEventReceivers(TArray<AActor*> AdditionalReceivers);

	/** Refresh this actor's burn in */
	void RefreshBurnIn();

public:

	/** Overrides the specified binding with the specified actors, optionally still allowing the bindings defined in the Level Sequence asset */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic|Bindings")
	void SetBinding(FMovieSceneObjectBindingID Binding, const TArray<AActor*>& Actors, bool bAllowBindingsFromAsset = false)
	{
		BindingOverrides->SetBinding(Binding, TArray<UObject*>(Actors), bAllowBindingsFromAsset);
		if (SequencePlayer)
		{
			SequencePlayer->State.Invalidate(Binding.GetGuid(), Binding.GetSequenceID());
		}
	}

	/** Adds the specified actor to the overridden bindings for the specified binding ID, optionally still allowing the bindings defined in the Level Sequence asset */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic|Bindings")
	void AddBinding(FMovieSceneObjectBindingID Binding, AActor* Actor, bool bAllowBindingsFromAsset = false)
	{
		BindingOverrides->AddBinding(Binding, Actor);
		if (SequencePlayer)
		{
			SequencePlayer->State.Invalidate(Binding.GetGuid(), Binding.GetSequenceID());
		}
	}

	/** Removes the specified actor from the specified binding's actor array */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic|Bindings")
	void RemoveBinding(FMovieSceneObjectBindingID Binding, AActor* Actor)
	{
		BindingOverrides->RemoveBinding(Binding, Actor);
		if (SequencePlayer)
		{
			SequencePlayer->State.Invalidate(Binding.GetGuid(), Binding.GetSequenceID());
		}
	}

	/** Resets the specified binding back to the defaults defined by the Level Sequence asset */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic|Bindings")
	void ResetBinding(FMovieSceneObjectBindingID Binding)
	{
		BindingOverrides->ResetBinding(Binding);
		if (SequencePlayer)
		{
			SequencePlayer->State.Invalidate(Binding.GetGuid(), Binding.GetSequenceID());
		}
	}

	/** Resets all overridden bindings back to the defaults defined by the Level Sequence asset */
	UFUNCTION(BlueprintCallable, Category="Game|Cinematic|Bindings")
	void ResetBindings()
	{
		BindingOverrides->ResetBindings();
		if (SequencePlayer)
		{
			SequencePlayer->State.ClearObjectCaches(*SequencePlayer);
		}
	}

public:

	virtual void PostInitializeComponents() override;
	virtual void Tick(float DeltaSeconds) override;
	virtual void PostLoad() override;

protected:
	virtual void BeginPlay() override;

public:

#if WITH_EDITOR
	virtual bool GetReferencedContentObjects(TArray<UObject*>& Objects) const override;
#endif //WITH_EDITOR

	void InitializePlayer();

	void OnSequenceLoaded(const FName& PackageName, UPackage* Package, EAsyncLoadingResult::Type Result, bool bInitializePlayer);

#if WITH_EDITOR
	virtual TSharedPtr<FStructOnScope> GetObjectPickerProxy(TSharedPtr<IPropertyHandle> PropertyHandle) override;
	virtual void UpdateObjectFromProxy(FStructOnScope& Proxy, IPropertyHandle& ObjectPropertyHandle) override;
	virtual UMovieSceneSequence* RetrieveOwnedSequence() const override
	{
		return GetSequence(true);
	}
#endif

private:
	/** Burn-in widget */
	UPROPERTY()
	ULevelSequenceBurnIn* BurnInInstance;
};


USTRUCT()
struct FBoundActorProxy
{
	GENERATED_BODY()

#if WITH_EDITORONLY_DATA

	/** Specifies the actor to override the binding with */
	UPROPERTY(EditInstanceOnly, AdvancedDisplay, Category="General")
	AActor* BoundActor;

	void Initialize(TSharedPtr<IPropertyHandle> InPropertyHandle);

	void OnReflectedPropertyChanged();

	TSharedPtr<IPropertyHandle> ReflectedProperty;

#endif
};