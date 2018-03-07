// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "ISettingsModule.h"
#include "Interfaces/ITargetPlatformModule.h"
#include "GenericWindowsTargetPlatform.h"
#include "ModuleManager.h"
#include "UObject/Package.h"
#include "UObject/WeakObjectPtr.h"

#define LOCTEXT_NAMESPACE "FWindowsTargetPlatformModule"


/** Holds the target platform singleton. */
static ITargetPlatform* Singleton = nullptr;


/**
 * Implements the Windows target platform module.
 */
class FWindowsTargetPlatformModule
	: public ITargetPlatformModule
{
public:

	/** Destructor. */
	~FWindowsTargetPlatformModule( )
	{
		Singleton = nullptr;
	}

public:

	// this is an example of a hotfix, declared here for no particular reason. Once we have other examples, it can be deleted.
#if 0
	void HotfixTest( void *InPayload, int PayloadSize )
	{
		check(sizeof(FTestHotFixPayload) == PayloadSize);
		
		FTestHotFixPayload* Payload = (FTestHotFixPayload*)InPayload;
		UE_LOG(LogTemp, Log, TEXT("Hotfix Test %s"), *Payload->Message);
		Payload->Result = Payload->ValueToReturn;
	}
#endif

public:

	// ITargetPlatformModule interface

	virtual ITargetPlatform* GetTargetPlatform( ) override
	{
		if (Singleton == nullptr)
		{
			Singleton = new TGenericWindowsTargetPlatform<true, false, false>();
		}

		return Singleton;
	}

public:

	// IModuleInterface interface

	virtual void StartupModule() override
	{
		// this is an example of a hotfix, declared here for no particular reason. Once we have other examples, it can be deleted.
#if 0
		FCoreDelegates::GetHotfixDelegate(EHotfixDelegates::Test).BindRaw(this, &FWindowsTargetPlatformModule::HotfixTest);
#endif
	}

	virtual void ShutdownModule() override
	{
		// this is an example of a hotfix, declared here for no particular reason. Once we have other examples, it can be deleted.
#if 0
		FCoreDelegates::GetHotfixDelegate(EHotfixDelegates::Test).Unbind();
#endif

		ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");
	}

};


#undef LOCTEXT_NAMESPACE


IMPLEMENT_MODULE(FWindowsTargetPlatformModule, WindowsTargetPlatform);
