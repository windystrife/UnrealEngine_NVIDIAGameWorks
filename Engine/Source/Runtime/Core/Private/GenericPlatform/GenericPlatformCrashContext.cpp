// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "GenericPlatform/GenericPlatformCrashContext.h"
#include "HAL/PlatformTime.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "Internationalization/Culture.h"
#include "Misc/Optional.h"
#include "Internationalization/Internationalization.h"
#include "Misc/Guid.h"
#include "Containers/Ticker.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/CoreDelegates.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Misc/EngineBuildSettings.h"

#ifndef NOINITCRASHREPORTER
#define NOINITCRASHREPORTER 0
#endif

extern CORE_API bool GIsGPUCrashed;

/*-----------------------------------------------------------------------------
	FGenericCrashContext
-----------------------------------------------------------------------------*/

const ANSICHAR* FGenericCrashContext::CrashContextRuntimeXMLNameA = "CrashContext.runtime-xml";
const TCHAR* FGenericCrashContext::CrashContextRuntimeXMLNameW = TEXT( "CrashContext.runtime-xml" );

const ANSICHAR* FGenericCrashContext::CrashConfigFileNameA = "CrashReportClient.ini";
const TCHAR* FGenericCrashContext::CrashConfigFileNameW = TEXT("CrashReportClient.ini");
const FString FGenericCrashContext::CrashConfigExtension = TEXT(".ini");
const FString FGenericCrashContext::ConfigSectionName = TEXT("CrashReportClient");
const FString FGenericCrashContext::CrashConfigPurgeDays = TEXT("CrashConfigPurgeDays");
const FString FGenericCrashContext::CrashGUIDRootPrefix = TEXT("UE4CC-");

const FString FGenericCrashContext::CrashContextExtension = TEXT(".runtime-xml");
const FString FGenericCrashContext::RuntimePropertiesTag = TEXT( "RuntimeProperties" );
const FString FGenericCrashContext::PlatformPropertiesTag = TEXT( "PlatformProperties" );
const FString FGenericCrashContext::EnabledPluginsTag = TEXT("EnabledPlugins");
const FString FGenericCrashContext::UE4MinidumpName = TEXT( "UE4Minidump.dmp" );
const FString FGenericCrashContext::NewLineTag = TEXT( "&nl;" );

const FString FGenericCrashContext::CrashTypeCrash = TEXT("Crash");
const FString FGenericCrashContext::CrashTypeAssert = TEXT("Assert");
const FString FGenericCrashContext::CrashTypeEnsure = TEXT("Ensure");
const FString FGenericCrashContext::CrashTypeGPU = TEXT("GPUCrash");

const FString FGenericCrashContext::EngineModeExUnknown = TEXT("Unset");
const FString FGenericCrashContext::EngineModeExDirty = TEXT("Dirty");
const FString FGenericCrashContext::EngineModeExVanilla = TEXT("Vanilla");

bool FGenericCrashContext::bIsInitialized = false;
FPlatformMemoryStats FGenericCrashContext::CrashMemoryStats = FPlatformMemoryStats();
int32 FGenericCrashContext::StaticCrashContextIndex = 0;

namespace NCachedCrashContextProperties
{
	static bool bIsInternalBuild;
	static bool bIsPerforceBuild;
	static bool bIsSourceDistribution;
	static bool bIsUE4Release;
	static TOptional<bool> bIsVanilla;
	static FString GameName;
	static FString ExecutableName;
	static FString PlatformName;
	static FString PlatformNameIni;
	static FString DeploymentName;
	static FString BaseDir;
	static FString RootDir;
	static FString EpicAccountId;
	static FString LoginIdStr;
	static FString OsVersion;
	static FString OsSubVersion;
	static int32 NumberOfCores;
	static int32 NumberOfCoresIncludingHyperthreads;
	static FString CPUVendor;
	static FString CPUBrand;
	static FString PrimaryGPUBrand;
	static FString UserName;
	static FString DefaultLocale;
	static int32 CrashDumpMode;
	static int32 SecondsSinceStart;
	static FString CrashGUIDRoot;
	static FString UserActivityHint;
	static FString GameSessionID;
	static FString CommandLine;
	static int32 LanguageLCID;
	static FString CrashReportClientRichText;
	static TArray<FString> EnabledPluginsList;

}

