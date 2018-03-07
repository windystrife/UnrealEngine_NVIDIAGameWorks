// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ModuleDescriptor.h"
#include "Misc/ScopedSlowTask.h"
#include "Dom/JsonObject.h"
#include "Modules/ModuleManager.h"

#define LOCTEXT_NAMESPACE "ModuleDescriptor"

ELoadingPhase::Type ELoadingPhase::FromString( const TCHAR *String )
{
	ELoadingPhase::Type TestType = (ELoadingPhase::Type)0;
	for(; TestType < ELoadingPhase::Max; TestType = (ELoadingPhase::Type)(TestType + 1))
	{
		const TCHAR *TestString = ToString(TestType);
		if(FCString::Stricmp(String, TestString) == 0)
		{
			break;
		}
	}
	return TestType;
}

const TCHAR* ELoadingPhase::ToString( const ELoadingPhase::Type Value )
{
	switch( Value )
	{
	case Default:
		return TEXT( "Default" );

	case PostDefault:
		return TEXT( "PostDefault" );

	case PreDefault:
		return TEXT( "PreDefault" );

	case PostConfigInit:
		return TEXT( "PostConfigInit" );
			
	case PreLoadingScreen:
		return TEXT( "PreLoadingScreen" );

	case PostEngineInit:
		return TEXT( "PostEngineInit" );

	case None:
		return TEXT( "None" );

	default:
		ensureMsgf( false, TEXT( "Unrecognized ELoadingPhase value: %i" ), Value );
		return NULL;
	}
}

EHostType::Type EHostType::FromString( const TCHAR *String )
{
	EHostType::Type TestType = (EHostType::Type)0;
	for(; TestType < EHostType::Max; TestType = (EHostType::Type)(TestType + 1))
	{
		const TCHAR *TestString = ToString(TestType);
		if(FCString::Stricmp(String, TestString) == 0)
		{
			break;
		}
	}
	return TestType;
}

const TCHAR* EHostType::ToString( const EHostType::Type Value )
{
	switch( Value )
	{
		case Runtime:
			return TEXT( "Runtime" );

		case RuntimeNoCommandlet:
			return TEXT( "RuntimeNoCommandlet" );

		case RuntimeAndProgram:
			return TEXT("RuntimeAndProgram");

		case CookedOnly:
			return TEXT("CookedOnly");

		case Developer:
			return TEXT( "Developer" );

		case Editor:
			return TEXT( "Editor" );

		case EditorNoCommandlet:
			return TEXT( "EditorNoCommandlet" );

		case Program:
			return TEXT("Program");

		case ServerOnly:
			return TEXT("ServerOnly");

		case ClientOnly:
			return TEXT("ClientOnly");

		default:
			ensureMsgf( false, TEXT( "Unrecognized EModuleType value: %i" ), Value );
			return NULL;
	}
}

FModuleDescriptor::FModuleDescriptor(const FName InName, EHostType::Type InType, ELoadingPhase::Type InLoadingPhase)
	: Name(InName)
	, Type(InType)
	, LoadingPhase(InLoadingPhase)
{
}

