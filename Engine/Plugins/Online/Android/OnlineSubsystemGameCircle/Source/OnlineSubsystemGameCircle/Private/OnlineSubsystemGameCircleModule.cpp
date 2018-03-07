// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemGameCircleModule.h"
#include "OnlineSubsystemGameCircle.h"

IMPLEMENT_MODULE( FOnlineSubsystemGameCircleModule, OnlineSubsystemGameCircle );

/**
 * Class responsible for creating instance(s) of the subsystem
 */
class FOnlineFactoryGameCircle : public IOnlineFactory
{

private:

	/** Single instantiation of the IOS interface */
	static FOnlineSubsystemGameCirclePtr GameCircleSingleton;

	virtual void DestroySubsystem()
	{
		if (GameCircleSingleton.IsValid())
		{
			GameCircleSingleton->Shutdown();
			GameCircleSingleton = NULL;
		}
	}

public:

	FOnlineFactoryGameCircle() {}
	virtual ~FOnlineFactoryGameCircle() 
	{
		DestroySubsystem();
	}

	virtual IOnlineSubsystemPtr CreateSubsystem(FName InInstanceName)
	{
		if (!GameCircleSingleton.IsValid())
		{
			GameCircleSingleton = MakeShareable(new FOnlineSubsystemGameCircle(InInstanceName));
			if (GameCircleSingleton->IsEnabled())
			{
				if(!GameCircleSingleton->Init())
				{
					UE_LOG(LogOnline, Warning, TEXT("FOnlineSubsystemGameCircleModule failed to initialize!"));
					DestroySubsystem();
				}
			}
			else
			{
				UE_LOG(LogOnline, Warning, TEXT("FOnlineSubsystemGameCircleModule was disabled"));
				DestroySubsystem();
			}

			return GameCircleSingleton;
		}

		UE_LOG_ONLINE(Warning, TEXT("Can't create more than one instance of a Game Circle online subsystem!"));
		return NULL;
	}
};

FOnlineSubsystemGameCirclePtr FOnlineFactoryGameCircle::GameCircleSingleton = NULL;


void FOnlineSubsystemGameCircleModule::StartupModule()
{
	UE_LOG(LogOnline, Display, TEXT("OnlineSubsystemGameCircleModule::StartupModule()"));

	GameCircleFactory = new FOnlineFactoryGameCircle();

	FOnlineSubsystemModule& OSS = FModuleManager::GetModuleChecked<FOnlineSubsystemModule>("OnlineSubsystem");
	OSS.RegisterPlatformService(GAMECIRCLE_SUBSYSTEM, GameCircleFactory);
}


void FOnlineSubsystemGameCircleModule::ShutdownModule()
{
	UE_LOG(LogOnline, Display, TEXT("FOnlineSubsystemGameCircleModule::ShutdownModule()"));

	delete GameCircleFactory;
	GameCircleFactory = nullptr;
}
