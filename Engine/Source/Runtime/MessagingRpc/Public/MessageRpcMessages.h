// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/UnrealString.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"

#include "MessageRpcMessages.generated.h"


/** Message for canceling an RPC call. */
USTRUCT()
struct FMessageRpcCancel
{
	GENERATED_USTRUCT_BODY()

	/** Correlation identifier for the RPC call that this message refers to. */
	UPROPERTY(EditAnywhere, Category="Message")
	FGuid CallId;

	/** Default constructor. */
	FMessageRpcCancel() { }

	/** Creates and initializes a new instance. */
	FMessageRpcCancel(const FGuid& InCallId)
		: CallId(InCallId)
	{ }
};


/** Message for updating the progress of an RPC call. */
USTRUCT()
struct FMessageRpcProgress
{
	GENERATED_USTRUCT_BODY()

	/** Completion percentage (0.0 to 1.0). */
	UPROPERTY(EditAnywhere, Category="Message")
	float Completion;

	/** Correlation identifier for the RPC call that this message refers to. */
	UPROPERTY(EditAnywhere, Category="Message")
	FGuid CallId;

	/** Status text. */
	UPROPERTY(EditAnywhere, Category="Message")
	FString StatusText;

	/** Default constructor. */
	FMessageRpcProgress() { }

	/** Creates and initializes a new instance. */
	FMessageRpcProgress(const FGuid& InCallId, float InCompletion, const FText& InStatusText)
		: Completion(InCompletion)
		, CallId(InCallId)
		, StatusText(InStatusText.ToString())
	{ }
};


/** Message for notifying RPC clients that a call was not handled by the server. */
USTRUCT()
struct FMessageRpcUnhandled
{
	GENERATED_USTRUCT_BODY()

	/** Correlation identifier for the RPC call that this message refers to. */
	UPROPERTY(EditAnywhere, Category="Message")
	FGuid CallId;

	/** Default constructor. */
	FMessageRpcUnhandled() { }

	/** Creates and initializes a new instance. */
	FMessageRpcUnhandled(const FGuid& InCallId)
		: CallId(InCallId)
	{ }
};
