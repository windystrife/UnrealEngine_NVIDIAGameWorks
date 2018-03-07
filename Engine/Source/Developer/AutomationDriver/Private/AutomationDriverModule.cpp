// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "IAutomationDriverModule.h"
#include "PassThroughMessageHandler.h"
#include "AutomatedApplication.h"
#include "AutomationDriver.h"
#include "Framework/Application/SlateApplication.h"

class FAutomationDriverModule
	: public IAutomationDriverModule
{
public:

	virtual void StartupModule() override
	{

	}

	virtual void ShutdownModule() override
	{

	}

	virtual TSharedRef<IAutomationDriver, ESPMode::ThreadSafe> CreateDriver() const override
	{
		return FAutomationDriverFactory::Create(AutomatedApplication.Get());
	}

	virtual TSharedRef<IAutomationDriver, ESPMode::ThreadSafe> CreateDriver(const TSharedRef<FDriverConfiguration, ESPMode::ThreadSafe>& Configuration) const override
	{
		return FAutomationDriverFactory::Create(AutomatedApplication.Get(), Configuration);
	}

	virtual TSharedRef<IAsyncAutomationDriver, ESPMode::ThreadSafe> CreateAsyncDriver() const override
	{
		return FAsyncAutomationDriverFactory::Create(AutomatedApplication.Get());
	}

	virtual TSharedRef<IAsyncAutomationDriver, ESPMode::ThreadSafe> CreateAsyncDriver(const TSharedRef<FDriverConfiguration, ESPMode::ThreadSafe>& Configuration) const override
	{
		return FAsyncAutomationDriverFactory::Create(AutomatedApplication.Get(), Configuration);
	}

	virtual bool IsEnabled() const override
	{
		return AutomatedApplication.IsValid();
	}

	virtual void Enable() override
	{
		if (AutomatedApplication.IsValid())
		{
			return;
		}

		RealApplication = FSlateApplication::Get().GetPlatformApplication();
		RealMessageHandler = RealApplication->GetMessageHandler();

		AutomatedApplication = FAutomatedApplicationFactory::Create(
			RealApplication.ToSharedRef(),
			FPassThroughMessageHandlerFactoryFactory::Create());

		FSlateApplication::Get().SetPlatformApplication(AutomatedApplication.ToSharedRef());
	}

	virtual void Disable() override
	{
		if (!AutomatedApplication.IsValid())
		{
			return;
		}

		FSlateApplication::Get().SetPlatformApplication(RealApplication.ToSharedRef());
		RealApplication->SetMessageHandler(RealMessageHandler.ToSharedRef());

		AutomatedApplication.Reset();
		RealApplication.Reset();
		RealMessageHandler.Reset();
	}

private:

	TSharedPtr<FAutomatedApplication> AutomatedApplication;
	TSharedPtr<GenericApplication> RealApplication;
	TSharedPtr<FGenericApplicationMessageHandler> RealMessageHandler;
};

IMPLEMENT_MODULE(FAutomationDriverModule, AutomationDriver);
