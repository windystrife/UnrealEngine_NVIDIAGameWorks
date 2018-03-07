// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/Sequencer2DTransformTrackEditor.h"
#include "PropertySection.h"
#include "ISectionLayoutBuilder.h"
#include "FloatCurveKeyArea.h"


FName F2DTransformTrackEditor::TranslationName( "Translation" );
FName F2DTransformTrackEditor::ScaleName( "Scale" );
FName F2DTransformTrackEditor::ShearName( "Shear" );
FName F2DTransformTrackEditor::AngleName( "Angle" );


class F2DTransformSection
	: public FPropertySection
{
public:

	F2DTransformSection(ISequencer* InSequencer, FGuid InObjectBinding, FName InPropertyName, const FString& InPropertyPath, UMovieSceneSection& InSectionObject, const FText& SectionName)
		: FPropertySection(InSequencer, InObjectBinding, InPropertyName, InPropertyPath, InSectionObject, SectionName)
	{
	}

	virtual void GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const override
	{
		static const FLinearColor GreenKeyAreaColor(0.0f, 0.7f, 0.0f, 0.2f);
		static const FLinearColor RedKeyAreaColor(0.7f, 0.0f, 0.0f, 0.2f);

		UMovieScene2DTransformSection* TransformSection = Cast<UMovieScene2DTransformSection>(&SectionObject);

		// Translation
		TAttribute<TOptional<float>> TranslationXExternalValue = TAttribute<TOptional<float>>::Create(
			TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &F2DTransformSection::GetTranslationXValue));
		TSharedRef<FFloatCurveKeyArea> TranslationXKeyArea = MakeShareable(new FFloatCurveKeyArea(&TransformSection->GetTranslationCurve(EAxis::X), TranslationXExternalValue, TransformSection, RedKeyAreaColor));

		TAttribute<TOptional<float>> TranslationYExternalValue = TAttribute<TOptional<float>>::Create(
			TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &F2DTransformSection::GetTranslationYValue));
		TSharedRef<FFloatCurveKeyArea> TranslationYKeyArea = MakeShareable(new FFloatCurveKeyArea(&TransformSection->GetTranslationCurve(EAxis::Y), TranslationYExternalValue, TransformSection, GreenKeyAreaColor));

		// Rotation
		TAttribute<TOptional<float>> RotationExternalValue = TAttribute<TOptional<float>>::Create(
			TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &F2DTransformSection::GetRotationValue));
		TSharedRef<FFloatCurveKeyArea> RotationKeyArea = MakeShareable(new FFloatCurveKeyArea(&TransformSection->GetRotationCurve(), RotationExternalValue, TransformSection));

		// Scale
		TAttribute<TOptional<float>> ScaleXExternalValue = TAttribute<TOptional<float>>::Create(
			TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &F2DTransformSection::GetScaleXValue));
		TSharedRef<FFloatCurveKeyArea> ScaleXKeyArea = MakeShareable(new FFloatCurveKeyArea(&TransformSection->GetScaleCurve(EAxis::X), ScaleXExternalValue, TransformSection, RedKeyAreaColor));

		TAttribute<TOptional<float>> ScaleYExternalValue = TAttribute<TOptional<float>>::Create(
			TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &F2DTransformSection::GetScaleYValue));
		TSharedRef<FFloatCurveKeyArea> ScaleYKeyArea = MakeShareable(new FFloatCurveKeyArea(&TransformSection->GetScaleCurve(EAxis::Y), ScaleYExternalValue, TransformSection, GreenKeyAreaColor));

		// Shear
		TAttribute<TOptional<float>> ShearXExternalValue = TAttribute<TOptional<float>>::Create(
			TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &F2DTransformSection::GetShearXValue));
		TSharedRef<FFloatCurveKeyArea> ShearXKeyArea = MakeShareable(new FFloatCurveKeyArea(&TransformSection->GetShearCurve(EAxis::X), ShearXExternalValue, TransformSection, RedKeyAreaColor));

		TAttribute<TOptional<float>> ShearYExternalValue = TAttribute<TOptional<float>>::Create(
			TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &F2DTransformSection::GetShearYValue));
		TSharedRef<FFloatCurveKeyArea> ShearYKeyArea = MakeShareable(new FFloatCurveKeyArea(&TransformSection->GetShearCurve(EAxis::Y), ShearYExternalValue, TransformSection, GreenKeyAreaColor));

		// This generates the tree structure for the transform section
		LayoutBuilder.PushCategory("Translation", NSLOCTEXT("F2DTransformSection", "TranslationArea", "Translation"));
			LayoutBuilder.AddKeyArea("Translation.X", NSLOCTEXT("F2DTransformSection", "TransXArea", "X"), TranslationXKeyArea);
			LayoutBuilder.AddKeyArea("Translation.Y", NSLOCTEXT("F2DTransformSection", "TransYArea", "Y"), TranslationYKeyArea);
		LayoutBuilder.PopCategory();

		LayoutBuilder.PushCategory("Rotation", NSLOCTEXT("F2DTransformSection", "RotationArea", "Rotation"));
			LayoutBuilder.AddKeyArea("Rotation.Angle", NSLOCTEXT("F2DTransformSection", "AngleArea", "Angle"), RotationKeyArea);
		LayoutBuilder.PopCategory();

		LayoutBuilder.PushCategory("Scale", NSLOCTEXT("F2DTransformSection", "ScaleArea", "Scale"));
			LayoutBuilder.AddKeyArea("Scale.X", NSLOCTEXT("F2DTransformSection", "ScaleXArea", "X"), ScaleXKeyArea);
			LayoutBuilder.AddKeyArea("Scale.Y", NSLOCTEXT("F2DTransformSection", "ScaleYArea", "Y"), ScaleYKeyArea);
		LayoutBuilder.PopCategory();

		LayoutBuilder.PushCategory("Shear", NSLOCTEXT("F2DTransformSection", "ShearArea", "Shear"));
			LayoutBuilder.AddKeyArea("Shear.X", NSLOCTEXT("F2DTransformSection", "SheerXArea", "X"), ShearXKeyArea);
			LayoutBuilder.AddKeyArea("Shear.Y", NSLOCTEXT("F2DTransformSection", "SheerYArea", "Y"), ShearYKeyArea);
		LayoutBuilder.PopCategory();
	}

