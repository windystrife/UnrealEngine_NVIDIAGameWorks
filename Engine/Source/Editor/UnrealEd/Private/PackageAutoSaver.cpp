// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PackageAutoSaver.h"
#include "UObject/Package.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "Misc/App.h"
#include "Dom/JsonValue.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Policies/PrettyJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Framework/Application/SlateApplication.h"
#include "EditorStyleSet.h"
#include "Editor/UnrealEdEngine.h"
#include "Settings/EditorLoadingSavingSettings.h"
#include "EditorModeManager.h"
#include "EditorModes.h"
#include "UnrealEdMisc.h"
#include "FileHelpers.h"
#include "UnrealEdGlobals.h"
#include "PackageRestore.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "AutoSaveUtils.h"
#include "ShaderCompiler.h"
#include "EditorLevelUtils.h"
#include "IVREditorModule.h"

namespace PackageAutoSaverJson
{
	typedef TCHAR CharType;

	typedef TJsonWriter< CharType, TPrettyJsonPrintPolicy<CharType> > FStringWriter;
	typedef TJsonWriterFactory< CharType, TPrettyJsonPrintPolicy<CharType> > FStringWriterFactory;

	typedef TJsonReader<CharType> FJsonReader;
	typedef TJsonReaderFactory<CharType> FJsonReaderFactory;

	static const FString TagRestoreEnabled	= TEXT("RestoreEnabled");
	static const FString TagPackages		= TEXT("Packages");
	static const FString TagPackagePathName = TEXT("PackagePathName");
	static const FString TagAutoSavePath	= TEXT("AutoSavePath");
	static const FString RestoreFilename	= TEXT("PackageRestoreData.json");

	/**
	 * @param bEnsurePath True to ensure that the directory for the restore file exists

	 * @return Returns the full path to the restore file
	 */
	FString GetRestoreFilename(const bool bEnsurePath);

	/**
	 * Load the restore file from disk (if present)
	 *
	 * @return The packages that have auto-saves that they can be restored from
	 */
	TMap<FString, FString> LoadRestoreFile();

	/**
	 * Save the file on disk that's used to restore auto-saved packages in the event of a crash
	 * 
	 * @param bRestoreEnabled	Is the restore enabled, or is it disabled because we've shut-down cleanly, or are running under the debugger?
	 * @param DirtyPackages		Packages that may have auto-saves that they could be restored from
	 */
	void SaveRestoreFile(const bool bRestoreEnabled, const TMap< TWeakObjectPtr<UPackage>, FString >& DirtyPackages);

	/** @return whether the auto-save restore should be enabled (you can force this to true when testing with a debugger attached) */
	bool IsRestoreEnabled()
	{
		// Note: Restore is disabled when running under the debugger, as programmers
		// like to just kill applications and we don't want this to count as a crash
		return !FPlatformMisc::IsDebuggerPresent();
	}
}


/************************************************************************/
/* FPackageAutoSaver                                                    */
/************************************************************************/

DEFINE_LOG_CATEGORY_STATIC(PackageAutoSaver, Log, All);

FPackageAutoSaver::FPackageAutoSaver()
	: AutoSaveIndex(0)
	, AutoSaveCount(0.0f)
	, bIsAutoSaving(false)
	, bDelayingDueToFailedSave(false)
{
	const UEditorLoadingSavingSettings* LoadingSavingSettings = GetDefault<UEditorLoadingSavingSettings>();

	// Register for the package dirty state updated callback to catch packages that have been cleaned without being saved
	UPackage::PackageDirtyStateChangedEvent.AddRaw(this, &FPackageAutoSaver::OnPackageDirtyStateUpdated);

	// Register for the "MarkPackageDirty" callback to catch packages that have been modified and need to be saved
	UPackage::PackageMarkedDirtyEvent.AddRaw(this, &FPackageAutoSaver::OnMarkPackageDirty);

	// Register for the package modified callback to catch packages that have been saved
	UPackage::PackageSavedEvent.AddRaw(this, &FPackageAutoSaver::OnPackageSaved);
}

FPackageAutoSaver::~FPackageAutoSaver()
{
	UPackage::PackageDirtyStateChangedEvent.RemoveAll(this);
	UPackage::PackageMarkedDirtyEvent.RemoveAll(this);
	UPackage::PackageSavedEvent.RemoveAll(this);
}

