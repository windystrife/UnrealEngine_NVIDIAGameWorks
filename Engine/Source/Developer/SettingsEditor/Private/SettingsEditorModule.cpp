// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "ISettingsContainer.h"
#include "ISettingsEditorModel.h"
#include "Models/SettingsEditorModel.h"
#include "Widgets/SWidget.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/SSettingsEditor.h"

#include "ISettingsEditorModule.h"
#include "ISettingsModule.h"
#include "Engine/DeveloperSettings.h"

#define LOCTEXT_NAMESPACE "SSettingsEditor"

/** Holds auto discovered settings information so that they can be unloaded automatically when refreshing. */
struct FRegisteredSettings
{
	FName ContainerName;
	FName CategoryName;
	FName SectionName;
};


/** Manages the notification for when the application needs to be restarted due to a settings change */
class FApplicationRestartRequiredNotification
{
public:
	void SetOnRestartApplicationCallback( FSimpleDelegate InRestartApplicationDelegate )
	{
		RestartApplicationDelegate = InRestartApplicationDelegate;
	}

	void OnRestartRequired()
	{
		TSharedPtr<SNotificationItem> NotificationPin = NotificationPtr.Pin();
		if (NotificationPin.IsValid() || !RestartApplicationDelegate.IsBound())
		{
			return;
		}
		
		FNotificationInfo Info( LOCTEXT("RestartRequiredTitle", "Restart required to apply new settings") );

		// Add the buttons with text, tooltip and callback
		Info.ButtonDetails.Add(FNotificationButtonInfo(
			LOCTEXT("RestartNow", "Restart Now"), 
			LOCTEXT("RestartNowToolTip", "Restart now to finish applying your new settings."), 
			FSimpleDelegate::CreateRaw(this, &FApplicationRestartRequiredNotification::OnRestartClicked))
			);
		Info.ButtonDetails.Add(FNotificationButtonInfo(
			LOCTEXT("RestartLater", "Restart Later"), 
			LOCTEXT("RestartLaterToolTip", "Dismiss this notificaton without restarting. Some new settings will not be applied."), 
			FSimpleDelegate::CreateRaw(this, &FApplicationRestartRequiredNotification::OnDismissClicked))
			);

		// We will be keeping track of this ourselves
		Info.bFireAndForget = false;

		// Set the width so that the notification doesn't resize as its text changes
		Info.WidthOverride = 300.0f;

		Info.bUseLargeFont = false;
		Info.bUseThrobber = false;
		Info.bUseSuccessFailIcons = false;

		// Launch notification
		NotificationPtr = FSlateNotificationManager::Get().AddNotification(Info);
		NotificationPin = NotificationPtr.Pin();

		if (NotificationPin.IsValid())
		{
			NotificationPin->SetCompletionState(SNotificationItem::CS_Pending);
		}
	}



private:
	void OnRestartClicked()
	{
		TSharedPtr<SNotificationItem> NotificationPin = NotificationPtr.Pin();
		if (NotificationPin.IsValid())
		{
			NotificationPin->SetText(LOCTEXT("RestartingNow", "Restarting..."));
			NotificationPin->SetCompletionState(SNotificationItem::CS_Success);
			NotificationPin->ExpireAndFadeout();
			NotificationPtr.Reset();
		}

		RestartApplicationDelegate.ExecuteIfBound();
	}

	void OnDismissClicked()
	{
		TSharedPtr<SNotificationItem> NotificationPin = NotificationPtr.Pin();
		if (NotificationPin.IsValid())
		{
			NotificationPin->SetText(LOCTEXT("RestartDismissed", "Restart Dismissed..."));
			NotificationPin->SetCompletionState(SNotificationItem::CS_None);
			NotificationPin->ExpireAndFadeout();
			NotificationPtr.Reset();
		}
	}

	/** Used to reference to the active restart notification */
	TWeakPtr<SNotificationItem> NotificationPtr;

	/** Used to actually restart the application */
	FSimpleDelegate RestartApplicationDelegate;
};


/**
 * Implements the SettingsEditor module.
 */
