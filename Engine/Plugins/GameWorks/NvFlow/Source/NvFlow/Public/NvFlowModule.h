#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "GameFramework/HUD.h"

#include "ModuleInterface.h"

struct FNvFlowCommands;
struct RendererHooksNvFlow;


class FNvFlowDebugInfoQueue
{
public:
	typedef TArray<FString> DebugInfo_t;

	void StartSubmitInfo()
	{
		bSubmitEnabled = (SubmitInfoCount < SubmitInfoCountThreshold);
		InfoToSubmit->Empty();
	}

	inline DebugInfo_t* GetSubmitInfo() const
	{
		return bSubmitEnabled ? InfoToSubmit : nullptr;
	}

	void FinishSubmitInfo()
	{
		if (bSubmitEnabled)
		{
			InfoToSubmit = (DebugInfo_t*)FPlatformAtomics::InterlockedExchangePtr((void**)&InfoToExchange, InfoToSubmit);

			FPlatformAtomics::InterlockedIncrement(&SubmitInfoCount);
		}
	}

	DebugInfo_t* FetchInfo()
	{
		if (FPlatformAtomics::InterlockedExchange(&SubmitInfoCount, 0) > 0)
		{
			InfoToFetch = (DebugInfo_t*)FPlatformAtomics::InterlockedExchangePtr((void**)&InfoToExchange, InfoToFetch);
		}
		return InfoToFetch;
	}

private:
	DebugInfo_t Info[3];

	DebugInfo_t* InfoToSubmit = &Info[0];
	DebugInfo_t* InfoToFetch = &Info[1];
	volatile DebugInfo_t* InfoToExchange = &Info[2];

	static const int32 SubmitInfoCountThreshold = 4;
	volatile int32 SubmitInfoCount = SubmitInfoCountThreshold;

	bool bSubmitEnabled = false;
};
extern FNvFlowDebugInfoQueue NvFlowDebugInfoQueue;


class FNvFlowModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;

	virtual void ShutdownModule() override;

protected:
	void* FlowModule;
	TUniquePtr<FNvFlowCommands> Commands;

private:
	// Callback function registered with HUD to supply debug info when "ShowDebug NvFlow" has been entered on the console
	static void OnShowDebugInfo(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos);

};

//////////////////////////////////////////////////////////////////////////

