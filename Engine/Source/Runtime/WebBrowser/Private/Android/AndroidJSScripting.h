// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "WebJSFunction.h"
#include "WebJSScripting.h"

typedef TSharedRef<class FAndroidJSScripting> FAndroidJSScriptingRef;
typedef TSharedPtr<class FAndroidJSScripting> FAndroidJSScriptingPtr;

/**
 * Implements handling of bridging UObjects client side with JavaScript renderer side.
 */
class FAndroidJSScripting
	: public FWebJSScripting
	, public TSharedFromThis<FAndroidJSScripting>
{
public:
	static const FString JSMessageTag;

	FAndroidJSScripting(bool bJSBindingToLoweringEnabled);

	virtual void BindUObject(const FString& Name, UObject* Object, bool bIsPermanent = true) override;
	virtual void UnbindUObject(const FString& Name, UObject* Object = nullptr, bool bIsPermanent = true) override;

	/**
	 * Called when a message was received from the browser process.
	 *
	 * @param Command The command sent from the browser.
	 * @param Params Command-specific data.
	 * @return true if the message was handled, else false.
	 */
	bool OnJsMessageReceived(const FString& Command, const TArray<FString>& Params, const FString& Origin);

	FString ConvertStruct(UStruct* TypeInfo, const void* StructPtr);
	FString ConvertObject(UObject* Object);

	virtual void InvokeJSFunction(FGuid FunctionId, int32 ArgCount, FWebJSParam Arguments[], bool bIsError=false) override;
	virtual void InvokeJSErrorResult(FGuid FunctionId, const FString& Error) override;
	void PageLoaded(TSharedRef<class FAndroidWebBrowserWindow> InWindow); // Called on page load
private:
	void InvokeJSFunctionRaw(FGuid FunctionId, const FString& JSValue, bool bIsError=false);
	bool IsValid()
	{
		return WindowPtr.Pin().IsValid();
	}

	/** Message handling helpers */

	bool HandleExecuteUObjectMethodMessage(const TArray<FString>& Params);

	/** Pointer to the Android Browser for this window. */
	TWeakPtr<class FAndroidWebBrowserWindow> WindowPtr;
};
