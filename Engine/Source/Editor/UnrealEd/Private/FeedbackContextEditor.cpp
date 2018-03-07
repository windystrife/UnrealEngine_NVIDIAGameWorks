// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "FeedbackContextEditor.h"
#include "HAL/PlatformSplash.h"
#include "Modules/ModuleManager.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "Editor.h"
#include "Dialogs/SBuildProgress.h"
#include "Interfaces/IMainFrameModule.h"
#include "HAL/PlatformApplicationMisc.h"

/** Called to cancel the slow task activity */
DECLARE_DELEGATE( FOnCancelClickedDelegate );

/**
 * Simple "slow task" widget
 */
class SSlowTaskWidget : public SBorder
{
	/** The maximum number of secondary bars to show on the widget */
	static const int32 MaxNumSecondaryBars = 3;

	/** The width of the dialog, and horizontal padding */
	static const int32 FixedWidth = 600, FixedPaddingH = 24;

	/** The heights of the progress bars on this widget */
	static const int32 MainBarHeight = 12, SecondaryBarHeight = 3;
public:
	SLATE_BEGIN_ARGS( SSlowTaskWidget )	{ }

		/** Called to when an asset is clicked */
		SLATE_EVENT( FOnCancelClickedDelegate, OnCancelClickedDelegate )

		/** The feedback scope stack that we are presenting to the user */
		SLATE_ARGUMENT( TWeakPtr<FSlowTaskStack>, ScopeStack )

	SLATE_END_ARGS()