void FPackageAutoSaver::UpdateAutoSaveCount(const float DeltaSeconds)
{
	const UEditorLoadingSavingSettings* LoadingSavingSettings = GetDefault<UEditorLoadingSavingSettings>();

	const float AutoSaveWarningTime = FMath::Max(0.0f, static_cast<float>(LoadingSavingSettings->AutoSaveTimeMinutes * 60 - LoadingSavingSettings->AutoSaveWarningInSeconds));

	// Make sure we don't skip the auto-save warning when debugging the editor. 
	if (AutoSaveCount < AutoSaveWarningTime && (AutoSaveCount + DeltaSeconds) > AutoSaveWarningTime)
	{
		AutoSaveCount = AutoSaveWarningTime;
	}
	else
	{
		AutoSaveCount += DeltaSeconds;
	}
}

void FPackageAutoSaver::ResetAutoSaveTimer()
{
	// Reset the "seconds since last auto-save" counter.
	AutoSaveCount = 0.0f;
}

void FPackageAutoSaver::ForceAutoSaveTimer()
{
	AutoSaveCount = GetDefault<UEditorLoadingSavingSettings>()->AutoSaveTimeMinutes * 60.0f;
}

void FPackageAutoSaver::ForceMinimumTimeTillAutoSave(const float TimeTillAutoSave)
{
	const float MinumumTime = (GetDefault<UEditorLoadingSavingSettings>()->AutoSaveTimeMinutes * 60.0f) - TimeTillAutoSave;
	AutoSaveCount = (MinumumTime < AutoSaveCount) ? MinumumTime : AutoSaveCount;
}

