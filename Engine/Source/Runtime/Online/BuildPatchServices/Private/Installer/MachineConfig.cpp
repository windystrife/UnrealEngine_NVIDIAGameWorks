// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Installer/MachineConfig.h"
#include "Core/AsyncHelpers.h"
#include "Misc/ConfigCacheIni.h"

namespace MachineConfigHelpers
{
	TSet<FString> LoadInstalledPrereqIds(const FString& LocalMachineConfigFile)
	{
		check(IsInGameThread());
		TArray<FString> ConfigStrings;
		GConfig->GetArray(TEXT("Portal.BuildPatch"), TEXT("InstalledPrereqs"), ConfigStrings, LocalMachineConfigFile);
		return TSet<FString>(ConfigStrings);
	};

	void SaveInstalledPrereqIds(const FString& LocalMachineConfigFile, const TSet<FString>& InstalledPrereqIds, bool bFlush)
	{
		check(IsInGameThread());
		GConfig->SetArray(TEXT("Portal.BuildPatch"), TEXT("InstalledPrereqs"), InstalledPrereqIds.Array(), LocalMachineConfigFile);
		if (bFlush)
		{
			GConfig->Flush(false, LocalMachineConfigFile);
		}
	}
}

namespace BuildPatchServices
{
	class FMachineConfig
		: public IMachineConfig
	{
	public:
		/**
		 * Constructor
		 * @param InLocalMachineConfigFile      The name of the configuration file which will hold the per machine config data.
		 * @param bAlwaysFlushChanges           If true, then we'll always flush changes straight to disk after saving values.
		 */
		FMachineConfig(const FString& InLocalMachineConfigFile, bool bAlwaysFlushChanges);

		//~ Begin IMachineConfig Interface
		virtual TSet<FString> LoadInstalledPrereqIds() override;
		virtual void SaveInstalledPrereqIds(const TSet<FString>& InstalledPrereqIds) override;
		//~ End IMachineConfig Interface
	private:
		// The filename for the local machine config. This is used for per-machine values rather than per-user or shipped config.
		FString LocalMachineConfigFile;

		// If true, then we'll always flush changes straight to disk after saving values.
		bool bAlwaysFlushChanges;
	};

	FMachineConfig::FMachineConfig(const FString& InLocalMachineConfigFile, bool bInAlwaysFlushChanges)
		: LocalMachineConfigFile(InLocalMachineConfigFile)
		, bAlwaysFlushChanges(bInAlwaysFlushChanges)
	{ }

	TSet<FString> FMachineConfig::LoadInstalledPrereqIds()
	{
		return AsyncHelpers::ExecuteOnGameThread<TSet<FString>>([this]()
		{
			return MachineConfigHelpers::LoadInstalledPrereqIds(LocalMachineConfigFile);
		}).Get();
	}

	void FMachineConfig::SaveInstalledPrereqIds(const TSet<FString>& InstalledPrereqIds)
	{
		AsyncHelpers::ExecuteOnGameThread<void>([this, &InstalledPrereqIds]()
		{
			MachineConfigHelpers::SaveInstalledPrereqIds(LocalMachineConfigFile, InstalledPrereqIds, bAlwaysFlushChanges);
		}).Wait();
	}

	IMachineConfig* FMachineConfigFactory::Create(const FString& LocalMachineConfigFile, bool bAlwaysFlushChanges)
	{
		return new FMachineConfig(LocalMachineConfigFile, bAlwaysFlushChanges);
	}

}
