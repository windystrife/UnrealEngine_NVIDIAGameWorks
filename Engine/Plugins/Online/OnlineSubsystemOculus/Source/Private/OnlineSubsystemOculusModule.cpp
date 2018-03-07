// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineSubsystemOculusModule.h"
#include "OnlineSubsystemOculusPrivate.h"

// FOnlineSubsystemOculusModule

IMPLEMENT_MODULE(FOnlineSubsystemOculusModule, OnlineSubsystemOculus);

/**
 * Class responsible for creating instance(s) of the subsystem
 */
class FOnlineFactoryOculus : public IOnlineFactory
{
private:

	/** Singleton instance of the Oculus OSS */
	FOnlineSubsystemOculusPtr OnlineSub;

public:

	FOnlineFactoryOculus() {}	
	virtual ~FOnlineFactoryOculus() {}

	IOnlineSubsystemPtr CreateSubsystem(FName InstanceName) override
	{
		if (!OnlineSub.IsValid())
		{
			OnlineSub = MakeShareable(new FOnlineSubsystemOculus(InstanceName));
		}
		if (OnlineSub->IsEnabled())
		{
			if (!OnlineSub->IsInitialized())
			{
				if (!OnlineSub->Init())
				{
					UE_LOG_ONLINE(Warning, TEXT("Oculus API failed to initialize!"));
					// Shutdown already called in Init() when this failed
					OnlineSub = nullptr;
				}
			}
			else
			{
				UE_LOG_ONLINE(Log, TEXT("Oculus API already initialized!"));
			}
		}
		else
		{
			UE_LOG_ONLINE(Warning, TEXT("Oculus API disabled!"));
			OnlineSub->Shutdown();
			OnlineSub = nullptr;
		}

		return OnlineSub;
	}
};

void FOnlineSubsystemOculusModule::StartupModule()
{
	UE_LOG_ONLINE(Log, TEXT("Oculus Startup!"));

	OculusFactory = new FOnlineFactoryOculus();

	// Create and register our singleton factory with the main online subsystem for easy access
	FOnlineSubsystemModule& OSS = FModuleManager::GetModuleChecked<FOnlineSubsystemModule>("OnlineSubsystem");
	OSS.RegisterPlatformService(OCULUS_SUBSYSTEM, OculusFactory);
}

void FOnlineSubsystemOculusModule::ShutdownModule()
{
	UE_LOG_ONLINE(Log, TEXT("Oculus Shutdown!"));

	FOnlineSubsystemModule& OSS = FModuleManager::GetModuleChecked<FOnlineSubsystemModule>("OnlineSubsystem");
	OSS.UnregisterPlatformService(OCULUS_SUBSYSTEM);
	
	delete OculusFactory;
	OculusFactory = nullptr;
}
