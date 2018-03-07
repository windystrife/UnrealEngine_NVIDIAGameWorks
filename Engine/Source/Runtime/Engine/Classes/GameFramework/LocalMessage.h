// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

//=============================================================================
// LocalMessage
//
// LocalMessages are abstract classes which contain an array of localized text.
// The PlayerController function ReceiveLocalizedMessage() is used to send messages
// to a specific player by specifying the LocalMessage class and index.  This allows
// the message to be localized on the client side, and saves network bandwidth since
// the text is not sent.  Actors (such as the GameMode) use one or more LocalMessage
// classes to send messages.  
//
//=============================================================================

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "LocalMessage.generated.h"

class APlayerController;
class APlayerState;

/** Handles the many pieces of data passed into Client Receive */
USTRUCT()
struct ENGINE_API FClientReceiveData
{
	//always need to be here
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	APlayerController* LocalPC;

	UPROPERTY()
	FName MessageType;

	UPROPERTY()
	int32 MessageIndex;

	UPROPERTY()
	FString MessageString;

	UPROPERTY()
	APlayerState* RelatedPlayerState_1;

	UPROPERTY()
	APlayerState* RelatedPlayerState_2;

	UPROPERTY()
	UObject* OptionalObject;

	FClientReceiveData();
};

UCLASS(abstract)
class ENGINE_API ULocalMessage : public UObject
{
	GENERATED_UCLASS_BODY()
	/** send message to client */
	virtual void ClientReceive(const FClientReceiveData& ClientData) const PURE_VIRTUAL(ULocalMessage::ClientReceive,);
};



