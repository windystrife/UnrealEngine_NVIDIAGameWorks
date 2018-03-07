// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SAuthorizingPlugin.h"
#include "Misc/MessageDialog.h"
#include "Containers/Ticker.h"
#include "Async/TaskGraphInterfaces.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Input/SButton.h"
#include "EditorStyleSet.h"
#include "Editor.h"
#include "Widgets/Images/SThrobber.h"

#include "IPortalServiceLocator.h"
#include "Account/IPortalUserLogin.h"
#include "Application/IPortalApplicationWindow.h"

#include "ILauncherPlatform.h"
#include "LauncherPlatformModule.h"

#define LOCTEXT_NAMESPACE "PluginWarden"

void SAuthorizingPlugin::Construct(const FArguments& InArgs, const TSharedRef<SWindow>& InParentWindow, const FText& InPluginFriendlyName, const FString& InPluginItemId, const FString& InPluginOfferId, TFunction<void()> InAuthorizedCallback)
{
	CurrentState = EPluginAuthorizationState::Initializing;
	WaitingTime = 0;
	ParentWindow = InParentWindow;
	PluginFriendlyName = InPluginFriendlyName;
	PluginItemId = InPluginItemId;
	PluginOfferId = InPluginOfferId;
	AuthorizedCallback = InAuthorizedCallback;

	InParentWindow->SetOnWindowClosed(FOnWindowClosed::CreateSP(this, &SAuthorizingPlugin::OnWindowClosed));
	bUserInterrupted = true;

	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SAuthorizingPlugin::RefreshStatus));

	ChildSlot
	[
		SNew(SBox)
		.MinDesiredWidth(500)
		[
			SNew(SBorder)
			.BorderImage(FEditorStyle::GetBrush("ToolPanel.GroupBorder"))
			[
				SNew(SVerticalBox)

				+ SVerticalBox::Slot()
				.FillHeight(1.0f)
				.Padding(10, 30, 10, 20)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(SThrobber)
					]

					+ SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.Padding(10, 0)
					[
						SNew(STextBlock)
						.Text(this, &SAuthorizingPlugin::GetWaitingText)
						.Font(FSlateFontInfo(FPaths::EngineContentDir() / TEXT("Slate/Fonts/Roboto-Bold.ttf"), 12))
					]
				]

				+ SVerticalBox::Slot()
				.AutoHeight()
				.HAlign(HAlign_Right)
				.Padding(10)
				[
					SNew(SButton)
					.Text(LOCTEXT("CancelText", "Cancel"))
					.OnClicked(this, &SAuthorizingPlugin::OnCancel)
				]
			]
		]
	];

	TSharedRef<IPortalServiceLocator> ServiceLocator = GEditor->GetServiceLocator();
	PortalWindowService = ServiceLocator->GetServiceRef<IPortalApplicationWindow>();
	PortalUserService = ServiceLocator->GetServiceRef<IPortalUser>();
	PortalUserLoginService = ServiceLocator->GetServiceRef<IPortalUserLogin>();
}

FText SAuthorizingPlugin::GetWaitingText() const
{
	switch ( CurrentState )
	{
	case EPluginAuthorizationState::Initializing:
	case EPluginAuthorizationState::StartLauncher:
		return LOCTEXT("StartingLauncher", "Starting Epic Games Launcher...");
	case EPluginAuthorizationState::StartLauncher_Waiting:
		return LOCTEXT("ConnectingToLauncher", "Connecting...");
	case EPluginAuthorizationState::AuthorizePlugin:
	case EPluginAuthorizationState::AuthorizePlugin_Waiting:
		return FText::Format(LOCTEXT("CheckingIfYouCanUseFormat", "Checking license for {0}..."), PluginFriendlyName);
	case EPluginAuthorizationState::IsUserSignedIn:
	case EPluginAuthorizationState::IsUserSignedIn_Waiting:
		return LOCTEXT("CheckingIfUserSignedIn", "Authorization failed, checking user information...");
	case EPluginAuthorizationState::SigninRequired:
	case EPluginAuthorizationState::SigninRequired_Waiting:
		return LOCTEXT("NeedUserToLoginToCheck", "Authorization failed, Sign-in required...");
	case EPluginAuthorizationState::Signin_Waiting:
		return LOCTEXT("WaitingForSignin", "Waiting for Sign-in...");
	}

	return LOCTEXT("Processing", "Processing...");
}

