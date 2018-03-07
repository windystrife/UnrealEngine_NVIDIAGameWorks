// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Net/OnlineEngineInterface.h"
#include "UObject/Package.h"

UOnlineEngineInterface* UOnlineEngineInterface::Singleton = nullptr;

UOnlineEngineInterface::UOnlineEngineInterface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UOnlineEngineInterface* UOnlineEngineInterface::Get()
{
	if (!Singleton)
	{
		// Proper interface class hard coded here to emphasize the fact that this is not expected to change much, any need to do so should go through the OGS team first
		UClass* OnlineEngineInterfaceClass = StaticLoadClass(UOnlineEngineInterface::StaticClass(), NULL, TEXT("/Script/OnlineSubsystemUtils.OnlineEngineInterfaceImpl"), NULL, LOAD_Quiet, NULL);
		if (!OnlineEngineInterfaceClass)
		{
			// Default to the no op class if necessary
			OnlineEngineInterfaceClass = UOnlineEngineInterface::StaticClass();
		}

		Singleton = NewObject<UOnlineEngineInterface>(GetTransientPackage(), OnlineEngineInterfaceClass);
		Singleton->AddToRoot();
	}

	return Singleton;
}