void FGenericCrashContext::Initialize()
{
#if !NOINITCRASHREPORTER
	NCachedCrashContextProperties::bIsInternalBuild = FEngineBuildSettings::IsInternalBuild();
	NCachedCrashContextProperties::bIsPerforceBuild = FEngineBuildSettings::IsPerforceBuild();
	NCachedCrashContextProperties::bIsSourceDistribution = FEngineBuildSettings::IsSourceDistribution();
	NCachedCrashContextProperties::bIsUE4Release = FApp::IsEngineInstalled();

	NCachedCrashContextProperties::GameName = FString::Printf( TEXT("UE4-%s"), FApp::GetProjectName() );
	NCachedCrashContextProperties::ExecutableName = FPlatformProcess::ExecutableName();
	NCachedCrashContextProperties::PlatformName = FPlatformProperties::PlatformName();
	NCachedCrashContextProperties::PlatformNameIni = FPlatformProperties::IniPlatformName();
	NCachedCrashContextProperties::DeploymentName = FApp::GetDeploymentName();
	NCachedCrashContextProperties::BaseDir = FPlatformProcess::BaseDir();
	NCachedCrashContextProperties::RootDir = FPlatformMisc::RootDir();
	NCachedCrashContextProperties::EpicAccountId = FPlatformMisc::GetEpicAccountId();
	NCachedCrashContextProperties::LoginIdStr = FPlatformMisc::GetLoginId();
	FPlatformMisc::GetOSVersions(NCachedCrashContextProperties::OsVersion, NCachedCrashContextProperties::OsSubVersion);
	NCachedCrashContextProperties::NumberOfCores = FPlatformMisc::NumberOfCores();
	NCachedCrashContextProperties::NumberOfCoresIncludingHyperthreads = FPlatformMisc::NumberOfCoresIncludingHyperthreads();

	NCachedCrashContextProperties::CPUVendor = FPlatformMisc::GetCPUVendor();
	NCachedCrashContextProperties::CPUBrand = FPlatformMisc::GetCPUBrand();
	NCachedCrashContextProperties::PrimaryGPUBrand = FPlatformMisc::GetPrimaryGPUBrand();
	NCachedCrashContextProperties::UserName = FPlatformProcess::UserName();
	NCachedCrashContextProperties::DefaultLocale = FPlatformMisc::GetDefaultLocale();
	NCachedCrashContextProperties::CommandLine = FCommandLine::IsInitialized() ? FCommandLine::GetOriginalForLogging() : TEXT(""); 

	if (FInternationalization::IsAvailable())
	{
		NCachedCrashContextProperties::LanguageLCID = FInternationalization::Get().GetCurrentCulture()->GetLCID();
	}
	else
	{
		FCulturePtr DefaultCulture = FInternationalization::Get().GetCulture(TEXT("en"));
		if (DefaultCulture.IsValid())
		{
			NCachedCrashContextProperties::LanguageLCID = DefaultCulture->GetLCID();
		}
		else
		{
			const int DefaultCultureLCID = 1033;
			NCachedCrashContextProperties::LanguageLCID = DefaultCultureLCID;
		}
	}

	// Using the -fullcrashdump parameter will cause full memory minidumps to be created for crashes
	NCachedCrashContextProperties::CrashDumpMode = (int32)ECrashDumpMode::Default;
	if (FPlatformMisc::SupportsFullCrashDumps() && FCommandLine::IsInitialized())
	{
		const TCHAR* CmdLine = FCommandLine::Get();
		if (FParse::Param( CmdLine, TEXT("fullcrashdumpalways") ))
		{
			NCachedCrashContextProperties::CrashDumpMode = (int32)ECrashDumpMode::FullDumpAlways;
		}
		else if (FParse::Param( CmdLine, TEXT("fullcrashdump") ))
		{
			NCachedCrashContextProperties::CrashDumpMode = (int32)ECrashDumpMode::FullDump;
		}
	}

	const FGuid Guid = FGuid::NewGuid();
	NCachedCrashContextProperties::CrashGUIDRoot = FString::Printf(TEXT("%s%s-%s"), *CrashGUIDRootPrefix, *NCachedCrashContextProperties::PlatformNameIni, *Guid.ToString(EGuidFormats::Digits));

	// Initialize delegate for updating SecondsSinceStart, because FPlatformTime::Seconds() is not POSIX safe.
	const float PollingInterval = 1.0f;
	FTicker::GetCoreTicker().AddTicker( FTickerDelegate::CreateLambda( []( float DeltaTime )
	{
		NCachedCrashContextProperties::SecondsSinceStart = int32(FPlatformTime::Seconds() - GStartTime);
		return true;
	} ), PollingInterval );

	FCoreDelegates::UserActivityStringChanged.AddLambda([](const FString& InUserActivity)
	{
		NCachedCrashContextProperties::UserActivityHint = InUserActivity;
	});

	FCoreDelegates::GameSessionIDChanged.AddLambda([](const FString& InGameSessionID)
	{
		NCachedCrashContextProperties::GameSessionID = InGameSessionID;
	});

	FCoreDelegates::CrashOverrideParamsChanged.AddLambda([](const FCrashOverrideParameters& InParams)
	{
		NCachedCrashContextProperties::CrashReportClientRichText = InParams.CrashReportClientMessageText;
	});

	FCoreDelegates::IsVanillaProductChanged.AddLambda([](bool bIsVanilla)
	{
		NCachedCrashContextProperties::bIsVanilla = bIsVanilla;
	});

	FCoreDelegates::ConfigReadyForUse.AddStatic(FGenericCrashContext::InitializeFromConfig);

	bIsInitialized = true;
#endif	// !NOINITCRASHREPORTER
}