EActiveTimerReturnType SAuthorizingPlugin::RefreshStatus(double InCurrentTime, float InDeltaTime)
{
	// Engine tick isn't running when the modal window is open, so we need to tick any core tickers
	// to as that's what the RPC system uses to update the current state of RPC calls.
	FTaskGraphInterface::Get().ProcessThreadUntilIdle(ENamedThreads::GameThread);
	FTicker::GetCoreTicker().Tick(InDeltaTime);

	switch ( CurrentState )
	{
		case EPluginAuthorizationState::Initializing:
		{
			WaitingTime = 0;
			if ( PortalWindowService->IsAvailable() && PortalUserService->IsAvailable() )
			{
				CurrentState = EPluginAuthorizationState::AuthorizePlugin;
			}
			else
			{
				CurrentState = EPluginAuthorizationState::StartLauncher;
			}
			break;
		}
		case EPluginAuthorizationState::StartLauncher:
		{
			WaitingTime = 0;
			ILauncherPlatform* LauncherPlatform = FLauncherPlatformModule::Get();

			if (LauncherPlatform != nullptr )
			{
				if ( !FPlatformProcess::IsApplicationRunning(TEXT("EpicGamesLauncher")) &&
					 !FPlatformProcess::IsApplicationRunning(TEXT("EpicGamesLauncher-Mac-Shipping")) )
				{
					FOpenLauncherOptions SilentOpen;
					if (LauncherPlatform->OpenLauncher(SilentOpen) )
					{
						CurrentState = EPluginAuthorizationState::StartLauncher_Waiting;
					}
					else
					{
						CurrentState = EPluginAuthorizationState::LauncherStartFailed;
					}
				}
				else
				{
					// If the process is found to be running already, move into the next state.
					CurrentState = EPluginAuthorizationState::StartLauncher_Waiting;
				}
			}
			else
			{
				CurrentState = EPluginAuthorizationState::LauncherStartFailed;
			}
			break;
		}
		case EPluginAuthorizationState::StartLauncher_Waiting:
		{
			if ( PortalWindowService->IsAvailable() && PortalUserService->IsAvailable() )
			{
				CurrentState = EPluginAuthorizationState::AuthorizePlugin;
			}
			else
			{
				WaitingTime += InDeltaTime;
			}
			break;
		}
		case EPluginAuthorizationState::AuthorizePlugin:
		{
			WaitingTime = 0;
			EntitlementResult = PortalUserService->IsEntitledToItem(PluginItemId, EEntitlementCacheLevelRequest::Memory);
			CurrentState = EPluginAuthorizationState::AuthorizePlugin_Waiting;
			break;
		}
		case EPluginAuthorizationState::AuthorizePlugin_Waiting:
		{
			WaitingTime += InDeltaTime;
			
			check(EntitlementResult.GetFuture().IsValid());
			if ( EntitlementResult.GetFuture().IsReady() )
			{
				FPortalUserIsEntitledToItemResult Entitlement = EntitlementResult.GetFuture().Get();
				if ( Entitlement.IsEntitled )
				{
					CurrentState = EPluginAuthorizationState::Authorized;
				}
				else
				{
					CurrentState = EPluginAuthorizationState::IsUserSignedIn;
				}
			}

			break;
		}
		case EPluginAuthorizationState::IsUserSignedIn:
		{
			WaitingTime = 0;
			UserDetailsResult = PortalUserService->GetUserDetails();
			CurrentState = EPluginAuthorizationState::IsUserSignedIn_Waiting;
			break;
		}
		case EPluginAuthorizationState::IsUserSignedIn_Waiting:
		{
			WaitingTime += InDeltaTime;

			check(UserDetailsResult.GetFuture().IsValid());
			if ( UserDetailsResult.GetFuture().IsReady() )
			{
				FPortalUserDetails UserDetails = UserDetailsResult.GetFuture().Get();

				if ( UserDetails.IsSignedIn )
				{
					// if the user is signed in, and we're at this stage, we know they are unauthorized.
					CurrentState = EPluginAuthorizationState::Unauthorized;
				}
				else
				{
					// If they're not signed in, but they were unauthorized, they may have purchased it
					// they may just need to sign-in.
					if ( PortalUserLoginService->IsAvailable() )
					{
						CurrentState = EPluginAuthorizationState::SigninRequired;
					}
				}
			}

			break;
		}
		case EPluginAuthorizationState::SigninRequired:
		{
			WaitingTime = 0;
			UserSigninResult = PortalUserLoginService->PromptUserForSignIn();
			CurrentState = EPluginAuthorizationState::SigninRequired_Waiting;
			break;
		}
		case EPluginAuthorizationState::SigninRequired_Waiting:
		{
			// We don't advance the wait time in the sign-required state, as this may take a long time.

			check(UserSigninResult.GetFuture().IsValid());
			if ( UserSigninResult.GetFuture().IsReady() )
			{
				bool IsUserSignedIn = UserSigninResult.GetFuture().Get();
				if ( IsUserSignedIn )
				{
					UserDetailsResult = PortalUserService->GetUserDetails();
					CurrentState = EPluginAuthorizationState::Signin_Waiting;
				}
				else
				{
					CurrentState = EPluginAuthorizationState::Unauthorized;
				}
			}

			break;
		}
		// We stay in the Signin_Waiting state until the user is signed in or until they cancel the
		// authorizing plug-in UI.  It would be nice to be able to know if the user closes the sign-in
		// dialog and cancel out of this dialog automatically.
		case EPluginAuthorizationState::Signin_Waiting:
		{
			WaitingTime = 0;

			check(UserDetailsResult.GetFuture().IsValid());
			if ( UserDetailsResult.GetFuture().IsReady() )
			{
				FPortalUserDetails UserDetails = UserDetailsResult.GetFuture().Get();

				if ( UserDetails.IsSignedIn )
				{
					// if the user is signed in, and we're at this stage, we know they are unauthorized.
					CurrentState = EPluginAuthorizationState::AuthorizePlugin;
				}
				else
				{
					UserDetailsResult = PortalUserService->GetUserDetails();
				}
			}

			break;
		}
		case EPluginAuthorizationState::Authorized:
		case EPluginAuthorizationState::Unauthorized:
		case EPluginAuthorizationState::Timeout:
		case EPluginAuthorizationState::LauncherStartFailed:
		{
			bUserInterrupted = false;
			ParentWindow.Pin()->RequestDestroyWindow();
			break;
		}
		case EPluginAuthorizationState::Canceled:
		{
			bUserInterrupted = true;
			ParentWindow.Pin()->RequestDestroyWindow();
			break;
		}
		default:
		{
			// Should never be in a non-explicit state.
			check(false);
			break;
		}
		}

		// If we're in a waiting state, check to see if we're over the timeout period.
		switch ( CurrentState )
		{
		case EPluginAuthorizationState::StartLauncher_Waiting:
		case EPluginAuthorizationState::AuthorizePlugin_Waiting:
		case EPluginAuthorizationState::IsUserSignedIn_Waiting:
		case EPluginAuthorizationState::SigninRequired_Waiting:
		// We Ignore EPluginAuthorizationState::Signin_Waiting, that state could take forever, the user needs to sign-in or close the dialog.
		{
			const float TimeoutSeconds = 15;
			if ( WaitingTime > TimeoutSeconds )
			{
				bUserInterrupted = false;
				CurrentState = EPluginAuthorizationState::Timeout;
			}
			break;
		}
	}

	return EActiveTimerReturnType::Continue;
}

