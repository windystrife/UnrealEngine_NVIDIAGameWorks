// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

#if WITH_CEF3

#if PLATFORM_WINDOWS
#include "WindowsHWrapper.h"
#include "AllowWindowsPlatformTypes.h"
#include "AllowWindowsPlatformAtomics.h"
#endif

#pragma push_macro("OVERRIDE")
#undef OVERRIDE // cef headers provide their own OVERRIDE macro
THIRD_PARTY_INCLUDES_START
#include "include/cef_resource_handler.h"
THIRD_PARTY_INCLUDES_END
#pragma pop_macro("OVERRIDE")

#if PLATFORM_WINDOWS
#include "HideWindowsPlatformAtomics.h"
#include "HideWindowsPlatformTypes.h"
#endif


/**
 * Implements a resource handler that will return the contents of a string as the result.
 */
class FCEFBrowserByteResource
	: public CefResourceHandler
{
public:
	/**
	 */
	FCEFBrowserByteResource(const CefRefPtr<CefPostDataElement>& PostData, const FString& InMimeType);
	~FCEFBrowserByteResource();
	
	// CefResourceHandler interface
	virtual void Cancel() override;
	virtual void GetResponseHeaders(CefRefPtr<CefResponse> Response, int64& ResponseLength, CefString& RedirectUrl) override;
	virtual bool ProcessRequest(CefRefPtr<CefRequest> Request, CefRefPtr<CefCallback> Callback) override;
	virtual bool ReadResponse(void* DataOut, int BytesToRead, int& BytesRead, CefRefPtr<CefCallback> Callback) override;
	
private:
	int32 Position;
	int32 Size;
	unsigned char* Buffer;
	FString MimeType;
	
	// Include the default reference counting implementation.
	IMPLEMENT_REFCOUNTING(FCEFBrowserByteResource);
};


#endif
