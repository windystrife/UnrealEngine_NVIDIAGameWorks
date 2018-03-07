// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

enum class EAutomationEventType : uint8
{
	Info,
	Warning,
	Error
};

struct CORE_API FAutomationEvent
{
	EAutomationEventType Type;
	FString Message;
	FString Context;
	FString Filename;
	int32 LineNumber;
	FDateTime Timestamp;

	FAutomationEvent(EAutomationEventType InType, FString InMessage)
		: Type(InType)
		, Message(InMessage)
		, Context()
		, Filename()
		, LineNumber(-1)
		, Timestamp(FDateTime::UtcNow())
	{
	}

	FAutomationEvent(EAutomationEventType InType, FString InMessage, FString InContext)
		: Type(InType)
		, Message(InMessage)
		, Context(InContext)
		, Filename()
		, LineNumber(-1)
		, Timestamp(FDateTime::UtcNow())
	{
	}

	FAutomationEvent(EAutomationEventType InType, FString InMessage, FString InContext, FString InFilename, int32 InLineNumber)
		: Type(InType)
		, Message(InMessage)
		, Context(InContext)
		, Filename(InFilename)
		, LineNumber(InLineNumber)
		, Timestamp(FDateTime::UtcNow())
	{
	}

	FString ToString() const;
};
