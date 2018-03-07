// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Base interface for Http Requests and Responses.
 */
class IHttpBase
{
public:

	/**
	 * Get the URL used to send the request.
	 *
	 * @return the URL string.
	 */
	virtual FString GetURL() = 0;

	/** 
	 * Gets an URL parameter.
	 * expected format is ?Key=Value&Key=Value...
	 * If that format is not used, this function will not work.
	 * 
	 * @param ParameterName - the parameter to request.
	 * @return the parameter value string.
	 */
	virtual FString GetURLParameter(const FString& ParameterName) = 0;

	/** 
	 * Gets the value of a header, or empty string if not found. 
	 * 
	 * @param HeaderName - name of the header to set.
	 */
	virtual FString GetHeader(const FString& HeaderName) = 0;

	/**
	 * Return all headers in an array in "Name: Value" format.
	 *
	 * @return the header array of strings
	 */
	virtual TArray<FString> GetAllHeaders() = 0;

	/**
	 * Shortcut to get the Content-Type header value (if available)
	 *
	 * @return the content type.
	 */
	virtual FString GetContentType() = 0;

	/**
	 * Shortcut to get the Content-Length header value. Will not always return non-zero.
	 * If you want the real length of the payload, get the payload and check it's length.
	 *
	 * @return the content length (if available)
	 */
	virtual int32 GetContentLength() = 0;

	/**
	 * Get the content payload of the request or response.
	 *
	 * @param Content - array that will be filled with the content.
	 */
	virtual const TArray<uint8>& GetContent() = 0;

	/** 
	 * Destructor for overrides 
	 */
	virtual ~IHttpBase() {};
};

