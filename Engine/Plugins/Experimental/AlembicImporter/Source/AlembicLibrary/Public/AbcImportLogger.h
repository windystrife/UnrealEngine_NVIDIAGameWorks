// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/TokenizedMessage.h"

class FAbcImportLogger
{
protected:
	FAbcImportLogger();
public:
	/** Adds an import message to the stored array for later output*/
	static void AddImportMessage(const TSharedRef<FTokenizedMessage> Message);
	/** Outputs the messages to a new named page in the message log */
	ALEMBICLIBRARY_API static void OutputMessages(const FString& PageName);
private:
	/** Error messages **/
	static TArray<TSharedRef<FTokenizedMessage>> TokenizedErrorMessages;
	static FCriticalSection MessageLock;
};

