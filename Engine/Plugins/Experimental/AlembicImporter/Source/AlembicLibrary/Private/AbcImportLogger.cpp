// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AbcImportLogger.h"
#include "Logging/MessageLog.h"

TArray<TSharedRef<FTokenizedMessage>> FAbcImportLogger::TokenizedErrorMessages;
FCriticalSection FAbcImportLogger::MessageLock;

void FAbcImportLogger::AddImportMessage(const TSharedRef<FTokenizedMessage> Message)
{
	MessageLock.Lock();
	TokenizedErrorMessages.Add(Message);
	MessageLock.Unlock();
}

void FAbcImportLogger::OutputMessages(const FString& PageName)
{	
	static const FName LogName = "AssetTools";
	FMessageLog MessageLog(LogName);
	MessageLog.NewPage(FText::FromString(PageName));
	
	MessageLock.Lock();	
	MessageLog.AddMessages(TokenizedErrorMessages);
	TokenizedErrorMessages.Empty();
	MessageLock.Unlock();

	MessageLog.Open();
}