void FPackageAutoSaver::AttemptAutoSave()
{
	const UEditorLoadingSavingSettings* LoadingSavingSettings = GetDefault<UEditorLoadingSavingSettings>();
	FUnrealEdMisc& UnrealEdMisc = FUnrealEdMisc::Get();

	// Don't auto-save if disabled or if it is not yet time to auto-save.
	const bool bTimeToAutosave = (LoadingSavingSettings->bAutoSaveEnable && AutoSaveCount >= LoadingSavingSettings->AutoSaveTimeMinutes * 60.0f);
	bool bAutosaveHandled = false;

	if (bTimeToAutosave)
	{
		ClearStalePointers();

		// If we don't need to perform an auto-save, then just reset the timer and bail
		const bool bNeedsAutoSave = DoPackagesNeedAutoSave();
		if(!bNeedsAutoSave)
		{
			ResetAutoSaveTimer();
			return;
		}

		// Don't auto-save during interpolation editing, if there's another slow task
		// already in progress, or while a PIE world is playing or when doing automated tests.
		const bool bCanAutosave = CanAutoSave();
		if (bCanAutosave)
		{
			FScopedSlowTask SlowTask(100.f, NSLOCTEXT("AutoSaveNotify", "PerformingAutoSave_Caption", "Auto-saving out of date packages..."));
			SlowTask.MakeDialog();

			bAutosaveHandled = true;

			bIsAutoSaving = true;
			UnrealEdMisc.SetAutosaveState(FUnrealEdMisc::EAutosaveState::Saving);

			GUnrealEd->SaveConfig();

			// Make sure the auto-save directory exists before attempting to write the file
			const FString AutoSaveDir = AutoSaveUtils::GetAutoSaveDir();
			IFileManager::Get().MakeDirectory(*AutoSaveDir, true);

			// Auto-save maps and/or content packages based on user settings.
			const int32 NewAutoSaveIndex = (AutoSaveIndex + 1) % 10;

			bool bLevelSaved = false;
			auto MapsSaveResults = EAutosaveContentPackagesResult::NothingToDo;
			auto AssetsSaveResults = EAutosaveContentPackagesResult::NothingToDo;

			SlowTask.EnterProgressFrame(50);

			if (LoadingSavingSettings->bAutoSaveMaps)
			{
				MapsSaveResults = FEditorFileUtils::AutosaveMapEx(AutoSaveDir, NewAutoSaveIndex, false, DirtyMapsForAutoSave);
				if (MapsSaveResults == EAutosaveContentPackagesResult::Success)
				{
					DirtyMapsForAutoSave.Empty();
				}
			}

			SlowTask.EnterProgressFrame(50);

			if (LoadingSavingSettings->bAutoSaveContent && UnrealEdMisc.GetAutosaveState() != FUnrealEdMisc::EAutosaveState::Cancelled)
			{
				AssetsSaveResults = FEditorFileUtils::AutosaveContentPackagesEx(AutoSaveDir, NewAutoSaveIndex, false, DirtyContentForAutoSave);
				if (AssetsSaveResults == EAutosaveContentPackagesResult::Success)
				{
					DirtyContentForAutoSave.Empty();
				}
			}

			const bool bNothingToDo = (MapsSaveResults == EAutosaveContentPackagesResult::NothingToDo && AssetsSaveResults == EAutosaveContentPackagesResult::NothingToDo);
			const bool bSuccess = (MapsSaveResults != EAutosaveContentPackagesResult::Failure && AssetsSaveResults != EAutosaveContentPackagesResult::Failure && !bNothingToDo);
			const bool bFailure = (MapsSaveResults == EAutosaveContentPackagesResult::Failure || AssetsSaveResults == EAutosaveContentPackagesResult::Failure);

			// Auto-saved, so close any warning notifications.
			CloseAutoSaveNotification(
				bSuccess ?		ECloseNotification::Success		:
				bFailure ?		ECloseNotification::Failed		:
				bNothingToDo ?	ECloseNotification::NothingToDo	:
								ECloseNotification::Postponed);

			if (bSuccess)
			{
				// If a level was actually saved, update the auto-save index
				AutoSaveIndex = NewAutoSaveIndex;

				// Update the restore information
				UpdateRestoreFile(PackageAutoSaverJson::IsRestoreEnabled());
			}

			ResetAutoSaveTimer();
			bDelayingDueToFailedSave = false;

			if (UnrealEdMisc.GetAutosaveState() == FUnrealEdMisc::EAutosaveState::Cancelled)
			{
				UE_LOG(PackageAutoSaver, Warning, TEXT("Autosave was cancelled."));
			}

			bIsAutoSaving = false;
			UnrealEdMisc.SetAutosaveState(FUnrealEdMisc::EAutosaveState::Inactive);
		}
		else
		{
			bDelayingDueToFailedSave = true;

			// Extend the time by 3 seconds if we failed to save because the user was interacting.  
			// We do this to avoid cases where they are rapidly clicking and are interrupted by autosaves
			AutoSaveCount = (LoadingSavingSettings->AutoSaveTimeMinutes*60.0f) - 3.0f;

			TSharedPtr<SNotificationItem> NotificationItem = AutoSaveNotificationPtr.Pin();

			// ensure the notification exists
			if (NotificationItem.IsValid())
			{
				// update notification
				NotificationItem->SetText(NSLOCTEXT("AutoSaveNotify", "WaitingToPerformAutoSave", "Waiting to perform Auto-save..."));
			}
		}
	}


	// The auto save notification must always be ticked, 
	// so as to correctly handle pausing and resetting.
	if (!bAutosaveHandled)
	{
		UpdateAutoSaveNotification();
	}
}

void FPackageAutoSaver::LoadRestoreFile()
{
	PackagesThatCanBeRestored = PackageAutoSaverJson::LoadRestoreFile();
}

void FPackageAutoSaver::UpdateRestoreFile(const bool bRestoreEnabled) const
{
	PackageAutoSaverJson::SaveRestoreFile(bRestoreEnabled, DirtyPackagesForUserSave);
}

bool FPackageAutoSaver::HasPackagesToRestore() const
{
	// Don't offer to restore packages during automation testing; the dlg is modal and blocks
	return !GIsAutomationTesting && PackagesThatCanBeRestored.Num() > 0;
}

void FPackageAutoSaver::OfferToRestorePackages()
{
	bool bRemoveRestoreFile = true;

	if(HasPackagesToRestore())
	{
		// If we failed to restore, keep the restore information around
		if(PackageRestore::PromptToRestorePackages(PackagesThatCanBeRestored) == FEditorFileUtils::PR_Failure)
		{
			bRemoveRestoreFile = false;
		}
	}

	if(bRemoveRestoreFile)
	{
		// We've finished restoring, so remove this file to avoid being prompted about it again
		UpdateRestoreFile(false);
	}
}