FReply SAuthorizingPlugin::OnCancel()
{
	bUserInterrupted = true;
	ParentWindow.Pin()->RequestDestroyWindow();

	return FReply::Handled();
}

void SAuthorizingPlugin::OnWindowClosed(const TSharedRef<SWindow>& InWindow)
{
	if ( bUserInterrupted || CurrentState == EPluginAuthorizationState::Canceled )
	{
		// User interrupted or canceled, just close down.
	}
	else
	{
		if ( CurrentState == EPluginAuthorizationState::Authorized )
		{
			AuthorizedPlugins.Add(PluginItemId);
			AuthorizedCallback();
		}
		else
		{
			switch ( CurrentState )
			{
			case EPluginAuthorizationState::Timeout:
			{
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("TimeoutFailure", "Something went wrong.  We were unable to verify your access to the plugin before timing out."));
				break;
			}
			case EPluginAuthorizationState::LauncherStartFailed:
			{
				FMessageDialog::Open(EAppMsgType::Ok, LOCTEXT("LauncherStartFailure", "Something went wrong starting the launcher.  We were unable to verify your access to the plugin."));
				break;
			}
			case EPluginAuthorizationState::Unauthorized:
			{
				FText FailureMessage = FText::Format(LOCTEXT("UnathorizedFailure", "It doesn't look like you've purchased {0}.\n\nWould you like to see the store page?"), PluginFriendlyName);
				EAppReturnType::Type Response = FMessageDialog::Open(EAppMsgType::YesNo, FailureMessage);
				if ( Response == EAppReturnType::Yes )
				{
					ShowStorePageForPlugin();
				}
				break;
			}
			default:
			{
				// We expect all failure situations to have an explicit case.
				check(false);
				break;
			}
			}
		}
	}
}

void SAuthorizingPlugin::ShowStorePageForPlugin()
{
	ILauncherPlatform* LauncherPlatform = FLauncherPlatformModule::Get();

	if (LauncherPlatform != nullptr )
	{
		FOpenLauncherOptions StorePageOpen(FString(TEXT("/ue/marketplace/content/")) + PluginOfferId);
		LauncherPlatform->OpenLauncher(StorePageOpen);
	}
}

#undef LOCTEXT_NAMESPACE