bool FModuleDescriptor::Read(const FJsonObject& Object, FText& OutFailReason)
{
	// Read the module name
	TSharedPtr<FJsonValue> NameValue = Object.TryGetField(TEXT("Name"));
	if(!NameValue.IsValid() || NameValue->Type != EJson::String)
	{
		OutFailReason = LOCTEXT("ModuleWithoutAName", "Found a 'Module' entry with a missing 'Name' field");
		return false;
	}
	Name = FName(*NameValue->AsString());

	// Read the module type
	TSharedPtr<FJsonValue> TypeValue = Object.TryGetField(TEXT("Type"));
	if(!TypeValue.IsValid() || TypeValue->Type != EJson::String)
	{
		OutFailReason = FText::Format( LOCTEXT( "ModuleWithoutAType", "Found Module entry '{0}' with a missing 'Type' field" ), FText::FromName(Name) );
		return false;
	}
	Type = EHostType::FromString(*TypeValue->AsString());
	if(Type == EHostType::Max)
	{
		OutFailReason = FText::Format( LOCTEXT( "ModuleWithInvalidType", "Module entry '{0}' specified an unrecognized module Type '{1}'" ), FText::FromName(Name), FText::FromString(TypeValue->AsString()) );
		return false;
	}

	// Read the loading phase
	TSharedPtr<FJsonValue> LoadingPhaseValue = Object.TryGetField(TEXT("LoadingPhase"));
	if(LoadingPhaseValue.IsValid() && LoadingPhaseValue->Type == EJson::String)
	{
		LoadingPhase = ELoadingPhase::FromString(*LoadingPhaseValue->AsString());
		if(LoadingPhase == ELoadingPhase::Max)
		{
			OutFailReason = FText::Format( LOCTEXT( "ModuleWithInvalidLoadingPhase", "Module entry '{0}' specified an unrecognized module LoadingPhase '{1}'" ), FText::FromName(Name), FText::FromString(LoadingPhaseValue->AsString()) );
			return false;
		}
	}

	// Read the whitelisted platforms
	TSharedPtr<FJsonValue> WhitelistPlatformsValue = Object.TryGetField(TEXT("WhitelistPlatforms"));
	if(WhitelistPlatformsValue.IsValid() && WhitelistPlatformsValue->Type == EJson::Array)
	{
		const TArray< TSharedPtr< FJsonValue > >& PlatformsArray = WhitelistPlatformsValue->AsArray();
		for(int Idx = 0; Idx < PlatformsArray.Num(); Idx++)
		{
			WhitelistPlatforms.Add(PlatformsArray[Idx]->AsString());
		}
	}

	// Read the blacklisted platforms
	TSharedPtr<FJsonValue> BlacklistPlatformsValue = Object.TryGetField(TEXT("BlacklistPlatforms"));
	if(BlacklistPlatformsValue.IsValid() && BlacklistPlatformsValue->Type == EJson::Array)
	{
		const TArray< TSharedPtr< FJsonValue > >& PlatformsArray = BlacklistPlatformsValue->AsArray();
		for(int Idx = 0; Idx < PlatformsArray.Num(); Idx++)
		{
			BlacklistPlatforms.Add(PlatformsArray[Idx]->AsString());
		}
	}

	// Read the whitelisted targets
	TSharedPtr<FJsonValue> WhitelistTargetsValue = Object.TryGetField(TEXT("WhitelistTargets"));
	if (WhitelistTargetsValue.IsValid() && WhitelistTargetsValue->Type == EJson::Array)
	{
		const TArray< TSharedPtr< FJsonValue > >& TargetsArray = WhitelistTargetsValue->AsArray();
		for (int Idx = 0; Idx < TargetsArray.Num(); Idx++)
		{
			WhitelistTargets.Add(TargetsArray[Idx]->AsString());
		}
	}

	// Read the blacklisted targets
	TSharedPtr<FJsonValue> BlacklistTargetsValue = Object.TryGetField(TEXT("BlacklistTargets"));
	if (BlacklistTargetsValue.IsValid() && BlacklistTargetsValue->Type == EJson::Array)
	{
		const TArray< TSharedPtr< FJsonValue > >& TargetsArray = BlacklistTargetsValue->AsArray();
		for (int Idx = 0; Idx < TargetsArray.Num(); Idx++)
		{
			BlacklistTargets.Add(TargetsArray[Idx]->AsString());
		}
	}

	// Read the additional dependencies
	TSharedPtr<FJsonValue> AdditionalDependenciesValue = Object.TryGetField(TEXT("AdditionalDependencies"));
	if (AdditionalDependenciesValue.IsValid() && AdditionalDependenciesValue->Type == EJson::Array)
	{
		const TArray< TSharedPtr< FJsonValue > >& DepArray = AdditionalDependenciesValue->AsArray();
		for (int Idx = 0; Idx < DepArray.Num(); Idx++)
		{
			AdditionalDependencies.Add(DepArray[Idx]->AsString());
		}
	}

	return true;
}

bool FModuleDescriptor::ReadArray(const FJsonObject& Object, const TCHAR* Name, TArray<FModuleDescriptor>& OutModules, FText& OutFailReason)
{
	bool bResult = true;

	TSharedPtr<FJsonValue> ModulesArrayValue = Object.TryGetField(Name);
	if(ModulesArrayValue.IsValid() && ModulesArrayValue->Type == EJson::Array)
	{
		const TArray< TSharedPtr< FJsonValue > >& ModulesArray = ModulesArrayValue->AsArray();
		for(int Idx = 0; Idx < ModulesArray.Num(); Idx++)
		{
			const TSharedPtr<FJsonValue>& ModuleValue = ModulesArray[Idx];
			if(ModuleValue.IsValid() && ModuleValue->Type == EJson::Object)
			{
				FModuleDescriptor Descriptor;
				if(Descriptor.Read(*ModuleValue->AsObject().Get(), OutFailReason))
				{
					OutModules.Add(Descriptor);
				}
				else
				{
					bResult = false;
				}
			}
			else
			{
				OutFailReason = LOCTEXT( "ModuleWithInvalidModulesArray", "The 'Modules' array has invalid contents and was not able to be loaded." );
				bResult = false;
			}
		}
	}
	
	return bResult;
}

