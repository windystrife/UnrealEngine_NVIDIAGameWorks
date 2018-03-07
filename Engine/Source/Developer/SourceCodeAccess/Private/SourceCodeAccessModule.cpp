// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SourceCodeAccessModule.h"
#include "Modules/ModuleManager.h"
#include "Framework/Application/SlateApplication.h"
#include "Features/IModularFeatures.h"
#include "ISettingsModule.h"
#include "SourceCodeAccessSettings.h"


IMPLEMENT_MODULE( FSourceCodeAccessModule, SourceCodeAccess );

#define LOCTEXT_NAMESPACE "SourceCodeAccessModule"

static FName SourceCodeAccessorFeatureName(TEXT("SourceCodeAccessor"));


FSourceCodeAccessModule::FSourceCodeAccessModule()
	: CurrentSourceCodeAccessor(nullptr)
{ }


void FSourceCodeAccessModule::StartupModule()
{
	GetMutableDefault<USourceCodeAccessSettings>()->LoadConfig();

	// Register to check for source control features
	IModularFeatures::Get().OnModularFeatureRegistered().AddRaw(this, &FSourceCodeAccessModule::HandleModularFeatureRegistered);
	IModularFeatures::Get().OnModularFeatureUnregistered().AddRaw(this, &FSourceCodeAccessModule::HandleModularFeatureUnregistered);

	// bind default accessor to editor
	IModularFeatures::Get().RegisterModularFeature(SourceCodeAccessorFeatureName, &DefaultSourceCodeAccessor);

	// Register to display our settings
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

	if (SettingsModule != nullptr)
	{
		SettingsModule->RegisterSettings("Editor", "General", "Source Code",
			LOCTEXT("TargetSettingsName", "Source Code"),
			LOCTEXT("TargetSettingsDescription", "Control how the editor accesses source code."),
			GetMutableDefault<USourceCodeAccessSettings>()
		);
	}
}


void FSourceCodeAccessModule::ShutdownModule()
{
	// Unregister our settings
	ISettingsModule* SettingsModule = FModuleManager::GetModulePtr<ISettingsModule>("Settings");

	if (SettingsModule != nullptr)
	{
		SettingsModule->UnregisterSettings("Editor", "General", "Source Code");
	}

	// unbind default provider from editor
	IModularFeatures::Get().UnregisterModularFeature(SourceCodeAccessorFeatureName, &DefaultSourceCodeAccessor);

	// we don't care about modular features any more
	IModularFeatures::Get().OnModularFeatureRegistered().RemoveAll(this);
	IModularFeatures::Get().OnModularFeatureUnregistered().RemoveAll(this);
}


bool FSourceCodeAccessModule::CanAccessSourceCode() const
{
	return CurrentSourceCodeAccessor->CanAccessSourceCode();
}


ISourceCodeAccessor& FSourceCodeAccessModule::GetAccessor() const
{
	return *CurrentSourceCodeAccessor;
}


void FSourceCodeAccessModule::SetAccessor(const FName& InName)
{
	const int32 FeatureCount = IModularFeatures::Get().GetModularFeatureImplementationCount(SourceCodeAccessorFeatureName);
	for(int32 FeatureIndex = 0; FeatureIndex < FeatureCount; FeatureIndex++)
	{
		IModularFeature* Feature = IModularFeatures::Get().GetModularFeatureImplementation(SourceCodeAccessorFeatureName, FeatureIndex);
		check(Feature);

		ISourceCodeAccessor& Accessor = *static_cast<ISourceCodeAccessor*>(Feature);
		if(InName == Accessor.GetFName())
		{
			CurrentSourceCodeAccessor = static_cast<ISourceCodeAccessor*>(Feature);
			if(FSlateApplication::IsInitialized())
			{
				FSlateApplication::Get().SetWidgetReflectorSourceAccessDelegate( FAccessSourceCode::CreateRaw( CurrentSourceCodeAccessor, &ISourceCodeAccessor::OpenFileAtLine ) );
				FSlateApplication::Get().SetWidgetReflectorQuerySourceAccessDelegate( FQueryAccessSourceCode::CreateRaw( CurrentSourceCodeAccessor, &ISourceCodeAccessor::CanAccessSourceCode ) );
			}
			break;
		}
	}
}


FLaunchingCodeAccessor& FSourceCodeAccessModule::OnLaunchingCodeAccessor()
{
	return LaunchingCodeAccessorDelegate;
}

FDoneLaunchingCodeAccessor& FSourceCodeAccessModule::OnDoneLaunchingCodeAccessor()
{
	return DoneLaunchingCodeAccessorDelegate;
}


FOpenFileFailed& FSourceCodeAccessModule::OnOpenFileFailed()
{
	return OpenFileFailedDelegate;
}


void FSourceCodeAccessModule::HandleModularFeatureRegistered(const FName& Type, IModularFeature* ModularFeature)
{
	if(Type == SourceCodeAccessorFeatureName)
	{
		CurrentSourceCodeAccessor = &DefaultSourceCodeAccessor;

		const FString PreferredAccessor = GetDefault<USourceCodeAccessSettings>()->PreferredAccessor;

		const int32 FeatureCount = IModularFeatures::Get().GetModularFeatureImplementationCount(Type);
		for(int32 FeatureIndex = 0; FeatureIndex < FeatureCount; FeatureIndex++)
		{
			IModularFeature* Feature = IModularFeatures::Get().GetModularFeatureImplementation(Type, FeatureIndex);
			check(Feature);

			ISourceCodeAccessor& Accessor = *static_cast<ISourceCodeAccessor*>(Feature);
			if(PreferredAccessor == Accessor.GetFName().ToString())
			{
				CurrentSourceCodeAccessor = static_cast<ISourceCodeAccessor*>(Feature);
				if(FSlateApplication::IsInitialized())
				{
					FSlateApplication::Get().SetWidgetReflectorSourceAccessDelegate( FAccessSourceCode::CreateRaw( CurrentSourceCodeAccessor, &ISourceCodeAccessor::OpenFileAtLine ) );
					FSlateApplication::Get().SetWidgetReflectorQuerySourceAccessDelegate( FQueryAccessSourceCode::CreateRaw( CurrentSourceCodeAccessor, &ISourceCodeAccessor::CanAccessSourceCode ) );
				}
				break;
			}
		}
	}
}


void FSourceCodeAccessModule::HandleModularFeatureUnregistered(const FName& Type, IModularFeature* ModularFeature)
{
	if(Type == SourceCodeAccessorFeatureName && CurrentSourceCodeAccessor == static_cast<ISourceCodeAccessor*>(ModularFeature))
	{
		CurrentSourceCodeAccessor = &DefaultSourceCodeAccessor;
	}
}


#undef LOCTEXT_NAMESPACE