void FGenericCrashContext::InitializeFromConfig()
{
#if !NOINITCRASHREPORTER
	PurgeOldCrashConfig();

	const bool bForceGetSection = false;
	const bool bConstSection = true;
	FConfigSection* CRCConfigSection = GConfig->GetSectionPrivate(*ConfigSectionName, bForceGetSection, bConstSection, GEngineIni);

	if (CRCConfigSection != nullptr)
	{
		// Create a config file and save to a temp location. This file will be copied to
		// the crash folder for all crash reports create by this session.
		FConfigFile CrashConfigFile;

		FConfigSection CRCConfigSectionCopy(*CRCConfigSection);
		CrashConfigFile.Add(ConfigSectionName, CRCConfigSectionCopy);

		CrashConfigFile.Dirty = true;
		CrashConfigFile.Write(GetCrashConfigFilePath());
	}
#endif	// !NOINITCRASHREPORTER
}

FGenericCrashContext::FGenericCrashContext()
	: bIsEnsure(false)
{
	CommonBuffer.Reserve( 32768 );
	CrashContextIndex = StaticCrashContextIndex++;
}

void FGenericCrashContext::SerializeContentToBuffer()
{
	TCHAR CrashGUID[CrashGUIDLength];
	GetUniqueCrashName(CrashGUID, CrashGUIDLength);

	// Must conform against:
	// https://www.securecoding.cert.org/confluence/display/seccode/SIG30-C.+Call+only+asynchronous-safe+functions+within+signal+handlers
	AddHeader();

	BeginSection( *RuntimePropertiesTag );
	AddCrashProperty( TEXT( "CrashVersion" ), (int32)ECrashDescVersions::VER_3_CrashContext );
	AddCrashProperty( TEXT( "CrashGUID" ), (const TCHAR*)CrashGUID);
	AddCrashProperty( TEXT( "ProcessId" ), FPlatformProcess::GetCurrentProcessId() );
	AddCrashProperty( TEXT( "IsInternalBuild" ), NCachedCrashContextProperties::bIsInternalBuild );
	AddCrashProperty( TEXT( "IsPerforceBuild" ), NCachedCrashContextProperties::bIsPerforceBuild );
	AddCrashProperty( TEXT( "IsSourceDistribution" ), NCachedCrashContextProperties::bIsSourceDistribution );
	AddCrashProperty( TEXT( "IsEnsure" ), bIsEnsure );
	AddCrashProperty( TEXT( "IsAssert" ), FDebug::bHasAsserted );
	AddCrashProperty( TEXT( "CrashType" ), GetCrashTypeString(bIsEnsure, FDebug::bHasAsserted, GIsGPUCrashed) );

	AddCrashProperty( TEXT( "SecondsSinceStart" ), NCachedCrashContextProperties::SecondsSinceStart );

	// Add common crash properties.
	AddCrashProperty( TEXT( "GameName" ), *NCachedCrashContextProperties::GameName );
	AddCrashProperty( TEXT( "ExecutableName" ), *NCachedCrashContextProperties::ExecutableName );
	AddCrashProperty( TEXT( "BuildConfiguration" ), EBuildConfigurations::ToString( FApp::GetBuildConfiguration() ) );
	AddCrashProperty( TEXT( "GameSessionID" ), *NCachedCrashContextProperties::GameSessionID );

	AddCrashProperty( TEXT( "PlatformName" ), *NCachedCrashContextProperties::PlatformName );
	AddCrashProperty( TEXT( "PlatformNameIni" ), *NCachedCrashContextProperties::PlatformNameIni );
	AddCrashProperty( TEXT( "EngineMode" ), FPlatformMisc::GetEngineMode() );
	AddCrashProperty( TEXT( "EngineModeEx" ), EngineModeExString());

	AddCrashProperty( TEXT( "DeploymentName"), *NCachedCrashContextProperties::DeploymentName );

	AddCrashProperty( TEXT( "EngineVersion" ), *FEngineVersion::Current().ToString() );
	AddCrashProperty( TEXT( "CommandLine" ), *NCachedCrashContextProperties::CommandLine );
	AddCrashProperty( TEXT( "LanguageLCID" ), NCachedCrashContextProperties::LanguageLCID );
	AddCrashProperty( TEXT( "AppDefaultLocale" ), *NCachedCrashContextProperties::DefaultLocale );
	AddCrashProperty( TEXT( "BuildVersion" ), FApp::GetBuildVersion() );
	AddCrashProperty( TEXT( "IsUE4Release" ), NCachedCrashContextProperties::bIsUE4Release );

	// Remove periods from user names to match AutoReporter user names
	// The name prefix is read by CrashRepository.AddNewCrash in the website code
	const bool bSendUserName = NCachedCrashContextProperties::bIsInternalBuild;
	AddCrashProperty( TEXT( "UserName" ), bSendUserName ? *NCachedCrashContextProperties::UserName.Replace( TEXT( "." ), TEXT( "" ) ) : TEXT( "" ) );

	AddCrashProperty( TEXT( "BaseDir" ), *NCachedCrashContextProperties::BaseDir );
	AddCrashProperty( TEXT( "RootDir" ), *NCachedCrashContextProperties::RootDir );
	AddCrashProperty( TEXT( "MachineId" ), *NCachedCrashContextProperties::LoginIdStr.ToUpper() );
	AddCrashProperty( TEXT( "LoginId" ), *NCachedCrashContextProperties::LoginIdStr );
	AddCrashProperty( TEXT( "EpicAccountId" ), *NCachedCrashContextProperties::EpicAccountId );

	AddCrashProperty( TEXT( "CallStack" ), TEXT( "" ) );
	AddCrashProperty( TEXT( "SourceContext" ), TEXT( "" ) );
	AddCrashProperty( TEXT( "UserDescription" ), TEXT( "" ) );
	AddCrashProperty( TEXT( "UserActivityHint" ), *NCachedCrashContextProperties::UserActivityHint );
	AddCrashProperty( TEXT( "ErrorMessage" ), (const TCHAR*)GErrorMessage ); // GErrorMessage may be broken.
	AddCrashProperty( TEXT( "CrashDumpMode" ), NCachedCrashContextProperties::CrashDumpMode );
	AddCrashProperty( TEXT( "CrashReporterMessage" ), *NCachedCrashContextProperties::CrashReportClientRichText );

	// Add misc stats.
	AddCrashProperty( TEXT( "Misc.NumberOfCores" ), NCachedCrashContextProperties::NumberOfCores );
	AddCrashProperty( TEXT( "Misc.NumberOfCoresIncludingHyperthreads" ), NCachedCrashContextProperties::NumberOfCoresIncludingHyperthreads );
	AddCrashProperty( TEXT( "Misc.Is64bitOperatingSystem" ), (int32)FPlatformMisc::Is64bitOperatingSystem() );

	AddCrashProperty( TEXT( "Misc.CPUVendor" ), *NCachedCrashContextProperties::CPUVendor );
	AddCrashProperty( TEXT( "Misc.CPUBrand" ), *NCachedCrashContextProperties::CPUBrand );
	AddCrashProperty( TEXT( "Misc.PrimaryGPUBrand" ), *NCachedCrashContextProperties::PrimaryGPUBrand );
	AddCrashProperty( TEXT( "Misc.OSVersionMajor" ), *NCachedCrashContextProperties::OsVersion );
	AddCrashProperty( TEXT( "Misc.OSVersionMinor" ), *NCachedCrashContextProperties::OsSubVersion );


	// #CrashReport: 2015-07-21 Move to the crash report client.
	/*{
		uint64 AppDiskTotalNumberOfBytes = 0;
		uint64 AppDiskNumberOfFreeBytes = 0;
		FPlatformMisc::GetDiskTotalAndFreeSpace( FPlatformProcess::BaseDir(), AppDiskTotalNumberOfBytes, AppDiskNumberOfFreeBytes );
		AddCrashProperty( TEXT( "Misc.AppDiskTotalNumberOfBytes" ), AppDiskTotalNumberOfBytes );
		AddCrashProperty( TEXT( "Misc.AppDiskNumberOfFreeBytes" ), AppDiskNumberOfFreeBytes );
	}*/

	// FPlatformMemory::GetConstants is called in the GCreateMalloc, so we can assume it is always valid.
	{
		// Add memory stats.
		const FPlatformMemoryConstants& MemConstants = FPlatformMemory::GetConstants();

		AddCrashProperty( TEXT( "MemoryStats.TotalPhysical" ), (uint64)MemConstants.TotalPhysical );
		AddCrashProperty( TEXT( "MemoryStats.TotalVirtual" ), (uint64)MemConstants.TotalVirtual );
		AddCrashProperty( TEXT( "MemoryStats.PageSize" ), (uint64)MemConstants.PageSize );
		AddCrashProperty( TEXT( "MemoryStats.TotalPhysicalGB" ), MemConstants.TotalPhysicalGB );
	}

	AddCrashProperty( TEXT( "MemoryStats.AvailablePhysical" ), (uint64)CrashMemoryStats.AvailablePhysical );
	AddCrashProperty( TEXT( "MemoryStats.AvailableVirtual" ), (uint64)CrashMemoryStats.AvailableVirtual );
	AddCrashProperty( TEXT( "MemoryStats.UsedPhysical" ), (uint64)CrashMemoryStats.UsedPhysical );
	AddCrashProperty( TEXT( "MemoryStats.PeakUsedPhysical" ), (uint64)CrashMemoryStats.PeakUsedPhysical );
	AddCrashProperty( TEXT( "MemoryStats.UsedVirtual" ), (uint64)CrashMemoryStats.UsedVirtual );
	AddCrashProperty( TEXT( "MemoryStats.PeakUsedVirtual" ), (uint64)CrashMemoryStats.PeakUsedVirtual );
	AddCrashProperty( TEXT( "MemoryStats.bIsOOM" ), (int32)FPlatformMemory::bIsOOM );
	AddCrashProperty( TEXT( "MemoryStats.OOMAllocationSize"), (uint64)FPlatformMemory::OOMAllocationSize );
	AddCrashProperty( TEXT( "MemoryStats.OOMAllocationAlignment"), (int32)FPlatformMemory::OOMAllocationAlignment );

	//Architecture
	//CrashedModuleName
	//LoadedModules
	EndSection( *RuntimePropertiesTag );
	
	// Add platform specific properties.
	BeginSection( *PlatformPropertiesTag );
	AddPlatformSpecificProperties();
	EndSection( *PlatformPropertiesTag );

	if(NCachedCrashContextProperties::EnabledPluginsList.Num() > 0)
	{
		BeginSection(*EnabledPluginsTag);

		for (const FString& Str : NCachedCrashContextProperties::EnabledPluginsList)
		{
			AddCrashProperty(TEXT("Plugin"), *Str);
		}

		EndSection(*EnabledPluginsTag);
	}

	AddFooter();
}

