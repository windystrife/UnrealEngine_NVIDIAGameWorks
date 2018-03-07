// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "PlatformInfo.h"
#include "DesktopPlatformPrivate.h"

#define LOCTEXT_NAMESPACE "PlatformInfo"

namespace PlatformInfo
{

namespace
{

FPlatformInfo BuildPlatformInfo(const FName& InPlatformInfoName, const FName& InTargetPlatformName, const FText& InDisplayName, const EPlatformType InPlatformType, const EPlatformFlags::Flags InPlatformFlags, const FPlatformIconPaths& InIconPaths, const FString& InUATCommandLine, const FString& InAutoSDKPath, EPlatformSDKStatus InStatus, const FString& InTutorial, bool InEnabled, FString InBinaryFolderName, FString InIniPlatformName, bool InUsesHostCompiler, bool InUATClosesAfterLaunch, bool InIsConfidential, const FName& InUBTTargetId)
{
	FPlatformInfo PlatformInfo;

	PlatformInfo.PlatformInfoName = InPlatformInfoName;
	PlatformInfo.TargetPlatformName = InTargetPlatformName;

	// See if this name also contains a flavor
	const FString InPlatformInfoNameString = InPlatformInfoName.ToString();
	{
		int32 UnderscoreLoc;
		if(InPlatformInfoNameString.FindChar(TEXT('_'), UnderscoreLoc))
		{
			PlatformInfo.VanillaPlatformName = *InPlatformInfoNameString.Mid(0, UnderscoreLoc);
			PlatformInfo.PlatformFlavor = *InPlatformInfoNameString.Mid(UnderscoreLoc + 1);
		}
		else
		{
			PlatformInfo.VanillaPlatformName = InPlatformInfoName;
		}
	}

	PlatformInfo.DisplayName = InDisplayName;
	PlatformInfo.PlatformType = InPlatformType;
	PlatformInfo.PlatformFlags = InPlatformFlags;
	PlatformInfo.IconPaths = InIconPaths;
	PlatformInfo.UATCommandLine = InUATCommandLine;
	PlatformInfo.AutoSDKPath = InAutoSDKPath;
	PlatformInfo.BinaryFolderName = InBinaryFolderName;
	PlatformInfo.IniPlatformName = InIniPlatformName;
	PlatformInfo.UBTTargetId = InUBTTargetId;

	// Generate the icon style names for FEditorStyle
	PlatformInfo.IconPaths.NormalStyleName = *FString::Printf(TEXT("Launcher.Platform_%s"), *InPlatformInfoNameString);
	PlatformInfo.IconPaths.LargeStyleName  = *FString::Printf(TEXT("Launcher.Platform_%s.Large"), *InPlatformInfoNameString);
	PlatformInfo.IconPaths.XLargeStyleName = *FString::Printf(TEXT("Launcher.Platform_%s.XLarge"), *InPlatformInfoNameString);

	// SDK data
	PlatformInfo.SDKStatus = InStatus;
	PlatformInfo.SDKTutorial = InTutorial;

	// Distribution data
	PlatformInfo.bEnabledForUse = InEnabled;
	PlatformInfo.bUsesHostCompiler = InUsesHostCompiler;
	PlatformInfo.bUATClosesAfterLaunch = InUATClosesAfterLaunch;
	PlatformInfo.bIsConfidential = InIsConfidential;
	return PlatformInfo;
}

#if PLATFORM_WINDOWS
static const bool IsAvailableOnWindows = true;
static const bool IsAvailableOnMac = false;
static const bool IsAvailableOnLinux = false;
static const FString IOSTutorial = TEXT("/Engine/Tutorial/Mobile/InstallingiTunesTutorial.InstallingiTunesTutorial");
#elif PLATFORM_MAC
static const bool IsAvailableOnWindows = false;
static const bool IsAvailableOnMac = true;
static const bool IsAvailableOnLinux = false;
static const FString IOSTutorial = TEXT("/Engine/Tutorial/Installation/InstallingXCodeTutorial.InstallingXCodeTutorial");
#elif PLATFORM_LINUX
static const bool IsAvailableOnWindows = false;
static const bool IsAvailableOnMac = false;
static const bool IsAvailableOnLinux = true;
static const FString IOSTutorial = TEXT("/Engine/Tutorial/NotYetImplemented");
#endif

static const FPlatformInfo AllPlatformInfoArray[] = {
	// PlatformInfoName									TargetPlatformName			DisplayName														PlatformType			PlatformFlags					IconPaths																																		UATCommandLine										AutoSDKPath			SDKStatus						SDKTutorial																								bEnabledForUse										BinaryFolderName	IniPlatformName		FbUsesHostCompiler		bUATClosesAfterLaunch	bIsConfidential	UBTTargetId (match UBT's UnrealTargetPlatform enum)
	BuildPlatformInfo(TEXT("WindowsNoEditor"),			TEXT("WindowsNoEditor"),	LOCTEXT("WindowsNoEditor", "Windows"),							EPlatformType::Game,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Windows/Platform_WindowsNoEditor_24x"), TEXT("Launcher/Windows/Platform_WindowsNoEditor_128x")),	TEXT("-targetplatform=Win64"),						TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Installation/InstallingVisualStudioTutorial.InstallingVisualStudioTutorial"),	IsAvailableOnWindows,											TEXT("Win64"),		TEXT("Windows"),	IsAvailableOnWindows,	false,					false,			TEXT("Win64")),
	BuildPlatformInfo(TEXT("WindowsNoEditor_Win32"),	TEXT("WindowsNoEditor"),	LOCTEXT("WindowsNoEditor_Win32", "Windows (32-bit)"),			EPlatformType::Game,	EPlatformFlags::BuildFlavor,	FPlatformIconPaths(TEXT("Launcher/Windows/Platform_WindowsNoEditor_24x"), TEXT("Launcher/Windows/Platform_WindowsNoEditor_128x")),	TEXT("-targetplatform=Win32"),						TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Installation/InstallingVisualStudioTutorial.InstallingVisualStudioTutorial"),	IsAvailableOnWindows,											TEXT("Win32"),		TEXT("Windows"),	IsAvailableOnWindows,	false,					false,			TEXT("Win32")),
	BuildPlatformInfo(TEXT("WindowsNoEditor_Win64"),	TEXT("WindowsNoEditor"),	LOCTEXT("WindowsNoEditor_Win64", "Windows (64-bit)"),			EPlatformType::Game,	EPlatformFlags::BuildFlavor,	FPlatformIconPaths(TEXT("Launcher/Windows/Platform_WindowsNoEditor_24x"), TEXT("Launcher/Windows/Platform_WindowsNoEditor_128x")),	TEXT("-targetplatform=Win64"),						TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Installation/InstallingVisualStudioTutorial.InstallingVisualStudioTutorial"),	IsAvailableOnWindows,											TEXT("Win64"),		TEXT("Windows"),	IsAvailableOnWindows,	false,					false,			TEXT("Win64")),
	BuildPlatformInfo(TEXT("Windows"),					TEXT("Windows"),			LOCTEXT("WindowsEditor", "Windows (Editor)"),					EPlatformType::Editor,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Windows/Platform_Windows_24x"), TEXT("Launcher/Windows/Platform_Windows_128x")),					TEXT(""),											TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Installation/InstallingVisualStudioTutorial.InstallingVisualStudioTutorial"),	IsAvailableOnWindows,											TEXT("Win64"),		TEXT("Windows"),	IsAvailableOnWindows,	false,					false,			TEXT("Win64")),
	BuildPlatformInfo(TEXT("WindowsClient"),			TEXT("WindowsClient"),		LOCTEXT("WindowsClient", "Windows (Client-only)"),				EPlatformType::Client,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Windows/Platform_Windows_24x"), TEXT("Launcher/Windows/Platform_Windows_128x")),					TEXT("-client"),									TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Installation/InstallingVisualStudioTutorial.InstallingVisualStudioTutorial"),	IsAvailableOnWindows,											TEXT("Win64"),		TEXT("Windows"),	IsAvailableOnWindows,	false,					false,			TEXT("Win64")),
	BuildPlatformInfo(TEXT("WindowsServer"),			TEXT("WindowsServer"),		LOCTEXT("WindowsServer", "Windows (Dedicated Server)"),			EPlatformType::Server,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Windows/Platform_WindowsServer_24x"), TEXT("Launcher/Windows/Platform_WindowsServer_128x")),		TEXT(""),											TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Installation/InstallingVisualStudioTutorial.InstallingVisualStudioTutorial"),	IsAvailableOnWindows,											TEXT("Win64"),		TEXT("Windows"),	IsAvailableOnWindows,	false,					false,			TEXT("Win64")),

