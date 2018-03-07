// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateColor.h"
#include "EpicSurvey.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Animation/CurveSequence.h"
#include "Widgets/SCompoundWidget.h"
#include "Survey.h"
#include "SSurvey.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"

#define LOCTEXT_NAMESPACE "EpicSurvey"

class SSurveyNotification : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SSurvey )
	{ }
	SLATE_END_ARGS()

	/** Widget constructor */
	void Construct( const FArguments& Args, const TSharedRef< FEpicSurvey >& InEpicSurvey, const TSharedRef< FSurvey >& InSurvey )
	{
		EpicSurvey = InEpicSurvey;
		Survey = InSurvey;

		Color = FSlateColor( FLinearColor( 1, 1, 1, 0 ) );
		Sequence.AddCurve( 0, 1, ECurveEaseFunction::QuadOut );

		ChildSlot
		[
			SNew( SButton )
			.ButtonStyle( FEditorStyle::Get(), "ToggleButton" )
			.ForegroundColor( this, &SSurveyNotification::GetInvertedForegroundIfHovered )
			.ContentPadding( 0 )
			.OnClicked( this, &SSurveyNotification::HandleClicked )
			.ToolTipText( LOCTEXT( "SurveyNotificationToolTip", "Help Epic improve UE4 by taking a minute to fill out this survey!" ) )
			[
				SNew( SImage )
				.Image( FEditorStyle::GetBrush( "Icons.Download" ) )
				.ColorAndOpacity( this, &SSurveyNotification::GetPinColorAndOpacity )
			]
		];
	}

	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override
	{
		if ( this->IsHovered() )
		{
			const_cast<SSurveyNotification*>(this)->Sequence.JumpToStart();
			Color = FSlateColor::UseForeground();
		}
		else
		{
			float Opacity = 1.0f;
			if ( EpicSurvey->ShouldPulseSurveyIcon( Survey.ToSharedRef() ) )
			{
				// When pulsing starts jump to the start of the animation.
				if (!bIsPulsing)
				{
					const_cast<SSurveyNotification*>(this)->Sequence.JumpToStart();
					bIsPulsing = true;
				}
				Opacity = 1 - (Sequence.GetLerp() * .8f);
			}
			else
			{
				bIsPulsing = false;
			}

			if ( !Sequence.IsPlaying() )
			{
				if ( Sequence.IsAtStart() )
				{
					const_cast<SSurveyNotification*>(this)->Sequence.Play(this->AsShared());
				}
				else
				{
					const_cast<SSurveyNotification*>(this)->Sequence.PlayReverse(this->AsShared());
				}
			}

			Color = FSlateColor( FColor( 255, 255, 255, FMath::Lerp( 0, 200, Opacity ) ).ReinterpretAsLinear() );
		}
	}


private:

	FReply HandleClicked()
	{
		EpicSurvey->OpenEpicSurveyWindow();
		return FReply::Handled();
	}

	FSlateColor GetInvertedForegroundIfHovered() const
	{
		static const FName InvertedForeground("InvertedForeground");
		return this->IsHovered() ? FEditorStyle::GetSlateColor( InvertedForeground ) : FSlateColor::UseForeground();
	}

	FSlateColor GetPinColorAndOpacity() const
	{
		return Color;
	}


private:

	TSharedPtr< FEpicSurvey > EpicSurvey;
	TSharedPtr< FSurvey > Survey;

	/** Animation sequence to pulse the image */
	FCurveSequence Sequence;
	FSlateColor Color;
	bool bIsPulsing;

};

#undef LOCTEXT_NAMESPACE