void FPackageAutoSaver::OnPackagesDeleted(const TArray<UPackage*>& DeletedPackages)
{
	ClearStalePointers();

	for(UPackage* DeletedPackage : DeletedPackages)
	{
		DirtyMapsForAutoSave.Remove(DeletedPackage);
		DirtyContentForAutoSave.Remove(DeletedPackage);
		DirtyPackagesForUserSave.Remove(DeletedPackage);
	}

	UpdateRestoreFile(PackageAutoSaverJson::IsRestoreEnabled());
}

void FPackageAutoSaver::OnPackageDirtyStateUpdated(UPackage* Pkg)
{
	UpdateDirtyListsForPackage(Pkg);
}

void FPackageAutoSaver::OnMarkPackageDirty(UPackage* Pkg, bool bWasDirty)
{
	UpdateDirtyListsForPackage(Pkg);
}

void FPackageAutoSaver::OnPackageSaved(const FString& Filename, UObject* Obj)
{
	UPackage* const Pkg = Cast<UPackage>(Obj);

	// If this has come from an auto-save, update the last known filename in the user dirty list so that we can offer is up as a restore file later
	if(IsAutoSaving())
	{
		FString* const AutoSaveFilename = DirtyPackagesForUserSave.Find(Pkg);
		if(AutoSaveFilename)
		{
			// Make the filename relative to the auto-save directory
			// Note: MakePathRelativeTo modifies in-place, hence the copy of Filename
			const FString AutoSaveDir = AutoSaveUtils::GetAutoSaveDir() / "";
			FString RelativeFilename = Filename;
			FPaths::MakePathRelativeTo(RelativeFilename, *AutoSaveDir);

			(*AutoSaveFilename) = RelativeFilename;
		}
	}

	UpdateDirtyListsForPackage(Pkg);
}

void FPackageAutoSaver::UpdateDirtyListsForPackage(UPackage* Pkg)
{
	const UPackage* TransientPackage = GetTransientPackage();

	// Don't auto-save the transient package or packages with the transient flag.
	if ( Pkg == TransientPackage || Pkg->HasAnyFlags(RF_Transient) || Pkg->HasAnyPackageFlags(PKG_CompiledIn) )
	{
		return;
	}

	if ( Pkg->IsDirty() )
	{
		// Always add the package to the user list
		DirtyPackagesForUserSave.FindOrAdd(Pkg);

		// Only add the package to the auto-save list if we're not auto-saving
		// Note: Packages get dirtied again after they're auto-saved, so this would add them back again, which we don't want
		if ( !IsAutoSaving() )
		{
			// Get the set of all reference worlds.
			FWorldContext& EditorContext = GEditor->GetEditorWorldContext();
			TArray<UWorld*> WorldsArray;
			EditorLevelUtils::GetWorlds(EditorContext.World(), WorldsArray, true);

			bool bPackageIsMap = false;
			for (UWorld* World : WorldsArray)
			{
				UPackage* Package = CastChecked<UPackage>(World->GetOuter());
				if (Package == Pkg)
				{
					bPackageIsMap = true;
					break;
				}
			}

			// Add package into the appropriate list (map or content)
			if (bPackageIsMap)
			{
				DirtyMapsForAutoSave.Add(Pkg);
			}
			else
			{
				DirtyContentForAutoSave.Add(Pkg);
			}
		}
	}
	else
	{
		// Always remove the package from the auto-save list
		DirtyMapsForAutoSave.Remove(Pkg);
		DirtyContentForAutoSave.Remove(Pkg);

		// Only remove the package from the user list if we're not auto-saving
		// Note: Packages call this even when auto-saving, so this would remove them from the user list, which we don't want as they're still dirty
		if ( !IsAutoSaving() )
		{
			if ( DirtyPackagesForUserSave.Remove(Pkg) )
			{
				// Update the restore information too
				UpdateRestoreFile(PackageAutoSaverJson::IsRestoreEnabled());
			}
		}
	}
}

