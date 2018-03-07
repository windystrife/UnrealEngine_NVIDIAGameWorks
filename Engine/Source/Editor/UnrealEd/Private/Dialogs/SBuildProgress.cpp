// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "Dialogs/SBuildProgress.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "UnrealEdMisc.h"


SBuildProgressWidget::SBuildProgressWidget() 
{			
}

SBuildProgressWidget::~SBuildProgressWidget()
{
}

void SBuildProgressWidget::Construct( const FArguments& InArgs )
{
	BorderImage = FEditorStyle::GetBrush("Menu.Background");
	this->ChildSlot
	.VAlign(VAlign_Center)
	[
		SNew( SVerticalBox )
		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.Padding( 10.0f, 4.0f )
		[
			SNew(SBorder)
			[
				SNew( SVerticalBox )
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Fill)
				.Padding( 10.0f, 4.0f )
				[
					SNew( STextBlock )
					.Text( NSLOCTEXT("BuildProgress", "BuildStatusLabel", "Build Status") )
					.ShadowOffset( FVector2D( 1.0f, 1.0f ) )
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Fill)
				.Padding( 10.0f, 4.0f )
				[
					SNew( SHorizontalBox)
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding( 0, 7.0f )
					[
						SNew( STextBlock )
						.Text( this, &SBuildProgressWidget::OnGetBuildTimeText )
						.ShadowOffset( FVector2D( 1.0f, 1.0f ) )
					]
					+SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(10.0f, 7.0f, 10.0f, 7.0f)
					[
						SNew( STextBlock )
						.Text( this, &SBuildProgressWidget::OnGetProgressText )
						.ShadowOffset( FVector2D( 1.0f, 1.0f ) )
					]
				]
			]
		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign(HAlign_Fill)
		.Padding(10.0f, 1.0f)
		[
			SNew(SBorder)
			[
				SNew( SVerticalBox )
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Fill)
				.Padding( 10.0f, 4.0f )
				[
					SNew( STextBlock )
					.Text( NSLOCTEXT("BuildProgress", "BuildProgressLabel", "Build Progress") )
					.ShadowOffset( FVector2D( 1.0f, 1.0f ) )
				]
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Fill)
				.Padding( 10.0f, 7.0f, 10.0f, 7.0f )
				[
					SNew(SProgressBar)
					.Percent( this, &SBuildProgressWidget::OnGetProgressFraction )	
				]
			]

		]
		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding(15.0f, 4.0f)
		.HAlign(HAlign_Center)
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text( NSLOCTEXT("BuildProgress", "StopBuildButtonLabel", "Stop Build") )
				.ContentPadding( 5 )
				.OnClicked( this, &SBuildProgressWidget::OnStopBuild )
			]
		]
	];

	// Reset progress indicators
	BuildStartTime = -1;
	bStoppingBuild = false;
	SetBuildStatusText( FText::GetEmpty() );
	SetBuildProgressPercent( 0, 100 );
}

FText SBuildProgressWidget::OnGetProgressText() const
{
	return ProgressStatusText;
}

void SBuildProgressWidget::UpdateProgressText()
{
	if( ProgressNumerator > 0 && ProgressDenominator > 0 )
	{
		FFormatNamedArguments Args;
		Args.Add( TEXT("StatusText"), BuildStatusText );
		Args.Add( TEXT("ProgressCompletePercentage"), FText::AsPercent( (float)ProgressNumerator/ProgressDenominator) );
		ProgressStatusText = FText::Format( NSLOCTEXT("BuildProgress", "ProgressStatusFormat", "{StatusText} ({ProgressCompletePercentage})"), Args );
	}
	else
	{
		ProgressStatusText = BuildStatusText;
	}
}

FText SBuildProgressWidget::OnGetBuildTimeText() const
{
	// Only show a percentage if there is something interesting to report
	return BuildStatusTime; 
}

TOptional<float> SBuildProgressWidget::OnGetProgressFraction() const
{
	// Only show a percentage if there is something interesting to report
	if( ProgressNumerator > 0 && ProgressDenominator > 0 )
	{
		return (float)ProgressNumerator/ProgressDenominator;
	}
	else
	{
		// Return non-value to indicate marquee mode
		// for the progress bar.
		return TOptional<float>();
	}
}

void SBuildProgressWidget::SetBuildType(EBuildType InBuildType)
{
	BuildType = InBuildType;
}

FText SBuildProgressWidget::BuildElapsedTimeText() const
{
	// Display elapsed build time.
	return FText::AsTimespan( FDateTime::Now() - BuildStartTime );
}

void SBuildProgressWidget::UpdateTime()
{
	BuildStatusTime = BuildElapsedTimeText();
}

void SBuildProgressWidget::SetBuildStatusText( const FText& StatusText )
{
	UpdateTime();
	
	// Only update the text if we haven't canceled the build.
	if( !bStoppingBuild )
	{
		BuildStatusText = StatusText;
		UpdateProgressText();
	}
}

void SBuildProgressWidget::SetBuildProgressPercent( int32 InProgressNumerator, int32 InProgressDenominator )
{
	UpdateTime();

	// Only update the progress bar if we haven't canceled the build.
	if( !bStoppingBuild )
	{
		ProgressNumerator = InProgressNumerator;
		ProgressDenominator = InProgressDenominator;
		UpdateProgressText();
	}
}

void SBuildProgressWidget::MarkBuildStartTime()
{
	BuildStartTime = FDateTime::Now();
}

FReply SBuildProgressWidget::OnStopBuild()
{
	FUnrealEdMisc::Get().SetMapBuildCancelled( true );
	
	SetBuildStatusText( NSLOCTEXT("UnrealEd", "StoppingMapBuild", "Stopping Map Build...") );

	bStoppingBuild = true;

	return FReply::Handled();
}
