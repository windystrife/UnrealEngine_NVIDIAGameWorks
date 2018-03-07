// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/CoreMisc.h"
#include "AISystem.h"
#if !UE_BUILD_SHIPPING
#include "BehaviorTree/BehaviorTreeManager.h"

struct FAISystemExec : public FSelfRegisteringExec
{
	FAISystemExec()
	{
	}

	// Begin FExec Interface
	virtual bool Exec(UWorld* Inworld, const TCHAR* Cmd, FOutputDevice& Ar) override;
	// End FExec Interface
};

FAISystemExec AISystemExecInstance;

bool FAISystemExec::Exec(UWorld* Inworld, const TCHAR* Cmd, FOutputDevice& Ar)
{
	if (Inworld == NULL)
	{
		return false;
	}

	UAISystem* AISys = UAISystem::GetCurrent(*Inworld);	
	bool bHandled = false;

	if (AISys != NULL)
	{
		if (FParse::Command(&Cmd, TEXT("AIIgnorePlayers")))
		{
			AISys->AIIgnorePlayers();
			bHandled = true;
		}
		else if (FParse::Command(&Cmd, TEXT("AILoggingVerbose")))
		{
			AISys->AILoggingVerbose();
			bHandled = true;
		}
		else if (FParse::Command(&Cmd, TEXT("DumpBTUsageStats")))
		{
			if (AISys->GetBehaviorTreeManager())
			{
				AISys->GetBehaviorTreeManager()->DumpUsageStats();
				bHandled = true;
			}
		}
	}

	return bHandled;
}

#endif // !UE_BUILD_SHIPPING