	BuildPlatformInfo(TEXT("MacNoEditor"),				TEXT("MacNoEditor"),		LOCTEXT("MacNoEditor", "Mac"),									EPlatformType::Game,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Mac/Platform_Mac_24x"), TEXT("Launcher/Mac/Platform_Mac_128x")),									TEXT("-targetplatform=Mac"),						TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Installation/InstallingXCodeTutorial.InstallingXCodeTutorial"),					IsAvailableOnMac,												TEXT("Mac"),		TEXT("Mac"),		IsAvailableOnMac,		true,					false,			TEXT("Mac")),
	BuildPlatformInfo(TEXT("Mac"),						TEXT("Mac"),				LOCTEXT("MacEditor", "Mac (Editor)"),							EPlatformType::Editor,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Mac/Platform_Mac_24x"), TEXT("Launcher/Mac/Platform_Mac_128x")),									TEXT(""),											TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Installation/InstallingXCodeTutorial.InstallingXCodeTutorial"),					IsAvailableOnMac,												TEXT("Mac"),		TEXT("Mac"),		IsAvailableOnMac,		true,					false,			TEXT("Mac")),
	BuildPlatformInfo(TEXT("MacClient"),				TEXT("MacClient"),			LOCTEXT("MacClient", "Mac (Client-only)"),						EPlatformType::Client,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Mac/Platform_Mac_24x"), TEXT("Launcher/Mac/Platform_Mac_128x")),									TEXT("-client"),									TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Installation/InstallingXCodeTutorial.InstallingXCodeTutorial"),					IsAvailableOnMac,												TEXT("Mac"),		TEXT("Mac"),		IsAvailableOnMac,		true,					false,			TEXT("Mac")),
	BuildPlatformInfo(TEXT("MacServer"),				TEXT("MacServer"),			LOCTEXT("MacServer", "Mac (Dedicated Server)"),					EPlatformType::Server,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Mac/Platform_Mac_24x"), TEXT("Launcher/Mac/Platform_Mac_128x")),									TEXT(""),											TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Installation/InstallingXCodeTutorial.InstallingXCodeTutorial"),					IsAvailableOnMac,												TEXT("Mac"),		TEXT("Mac"),		IsAvailableOnMac,		true,					false,			TEXT("Mac")),

	BuildPlatformInfo(TEXT("LinuxNoEditor"),			TEXT("LinuxNoEditor"),		LOCTEXT("LinuxNoEditor", "Linux"),								EPlatformType::Game,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Linux/Platform_Linux_24x"), TEXT("Launcher/Linux/Platform_Linux_128x")),							TEXT("-targetplatform=Linux"),						TEXT("Linux_x64"),	EPlatformSDKStatus::Unknown,	TEXT("/Platforms/Linux/GettingStarted"),																IsAvailableOnLinux || IsAvailableOnWindows,						TEXT("Linux"),		TEXT("Linux"),		IsAvailableOnLinux,		true,					false,			TEXT("Linux")),
	BuildPlatformInfo(TEXT("Linux"),					TEXT("Linux"),				LOCTEXT("LinuxEditor", "Linux (Editor)"),						EPlatformType::Editor,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Linux/Platform_Linux_24x"), TEXT("Launcher/Linux/Platform_Linux_128x")),							TEXT(""),											TEXT("Linux_x64"),	EPlatformSDKStatus::Unknown,	TEXT("/Platforms/Linux/GettingStarted"),																IsAvailableOnLinux,												TEXT("Linux"),		TEXT("Linux"),		IsAvailableOnLinux,		true,					false,			TEXT("Linux")),
	BuildPlatformInfo(TEXT("LinuxClient"),				TEXT("LinuxClient"),		LOCTEXT("LinuxClient", "Linux (Client-only)"),					EPlatformType::Client,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Linux/Platform_Linux_24x"), TEXT("Launcher/Linux/Platform_Linux_128x")),							TEXT("-client"),									TEXT("Linux_x64"),	EPlatformSDKStatus::Unknown,	TEXT("/Platforms/Linux/GettingStarted"),																IsAvailableOnLinux || IsAvailableOnWindows,						TEXT("Linux"),		TEXT("Linux"),		IsAvailableOnLinux,		true,					false,			TEXT("Linux")),
	BuildPlatformInfo(TEXT("LinuxServer"),				TEXT("LinuxServer"),		LOCTEXT("LinuxServer", "Linux (Dedicated Server)"),				EPlatformType::Server,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Linux/Platform_Linux_24x"), TEXT("Launcher/Linux/Platform_Linux_128x")),							TEXT(""),											TEXT("Linux_x64"),	EPlatformSDKStatus::Unknown,	TEXT("/Platforms/Linux/GettingStarted"),																IsAvailableOnLinux || IsAvailableOnWindows,						TEXT("Linux"),		TEXT("Linux"),		IsAvailableOnLinux,		true,					false,			TEXT("Linux")),

	BuildPlatformInfo(TEXT("IOS"),						TEXT("IOS"),				LOCTEXT("IOS", "iOS"),											EPlatformType::Game,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/iOS/Platform_iOS_24x"), TEXT("Launcher/iOS/Platform_iOS_128x")),									TEXT(""),											TEXT(""),			EPlatformSDKStatus::Unknown,	IOSTutorial,																							IsAvailableOnWindows || IsAvailableOnMac,						TEXT("IOS"),		TEXT("IOS"),		false,					true,					false,			TEXT("IOS")),

	BuildPlatformInfo(TEXT("Android"),					TEXT("Android"),			LOCTEXT("Android", "Android"),									EPlatformType::Game,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Android/Platform_Android_24x"), TEXT("Launcher/Android/Platform_Android_128x")),					TEXT(""),											TEXT("Android"),	EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Mobile/SettingUpAndroidTutorial.SettingUpAndroidTutorial"),						IsAvailableOnLinux || IsAvailableOnWindows || IsAvailableOnMac,	TEXT("Android"),	TEXT("Android"),	false,					true,					false,			TEXT("Android")),
	BuildPlatformInfo(TEXT("Android_Multi"),			TEXT("Android_Multi"),		LOCTEXT("Android_Multi", "Android (Multi)"),					EPlatformType::Game,	EPlatformFlags::CookFlavor,		FPlatformIconPaths(TEXT("Launcher/Android/Platform_Android_24x"), TEXT("Launcher/Android/Platform_Android_128x")),					TEXT("-targetplatform=Android -cookflavor=Multi"),	TEXT("Android"),	EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Mobile/SettingUpAndroidTutorial.SettingUpAndroidTutorial"),						IsAvailableOnLinux || IsAvailableOnWindows || IsAvailableOnMac,	TEXT("Android"),	TEXT("Android"),	false,					true,					false,			TEXT("Android")),
	BuildPlatformInfo(TEXT("Android_ATC"),				TEXT("Android_ATC"),		LOCTEXT("Android_ATC", "Android (ATC)"),						EPlatformType::Game,	EPlatformFlags::CookFlavor,		FPlatformIconPaths(TEXT("Launcher/Android/Platform_Android_ATC_24x"), TEXT("Launcher/Android/Platform_Android_128x")),				TEXT("-targetplatform=Android -cookflavor=ATC"),	TEXT("Android"),	EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Mobile/SettingUpAndroidTutorial.SettingUpAndroidTutorial"),						IsAvailableOnLinux || IsAvailableOnWindows || IsAvailableOnMac,	TEXT("Android"),	TEXT("Android"),	false,					true,					false,			TEXT("Android")),
	BuildPlatformInfo(TEXT("Android_DXT"),				TEXT("Android_DXT"),		LOCTEXT("Android_DXT", "Android (DXT)"),						EPlatformType::Game,	EPlatformFlags::CookFlavor,		FPlatformIconPaths(TEXT("Launcher/Android/Platform_Android_DXT_24x"), TEXT("Launcher/Android/Platform_Android_128x")),				TEXT("-targetplatform=Android -cookflavor=DXT"),	TEXT("Android"),	EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Mobile/SettingUpAndroidTutorial.SettingUpAndroidTutorial"),						IsAvailableOnLinux || IsAvailableOnWindows || IsAvailableOnMac,	TEXT("Android"),	TEXT("Android"),	false,					true,					false,			TEXT("Android")),
	BuildPlatformInfo(TEXT("Android_ETC1"),				TEXT("Android_ETC1"),		LOCTEXT("Android_ETC1", "Android (ETC1)"),						EPlatformType::Game,	EPlatformFlags::CookFlavor,		FPlatformIconPaths(TEXT("Launcher/Android/Platform_Android_ETC1_24x"), TEXT("Launcher/Android/Platform_Android_128x")),				TEXT("-targetplatform=Android -cookflavor=ETC1"),	TEXT("Android"),	EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Mobile/SettingUpAndroidTutorial.SettingUpAndroidTutorial"),						IsAvailableOnLinux || IsAvailableOnWindows || IsAvailableOnMac,	TEXT("Android"),	TEXT("Android"),	false,					true,					false,			TEXT("Android")),
	BuildPlatformInfo(TEXT("Android_ETC2"),				TEXT("Android_ETC2"),		LOCTEXT("Android_ETC2", "Android (ETC2)"),						EPlatformType::Game,	EPlatformFlags::CookFlavor,		FPlatformIconPaths(TEXT("Launcher/Android/Platform_Android_ETC2_24x"), TEXT("Launcher/Android/Platform_Android_128x")),				TEXT("-targetplatform=Android -cookflavor=ETC2"),	TEXT("Android"),	EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Mobile/SettingUpAndroidTutorial.SettingUpAndroidTutorial"),						IsAvailableOnLinux || IsAvailableOnWindows || IsAvailableOnMac,	TEXT("Android"),	TEXT("Android"),	false,					true,					false,			TEXT("Android")),
	BuildPlatformInfo(TEXT("Android_PVRTC"),			TEXT("Android_PVRTC"),		LOCTEXT("Android_PVRTC", "Android (PVRTC)"),					EPlatformType::Game,	EPlatformFlags::CookFlavor,		FPlatformIconPaths(TEXT("Launcher/Android/Platform_Android_PVRTC_24x"), TEXT("Launcher/Android/Platform_Android_128x")),			TEXT("-targetplatform=Android -cookflavor=PVRTC"),	TEXT("Android"),	EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Mobile/SettingUpAndroidTutorial.SettingUpAndroidTutorial"),						IsAvailableOnLinux || IsAvailableOnWindows || IsAvailableOnMac,	TEXT("Android"),	TEXT("Android"),	false,					true,					false,			TEXT("Android")),
	BuildPlatformInfo(TEXT("Android_ASTC"),				TEXT("Android_ASTC"),		LOCTEXT("Android_ASTC", "Android (ASTC)"),						EPlatformType::Game,	EPlatformFlags::CookFlavor,		FPlatformIconPaths(TEXT("Launcher/Android/Platform_Android_ASTC_24x"), TEXT("Launcher/Android/Platform_Android_128x")),				TEXT("-targetplatform=Android -cookflavor=ASTC"),	TEXT("Android"),	EPlatformSDKStatus::Unknown,	TEXT("/Engine/Tutorial/Mobile/SettingUpAndroidTutorial.SettingUpAndroidTutorial"),						IsAvailableOnLinux || IsAvailableOnWindows || IsAvailableOnMac,	TEXT("Android"),	TEXT("Android"),	false,					true,					false,			TEXT("Android")),

	BuildPlatformInfo(TEXT("HTML5"),					TEXT("HTML5"),				LOCTEXT("HTML5", "HTML5"),										EPlatformType::Game,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/HTML5/Platform_HTML5_24x"), TEXT("Launcher/HTML5/Platform_HTML5_128x")),							TEXT(""),											TEXT("HTML5"),		EPlatformSDKStatus::Unknown,	TEXT("/Platforms/HTML5/GettingStarted"),																IsAvailableOnLinux || IsAvailableOnWindows || IsAvailableOnMac,	TEXT("HTML5"),		TEXT("HTML5"),		false,					true,					false,			TEXT("HTML5")),

	BuildPlatformInfo(TEXT("PS4"),						TEXT("PS4"),				LOCTEXT("PS4", "PlayStation 4"),								EPlatformType::Game,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/PS4/Platform_PS4_24x"), TEXT("Launcher/PS4/Platform_PS4_128x")),									TEXT(""),											TEXT("PS4"),		EPlatformSDKStatus::Unknown,	TEXT("/Platforms/PS4/GettingStarted"),																	IsAvailableOnWindows,											TEXT("PS4"),		TEXT("PS4"),		false,					false,					true,			TEXT("PS4")),

	BuildPlatformInfo(TEXT("XboxOne"),					TEXT("XboxOne"),			LOCTEXT("XboxOne", "Xbox One"),									EPlatformType::Game,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/XboxOne/Platform_XboxOne_24x"), TEXT("Launcher/XboxOne/Platform_XboxOne_128x")),					TEXT(""),											TEXT("XboxOne"),	EPlatformSDKStatus::Unknown,	TEXT("/Platforms/XboxOne/GettingStarted"),																IsAvailableOnWindows,											TEXT("XboxOne"),	TEXT("XboxOne"),	false,					true,					true,			TEXT("XboxOne")),

	BuildPlatformInfo(TEXT("AllDesktop"),				TEXT("AllDesktop"),			LOCTEXT("DesktopTargetPlatDisplay", "Desktop (Win+Mac+Linux)"),	EPlatformType::Game,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Desktop/Platform_Desktop_24x"), TEXT("Launcher/Desktop/Platform_Desktop_128x")),					TEXT(""),											TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT(""),																								IsAvailableOnWindows /* see note below */,						TEXT(""),			TEXT(""),			false,					true,					false,			TEXT("AllDesktop")),

	BuildPlatformInfo(TEXT("TVOS"),						TEXT("TVOS"),				LOCTEXT("TVOSTargetPlatDisplay", "tvOS"),						EPlatformType::Game,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/TVOS/Platform_TVOS_24x"), TEXT("Launcher/TVOS/Platform_TVOS_128x")),								TEXT(""),											TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT(""),																								IsAvailableOnWindows || IsAvailableOnMac,						TEXT("TVOS"),		TEXT("TVOS"),		false,					true,					false,			TEXT("TVOS")),
	BuildPlatformInfo(TEXT("Switch"),					TEXT("Switch"),				LOCTEXT("Switch", "Switch"),									EPlatformType::Game,	EPlatformFlags::None,			FPlatformIconPaths(TEXT("Launcher/Switch/Platform_Switch_24x"), TEXT("Launcher/Switch/Platform_Switch_128x")),						TEXT(""),											TEXT(""),			EPlatformSDKStatus::Unknown,	TEXT(""),																								IsAvailableOnWindows,											TEXT("Switch"),		TEXT("Switch"),		false,					true,					true,			TEXT("Switch")),

	// Note: For "AllDesktop" bEnabledForUse value, see SProjectTargetPlatformSettings::Construct !!!! IsAvailableOnWindows || IsAvailableOnMac || IsAvailableOnLinux
};

} // anonymous namespace

const FPlatformInfo* FindPlatformInfo(const FName& InPlatformName)
{
	for(const FPlatformInfo& PlatformInfo : AllPlatformInfoArray)
	{
		if(PlatformInfo.PlatformInfoName == InPlatformName)
		{
			return &PlatformInfo;
		}
	}
	return nullptr;
}

const FPlatformInfo* FindVanillaPlatformInfo(const FName& InPlatformName)
{
	const FPlatformInfo* const FoundInfo = FindPlatformInfo(InPlatformName);
	return (FoundInfo) ? (FoundInfo->IsVanilla()) ? FoundInfo : FindPlatformInfo(FoundInfo->VanillaPlatformName) : nullptr;
}

const FPlatformInfo* GetPlatformInfoArray(int32& OutNumPlatforms)
{
	OutNumPlatforms = ARRAY_COUNT(AllPlatformInfoArray);
	return AllPlatformInfoArray;
}

void UpdatePlatformSDKStatus(FString InPlatformName, EPlatformSDKStatus InStatus)
{
	for(const FPlatformInfo& PlatformInfo : AllPlatformInfoArray)
	{
		if(PlatformInfo.VanillaPlatformName == FName(*InPlatformName))
		{
			const_cast<FPlatformInfo&>(PlatformInfo).SDKStatus = InStatus;
		}
	}
}

void UpdatePlatformDisplayName(FString InPlatformName, FText InDisplayName)
{
	for (const FPlatformInfo& PlatformInfo : AllPlatformInfoArray)
	{
		if (PlatformInfo.TargetPlatformName == FName(*InPlatformName))
		{
			const_cast<FPlatformInfo&>(PlatformInfo).DisplayName = InDisplayName;
		}
	}
}

FPlatformEnumerator EnumeratePlatformInfoArray(bool bAccessiblePlatformsOnly)
{
	if (bAccessiblePlatformsOnly)
	{
		static bool bHasSearchedForPlatforms = false;
		static TArray<FPlatformInfo> AccessiblePlatforms;

		if (!bHasSearchedForPlatforms)
		{
			FPlatformEnumerator Enumerator(AllPlatformInfoArray, ARRAY_COUNT(AllPlatformInfoArray));

			const TArray<FString>& ConfidentalPlatforms = FPlatformMisc::GetConfidentialPlatforms();

			for (const FPlatformInfo& PlatformInfo : Enumerator)
			{
				if (PlatformInfo.bIsConfidential && ConfidentalPlatforms.Contains(PlatformInfo.IniPlatformName))
				{
					AccessiblePlatforms.Add(PlatformInfo);
				}
				else if(!PlatformInfo.bIsConfidential)
				{
					AccessiblePlatforms.Add(PlatformInfo);
				}
			}

			bHasSearchedForPlatforms = true;
		}

		return FPlatformEnumerator(AccessiblePlatforms.GetData(), AccessiblePlatforms.Num());
	}
	else
	{

		return FPlatformEnumerator(AllPlatformInfoArray, ARRAY_COUNT(AllPlatformInfoArray));
	}
}

TArray<FVanillaPlatformEntry> BuildPlatformHierarchy(const EPlatformFilter InFilter)
{
	TArray<FVanillaPlatformEntry> VanillaPlatforms;

	// Build up a tree from the platforms we support (vanilla outers, with a list of flavors)
	// PlatformInfoArray should be ordered in such a way that the vanilla platforms always appear before their flavors
	for(const FPlatformInfo& PlatformInfo : AllPlatformInfoArray)
	{
		if(PlatformInfo.IsVanilla())
		{
			VanillaPlatforms.Add(FVanillaPlatformEntry(&PlatformInfo));
		}
		else
		{
			const bool bHasBuildFlavor = !!(PlatformInfo.PlatformFlags & EPlatformFlags::BuildFlavor);
			const bool bHasCookFlavor  = !!(PlatformInfo.PlatformFlags & EPlatformFlags::CookFlavor);
			
			const bool bValidFlavor = 
				InFilter == EPlatformFilter::All || 
				(InFilter == EPlatformFilter::BuildFlavor && bHasBuildFlavor) || 
				(InFilter == EPlatformFilter::CookFlavor && bHasCookFlavor);

			if(bValidFlavor)
			{
				const FName VanillaPlatformName = PlatformInfo.VanillaPlatformName;
				FVanillaPlatformEntry* const VanillaEntry = VanillaPlatforms.FindByPredicate([VanillaPlatformName](const FVanillaPlatformEntry& Item) -> bool
				{
					return Item.PlatformInfo->PlatformInfoName == VanillaPlatformName;
				});
				check(VanillaEntry);
				VanillaEntry->PlatformFlavors.Add(&PlatformInfo);
			}
		}
	}

	return VanillaPlatforms;
}

FVanillaPlatformEntry BuildPlatformHierarchy(const FName& InPlatformName, const EPlatformFilter InFilter)
{
	FVanillaPlatformEntry VanillaPlatformEntry;
	const FPlatformInfo* VanillaPlatformInfo = FindVanillaPlatformInfo(InPlatformName);
	
	if (VanillaPlatformInfo)
	{
		VanillaPlatformEntry.PlatformInfo = VanillaPlatformInfo;
		
		for (const FPlatformInfo& PlatformInfo : AllPlatformInfoArray)
		{
			if (!PlatformInfo.IsVanilla() && PlatformInfo.VanillaPlatformName == VanillaPlatformInfo->PlatformInfoName)
			{
				const bool bHasBuildFlavor = !!(PlatformInfo.PlatformFlags & EPlatformFlags::BuildFlavor);
				const bool bHasCookFlavor = !!(PlatformInfo.PlatformFlags & EPlatformFlags::CookFlavor);

				const bool bValidFlavor =
					InFilter == EPlatformFilter::All ||
					(InFilter == EPlatformFilter::BuildFlavor && bHasBuildFlavor) ||
					(InFilter == EPlatformFilter::CookFlavor && bHasCookFlavor);

				if (bValidFlavor)
				{
					VanillaPlatformEntry.PlatformFlavors.Add(&PlatformInfo);
				}
			}
		}
	}
	
	return VanillaPlatformEntry;
}

EPlatformType EPlatformTypeFromString(const FString& PlatformTypeName)
{
	if (FCString::Strcmp(*PlatformTypeName, TEXT("Game")) == 0)
	{
		return PlatformInfo::EPlatformType::Game;
	}
	else if (FCString::Strcmp(*PlatformTypeName, TEXT("Editor")) == 0)
	{
		return PlatformInfo::EPlatformType::Editor;
	}
	else if (FCString::Strcmp(*PlatformTypeName, TEXT("Client")) == 0)
	{
		return PlatformInfo::EPlatformType::Client;
	}
	else if (FCString::Strcmp(*PlatformTypeName, TEXT("Server")) == 0)
	{
		return PlatformInfo::EPlatformType::Server;
	}
	else
	{
		UE_LOG(LogDesktopPlatform, Warning, TEXT("Unable to read Platform Type from %s, defaulting to Game"), *PlatformTypeName);
		return PlatformInfo::EPlatformType::Game;
	}
}

} // namespace PlatformFamily

namespace Lex
{
	FString ToString(const PlatformInfo::EPlatformType Value)
	{
		switch (Value)
		{
		case PlatformInfo::EPlatformType::Game:
			return TEXT("Game");
		case PlatformInfo::EPlatformType::Editor:
			return TEXT("Editor");
		case PlatformInfo::EPlatformType::Client:
			return TEXT("Client");
		case PlatformInfo::EPlatformType::Server:
			return TEXT("Server");
		}

		return TEXT("");
	}
}

#undef LOCTEXT_NAMESPACE
