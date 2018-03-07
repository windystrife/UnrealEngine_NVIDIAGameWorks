// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "PacketHandler.h"
#include "ModuleManager.h"

/** Encryption Handler Component Module Interface */
class FEncryptionHandlerComponentModuleInterface : public FPacketHandlerComponentModuleInterface
{
	public:
	/* Creates an instance of this component */
	virtual TSharedPtr<HandlerComponent> CreateComponentInstance(FString& Options) override;
};

namespace EEncryptionHandler
{
	enum State
	{
		UnInitialized,
		InitializingAsymmetric,
		InitializingSymmetric,
		Initialized
	};
}

/*
* This class uses Asymmetric encryption to send an encrypted symmetric encryption key
* to the remote side. Defaults are RSA for asymmetric encryption and XOR stream cipher
* for symmetric encryption.
*/
class ENCRYPTIONHANDLERCOMPONENT_API EncryptionHandlerComponent : public HandlerComponent
{
public:
	/* Initializes the default encryption handler components unless specified by the user */ 
	EncryptionHandlerComponent(HandlerComponent* SymmetricHandlerComponent = nullptr, HandlerComponent* AsymmetricHandlerComponent = nullptr);

	/* Initializes the handler component */
	virtual void Initialize() override;

	/* Whether the handler component is valid */
	virtual bool IsValid() const override;

	/* Handles any incoming packets */
	virtual void Incoming(FBitReader& Packet) override;

	/* Handles any outgoing packets */
	virtual void Outgoing(FBitWriter& Packet) override;

protected:
	/* Sets the state of the handler */
	void SetState(EEncryptionHandler::State State);

	/* The state of this handler */
	EEncryptionHandler::State State;

	/* Symmetric encryption handler component */
	HandlerComponent* SymmetricHandlerComponent;

	/* Asymmetric encryption handler component */
	HandlerComponent* AsymmetricHandlerComponent;
};
