// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "ComposurePostMoveSettingsPropertySection.h"
#include "MovieScene/MovieSceneComposurePostMoveSettingsSection.h"
#include "ISectionLayoutBuilder.h"
#include "FloatCurveKeyArea.h"
#include "ComposurePostMoves.h"

FComposurePostMoveSettingsPropertySection::FComposurePostMoveSettingsPropertySection(ISequencer* InSequencer, FGuid InObjectBinding, FName InPropertyName, const FString& InPropertyPath, UMovieSceneSection& InSectionObject, const FText& SectionName)
	: FPropertySection(InSequencer, InObjectBinding, InPropertyName, InPropertyPath, InSectionObject, SectionName)
{
}

void FComposurePostMoveSettingsPropertySection::GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const
{
	static const FLinearColor GreenKeyAreaColor(0.0f, 0.7f, 0.0f, 0.2f);
	static const FLinearColor RedKeyAreaColor(0.7f, 0.0f, 0.0f, 0.2f);

	UMovieSceneComposurePostMoveSettingsSection* PostMoveSection = Cast<UMovieSceneComposurePostMoveSettingsSection>(&SectionObject);

	// Pivot
	TAttribute<TOptional<float>> PivotXExternalValue = TAttribute<TOptional<float>>::Create(
		TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &FComposurePostMoveSettingsPropertySection::GetPivotXValue));
	TSharedRef<FFloatCurveKeyArea> PivotXKeyArea = MakeShared<FFloatCurveKeyArea>(&PostMoveSection->GetCurve(EComposurePostMoveSettingsChannel::Pivot, EComposurePostMoveSettingsAxis::X), PivotXExternalValue, PostMoveSection, RedKeyAreaColor);

	TAttribute<TOptional<float>> PivotYExternalValue = TAttribute<TOptional<float>>::Create(
		TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &FComposurePostMoveSettingsPropertySection::GetPivotYValue));
	TSharedRef<FFloatCurveKeyArea> PivotYKeyArea = MakeShared<FFloatCurveKeyArea>(&PostMoveSection->GetCurve(EComposurePostMoveSettingsChannel::Pivot, EComposurePostMoveSettingsAxis::Y), PivotYExternalValue, PostMoveSection, GreenKeyAreaColor);

	// Translation
	TAttribute<TOptional<float>> TranslationXExternalValue = TAttribute<TOptional<float>>::Create(
		TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &FComposurePostMoveSettingsPropertySection::GetTranslationXValue));
	TSharedRef<FFloatCurveKeyArea> TranslationXKeyArea = MakeShared<FFloatCurveKeyArea>(&PostMoveSection->GetCurve(EComposurePostMoveSettingsChannel::Translation, EComposurePostMoveSettingsAxis::X), TranslationXExternalValue, PostMoveSection, RedKeyAreaColor);

	TAttribute<TOptional<float>> TranslationYExternalValue = TAttribute<TOptional<float>>::Create(
		TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &FComposurePostMoveSettingsPropertySection::GetTranslationYValue));
	TSharedRef<FFloatCurveKeyArea> TranslationYKeyArea = MakeShared<FFloatCurveKeyArea>(&PostMoveSection->GetCurve(EComposurePostMoveSettingsChannel::Translation, EComposurePostMoveSettingsAxis::Y), TranslationYExternalValue, PostMoveSection, GreenKeyAreaColor);

	// Rotation
	TAttribute<TOptional<float>> RotationExternalValue = TAttribute<TOptional<float>>::Create(
		TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &FComposurePostMoveSettingsPropertySection::GetRotationValue));
	TSharedRef<FFloatCurveKeyArea> RotationKeyArea = MakeShared<FFloatCurveKeyArea>(&PostMoveSection->GetCurve(EComposurePostMoveSettingsChannel::RotationAngle, EComposurePostMoveSettingsAxis::None), RotationExternalValue, PostMoveSection);

	// Scale
	TAttribute<TOptional<float>> ScaleExternalValue = TAttribute<TOptional<float>>::Create(
		TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &FComposurePostMoveSettingsPropertySection::GetScaleValue));
	TSharedRef<FFloatCurveKeyArea> ScaleKeyArea = MakeShared<FFloatCurveKeyArea>(&PostMoveSection->GetCurve(EComposurePostMoveSettingsChannel::Scale, EComposurePostMoveSettingsAxis::None), ScaleExternalValue, PostMoveSection);

	// Layout
	LayoutBuilder.PushCategory("Pivot", NSLOCTEXT("FPostMoveSettingsSection", "PivotCategory", "Pivot"));
	LayoutBuilder.AddKeyArea("Pivot.X", NSLOCTEXT("FPostMoveSettingsSection", "PivotXArea", "X"), PivotXKeyArea);
	LayoutBuilder.AddKeyArea("Pivot.Y", NSLOCTEXT("FPostMoveSettingsSection", "PivotYArea", "Y"), PivotYKeyArea);
	LayoutBuilder.PopCategory();

	LayoutBuilder.PushCategory("Translation", NSLOCTEXT("FPostMoveSettingsSection", "TranslationCategory", "Translation"));
	LayoutBuilder.AddKeyArea("Translation.X", NSLOCTEXT("FPostMoveSettingsSection", "TranslationXArea", "X"), TranslationXKeyArea);
	LayoutBuilder.AddKeyArea("Translation.Y", NSLOCTEXT("FPostMoveSettingsSection", "TranslationYArea", "Y"), TranslationYKeyArea);
	LayoutBuilder.PopCategory();

	LayoutBuilder.PushCategory("Rotation", NSLOCTEXT("FPostMoveSettingsSection", "RotationCategory", "Rotation"));
	LayoutBuilder.AddKeyArea("Rotation", NSLOCTEXT("FPostMoveSettingsSection", "RotationArea", "Rotation"), RotationKeyArea);
	LayoutBuilder.PopCategory();

	LayoutBuilder.PushCategory("Scale", NSLOCTEXT("FPostMoveSettingsSection", "ScaleCategory", "Scale"));
	LayoutBuilder.AddKeyArea("Scale", NSLOCTEXT("FPostMoveSettingsSection", "ScaleArea", "Scale"), ScaleKeyArea);
	LayoutBuilder.PopCategory();
}

