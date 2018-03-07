// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Frame/MainFrameHandler.h"
#include "HAL/FileManager.h"
#include "ThumbnailRendering/ThumbnailManager.h"
#include "Frame/RootWindowLocation.h"
#include "Widgets/Docking/SDockTab.h"
#include "HAL/PlatformApplicationMisc.h"

void FMainFrameHandler::ShutDownEditor()
{
	FEditorDelegates::OnShutdownPostPackagesSaved.Broadcast();

	// Any pending autosaves should not happen.  A tick will go by before the editor shuts down and we want to avoid auto-saving during this time.
	GUnrealEd->GetPackageAutoSaver().ResetAutoSaveTimer();
	GEditor->RequestEndPlayMap();

	// End any play on console/PC games still happening
	GEditor->EndPlayOnLocalPc();

	// Cancel any current Launch On in progress
	GEditor->CancelPlayingViaLauncher();
	
	//Broadcast we are closing the editor
	GEditor->BroadcastEditorClose();

	TSharedPtr<SWindow> RootWindow = RootWindowPtr.Pin();

	// Save root window placement so we can restore it.
	if (RootWindow.IsValid())
	{
		FSlateRect WindowRect = RootWindow->GetNonMaximizedRectInScreen();

		if (!RootWindow->HasOSWindowBorder())
		{
			// If the window has a specified border size, shrink its screen size by that amount to prevent it from growing
			// over multiple shutdowns
			const FMargin WindowBorder = RootWindow->GetNonMaximizedWindowBorderSize();
			WindowRect.Right -= WindowBorder.Left + WindowBorder.Right;
			WindowRect.Bottom -= WindowBorder.Top + WindowBorder.Bottom;
		}

		// Save without any DPI Scale so we can save the position and scale in a DPI independent way
		const float DPIScale = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(WindowRect.Left, WindowRect.Top);

		FRootWindowLocation RootWindowLocation(FVector2D(WindowRect.Left, WindowRect.Top)/ DPIScale, WindowRect.GetSize() / DPIScale, RootWindow->IsWindowMaximized());
		RootWindowLocation.SaveToIni();
	}

	// Save the visual state of the editor before we even
	// ask whether we can shut down.
	TSharedRef<FGlobalTabmanager> GlobalTabManager = FGlobalTabmanager::Get();
	if (FUnrealEdMisc::Get().IsSavingLayoutOnClosedAllowed())
	{
		GlobalTabManager->SaveAllVisualState();
	}
	else
	{
		GConfig->EmptySection(TEXT("EditorLayouts"), *GEditorLayoutIni);
	}

	// Clear the callback for destructionfrom the main tab; otherwise it will re-enter this shutdown function.
	if (MainTabPtr.IsValid())
	{
		MainTabPtr.Pin()->SetOnTabClosed(SDockTab::FOnTabClosedCallback());
	}

	// Inform the AssetEditorManager that the editor is exiting so that it may save open assets
	// and report usage stats
	FAssetEditorManager::Get().OnExit();

	if (RootWindow.IsValid())
	{
		RootWindow->SetRequestDestroyWindowOverride(FRequestDestroyWindowOverride());
		RootWindow->RequestDestroyWindow();
	}

	// Save out any config settings for the editor so they don't get lost
	GEditor->SaveConfig();
	GLevelEditorModeTools().SaveConfig();

	// Delete user settings, if requested
	if (FUnrealEdMisc::Get().IsDeletePreferences())
	{
		IFileManager::Get().Delete(*GEditorPerProjectIni);
	}

	// Take a screenshot of this project for the project browser
	if (FApp::HasProjectName())
	{
		const FString ExistingBaseFilename = FString(FApp::GetProjectName()) + TEXT(".png");
		const FString ExistingScreenshotFilename = FPaths::Combine(*FPaths::ProjectDir(), *ExistingBaseFilename);

		// If there is already a screenshot, no need to take an auto screenshot
		if (!FPaths::FileExists(ExistingScreenshotFilename))
		{
			const FString ScreenShotFilename = FPaths::Combine(*FPaths::ProjectSavedDir(), TEXT("AutoScreenshot.png"));
			FViewport* Viewport = GEditor->GetActiveViewport();
			if (Viewport)
			{
				UThumbnailManager::CaptureProjectThumbnail(Viewport, ScreenShotFilename, false);
			}
		}
	}

	// Shut down the editor
	// NOTE: We can't close the editor from within this stack frame as it will cause various DLLs
	//       (such as MainFrame) to become unloaded out from underneath the code pointer.  We'll shut down
	//       as soon as it's safe to do so.
	// Note this is the only place in slate that should be calling QUIT_EDITOR
	GEngine->DeferredCommands.Add(TEXT("QUIT_EDITOR"));
}

void FMainFrameHandler::EnableTabClosedDelegate()
{
	if (MainTabPtr.IsValid())
	{
		MainTabPtr.Pin()->SetOnTabClosed(SDockTab::FOnTabClosedCallback::CreateRaw(this, &FMainFrameHandler::ShutDownEditor));
		MainTabPtr.Pin()->SetCanCloseTab(SDockTab::FCanCloseTab::CreateRaw(this, &FMainFrameHandler::CanCloseTab));
	}
}

void FMainFrameHandler::DisableTabClosedDelegate()
{
	if (MainTabPtr.IsValid())
	{
		MainTabPtr.Pin()->SetOnTabClosed(SDockTab::FOnTabClosedCallback());
		MainTabPtr.Pin()->SetCanCloseTab(SDockTab::FCanCloseTab());
	}
}
