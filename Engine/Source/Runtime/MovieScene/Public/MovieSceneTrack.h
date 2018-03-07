// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/EnumClassFlags.h"
#include "UObject/ObjectMacros.h"
#include "IMovieSceneTrackInstance.h"
#include "Misc/Guid.h"
#include "MovieSceneSignedObject.h"
#include "MovieSceneSection.h"
#include "Misc/InlineValue.h"
#include "MovieSceneTrack.generated.h"

struct FMovieSceneEvaluationTrack;
struct FMovieSceneSegmentCompilerRules;
struct FMovieSceneSequenceTemplateStore;
struct IMovieSceneTemplateGenerator;

/** Flags used to perform cook-time optimization of movie scene data */
enum class ECookOptimizationFlags
{
	/** Perform no cook optimization */
	None 			= 0,
	/** Remove this track since its of no consequence to runtime */
	RemoveTrack		= 1 << 0,
	/** Remove this track's object since its of no consequence to runtime */
	RemoveObject	= 1 << 1,
};
ENUM_CLASS_FLAGS(ECookOptimizationFlags)

/** Movie scene compilation parameters. Serialized items contribute to a compiled template's cached hash */
USTRUCT()
struct FMovieSceneTrackCompilationParams
{
	GENERATED_BODY()

	FMovieSceneTrackCompilationParams()
		: bForEditorPreview(false)
		, bDuringBlueprintCompile(false)
	{
	}

	friend bool operator!=(const FMovieSceneTrackCompilationParams& A, const FMovieSceneTrackCompilationParams& B)
	{
		return !(A == B);
	}

	friend bool operator==(const FMovieSceneTrackCompilationParams& A, const FMovieSceneTrackCompilationParams& B)
	{
		return A.bForEditorPreview == B.bForEditorPreview && A.bDuringBlueprintCompile == B.bDuringBlueprintCompile;
	}

	/** Whether we're generating for an editor preview, or for efficient runtime evaluation */
	UPROPERTY()
	bool bForEditorPreview;

	/** Whether we're generating during a blueprint compile. As such, UObject types may not have been fully loaded. */
	UPROPERTY()
	bool bDuringBlueprintCompile;
};

/** Track compiler arguments */
struct FMovieSceneTrackCompilerArgs
{
	FMovieSceneTrackCompilerArgs(IMovieSceneTemplateGenerator& InGenerator, FMovieSceneSequenceTemplateStore& InSubSequenceStore)
		: Generator(InGenerator)
		, SubSequenceStore(InSubSequenceStore)
	{
	}

	/** Compilation parameters */
	FMovieSceneTrackCompilationParams Params;

	/** The object binding ID that this track belongs to. */
	FGuid ObjectBindingId;

	/** The generator responsible for generating the template */
	IMovieSceneTemplateGenerator& Generator;

	/** Store that describes how to find sub sequence templates */
	FMovieSceneSequenceTemplateStore& SubSequenceStore;
};

/** Generic evaluation options for any track */
USTRUCT()
struct FMovieSceneTrackEvalOptions
{
	GENERATED_BODY()
	
	FMovieSceneTrackEvalOptions()
		: bCanEvaluateNearestSection(false)
		, bEvalNearestSection(false)
		, bEvaluateInPreroll(false)
		, bEvaluateInPostroll(false)
		, bEvaluateNearestSection_DEPRECATED(false)
	{}

	/** true when the value of bEvalNearestSection is to be considered for the track */
	UPROPERTY()
	uint32 bCanEvaluateNearestSection : 1;

	/** When evaluating empty space on a track, will evaluate the last position of the previous section (if possible), or the first position of the next section, in that order of preference. */
	UPROPERTY(EditAnywhere, Category="General", DisplayName="Evaluate Nearest Section", meta=(EditCondition=bCanEvaluateNearestSection))
	uint32 bEvalNearestSection : 1;

	/** Evaluate this track as part of its parent sub-section's pre-roll, if applicable */
	UPROPERTY(EditAnywhere, Category="General")
	uint32 bEvaluateInPreroll : 1;

	/** Evaluate this track as part of its parent sub-section's post-roll, if applicable */
	UPROPERTY(EditAnywhere, Category="General")
	uint32 bEvaluateInPostroll : 1;

	UPROPERTY()
	uint32 bEvaluateNearestSection_DEPRECATED : 1;
};

