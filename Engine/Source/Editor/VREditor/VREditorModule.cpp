// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VREditorModule.h"
#include "Modules/ModuleManager.h"
#include "Stats/Stats.h"
#include "HAL/IConsoleManager.h"
#include "IVREditorModule.h"
#include "VREditorModeManager.h"
#include "VREditorStyle.h"
#include "VREditorMode.h"
#include "MultiBoxBuilder.h"
#include "MultiBoxExtender.h"
#include "HeadMountedDisplayTypes.h"

class FVREditorModule : public IVREditorModule
{
public:
	FVREditorModule()
	{
	}

	// FModuleInterface overrides
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	virtual void PostLoadCallback() override;
	virtual bool SupportsDynamicReloading() override
	{
		return true;
	}

	// IVREditorModule overrides
	virtual bool IsVREditorEnabled() const override;
	virtual bool IsVREditorAvailable() const override;
	virtual void EnableVREditor( const bool bEnable, const bool bForceWithoutHMD ) override;
	virtual bool IsVREditorModeActive() override;
	virtual UVREditorMode* GetVRMode() override;
	virtual void UpdateActorPreview(TSharedRef<SWidget> InWidget) override;

	virtual const TSharedRef<FExtender>& GetRadialMenuExtender() override
	{
		static TSharedRef<class FExtender> RadialMenuExtender( new FExtender() );
		return RadialMenuExtender;
	}


	static void ToggleForceVRMode();

private:

	/** Handles turning VR Editor mode on and off */
	FVREditorModeManager ModeManager;
};

namespace VREd
{
	static FAutoConsoleCommand ForceVRMode( TEXT( "VREd.ForceVRMode" ), TEXT( "Toggles VREditorMode, even if not in immersive VR" ), FConsoleCommandDelegate::CreateStatic( &FVREditorModule::ToggleForceVRMode ) );
}

void FVREditorModule::StartupModule()
{
	if (GIsEditor)
	{
		FVREditorStyle::Initialize();
	}

}

void FVREditorModule::ShutdownModule()
{

	if (GIsEditor)
	{
		FVREditorStyle::Shutdown();
	}
}

void FVREditorModule::PostLoadCallback()
{
}

bool FVREditorModule::IsVREditorEnabled() const
{
	return ModeManager.IsVREditorActive();
}
													
bool FVREditorModule::IsVREditorAvailable() const
{
	return ModeManager.IsVREditorAvailable();
}	


void FVREditorModule::EnableVREditor( const bool bEnable, const bool bForceWithoutHMD )
{
	ModeManager.EnableVREditor( bEnable, bForceWithoutHMD );
}

bool FVREditorModule::IsVREditorModeActive()
{
	return ModeManager.IsVREditorActive();
}

UVREditorMode* FVREditorModule::GetVRMode()
{
	return ModeManager.GetCurrentVREditorMode();
}

void FVREditorModule::UpdateActorPreview(TSharedRef<SWidget> InWidget)
{
	GetVRMode()->RefreshActorPreviewWidget(InWidget);
}

void FVREditorModule::ToggleForceVRMode()
{
	const bool bForceWithoutHMD = true;
	FVREditorModule& Self = FModuleManager::GetModuleChecked< FVREditorModule >( TEXT( "VREditor" ) );
	Self.EnableVREditor( !Self.IsVREditorEnabled(), bForceWithoutHMD );
}

IMPLEMENT_MODULE( FVREditorModule, VREditor )
