// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SSurvey.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SScrollBox.h"
#include "EditorStyleSet.h"
#include "SurveyPage.h"
#include "SSurveyPage.h"

#define LOCTEXT_NAMESPACE "EpicSurvey"

void SSurvey::Construct( const FArguments& Args, const TSharedRef< FEpicSurvey >& InEpicSurvey, const TSharedRef< FSurvey >& InSurvey )
{
	EpicSurvey = InEpicSurvey;
	Survey = InSurvey;
	PageBox = NULL;

	TitleFont = MakeShareable( new FTextBlockStyle() );
	TitleFont->SetFont( FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 18 ) );
	TitleFont->SetColorAndOpacity( FLinearColor::White );
	TitleFont->SetShadowOffset( FVector2D( 1,1 ) );
	TitleFont->SetShadowColorAndOpacity( FLinearColor::Black );

	ChildSlot
	[
		SAssignNew( ScrollBox, SScrollBox )
		+SScrollBox::Slot()
		[
			SNew( SHorizontalBox )
			+SHorizontalBox::Slot()
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				// Drop shadow border
				SNew(SBorder)
				.Padding( 4.0f )
				.BorderImage( FEditorStyle::GetBrush("ContentBrowser.ThumbnailShadow") )
				[
					SAssignNew( ContentsContainer, SBorder )
					.BorderImage( FEditorStyle::GetBrush( "Docking.Tab.ContentAreaBrush" ) )
					.Padding(0)
				]
			]
			+SHorizontalBox::Slot()
		]
	];

	bool bIsLoaded = false;
	if ( InSurvey->GetInitializationState() == EContentInitializationState::NotStarted )
	{
		InSurvey->Initialize();
		ConstructLoadingLayout();
	}
	else if ( InSurvey->GetInitializationState() == EContentInitializationState::Working )
	{
		ConstructLoadingLayout();
	}
	else if ( InSurvey->GetInitializationState() == EContentInitializationState::Success )
	{
		ConstructSurveyLayout();
		bIsLoaded = true;
	}
	else if ( InSurvey->GetInitializationState() == EContentInitializationState::Failure )
	{
		ConstructFailureLayout();
		bIsLoaded = true;
	}

	if (!bIsLoaded)
	{
		//Construct the proper layout when finished loading
		RegisterActiveTimer( 0.1f, FWidgetActiveTimerDelegate::CreateSP( this, &SSurvey::MonitorLoadStatePostConstruct ) );
	}
}

EActiveTimerReturnType SSurvey::MonitorLoadStatePostConstruct( double InCurrentTime, float InDeltaTime )
{
	EContentInitializationState::Type State = Survey->GetInitializationState();

	bool bIsLoaded = false;
	if ( State == EContentInitializationState::Success )
	{
		bIsLoaded = true;
		ConstructSurveyLayout();
	}
	else if ( State == EContentInitializationState::Failure )
	{
		//Show Error message
		bIsLoaded = true;
		ConstructFailureLayout();
	}

	return bIsLoaded ? EActiveTimerReturnType::Stop : EActiveTimerReturnType::Continue;
}

void SSurvey::ConstructLoadingLayout()
{
	ContentsContainer->SetContent( 
		SNew( SVerticalBox )
		+SVerticalBox::Slot()
		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign( HAlign_Center )
		.VAlign( VAlign_Center )
		[
			SNew( SBox )
			.WidthOverride( 800.0f )
			.HeightOverride( 555.0f )
			.Padding( FMargin( 50, 50 ) )
			.VAlign( VAlign_Center )
			[
				SNew( SProgressBar )
			]
		]
		+SVerticalBox::Slot()
	);
}