void FGenericCrashContext::GetUniqueCrashName(TCHAR* GUIDBuffer, int32 BufferSize) const
{
	FCString::Snprintf(GUIDBuffer, BufferSize, TEXT("%s_%04i"), *NCachedCrashContextProperties::CrashGUIDRoot, CrashContextIndex);
}

const bool FGenericCrashContext::IsFullCrashDump() const
{
	return (NCachedCrashContextProperties::CrashDumpMode == (int32)ECrashDumpMode::FullDump) ||
		(NCachedCrashContextProperties::CrashDumpMode == (int32)ECrashDumpMode::FullDumpAlways);
}

const bool FGenericCrashContext::IsFullCrashDumpOnEnsure() const
{
	return (NCachedCrashContextProperties::CrashDumpMode == (int32)ECrashDumpMode::FullDumpAlways);
}

void FGenericCrashContext::SerializeAsXML( const TCHAR* Filename )
{
	SerializeContentToBuffer();
	// Use OS build-in functionality instead.
	FFileHelper::SaveStringToFile( CommonBuffer, Filename, FFileHelper::EEncodingOptions::AutoDetect );
}

void FGenericCrashContext::AddCrashProperty( const TCHAR* PropertyName, const TCHAR* PropertyValue )
{
	CommonBuffer += TEXT( "<" );
	CommonBuffer += PropertyName;
	CommonBuffer += TEXT( ">" );


	CommonBuffer += EscapeXMLString( PropertyValue );

	CommonBuffer += TEXT( "</" );
	CommonBuffer += PropertyName;
	CommonBuffer += TEXT( ">" );
	CommonBuffer += LINE_TERMINATOR;
}