bool FPackageAutoSaver::CanAutoSave() const
{
	// Don't allow auto-saving if the auto-save wouldn't save anything
	const bool bPackagesNeedAutoSave = DoPackagesNeedAutoSave();

	double LastInteractionTime = FSlateApplication::Get().GetLastUserInteractionTime();
	
	const float InteractionDelay = 15.0f;

	const bool bDidInteractRecently = (FApp::GetCurrentTime() - LastInteractionTime) < InteractionDelay;
	const bool bAutosaveEnabled	= GetDefault<UEditorLoadingSavingSettings>()->bAutoSaveEnable && bPackagesNeedAutoSave;
	const bool bSlowTask = GIsSlowTask;
	const bool bInterpEditMode = GLevelEditorModeTools().IsModeActive(FBuiltinEditorModes::EM_InterpEdit);
	const bool bPlayWorldValid = GUnrealEd->PlayWorld != nullptr;
	const bool bAnyMenusVisible	= FSlateApplication::Get().AnyMenusVisible();
	const bool bAutomationTesting = GIsAutomationTesting;
	const bool bIsInteracting = FSlateApplication::Get().HasAnyMouseCaptor() || GUnrealEd->IsUserInteracting() || (bDidInteractRecently && !bAutoSaveNotificationLaunched && !bDelayingDueToFailedSave);
	const bool bHasGameOrProjectLoaded = FApp::HasProjectName();
	const bool bAreShadersCompiling = GShaderCompilingManager->IsCompiling();
	const bool bIsVREditorActive = IVREditorModule::Get().IsVREditorEnabled();	// @todo vreditor: Eventually we should support this while in VR (modal VR progress, with sufficient early warning)

	return (bAutosaveEnabled && !bSlowTask && !bInterpEditMode && !bPlayWorldValid && !bAnyMenusVisible && !bAutomationTesting && !bIsInteracting && !GIsDemoMode && bHasGameOrProjectLoaded && !bAreShadersCompiling && !bIsVREditorActive);
}

bool FPackageAutoSaver::DoPackagesNeedAutoSave() const
{
	const UEditorLoadingSavingSettings* LoadingSavingSettings = GetDefault<UEditorLoadingSavingSettings>();

	const bool bHasDirtyMapsForAutoSave = DirtyMapsForAutoSave.Num() != 0;
	const bool bHasDirtyContentForAutoSave = DirtyContentForAutoSave.Num() != 0;
	const bool bWorldsMightBeDirty = LoadingSavingSettings->bAutoSaveMaps && bHasDirtyMapsForAutoSave;
	const bool bContentPackagesMightBeDirty = LoadingSavingSettings->bAutoSaveContent && bHasDirtyContentForAutoSave;
	const bool bPackagesNeedAutoSave = bWorldsMightBeDirty || bContentPackagesMightBeDirty;

	return bPackagesNeedAutoSave;
}

FText FPackageAutoSaver::GetAutoSaveNotificationText(const int32 TimeInSecondsUntilAutosave)
{
	// Don't switch to pending text unless auto-save really is overdue
	if(!bDelayingDueToFailedSave && TimeInSecondsUntilAutosave > -1)
	{
		const UEditorLoadingSavingSettings* LoadingSavingSettings = GetDefault<UEditorLoadingSavingSettings>();
		int32 NumPackagesToAutoSave = 0;
		if (DirtyMapsForAutoSave.Num() != 0 && LoadingSavingSettings->bAutoSaveMaps)
		{
			NumPackagesToAutoSave += DirtyMapsForAutoSave.Num();
		}
		if (DirtyContentForAutoSave.Num() != 0 && LoadingSavingSettings->bAutoSaveContent)
		{
			NumPackagesToAutoSave += DirtyContentForAutoSave.Num();
		}

		// Count down the time
		FFormatNamedArguments Args;
		Args.Add(TEXT("TimeInSecondsUntilAutosave"), TimeInSecondsUntilAutosave);
		Args.Add(TEXT("DirtyPackagesCount"), NumPackagesToAutoSave);
		return (NumPackagesToAutoSave == 1)
			? FText::Format(NSLOCTEXT("AutoSaveNotify", "AutoSaveIn", "Autosave in {TimeInSecondsUntilAutosave} seconds"), Args)
			: FText::Format(NSLOCTEXT("AutoSaveNotify", "AutoSaveXPackagesIn", "Autosave in {TimeInSecondsUntilAutosave} seconds for {DirtyPackagesCount} items"), Args);
	}

	// Auto-save is imminent 
	return NSLOCTEXT("AutoSaveNotify", "AutoSavePending", "Autosave pending");
}

