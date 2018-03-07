// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ISourceCodeAccessModule.h"
#include "DefaultSourceCodeAccessor.h"

/**
 * Implementation of ISourceCodeAccessModule.
 */
class FSourceCodeAccessModule
	: public ISourceCodeAccessModule
{
public:

	/** Default constructor. */
	FSourceCodeAccessModule();

public:

	// IModuleInterface interface

	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

public:

	// ISourceCodeAccessModule interface

	virtual bool CanAccessSourceCode() const override;
	virtual ISourceCodeAccessor& GetAccessor() const override;
	virtual void SetAccessor(const FName& InName) override;
	virtual FLaunchingCodeAccessor& OnLaunchingCodeAccessor() override;
	virtual FDoneLaunchingCodeAccessor& OnDoneLaunchingCodeAccessor() override;
	virtual FOpenFileFailed& OnOpenFileFailed() override;

private:

	/** Handle when one of the modular features we are interested in is registered */
	void HandleModularFeatureRegistered(const FName& Type, IModularFeature* ModularFeature);

	/** Handle when one of the modular features we are interested in is unregistered */
	void HandleModularFeatureUnregistered(const FName& Type, IModularFeature* ModularFeature);

private:

	/** Event delegate fired when launching code accessor. */
	FLaunchingCodeAccessor LaunchingCodeAccessorDelegate;

	/** Event delegate fired when done launching code accessor.  */
	FDoneLaunchingCodeAccessor DoneLaunchingCodeAccessorDelegate;

	/** Event delegate fired when opening a file has failed. */
	FOpenFileFailed OpenFileFailedDelegate;

	/** The default accessor we will use if we have no IDE. */
	FDefaultSourceCodeAccessor DefaultSourceCodeAccessor;

	/** The current accessor. */
	ISourceCodeAccessor* CurrentSourceCodeAccessor;
};
