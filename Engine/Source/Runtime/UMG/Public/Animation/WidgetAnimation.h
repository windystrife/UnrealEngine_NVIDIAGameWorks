// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/ScriptMacros.h"
#include "MovieSceneSequence.h"
#include "Animation/WidgetAnimationBinding.h"
#include "WidgetAnimation.generated.h"

class UMovieScene;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnWidgetAnimationPlaybackStatusChanged);

/**
 * 
 */
UCLASS(BlueprintType, MinimalAPI, DefaultToInstanced)
class UWidgetAnimation : public UMovieSceneSequence
{
	GENERATED_UCLASS_BODY()

public:

#if WITH_EDITOR
	/**
	 * Get a placeholder animation.
	 *
	 * @return Placeholder animation.
	 */
	static UMG_API UWidgetAnimation* GetNullAnimation();
#endif

	/**
	 * Get the start time of this animation.
	 *
	 * @return Start time in seconds.
	 * @see GetEndTime
	 */
	UFUNCTION(BlueprintCallable, Category="Animation")
	UMG_API float GetStartTime() const;

	/**
	 * Get the end time of this animation.
	 *
	 * @return End time in seconds.
	 * @see GetStartTime
	 */
	UFUNCTION(BlueprintCallable, Category="Animation")
	UMG_API float GetEndTime() const;

	/** Fires when the widget animation starts playing. */
	UPROPERTY(BlueprintAssignable, Category="Animation")
	FOnWidgetAnimationPlaybackStatusChanged OnAnimationStarted;

	/** Fires when the widget animation is finished. */
	UPROPERTY(BlueprintAssignable, Category="Animation")
	FOnWidgetAnimationPlaybackStatusChanged OnAnimationFinished;

public:

	// UMovieSceneAnimation overrides
	virtual void BindPossessableObject(const FGuid& ObjectId, UObject& PossessedObject, UObject* Context) override;
	virtual bool CanPossessObject(UObject& Object, UObject* InPlaybackContext) const override;
	virtual UMovieScene* GetMovieScene() const override;
	virtual UObject* GetParentObject(UObject* Object) const override;
	virtual void UnbindPossessableObjects(const FGuid& ObjectId) override;
	virtual void LocateBoundObjects(const FGuid& ObjectId, UObject* Context, TArray<UObject*, TInlineAllocator<1>>& OutObjects) const override;
	// ~UMovieSceneAnimation overrides

	/** Get Animation bindings of the animation */
	const TArray<FWidgetAnimationBinding>& GetBindings() const { return AnimationBindings; }

public:

	/** Pointer to the movie scene that controls this animation. */
	UPROPERTY()
	UMovieScene* MovieScene;

	/**  */
	UPROPERTY()
	TArray<FWidgetAnimationBinding> AnimationBindings;
};