void FGenericCrashContext::AddPlatformSpecificProperties()
{
	// Nothing really to do here. Can be overridden by the platform code.
	// @see FWindowsPlatformCrashContext::AddPlatformSpecificProperties
}

void FGenericCrashContext::AddHeader()
{
	CommonBuffer += TEXT( "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" ) LINE_TERMINATOR;
	BeginSection( TEXT("FGenericCrashContext") );
}

void FGenericCrashContext::AddFooter()
{
	EndSection( TEXT( "FGenericCrashContext" ) );
}

void FGenericCrashContext::BeginSection( const TCHAR* SectionName )
{
	CommonBuffer += TEXT( "<" );
	CommonBuffer += SectionName;
	CommonBuffer += TEXT( ">" );
	CommonBuffer += LINE_TERMINATOR;
}

void FGenericCrashContext::EndSection( const TCHAR* SectionName )
{
	CommonBuffer += TEXT( "</" );
	CommonBuffer += SectionName;
	CommonBuffer += TEXT( ">" );
	CommonBuffer += LINE_TERMINATOR;
}

FString FGenericCrashContext::EscapeXMLString( const FString& Text )
{
	return Text
		.Replace(TEXT("&"), TEXT("&amp;"))
		.Replace(TEXT("\""), TEXT("&quot;"))
		.Replace(TEXT("'"), TEXT("&apos;"))
		.Replace(TEXT("<"), TEXT("&lt;"))
		.Replace(TEXT(">"), TEXT("&gt;"))
		.Replace( TEXT( "\r" ), TEXT("") );
}

