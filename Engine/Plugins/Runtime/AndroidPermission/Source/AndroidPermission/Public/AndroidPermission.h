// Copyright 2017 Google Inc.

#pragma once

#include "ModuleManager.h"

ANDROIDPERMISSION_API DECLARE_LOG_CATEGORY_EXTERN(LogAndroidPermission, Log, All);

class FAndroidPermissionModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};