	/** Construct this widget */
	void Construct( const FArguments& InArgs )
	{
		OnCancelClickedDelegate = InArgs._OnCancelClickedDelegate;
		WeakStack = InArgs._ScopeStack;

		// This is a temporary widget that needs to be updated over its entire lifespan => has an active timer registered for its entire lifespan
		RegisterActiveTimer( 0.f, FWidgetActiveTimerDelegate::CreateSP( this, &SSlowTaskWidget::UpdateProgress ) );

		TSharedRef<SVerticalBox> VerticalBox = SNew(SVerticalBox)

			// Construct the main progress bar and text
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.AutoHeight()
				.Padding(FMargin(0, 0, 0, 5.f))
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.HeightOverride(24.f)
					[
						SNew(SHorizontalBox)

						+ SHorizontalBox::Slot()
						[
							SNew( STextBlock )
							.AutoWrapText(true)
							.Text( this, &SSlowTaskWidget::GetProgressText, 0 )
							// The main font size dynamically changes depending on the content
							.Font( this, &SSlowTaskWidget::GetMainTextFont )
						]

						+ SHorizontalBox::Slot()
						.Padding(FMargin(5.f, 0, 0, 0))
						.AutoWidth()
						[
							SNew( STextBlock )
							.Text( this, &SSlowTaskWidget::GetPercentageText )
							// The main font size dynamically changes depending on the content
							.Font( FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Light.ttf"), 14, EFontHinting::AutoLight ) )
						]
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				[
					SNew(SBox)
					.HeightOverride(MainBarHeight)
					[
						SNew(SProgressBar)
						.BorderPadding(FVector2D::ZeroVector)
						.Percent( this, &SSlowTaskWidget::GetProgressFraction, 0 )
						.BackgroundImage( FEditorStyle::GetBrush("ProgressBar.ThinBackground") )
						.FillImage( FEditorStyle::GetBrush("ProgressBar.ThinFill") )
					]
				]
			]
			
			// Secondary progress bars
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(FMargin(0.f, 8.f, 0.f, 0.f))
			[
				SAssignNew(SecondaryBars, SVerticalBox)
			];
		

		if ( OnCancelClickedDelegate.IsBound() )
		{
			VerticalBox->AddSlot()
				.AutoHeight()
				.HAlign(HAlign_Center)
				.Padding(10.0f, 7.0f)
				[
					SNew(SButton)
					.Text( NSLOCTEXT("FeedbackContextProgress", "Cancel", "Cancel") )
					.HAlign(EHorizontalAlignment::HAlign_Center)
					.OnClicked(this, &SSlowTaskWidget::OnCancel)
				];
		}

		SBorder::Construct( SBorder::FArguments()
			.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
			.VAlign(VAlign_Center)
			.Padding(FMargin(FixedPaddingH))
			[
				SNew(SBox).
				WidthOverride(FixedWidth) 
				[
					VerticalBox 
				]
			]
		);

		// Make sure all our bars are set up
		UpdateDynamicProgressBars();
	}

private:

	/** Active timer to update the progress bars */
	EActiveTimerReturnType UpdateProgress(double InCurrentTime, float InDeltaTime)
	{
		UpdateDynamicProgressBars();

		return EActiveTimerReturnType::Continue;
	}

	/** Updates the dynamic progress bars for this widget */
	void UpdateDynamicProgressBars()
	{
		auto ScopeStack = WeakStack.Pin();
		if (!ScopeStack.IsValid())
		{
			return;
		}

		static const double VisibleScopeThreshold = 0.5;

		DynamicProgressIndices.Reset();
		
		// Always show the first one
		DynamicProgressIndices.Add(0);

		for (int32 Index = 1; Index < ScopeStack->Num() && DynamicProgressIndices.Num() <= MaxNumSecondaryBars - 1; ++Index)
		{
			const auto* Scope = (*ScopeStack)[Index];

			if (Scope->Visibility == ESlowTaskVisibility::ForceVisible)
			{
				DynamicProgressIndices.Add(Index);
			}
			else if (Scope->Visibility == ESlowTaskVisibility::Default && !Scope->DefaultMessage.IsEmpty())
			{
				const auto TimeOpen = FPlatformTime::Seconds() - Scope->StartTime;
				const auto WorkDone = ScopeStack->GetProgressFraction(Index);

				// We only show visible scopes if they have been opened a while, and have a reasonable amount of work left
				if (WorkDone * TimeOpen > VisibleScopeThreshold)
				{
					DynamicProgressIndices.Add(Index);
				}
			}
		}

		// Create progress bars for anything that we haven't cached yet
		// We don't destroy old widgets, they just remain ghosted until shown again
		for (int32 Index = SecondaryBars->GetChildren()->Num() + 1; Index < DynamicProgressIndices.Num(); ++Index)
		{
			CreateSecondaryBar(Index);
		}
	}

	/** Create a progress bar for the specified index */
	void CreateSecondaryBar(int32 Index) 
	{
		SecondaryBars->AddSlot()
		.Padding( 0.f, 16.f, 0.f, 0.f )
		[
			SNew(SVerticalBox)
			.Visibility( this, &SSlowTaskWidget::GetSecondaryBarVisibility, Index )
			+ SVerticalBox::Slot()
			.Padding( 0.f, 0.f, 0.f, 4.f )
			.AutoHeight()
			[
				SNew( STextBlock )
				.Text( this, &SSlowTaskWidget::GetProgressText, Index )
				.Font( FSlateFontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Regular.ttf"), 9, EFontHinting::AutoLight ) )
				.ColorAndOpacity( FSlateColor::UseSubduedForeground() )
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBox)
				.HeightOverride(SecondaryBarHeight)
				[
					SNew(SBorder)
					.Padding(0)
					.BorderImage(FEditorStyle::GetBrush("NoBorder"))
					.ColorAndOpacity( this, &SSlowTaskWidget::GetSecondaryProgressBarTint, Index )
					[
						SNew(SProgressBar)
						.BorderPadding(FVector2D::ZeroVector)
						.Percent( this, &SSlowTaskWidget::GetProgressFraction, Index )
						.BackgroundImage( FEditorStyle::GetBrush("ProgressBar.ThinBackground") )
						.FillImage( FEditorStyle::GetBrush("ProgressBar.ThinFill") )
					]
				]
			]
		];
	}

private:

	/** The main text that we will display in the window */
	FText GetPercentageText() const
	{
		auto ScopeStack = WeakStack.Pin();
		if (ScopeStack.IsValid())
		{
			const float ProgressInterp = ScopeStack->GetProgressFraction(0);
			return FText::AsPercent(ProgressInterp);
		}
		return FText();
	}

	/** Calculate the best font to display the main text with */
	FSlateFontInfo GetMainTextFont() const
	{
		TSharedRef<FSlateFontMeasure> MeasureService = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();

		const int32 MaxFontSize = 14;
		FSlateFontInfo FontInfo( FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Light.ttf"), MaxFontSize, EFontHinting::AutoLight );

		const FText MainText = GetProgressText(0);
		const int32 MaxTextWidth = FixedWidth - FixedPaddingH*2;
		while( FontInfo.Size > 9 && MeasureService->Measure(MainText, FontInfo).X > MaxTextWidth )
		{
			FontInfo.Size -= 4;
		}

		return FontInfo;
	}

	/** Get the tint for a secondary progress bar */
	FLinearColor GetSecondaryProgressBarTint(int32 Index) const
	{
		auto ScopeStack = WeakStack.Pin();
		if (ScopeStack.IsValid())
		{
			if (!DynamicProgressIndices.IsValidIndex(Index) || !ScopeStack->IsValidIndex(DynamicProgressIndices[Index]))
			{
				return FLinearColor::White.CopyWithNewOpacity(0.25f);
			}
		}
		return FLinearColor::White;
	}

	/** Get the fractional percentage of completion for a progress bar */
	TOptional<float> GetProgressFraction(int32 Index) const
	{
		auto ScopeStack = WeakStack.Pin();
		if (ScopeStack.IsValid())
		{
			if (DynamicProgressIndices.IsValidIndex(Index) && ScopeStack->IsValidIndex(DynamicProgressIndices[Index]))
			{
				return ScopeStack->GetProgressFraction(DynamicProgressIndices[Index]);
			}
		}
		return TOptional<float>();
	}

	/** Get the text to display for a progress bar */
	FText GetProgressText(int32 Index) const
	{
		auto ScopeStack = WeakStack.Pin();
		if (ScopeStack.IsValid())
		{
			if (DynamicProgressIndices.IsValidIndex(Index) && ScopeStack->IsValidIndex(DynamicProgressIndices[Index]))
			{
				return (*ScopeStack)[DynamicProgressIndices[Index]]->GetCurrentMessage();
			}
		}

		return FText();
	}

	EVisibility GetSecondaryBarVisibility(int32 Index) const
	{
		return DynamicProgressIndices.IsValidIndex(Index) ? EVisibility::HitTestInvisible : EVisibility::Collapsed;
	}

	/** Called when the cancel button is clicked */
	FReply OnCancel()
	{
		OnCancelClickedDelegate.ExecuteIfBound();
		return FReply::Handled();
	}

private:

	/** Delegate to invoke if the user clicks cancel */
	FOnCancelClickedDelegate OnCancelClickedDelegate;

	/** The scope stack that we are reflecting */
	TWeakPtr<FSlowTaskStack> WeakStack;

	/** The vertical box containing the secondary progress bars */
	TSharedPtr<SVerticalBox> SecondaryBars;

	/** Array mapping progress bar index -> scope stack index. Updated every tick. */
	TArray<int32> DynamicProgressIndices;
};

/** Static integer definitions required on some builds where the linker needs access to these */
const int32 SSlowTaskWidget::MaxNumSecondaryBars;
const int32 SSlowTaskWidget::FixedWidth;
const int32 SSlowTaskWidget::FixedPaddingH;;
const int32 SSlowTaskWidget::MainBarHeight;
const int32 SSlowTaskWidget::SecondaryBarHeight;

static void TickSlate(TSharedPtr<SWindow> SlowTaskWindow)
{
	// Avoid re-entrancy by ticking the active modal window again. This can happen if thhe slow task window is open and a sibling modal window is open as well.  We only tick slate if we are the active modal window or a child of the active modal window
	if( SlowTaskWindow.IsValid() && ( FSlateApplication::Get().GetActiveModalWindow() == SlowTaskWindow || SlowTaskWindow->IsDescendantOf( FSlateApplication::Get().GetActiveModalWindow() ) ) )
	{
		// Tick Slate application
		FSlateApplication::Get().Tick();

		// Sync the game thread and the render thread. This is needed if many StatusUpdate are called
		FSlateApplication::Get().GetRenderer()->Sync();
	}
}

FFeedbackContextEditor::FFeedbackContextEditor()
	: HasTaskBeenCancelled(false)
{
	
}

void FFeedbackContextEditor::Serialize( const TCHAR* V, ELogVerbosity::Type Verbosity, const class FName& Category )
{
	if( !GLog->IsRedirectingTo( this ) )
	{
		GLog->Serialize( V, Verbosity, Category );
	}
}

void FFeedbackContextEditor::StartSlowTask( const FText& Task, bool bShowCancelButton )
{
	FFeedbackContext::StartSlowTask( Task, bShowCancelButton );

	// Attempt to parent the slow task window to the slate main frame
	TSharedPtr<SWindow> ParentWindow;
	if( FModuleManager::Get().IsModuleLoaded( "MainFrame" ) )
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>( "MainFrame" );
		ParentWindow = MainFrame.GetParentWindow();
	}

	if (GIsEditor && ParentWindow.IsValid())
	{
		GSlowTaskOccurred = GIsSlowTask;

		// Don't show the progress dialog if the Build Progress dialog is already visible
		bool bProgressWindowShown = BuildProgressWidget.IsValid();

		// Don't show the progress dialog if a Slate menu is currently open
		const bool bHaveOpenMenu = FSlateApplication::Get().AnyMenusVisible();

		if( bHaveOpenMenu )
		{
			UE_LOG(LogSlate, Warning, TEXT("Prevented a slow task dialog from being summoned while a context menu was open") );
		}

		// reset the cancellation flag
		HasTaskBeenCancelled = false;

		if (!bProgressWindowShown && !bHaveOpenMenu && FSlateApplication::Get().CanDisplayWindows())
		{
			FOnCancelClickedDelegate OnCancelClicked;
			if ( bShowCancelButton )
			{
				// The cancel button is only displayed if a delegate is bound to it.
				OnCancelClicked = FOnCancelClickedDelegate::CreateRaw(this, &FFeedbackContextEditor::OnUserCancel);
			}

			const bool bFocusAndActivate = FPlatformApplicationMisc::IsThisApplicationForeground();

			TSharedRef<SWindow> SlowTaskWindowRef = SNew(SWindow)
				.SizingRule(ESizingRule::Autosized)
				.AutoCenter(EAutoCenter::PreferredWorkArea)
				.IsPopupWindow(true)
				.CreateTitleBar(true)
				.ActivationPolicy(bFocusAndActivate ? EWindowActivationPolicy::Always : EWindowActivationPolicy::Never)
				.FocusWhenFirstShown(bFocusAndActivate);

			SlowTaskWindowRef->SetContent(
				SNew(SSlowTaskWidget)
				.ScopeStack(ScopeStack)
				.OnCancelClickedDelegate( OnCancelClicked )
			);

			SlowTaskWindow = SlowTaskWindowRef;

			const bool bSlowTask = true;
			FSlateApplication::Get().AddModalWindow( SlowTaskWindowRef, ParentWindow, bSlowTask );

			SlowTaskWindowRef->ShowWindow();

			TickSlate(SlowTaskWindow.Pin());
		}

		FPlatformSplash::SetSplashText( SplashTextType::StartupProgress, *Task.ToString() );
	}
}

void FFeedbackContextEditor::FinalizeSlowTask()
{
	auto Window = SlowTaskWindow.Pin();
	if (Window.IsValid())
	{
		Window->SetContent(SNullWidget::NullWidget);
		Window->RequestDestroyWindow();
		SlowTaskWindow.Reset();
	}

	FFeedbackContext::FinalizeSlowTask( );
}

void FFeedbackContextEditor::ProgressReported( const float TotalProgressInterp, FText DisplayMessage )
{
	if (!(FPlatformSplash::IsShown() || BuildProgressWidget.IsValid() || SlowTaskWindow.IsValid()))
	{
		return;
	}

	// Clean up deferred cleanup objects from rendering thread every once in a while.
	static double LastTimePendingCleanupObjectsWhereDeleted;
	if( FPlatformTime::Seconds() - LastTimePendingCleanupObjectsWhereDeleted > 1 )
	{
		// Get list of objects that are pending cleanup.
		FPendingCleanupObjects* PendingCleanupObjects = GetPendingCleanupObjects();
		// Flush rendering commands in the queue.
		FlushRenderingCommands();
		// It is now safe to delete the pending clean objects.
		delete PendingCleanupObjects;
		// Keep track of time this operation was performed so we don't do it too often.
		LastTimePendingCleanupObjectsWhereDeleted = FPlatformTime::Seconds();
	}

	if (BuildProgressWidget.IsValid() || SlowTaskWindow.IsValid())
	{
		// CanDisplayWindows can be slow when called repeatedly, so we only call it if a window is open
		if (!FSlateApplication::Get().CanDisplayWindows())
		{
			return;
		}

		if (BuildProgressWidget.IsValid())
		{
			if (!DisplayMessage.IsEmpty())
			{
				BuildProgressWidget->SetBuildStatusText(DisplayMessage);
			}

			BuildProgressWidget->SetBuildProgressPercent(TotalProgressInterp * 100, 100);
			TickSlate(BuildProgressWindow.Pin());
		}
		else if (SlowTaskWindow.IsValid())
		{
			TickSlate(SlowTaskWindow.Pin());
		}
	}
	else if (FPlatformSplash::IsShown())
	{
		// Always show the top-most message
		for (auto& Scope : *ScopeStack)
		{
			const FText ThisMessage = Scope->GetCurrentMessage();
			if (!ThisMessage.IsEmpty())
			{
				DisplayMessage = ThisMessage;
				break;
			}
		}

		if (!DisplayMessage.IsEmpty())
		{
			const int32 DotCount = 4;
			const float MinTimeBetweenUpdates = 0.2f;
			static double LastUpdateTime = -100000.0;
			static int32 DotProgress = 0;
			const double CurrentTime = FPlatformTime::Seconds();
			if( CurrentTime - LastUpdateTime >= MinTimeBetweenUpdates )
			{
				LastUpdateTime = CurrentTime;
				DotProgress = ( DotProgress + 1 ) % DotCount;
			}

			FString NewDisplayMessage = DisplayMessage.ToString();
			NewDisplayMessage.RemoveFromEnd( TEXT( "..." ) );
			for( int32 DotIndex = 0; DotIndex <= DotCount; ++DotIndex )
			{
				if( DotIndex <= DotProgress )
				{
					NewDisplayMessage.AppendChar( TCHAR( '.' ) );
				}
				else
				{
					NewDisplayMessage.AppendChar( TCHAR( ' ' ) );
				}				
			}
			NewDisplayMessage.Append( FString::Printf( TEXT( " %i%%" ), int(TotalProgressInterp * 100.f) ) );
			DisplayMessage = FText::FromString( NewDisplayMessage );
		}

		FPlatformSplash::SetSplashText(SplashTextType::StartupProgress, *DisplayMessage.ToString());
	}
}

/** Whether or not the user has canceled out of this dialog */
bool FFeedbackContextEditor::ReceivedUserCancel( void )
{
	const bool res = HasTaskBeenCancelled;
	HasTaskBeenCancelled = false;
	return res;
}

void FFeedbackContextEditor::OnUserCancel()
{
	HasTaskBeenCancelled = true;
}

/** 
 * Show the Build Progress Window 
 * @return Handle to the Build Progress Widget created
 */
TWeakPtr<class SBuildProgressWidget> FFeedbackContextEditor::ShowBuildProgressWindow()
{
	TSharedRef<SWindow> BuildProgressWindowRef = SNew(SWindow)
		.ClientSize(FVector2D(500,200))
		.IsPopupWindow(true);

	BuildProgressWidget = SNew(SBuildProgressWidget);
		
	BuildProgressWindowRef->SetContent( BuildProgressWidget.ToSharedRef() );

	BuildProgressWindow = BuildProgressWindowRef;
				
	// Attempt to parent the slow task window to the slate main frame
	TSharedPtr<SWindow> ParentWindow;

	if( FModuleManager::Get().IsModuleLoaded( "MainFrame" ) )
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>( "MainFrame" );
		ParentWindow = MainFrame.GetParentWindow();
	}

	FSlateApplication::Get().AddModalWindow( BuildProgressWindowRef, ParentWindow, true );
	BuildProgressWindowRef->ShowWindow();	

	BuildProgressWidget->MarkBuildStartTime();
	
	if (FSlateApplication::Get().CanDisplayWindows())
	{
		TickSlate(BuildProgressWindow.Pin());
	}

	return BuildProgressWidget;
}

/** Close the Build Progress Window */
void FFeedbackContextEditor::CloseBuildProgressWindow()
{
	if( BuildProgressWindow.IsValid() && BuildProgressWidget.IsValid())
	{
		BuildProgressWindow.Pin()->RequestDestroyWindow();
		BuildProgressWindow.Reset();
		BuildProgressWidget.Reset();	
	}
}

bool FFeedbackContextEditor::IsPlayingInEditor() const
{
	return (GIsPlayInEditorWorld || (GEditor && GEditor->PlayWorld != nullptr));
}