TOptional<float> FComposurePostMoveSettingsPropertySection::GetPivotXValue() const
{
	TOptional<FComposurePostMoveSettings> PostMoveSettings = GetPropertyValue<FComposurePostMoveSettings>();
	return PostMoveSettings.IsSet() ? TOptional<float>(PostMoveSettings.GetValue().Pivot.X) : TOptional<float>();
}

TOptional<float> FComposurePostMoveSettingsPropertySection::GetPivotYValue() const
{
	TOptional<FComposurePostMoveSettings> PostMoveSettings = GetPropertyValue<FComposurePostMoveSettings>();
	return PostMoveSettings.IsSet() ? TOptional<float>(PostMoveSettings.GetValue().Pivot.Y) : TOptional<float>();
}

TOptional<float> FComposurePostMoveSettingsPropertySection::GetTranslationXValue() const
{
	TOptional<FComposurePostMoveSettings> PostMoveSettings = GetPropertyValue<FComposurePostMoveSettings>();
	return PostMoveSettings.IsSet() ? TOptional<float>(PostMoveSettings.GetValue().Translation.X) : TOptional<float>();
}

TOptional<float> FComposurePostMoveSettingsPropertySection::GetTranslationYValue() const
{
	TOptional<FComposurePostMoveSettings> PostMoveSettings = GetPropertyValue<FComposurePostMoveSettings>();
	return PostMoveSettings.IsSet() ? TOptional<float>(PostMoveSettings.GetValue().Translation.Y) : TOptional<float>();
}

TOptional<float> FComposurePostMoveSettingsPropertySection::GetRotationValue() const
{
	TOptional<FComposurePostMoveSettings> PostMoveSettings = GetPropertyValue<FComposurePostMoveSettings>();
	return PostMoveSettings.IsSet() ? TOptional<float>(PostMoveSettings.GetValue().RotationAngle) : TOptional<float>();
}

TOptional<float> FComposurePostMoveSettingsPropertySection::GetScaleValue() const
{
	TOptional<FComposurePostMoveSettings> PostMoveSettings = GetPropertyValue<FComposurePostMoveSettings>();
	return PostMoveSettings.IsSet() ? TOptional<float>(PostMoveSettings.GetValue().Scale) : TOptional<float>();
}