private:

	TOptional<float> GetTranslationXValue() const
	{
		TOptional<FWidgetTransform> Transform = GetPropertyValue<FWidgetTransform>();
		return Transform.IsSet() ? TOptional<float>(Transform.GetValue().Translation.X) : TOptional<float>();
	}

	TOptional<float> GetTranslationYValue() const
	{
		TOptional<FWidgetTransform> Transform = GetPropertyValue<FWidgetTransform>();
		return Transform.IsSet() ? TOptional<float>(Transform.GetValue().Translation.Y) : TOptional<float>();
	}

	TOptional<float> GetRotationValue() const
	{
		TOptional<FWidgetTransform> Transform = GetPropertyValue<FWidgetTransform>();
		return Transform.IsSet() ? TOptional<float>(Transform.GetValue().Angle) : TOptional<float>();
	}

	TOptional<float> GetScaleXValue() const
	{
		TOptional<FWidgetTransform> Transform = GetPropertyValue<FWidgetTransform>();
		return Transform.IsSet() ? TOptional<float>(Transform.GetValue().Scale.X) : TOptional<float>();
	}

	TOptional<float> GetScaleYValue() const
	{
		TOptional<FWidgetTransform> Transform = GetPropertyValue<FWidgetTransform>();
		return Transform.IsSet() ? TOptional<float>(Transform.GetValue().Scale.Y) : TOptional<float>();
	}

	TOptional<float> GetShearXValue() const
	{
		TOptional<FWidgetTransform> Transform = GetPropertyValue<FWidgetTransform>();
		return Transform.IsSet() ? TOptional<float>(Transform.GetValue().Shear.X) : TOptional<float>();
	}

	TOptional<float> GetShearYValue() const
	{
		TOptional<FWidgetTransform> Transform = GetPropertyValue<FWidgetTransform>();
		return Transform.IsSet() ? TOptional<float>(Transform.GetValue().Shear.Y) : TOptional<float>();
	}

};


TSharedRef<ISequencerTrackEditor> F2DTransformTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new F2DTransformTrackEditor( InSequencer ) );
}


TSharedRef<ISequencerSection> F2DTransformTrackEditor::MakeSectionInterface(UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding)
{
	UMovieScenePropertyTrack* PropertyTrack = Cast<UMovieScenePropertyTrack>(&Track);
	checkf(PropertyTrack != nullptr, TEXT("Incompatible track in F2DTransformTrackEditor"));
	return MakeShareable(new F2DTransformSection(GetSequencer().Get(), ObjectBinding, PropertyTrack->GetPropertyName(), PropertyTrack->GetPropertyPath(), SectionObject, Track.GetDisplayName()));
}


void F2DTransformTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, TArray<F2DTransformKey>& NewGeneratedKeys, TArray<F2DTransformKey>& DefaultGeneratedKeys )
{
	FName ChannelName = PropertyChangedParams.StructPropertyNameToKey;
	FWidgetTransform Transform = PropertyChangedParams.GetPropertyValue<FWidgetTransform>();

	TArray<F2DTransformKey>& TranslationKeys = ChannelName == NAME_None || ChannelName == TranslationName ? NewGeneratedKeys : DefaultGeneratedKeys;
	TranslationKeys.Add( F2DTransformKey( EKey2DTransformChannel::Translation, EKey2DTransformAxis::X, Transform.Translation.X ) );
	TranslationKeys.Add( F2DTransformKey( EKey2DTransformChannel::Translation, EKey2DTransformAxis::Y, Transform.Translation.Y ) );

	TArray<F2DTransformKey>& ScaleKeys = ChannelName == NAME_None || ChannelName == ScaleName ? NewGeneratedKeys : DefaultGeneratedKeys;
	ScaleKeys.Add( F2DTransformKey( EKey2DTransformChannel::Scale, EKey2DTransformAxis::X, Transform.Scale.X ) );
	ScaleKeys.Add( F2DTransformKey( EKey2DTransformChannel::Scale, EKey2DTransformAxis::Y, Transform.Scale.Y ) );

	TArray<F2DTransformKey>& ShearKeys = ChannelName == NAME_None || ChannelName == ShearName ? NewGeneratedKeys : DefaultGeneratedKeys;
	ShearKeys.Add( F2DTransformKey( EKey2DTransformChannel::Shear, EKey2DTransformAxis::X, Transform.Shear.X ) );
	ShearKeys.Add( F2DTransformKey( EKey2DTransformChannel::Shear, EKey2DTransformAxis::Y, Transform.Shear.Y ) );

	TArray<F2DTransformKey>& AngleKeys = ChannelName == NAME_None || ChannelName == AngleName ? NewGeneratedKeys : DefaultGeneratedKeys;
	AngleKeys.Add( F2DTransformKey( EKey2DTransformChannel::Rotation, EKey2DTransformAxis::None, Transform.Angle ) );
}
