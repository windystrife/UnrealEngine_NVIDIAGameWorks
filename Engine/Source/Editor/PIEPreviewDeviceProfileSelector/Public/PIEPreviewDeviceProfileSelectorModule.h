// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "IDeviceProfileSelectorModule.h"
#include "PIEPreviewDeviceEnumeration.h"
#include "RHIDefinitions.h"
#include "CommandLine.h"

/**
* Implements the Preview Device Profile Selector module.
*/
class PIEPREVIEWDEVICEPROFILESELECTOR_API FPIEPreviewDeviceProfileSelectorModule
	: public IDeviceProfileSelectorModule
{
public:
	FPIEPreviewDeviceProfileSelectorModule() : bInitialized(false)
	{
	}

	//~ Begin IDeviceProfileSelectorModule Interface
	virtual const FString GetRuntimeDeviceProfileName() override;

	//~ End IDeviceProfileSelectorModule Interface

	//~ Begin IModuleInterface Interface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	//~ End IModuleInterface Interface

	/**
	* Virtual destructor.
	*/
	virtual ~FPIEPreviewDeviceProfileSelectorModule()
	{
	}

	void ApplyPreviewDeviceState();
	const FPIEPreviewDeviceContainer& GetPreviewDeviceContainer();
	TSharedPtr<FPIEPreviewDeviceContainerCategory> GetPreviewDeviceRootCategory() const { return EnumeratedDevices.GetRootCategory(); }

	static bool IsRequestingPreviewDevice()
	{
		FString PreviewDeviceDummy;
		return FParse::Value(FCommandLine::Get(), GetPreviewDeviceCommandSwitch(), PreviewDeviceDummy);
	}

private:
	static const TCHAR* GetPreviewDeviceCommandSwitch()
	{
		return TEXT("MobileTargetDevice=");
	}

	void InitPreviewDevice();
	static FString GetDeviceSpecificationContentDir();
	bool ReadDeviceSpecification();
	ERHIFeatureLevel::Type GetPreviewDeviceFeatureLevel() const;
	FString FindDeviceSpecificationFilePath(const FString& SearchDevice);

	bool bInitialized;
	FString DeviceProfile;
	FString PreviewDevice;

	FPIEPreviewDeviceContainer EnumeratedDevices;
	TSharedPtr<struct FPIEPreviewDeviceSpecifications> DeviceSpecs;
};