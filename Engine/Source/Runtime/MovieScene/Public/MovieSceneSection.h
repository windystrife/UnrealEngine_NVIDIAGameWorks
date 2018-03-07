// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "MovieSceneFwd.h"
#include "KeyParams.h"
#include "Curves/KeyHandle.h"
#include "Curves/RichCurve.h"
#include "MovieSceneSignedObject.h"
#include "MovieSceneBlendType.h"
#include "Generators/MovieSceneEasingFunction.h"
#include "MovieSceneSection.generated.h"

class FStructOnScope;
class UAISenseEvent;
struct FMovieSceneEvalTemplatePtr;

/** Enumeration specifying how to handle state when this section is no longer evaluated */
UENUM()
enum class EMovieSceneCompletionMode : uint8
{
	KeepState,

	RestoreState,
};


USTRUCT()
struct FMovieSceneSectionEvalOptions
{
	GENERATED_BODY()
	
	FMovieSceneSectionEvalOptions()
		: bCanEditCompletionMode(false)
		, CompletionMode(EMovieSceneCompletionMode::KeepState)
	{}

	void EnableAndSetCompletionMode(EMovieSceneCompletionMode NewCompletionMode)
	{
		bCanEditCompletionMode = true;
		CompletionMode = NewCompletionMode;
	}

	UPROPERTY()
	bool bCanEditCompletionMode;

	/** When set to "RestoreState", this section will restore any animation back to its previous state  */
	UPROPERTY(EditAnywhere, DisplayName="When Finished", Category="Section")
	EMovieSceneCompletionMode CompletionMode;
};

USTRUCT()
struct FMovieSceneEasingSettings
{
	GENERATED_BODY()

	FMovieSceneEasingSettings()
		: AutoEaseInTime(0.f), AutoEaseOutTime(0.f)
		, EaseIn(nullptr), bManualEaseIn(false), ManualEaseInTime(0.f)
		, EaseOut(nullptr), bManualEaseOut(false), ManualEaseOutTime(0.f)
	{}

public:

	float GetEaseInTime() const
	{
		return bManualEaseIn ? ManualEaseInTime : AutoEaseInTime;
	}

	float GetEaseOutTime() const
	{
		return bManualEaseOut ? ManualEaseOutTime : AutoEaseOutTime;
	}

public:

	/** Automatically applied ease in time */
	UPROPERTY()
	float AutoEaseInTime;

	/** Automatically applied ease out time */
	UPROPERTY()
	float AutoEaseOutTime;

	UPROPERTY()
	TScriptInterface<IMovieSceneEasingFunction> EaseIn;

	/** Whether to manually override this section's ease in time */
	UPROPERTY()
	bool bManualEaseIn;

	/** Manually override this section's ease in time */
	UPROPERTY()
	float ManualEaseInTime;

	UPROPERTY()
	TScriptInterface<IMovieSceneEasingFunction> EaseOut;

	/** Whether to manually override this section's ease out time */
	UPROPERTY()
	bool bManualEaseOut;

	/** Manually override this section's ease-out time */
	UPROPERTY()
	float ManualEaseOutTime;
};

/**
 * Base class for movie scene sections
 */
UCLASS(abstract, DefaultToInstanced, MinimalAPI)
class UMovieSceneSection
	: public UMovieSceneSignedObject
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(EditAnywhere, Category="Section", meta=(ShowOnlyInnerProperties))
	FMovieSceneSectionEvalOptions EvalOptions;

