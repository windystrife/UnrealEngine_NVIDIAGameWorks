// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "ILoginFlowModule.h"
#include "ILoginFlowManager.h"
#include "OnlineExternalUIInterface.h"

class FUniqueNetId;
class FOnlineSubsystemMcp;
class SWidget;
struct FLoginFlowProperties;

/**
 * Create and configure one of these to enable web login flow in your application
 *
 * OnlineSubsystemFacebook and OnlineSubsystemGoogle for Windows requires this
 */
class FLoginFlowManager
	: public ILoginFlowManager
{
public:
	FLoginFlowManager();
	~FLoginFlowManager();

	//~ Begin ILoginFlowManager interface
	virtual bool AddLoginFlow(FName OnlineIdentifier, const FOnDisplayPopup& InPopupDelegate, bool bPersistCookies) override;
	virtual void CancelLoginFlow() override;
	virtual void Reset() override;
	//~ End ILoginFlowManager interface

private:

	/** Delegate fired by the web engine on any error */
	void OnLoginFlow_Error(ELoginFlowErrorResult ErrorType, const FString& ErrorInfo, FString InstanceId);
	/** Delegate fired when the browser window is closed */
	void OnLoginFlow_Close(const FString& CloseInfo, FString InstanceId);
	/** Delegate fired when the browser window indicates a URL redirect */
	bool OnLoginFlow_RedirectURL(const FString& RedirectURL, FString InstanceId);
	/** Delegate fired when a login flow is requested by an external provider */
	void OnLoginFlowStarted(const FString& RequestedURL, const FOnLoginRedirectURL& OnRedirectURL, const FOnLoginFlowComplete& OnLoginFlowComplete, bool& bOutShouldContinueLogin, FName InOnlineIdentifier);
	/** Finish login flow, notifying listeners */
	void FinishLogin();

	/** Delegate fired by online identity when a logout/cleanup is requested */
	void OnLoginFlowLogout(const TArray<FString>& LoginDomains, FName OnlineIdentifier);

private:

	struct FOnlineParams
	{
		/** Online identifier <subsystem>:<instancename> that describes the OnlineSubsystem */
		FName OnlineIdentifier;
		/** Single-cast delegate instance (bind to this to handle display) */
		FOnDisplayPopup OnDisplayPopup;
		/** Handle to bound login flow ui required delegate */
		FDelegateHandle LoginFlowUIRequiredDelegateHandle;
		/** Handle to bound login flow logout delegate */
		FDelegateHandle LoginFlowLogoutDelegateHandle;
		/** Optional browser context settings if bPersistCookies is false */
		TSharedPtr<FBrowserContextSettings> BrowserContextSettings;
	};

	/** Mapping of online subsystem identifiers to the parameters they have setup for login flow */
	TMap<FName, FOnlineParams> OnlineSubsystemsMap;

	/** Is there a login attempt in progress */
	bool bLoginFlowInProgress;
	/** Properties related to the current login attempt */
	TSharedPtr<FLoginFlowProperties> PendingLogin;
};

