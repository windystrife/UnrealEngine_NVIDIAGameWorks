// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AITestSuite.h"
#include "AITestsCommon.h"

DEFINE_LOG_CATEGORY(LogAITestSuite);
DEFINE_LOG_CATEGORY(LogBehaviorTreeTest);

class FAITestSuite : public IAITestSuite
{
	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};

IMPLEMENT_MODULE(FAITestSuite, AITestSuite)



void FAITestSuite::StartupModule()
{
	// This code will execute after your module is loaded into memory (but after global variables are initialized, of course.)
}


void FAITestSuite::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