public:

	/**
	 * Calls Modify if this section can be modified, i.e. can't be modified if it's locked
	 *
	 * @return Returns whether this section is locked or not
	 */
	virtual MOVIESCENE_API bool TryModify(bool bAlwaysMarkDirty=true);

	/**
	 * @return The start time of the section
	 */
	float GetStartTime() const
	{
		return StartTime;
	}

	/**
	 * @return The end time of the section
	 */
	float GetEndTime() const
	{
		return EndTime;
	}
	
	/**
	 * @return The size of the time range of the section
	 */
	float GetTimeSize() const
	{
		return EndTime - StartTime;
	}

	/**
	 * Sets a new end time for this section
	 * 
	 * @param InEndTime	The new end time
	 */
	void SetStartTime(float NewStartTime)
	{ 
		if (TryModify())
		{
			StartTime = NewStartTime;
		}
	}

	/**
	 * Sets a new end time for this section
	 * 
	 * @param InEndTime	The new end time
	 */
	void SetEndTime(float NewEndTime)
	{ 
		if (TryModify())
		{
			EndTime = NewEndTime;
		}
	}
	
	/**
	 * @return The range of times of the section
	 */
	TRange<float> GetRange() const 
	{
		// Use the single value constructor for zero sized ranges because it creates a range that is inclusive on both upper and lower
		// bounds which isn't considered "empty".  Use the standard constructor for non-zero sized ranges so that they work well when
		// calculating overlap with other non-zero sized ranges.
		return (StartTime == EndTime)
			? TRange<float>(StartTime)
			: TRange<float>(StartTime, TRangeBound<float>::Inclusive(EndTime));
	}
	
	/**
	 * Sets a new range of times for this section
	 * 
	 * @param NewRange	The new range of times
	 */
	void SetRange(TRange<float> NewRange)
	{
		check(NewRange.HasLowerBound() && NewRange.HasUpperBound());

		if (TryModify())
		{
			StartTime = NewRange.GetLowerBoundValue();
			EndTime = NewRange.GetUpperBoundValue();
		}
	}

	/**
	 * Returns whether or not a provided position in time is within the timespan of the section 
	 *
	 * @param Position	The position to check
	 * @return true if the position is within the timespan, false otherwise
	 */
	bool IsTimeWithinSection(float Position) const 
	{
		return Position >= StartTime && Position <= EndTime;
	}

	/**
	 * Gets this section's blend type
	 */
	FOptionalMovieSceneBlendType GetBlendType() const
	{
		return BlendType;
	}

	/**
	 * Sets this section's blend type
	 */
	void SetBlendType(EMovieSceneBlendType InBlendType)
	{
		if (GetSupportedBlendTypes().Contains(InBlendType))
		{
			BlendType = InBlendType;
		}
	}

	/**
	 * Gets what kind of blending is supported by this section
	 */
	MOVIESCENE_API FMovieSceneBlendTypeField GetSupportedBlendTypes() const;

	/**
	 * Moves the section by a specific amount of time
	 *
	 * @param DeltaTime	The distance in time to move the curve
	 * @param KeyHandles The key handles to operate on
	 */
	virtual void MoveSection(float DeltaTime, TSet<FKeyHandle>& KeyHandles)
	{
		if (TryModify())
		{
			StartTime += DeltaTime;
			EndTime += DeltaTime;
		}
	}
	
	/**
	 * Dilates the section by a specific factor
	 *
	 * @param DilationFactor The multiplier which scales this section
	 * @param bFromStart Whether to dilate from the beginning or end (whichever stays put)
	 * @param KeyHandles The key handles to operate on
	 */
	virtual void DilateSection(float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles)
	{
		if (TryModify())
		{
			StartTime = (StartTime - Origin) * DilationFactor + Origin;
			EndTime = (EndTime - Origin) * DilationFactor + Origin;
		}
	}
	
	/**
	 * Split a section in two at the split time
	 *
	 * @param SplitTime The time at which to split
	 * @return The newly created split section
	 */
	virtual MOVIESCENE_API UMovieSceneSection* SplitSection(float SplitTime);

	/**
	 * Trim a section at the trim time
	 *
	 * @param TrimTime The time at which to trim
	 * @param bTrimLeft Whether to trim left or right
	 */
	virtual MOVIESCENE_API void TrimSection(float TrimTime, bool bTrimLeft);

	/**
	 * Get the key handles for the keys on the curves within this section
	 *
	 * @param OutKeyHandles Will contain the key handles of the keys on the curves within this section
	 * @param TimeRange Optional time range that the keys must be in (default = all)
	 */
	virtual void GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const { }

	/**
	 * Get the data structure representing the specified key.
	 *
	 * @param KeyHandle The handle of the key.
	 * @return The key's data structure representation, or nullptr if key not found or no structure available.
	 */
	virtual TSharedPtr<FStructOnScope> GetKeyStruct(const TArray<FKeyHandle>& KeyHandles)
	{
		return nullptr;
	}

	/**
	 * Generate an evaluation template for this section
	 * @return a valid evaluation template ptr, or nullptr
	 */
	MOVIESCENE_API virtual FMovieSceneEvalTemplatePtr GenerateTemplate() const;

	/**
	 * Gets all snap times for this section
	 *
	 * @param OutSnapTimes The array of times we will to output
	 * @param bGetSectionBorders Gets the section borders in addition to any custom snap times
	 */
	virtual void GetSnapTimes(TArray<float>& OutSnapTimes, bool bGetSectionBorders) const
	{
		if (bGetSectionBorders)
		{
			OutSnapTimes.Add(StartTime);
			OutSnapTimes.Add(EndTime);
		}
	}

	virtual void GetAllKeyTimes(TArray<float>& OutKeyTimes) const {}

	/** Sets this section's new row index */
	void SetRowIndex(int32 NewRowIndex) {RowIndex = NewRowIndex;}

	/** Gets the row index for this section */
	int32 GetRowIndex() const {return RowIndex;}
	
	/** Sets this section's priority over overlapping sections (higher wins) */
	void SetOverlapPriority(int32 NewPriority)
	{
		OverlapPriority = NewPriority;
	}

	/** Gets this section's priority over overlapping sections (higher wins) */
	int32 GetOverlapPriority() const
	{
		return OverlapPriority;
	}

	/**
	 * Adds a key to a rich curve, finding an existing key to modify or adding a new one.
	 *
	 * @param InCurve The curve to add keys to.
	 * @param Time The time where the key should be added.
	 * @param Value The value at the given time.
	 * @param Interpolation The key interpolation to use.
	 * @param bUnwindRotation Unwind rotation.
	 */
	void MOVIESCENE_API AddKeyToCurve(FRichCurve& InCurve, float Time, float Value, EMovieSceneKeyInterpolation Interpolation, const bool bUnwindRotation = false);

	/**
	 * Sets the default value for a curve.
	 *
	 * @param InCurve The curve to set a default value on.
	 * @param Value The value to use as the default.
	 */
	void MOVIESCENE_API SetCurveDefault(FRichCurve& InCurve, float Value);

	/**
	 * Checks to see if this section overlaps with an array of other sections
	 * given an optional time and track delta.
	 *
	 * @param Sections Section array to check against.
	 * @param TrackDelta Optional offset to this section's track index.
	 * @param TimeDelta Optional offset to this section's time delta.
	 * @return The first section that overlaps, or null if there is no overlap.
	 */
	virtual MOVIESCENE_API const UMovieSceneSection* OverlapsWithSections(const TArray<UMovieSceneSection*>& Sections, int32 TrackDelta = 0, float TimeDelta = 0.f) const;
	
	/**
	 * Places this section at the first valid row at the specified time. Good for placement upon creation.
	 *
	 * @param Sections Sections that we can not overlap with.
	 * @param InStartTime The new start time.
	 * @param InEndTime The new end time.
	 * @param bAllowMultipleRows If false, it will move the section in the time direction to make it fit, rather than the row direction.
	 */
	virtual MOVIESCENE_API void InitialPlacement(const TArray<UMovieSceneSection*>& Sections, float InStartTime, float InEndTime, bool bAllowMultipleRows);

	/** Whether or not this section is active. */
	void SetIsActive(bool bInIsActive) { bIsActive = bInIsActive; }
	bool IsActive() const { return bIsActive; }

	/** Whether or not this section is locked. */
	void SetIsLocked(bool bInIsLocked) { bIsLocked = bInIsLocked; }
	bool IsLocked() const { return bIsLocked; }

	/** Whether or not this section is infinite. An infinite section will draw the entire width of the track. StartTime and EndTime will be ignored but not discarded. */
	void SetIsInfinite(bool bInIsInfinite) { bIsInfinite = bInIsInfinite; }
	bool IsInfinite() const { return bIsInfinite; }

	/** Gets the amount of time to prepare this section for evaluation before it actually starts. */
	void SetPreRollTime(float InPreRollTime) { PreRollTime = InPreRollTime; }
	float GetPreRollTime() const { return PreRollTime; }

	/** Gets/sets the amount of time to continue 'postrolling' this section for after evaluation has ended. */
	void SetPostRollTime(float InPostRollTime) { PostRollTime = InPostRollTime; }
	float GetPostRollTime() const { return PostRollTime; }

	/** The optional offset time of this section */
	virtual TOptional<float> GetOffsetTime() const { return TOptional<float>(); }

	/** Gets the time for the key referenced by the supplied key handle. */
	virtual TOptional<float> GetKeyTime( FKeyHandle KeyHandle ) const PURE_VIRTUAL( UAISenseEvent::GetKeyTime, return TOptional<float>(); );

	/** Sets the time for the key referenced by the supplied key handle. */
	virtual void SetKeyTime( FKeyHandle KeyHandle, float Time ) PURE_VIRTUAL( UAISenseEvent::SetKeyTime, );
	/**
	 * When guid bindings are updated to allow this section to fix-up any internal bindings
	 *
	 */
	virtual void OnBindingsUpdated(const TMap<FGuid, FGuid>& OldGuidToNewGuidMap) { }

	/**
	 * Gets a list of all overlapping sections
	 */
	MOVIESCENE_API void GetOverlappingSections(TArray<UMovieSceneSection*>& OutSections, bool bSameRow, bool bIncludeThis);

	/**
	 * Evaluate this sections's easing functions based on the specified time
	 */
	MOVIESCENE_API float EvaluateEasing(float InTime) const;

	/**
	 * Evaluate this sections's easing functions based on the specified time
	 */
	MOVIESCENE_API void EvaluateEasing(float InTime, TOptional<float>& OutEaseInValue, TOptional<float>& OutEaseOutValue, float* OutEaseInInterp, float* OutEaseOutInterp) const;

	MOVIESCENE_API TRange<float> GetEaseInRange() const;

	MOVIESCENE_API TRange<float> GetEaseOutRange() const;