int32 FPackageAutoSaver::GetTimeTillAutoSave(const bool bIgnoreCanAutoSave) const
{
	int32 Result = -1;
	if (bIgnoreCanAutoSave || CanAutoSave())
	{
		Result = FMath::CeilToInt(GetDefault<UEditorLoadingSavingSettings>()->AutoSaveTimeMinutes * 60.0f - AutoSaveCount);
	}

	return Result;
}

void FPackageAutoSaver::UpdateAutoSaveNotification()
{
	const UEditorLoadingSavingSettings* LoadingSavingSettings = GetDefault<UEditorLoadingSavingSettings>();

	const bool bIgnoreCanAutoSave = true;
	const int32 TimeInSecondsUntilAutosave = GetTimeTillAutoSave(bIgnoreCanAutoSave);

	const bool UserAllowsAutosave = LoadingSavingSettings->bAutoSaveEnable && !GIsDemoMode;
	const bool InGame = (GUnrealEd->PlayWorld != nullptr);

	if (UserAllowsAutosave && // The user has set to allow auto-save in preferences
		TimeInSecondsUntilAutosave < LoadingSavingSettings->AutoSaveWarningInSeconds && 
		!InGame && // we want to hide auto-save if we are simulating/playing
		!GLevelEditorModeTools().IsModeActive(FBuiltinEditorModes::EM_InterpEdit) // we want to hide auto-save if we are in matinee 
		)
	{		
		if (!bAutoSaveNotificationLaunched && !bDelayingDueToFailedSave)
		{
			if (CanAutoSave())
			{
				ClearStalePointers();

				// Starting a new request! Notify the UI.
				if (AutoSaveNotificationPtr.IsValid())
				{
					AutoSaveNotificationPtr.Pin()->ExpireAndFadeout();
				}

				// Setup button localized strings
				static FText AutoSaveCancelButtonText			= NSLOCTEXT("AutoSaveNotify", "AutoSaveCancel", "Cancel");
				static FText AutoSaveCancelButtonToolTipText	= NSLOCTEXT("AutoSaveNotify", "AutoSaveCancelToolTip", "Postpone Autosave");
				static FText AutoSaveSaveButtonText				= NSLOCTEXT("AutoSaveNotify", "AutoSaveSave", "Save Now");
				static FText AutoSaveSaveButtonToolTipText		= NSLOCTEXT("AutoSaveNotify", "AutoSaveSaveToolTip", "Force Autosave");

				FNotificationInfo Info( GetAutoSaveNotificationText(TimeInSecondsUntilAutosave) );
				Info.Image = FEditorStyle::GetBrush("MainFrame.AutoSaveImage");

				// Add the buttons with text, tooltip and callback
				Info.ButtonDetails.Add(FNotificationButtonInfo(AutoSaveCancelButtonText, AutoSaveCancelButtonToolTipText, FSimpleDelegate::CreateRaw(this, &FPackageAutoSaver::OnAutoSaveCancel)));
				Info.ButtonDetails.Add(FNotificationButtonInfo(AutoSaveSaveButtonText, AutoSaveSaveButtonToolTipText, FSimpleDelegate::CreateRaw(this, &FPackageAutoSaver::OnAutoSaveSave)));

				// Force the width so that any text changes don't resize the notification
				Info.WidthOverride = 240.0f;

				// We will be keeping track of this ourselves
				Info.bFireAndForget = false;

				// We want the auto-save to be subtle
				Info.bUseLargeFont = false;
				Info.bUseThrobber = false;
				Info.bUseSuccessFailIcons = false;

				// Launch notification
				AutoSaveNotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);

				if(AutoSaveNotificationPtr.IsValid())
				{
					AutoSaveNotificationPtr.Pin()->SetCompletionState(SNotificationItem::CS_Pending);
				}

				// Update launched flag
				bAutoSaveNotificationLaunched = true;
			}
			else // defer until the user finishes using pop-up menus or the notification will dismiss them...
			{
				ForceMinimumTimeTillAutoSave(LoadingSavingSettings->AutoSaveWarningInSeconds);
			}
		}
		else
		{
			// Update the remaining time on the notification
			TSharedPtr<SNotificationItem> NotificationItem = AutoSaveNotificationPtr.Pin();
			if (NotificationItem.IsValid())
			{
				// update text
				NotificationItem->SetText(GetAutoSaveNotificationText(TimeInSecondsUntilAutosave));
			}
		}
	}
	else
	{
		// Ensures notifications are cleaned up
		CloseAutoSaveNotification(false);
	}
}

