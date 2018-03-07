// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IWebBrowserWindow.h"

class IWebBrowserAdapter
{
public:

	virtual FString GetName() const = 0;

	virtual bool IsPermanent() const = 0;

	virtual void ConnectTo(const TSharedRef<IWebBrowserWindow>& BrowserWindow) = 0;

	virtual void DisconnectFrom(const TSharedRef<IWebBrowserWindow>& BrowserWindow) = 0;

};

class WEBBROWSER_API FWebBrowserAdapterFactory 
{ 
public: 

	static TSharedRef<IWebBrowserAdapter> Create(const FString& Name, UObject* JSBridge, bool IsPermanent); 

	static TSharedRef<IWebBrowserAdapter> Create(const FString& Name, UObject* JSBridge, bool IsPermanent, const FString& ConnectScriptText, const FString& DisconnectScriptText);
}; 
