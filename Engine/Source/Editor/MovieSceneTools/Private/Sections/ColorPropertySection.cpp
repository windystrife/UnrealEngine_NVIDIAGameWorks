// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/ColorPropertySection.h"
#include "Rendering/DrawElements.h"
#include "Sections/MovieSceneColorSection.h"
#include "SequencerSectionPainter.h"
#include "FloatCurveKeyArea.h"
#include "ISectionLayoutBuilder.h"
#include "EditorStyleSet.h"
#include "CommonMovieSceneTools.h"

const FName SlateColorName("SlateColor");

void FColorPropertySection::GenerateSectionLayout( class ISectionLayoutBuilder& LayoutBuilder ) const
{
	UMovieSceneColorSection* ColorSection = Cast<UMovieSceneColorSection>( &SectionObject );

	TAttribute<TOptional<float>> RedExternalValue = TAttribute<TOptional<float>>::Create(
		TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &FColorPropertySection::GetColorRedValue));
	TSharedRef<FFloatCurveKeyArea> RedKeyArea = MakeShareable( new FFloatCurveKeyArea( &ColorSection->GetRedCurve(), RedExternalValue, ColorSection ) ) ;

	TAttribute<TOptional<float>> GreenExternalValue = TAttribute<TOptional<float>>::Create(
		TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &FColorPropertySection::GetColorGreenValue));
	TSharedRef<FFloatCurveKeyArea> GreenKeyArea = MakeShareable( new FFloatCurveKeyArea( &ColorSection->GetGreenCurve(), GreenExternalValue, ColorSection ) ) ;
	
	TAttribute<TOptional<float>> BlueExternalValue = TAttribute<TOptional<float>>::Create(
		TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &FColorPropertySection::GetColorBlueValue));
	TSharedRef<FFloatCurveKeyArea> BlueKeyArea = MakeShareable( new FFloatCurveKeyArea( &ColorSection->GetBlueCurve(), BlueExternalValue, ColorSection ) ) ;
	
	TAttribute<TOptional<float>> AlphaExternalValue = TAttribute<TOptional<float>>::Create(
		TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &FColorPropertySection::GetColorAlphaValue));
	TSharedRef<FFloatCurveKeyArea> AlphaKeyArea = MakeShareable( new FFloatCurveKeyArea( &ColorSection->GetAlphaCurve(), AlphaExternalValue, ColorSection ) );

	LayoutBuilder.AddKeyArea( "R", NSLOCTEXT( "FColorPropertySection", "RedArea", "Red" ), RedKeyArea );
	LayoutBuilder.AddKeyArea( "G", NSLOCTEXT( "FColorPropertySection", "GreenArea", "Green" ), GreenKeyArea );
	LayoutBuilder.AddKeyArea( "B", NSLOCTEXT( "FColorPropertySection", "BlueArea", "Blue" ), BlueKeyArea );
	LayoutBuilder.AddKeyArea( "A", NSLOCTEXT( "FColorPropertySection", "OpacityArea", "Opacity" ), AlphaKeyArea );
}


int32 FColorPropertySection::OnPaintSection( FSequencerSectionPainter& Painter ) const
{
	int32 LayerId = Painter.PaintSectionBackground();

	const ESlateDrawEffect DrawEffects = Painter.bParentEnabled ? ESlateDrawEffect::None : ESlateDrawEffect::DisabledEffect;

	const UMovieSceneColorSection* ColorSection = Cast<const UMovieSceneColorSection>( &SectionObject );

	const FTimeToPixel& TimeConverter = Painter.GetTimeConverter();

	const float StartTime = TimeConverter.PixelToTime(0.f);
	const float EndTime = TimeConverter.PixelToTime(Painter.SectionGeometry.GetLocalSize().X);
	const float SectionDuration = EndTime - StartTime;

	FVector2D GradientSize = FVector2D( Painter.SectionGeometry.Size.X - 2.f, (Painter.SectionGeometry.Size.Y / 4) - 3.0f );
	if ( GradientSize.X >= 1.f )
	{

		FPaintGeometry PaintGeometry = Painter.SectionGeometry.ToPaintGeometry( FVector2D( 1.f, 1.f ), GradientSize );

		// If we are showing a background pattern and the colors is transparent, draw a checker pattern
		FSlateDrawElement::MakeBox(
			Painter.DrawElements,
			LayerId,
			PaintGeometry,
			FEditorStyle::GetBrush( "Checker" ),
			DrawEffects);

		TArray<FSlateGradientStop> GradientStops;

		TArray< TKeyValuePair<float, FLinearColor> > ColorKeys;
		ConsolidateColorCurves( ColorKeys, ColorSection );

		for ( int32 i = 0; i < ColorKeys.Num(); ++i )
		{
			float Time = ColorKeys[i].Key;
			FLinearColor Color = ColorKeys[i].Value;

			// HACK: The color is converted to SRgb and then reinterpreted as linear here because gradients are converted to FColor
			// without the SRgb conversion before being passed to the renderer for some reason.
			Color = Color.ToFColor( true ).ReinterpretAsLinear();

			float TimeFraction = (Time - StartTime) / SectionDuration;

			GradientStops.Add( FSlateGradientStop( FVector2D( TimeFraction * Painter.SectionGeometry.Size.X, 0 ), Color ) );
		}

		if ( GradientStops.Num() > 0 )
		{
			FSlateDrawElement::MakeGradient(
				Painter.DrawElements,
				Painter.LayerId + 1,
				PaintGeometry,
				GradientStops,
				Orient_Vertical,
				DrawEffects
				);
		}
	}

	return LayerId + 1;
}


