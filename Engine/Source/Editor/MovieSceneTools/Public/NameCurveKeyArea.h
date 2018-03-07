// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/WeakObjectPtr.h"
#include "Curves/KeyHandle.h"
#include "Widgets/SWidget.h"
#include "MovieSceneSection.h"
#include "NamedKeyArea.h"

class FMovieSceneClipboardBuilder;
class FMovieSceneClipboardKeyTrack;
class FStructOnScope;
class ISequencer;
struct FMovieSceneClipboardEnvironment;
struct FNameCurve;
struct FSequencerPasteEnvironment;

/**
 * A key area for FName curves.
 */
class MOVIESCENETOOLS_API FNameCurveKeyArea
	: public FNamedKeyArea
{
public:

	/**
	 * Create and initialize a new instance.
	 *
	 * @param InCurve The curve to assign to this key area.
	 * @param InOwningSection The section that owns this key area.
	 */
	FNameCurveKeyArea(FNameCurve& InCurve, UMovieSceneSection* InOwningSection)
		: Curve(InCurve)
		, OwningSection(InOwningSection)
	{ }

public:

	//~ IKeyArea interface

	virtual TArray<FKeyHandle> AddKeyUnique(float Time, EMovieSceneKeyInterpolation InKeyInterpolation, float TimeToCopyFrom = FLT_MAX) override;
	virtual TOptional<FKeyHandle> DuplicateKey(FKeyHandle KeyToDuplicate) override;
	virtual bool CanCreateKeyEditor() override;
	virtual TSharedRef<SWidget> CreateKeyEditor(ISequencer* Sequencer) override;
	virtual void DeleteKey(FKeyHandle KeyHandle) override;
	virtual TOptional<FLinearColor> GetColor() override;
	virtual ERichCurveExtrapolation GetExtrapolationMode(bool bPreInfinity) const override;
	virtual ERichCurveInterpMode GetKeyInterpMode(FKeyHandle Keyhandle) const override;
	virtual TSharedPtr<FStructOnScope> GetKeyStruct(FKeyHandle KeyHandle) override;
	virtual ERichCurveTangentMode GetKeyTangentMode(FKeyHandle KeyHandle) const override;
	virtual float GetKeyTime(FKeyHandle KeyHandle) const override;
	virtual UMovieSceneSection* GetOwningSection() override;
	virtual FRichCurve* GetRichCurve() override;
	virtual TArray<FKeyHandle> GetUnsortedKeyHandles() const override;
	virtual FKeyHandle DilateKey(FKeyHandle KeyHandle, float Scale, float Origin) override;
	virtual FKeyHandle MoveKey(FKeyHandle KeyHandle, float DeltaPosition) override;
	virtual void SetExtrapolationMode(ERichCurveExtrapolation ExtrapMode, bool bPreInfinity) override;
	virtual void SetKeyInterpMode(FKeyHandle KeyHandle, ERichCurveInterpMode InterpMode) override;
	virtual void SetKeyTangentMode(FKeyHandle KeyHandle, ERichCurveTangentMode TangentMode) override;
	virtual void SetKeyTime(FKeyHandle KeyHandle, float NewKeyTime) override;
	virtual void CopyKeys(FMovieSceneClipboardBuilder& ClipboardBuilder, const TFunctionRef<bool(FKeyHandle, const IKeyArea&)>& KeyMask) const override;
	virtual void PasteKeys(const FMovieSceneClipboardKeyTrack& KeyTrack, const FMovieSceneClipboardEnvironment& SrcEnvironment, const FSequencerPasteEnvironment& DstEnvironment) override;

protected:

	/** The curve managed by this area. */
	FNameCurve& Curve;

	/** The section that owns this area. */
	TWeakObjectPtr<UMovieSceneSection> OwningSection;
};
