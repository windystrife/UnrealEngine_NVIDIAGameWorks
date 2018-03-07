// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/SWidget.h"
#include "Curves/KeyHandle.h"
#include "Curves/RichCurve.h"

class FMovieSceneClipboardBuilder;
class FMovieSceneClipboardKeyTrack;
class FStructOnScope;
class ISequencer;
class UMovieSceneSection;
struct FMovieSceneClipboardEnvironment;
struct FSequencerPasteEnvironment;
struct FSlateBrush;
enum class EMovieSceneKeyInterpolation : uint8;

/**
 * Information for how to draw each key.
 */
struct FKeyDrawingInfo
{
	FKeyDrawingInfo()
		: Brush(nullptr)
		, Size(FVector2D::ZeroVector)
	{ }

	/** The brush to use for each key */
	const FSlateBrush* Brush;

	/** The size of each key */
	const FVector2D Size;
};


/**
 * Interface that should be implemented for the UI portion of a key area within a section
 */
class IKeyArea : public TSharedFromThis<IKeyArea>
{
public:

	/** Virtual destructor. */
	virtual ~IKeyArea() { }

	/** @return The array of unsorted key handles in the key area */
	virtual TArray<FKeyHandle> GetUnsortedKeyHandles() const = 0;

	/**
	 * Sets the time of a key given a handle
	 *
	 * @param KeyHandle Handle of the key
	 * @param The new time of the key
	 */
	virtual void SetKeyTime(FKeyHandle KeyHandle, float NewKeyTime) = 0;

	/**
	 * Gets the time of a key given a handle
	 *
	 * @param KeyHandle Handle of the key
	 * @return The time of the key
	 */
	virtual float GetKeyTime(FKeyHandle KeyHandle) const = 0;

	/**
	 * Dilates the key by a specific factor
	 *
	 * @param Scale The multiplier which scales this key
	 * @param Origin Time to scale from
	 * @return The handle of the key
	 */
	virtual FKeyHandle DilateKey(FKeyHandle KeyHandle, float Scale, float Origin) = 0;

	/**
	 * Moves a key
	 * 
	 * @param KeyHandle		Handle of the key to move
	 * @param DeltaPosition	The delta position of the key
	 */
	virtual FKeyHandle MoveKey(FKeyHandle KeyHandle, float DeltaPosition) = 0;
		
	/**
	 * Deletes a key
	 * 
	 * @param KeyHandle The key to delete
	 */
	virtual void DeleteKey(FKeyHandle KeyHandle) = 0;

	/**
	 * Set key interpolation
	 *
	 * @param KeyHandle The key handle
	 * @param InterpMode The interpolation mode
	 */
	virtual void SetKeyInterpMode(FKeyHandle KeyHandle, ERichCurveInterpMode InterpMode) = 0;

	/**
	 * Get key interpolation
	 *
	 * @param KeyHandle The key handle
	 * @return InterpMode The interpolation mode
	 */
	virtual ERichCurveInterpMode GetKeyInterpMode(FKeyHandle Keyhandle) const = 0;

	/**
	 * Set key tangent
	 *
	 * @param KeyHandle The key handle
	 * @param TangentMode The tangent mode
	 */
	virtual void SetKeyTangentMode(FKeyHandle KeyHandle, ERichCurveTangentMode TangentMode) = 0;

	/**
	 * Get key tangent
	 *
	 * @param KeyHandle The key handle
	 * @return TangentMode The tangent mode
	 */
	virtual ERichCurveTangentMode GetKeyTangentMode(FKeyHandle KeyHandle) const = 0;

	/**
	 * Set extrapolation
	 *
	 * @param ExtrapMode The extrapolation mode
	 * @param bPreInfinity Set pre-infinity (or post-infinity)
	 */
	virtual void SetExtrapolationMode(ERichCurveExtrapolation ExtrapMode, bool bPreInfinity) = 0;

	/**
	 * Get extrapolation
	 *
	 * @param bPreInfinity Get pre-infinity (or post-infinity)
	 * @return ExtrapMode The extrapolation mode
	 */
	virtual ERichCurveExtrapolation GetExtrapolationMode(bool bPreInfinity) const = 0;

	/**
	 * @return Whether this key area can set extrapolation mode
	 */
	virtual bool CanSetExtrapolationMode() const { return false; }

	/**
	 * Adds a key at the specified time if there isn't already a key present.  The value of the added key should
	 * be the value which would be returned if the animation containing this key area was evaluated at the specified time.
	 *
	 * @param InKeyInterpolation KeyInterpolation
	 * @param TimeToCopyFrom Optional time to copy key parameters from
	 * @param return      The new keys that were added
	 */
	virtual TArray<FKeyHandle> AddKeyUnique(float Time, EMovieSceneKeyInterpolation InKeyInterpolation, float TimeToCopyFrom = FLT_MAX) = 0;

	/**
	 * Duplicate the specified key
	 */
	virtual TOptional<FKeyHandle> DuplicateKey(FKeyHandle KeyToDuplicate) = 0;

	/** Gets the rich curve associated with this key area.  This can be null but it must be valid in order to be
	  * edited by the curve editor. */
	virtual FRichCurve* GetRichCurve() = 0;

	/**
	 * Gets the section which owns this key area.
	 */
	virtual UMovieSceneSection* GetOwningSection() = 0;

	/**
	 * Returns true if this key area can create a key editor widget for the animation outliner.
	 */
	virtual bool CanCreateKeyEditor() = 0;

	/**
	 * Creates a key editor for this key area for use in the animation outliner.
	 */
	virtual TSharedRef<SWidget> CreateKeyEditor(ISequencer* Sequencer) = 0;

	/**
	 * Copy keys from this key area
	 *
	 * @param ClipboardBuilder	Clipboard builder to add keys to
	 * @param KeyMask			Predicate to be called to check whether a key should be copied
	 */
	virtual void CopyKeys(FMovieSceneClipboardBuilder& ClipboardBuilder, const TFunctionRef<bool(FKeyHandle, const IKeyArea&)>& KeyMask) const = 0;

	/**
	 * Paste keys into this key area
	 *
	 * @param KeyTrack			Container of keys to paste into this area
	 * @param SrcEnvironment		The source environment the cliboard originated from
	 * @param DstEnvironment		The new environment to paste in
	 */
	virtual void PasteKeys(const FMovieSceneClipboardKeyTrack& KeyTrack, const FMovieSceneClipboardEnvironment& SrcEnvironment, const FSequencerPasteEnvironment& DstEnvironment) = 0;

	/**
	 * Give this key area a specific name
	 */
	virtual void SetName(FName Name) = 0;

	/**
	 * Get this area's name, or None if not set
	 */
	virtual FName GetName() const = 0;

	/**
	 * Get the key area's color.
	 *
	 * @return Key area color.
	 */
	virtual TOptional<FLinearColor> GetColor() = 0;

	/**
	 * Get the data structure representing the specified key.
	 *
	 * @param KeyHandle The handle of the key.
	 * @return The key's data structure representation, or nullptr if key not found or no structure available.
	 */
	virtual TSharedPtr<FStructOnScope> GetKeyStruct(FKeyHandle KeyHandle) = 0;
};