void FColorPropertySection::ConsolidateColorCurves( TArray< TKeyValuePair<float, FLinearColor> >& OutColorKeys, const UMovieSceneColorSection* Section ) const
{
	// Get the default color from the current property value, or use black if the current property
	// value can't be found.
	FLinearColor DefaultColor;
	TOptional<FLinearColor> CurrentPropertyValue = GetPropertyValueAsLinearColor();
	if (CurrentPropertyValue.IsSet())
	{
		DefaultColor = CurrentPropertyValue.GetValue();
	}
	else
	{
		DefaultColor = FLinearColor::Black;
	}

	// @todo Sequencer Optimize - This could all get cached, instead of recalculating everything every OnPaint

	const FRichCurve* Curves[4] = {
		&Section->GetRedCurve(),
		&Section->GetGreenCurve(),
		&Section->GetBlueCurve(),
		&Section->GetAlphaCurve()
	};

	// @todo Sequencer Optimize - This is a O(n^2) loop!
	// Our times are floats, which means we can't use a map and
	// do a quick lookup to see if the keys already exist
	// because the keys are ordered, we could take advantage of that, however
	TArray<float> TimesWithKeys;
	for ( int32 i = 0; i < 4; ++i )
	{
		const FRichCurve* Curve = Curves[i];
		for ( auto It( Curve->GetKeyIterator() ); It; ++It )
		{
			float KeyTime = It->Time;

			bool bShouldAddKey = true;

			int32 InsertKeyIndex = INDEX_NONE;
			for ( int32 k = 0; k < TimesWithKeys.Num(); ++k )
			{
				if ( FMath::IsNearlyEqual( TimesWithKeys[k], KeyTime ) )
				{
					bShouldAddKey = false;
					break;
				}
				else if ( TimesWithKeys[k] > KeyTime )
				{
					InsertKeyIndex = k;
					break;
				}
			}

			if ( InsertKeyIndex == INDEX_NONE && bShouldAddKey )
			{
				InsertKeyIndex = TimesWithKeys.Num();
			}

			if ( bShouldAddKey )
			{
				TimesWithKeys.Insert( KeyTime, InsertKeyIndex );
			}
		}
	}

	// Enforce at least one key for the default value
	if (TimesWithKeys.Num() == 0)
	{
		TimesWithKeys.Add(0);
	}

	// @todo Sequencer Optimize - This another O(n^2) loop, since Eval is O(n)!
	for ( int32 i = 0; i < TimesWithKeys.Num(); ++i )
	{
		float Time = TimesWithKeys[i];

		FLinearColor Color(
			Section->GetRedCurve().Eval(Time, DefaultColor.R),
			Section->GetGreenCurve().Eval(Time, DefaultColor.G),
			Section->GetBlueCurve().Eval(Time, DefaultColor.B),
			Section->GetAlphaCurve().Eval(Time, DefaultColor.A)
			);
		OutColorKeys.Add( TKeyValuePair<float, FLinearColor>( TimesWithKeys[i], Color ) );
	}
}


TOptional<FLinearColor> FColorPropertySection::GetPropertyValueAsLinearColor() const
{
	UStructProperty* ColorStructProperty = Cast<UStructProperty>(GetProperty());
	if (ColorStructProperty != nullptr)
	{
		if (ColorStructProperty->Struct->GetFName() == SlateColorName)
		{
			TOptional<FSlateColor> SlateColor = GetPropertyValue<FSlateColor>();
			if (SlateColor.IsSet())
			{
				return TOptional<FLinearColor>(SlateColor.GetValue().GetSpecifiedColor());
			}
		}

		if (ColorStructProperty->Struct->GetFName() == NAME_LinearColor)
		{
			return GetPropertyValue<FLinearColor>();
		}

		if (ColorStructProperty->Struct->GetFName() == NAME_Color)
		{
			TOptional<FColor> Color = GetPropertyValue<FColor>();
			if (Color.IsSet())
			{
				return TOptional<FLinearColor>(Color.GetValue());
			}
		}
	}
	return TOptional<FLinearColor>();
}

TOptional<float> FColorPropertySection::GetColorRedValue() const
{
	TOptional<FLinearColor> CurrentValue = GetPropertyValueAsLinearColor();
	return CurrentValue.IsSet() ? CurrentValue.GetValue().R : TOptional<float>();
}

TOptional<float> FColorPropertySection::GetColorGreenValue() const
{
	TOptional<FLinearColor> CurrentValue = GetPropertyValueAsLinearColor();
	return CurrentValue.IsSet() ? CurrentValue.GetValue().G : TOptional<float>();
}

TOptional<float> FColorPropertySection::GetColorBlueValue() const
{
	TOptional<FLinearColor> CurrentValue = GetPropertyValueAsLinearColor();
	return CurrentValue.IsSet() ? CurrentValue.GetValue().B : TOptional<float>();
}

TOptional<float> FColorPropertySection::GetColorAlphaValue() const
{
	TOptional<FLinearColor> CurrentValue = GetPropertyValueAsLinearColor();
	return CurrentValue.IsSet() ? CurrentValue.GetValue().A : TOptional<float>();
}