void FModuleDescriptor::Write(TJsonWriter<>& Writer) const
{
	Writer.WriteObjectStart();
	Writer.WriteValue(TEXT("Name"), Name.ToString());
	Writer.WriteValue(TEXT("Type"), FString(EHostType::ToString(Type)));
	Writer.WriteValue(TEXT("LoadingPhase"), FString(ELoadingPhase::ToString(LoadingPhase)));
	if (WhitelistPlatforms.Num() > 0)
	{
		Writer.WriteArrayStart(TEXT("WhitelistPlatforms"));
		for(int Idx = 0; Idx < WhitelistPlatforms.Num(); Idx++)
		{
			Writer.WriteValue(WhitelistPlatforms[Idx]);
		}
		Writer.WriteArrayEnd();
	}
	if (BlacklistPlatforms.Num() > 0)
	{
		Writer.WriteArrayStart(TEXT("BlacklistPlatforms"));
		for(int Idx = 0; Idx < BlacklistPlatforms.Num(); Idx++)
		{
			Writer.WriteValue(BlacklistPlatforms[Idx]);
		}
		Writer.WriteArrayEnd();
	}
	if (WhitelistTargets.Num() > 0)
	{
		Writer.WriteArrayStart(TEXT("WhitelistTargets"));
		for (int Idx = 0; Idx < WhitelistTargets.Num(); Idx++)
		{
			Writer.WriteValue(WhitelistTargets[Idx]);
		}
		Writer.WriteArrayEnd();
	}
	if (BlacklistTargets.Num() > 0)
	{
		Writer.WriteArrayStart(TEXT("BlacklistTargets"));
		for (int Idx = 0; Idx < BlacklistTargets.Num(); Idx++)
		{
			Writer.WriteValue(BlacklistTargets[Idx]);
		}
		Writer.WriteArrayEnd();
	}
	if (AdditionalDependencies.Num() > 0)
	{
		Writer.WriteArrayStart(TEXT("AdditionalDependencies"));
		for (int Idx = 0; Idx < AdditionalDependencies.Num(); Idx++)
		{
			Writer.WriteValue(AdditionalDependencies[Idx]);
		}
		Writer.WriteArrayEnd();
	}
	Writer.WriteObjectEnd();
}

void FModuleDescriptor::WriteArray(TJsonWriter<>& Writer, const TCHAR* Name, const TArray<FModuleDescriptor>& Modules)
{
	if(Modules.Num() > 0)
	{
		Writer.WriteArrayStart(Name);
		for(int Idx = 0; Idx < Modules.Num(); Idx++)
		{
			Modules[Idx].Write(Writer);
		}
		Writer.WriteArrayEnd();
	}
}

bool FModuleDescriptor::IsCompiledInCurrentConfiguration() const
{
	// Cache the string for the current platform
	static FString UBTPlatform(FPlatformMisc::GetUBTPlatform());

	// Check the platform is whitelisted
	if(WhitelistPlatforms.Num() > 0 && !WhitelistPlatforms.Contains(UBTPlatform))
	{
		return false;
	}

	// Check the platform is not blacklisted
	if(BlacklistPlatforms.Num() > 0 && BlacklistPlatforms.Contains(UBTPlatform))
	{
		return false;
	}

	static FString UBTTarget(FPlatformMisc::GetUBTTarget());

	// Check the target is whitelisted
	if (WhitelistTargets.Num() > 0 && !WhitelistTargets.Contains(UBTTarget))
	{
		return false;
	}

	// Check the target is not blacklisted
	if (BlacklistTargets.Num() > 0 && BlacklistTargets.Contains(UBTTarget))
	{
		return false;
	}

	// Check the module is compatible with this target. This should match ModuleDescriptor.IsCompiledInConfiguration in UBT
	switch (Type)
	{
	case EHostType::Runtime:
	case EHostType::RuntimeNoCommandlet:
#if IS_PROGRAM
		return false;
#else
		return true;
#endif

	case EHostType::RuntimeAndProgram:
		return true;

	case EHostType::CookedOnly:
		return FPlatformProperties::RequiresCookedData();

	case EHostType::Developer:
		#if WITH_UNREAL_DEVELOPER_TOOLS
			return true;
		#endif
		break;

	case EHostType::Editor:
	case EHostType::EditorNoCommandlet:
		#if WITH_EDITOR
			return true;
		#endif
		break;

	case EHostType::Program:
		#if IS_PROGRAM
			return true;
		#endif
		break;

	case EHostType::ServerOnly:
		return !FPlatformProperties::IsClientOnly();

	case EHostType::ClientOnly:
		return !FPlatformProperties::IsServerOnly();

	}

	return false;
}

