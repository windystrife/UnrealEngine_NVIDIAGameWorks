#pragma once
#include "CoreMinimal.h"

#include "ModuleInterface.h"
#include "Stats/Stats.h"

class FBlastModule : public IModuleInterface
{
public:
	FBlastModule();
	virtual ~FBlastModule() = default;

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};


DECLARE_STATS_GROUP(TEXT("Blast"), STATGROUP_Blast, STATCAT_Advanced);