FString FGenericCrashContext::UnescapeXMLString( const FString& Text )
{
	return Text
		.Replace(TEXT("&amp;"), TEXT("&"))
		.Replace(TEXT("&quot;"), TEXT("\""))
		.Replace(TEXT("&apos;"), TEXT("'"))
		.Replace(TEXT("&lt;"), TEXT("<"))
		.Replace(TEXT("&gt;"), TEXT(">"));
}

const TCHAR* FGenericCrashContext::GetCrashTypeString(bool InIsEnsure, bool InIsAssert, bool bIsGPUCrashed)
{
	if (bIsGPUCrashed)
	{
		return *CrashTypeGPU;
	}
	if (InIsEnsure)
	{
		return *CrashTypeEnsure;
	}
	else if (InIsAssert)
	{
		return *CrashTypeAssert;
	}

	return *CrashTypeCrash;
}

const TCHAR* FGenericCrashContext::EngineModeExString()
{
	return !NCachedCrashContextProperties::bIsVanilla.IsSet() ? *FGenericCrashContext::EngineModeExUnknown :
		(NCachedCrashContextProperties::bIsVanilla.GetValue() ? *FGenericCrashContext::EngineModeExVanilla : *FGenericCrashContext::EngineModeExDirty);
}

const TCHAR* FGenericCrashContext::GetCrashConfigFilePath()
{
	static FString CrashConfigFilePath;
	if (CrashConfigFilePath.IsEmpty())
	{
		CrashConfigFilePath = FPaths::Combine(GetCrashConfigFolder(), *NCachedCrashContextProperties::CrashGUIDRoot, FGenericCrashContext::CrashConfigFileNameW);
	}
	return *CrashConfigFilePath;
}