protected:

	//~ UObject interface
	MOVIESCENE_API virtual void PostInitProperties() override;

public:

	UPROPERTY(EditAnywhere, Category="Easing", meta=(ShowOnlyInnerProperties))
	FMovieSceneEasingSettings Easing;

private:

	/** The start time of the section */
	UPROPERTY(EditAnywhere, Category="Section")
	float StartTime;

	/** The end time of the section */
	UPROPERTY(EditAnywhere, Category="Section")
	float EndTime;

	/** The row index that this section sits on */
	UPROPERTY()
	int32 RowIndex;

	/** This section's priority over overlapping sections */
	UPROPERTY()
	int32 OverlapPriority;

	/** Toggle whether this section is active/inactive */
	UPROPERTY(EditAnywhere, Category="Section")
	uint32 bIsActive : 1;

	/** Toggle whether this section is locked/unlocked */
	UPROPERTY(EditAnywhere, Category="Section")
	uint32 bIsLocked : 1;

	/** Toggle to set this section to be infinite */
	UPROPERTY(EditAnywhere, Category="Section")
	uint32 bIsInfinite : 1;

	/** The amount of time to prepare this section for evaluation before it actually starts. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category="Section", meta=(Units=s, UIMin=0))
	float PreRollTime;

	/** The amount of time to continue 'postrolling' this section for after evaluation has ended. */
	UPROPERTY(EditAnywhere, AdvancedDisplay, Category="Section", meta=(Units=s, UIMin=0))
	float PostRollTime;

protected:

	UPROPERTY()
	FOptionalMovieSceneBlendType BlendType;
};
