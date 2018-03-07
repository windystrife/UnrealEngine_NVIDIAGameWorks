// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Attribute.h"
#include "Curves/KeyHandle.h"
#include "Widgets/SWidget.h"
#include "NamedKeyArea.h"

class FMovieSceneClipboardBuilder;
class FMovieSceneClipboardKeyTrack;
class FStructOnScope;
class ISequencer;
class UMovieSceneSection;
struct FMovieSceneClipboardEnvironment;
struct FSequencerPasteEnvironment;
struct FStringCurve;
enum class EMovieSceneKeyInterpolation : uint8;

/**
 * A key area for string keys.
 */
class MOVIESCENETOOLS_API FStringCurveKeyArea
	: public FNamedKeyArea
{
public:

	/**
	* Creates a new key area for editing string curves.
	*
	* @param InCurve The string curve which has the string keys.
	* @param InOwningSection The section which owns the curve which is being displayed and edited by this area.
	* @param InColor An optional color which is used to draw the background of this key area.
	*/
	FStringCurveKeyArea(FStringCurve* InCurve, UMovieSceneSection* InOwningSection, TOptional<FLinearColor> InColor = TOptional<FLinearColor>())
		: Color(InColor)
		, Curve(InCurve)
		, OwningSection(InOwningSection)
	{ }

	/**
	* Creates a new key area for editing string curves whose value can be overridden externally.
	*
	* @param InCurve The string curve which has the string keys.
	* @param InExternalValue An attribute which can provide an external value for this key area.  External values are
	*        useful for things like property tracks where the property value can change without changing the animation
	*        and we want to be able to key and update using the new property value.
	* @param InOwningSection The section which owns the curve which is being displayed and edited by this area.
	* @param InColor An optional color which is used to draw the background of this key area.
	*/
	FStringCurveKeyArea(FStringCurve* InCurve, TAttribute<TOptional<FString>> InExternalValue, UMovieSceneSection* InOwningSection, TOptional<FLinearColor> InColor = TOptional<FLinearColor>())
		: Color(InColor)
		, Curve(InCurve)
		, OwningSection(InOwningSection)
		, ExternalValue(InExternalValue)
	{ }

public:

	//` IKeyArea interface

	virtual TArray<FKeyHandle> AddKeyUnique(float Time, EMovieSceneKeyInterpolation InKeyInterpolation, float TimeToCopyFrom = FLT_MAX) override;
	virtual TOptional<FKeyHandle> DuplicateKey(FKeyHandle KeyToDuplicate) override;
	virtual bool CanCreateKeyEditor() override;
	virtual TSharedRef<SWidget> CreateKeyEditor(ISequencer* Sequencer) override;
	virtual void DeleteKey(FKeyHandle KeyHandle) override;
	virtual TOptional<FLinearColor> GetColor() override;
	virtual ERichCurveExtrapolation GetExtrapolationMode(bool bPreInfinity) const override;
	virtual ERichCurveInterpMode GetKeyInterpMode(FKeyHandle KeyHandle) const override;
	virtual TSharedPtr<FStructOnScope> GetKeyStruct(FKeyHandle KeyHandle) override;
	virtual ERichCurveTangentMode GetKeyTangentMode(FKeyHandle KeyHandle) const override;
	virtual float GetKeyTime(FKeyHandle KeyHandle) const override;
	virtual UMovieSceneSection* GetOwningSection() override;
	virtual FRichCurve* GetRichCurve() override;
	virtual TArray<FKeyHandle> GetUnsortedKeyHandles() const override;
	virtual FKeyHandle DilateKey(FKeyHandle KeyHandle, float Scale, float Origin) override;
	virtual FKeyHandle MoveKey(FKeyHandle KeyHandle, float DeltaPosition) override;
	virtual void SetExtrapolationMode(ERichCurveExtrapolation ExtrapMode, bool bPreInfinity) override;
	virtual bool CanSetExtrapolationMode() const override;
	virtual void SetKeyInterpMode(FKeyHandle KeyHandle, ERichCurveInterpMode InterpMode) override;
	virtual void SetKeyTangentMode(FKeyHandle KeyHandle, ERichCurveTangentMode TangentMode) override;
	virtual void SetKeyTime(FKeyHandle KeyHandle, float NewKeyTime) override;
	virtual void CopyKeys(FMovieSceneClipboardBuilder& ClipboardBuilder, const TFunctionRef<bool(FKeyHandle, const IKeyArea&)>& KeyMask) const override;
	virtual void PasteKeys(const FMovieSceneClipboardKeyTrack& KeyTrack, const FMovieSceneClipboardEnvironment& SrcEnvironment, const FSequencerPasteEnvironment& DstEnvironment) override;

private:

	/** The key area's color. */
	TOptional<FLinearColor> Color;

	/** The curve which provides the keys for this key area. */
	FStringCurve* Curve;

	/** The section that owns this key area. */
	UMovieSceneSection* OwningSection;

	/** An attribute which allows the value for this key area to be overridden externally. */
	TAttribute<TOptional<FString>> ExternalValue;
};