/** Enumeration specifying the result of a compilation */
enum class EMovieSceneCompileResult : uint8
{
	/** The compilation was successful */
	Success,
	/** The compilation was not successful */
	Failure,
	/** No compilation routine was implemented */
	Unimplemented
};

/**
 * Base class for a track in a Movie Scene
 */
UCLASS(abstract, DefaultToInstanced, MinimalAPI)
class UMovieSceneTrack
	: public UMovieSceneSignedObject
{
	GENERATED_BODY()

public:

	MOVIESCENE_API UMovieSceneTrack(const FObjectInitializer& InInitializer);

public:

	/** General evaluation options for a given track */
	UPROPERTY(EditAnywhere, Category="General", meta=(ShowOnlyInnerProperties))
	FMovieSceneTrackEvalOptions EvalOptions;

	/**
	 * Gets what kind of blending is supported by this section
	 */
	FMovieSceneBlendTypeField GetSupportedBlendTypes() const
	{
		return SupportedBlendTypes;
	}

	/**
	 * Update all auto-generated easing curves for all sections in this track
	 */
	MOVIESCENE_API void UpdateEasing();

public:

	//~ Methods relating to compilation 

	/** 
	 * Get compiler rules to use when compiling sections that overlap on the same row.
	 * These define how to deal with overlapping sections and empty space on a row
	 */
	MOVIESCENE_API virtual TInlineValue<FMovieSceneSegmentCompilerRules> GetRowCompilerRules() const;

	/** 
	 * Get compiler rules to use when compiling sections that overlap on different rows.
	 * These define how to deal with overlapping sections and empty space at the track level
	 */
	MOVIESCENE_API virtual TInlineValue<FMovieSceneSegmentCompilerRules> GetTrackCompilerRules() const;

	/**
	 * Generate a template for this track
	 *
	 * @param Args 			Compilation arguments
	 */
	MOVIESCENE_API virtual void GenerateTemplate(const FMovieSceneTrackCompilerArgs& Args) const;

	/**
	 * Get a raw compiled copy of this track with no additional shared tracks or compiler parameters
	 */
	MOVIESCENE_API FMovieSceneEvaluationTrack GenerateTrackTemplate() const;

protected:

	/**
	 * Overridable user defined custom compilation method
	 *
	 * @param Track 		Destination track to compile into
	 * @param Args 			Compilation arguments
	 * @return Compilation result
	 */
	virtual EMovieSceneCompileResult CustomCompile(FMovieSceneEvaluationTrack& Track, const FMovieSceneTrackCompilerArgs& Args) const { return EMovieSceneCompileResult::Unimplemented; }

	/**
	 * Called after this track has been compiled, regardless of whether it was compiled through CustomCompile, or the default logic
	 *
	 * @param Track 		Destination track to compile into
	 * @param Args 			Compilation arguments
	 */
	virtual void PostCompile(FMovieSceneEvaluationTrack& Track, const FMovieSceneTrackCompilerArgs& Args) const {}

public:

	/**
	 * Internal function to create a legacy track instance
	 */
	TSharedPtr<class IMovieSceneTrackInstance> CreateLegacyInstance() const;

protected:

	/**
	 * Create a movie scene eval template for the specified section
	 *
	 * @param InSection		The section to create a template for
	 * @return A template, or null
	 */
	MOVIESCENE_API virtual FMovieSceneEvalTemplatePtr CreateTemplateForSection(const UMovieSceneSection& InSection) const;

	/**
	 * Compile this movie scene track into an efficient runtime structure
	 *
	 * @param Track 		Destination track to compile into
	 * @param Args 			Compilation arguments
	 * @return Compilation result
	 */
	MOVIESCENE_API EMovieSceneCompileResult Compile(FMovieSceneEvaluationTrack& Track, const FMovieSceneTrackCompilerArgs& Args) const;

	/**
	 * Deprecated method of creating track instances
	 */
	DEPRECATED(4.15, "Create Instance has been deprecated. Please provide an evaluation template through CreateTemplateForSection instead.")
	virtual TSharedPtr<class IMovieSceneTrackInstance> CreateInstance() { return nullptr; }

protected:

	//~ UObject interface
	MOVIESCENE_API virtual void PostInitProperties() override;
	MOVIESCENE_API virtual void PostLoad() override;

	/** Intentionally not a UPROPERTY so this isn't serialized */
	FMovieSceneBlendTypeField SupportedBlendTypes;

public:

	/**
	 * @return The name that makes this track unique from other track of the same class.
	 */
	virtual FName GetTrackName() const { return NAME_None; }

	/**
	 * @return Whether or not this track has any data in it.
	 */
	virtual bool IsEmpty() const PURE_VIRTUAL(UMovieSceneTrack::IsEmpty, return true;);

	/**
	 * Removes animation data.
	 */
	virtual void RemoveAllAnimationData() { }

	/**
	 * @return Whether or not this track supports multiple row indices.
	 */
	virtual bool SupportsMultipleRows() const
	{
		return SupportedBlendTypes.Num() != 0;
	}

	/** Gets the greatest row index of all the sections owned by this track. */
	MOVIESCENE_API int32 GetMaxRowIndex() const;

	/**
	 * Updates the row indices of sections owned by this track so that all row indices which are used are consecutive with no gaps. 
	 * @return Whether or not fixes were made. 
	 */
	MOVIESCENE_API bool FixRowIndices();

	/**
	 * @return Whether or not this track's section bounds should be added to the play range
	 */
	virtual bool AddsSectionBoundsToPlayRange() const
	{
		return false;
	}

public:

	/**
	 * Add a section to this track.
	 *
	 * @param Section The section to add.
	 */
	virtual void AddSection(UMovieSceneSection& Section) PURE_VIRTUAL(UMovieSceneSection::AddSection,);

	/**
	 * Generates a new section suitable for use with this track.
	 *
	 * @return a new section suitable for use with this track.
	 */
	virtual class UMovieSceneSection* CreateNewSection() PURE_VIRTUAL(UMovieSceneTrack::CreateNewSection, return nullptr;);
	
	/**
	 * Called when all the sections of the track need to be retrieved.
	 * 
	 * @return List of all the sections in the track.
	 */
	virtual const TArray<UMovieSceneSection*>& GetAllSections() const PURE_VIRTUAL(UMovieSceneTrack::GetAllSections, static TArray<UMovieSceneSection*> Empty; return Empty;);

	/**
	 * Gets the section boundaries of this track.
	 * 
	 * @return The range of time boundaries.
	 */
	virtual TRange<float> GetSectionBoundaries() const PURE_VIRTUAL(UMovieSceneTrack::GetSectionBoundaries, return TRange<float>::Empty(););

	/**
	 * Checks to see if the section is in this track.
	 *
	 * @param Section The section to query for.
	 * @return True if the section is in this track.
	 */
	virtual bool HasSection(const UMovieSceneSection& Section) const PURE_VIRTUAL(UMovieSceneSection::HasSection, return false;);

	/**
	 * Removes a section from this track.
	 *
	 * @param Section The section to remove.
	 */
	virtual void RemoveSection(UMovieSceneSection& Section) PURE_VIRTUAL(UMovieSceneSection::RemoveSection,);

#if WITH_EDITOR

	/**
	 * Called when this track's movie scene is being cooked to determine if/how this track should be cooked.
	 * @return ECookOptimizationFlags detailing how to optimize this track
	 */
	virtual ECookOptimizationFlags GetCookOptimizationFlags() const { return ECookOptimizationFlags::None; }

#endif

#if WITH_EDITORONLY_DATA

	/**
	 * Get the track's display name.
	 *
	 * @return Display name text.
	 */
	virtual FText GetDisplayName() const PURE_VIRTUAL(UMovieSceneTrack::GetDisplayName, return FText::FromString(TEXT("Unnamed Track")););

	/**
	 * Get this track's color tint.
	 *
	 * @return Color Tint.
	 */
	const FColor& GetColorTint() const
	{
		return TrackTint;
	}

	/**
	 * Set this track's color tint.
	 *
	 * @param InTrackTint The color to tint this track.
	 */
	void SetColorTint(const FColor& InTrackTint)
	{
		TrackTint = InTrackTint;
	}

protected:

	/** This track's tint color */
	UPROPERTY(EditAnywhere, Category=General, DisplayName=Color)
	FColor TrackTint;

public:
#endif

#if WITH_EDITOR
	/**
	 * Called if the section is moved in Sequencer.
	 *
	 * @param Section The section that moved.
	 */
	virtual void OnSectionMoved(UMovieSceneSection& Section) { }
#endif
};