void FPackageAutoSaver::CloseAutoSaveNotification(const bool Success)
{
	CloseAutoSaveNotification(Success ? ECloseNotification::Success : ECloseNotification::Postponed);
}

void FPackageAutoSaver::CloseAutoSaveNotification(ECloseNotification::Type Type)
{
	// If a notification is open close it
	if(bAutoSaveNotificationLaunched)
	{
		TSharedPtr<SNotificationItem> NotificationItem = AutoSaveNotificationPtr.Pin();

		// ensure the notification exists
		if(NotificationItem.IsValid())
		{
			SNotificationItem::ECompletionState CloseState;
			FText CloseMessage;

			// Set the test on the notification based on whether it was a successful launch
			if (Type == ECloseNotification::Success)
			{
				CloseMessage = NSLOCTEXT("AutoSaveNotify", "AutoSaving", "Saving");
				CloseState = SNotificationItem::CS_Success;
			}
			else if (Type == ECloseNotification::Postponed)
			{
				CloseMessage = NSLOCTEXT("AutoSaveNotify", "AutoSavePostponed", "Autosave postponed");
				CloseState = SNotificationItem::CS_None; // Set back to none rather than failed, as this is too harsh
			}
			else if (Type == ECloseNotification::Failed)
			{
				CloseMessage = NSLOCTEXT("AutoSaveNotify", "AutoSaveFailed", "Auto-save failed. Please check the log for the details.");
				CloseState = SNotificationItem::CS_Fail;
			}
			else
			{
				CloseMessage = NSLOCTEXT("AutoSaveNotify", "AutoSaveNothingToDo", "Already auto-saved.");
				CloseState = SNotificationItem::CS_None;
			}

			// update notification
			NotificationItem->SetText(CloseMessage);
			NotificationItem->SetCompletionState(CloseState);
			NotificationItem->ExpireAndFadeout();

			// clear reference
			AutoSaveNotificationPtr.Reset();
		}

		// Auto-save has been closed
		bAutoSaveNotificationLaunched = false;
	}
}

void FPackageAutoSaver::OnAutoSaveSave()
{
	ForceAutoSaveTimer();
	CloseAutoSaveNotification(true);
}

void FPackageAutoSaver::OnAutoSaveCancel()
{
	ResetAutoSaveTimer();
	CloseAutoSaveNotification(false);
}

void FPackageAutoSaver::ClearStalePointers()
{
	auto DirtyPackagesForUserSaveTmp = DirtyPackagesForUserSave;
	for(auto It = DirtyPackagesForUserSaveTmp.CreateConstIterator(); It; ++It)
	{
		const TWeakObjectPtr<UPackage>& Package = It->Key;
		if(!Package.IsValid())
		{
			DirtyPackagesForUserSave.Remove(Package);
		}
	}

	auto DirtyMapsForAutoSaveTmp = DirtyMapsForAutoSave;
	for(auto It = DirtyMapsForAutoSaveTmp.CreateConstIterator(); It; ++It)
	{
		const TWeakObjectPtr<UPackage>& Package = *It;
		if(!Package.IsValid())
		{
			DirtyMapsForAutoSave.Remove(Package);
		}
	}

	auto DirtyContentForAutoSaveTmp = DirtyContentForAutoSave;
	for (auto It = DirtyContentForAutoSaveTmp.CreateConstIterator(); It; ++It)
	{
		const TWeakObjectPtr<UPackage>& Package = *It;
		if (!Package.IsValid())
		{
			DirtyContentForAutoSave.Remove(Package);
		}
	}
}


