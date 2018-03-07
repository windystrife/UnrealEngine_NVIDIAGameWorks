// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "EncryptionHandlerComponent.h"
#include "BlockEncryptionHandlerComponent.h"
#include "RSAEncryptionHandlerComponent.h"

IMPLEMENT_MODULE(FEncryptionHandlerComponentModuleInterface, EncryptionHandlerComponent);

// ENCRYPTION PROCESSOR
EncryptionHandlerComponent::EncryptionHandlerComponent(HandlerComponent* InSymmetricHandlerComponent,
														HandlerComponent* InAsymmetricHandlerComponent)
	: State(EEncryptionHandler::State::UnInitialized)
{
	if (InSymmetricHandlerComponent != nullptr)
	{
		SymmetricHandlerComponent = InSymmetricHandlerComponent;
	} 
	else
	{
		SymmetricHandlerComponent = new BlockEncryptionHandlerComponent;
	}

	if (InAsymmetricHandlerComponent != nullptr)
	{
		AsymmetricHandlerComponent = InAsymmetricHandlerComponent;
	} 
	else
	{
		AsymmetricHandlerComponent = new RSAEncryptionHandlerComponent;
	}

	SetActive(true);
}

void EncryptionHandlerComponent::Initialize()
{
	SymmetricHandlerComponent->Handler = Handler;
	SymmetricHandlerComponent->Initialize();

	AsymmetricHandlerComponent->Handler = Handler;
	AsymmetricHandlerComponent->Initialize();

	SetState(EEncryptionHandler::State::InitializingAsymmetric);
}

bool EncryptionHandlerComponent::IsValid() const
{
	return SymmetricHandlerComponent->IsValid() && AsymmetricHandlerComponent->IsValid();
}

void EncryptionHandlerComponent::SetState(EEncryptionHandler::State InState)
{
	State = InState;
}

void EncryptionHandlerComponent::Incoming(FBitReader& Packet)
{
	switch (State)
	{
		case EEncryptionHandler::State::InitializingAsymmetric:
		{
			AsymmetricHandlerComponent->Incoming(Packet);

			if (AsymmetricHandlerComponent->IsInitialized())
			{
				if (Handler->Mode == Handler::Mode::Client)
				{
					SetState(EEncryptionHandler::State::InitializingSymmetric);
				} 
				else if (Handler->Mode == Handler::Mode::Server)
				{
					SymmetricHandlerComponent->Incoming(Packet);
				}
			}
			break;
		}
		case EEncryptionHandler::State::InitializingSymmetric:
		{
			AsymmetricHandlerComponent->Incoming(Packet);
			SymmetricHandlerComponent->Incoming(Packet);
			break;
		}
		case EEncryptionHandler::State::Initialized:
		{
			SymmetricHandlerComponent->Incoming(Packet);
			break;
		}
		default:
		{
			break;
		}
	}

	if (State != EEncryptionHandler::State::Initialized && SymmetricHandlerComponent->IsInitialized())
	{
		AsymmetricHandlerComponent->SetActive(false);
		Initialized();
		SetState(EEncryptionHandler::State::Initialized);
	}
}

void EncryptionHandlerComponent::Outgoing(FBitWriter& Packet)
{
	switch (State)
	{
		case EEncryptionHandler::State::InitializingAsymmetric:
		{
			AsymmetricHandlerComponent->Outgoing(Packet);

			if (AsymmetricHandlerComponent->IsInitialized())
			{
				SetState(EEncryptionHandler::State::InitializingSymmetric);
			}
			break;
		}
		case EEncryptionHandler::State::InitializingSymmetric:
		{
			SymmetricHandlerComponent->Outgoing(Packet);
			AsymmetricHandlerComponent->Outgoing(Packet);
			break;
		}
		case EEncryptionHandler::State::Initialized:
		{
			if(Packet.GetNumBytes() > 0)
			{
				SymmetricHandlerComponent->Outgoing(Packet);
			}
			break;
		}
		default:
		{
			break;
		}
	}

	if (State != EEncryptionHandler::State::Initialized && SymmetricHandlerComponent->IsInitialized())
	{
		AsymmetricHandlerComponent->SetActive(false);
		Initialized();
		SetState(EEncryptionHandler::State::Initialized);
	}
}

//MODULE INTERFACE
TSharedPtr<HandlerComponent> FEncryptionHandlerComponentModuleInterface::CreateComponentInstance(FString& Options)
{
	return MakeShareable(new EncryptionHandlerComponent);
}