class FSettingsEditorModule
	: public ISettingsEditorModule
{
public:

	FSettingsEditorModule()
		: bAreSettingsStale(true)
	{
	}

	// IModuleInterface interface

	virtual void StartupModule() override
	{
		FModuleManager::Get().OnModulesChanged().AddRaw(this, &FSettingsEditorModule::ModulesChangesCallback);
	}

	virtual void ShutdownModule() override
	{
		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

		if ( SettingsModule != nullptr )
		{
			UnregisterAutoDiscoveredSettings(*SettingsModule);
		}

		FModuleManager::Get().OnModulesChanged().RemoveAll(this);
	}

	// ISettingsEditorModule interface

	virtual TSharedRef<SWidget> CreateEditor( const TSharedRef<ISettingsEditorModel>& Model ) override
	{
		UpdateSettings(true);

		TSharedRef<SWidget> Editor = SNew(SSettingsEditor, Model)
			.OnApplicationRestartRequired(FSimpleDelegate::CreateRaw(this, &FSettingsEditorModule::OnApplicationRestartRequired));

		ClearStaleEditorWidgets();
		EditorWidgets.Add(Editor);

		return Editor;
	}

	virtual ISettingsEditorModelRef CreateModel( const TSharedRef<ISettingsContainer>& SettingsContainer ) override
	{
		return MakeShareable(new FSettingsEditorModel(SettingsContainer));
	}

	virtual void OnApplicationRestartRequired() override
	{
		ApplicationRestartRequiredNotification.OnRestartRequired();
	}

	virtual void SetRestartApplicationCallback( FSimpleDelegate InRestartApplicationDelegate ) override
	{
		ApplicationRestartRequiredNotification.SetOnRestartApplicationCallback(InRestartApplicationDelegate);
	}

private:

	void ModulesChangesCallback(FName ModuleName, EModuleChangeReason ReasonForChange)
	{
		ClearStaleEditorWidgets();
		bAreSettingsStale = true;
		UpdateSettings();
	}

	void UpdateSettings(bool bForce = false)
	{
		if ( ( AnyActiveSettingsEditor() || bForce ) && bAreSettingsStale )
		{
			bAreSettingsStale = false;

			ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

			if ( SettingsModule != nullptr )
			{
				UnregisterAutoDiscoveredSettings(*SettingsModule);
				RegisterAutoDiscoveredSettings(*SettingsModule);
			}
		}
	}

	void ClearStaleEditorWidgets()
	{
		for ( int32 i = 0; i < EditorWidgets.Num(); i++ )
		{
			if ( !EditorWidgets[i].IsValid() )
			{
				EditorWidgets.RemoveAtSwap(i);
				i--;
			}
		}
	}

	bool AnyActiveSettingsEditor()
	{
		return EditorWidgets.Num() > 0;
	}

private:

	void RegisterAutoDiscoveredSettings(ISettingsModule& SettingsModule)
	{
		// Find game object
		for ( TObjectIterator<UDeveloperSettings> SettingsIt(RF_NoFlags); SettingsIt; ++SettingsIt )
		{
			if ( UDeveloperSettings* Settings = *SettingsIt )
			{
				// Only Add the CDO of any UDeveloperSettings objects.
				if ( Settings->HasAnyFlags(RF_ClassDefaultObject) && !Settings->GetClass()->HasAnyClassFlags(CLASS_Deprecated | CLASS_Abstract) )
				{
					// Ignore the setting if it's specifically the UDeveloperSettings or other abstract settings classes
					if ( Settings->GetClass()->HasAnyClassFlags(CLASS_Abstract) || !Settings->SupportsAutoRegistration() )
					{
						continue;
					}

					FRegisteredSettings Registered;
					Registered.ContainerName = Settings->GetContainerName();
					Registered.CategoryName = Settings->GetCategoryName();
					Registered.SectionName = Settings->GetSectionName();

					TSharedPtr<SWidget> CustomWidget = Settings->GetCustomSettingsWidget();
					if ( CustomWidget.IsValid() )
					{
						// Add Settings
						SettingsModule.RegisterSettings(Registered.ContainerName, Registered.CategoryName, Registered.SectionName,
							Settings->GetSectionText(),
							Settings->GetSectionDescription(),
							CustomWidget.ToSharedRef()
							);
					}
					else
					{
						// Add Settings
						SettingsModule.RegisterSettings(Registered.ContainerName, Registered.CategoryName, Registered.SectionName,
							Settings->GetSectionText(),
							Settings->GetSectionDescription(),
							Settings
							);
					}

					AutoDiscoveredSettings.Add(Registered);
				}
			}
		}
	}

	void UnregisterAutoDiscoveredSettings(ISettingsModule& SettingsModule)
	{
		// Unregister any auto discovers settings.
		for ( const FRegisteredSettings& Settings : AutoDiscoveredSettings )
		{
			SettingsModule.UnregisterSettings(Settings.ContainerName, Settings.CategoryName, Settings.SectionName);
		}

		AutoDiscoveredSettings.Reset();
	}

private:

	FApplicationRestartRequiredNotification ApplicationRestartRequiredNotification;

	/** The list of auto discovered settings that need to be unregistered. */
	TArray<FRegisteredSettings> AutoDiscoveredSettings;

	/** Living editor widgets that have been handed out. */
	TArray< TWeakPtr<SWidget> > EditorWidgets;

	/** Flag if the settings are stale currently and need to be refreshed. */
	bool bAreSettingsStale;
};


IMPLEMENT_MODULE(FSettingsEditorModule, SettingsEditor);


#undef LOCTEXT_NAMESPACE