/************************************************************************/
/* PackageAutoSaverJson                                                 */
/************************************************************************/

FString PackageAutoSaverJson::GetRestoreFilename(const bool bEnsurePath)
{
	const FString AutoSaveDir = AutoSaveUtils::GetAutoSaveDir();
	if(bEnsurePath)
	{
		// Make sure the auto-save directory exists before attempting to write the file
		IFileManager::Get().MakeDirectory(*AutoSaveDir, true);
	}

	const FString Filename = AutoSaveDir / RestoreFilename;
	return Filename;
}

TMap<FString, FString> PackageAutoSaverJson::LoadRestoreFile()
{
	TMap<FString, FString> PackagesThatCanBeRestored;

	const FString Filename = GetRestoreFilename(false);
	FArchive* const FileAr = IFileManager::Get().CreateFileReader(*Filename);
	if(!FileAr)
	{
		// File doesn't exist; nothing to restore
		return PackagesThatCanBeRestored;
	}

	bool bJsonLoaded = false;
	TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);
	{
		TSharedRef<FJsonReader> Reader = FJsonReaderFactory::Create(FileAr);
		bJsonLoaded = FJsonSerializer::Deserialize(Reader, RootObject);		
		FileAr->Close();
	}

	if(!bJsonLoaded || !RootObject->GetBoolField(TagRestoreEnabled))
	{
		// File failed to load, or the restore is disabled; nothing to restore
		return PackagesThatCanBeRestored;
	}

	TArray< TSharedPtr<FJsonValue> > PackagesThatCanBeRestoredArray = RootObject->GetArrayField(TagPackages);
	for(auto It = PackagesThatCanBeRestoredArray.CreateConstIterator(); It; ++It)
	{
		TSharedPtr<FJsonObject> EntryObject = (*It)->AsObject();

		const FString PackagePathName = EntryObject->GetStringField(TagPackagePathName);
		const FString AutoSavePath = EntryObject->GetStringField(TagAutoSavePath);
		PackagesThatCanBeRestored.Add(PackagePathName, AutoSavePath);
	}

	return PackagesThatCanBeRestored;
}

void PackageAutoSaverJson::SaveRestoreFile(const bool bRestoreEnabled, const TMap< TWeakObjectPtr<UPackage>, FString >& DirtyPackages)
{
	TSharedPtr<FJsonObject> RootObject = MakeShareable(new FJsonObject);

	RootObject->SetBoolField(TagRestoreEnabled, bRestoreEnabled);

	TArray< TSharedPtr<FJsonValue> > PackagesThatCanBeRestored;

	// Only bother populating the list of packages if the restore is enabled
	if(bRestoreEnabled)
	{
		PackagesThatCanBeRestored.Reserve(DirtyPackages.Num());

		// Build up the array of package names with auto-saves that can be restored
		for(auto It = DirtyPackages.CreateConstIterator(); It; ++It)
		{
			const TWeakObjectPtr<UPackage>& Package = It.Key();
			const FString& AutoSavePath = It.Value();

			UPackage* const PackagePtr = Package.Get();
			if(PackagePtr && !AutoSavePath.IsEmpty())
			{
				const FString& PackagePathName = PackagePtr->GetPathName();

				TSharedPtr<FJsonObject> EntryObject = MakeShareable(new FJsonObject);
				EntryObject->SetStringField(TagPackagePathName, PackagePathName);
				EntryObject->SetStringField(TagAutoSavePath, AutoSavePath);

				TSharedPtr<FJsonValue> EntryValue = MakeShareable(new FJsonValueObject(EntryObject));
				PackagesThatCanBeRestored.Add(EntryValue);
			}
		}
	}

	RootObject->SetArrayField(TagPackages, PackagesThatCanBeRestored);

	const FString Filename = GetRestoreFilename(true);
	FArchive* const FileAr = IFileManager::Get().CreateFileWriter(*Filename, FILEWRITE_EvenIfReadOnly);
	if(FileAr)
	{
		TSharedRef<FStringWriter> Writer = FStringWriterFactory::Create(FileAr);
		FJsonSerializer::Serialize(RootObject.ToSharedRef(), Writer);		
		FileAr->Close();
	}
}
