// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Input/Reply.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/SWindow.h"
#include "Async/AsyncResult.h"
#include "Account/IPortalUser.h"

class IPortalApplicationWindow;
class IPortalUserLogin;

extern TSet<FString> AuthorizedPlugins;

enum class EPluginAuthorizationState
{
	Initializing,
	StartLauncher,
	StartLauncher_Waiting,
	AuthorizePlugin,
	AuthorizePlugin_Waiting,
	IsUserSignedIn,
	IsUserSignedIn_Waiting,
	SigninRequired,
	SigninRequired_Waiting,
	Signin_Waiting,
	Authorized,
	Unauthorized,
	LauncherStartFailed,
	Timeout,
	Canceled,
};

/**
 * The authorizing plug-in ui guides the user through the process of certifying their access to the plug-in.
 */
class SAuthorizingPlugin : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAuthorizingPlugin){}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const TSharedRef<SWindow>& InParentWindow, const FText& InPluginFriendlyName, const FString& InPluginItemId, const FString& InPluginOfferId, TFunction<void()> InAuthorizedCallback);

private:

	FText GetWaitingText() const;

	EActiveTimerReturnType RefreshStatus(double InCurrentTime, float InDeltaTime);

	/** Called when the user presses the Cancel button. */
	FReply OnCancel();

	void OnWindowClosed(const TSharedRef<SWindow>& InWindow);

	/** Show the store page for the plug-in, happens in response to the user asking to see the store page when license detection fails. */
	void ShowStorePageForPlugin();

private:
	/** The parent window holding this dialog, for when we need to trigger a close. */
	TWeakPtr<SWindow> ParentWindow;

	/** The display name of the plug-in used in the auto generated dialog text. */
	FText PluginFriendlyName;

	/** The unique id of the item for the plug-in on the marketplace. */
	FString PluginItemId;

	/** The unique id of the offer for the plug-in on the marketplace. */
	FString PluginOfferId;

	/** Flag for tracking user interruption of the process, either with the cancel button or the close button. */
	bool bUserInterrupted;

	/** The amount of time we've been waiting for confirmation for a given step.  It's possible a problem may arise and we need to timeout. */
	float WaitingTime;

	/** The portal application communication service. */
	TSharedPtr<IPortalApplicationWindow> PortalWindowService;

	/** The portal user service, to allow us to check entitlements for plugins. */
	TSharedPtr<IPortalUser> PortalUserService;

	/** The portal user login service, to allow us to trigger a prompt to sign-in, if required. */
	TSharedPtr<IPortalUserLogin> PortalUserLoginService;

	/** The current state of the plug-in auth pipeline. */
	EPluginAuthorizationState CurrentState;

	/** The entitlement result we may be waiting on. */
	TAsyncResult<FPortalUserIsEntitledToItemResult> EntitlementResult;

	/** The result from the request for user details. */
	TAsyncResult<FPortalUserDetails> UserDetailsResult;

	/** The result from requesting the user sign-in, they may sign-in, they may cancel. */
	TAsyncResult<bool> UserSigninResult;

	/** If the user is authorized to us the plug-in, we'll call this function to alert the plug-in that everything is good to go. */
	TFunction<void()> AuthorizedCallback;
};