bool FModuleDescriptor::IsLoadedInCurrentConfiguration() const
{
	// Check that the module is built for this configuration
	if(!IsCompiledInCurrentConfiguration())
	{
		return false;
	}

	// Check that the runtime environment allows it to be loaded
	switch (Type)
	{
	case EHostType::RuntimeAndProgram:
		#if (WITH_ENGINE || WITH_PLUGIN_SUPPORT)
			return true;
		#endif
		break;

	case EHostType::Runtime:
		#if (WITH_ENGINE || WITH_PLUGIN_SUPPORT) && !IS_PROGRAM
			return true;
		#endif
		break;
	
	case EHostType::RuntimeNoCommandlet:
		#if (WITH_ENGINE || WITH_PLUGIN_SUPPORT)  && !IS_PROGRAM
			if(!IsRunningCommandlet()) return true;
		#endif
		break;

	case EHostType::CookedOnly:
		return FPlatformProperties::RequiresCookedData();

	case EHostType::Developer:
		#if WITH_UNREAL_DEVELOPER_TOOLS
			return true;
		#endif
		break;

	case EHostType::Editor:
		#if WITH_EDITOR
			if(GIsEditor) return true;
		#endif
		break;

	case EHostType::EditorNoCommandlet:
		#if WITH_EDITOR
			if(GIsEditor && !IsRunningCommandlet()) return true;
		#endif
		break;

	case EHostType::Program:
		#if WITH_PLUGIN_SUPPORT && IS_PROGRAM
			return true;
		#endif
		break;

	case EHostType::ServerOnly:
		return !FPlatformProperties::IsClientOnly();

	case EHostType::ClientOnly:
		return !IsRunningDedicatedServer();

	}

	return false;
}

void FModuleDescriptor::LoadModulesForPhase(ELoadingPhase::Type LoadingPhase, const TArray<FModuleDescriptor>& Modules, TMap<FName, EModuleLoadResult>& ModuleLoadErrors)
{
	FScopedSlowTask SlowTask(Modules.Num());
	for (int Idx = 0; Idx < Modules.Num(); Idx++)
	{
		SlowTask.EnterProgressFrame(1);
		const FModuleDescriptor& Descriptor = Modules[Idx];

		// Don't need to do anything if this module is already loaded
		if (!FModuleManager::Get().IsModuleLoaded(Descriptor.Name))
		{
			if (LoadingPhase == Descriptor.LoadingPhase && Descriptor.IsLoadedInCurrentConfiguration())
			{
				// @todo plugin: DLL search problems.  Plugins that statically depend on other modules within this plugin may not be found?  Need to test this.

				// NOTE: Loading this module may cause other modules to become loaded, both in the engine or game, or other modules 
				//       that are part of this project or plugin.  That's totally fine.
				EModuleLoadResult FailureReason;
				IModuleInterface* ModuleInterface = FModuleManager::Get().LoadModuleWithFailureReason(Descriptor.Name, FailureReason);
				if (ModuleInterface == nullptr)
				{
					// The module failed to load. Note this in the ModuleLoadErrors list.
					ModuleLoadErrors.Add(Descriptor.Name, FailureReason);
				}
			}
		}
	}
}

bool FModuleDescriptor::CheckModuleCompatibility(const TArray<FModuleDescriptor>& Modules, bool bGameModules, TArray<FString>& OutIncompatibleFiles)
{
	bool bResult = true;
	for(int Idx = 0; Idx < Modules.Num(); Idx++)
	{
		const FModuleDescriptor &Module = Modules[Idx];
		if (Module.IsCompiledInCurrentConfiguration() && !FModuleManager::Get().IsModuleUpToDate(Module.Name))
		{
			OutIncompatibleFiles.Add(FModuleManager::GetCleanModuleFilename(Module.Name, bGameModules));
			bResult = false;
		}
	}
	return bResult;
}

#undef LOCTEXT_NAMESPACE
