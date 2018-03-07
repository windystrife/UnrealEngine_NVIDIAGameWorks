// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "LoginFlowPrivate.h"

struct FBrowserContextSettings;

/**
 * Represents the view transformation for the login flow.
 */
class FLoginFlowViewModel 
	: public TSharedFromThis<FLoginFlowViewModel>
{
public:
	virtual ~FLoginFlowViewModel() {}

	/** @return the starting web page */
	virtual FString GetLoginFlowUrl() = 0;

	/**
	 * Handle a request to close the browser window
	 *
	 * @param CloseInfo reason for the closure
	 */
	virtual void HandleRequestClose(const FString& CloseInfo) = 0;

	/**
	 * Handle a browser window load error
	 */
	virtual void HandleLoadError() = 0;
	
	/**
	 * Browser window URL has changed
	 *
	 * @param Url new location
	 */
	virtual bool HandleBrowserUrlChanged(const FText& Url) = 0;

	virtual bool HandleBeforeBrowse(const FString& Url) = 0;

	virtual bool HandleNavigation(const FString& Url) = 0;

	/** @return the context settings for the underlying browser */
	virtual const TSharedPtr<FBrowserContextSettings>& GetBrowserContextSettings() const = 0;
};

FACTORY(TSharedRef<FLoginFlowViewModel>, FLoginFlowViewModel,
		 const FString& HomePage,
		 const TSharedPtr<FBrowserContextSettings>& BrowserContextSettings,
		 const FOnLoginFlowRequestClose& OnRequestClose,
		 const FOnLoginFlowError& OnError,
	     const FOnLoginFlowRedirectURL& OnRedirectURL);
