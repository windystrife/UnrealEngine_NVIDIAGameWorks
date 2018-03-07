// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "CoreMinimal.h"

class FHttpManager;
class IHttpRequest;

// Temporary define until UE4Main has this function so OSS can know to avoid it
#define HTTP_GENERIC_PLATFORM_HAS_GETURLDOMAIN 1

/**
 * Platform specific Http implementations
 */
class HTTP_API FGenericPlatformHttp
{
public:

	/**
	 * Platform initialization step
	 */
	static void Init();

	/**
	 * Creates a platform-specific HTTP manager.
	 *
	 * @return nullptr if default implementation is to be used
	 */
	static FHttpManager* CreatePlatformHttpManager()
	{
		return nullptr;
	}

	/**
	 * Platform shutdown step
	 */
	static void Shutdown();

	/**
	 * Creates a new Http request instance for the current platform
	 *
	 * @return request object
	 */
	static IHttpRequest* ConstructRequest();

	/**
	 * Returns a percent-encoded version of the passed in string
	 *
	 * @param UnencodedString The unencoded string to convert to percent-encoding
	 * @return The percent-encoded string
	 */
	static FString UrlEncode(const FString& UnencodedString);

	/**
	 * Returns a decoded version of the percent-encoded passed in string
	 *
	 * @param EncodedString The percent encoded string to convert to string
	 * @return The decoded string
	 */
	static FString UrlDecode(const FString& EncodedString);

	/**
	 * Returns the &lt; &gt...etc encoding for strings between HTML elements.
	 *
	 * @param UnencodedString The unencoded string to convert to html encoding.
	 * @return The html encoded string
	 */
	static FString HtmlEncode(const FString& UnencodedString);

	/** 
	 * Returns the domain portion of the URL, e.g., "a.b.c" of "http://a.b.c/d"
	 * @param Url the URL to return the domain of
	 * @return the domain of the specified URL
	 */
	static FString GetUrlDomain(const FString& Url);

	/**
	 * Returns the mime type for the file.
	 */
	static FString GetMimeType(const FString& FilePath);

	
	/**
	 * Returns the default User-Agent string to use in HTTP requests.
	 * Requests that explicitly set the User-Agent header will not use this value.
	 *
	 * @return the default User-Agent string that requests should use.
	 */
	static FString GetDefaultUserAgent();
};