void SSurvey::ConstructSurveyLayout()
{
	TSharedPtr< SVerticalBox > VerticalBox;

	ContentsContainer->SetContent(
		SAssignNew( VerticalBox, SVerticalBox )
		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign( HAlign_Center )
		[
			SNew( SImage )
			.Image( Survey->GetBanner() )
		]

		+SVerticalBox::Slot()
		.AutoHeight()
		.Padding( FMargin( 5, 25, 5, 0 ) )
		[
			SNew( SBorder )
			.BorderImage( FEditorStyle::GetBrush("NoBorder") )
			.VAlign( VAlign_Center )
			[
				SNew( SHorizontalBox )
				+SHorizontalBox::Slot()
				.VAlign(VAlign_Bottom)
				[
					SNew( SVerticalBox )
					+SVerticalBox::Slot()
					.Padding( 5, 0 )
					.AutoHeight()
					.VAlign( VAlign_Center )
					[
						SNew( STextBlock )
						.Text( Survey->GetDisplayName() )
						.TextStyle( TitleFont.Get() )
						.WrapTextAt( 900 )
					]

					+SVerticalBox::Slot()
					.Padding( 5, 0 )
					.VAlign( VAlign_Center )
					[
						SNew( STextBlock )
						.Text( Survey->GetInstructions() )
						.ColorAndOpacity( FSlateColor::UseSubduedForeground() )
						.WrapTextAt( 900 )
					]
				]
			]
		]

		+SVerticalBox::Slot()
		.Padding( 5, 5 )
		.AutoHeight()
		.VAlign( VAlign_Center )
		[
			SNew( SSeparator )
		]

		+SVerticalBox::Slot()
		.Expose(PageBox)
		.Padding( 5, 5 )
		.AutoHeight()
		.VAlign( VAlign_Center )


		+SVerticalBox::Slot()
		.Padding( 5, 5 )
		.AutoHeight()
		.VAlign( VAlign_Center )
		[
			SNew( SSeparator )
		]

		+SVerticalBox::Slot()
		.VAlign( VAlign_Bottom )
		.Padding( 15, 5, 15, 15 )
		.AutoHeight()
		[
			SNew(SHorizontalBox)
			+SHorizontalBox::Slot()
			.FillWidth( 1.0f )
			[
				SNew( SButton )
				.Text( NSLOCTEXT( "EpicSurvey", "PageBackBtn", "Back" ) )
				.IsEnabled( Survey.Get(), &FSurvey::CanPageBack )
				.Visibility( this, &SSurvey::CanPageBack )
				.OnClicked( this, &SSurvey::PageBack )
				.HAlign( HAlign_Center )
			]

			+SHorizontalBox::Slot()
			.Padding( 200, 0, 200, 0 )
			.FillWidth( 1.0f )
			[
				SNew( SButton )
				.Text( NSLOCTEXT( "EpicSurvey", "SubmitSurveyBtn", "Submit" ) )
				.IsEnabled( Survey.Get(), &FSurvey::IsReadyToSubmit )
				.Visibility( this, &SSurvey::CanSubmitSurvey )
				.OnClicked( this, &SSurvey::SubmitSurvey )
				.HAlign( HAlign_Center )
			]

			+SHorizontalBox::Slot()
			.FillWidth( 1.0f )
			[
				SNew( SButton )
				.Text( NSLOCTEXT( "EpicSurvey", "PageNextBtn", "Next" ) )
				.IsEnabled( Survey.Get(), &FSurvey::CanPageNext )
				.Visibility( this, &SSurvey::CanPageNext )
				.OnClicked( this, &SSurvey::PageNext )
				.HAlign( HAlign_Center )
			]
		]
	);

	// Display the current page.
	DisplayPage( Survey->GetCurrentPage() );
}

void SSurvey::ConstructFailureLayout()
{
	ContentsContainer->SetContent( 
		SNew( SVerticalBox )
		+SVerticalBox::Slot()
		+SVerticalBox::Slot()
		.AutoHeight()
		.HAlign( HAlign_Center )
		.VAlign( VAlign_Center )
		[
			SNew( SBox )
			.WidthOverride( 800.0f )
			.HeightOverride( 555.0f )
			.Padding( FMargin( 50, 50 ) )
			.VAlign( VAlign_Center )
			[
				SNew( SVerticalBox )
				+SVerticalBox::Slot()
				.AutoHeight()
				.HAlign( HAlign_Center )
				[
					SNew( STextBlock )
					.Text( LOCTEXT( "FailureMessage01" , "We seem to be having some problems :(" ) )
					.Font( FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 18 ) )
				]
			]
		]
		+SVerticalBox::Slot()
	);
}

FReply SSurvey::SubmitSurvey()
{
	EpicSurvey->SubmitSurvey( Survey.ToSharedRef() );
	
	return FReply::Handled();
}

EVisibility SSurvey::CanSubmitSurvey() const
{
	EVisibility CanSubmit = EVisibility::Hidden;
	if ( Survey.IsValid() && Survey->IsReadyToSubmit() )
	{
		CanSubmit = EVisibility::Visible;
	}
	return CanSubmit;
}

EVisibility SSurvey::CanPageNext() const
{
	EVisibility CanPageNext = EVisibility::Hidden;
	if ( Survey.IsValid() && Survey->CanPageNext() )
	{
		CanPageNext = EVisibility::Visible;
	}
	return CanPageNext;
}

EVisibility SSurvey::CanPageBack() const
{
	EVisibility CanPageBack = EVisibility::Hidden;
	if ( Survey.IsValid() && Survey->CanPageBack() )
	{
		CanPageBack = EVisibility::Visible;
	}
	return CanPageBack;
}

FReply SSurvey::PageNext()
{
	if ( Survey.IsValid() && Survey->CanPageNext() )
	{
		int32 CurrentPageIndex = Survey->GetCurrentPage();

		DisplayPage( ++CurrentPageIndex );
		
		Survey->OnPageNext();
	}

	return FReply::Handled();
}

FReply SSurvey::PageBack()
{
	if( Survey.IsValid() && Survey->GetCurrentPage() > 0 && Survey->CanPageBack() )
	{
		Survey->OnPageBack();

		DisplayPage( Survey->GetCurrentPage()-1 );
	}

	return FReply::Handled();
}

void SSurvey::DisplayPage( int32 NewPageIndex )
{
	if( Survey.IsValid() )
	{
		Survey->SetCurrentPage(NewPageIndex);
		if( PageBox )
		{
			TArray< TSharedRef< FSurveyPage > > Pages = Survey->GetPages();
			if( Survey->GetCurrentPage() < Pages.Num() )
			{
				ScrollBox->SetScrollOffset( 0.0f );

				(*PageBox)
				[
					SNew( SSurveyPage, Pages[NewPageIndex] )
				];
			}
		}
	}
}

FReply SSurvey::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	// Mouse back and forward buttons traverse history
	if ( MouseEvent.GetEffectingButton() == EKeys::ThumbMouseButton)
	{
		return PageBack();
	}
	else if ( MouseEvent.GetEffectingButton() == EKeys::ThumbMouseButton2)
	{
		return PageNext();
	}

	return FReply::Unhandled();
}


#undef LOCTEXT_NAMESPACE
