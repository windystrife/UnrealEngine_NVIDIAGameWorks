// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Animation/MarginTrackEditor.h"
#include "PropertySection.h"
#include "ISectionLayoutBuilder.h"
#include "FloatCurveKeyArea.h"


FName FMarginTrackEditor::LeftName( "Left" );
FName FMarginTrackEditor::TopName( "Top" );
FName FMarginTrackEditor::RightName( "Right" );
FName FMarginTrackEditor::BottomName( "Bottom" );


class FMarginPropertySection
	: public FPropertySection
{
public:

	FMarginPropertySection(ISequencer* InSequencer, FGuid InObjectBinding, FName InPropertyName, const FString& InPropertyPath, UMovieSceneSection& InSectionObject, const FText& SectionName)
		: FPropertySection(InSequencer, InObjectBinding, InPropertyName, InPropertyPath, InSectionObject, SectionName)
	{
	}

	virtual void GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const override
	{
		UMovieSceneMarginSection* MarginSection = Cast<UMovieSceneMarginSection>(&SectionObject);

		TAttribute<TOptional<float>> MarginLeftExternalValue = TAttribute<TOptional<float>>::Create(
			TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &FMarginPropertySection::GetMarginLeftValue));
		TSharedRef<FFloatCurveKeyArea> LeftKeyArea = MakeShareable(new FFloatCurveKeyArea(&MarginSection->GetLeftCurve(), MarginLeftExternalValue, MarginSection));

		TAttribute<TOptional<float>> MarginTopExternalValue = TAttribute<TOptional<float>>::Create(
			TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &FMarginPropertySection::GetMarginTopValue));
		TSharedRef<FFloatCurveKeyArea> TopKeyArea = MakeShareable(new FFloatCurveKeyArea(&MarginSection->GetTopCurve(), MarginTopExternalValue, MarginSection));

		TAttribute<TOptional<float>> MarginRightExternalValue = TAttribute<TOptional<float>>::Create(
			TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &FMarginPropertySection::GetMarginRightValue));
		TSharedRef<FFloatCurveKeyArea> RightKeyArea = MakeShareable(new FFloatCurveKeyArea(&MarginSection->GetRightCurve(), MarginRightExternalValue, MarginSection));

		TAttribute<TOptional<float>> MarginBottomExternalValue = TAttribute<TOptional<float>>::Create(
			TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &FMarginPropertySection::GetMarginBottomValue));
		TSharedRef<FFloatCurveKeyArea> BottomKeyArea = MakeShareable(new FFloatCurveKeyArea( &MarginSection->GetBottomCurve(), MarginBottomExternalValue, MarginSection));

		LayoutBuilder.AddKeyArea("Left", NSLOCTEXT("FMarginPropertySection", "MarginLeft", "Left"), LeftKeyArea);
		LayoutBuilder.AddKeyArea("Top", NSLOCTEXT("FMarginPropertySection", "MarginTop", "Top"), TopKeyArea);
		LayoutBuilder.AddKeyArea("Right", NSLOCTEXT("FMarginPropertySection", "MarginRight", "Right"), RightKeyArea);
		LayoutBuilder.AddKeyArea("Bottom", NSLOCTEXT("FMarginPropertySection", "MarginBottom", "Bottom"), BottomKeyArea);
	}


private:

	TOptional<float> GetMarginLeftValue() const
	{
		TOptional<FMargin> Margin = GetPropertyValue<FMargin>();
		return Margin.IsSet() ? TOptional<float>(Margin.GetValue().Left) : TOptional<float>();
	}

	TOptional<float> GetMarginTopValue() const
	{
		TOptional<FMargin> Margin = GetPropertyValue<FMargin>();
		return Margin.IsSet() ? TOptional<float>(Margin.GetValue().Top) : TOptional<float>();
	}

	TOptional<float> GetMarginRightValue() const
	{
		TOptional<FMargin> Margin = GetPropertyValue<FMargin>();
		return Margin.IsSet() ? TOptional<float>(Margin.GetValue().Right) : TOptional<float>();
	}

	TOptional<float> GetMarginBottomValue() const
	{
		TOptional<FMargin> Margin = GetPropertyValue<FMargin>();
		return Margin.IsSet() ? TOptional<float>(Margin.GetValue().Bottom) : TOptional<float>();
	}
};


TSharedRef<ISequencerTrackEditor> FMarginTrackEditor::CreateTrackEditor( TSharedRef<ISequencer> InSequencer )
{
	return MakeShareable( new FMarginTrackEditor( InSequencer ) );
}


TSharedRef<ISequencerSection> FMarginTrackEditor::MakeSectionInterface( UMovieSceneSection& SectionObject, UMovieSceneTrack& Track, FGuid ObjectBinding )
{
	UMovieScenePropertyTrack* PropertyTrack = Cast<UMovieScenePropertyTrack>(&Track);
	checkf(PropertyTrack != nullptr, TEXT("Incompatible track in FMarginTrackEditor"));
	return MakeShareable(new FMarginPropertySection(GetSequencer().Get(), ObjectBinding, PropertyTrack->GetPropertyName(), PropertyTrack->GetPropertyPath(), SectionObject, Track.GetDisplayName()));
}


void FMarginTrackEditor::GenerateKeysFromPropertyChanged( const FPropertyChangedParams& PropertyChangedParams, TArray<FMarginKey>& NewGeneratedKeys, TArray<FMarginKey>& DefaultGeneratedKeys)
{
	FName ChannelName = PropertyChangedParams.StructPropertyNameToKey;
	FMargin Margin = PropertyChangedParams.GetPropertyValue<FMargin>();

	TArray<FMarginKey>& LeftKeys = ChannelName == NAME_None || ChannelName == LeftName ? NewGeneratedKeys : DefaultGeneratedKeys;
	LeftKeys.Add( FMarginKey( EKeyMarginChannel::Left, Margin.Left ) );

	TArray<FMarginKey>& TopKeys = ChannelName == NAME_None || ChannelName == TopName ? NewGeneratedKeys : DefaultGeneratedKeys;
	TopKeys.Add( FMarginKey( EKeyMarginChannel::Top, Margin.Top ) );

	TArray<FMarginKey>& RightKeys = ChannelName == NAME_None || ChannelName == RightName ? NewGeneratedKeys : DefaultGeneratedKeys;
	RightKeys.Add( FMarginKey( EKeyMarginChannel::Right, Margin.Right ) );

	TArray<FMarginKey>& BottomKeys = ChannelName == NAME_None || ChannelName == BottomName ? NewGeneratedKeys : DefaultGeneratedKeys;
	BottomKeys.Add( FMarginKey( EKeyMarginChannel::Bottom, Margin.Bottom ) );
}