const TCHAR* FGenericCrashContext::GetCrashConfigFolder()
{
	static FString CrashConfigFolder;
	if (CrashConfigFolder.IsEmpty())
	{
		CrashConfigFolder = FPaths::Combine(*FPaths::GeneratedConfigDir(), TEXT("CrashReportClient"));
	}
	return *CrashConfigFolder;
}

void FGenericCrashContext::PurgeOldCrashConfig()
{
	int32 PurgeDays = 2;
	GConfig->GetInt(*ConfigSectionName, *CrashConfigPurgeDays, PurgeDays, GEngineIni);

	if (PurgeDays > 0)
	{
		IFileManager& FileManager = IFileManager::Get();

		// Delete items older than PurgeDays
		TArray<FString> Directories;
		FileManager.FindFiles(Directories, *(FPaths::Combine(GetCrashConfigFolder(), *CrashGUIDRootPrefix) + TEXT("*")), false, true);

		for (const FString& Dir : Directories)
		{
			const FString CrashConfigDirectory = FPaths::Combine(GetCrashConfigFolder(), *Dir);
			const FDateTime DirectoryAccessTime = FileManager.GetTimeStamp(*CrashConfigDirectory);
			if (FDateTime::Now() - DirectoryAccessTime > FTimespan::FromDays(PurgeDays))
			{
				FileManager.DeleteDirectory(*CrashConfigDirectory, false, true);
			}
		}
	}
}

void FGenericCrashContext::AddPlugin(const FString& PluginDesc)
{
	NCachedCrashContextProperties::EnabledPluginsList.Add(PluginDesc);
}

FProgramCounterSymbolInfoEx::FProgramCounterSymbolInfoEx( FString InModuleName, FString InFunctionName, FString InFilename, uint32 InLineNumber, uint64 InSymbolDisplacement, uint64 InOffsetInModule, uint64 InProgramCounter ) :
	ModuleName( InModuleName ),
	FunctionName( InFunctionName ),
	Filename( InFilename ),
	LineNumber( InLineNumber ),
	SymbolDisplacement( InSymbolDisplacement ),
	OffsetInModule( InOffsetInModule ),
	ProgramCounter( InProgramCounter )
{

}
