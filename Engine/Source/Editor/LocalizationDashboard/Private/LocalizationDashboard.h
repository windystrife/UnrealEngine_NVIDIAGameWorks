// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "SlateFwd.h"

class SLocalizationDashboard;
class ULocalizationTarget;

class FLocalizationDashboard
{
public:
	static FLocalizationDashboard* Get();
	static void Initialize();
	static void Terminate();
	void Show();
	TWeakPtr<SDockTab> ShowTargetEditorTab(ULocalizationTarget* const LocalizationTarget);

private:
	FLocalizationDashboard();
	~FLocalizationDashboard();
	void RegisterTabSpawner();
	void UnregisterTabSpawner();
	
private:
	static const FName TabName;
	static FLocalizationDashboard* Instance;
	TSharedPtr<SLocalizationDashboard> LocalizationDashboardWidget;
};
