// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

namespace EOnlineServerConnectionStatus {
	enum Type : uint8;
}

/** Generic Error response for OSS calls */
struct ONLINESUBSYSTEM_API FOnlineError
{
public:
	FOnlineError();
	explicit FOnlineError(bool bSucceeded);
	explicit FOnlineError(const FString& ErrorCode);
	explicit FOnlineError(FString&& ErrorCode);
	explicit FOnlineError(const FText& ErrorMessage);

	/** Same as the Ctors but can be called any time */
	void SetFromErrorCode(const FString& ErrorCode);
	void SetFromErrorCode(FString&& ErrorCode);
	void SetFromErrorMessage(const FText& ErrorMessage);

	/** Converts the HttpResult into a EOnlineServerConnectionStatus */
	EOnlineServerConnectionStatus::Type GetConnectionStatusFromHttpResult() const;

	/** Code useful when all you have is raw error info from old APIs */
	static const FString GenericErrorCode;

	/** Call this if you want to log this out (will pick the best string representation) */
	const TCHAR* ToLogString() const;

public:
	/** Did the request succeed fully. If this is true the rest of the struct probably doesn't matter */
	bool bSucceeded;

	/** The HTTP response code. Will be 0 if a connection error occurred or if HTTP was not used */
	int32 HttpResult;

	/** The raw unparsed error message from server. Used for pass-through error processing by other systems. */
	FString ErrorRaw;

	/** Intended to be interpreted by code. */
	FString ErrorCode;

	/** Suitable for display to end user. Guaranteed to be in the current locale (or empty) */
	FText ErrorMessage;

	/** Numeric error code provided by the backend expected to correspond to error stored in ErrorCode */
	int32 NumericErrorCode;
};
