// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/Guid.h"

#if WITH_CEF3
#if PLATFORM_WINDOWS
#include "AllowWindowsPlatformTypes.h"
#include "AllowWindowsPlatformAtomics.h"
#endif

#pragma push_macro("OVERRIDE")
#undef OVERRIDE // cef headers provide their own OVERRIDE macro
THIRD_PARTY_INCLUDES_START
#include "include/cef_v8.h"
THIRD_PARTY_INCLUDES_END
#pragma pop_macro("OVERRIDE")

#if PLATFORM_WINDOWS
#include "HideWindowsPlatformAtomics.h"
#include "HideWindowsPlatformTypes.h"
#endif

#if PLATFORM_LINUX
typedef CefBase CefBaseRefCounted;
#endif

class FUnrealCEFSubProcessRemoteScripting;

class FUnrealCEFSubProcessRemoteObject
	: public CefBaseRefCounted
{
public:
	FUnrealCEFSubProcessRemoteObject(FUnrealCEFSubProcessRemoteScripting* InRemoteScripting, CefRefPtr<CefBrowser> InBrowser, const FGuid& InObjectId)
		: RemoteScripting(InRemoteScripting)
		, Browser(InBrowser)
		, ObjectId(InObjectId) {}

	virtual ~FUnrealCEFSubProcessRemoteObject();

	bool ExecuteMethod(const CefString& MethodName,
		CefRefPtr<CefV8Value> Object, const CefV8ValueList& Arguments,
		CefRefPtr<CefV8Value>& Retval, CefString& Exception);

private:
	FUnrealCEFSubProcessRemoteScripting* RemoteScripting;
	CefRefPtr<CefBrowser> Browser;
	FGuid ObjectId;

    // Include the default reference counting implementation.
    IMPLEMENT_REFCOUNTING(FUnrealCEFSubProcessRemoteObject);
};

class FUnrealCEFSubProcessRemoteMethodHandler
	: public CefV8Handler
{

public:
	FUnrealCEFSubProcessRemoteMethodHandler(CefRefPtr<FUnrealCEFSubProcessRemoteObject> InRemoteObject, CefString& InMethodName)
		: RemoteObject(InRemoteObject)
		, MethodName(InMethodName)
	{}

	virtual bool Execute(const CefString& Name,
		CefRefPtr<CefV8Value> Object, const CefV8ValueList& Arguments,
		CefRefPtr<CefV8Value>& Retval, CefString& Exception) override;

private:
	CefRefPtr<FUnrealCEFSubProcessRemoteObject> RemoteObject;
	CefString MethodName;

    // Include the default reference counting implementation.
    IMPLEMENT_REFCOUNTING(FUnrealCEFSubProcessRemoteMethodHandler);
};

#endif