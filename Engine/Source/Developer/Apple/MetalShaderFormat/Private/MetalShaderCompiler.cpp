// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
// ...

#include "CoreMinimal.h"
#include "MetalShaderFormat.h"
#include "ShaderCore.h"
#include "MetalShaderResources.h"
#include "ShaderCompilerCommon.h"
#include "Misc/ConfigCacheIni.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if PLATFORM_WINDOWS
#include "WindowsHWrapper.h"
#include "AllowWindowsPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
	#include "Windows/PreWindowsApi.h"
	#include <objbase.h>
	#include <assert.h>
	#include <stdio.h>
	#include "Windows/PostWindowsApi.h"
	#include "Windows/MinWindows.h"
THIRD_PARTY_INCLUDES_END
#include "HideWindowsPlatformTypes.h"
#endif

#include "ShaderPreprocessor.h"
#include "hlslcc.h"
#include "MetalBackend.h"
#include "MetalDerivedData.h"
#include "DerivedDataCacheInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogMetalShaderCompiler, Log, All); 

static FString	GRemoteBuildServerHost;
static FString	GRemoteBuildServerUser;
static FString	GRemoteBuildServerSSHKey;
static FString	GSSHPath;
static FString	GRSyncPath;
static FString	GMetalToolsPath[2];
static FString	GMetalBinaryPath[2];
static FString	GMetalLibraryPath[2];
static FString	GMetalCompilerVers[2];
static FString	GTempFolderPath;
static bool		GMetalLoggedRemoteCompileNotConfigured;	// This is used to reduce log spam, its not perfect because there is not a place to reset this flag so a log msg will only be given once per editor run
static bool		GRemoteBuildConfigured = false;

// Add (|| PLATFORM_MAC) to enable Mac to Mac remote building
#define UNIXLIKE_TO_MAC_REMOTE_BUILDING (PLATFORM_LINUX)

bool IsRemoteBuildingConfigured(const FShaderCompilerEnvironment* InEnvironment)
{
	// if we have gotten an environment, then it is possible the remote server data has changed, in all other cases, it is not possible for it change
	if (!GRemoteBuildConfigured || InEnvironment != nullptr)
	{
		GRemoteBuildConfigured = false;
		bool	remoteCompilingEnabled = false;
		GConfig->GetBool(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("EnableRemoteShaderCompile"), remoteCompilingEnabled, GEngineIni);
		if (!remoteCompilingEnabled && !FParse::Param(FCommandLine::Get(), TEXT("enableremote")))
		{
			if (InEnvironment == nullptr || InEnvironment->RemoteServerData.Num() < 2)
			{
				return false;
			}
		}

		GRemoteBuildServerHost = "";
		GConfig->GetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("RemoteServerName"), GRemoteBuildServerHost, GEngineIni);
		if(GRemoteBuildServerHost.Len() == 0)
		{
			// check for it on the command line - meant for ShaderCompileWorker
			if (!FParse::Value(FCommandLine::Get(), TEXT("servername"), GRemoteBuildServerHost) && GRemoteBuildServerHost.Len() == 0)
			{
				if (InEnvironment != nullptr && InEnvironment->RemoteServerData.Contains(TEXT("RemoteServerName")))
				{
					GRemoteBuildServerHost = InEnvironment->RemoteServerData[TEXT("RemoteServerName")];
				}
				if (GRemoteBuildServerHost.Len() == 0)
				{
					if (!GMetalLoggedRemoteCompileNotConfigured)
					{
						if (!PLATFORM_MAC || UNIXLIKE_TO_MAC_REMOTE_BUILDING)
						{
							UE_LOG(LogMetalShaderCompiler, Warning, TEXT("Remote Building is not configured: RemoteServerName is not set."));
						}
						GMetalLoggedRemoteCompileNotConfigured = true;
					}
					return false;
				}
			}
		}

		GRemoteBuildServerUser = "";
		GConfig->GetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("RSyncUsername"), GRemoteBuildServerUser, GEngineIni);

		if(GRemoteBuildServerUser.Len() == 0)
		{
			// check for it on the command line - meant for ShaderCompileWorker
			if (!FParse::Value(FCommandLine::Get(), TEXT("serveruser"), GRemoteBuildServerUser) && GRemoteBuildServerUser.Len() == 0)
			{
				if (InEnvironment != nullptr && InEnvironment->RemoteServerData.Contains(TEXT("RSyncUsername")))
				{
					GRemoteBuildServerUser = InEnvironment->RemoteServerData[TEXT("RSyncUsername")];
				}
				if (GRemoteBuildServerUser.Len() == 0)
				{
					if (!GMetalLoggedRemoteCompileNotConfigured)
					{
						if (!PLATFORM_MAC || UNIXLIKE_TO_MAC_REMOTE_BUILDING)
						{
							UE_LOG(LogMetalShaderCompiler, Warning, TEXT("Remote Building is not configured: RSyncUsername is not set."));
						}
						GMetalLoggedRemoteCompileNotConfigured = true;
					}
					return false;
				}
			}
		}

		GRemoteBuildServerSSHKey = "";
		GConfig->GetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("SSHPrivateKeyOverridePath"), GRemoteBuildServerSSHKey, GEngineIni);

		if(GRemoteBuildServerSSHKey.Len() == 0)
		{
			if (!FParse::Value(FCommandLine::Get(), TEXT("serverkey"), GRemoteBuildServerSSHKey) && GRemoteBuildServerSSHKey.Len() == 0)
			{
				if (InEnvironment != nullptr && InEnvironment->RemoteServerData.Contains(TEXT("SSHPrivateKeyOverridePath")))
				{
					GRemoteBuildServerSSHKey = InEnvironment->RemoteServerData[TEXT("SSHPrivateKeyOverridePath")];
				}
				if (GRemoteBuildServerSSHKey.Len() == 0)
				{
					// RemoteToolChain.cs in UBT looks in a few more places but the code in FIOSTargetSettingsCustomization::OnGenerateSSHKey() only puts the key in this location so just going with that to keep things simple
					TCHAR Path[4096];
					FPlatformMisc::GetEnvironmentVariable(TEXT("APPDATA"), Path, ARRAY_COUNT(Path));
					GRemoteBuildServerSSHKey = FString::Printf(TEXT("%s\\Unreal Engine\\UnrealBuildTool\\SSHKeys\\%s\\%s\\RemoteToolChainPrivate.key"), Path, *GRemoteBuildServerHost, *GRemoteBuildServerUser);
				}
			}
		}

		if (!FPaths::FileExists(GRemoteBuildServerSSHKey))
		{
			if (!GMetalLoggedRemoteCompileNotConfigured)
			{
				if (!PLATFORM_MAC || UNIXLIKE_TO_MAC_REMOTE_BUILDING)
				{
					UE_LOG(LogMetalShaderCompiler, Warning, TEXT("Remote Building is not configured: SSH private key was not found."));
				}
				GMetalLoggedRemoteCompileNotConfigured = true;
			}
			return false;
		}

	#if PLATFORM_LINUX || PLATFORM_MAC

		// On Unix like systems we have access to ssh and scp at the command line so we can invoke them directly
		GSSHPath = FString(TEXT("/usr/bin/ssh"));
		GRSyncPath = FString(TEXT("/usr/bin/scp"));

	#else

		// Windows requires a Delta copy install for ssh and rsync
		FString DeltaCopyPath;
		GConfig->GetString(TEXT("/Script/IOSRuntimeSettings.IOSRuntimeSettings"), TEXT("DeltaCopyInstallPath"), DeltaCopyPath, GEngineIni);
		if (DeltaCopyPath.IsEmpty() || !FPaths::DirectoryExists(DeltaCopyPath))
		{
			// If no user specified directory try the UE4 bundled directory
			DeltaCopyPath = FPaths::ConvertRelativePathToFull(FPaths::EngineDir() / TEXT("Extras\\ThirdPartyNotUE\\DeltaCopy\\Binaries"));
		}

		if (!FPaths::DirectoryExists(DeltaCopyPath))
		{
			// if no UE4 bundled version of DeltaCopy, try and use the default install location
			TCHAR ProgramPath[4096];
			FPlatformMisc::GetEnvironmentVariable(TEXT("PROGRAMFILES(X86)"), ProgramPath, ARRAY_COUNT(ProgramPath));
			DeltaCopyPath = FPaths::Combine(ProgramPath, TEXT("DeltaCopy"));
		}

		if (!FPaths::DirectoryExists(DeltaCopyPath))
		{
			if (!GMetalLoggedRemoteCompileNotConfigured)
			{
				if (!PLATFORM_MAC || UNIXLIKE_TO_MAC_REMOTE_BUILDING)
				{
					UE_LOG(LogMetalShaderCompiler, Warning, TEXT("Remote Building is not configured: DeltaCopy was not found."));
				}
				GMetalLoggedRemoteCompileNotConfigured = true;
			}
			return false;
		}

		GSSHPath = FPaths::Combine(*DeltaCopyPath, TEXT("ssh.exe"));
		GRSyncPath = FPaths::Combine(*DeltaCopyPath, TEXT("rsync.exe"));

	#endif
		GRemoteBuildConfigured = true;
	}

	return true;	
}

static bool CompileProcessAllowsRuntimeShaderCompiling(const FShaderCompilerInput& InputCompilerEnvironment)
{
    bool bArchiving = InputCompilerEnvironment.Environment.CompilerFlags.Contains(CFLAG_Archive);
    bool bDebug = InputCompilerEnvironment.Environment.CompilerFlags.Contains(CFLAG_Debug);
    
    return !bArchiving && bDebug;
}

bool ExecRemoteProcess(const TCHAR* Command, const TCHAR* Params, int32* OutReturnCode, FString* OutStdOut, FString* OutStdErr)
{
#if PLATFORM_MAC && !UNIXLIKE_TO_MAC_REMOTE_BUILDING
	return FPlatformProcess::ExecProcess(Command, Params, OutReturnCode, OutStdOut, OutStdErr);
#else
	FString CmdLine = FString(TEXT("-i \"")) + GRemoteBuildServerSSHKey + TEXT("\" ") + GRemoteBuildServerUser + '@' + GRemoteBuildServerHost + TEXT(" ") + Command + TEXT(" ") + (Params != nullptr ? Params : TEXT(""));
	return FPlatformProcess::ExecProcess(*GSSHPath, *CmdLine, OutReturnCode, OutStdOut, OutStdErr);
#endif
}

FString GetXcodePath()
{
#if PLATFORM_MAC && !UNIXLIKE_TO_MAC_REMOTE_BUILDING
	return FPlatformMisc::GetXcodePath();
#else
	FString XcodePath;
	if (ExecRemoteProcess(TEXT("/usr/bin/xcode-select"),TEXT("--print-path"), nullptr, &XcodePath, nullptr) && XcodePath.Len() > 0)
	{
		XcodePath.RemoveAt(XcodePath.Len() - 1); // Remove \n at the end of the string
	}
	return XcodePath;
#endif
}

FString GetMetalStdLibPath(FString const& PlatformPath)
{
	FString Result;
	bool bOK = false;
	FString Exec = FString::Printf(TEXT("\"%s/clang\" -name metal_stdlib"), *PlatformPath);
	bOK = ExecRemoteProcess(TEXT("/usr/bin/find"), *Exec, nullptr, &Result, nullptr);
	if (bOK && Result.Len() > 0)
	{
		Result.RemoveAt(Result.Len() - 1); // Remove \n at the end of the string
	}
	return Result;
}

FString GetMetalCompilerVers(FString const& PlatformPath)
{
	FString Result;
	bool bOK = false;
	bOK = ExecRemoteProcess(*PlatformPath, TEXT("-v"), nullptr, &Result, &Result);
	if (bOK && Result.Len() > 0)
	{
		TCHAR Buffer[256];
#if !PLATFORM_WINDOWS
		if(swscanf(*Result, TEXT("Apple LLVM version %*ls (%ls)"), Buffer))
#else
		if(swscanf_s(*Result, TEXT("Apple LLVM version %*ls (%ls)"), Buffer, 256))
#endif
		{
			Result = (&Buffer[0]);
			Result.RemoveFromEnd(TEXT(")"));
		}
		else
		{
			Result = TEXT("");
		}
	}
	return Result;
}

bool RemoteFileExists(const FString& Path)
{
#if PLATFORM_MAC && !UNIXLIKE_TO_MAC_REMOTE_BUILDING
	return IFileManager::Get().FileExists(*Path);
#else
	int32 ReturnCode = 1;
	FString StdOut;
	FString StdErr;
	return (ExecRemoteProcess(*FString::Printf(TEXT("test -e \"%s\""), *Path), nullptr, &ReturnCode, &StdOut, &StdErr) && ReturnCode == 0);
#endif
}

static uint32 GetMaxArgLength()
{
#if PLATFORM_MAC && !UNIXLIKE_TO_MAC_REMOTE_BUILDING
    return ARG_MAX;
#else
    // Ask the remote machine via "getconf ARG_MAX"
    return 1024 * 256;
#endif
}

FString MakeRemoteTempFolder(FString Path)
{
#if PLATFORM_MAC && !UNIXLIKE_TO_MAC_REMOTE_BUILDING
	return Path;
#else
	if(GTempFolderPath.Len() == 0)
	{
		FString TempFolderPath;
		if (ExecRemoteProcess(TEXT("mktemp -d -t UE4Metal"), nullptr, nullptr, &TempFolderPath, nullptr) && TempFolderPath.Len() > 0)
		{
			TempFolderPath.RemoveAt(TempFolderPath.Len() - 1); // Remove \n at the end of the string
		}
		GTempFolderPath = TempFolderPath;
	}

	return GTempFolderPath;
#endif
}

FString LocalPathToRemote(const FString& LocalPath, const FString& RemoteFolder)
{
#if PLATFORM_MAC && !UNIXLIKE_TO_MAC_REMOTE_BUILDING
	return LocalPath;
#else
	return RemoteFolder / FPaths::GetCleanFilename(LocalPath);
#endif
}

bool CopyLocalFileToRemote(FString const& LocalPath, FString const& RemotePath)
{
#if PLATFORM_MAC && !UNIXLIKE_TO_MAC_REMOTE_BUILDING
	return true;
#else
#if UNIXLIKE_TO_MAC_REMOTE_BUILDING
    // Params formatted for 'scp'
    FString	params = FString::Printf(TEXT("%s %s@%s:%s"), *LocalPath, *GRemoteBuildServerUser, *GRemoteBuildServerHost, *RemotePath);
#else
	FString	remoteBasePath;
	FString remoteFileName;
	FString	remoteFileExt;
	FPaths::Split(RemotePath, remoteBasePath, remoteFileName, remoteFileExt);
	
	FString cygwinLocalPath = TEXT("/cygdrive/") + LocalPath.Replace(TEXT(":"), TEXT(""));

	FString	params = 
		FString::Printf(
			TEXT("-zae \"'%s' -i '%s'\" --rsync-path=\"mkdir -p %s && rsync\" --chmod=ug=rwX,o=rxX '%s' %s@%s:'%s'"), 
			*GSSHPath,
			*GRemoteBuildServerSSHKey, 
			*remoteBasePath, 
			*cygwinLocalPath, 
			*GRemoteBuildServerUser,
			*GRemoteBuildServerHost,
			*RemotePath);
			
#endif

	int32	returnCode;
	FString	stdOut, stdErr;
	return (FPlatformProcess::ExecProcess(*GRSyncPath, *params, &returnCode, &stdOut, &stdErr) && returnCode == 0);
#endif
}

bool CopyRemoteFileToLocal(FString const& RemotePath, FString const& LocalPath)
{
#if PLATFORM_MAC && !UNIXLIKE_TO_MAC_REMOTE_BUILDING
	return true;
#else
#if UNIXLIKE_TO_MAC_REMOTE_BUILDING
    // Params formatted for 'scp'
    FString	params = FString::Printf(TEXT("%s@%s:%s %s"), *GRemoteBuildServerUser, *GRemoteBuildServerHost, *RemotePath, *LocalPath);
#else
	FString cygwinLocalPath = TEXT("/cygdrive/") + LocalPath.Replace(TEXT(":"), TEXT(""));

	FString	params = 
		FString::Printf(
			TEXT("-zae \"'%s' -i '%s'\" %s@%s:'%s' '%s'"), 
			*GSSHPath,
			*GRemoteBuildServerSSHKey, 
			*GRemoteBuildServerUser,
			*GRemoteBuildServerHost,
			*RemotePath, 
			*cygwinLocalPath);

#endif

	int32	returnCode;
	FString	stdOut, stdErr;
	return (FPlatformProcess::ExecProcess(*GRSyncPath, *params, &returnCode, &stdOut, &stdErr) && returnCode == 0);
#endif
}

FString GetMetalBinaryPath(uint32 ShaderPlatform)
{
	const bool bIsMobile = (ShaderPlatform == SP_METAL || ShaderPlatform == SP_METAL_MRT);
	if(GMetalBinaryPath[bIsMobile].Len() == 0 || GMetalToolsPath[bIsMobile].Len() == 0)
	{
		FString XcodePath = GetXcodePath();
		if (XcodePath.Len() > 0)
		{
			FString MetalToolsPath = FString::Printf(TEXT("%s/Toolchains/XcodeDefault.xctoolchain/usr/bin"), *XcodePath);
			FString MetalPath = MetalToolsPath + TEXT("/metal");
			if (!RemoteFileExists(MetalPath))
			{
				if (bIsMobile)
				{
					MetalToolsPath = FString::Printf(TEXT("%s/Platforms/iPhoneOS.platform/usr/bin"), *XcodePath);
				}
				else
				{
					MetalToolsPath = FString::Printf(TEXT("%s/Platforms/MacOSX.platform/usr/bin"), *XcodePath);
				}
				MetalPath = MetalToolsPath + TEXT("/metal");
			}

			if (RemoteFileExists(MetalPath))
			{
				GMetalBinaryPath[bIsMobile] = MetalPath;
				GMetalToolsPath[bIsMobile] = MetalToolsPath;
				
				GMetalCompilerVers[bIsMobile] = GetMetalCompilerVers(MetalPath);
				
				FString MetalLibraryPath;
				if (bIsMobile)
				{
					MetalLibraryPath = FString::Printf(TEXT("%s/Platforms/iPhoneOS.platform/usr/lib"), *XcodePath);
				}
				else
				{
					MetalLibraryPath = FString::Printf(TEXT("%s/Platforms/MacOSX.platform/usr/lib"), *XcodePath);
				}
				
				FString MetalStdLibPath = GetMetalStdLibPath(MetalLibraryPath);
				if (RemoteFileExists(MetalStdLibPath))
				{
					GMetalLibraryPath[bIsMobile] = MetalStdLibPath;
				}
			}
		}
	}

	return GMetalBinaryPath[bIsMobile];
}

FString GetMetalToolsPath(uint32 ShaderPlatform)
{
	GetMetalBinaryPath(ShaderPlatform);
	
	const bool bIsMobile = (ShaderPlatform == SP_METAL || ShaderPlatform == SP_METAL_MRT);
	return GMetalToolsPath[bIsMobile];
}

FString GetMetalLibraryPath(uint32 ShaderPlatform)
{
	GetMetalBinaryPath(ShaderPlatform);
	
	const bool bIsMobile = (ShaderPlatform == SP_METAL || ShaderPlatform == SP_METAL_MRT);
	return GMetalLibraryPath[bIsMobile];
}

FString GetMetalCompilerVersion(uint32 ShaderPlatform)
{
	GetMetalBinaryPath(ShaderPlatform);
	
	const bool bIsMobile = (ShaderPlatform == SP_METAL || ShaderPlatform == SP_METAL_MRT);
	return GMetalCompilerVers[bIsMobile];
}

uint16 GetXcodeVersion(uint64& BuildVersion)
{
	BuildVersion = 0;
	
	static uint64 Build = 0;
	static uint16 Version = UINT16_MAX;
	if (Version == UINT16_MAX)
	{
		Version = 0; // No Xcode install is 0, so only text shaders will work
		FString XcodePath = GetXcodePath();
		if (XcodePath.Len() > 0)
		{
			FString Path = FString::Printf(TEXT("%s/usr/bin/xcodebuild"), *XcodePath);
			FString Result;
			bool bOK = false;
			bOK = ExecRemoteProcess(*Path, TEXT("-version"), nullptr, &Result, nullptr);
			if (bOK && Result.Len() > 0)
			{
				uint32 Major = 0;
				uint32 Minor = 0;
				uint32 Patch = 0;
				int32 NumResults = 0;
	#if !PLATFORM_WINDOWS
				NumResults = swscanf(*Result, TEXT("Xcode %u.%u.%u"), &Major, &Minor, &Patch);
	#else
				NumResults = swscanf_s(*Result, TEXT("Xcode %u.%u.%u"), &Major, &Minor, &Patch);
	#endif
				if (NumResults >= 2)
				{
					Version = (((Major & 0xff) << 8) | ((Minor & 0xf) << 4) | (Patch & 0xf));
					
					ANSICHAR const* BuildScan = "Xcode %*u.%*u.%*u\nBuild version %s";
					if (NumResults == 2)
					{
						BuildScan = "Xcode %*u.%*u\nBuild version %s";
					}
					
					ANSICHAR Buffer[9] = {0,0,0,0,0,0,0,0,0};
#if !PLATFORM_WINDOWS
					if(sscanf(TCHAR_TO_ANSI(*Result), BuildScan, Buffer))
#else
					if(sscanf_s(TCHAR_TO_ANSI(*Result), BuildScan, Buffer, 9))
#endif
					{
						FMemory::Memcpy(&Build, Buffer, sizeof(uint64));
					}
				}
			}
		}
	}
	BuildVersion = Build;
	return Version;
}

bool ChecksumRemoteFile(FString const& RemotePath, uint32* CRC, uint32* Len)
{
	int32 ReturnCode = -1;
	FString Output;
	bool bOK = ExecRemoteProcess(TEXT("/usr/bin/cksum"), *RemotePath, &ReturnCode, &Output, nullptr);
	if (bOK)
	{
#if !PLATFORM_WINDOWS
		if(swscanf(*Output, TEXT("%u %u"), CRC, Len) != 2)
#else
		if(swscanf_s(*Output, TEXT("%u %u"), CRC, Len) != 2)
#endif
		{
			bOK = false;
		}
	}
	return bOK;
}

bool RemoveRemoteFile(FString const& RemotePath)
{
	int32 ReturnCode = -1;
	FString Output;
	bool bOK = ExecRemoteProcess(TEXT("/bin/rm"), *RemotePath, &ReturnCode, &Output, nullptr);
	if (bOK)
	{
		bOK = (ReturnCode == 0);
	}
	return bOK;
}

/*------------------------------------------------------------------------------
	Shader compiling.
------------------------------------------------------------------------------*/

static inline uint32 ParseNumber(const TCHAR* Str)
{
	uint32 Num = 0;
	while (*Str && *Str >= '0' && *Str <= '9')
	{
		Num = Num * 10 + *Str++ - '0';
	}
	return Num;
}

static inline uint32 ParseNumber(const ANSICHAR* Str)
{
	uint32 Num = 0;
	while (*Str && *Str >= '0' && *Str <= '9')
	{
		Num = Num * 10 + *Str++ - '0';
	}
	return Num;
}

struct FHlslccMetalHeader : public CrossCompiler::FHlslccHeader
{
	FHlslccMetalHeader(uint8 const Version, bool const bUsingTessellation);
	virtual ~FHlslccMetalHeader();
	
	// After the standard header, different backends can output their own info
	virtual bool ParseCustomHeaderEntries(const ANSICHAR*& ShaderSource) override;
	
	float  TessellationMaxTessFactor;
	uint32 TessellationOutputControlPoints;
	uint32 TessellationDomain; // 3 = tri, 4 = quad
	uint32 TessellationInputControlPoints;
	uint32 TessellationPatchesPerThreadGroup;
	uint32 TessellationPatchCountBuffer;
	uint32 TessellationIndexBuffer;
	uint32 TessellationHSOutBuffer;
	uint32 TessellationHSTFOutBuffer;
	uint32 TessellationControlPointOutBuffer;
	uint32 TessellationControlPointIndexBuffer;
	EMetalOutputWindingMode TessellationOutputWinding;
	EMetalPartitionMode TessellationPartitioning;
	uint8 Version;
	bool bUsingTessellation;
};

FHlslccMetalHeader::FHlslccMetalHeader(uint8 const InVersion, bool const bInUsingTessellation)
{
	TessellationMaxTessFactor = 0.0f;
	TessellationOutputControlPoints = 0;
	TessellationDomain = 0;
	TessellationInputControlPoints = 0;
	TessellationPatchesPerThreadGroup = 0;
	TessellationOutputWinding = EMetalOutputWindingMode::Clockwise;
	TessellationPartitioning = EMetalPartitionMode::Pow2;
	
	TessellationPatchCountBuffer = UINT_MAX;
	TessellationIndexBuffer = UINT_MAX;
	TessellationHSOutBuffer = UINT_MAX;
	TessellationHSTFOutBuffer = UINT_MAX;
	TessellationControlPointOutBuffer = UINT_MAX;
	TessellationControlPointIndexBuffer = UINT_MAX;
	
	Version = InVersion;
	bUsingTessellation = bInUsingTessellation;
}

FHlslccMetalHeader::~FHlslccMetalHeader()
{
	
}

bool FHlslccMetalHeader::ParseCustomHeaderEntries(const ANSICHAR*& ShaderSource)
{
#define DEF_PREFIX_STR(Str) \
static const ANSICHAR* Str##Prefix = "// @" #Str ": "; \
static const int32 Str##PrefixLen = FCStringAnsi::Strlen(Str##Prefix)
	DEF_PREFIX_STR(TessellationOutputControlPoints);
	DEF_PREFIX_STR(TessellationDomain);
	DEF_PREFIX_STR(TessellationInputControlPoints);
	DEF_PREFIX_STR(TessellationMaxTessFactor);
	DEF_PREFIX_STR(TessellationOutputWinding);
	DEF_PREFIX_STR(TessellationPartitioning);
	DEF_PREFIX_STR(TessellationPatchesPerThreadGroup);
	DEF_PREFIX_STR(TessellationPatchCountBuffer);
	DEF_PREFIX_STR(TessellationIndexBuffer);
	DEF_PREFIX_STR(TessellationHSOutBuffer);
	DEF_PREFIX_STR(TessellationHSTFOutBuffer);
	DEF_PREFIX_STR(TessellationControlPointOutBuffer);
	DEF_PREFIX_STR(TessellationControlPointIndexBuffer);
#undef DEF_PREFIX_STR
	
	// Early out for non-tessellation...
	if (Version < 2 || !bUsingTessellation)
	{
		return true;
	}
	
	auto ParseUInt32Attribute = [&ShaderSource](const ANSICHAR* prefix, int32 prefixLen, uint32& attributeOut)
	{
		if (FCStringAnsi::Strncmp(ShaderSource, prefix, prefixLen) == 0)
		{
			ShaderSource += prefixLen;
			
			if (!CrossCompiler::ParseIntegerNumber(ShaderSource, attributeOut))
			{
				return false;
			}
			
			if (!CrossCompiler::Match(ShaderSource, '\n'))
			{
				return false;
			}
		}
		
		return true;
	};
 
	// Read number of tessellation output control points
	if (!ParseUInt32Attribute(TessellationOutputControlPointsPrefix, TessellationOutputControlPointsPrefixLen, TessellationOutputControlPoints))
	{
		return false;
	}
	
	// Read the tessellation domain (tri vs quad)
	if (FCStringAnsi::Strncmp(ShaderSource, TessellationDomainPrefix, TessellationDomainPrefixLen) == 0)
	{
		ShaderSource += TessellationDomainPrefixLen;
		
		if (FCStringAnsi::Strncmp(ShaderSource, "tri", 3) == 0)
		{
			ShaderSource += 3;
			TessellationDomain = 3;
		}
		else if (FCStringAnsi::Strncmp(ShaderSource, "quad", 4) == 0)
		{
			ShaderSource += 4;
			TessellationDomain = 4;
		}
		else
		{
			return false;
		}
		
		if (!CrossCompiler::Match(ShaderSource, '\n'))
		{
			return false;
		}
	}
	
	// Read number of tessellation input control points
	if (!ParseUInt32Attribute(TessellationInputControlPointsPrefix, TessellationInputControlPointsPrefixLen, TessellationInputControlPoints))
	{
		return false;
	}
	
	// Read max tessellation factor
	if (FCStringAnsi::Strncmp(ShaderSource, TessellationMaxTessFactorPrefix, TessellationMaxTessFactorPrefixLen) == 0)
	{
		ShaderSource += TessellationMaxTessFactorPrefixLen;
		
#if PLATFORM_WINDOWS
		if (sscanf_s(ShaderSource, "%g\n", &TessellationMaxTessFactor) != 1)
#else
		if (sscanf(ShaderSource, "%g\n", &TessellationMaxTessFactor) != 1)
#endif
		{
			return false;
		}
		
		while (*ShaderSource != '\n')
		{
			++ShaderSource;
		}
		++ShaderSource; // to match the newline
	}
	
	// Read tessellation output winding mode
	if (FCStringAnsi::Strncmp(ShaderSource, TessellationOutputWindingPrefix, TessellationOutputWindingPrefixLen) == 0)
	{
		ShaderSource += TessellationOutputWindingPrefixLen;
		
		if (FCStringAnsi::Strncmp(ShaderSource, "cw", 2) == 0)
		{
			ShaderSource += 2;
			TessellationOutputWinding = EMetalOutputWindingMode::Clockwise;
		}
		else if (FCStringAnsi::Strncmp(ShaderSource, "ccw", 3) == 0)
		{
			ShaderSource += 3;
			TessellationOutputWinding = EMetalOutputWindingMode::CounterClockwise;
		}
		else
		{
			return false;
		}
		
		if (!CrossCompiler::Match(ShaderSource, '\n'))
		{
			return false;
		}
	}
	
	// Read tessellation partition mode
	if (FCStringAnsi::Strncmp(ShaderSource, TessellationPartitioningPrefix, TessellationPartitioningPrefixLen) == 0)
	{
		ShaderSource += TessellationPartitioningPrefixLen;
		
		static char const* partitionModeNames[] =
		{
			// order match enum order
			"pow2",
			"integer",
			"fractional_odd",
			"fractional_even",
		};
		
		bool match = false;
		for (size_t i = 0; i < sizeof(partitionModeNames) / sizeof(partitionModeNames[0]); ++i)
		{
			size_t partitionModeNameLen = strlen(partitionModeNames[i]);
			if (FCStringAnsi::Strncmp(ShaderSource, partitionModeNames[i], partitionModeNameLen) == 0)
			{
				ShaderSource += partitionModeNameLen;
				TessellationPartitioning = (EMetalPartitionMode)i;
				match = true;
				break;
			}
		}
		
		if (!match)
		{
			return false;
		}
		
		if (!CrossCompiler::Match(ShaderSource, '\n'))
		{
			return false;
		}
	}
	
	// Read number of tessellation patches per threadgroup
	if (!ParseUInt32Attribute(TessellationPatchesPerThreadGroupPrefix, TessellationPatchesPerThreadGroupPrefixLen, TessellationPatchesPerThreadGroup))
	{
		return false;
	}
	
	if (!ParseUInt32Attribute(TessellationPatchCountBufferPrefix, TessellationPatchCountBufferPrefixLen, TessellationPatchCountBuffer))
	{
		TessellationPatchCountBuffer = UINT_MAX;
	}
	
	if (!ParseUInt32Attribute(TessellationIndexBufferPrefix, TessellationIndexBufferPrefixLen, TessellationIndexBuffer))
	{
		TessellationIndexBuffer = UINT_MAX;
	}
	
	if (!ParseUInt32Attribute(TessellationHSOutBufferPrefix, TessellationHSOutBufferPrefixLen, TessellationHSOutBuffer))
	{
		TessellationHSOutBuffer = UINT_MAX;
	}
	
	if (!ParseUInt32Attribute(TessellationControlPointOutBufferPrefix, TessellationControlPointOutBufferPrefixLen, TessellationControlPointOutBuffer))
	{
		TessellationControlPointOutBuffer = UINT_MAX;
	}
	
	if (!ParseUInt32Attribute(TessellationHSTFOutBufferPrefix, TessellationHSTFOutBufferPrefixLen, TessellationHSTFOutBuffer))
	{
		TessellationHSTFOutBuffer = UINT_MAX;
	}
	
	if (!ParseUInt32Attribute(TessellationControlPointIndexBufferPrefix, TessellationControlPointIndexBufferPrefixLen, TessellationControlPointIndexBuffer))
	{
		TessellationControlPointIndexBuffer = UINT_MAX;
	}
	
	return true;
}

/**
 * Construct the final microcode from the compiled and verified shader source.
 * @param ShaderOutput - Where to store the microcode and parameter map.
 * @param InShaderSource - Metal source with input/output signature.
 * @param SourceLen - The length of the Metal source code.
 */
void BuildMetalShaderOutput(
	FShaderCompilerOutput& ShaderOutput,
	const FShaderCompilerInput& ShaderInput,
	FSHAHash const& GUIDHash,
	const ANSICHAR* InShaderSource,
	uint32 SourceLen,
	uint32 SourceCRCLen,
	uint32 SourceCRC,
	uint8 Version,
	TCHAR const* Standard,
	TCHAR const* MinOSVersion,
	TArray<FShaderCompilerError>& OutErrors,
	FMetalTessellationOutputs const& TessOutputAttribs,
	uint8 const AtomicUAVs,
	bool bAllowFastIntriniscs
	)
{
	ShaderOutput.bSucceeded = false;
	
	const ANSICHAR* USFSource = InShaderSource;
	
	FString const* UsingTessellationDefine = ShaderInput.Environment.GetDefinitions().Find(TEXT("USING_TESSELLATION"));
	bool bUsingTessellation = (UsingTessellationDefine != nullptr && FString("1") == *UsingTessellationDefine);
	
	FHlslccMetalHeader CCHeader(Version, bUsingTessellation);
	if (!CCHeader.Read(USFSource, SourceLen))
	{
		UE_LOG(LogMetalShaderCompiler, Fatal, TEXT("Bad hlslcc header found"));
	}
	
	const ANSICHAR* SideTableString = FCStringAnsi::Strstr(USFSource, "@SideTable: ");
	
	FMetalCodeHeader Header = {0};
	Header.CompileFlags = (ShaderInput.Environment.CompilerFlags.Contains(CFLAG_Debug) ? (1 << CFLAG_Debug) : 0);
	Header.CompileFlags |= (ShaderInput.Environment.CompilerFlags.Contains(CFLAG_NoFastMath) ? (1 << CFLAG_NoFastMath) : 0);
	Header.CompileFlags |= (ShaderInput.Environment.CompilerFlags.Contains(CFLAG_KeepDebugInfo) ? (1 << CFLAG_KeepDebugInfo) : 0);
	Header.CompileFlags |= (ShaderInput.Environment.CompilerFlags.Contains(CFLAG_ZeroInitialise) ? (1 <<  CFLAG_ZeroInitialise) : 0);
	Header.CompileFlags |= (ShaderInput.Environment.CompilerFlags.Contains(CFLAG_BoundsChecking) ? (1 << CFLAG_BoundsChecking) : 0);
	Header.CompileFlags |= (ShaderInput.Environment.CompilerFlags.Contains(CFLAG_Archive) ? (1 << CFLAG_Archive) : 0);
	Header.CompilerVersion = GetXcodeVersion(Header.CompilerBuild);
	Header.Version = Version;
	Header.SideTable = -1;
	Header.SourceLen = SourceCRCLen;
	Header.SourceCRC = SourceCRC;
	
	if (SideTableString)
	{
		int32 SideTableLoc = -1;
		while (*SideTableString && *SideTableString != '\n')
		{
			if (*SideTableString == '(')
			{
				SideTableString++;
				if (*SideTableString && *SideTableString != '\n')
				{
					SideTableLoc = (int32)ParseNumber(SideTableString);
				}
			}
			else
			{
				SideTableString++;
			}
		}
		if (SideTableLoc >= 0)
		{
			Header.SideTable = SideTableLoc;
		}
		else
		{
			UE_LOG(LogMetalShaderCompiler, Fatal, TEXT("Couldn't parse the SideTable buffer index for bounds checking"));
		}
	}
	
	FShaderParameterMap& ParameterMap = ShaderOutput.ParameterMap;
	EShaderFrequency Frequency = (EShaderFrequency)ShaderOutput.Target.Frequency;

	TBitArray<> UsedUniformBufferSlots;
	UsedUniformBufferSlots.Init(false,32);

	// Write out the magic markers.
	Header.Frequency = Frequency;

	// Only inputs for vertex shaders must be tracked.
	if (Frequency == SF_Vertex)
	{
		static const FString AttributePrefix = TEXT("in_ATTRIBUTE");
		for (auto& Input : CCHeader.Inputs)
		{
			// Only process attributes.
			if (Input.Name.StartsWith(AttributePrefix))
			{
				uint8 AttributeIndex = ParseNumber(*Input.Name + AttributePrefix.Len());
				Header.Bindings.InOutMask |= (1 << AttributeIndex);
			}
		}
	}

	// Then the list of outputs.
	static const FString TargetPrefix = "FragColor";
	static const FString GL_FragDepth = "FragDepth";
	// Only outputs for pixel shaders must be tracked.
	if (Frequency == SF_Pixel)
	{
		for (auto& Output : CCHeader.Outputs)
		{
			// Handle targets.
			if (Output.Name.StartsWith(TargetPrefix))
			{
				uint8 TargetIndex = ParseNumber(*Output.Name + TargetPrefix.Len());
				Header.Bindings.InOutMask |= (1 << TargetIndex);
			}
			// Handle depth writes.
			else if (Output.Name.Equals(GL_FragDepth))
			{
				Header.Bindings.InOutMask |= 0x8000;
			}
		}
	}

	bool bHasRegularUniformBuffers = false;

	// Then 'normal' uniform buffers.
	for (auto& UniformBlock : CCHeader.UniformBlocks)
	{
		uint16 UBIndex = UniformBlock.Index;
		if (UBIndex >= Header.Bindings.NumUniformBuffers)
		{
			Header.Bindings.NumUniformBuffers = UBIndex + 1;
		}
		UsedUniformBufferSlots[UBIndex] = true;
		ParameterMap.AddParameterAllocation(*UniformBlock.Name, UBIndex, 0, 0);
		bHasRegularUniformBuffers = true;
	}

	// Packed global uniforms
	const uint16 BytesPerComponent = 4;
	TMap<ANSICHAR, uint16> PackedGlobalArraySize;
	for (auto& PackedGlobal : CCHeader.PackedGlobals)
	{
		ParameterMap.AddParameterAllocation(
			*PackedGlobal.Name,
			PackedGlobal.PackedType,
			PackedGlobal.Offset * BytesPerComponent,
			PackedGlobal.Count * BytesPerComponent
			);

		uint16& Size = PackedGlobalArraySize.FindOrAdd(PackedGlobal.PackedType);
		Size = FMath::Max<uint16>(BytesPerComponent * (PackedGlobal.Offset + PackedGlobal.Count), Size);
	}

	// Packed Uniform Buffers
	TMap<int, TMap<ANSICHAR, uint16> > PackedUniformBuffersSize;
	for (auto& PackedUB : CCHeader.PackedUBs)
	{
		check(PackedUB.Attribute.Index == Header.Bindings.NumUniformBuffers);
		UsedUniformBufferSlots[PackedUB.Attribute.Index] = true;
		ParameterMap.AddParameterAllocation(*PackedUB.Attribute.Name, Header.Bindings.NumUniformBuffers++, 0, 0);

		// Nothing else...
		//for (auto& Member : PackedUB.Members)
		//{
		//}
	}

	// Packed Uniform Buffers copy lists & setup sizes for each UB/Precision entry
	for (auto& PackedUBCopy : CCHeader.PackedUBCopies)
	{
		CrossCompiler::FUniformBufferCopyInfo CopyInfo;
		CopyInfo.SourceUBIndex = PackedUBCopy.SourceUB;
		CopyInfo.SourceOffsetInFloats = PackedUBCopy.SourceOffset;
		CopyInfo.DestUBIndex = PackedUBCopy.DestUB;
		CopyInfo.DestUBTypeName = PackedUBCopy.DestPackedType;
		CopyInfo.DestUBTypeIndex = CrossCompiler::PackedTypeNameToTypeIndex(CopyInfo.DestUBTypeName);
		CopyInfo.DestOffsetInFloats = PackedUBCopy.DestOffset;
		CopyInfo.SizeInFloats = PackedUBCopy.Count;

		Header.UniformBuffersCopyInfo.Add(CopyInfo);

		auto& UniformBufferSize = PackedUniformBuffersSize.FindOrAdd(CopyInfo.DestUBIndex);
		uint16& Size = UniformBufferSize.FindOrAdd(CopyInfo.DestUBTypeName);
		Size = FMath::Max<uint16>(BytesPerComponent * (CopyInfo.DestOffsetInFloats + CopyInfo.SizeInFloats), Size);
	}

	for (auto& PackedUBCopy : CCHeader.PackedUBGlobalCopies)
	{
		CrossCompiler::FUniformBufferCopyInfo CopyInfo;
		CopyInfo.SourceUBIndex = PackedUBCopy.SourceUB;
		CopyInfo.SourceOffsetInFloats = PackedUBCopy.SourceOffset;
		CopyInfo.DestUBIndex = PackedUBCopy.DestUB;
		CopyInfo.DestUBTypeName = PackedUBCopy.DestPackedType;
		CopyInfo.DestUBTypeIndex = CrossCompiler::PackedTypeNameToTypeIndex(CopyInfo.DestUBTypeName);
		CopyInfo.DestOffsetInFloats = PackedUBCopy.DestOffset;
		CopyInfo.SizeInFloats = PackedUBCopy.Count;

		Header.UniformBuffersCopyInfo.Add(CopyInfo);

		uint16& Size = PackedGlobalArraySize.FindOrAdd(CopyInfo.DestUBTypeName);
		Size = FMath::Max<uint16>(BytesPerComponent * (CopyInfo.DestOffsetInFloats + CopyInfo.SizeInFloats), Size);
	}
	Header.Bindings.bHasRegularUniformBuffers = bHasRegularUniformBuffers;

	// Setup Packed Array info
	Header.Bindings.PackedGlobalArrays.Reserve(PackedGlobalArraySize.Num());
	for (auto Iterator = PackedGlobalArraySize.CreateIterator(); Iterator; ++Iterator)
	{
		ANSICHAR TypeName = Iterator.Key();
		uint16 Size = Iterator.Value();
		Size = (Size + 0xf) & (~0xf);
		CrossCompiler::FPackedArrayInfo Info;
		Info.Size = Size;
		Info.TypeName = TypeName;
		Info.TypeIndex = CrossCompiler::PackedTypeNameToTypeIndex(TypeName);
		Header.Bindings.PackedGlobalArrays.Add(Info);
	}

	// Setup Packed Uniform Buffers info
	Header.Bindings.PackedUniformBuffers.Reserve(PackedUniformBuffersSize.Num());
	for (auto Iterator = PackedUniformBuffersSize.CreateIterator(); Iterator; ++Iterator)
	{
		int BufferIndex = Iterator.Key();
		auto& ArraySizes = Iterator.Value();
		TArray<CrossCompiler::FPackedArrayInfo> InfoArray;
		InfoArray.Reserve(ArraySizes.Num());
		for (auto IterSizes = ArraySizes.CreateIterator(); IterSizes; ++IterSizes)
		{
			ANSICHAR TypeName = IterSizes.Key();
			uint16 Size = IterSizes.Value();
			Size = (Size + 0xf) & (~0xf);
			CrossCompiler::FPackedArrayInfo Info;
			Info.Size = Size;
			Info.TypeName = TypeName;
			Info.TypeIndex = CrossCompiler::PackedTypeNameToTypeIndex(TypeName);
			InfoArray.Add(Info);
		}

		Header.Bindings.PackedUniformBuffers.Add(InfoArray);
	}

	uint32 NumTextures = 0;
	
	// Then samplers.
	TMap<FString, uint32> SamplerMap;
	for (auto& Sampler : CCHeader.Samplers)
	{
		ParameterMap.AddParameterAllocation(
			*Sampler.Name,
			0,
			Sampler.Offset,
			Sampler.Count
			);

		NumTextures += Sampler.Count;

		for (auto& SamplerState : Sampler.SamplerStates)
		{
			SamplerMap.Add(SamplerState, Sampler.Count);
		}
	}
	
	Header.Bindings.NumSamplers = CCHeader.SamplerStates.Num();

	// Then UAVs (images in Metal)
	for (auto& UAV : CCHeader.UAVs)
	{
		ParameterMap.AddParameterAllocation(
			*UAV.Name,
			0,
			UAV.Offset,
			UAV.Count
			);

		Header.Bindings.NumUAVs = FMath::Max<uint8>(
			Header.Bindings.NumSamplers,
			UAV.Offset + UAV.Count
			);
	}

	for (auto& SamplerState : CCHeader.SamplerStates)
	{
		ParameterMap.AddParameterAllocation(
			*SamplerState.Name,
			0,
			SamplerState.Index,
			SamplerMap[SamplerState.Name]
			);
	}

	Header.NumThreadsX = CCHeader.NumThreads[0];
	Header.NumThreadsY = CCHeader.NumThreads[1];
	Header.NumThreadsZ = CCHeader.NumThreads[2];
	
	Header.TessellationOutputControlPoints 		= CCHeader.TessellationOutputControlPoints;
	Header.TessellationDomain					= CCHeader.TessellationDomain;
	Header.TessellationInputControlPoints       = CCHeader.TessellationInputControlPoints;
	Header.TessellationMaxTessFactor            = CCHeader.TessellationMaxTessFactor;
	Header.TessellationOutputWinding			= CCHeader.TessellationOutputWinding;
	Header.TessellationPartitioning				= CCHeader.TessellationPartitioning;
	Header.TessellationPatchesPerThreadGroup    = CCHeader.TessellationPatchesPerThreadGroup;
	Header.TessellationPatchCountBuffer         = CCHeader.TessellationPatchCountBuffer;
	Header.TessellationIndexBuffer              = CCHeader.TessellationIndexBuffer;
	Header.TessellationHSOutBuffer              = CCHeader.TessellationHSOutBuffer;
	Header.TessellationHSTFOutBuffer            = CCHeader.TessellationHSTFOutBuffer;
	Header.TessellationControlPointOutBuffer    = CCHeader.TessellationControlPointOutBuffer;
	Header.TessellationControlPointIndexBuffer  = CCHeader.TessellationControlPointIndexBuffer;
	Header.TessellationOutputAttribs            = TessOutputAttribs;
	Header.bFunctionConstants					= (FCStringAnsi::Strstr(USFSource, "[[ function_constant(") != nullptr);
	
	// Build the SRT for this shader.
	{
		// Build the generic SRT for this shader.
		FShaderCompilerResourceTable GenericSRT;
		BuildResourceTableMapping(ShaderInput.Environment.ResourceTableMap, ShaderInput.Environment.ResourceTableLayoutHashes, UsedUniformBufferSlots, ShaderOutput.ParameterMap, GenericSRT);

		// Copy over the bits indicating which resource tables are active.
		Header.Bindings.ShaderResourceTable.ResourceTableBits = GenericSRT.ResourceTableBits;

		Header.Bindings.ShaderResourceTable.ResourceTableLayoutHashes = GenericSRT.ResourceTableLayoutHashes;

		// Now build our token streams.
		BuildResourceTableTokenStream(GenericSRT.TextureMap, GenericSRT.MaxBoundResourceTable, Header.Bindings.ShaderResourceTable.TextureMap);
		BuildResourceTableTokenStream(GenericSRT.ShaderResourceViewMap, GenericSRT.MaxBoundResourceTable, Header.Bindings.ShaderResourceTable.ShaderResourceViewMap);
		BuildResourceTableTokenStream(GenericSRT.SamplerMap, GenericSRT.MaxBoundResourceTable, Header.Bindings.ShaderResourceTable.SamplerMap);
		BuildResourceTableTokenStream(GenericSRT.UnorderedAccessViewMap, GenericSRT.MaxBoundResourceTable, Header.Bindings.ShaderResourceTable.UnorderedAccessViewMap);

		Header.Bindings.NumUniformBuffers = FMath::Max((uint8)GetNumUniformBuffersUsed(GenericSRT), Header.Bindings.NumUniformBuffers);
		
		Header.Bindings.AtomicUAVs = AtomicUAVs;
	}

	FString MetalCode = FString(USFSource);
	if (ShaderInput.Environment.CompilerFlags.Contains(CFLAG_KeepDebugInfo) || ShaderInput.Environment.CompilerFlags.Contains(CFLAG_Debug))
	{
		MetalCode.InsertAt(0, FString::Printf(TEXT("// %s\n"), *CCHeader.Name));
		Header.ShaderName = CCHeader.Name;
	}
	
	if (Header.Bindings.NumSamplers > MaxMetalSamplers)
	{
		ShaderOutput.bSucceeded = false;
		FShaderCompilerError* NewError = new(ShaderOutput.Errors) FShaderCompilerError();
		
		FString SamplerList;
		for (int32 i = 0; i < CCHeader.SamplerStates.Num(); i++)
		{
			auto const& Sampler = CCHeader.SamplerStates[i];
			SamplerList += FString::Printf(TEXT("%d:%s\n"), Sampler.Index, *Sampler.Name);
		}
		
		NewError->StrippedErrorMessage =
			FString::Printf(TEXT("shader uses %d (%d) samplers exceeding the limit of %d\nSamplers:\n%s"),
				Header.Bindings.NumSamplers, CCHeader.SamplerStates.Num(), MaxMetalSamplers, *SamplerList);
	}
	else if(ShaderInput.Environment.CompilerFlags.Contains(CFLAG_Debug))
	{
		// Write out the header and shader source code.
		FMemoryWriter Ar(ShaderOutput.ShaderCode.GetWriteAccess(), true);
		uint8 PrecompiledFlag = 0;
		Ar << PrecompiledFlag;
		Ar << Header;
		Ar.Serialize((void*)USFSource, SourceLen + 1 - (USFSource - InShaderSource));
		
		// store data we can pickup later with ShaderCode.FindOptionalData('n'), could be removed for shipping
		ShaderOutput.ShaderCode.AddOptionalData('n', TCHAR_TO_UTF8(*ShaderInput.GenerateShaderName()));

		ShaderOutput.NumInstructions = 0;
		ShaderOutput.NumTextureSamplers = Header.Bindings.NumSamplers;
		ShaderOutput.bSucceeded = true;
	}
	else
	{
        // metal commandlines
        FString DebugInfo = (ShaderInput.Environment.CompilerFlags.Contains(CFLAG_KeepDebugInfo) || ShaderInput.Environment.CompilerFlags.Contains(CFLAG_Archive)) ? TEXT("-gline-tables-only") : TEXT("");
        FString MathMode = ShaderInput.Environment.CompilerFlags.Contains(CFLAG_NoFastMath) ? TEXT("-fno-fast-math") : TEXT("-ffast-math");
        
		// at this point, the shader source is ready to be compiled
		// We need to use a temp directory path that will be consistent across devices so that debug info
		// can be loaded (as it must be at a consistent location).
#if PLATFORM_MAC || UNIXLIKE_TO_MAC_REMOTE_BUILDING
		TCHAR const* TempDir = TEXT("/tmp");
#else
		TCHAR const* TempDir = FPlatformProcess::UserTempDir();
#endif
		
		FString ShaderIntermediateDir = TempDir;
		
		FString CompilerVersion = GetMetalCompilerVersion(ShaderInput.Target.Platform);
		
		FString HashedName = FString::Printf(TEXT("%u_%u"), SourceCRCLen, SourceCRC);
        FString MetalFilePath = (TempDir / HashedName) + TEXT(".metal");
        
		FString InputFilename = MetalFilePath;
		FString ObjFilename = FPaths::CreateTempFilename(TempDir, TEXT("ShaderIn"), TEXT(""));
		FString OutputFilename = FPaths::CreateTempFilename(TempDir, TEXT("ShaderIn"), TEXT(""));
		
        // write out shader source, the move it into place using an atomic move - ensures only one compile "wins"
        FString SaveFile = FPaths::CreateTempFilename(TempDir, TEXT("ShaderIn"), TEXT(""));
        FFileHelper::SaveStringToFile(MetalCode, *SaveFile);
        IFileManager::Get().Move(*MetalFilePath, *SaveFile, false, false, true, true);
        IFileManager::Get().Delete(*SaveFile);
		
		int32 ReturnCode = 0;
		FString Results;
		FString Errors;
		bool bCompileAtRuntime = true;
		bool bSucceeded = false;

#if METAL_OFFLINE_COMPILE
		bool bRemoteBuildingConfigured = IsRemoteBuildingConfigured(&ShaderInput.Environment);
		
		FString MetalPath = GetMetalBinaryPath(ShaderInput.Target.Platform);
		FString MetalToolsPath = GetMetalToolsPath(ShaderInput.Target.Platform);
		
		bool bMetalCompilerAvailable = false;

		if (((PLATFORM_MAC && !UNIXLIKE_TO_MAC_REMOTE_BUILDING) || bRemoteBuildingConfigured) && (MetalPath.Len() > 0 && MetalToolsPath.Len() > 0))
		{
			bMetalCompilerAvailable = true;
			bCompileAtRuntime = false;
			bSucceeded = false;
		}
		else if (CompileProcessAllowsRuntimeShaderCompiling(ShaderInput))
		{
			bCompileAtRuntime = true;
			bSucceeded = true;
		}
		else
		{
			TCHAR const* Message = nullptr;
			if (PLATFORM_MAC && !UNIXLIKE_TO_MAC_REMOTE_BUILDING)
			{
				Message = TEXT("Xcode's metal shader compiler was not found, verify Xcode has been installed on this Mac and that it has been selected in Xcode > Preferences > Locations > Command-line Tools.");
			}
			else if (!bRemoteBuildingConfigured)
			{
				Message = TEXT("Remote shader compilation has not been configured in the Editor settings for this project. Please follow the instructions for enabling remote compilation for iOS.");
			}
			else
			{
				Message = TEXT("Xcode's metal shader compiler was not found, verify Xcode has been installed on the Mac used for remote compilation and that the Mac is accessible via SSH from this machine.");
			}

			FShaderCompilerError* Error = new(OutErrors) FShaderCompilerError();
			Error->ErrorVirtualFilePath = InputFilename;
			Error->ErrorLineString = TEXT("0");
			Error->StrippedErrorMessage = FString(Message);

			bRemoteBuildingConfigured = false;
			bCompileAtRuntime = false;
			bSucceeded = false;
		}
		
		bool bDebugInfoSucceded = false;
		FMetalShaderBytecode Bytecode;
		FMetalShaderDebugInfo DebugCode;
		if (bCompileAtRuntime == false && bMetalCompilerAvailable == true)
		{
			bool bUseSharedPCH = false;
			FString MetalPCHFile;
			
			TCHAR const* CompileType = bRemoteBuildingConfigured ? TEXT("remotely") : TEXT("locally");
			
			bool bFoundStdLib = false;
            FString StdLibPath = GetMetalLibraryPath(ShaderInput.Target.Platform);
            bFoundStdLib = RemoteFileExists(*StdLibPath);
			uint32 PchCRC = 0;
			uint32 PchLen = 0;
			if(bFoundStdLib && ChecksumRemoteFile(StdLibPath, &PchCRC, &PchLen))
			{
				FString VersionedName = FString::Printf(TEXT("metal_stdlib_%u%u%s%s%s%s%s%s.pch"), PchCRC, PchLen, *GUIDHash.ToString(), *CompilerVersion, MinOSVersion, *DebugInfo, *MathMode, Standard);

				// get rid of some not so filename-friendly characters ('=',' ' -> '_')
    		    VersionedName = VersionedName.Replace(TEXT("="), TEXT("_")).Replace(TEXT(" "), TEXT("_"));
				MetalPCHFile = TempDir / VersionedName;
				
				FString RemoteMetalPCHFile = LocalPathToRemote(MetalPCHFile, TempDir);
				if(RemoteFileExists(*RemoteMetalPCHFile))
				{
					bUseSharedPCH = true;
				}
				else
				{
					FMetalShaderBytecodeJob Job;
					Job.ShaderFormat = ShaderInput.ShaderFormat;
					Job.Hash = GUIDHash;
					Job.TmpFolder = TempDir;
					Job.InputFile = StdLibPath;
					Job.OutputFile = MetalPCHFile;
					Job.CompilerVersion = CompilerVersion;
					Job.MinOSVersion = MinOSVersion;
					Job.DebugInfo = DebugInfo;
					Job.MathMode = MathMode;
					Job.Standard = Standard;
					Job.SourceCRCLen = PchLen;
					Job.SourceCRC = PchCRC;
					Job.bRetainObjectFile = false;
					Job.bCompileAsPCH = true;
					
					FMetalShaderBytecodeCooker* BytecodeCooker = new FMetalShaderBytecodeCooker(Job);
					bool bDataWasBuilt = false;
					TArray<uint8> OutData;
					bUseSharedPCH = GetDerivedDataCacheRef().GetSynchronous(BytecodeCooker, OutData, &bDataWasBuilt) && OutData.Num();
					if (bUseSharedPCH)
					{
						FMemoryReader Ar(OutData);
						Ar << Bytecode;
						
						if (!bDataWasBuilt)
						{
							bUseSharedPCH = FFileHelper::SaveArrayToFile(Bytecode.OutputFile, *MetalPCHFile);
							if (!bUseSharedPCH)
							{
								UE_LOG(LogMetalShaderCompiler, Warning, TEXT("Metal Shared PCH failed to save %s - compilation will proceed without a shared PCH: %s."), CompileType, *MetalPCHFile);
							}
						}
					}
					else
					{
						UE_LOG(LogMetalShaderCompiler, Warning, TEXT("Metal Shared PCH generation failed %s - compilation will proceed without a shared PCH: %s."), CompileType, *Job.Message);
					}
				}
			}
			else
			{
				UE_LOG(LogMetalShaderCompiler, Warning, TEXT("Metal Shared PCH generation failed - cannot find metal_stdlib header relative to %s %s."), *MetalToolsPath, CompileType);
			}
		
			uint32 DebugInfoHandle = 0;
			const bool bIsMobile = (ShaderInput.Target.Platform == SP_METAL || ShaderInput.Target.Platform == SP_METAL_MRT);
			if (!bIsMobile && !ShaderInput.Environment.CompilerFlags.Contains(CFLAG_Archive))
			{
				FMetalShaderDebugInfoJob Job;
				Job.ShaderFormat = ShaderInput.ShaderFormat;
				Job.Hash = GUIDHash;
				Job.CompilerVersion = CompilerVersion;
				Job.MinOSVersion = MinOSVersion;
				Job.DebugInfo = DebugInfo;
				Job.MathMode = MathMode;
				Job.Standard = Standard;
				Job.SourceCRCLen = SourceCRCLen;
				Job.SourceCRC = SourceCRC;
				
				Job.MetalCode = MetalCode;
				
				FMetalShaderDebugInfoCooker* DebugInfoCooker = new FMetalShaderDebugInfoCooker(Job);
				
				DebugInfoHandle = GetDerivedDataCacheRef().GetAsynchronous(DebugInfoCooker);
			}
			
			FMetalShaderBytecodeJob Job;
			
			Job.ShaderFormat = ShaderInput.ShaderFormat;
			Job.Hash = GUIDHash;
			Job.TmpFolder = TempDir;
			Job.InputFile = InputFilename;
			if (bUseSharedPCH)
			{
				Job.InputPCHFile = MetalPCHFile;
			}
			Job.OutputFile = OutputFilename;
			Job.OutputObjectFile = ObjFilename;
			Job.CompilerVersion = CompilerVersion;
			Job.MinOSVersion = MinOSVersion;
			Job.DebugInfo = DebugInfo;
			Job.MathMode = MathMode;
			Job.Standard = Standard;
			Job.SourceCRCLen = SourceCRCLen;
			Job.SourceCRC = SourceCRC;
			Job.bRetainObjectFile = ShaderInput.Environment.CompilerFlags.Contains(CFLAG_Archive);
			Job.bCompileAsPCH = false;
			
			FMetalShaderBytecodeCooker* BytecodeCooker = new FMetalShaderBytecodeCooker(Job);
			
			bool bDataWasBuilt = false;
			TArray<uint8> OutData;
			bSucceeded = GetDerivedDataCacheRef().GetSynchronous(BytecodeCooker, OutData, &bDataWasBuilt);
			if (bSucceeded)
			{
				if (OutData.Num())
				{
					FMemoryReader Ar(OutData);
					Ar << Bytecode;
					
					if (!bIsMobile && !ShaderInput.Environment.CompilerFlags.Contains(CFLAG_Archive))
					{
						GetDerivedDataCacheRef().WaitAsynchronousCompletion(DebugInfoHandle);
						TArray<uint8> DebugData;
						bDebugInfoSucceded = GetDerivedDataCacheRef().GetAsynchronousResults(DebugInfoHandle, DebugData);
						if (bDebugInfoSucceded)
						{
							if (DebugData.Num())
							{
								FMemoryReader DebugAr(DebugData);
								DebugAr << DebugCode;
							}
						}
					}
				}
				else
				{
					FShaderCompilerError* Error = new(OutErrors) FShaderCompilerError();
					Error->ErrorVirtualFilePath = InputFilename;
					Error->ErrorLineString = TEXT("0");
					Error->StrippedErrorMessage = FString::Printf(TEXT("DDC returned empty byte array despite claiming that the bytecode was built successfully."));
				}
			}
			else
			{
				FShaderCompilerError* Error = new(OutErrors) FShaderCompilerError();
				Error->ErrorVirtualFilePath = InputFilename;
				Error->ErrorLineString = TEXT("0");
				Error->StrippedErrorMessage = Job.Message;
			}
		}
#else
		// Assume success if we can't compile shaders offline unless we are compiling for archive type operations
		if(CompileProcessAllowsRuntimeShaderCompiling(ShaderInput))
		{
			bSucceeded = true;
		}
		
#endif	// METAL_OFFLINE_COMPILE

		if (bSucceeded)
		{
			// Write out the header and compiled shader code
			FMemoryWriter Ar(ShaderOutput.ShaderCode.GetWriteAccess(), true);
			uint8 PrecompiledFlag = bCompileAtRuntime ? 0 : 1;
			Ar << PrecompiledFlag;

			if (!bCompileAtRuntime)
			{
				Ar << Header;

				// jam it into the output bytes
				Ar.Serialize(Bytecode.OutputFile.GetData(), Bytecode.OutputFile.Num());

				if (ShaderInput.Environment.CompilerFlags.Contains(CFLAG_Archive))
				{
					ShaderOutput.ShaderCode.AddOptionalData('o', Bytecode.ObjectFile.GetData(), Bytecode.ObjectFile.Num());
				}
			}
			else
			{
				// Always debug flag, even if it wasn't set, as we are storing text.
				Header.CompileFlags |= (1 << CFLAG_Debug);
				// Can't be archived as we are storing text and not binary data.
				Header.CompileFlags &= ~(1 << CFLAG_Archive);
				
				Ar << Header;

				// Write out the header and shader source code.
				Ar.Serialize((void*)USFSource, SourceLen + 1 - (USFSource - InShaderSource));

				// store data we can pickup later with ShaderCode.FindOptionalData('n'), could be removed for shipping
				// Daniel L: This GenerateShaderName does not generate a deterministic output among shaders as the shader code can be shared. 
				//			uncommenting this will cause the project to have non deterministic materials and will hurt patch sizes
				//ShaderOutput.ShaderCode.AddOptionalData('n', TCHAR_TO_UTF8(*ShaderInput.GenerateShaderName()));
			}
			
			if (bDebugInfoSucceded && !bCompileAtRuntime && !ShaderInput.Environment.CompilerFlags.Contains(CFLAG_Archive) && DebugCode.CompressedData.Num())
			{
				ShaderOutput.ShaderCode.AddOptionalData('z', DebugCode.CompressedData.GetData(), DebugCode.CompressedData.Num());
				ShaderOutput.ShaderCode.AddOptionalData('p', TCHAR_TO_UTF8(*Bytecode.NativePath));
				ShaderOutput.ShaderCode.AddOptionalData('u', (const uint8*)&DebugCode.UncompressedSize, sizeof(DebugCode.UncompressedSize));
			}
			
			if (ShaderInput.Environment.CompilerFlags.Contains(CFLAG_KeepDebugInfo))
			{
				// store data we can pickup later with ShaderCode.FindOptionalData('n'), could be removed for shipping
				ShaderOutput.ShaderCode.AddOptionalData('n', TCHAR_TO_UTF8(*ShaderInput.GenerateShaderName()));
				if (DebugCode.CompressedData.Num() == 0)
				{
					ShaderOutput.ShaderCode.AddOptionalData('c', TCHAR_TO_UTF8(*MetalCode));
					ShaderOutput.ShaderCode.AddOptionalData('p', TCHAR_TO_UTF8(*Bytecode.NativePath));
				}
			}
			else if (ShaderInput.Environment.CompilerFlags.Contains(CFLAG_Archive))
			{
				ShaderOutput.ShaderCode.AddOptionalData('c', TCHAR_TO_UTF8(*MetalCode));
				ShaderOutput.ShaderCode.AddOptionalData('p', TCHAR_TO_UTF8(*Bytecode.NativePath));
			}
			
			ShaderOutput.NumTextureSamplers = Header.Bindings.NumSamplers;
		}
		
		ShaderOutput.NumInstructions = 0;
		ShaderOutput.bSucceeded = bSucceeded;
	}
}

/*------------------------------------------------------------------------------
	External interface.
------------------------------------------------------------------------------*/

static FString CreateCommandLineHLSLCC( const FString& ShaderFile, const FString& OutputFile, const FString& EntryPoint, EHlslCompileTarget Target, EHlslShaderFrequency Frequency, uint32 CCFlags )
{
	const TCHAR* VersionSwitch = TEXT("-metal");
	switch (Target)
	{
        case HCT_FeatureLevelES2:
		case HCT_FeatureLevelES3_1:
			VersionSwitch = TEXT("-metal");
			break;
		case HCT_FeatureLevelSM4:
			VersionSwitch = TEXT("-metalsm4");
			break;
		case HCT_FeatureLevelSM5:
			VersionSwitch = TEXT("-metalsm5");
			break;
			
		default:
			check(0);
	}
	return CrossCompiler::CreateBatchFileContents(ShaderFile, OutputFile, Frequency, EntryPoint, VersionSwitch, CCFlags, TEXT(""));
}

// For Metal <= 1.1
static const EHlslShaderFrequency FrequencyTable1[] =
{
	HSF_VertexShader,
	HSF_InvalidFrequency,
	HSF_InvalidFrequency,
	HSF_PixelShader,
	HSF_InvalidFrequency,
	HSF_ComputeShader
};

// For Metal >= 1.2
static const EHlslShaderFrequency FrequencyTable2[] =
{
	HSF_VertexShader,
	HSF_HullShader,
	HSF_DomainShader,
	HSF_PixelShader,
	HSF_InvalidFrequency,
	HSF_ComputeShader
};

FString CreateRemoteDataFromEnvironment(const FShaderCompilerEnvironment& Environment)
{
	FString Line = TEXT("\n#if 0 /*BEGIN_REMOTE_SERVER*/\n");
	for (auto Pair : Environment.RemoteServerData)
	{
		Line += FString::Printf(TEXT("%s=%s\n"), *Pair.Key, *Pair.Value);
	}
	Line += TEXT("#endif /*END_REMOTE_SERVER*/\n");
	return Line;
}

void CreateEnvironmentFromRemoteData(const FString& String, FShaderCompilerEnvironment& OutEnvironment)
{
	FString Prolog = TEXT("#if 0 /*BEGIN_REMOTE_SERVER*/");
	int32 FoundBegin = String.Find(Prolog, ESearchCase::CaseSensitive);
	if (FoundBegin == INDEX_NONE)
	{
		return;
	}
	int32 FoundEnd = String.Find(TEXT("#endif /*END_REMOTE_SERVER*/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, FoundBegin);
	if (FoundEnd == INDEX_NONE)
	{
		return;
	}

	// +1 for EOL
	const TCHAR* Ptr = &String[FoundBegin + 1 + Prolog.Len()];
	const TCHAR* PtrEnd = &String[FoundEnd];
	while (Ptr < PtrEnd)
	{
		FString Key;
		if (!CrossCompiler::ParseIdentifier(Ptr, Key))
		{
			return;
		}
		if (!CrossCompiler::Match(Ptr, TEXT("=")))
		{
			return;
		}
		FString Value;
		if (!CrossCompiler::ParseString(Ptr, Value))
		{
			return;
		}
		if (!CrossCompiler::Match(Ptr, '\n'))
		{
			return;
		}
		OutEnvironment.RemoteServerData.FindOrAdd(Key) = Value;
	}
}

void CompileShader_Metal(const FShaderCompilerInput& _Input,FShaderCompilerOutput& Output,const FString& WorkingDirectory)
{
	auto Input = _Input;
	FString PreprocessedShader;
	FShaderCompilerDefinitions AdditionalDefines;
	EHlslCompileTarget HlslCompilerTarget = HCT_FeatureLevelES3_1; // Always ES3.1 for now due to the way RCO has configured the MetalBackend
	EHlslCompileTarget MetalCompilerTarget = HCT_FeatureLevelES3_1; // Varies depending on the actual intended Metal target.

	// Work out which standard we need, this is dependent on the shader platform.
	const bool bIsMobile = (Input.Target.Platform == SP_METAL || Input.Target.Platform == SP_METAL_MRT);
	TCHAR const* StandardPlatform = nullptr;
	if (bIsMobile)
	{
		StandardPlatform = TEXT("ios");
		AdditionalDefines.SetDefine(TEXT("IOS"), 1);
	}
	else
	{
		StandardPlatform = TEXT("osx");
		AdditionalDefines.SetDefine(TEXT("MAC"), 1);
	}
	
	AdditionalDefines.SetDefine(TEXT("COMPILER_HLSLCC"), 1 );
	AdditionalDefines.SetDefine(TEXT("row_major"), TEXT(""));
	AdditionalDefines.SetDefine(TEXT("COMPILER_METAL"), 1);

	static FName NAME_SF_METAL(TEXT("SF_METAL"));
	static FName NAME_SF_METAL_MRT(TEXT("SF_METAL_MRT"));
	static FName NAME_SF_METAL_SM4(TEXT("SF_METAL_SM4"));
	static FName NAME_SF_METAL_SM5(TEXT("SF_METAL_SM5"));
	static FName NAME_SF_METAL_MACES3_1(TEXT("SF_METAL_MACES3_1"));
	static FName NAME_SF_METAL_MACES2(TEXT("SF_METAL_MACES2"));
	static FName NAME_SF_METAL_MRT_MAC(TEXT("SF_METAL_MRT_MAC"));
	
    EMetalGPUSemantics Semantics = EMetalGPUSemanticsMobile;
	
	FString const* MaxVersion = Input.Environment.GetDefinitions().Find(TEXT("MAX_SHADER_LANGUAGE_VERSION"));
    uint8 VersionEnum = 0;
    if (MaxVersion)
    {
        if(MaxVersion->IsNumeric())
        {
            LexicalConversion::FromString(VersionEnum, *(*MaxVersion));
        }
    }
    
    if (Input.ShaderFormat == NAME_SF_METAL)
	{
        VersionEnum = VersionEnum > 0 ? VersionEnum : 0;
        AdditionalDefines.SetDefine(TEXT("METAL_PROFILE"), 1);
	}
	else if (Input.ShaderFormat == NAME_SF_METAL_MRT)
	{
        UE_CLOG(VersionEnum == 0, LogShaders, Warning, TEXT("Metal shader version should be Metal v1.2 or higher for format %s!"), VersionEnum, *Input.ShaderFormat.ToString());
		AdditionalDefines.SetDefine(TEXT("METAL_MRT_PROFILE"), 1);
		VersionEnum = VersionEnum > 0 ? VersionEnum : 2;
		MetalCompilerTarget = HCT_FeatureLevelSM5;
		Semantics = EMetalGPUSemanticsTBDRDesktop;
	}
	else if (Input.ShaderFormat == NAME_SF_METAL_MACES2)
	{
        UE_CLOG(VersionEnum == 0, LogShaders, Warning, TEXT("Metal shader version should be Metal v1.1 or higher for format %s!"), VersionEnum, *Input.ShaderFormat.ToString());
		AdditionalDefines.SetDefine(TEXT("METAL_ES2_PROFILE"), 1);
		VersionEnum = VersionEnum > 0 ? VersionEnum : 1;
		MetalCompilerTarget = HCT_FeatureLevelES2;
		Semantics = EMetalGPUSemanticsImmediateDesktop;
	}
	else if (Input.ShaderFormat == NAME_SF_METAL_MACES3_1)
	{
        UE_CLOG(VersionEnum == 0, LogShaders, Warning, TEXT("Metal shader version should be Metal v1.1 or higher for format %s!"), VersionEnum, *Input.ShaderFormat.ToString());
		AdditionalDefines.SetDefine(TEXT("METAL_PROFILE"), 1);
		VersionEnum = VersionEnum > 0 ? VersionEnum : 1;
		MetalCompilerTarget = HCT_FeatureLevelES3_1;
		Semantics = EMetalGPUSemanticsImmediateDesktop;
	}
	else if (Input.ShaderFormat == NAME_SF_METAL_SM4)
	{
        UE_CLOG(VersionEnum == 0, LogShaders, Warning, TEXT("Metal shader version should be Metal v1.2 or higher for format %s!"), VersionEnum, *Input.ShaderFormat.ToString());
		AdditionalDefines.SetDefine(TEXT("METAL_SM4_PROFILE"), 1);
		AdditionalDefines.SetDefine(TEXT("USING_VERTEX_SHADER_LAYER"), 1);
        VersionEnum = VersionEnum > 0 ? VersionEnum : 2;
		MetalCompilerTarget = HCT_FeatureLevelSM4;
		Semantics = EMetalGPUSemanticsImmediateDesktop;
	}
	else if (Input.ShaderFormat == NAME_SF_METAL_SM5)
	{
        UE_CLOG(VersionEnum == 0, LogShaders, Warning, TEXT("Metal shader version should be Metal v1.2 or higher for format %s!"), VersionEnum, *Input.ShaderFormat.ToString());
		AdditionalDefines.SetDefine(TEXT("METAL_SM5_PROFILE"), 1);
		AdditionalDefines.SetDefine(TEXT("USING_VERTEX_SHADER_LAYER"), 1);
        VersionEnum = VersionEnum > 0 ? VersionEnum : 2;
		MetalCompilerTarget = HCT_FeatureLevelSM5;
		Semantics = EMetalGPUSemanticsImmediateDesktop;
	}
	else if (Input.ShaderFormat == NAME_SF_METAL_MRT_MAC)
	{
        UE_CLOG(VersionEnum == 0, LogShaders, Warning, TEXT("Metal shader version should be Metal v1.2 or higher for format %s!"), VersionEnum, *Input.ShaderFormat.ToString());
		AdditionalDefines.SetDefine(TEXT("METAL_MRT_PROFILE"), 1);
		VersionEnum = VersionEnum > 0 ? VersionEnum : 2;
		MetalCompilerTarget = HCT_FeatureLevelSM5;
		Semantics = EMetalGPUSemanticsTBDRDesktop;
	}
	else
	{
		Output.bSucceeded = false;
		new(Output.Errors) FShaderCompilerError(*FString::Printf(TEXT("Invalid shader format '%s' passed to compiler."), *Input.ShaderFormat.ToString()));
		return;
	}
	
    EMetalTypeBufferMode TypeMode = EMetalTypeBufferModeNone;
	FString MinOSVersion;
	FString StandardVersion;
	switch(VersionEnum)
	{
		case 3:
			// Enable full SM5 feature support so tessellation & fragment UAVs compile
            HlslCompilerTarget = HCT_FeatureLevelSM5;
			StandardVersion = TEXT("2.0");
			MinOSVersion = bIsMobile ? TEXT("") : TEXT("-mmacosx-version-min=10.13");
			break;
		case 2:
			// Enable full SM5 feature support so tessellation & fragment UAVs compile
            HlslCompilerTarget = HCT_FeatureLevelSM5;
			StandardVersion = TEXT("1.2");
			MinOSVersion = bIsMobile ? TEXT("") : TEXT("-mmacosx-version-min=10.12");
			break;
		case 1:
			HlslCompilerTarget = bIsMobile ? HlslCompilerTarget : HCT_FeatureLevelSM5;
			StandardVersion = TEXT("1.1");
			MinOSVersion = bIsMobile ? TEXT("") : TEXT("-mmacosx-version-min=10.11");
			break;
		case 0:
		default:
			check(bIsMobile);
			StandardVersion = TEXT("1.0");
			MinOSVersion = TEXT("");
			break;
	}
	
	// Force floats if the material requests it
	const bool bUseFullPrecisionInPS = Input.Environment.CompilerFlags.Contains(CFLAG_UseFullPrecisionInPS);
	if (bUseFullPrecisionInPS || (VersionEnum < 2)) // Too many bugs in Metal 1.0 & 1.1 with half floats the more time goes on and the compiler stack changes
	{
		AdditionalDefines.SetDefine(TEXT("FORCE_FLOATS"), (uint32)1);
	}
	
	FString Standard = FString::Printf(TEXT("-std=%s-metal%s"), StandardPlatform, *StandardVersion);
	
	bool const bDirectCompile = FParse::Param(FCommandLine::Get(), TEXT("directcompile"));
	if (bDirectCompile)
	{
		Input.DumpDebugInfoPath = FPaths::GetPath(Input.VirtualSourceFilePath);
	}
	
	const bool bDumpDebugInfo = (Input.DumpDebugInfoPath != TEXT("") && IFileManager::Get().DirectoryExists(*Input.DumpDebugInfoPath));

	// Allow the shader pipeline to override the platform default in here.
	uint32 MaxUnrollLoops = 32;
	if (Input.Environment.CompilerFlags.Contains(CFLAG_AvoidFlowControl))
	{
		AdditionalDefines.SetDefine(TEXT("COMPILER_SUPPORTS_ATTRIBUTES"), (uint32)0);
		MaxUnrollLoops = 1024; // Max. permitted by hlslcc
	}
	else if (Input.Environment.CompilerFlags.Contains(CFLAG_PreferFlowControl))
	{
		AdditionalDefines.SetDefine(TEXT("COMPILER_SUPPORTS_ATTRIBUTES"), (uint32)0);
		MaxUnrollLoops = 0;
	}
	else
	{
		AdditionalDefines.SetDefine(TEXT("COMPILER_SUPPORTS_ATTRIBUTES"), (uint32)1);
	}

	if (!Input.bSkipPreprocessedCache && !bDirectCompile)
	{
		FString const* UsingTessellationDefine = Input.Environment.GetDefinitions().Find(TEXT("USING_TESSELLATION"));
		bool bUsingTessellation = (UsingTessellationDefine != nullptr && FString("1") == *UsingTessellationDefine);
		if (bUsingTessellation && (Input.Target.Frequency == SF_Vertex))
		{
			// force HULLSHADER on so that VS that is USING_TESSELLATION can be built together with the proper HS
			FString const* VertexShaderDefine = Input.Environment.GetDefinitions().Find(TEXT("VERTEXSHADER"));
			check(VertexShaderDefine && FString("1") == *VertexShaderDefine);
			FString const* HullShaderDefine = Input.Environment.GetDefinitions().Find(TEXT("HULLSHADER"));
			check(HullShaderDefine && FString("0") == *HullShaderDefine);
			Input.Environment.SetDefine(TEXT("HULLSHADER"), 1u);
		}
		if (Input.Target.Frequency == SF_Hull)
		{
			check(bUsingTessellation);
			// force VERTEXSHADER on so that HS that is USING_TESSELLATION can be built together with the proper VS
			FString const* VertexShaderDefine = Input.Environment.GetDefinitions().Find(TEXT("VERTEXSHADER"));
			check(VertexShaderDefine && FString("0") == *VertexShaderDefine);
			FString const* HullShaderDefine = Input.Environment.GetDefinitions().Find(TEXT("HULLSHADER"));
			check(HullShaderDefine && FString("1") == *HullShaderDefine);

			// enable VERTEXSHADER so that this HS will hash uniquely with its associated VS
			// We do not want a given HS to be shared among numerous VS'Sampler
			// this should accomplish that goal -- see GenerateOutputHash
			Input.Environment.SetDefine(TEXT("VERTEXSHADER"), 1u);
		}
	}

	if (Input.bSkipPreprocessedCache)
	{
		if (!FFileHelper::LoadFileToString(PreprocessedShader, *Input.VirtualSourceFilePath))
		{
			return;
		}

		// Remove const as we are on debug-only mode
		CrossCompiler::CreateEnvironmentFromResourceTable(PreprocessedShader, (FShaderCompilerEnvironment&)Input.Environment);
		CreateEnvironmentFromRemoteData(PreprocessedShader, (FShaderCompilerEnvironment&)Input.Environment);
	}
	else
	{
		if (!PreprocessShader(PreprocessedShader, Output, Input, AdditionalDefines))
		{
			// The preprocessing stage will add any relevant errors.
			return;
		}
	}

	if (Input.ShaderFormat != NAME_SF_METAL_SM5)
	{
		// Disable instanced stereo on everything but Metal SM5 for 10.13+
		StripInstancedStereo(PreprocessedShader);
	}

	char* MetalShaderSource = NULL;
	char* ErrorLog = NULL;

	const EHlslShaderFrequency Frequency = HlslCompilerTarget < HCT_FeatureLevelSM5 ? FrequencyTable1[Input.Target.Frequency] : FrequencyTable2[Input.Target.Frequency];
	if (Frequency == HSF_InvalidFrequency)
	{
		Output.bSucceeded = false;
		FShaderCompilerError* NewError = new(Output.Errors) FShaderCompilerError();
		NewError->StrippedErrorMessage = FString::Printf(
			TEXT("%s shaders not supported for use in Metal."),
			CrossCompiler::GetFrequencyName((EShaderFrequency)Input.Target.Frequency)
			);
		return;
	}


	// This requires removing the HLSLCC_NoPreprocess flag later on!
	if (!RemoveUniformBuffersFromSource(PreprocessedShader))
	{
		return;
	}

	// Write out the preprocessed file and a batch file to compile it if requested (DumpDebugInfoPath is valid)
	if (bDumpDebugInfo)
	{
		FArchive* FileWriter = IFileManager::Get().CreateFileWriter(*(Input.DumpDebugInfoPath / FPaths::GetBaseFilename(Input.GetSourceFilename() + TEXT(".usf"))));
		if (FileWriter)
		{
			auto AnsiSourceFile = StringCast<ANSICHAR>(*PreprocessedShader);
			FileWriter->Serialize((ANSICHAR*)AnsiSourceFile.Get(), AnsiSourceFile.Length());
			{
				FString Line = CrossCompiler::CreateResourceTableFromEnvironment(Input.Environment);
				FileWriter->Serialize(TCHAR_TO_ANSI(*Line), Line.Len());

				// add the remote data if necessary
//				if (IsRemoteBuildingConfigured(&Input.Environment))
				{
					Line = CreateRemoteDataFromEnvironment(Input.Environment);
					FileWriter->Serialize(TCHAR_TO_ANSI(*Line), Line.Len());
				}
			}
			FileWriter->Close();
			delete FileWriter;
		}

		if (Input.bGenerateDirectCompileFile)
		{
			FFileHelper::SaveStringToFile(CreateShaderCompilerWorkerDirectCommandLine(Input), *(Input.DumpDebugInfoPath / TEXT("DirectCompile.txt")));
		}
	}

	uint32 CCFlags = HLSLCC_NoPreprocess | HLSLCC_PackUniforms | HLSLCC_FixAtomicReferences | HLSLCC_KeepSamplerAndImageNames;
		
	
	FSHAHash GUIDHash;
	if (!bDirectCompile)
	{
		TArray<FString> GUIDFiles;
		GUIDFiles.Add(FPaths::ConvertRelativePathToFull(TEXT("/Engine/Public/MetalCommon.ush")));
		GUIDFiles.Add(FPaths::ConvertRelativePathToFull(TEXT("/Engine/Public/ShaderVersion.ush")));
		GUIDHash = GetShaderFilesHash(GUIDFiles);
	}
	else
	{
		FGuid Guid = FGuid::NewGuid();
		FSHA1::HashBuffer(&Guid, sizeof(FGuid), GUIDHash.Hash);
	}
	
	// Required as we added the RemoveUniformBuffersFromSource() function (the cross-compiler won't be able to interpret comments w/o a preprocessor)
	CCFlags &= ~HLSLCC_NoPreprocess;

	FMetalShaderOutputCooker* Cooker = new FMetalShaderOutputCooker(Input,Output,WorkingDirectory, PreprocessedShader, GUIDHash, VersionEnum, CCFlags, HlslCompilerTarget, MetalCompilerTarget, Semantics, TypeMode, MaxUnrollLoops, Frequency, bDumpDebugInfo, Standard, MinOSVersion);
		
	bool bDataWasBuilt = false;
	TArray<uint8> OutData;
	bool bCompiled = GetDerivedDataCacheRef().GetSynchronous(Cooker, OutData, &bDataWasBuilt) && OutData.Num();
	if (bCompiled && !bDataWasBuilt)
	{
		FShaderCompilerOutput TestOutput;
		FMemoryReader Reader(OutData);
		Reader << TestOutput;
			
		// If successful update the header & optional data to provide the proper material name
		if (TestOutput.bSucceeded)
		{
			TArray<uint8> const& Code = TestOutput.ShaderCode.GetReadAccess();
				
			// Parse the existing data and extract the source code. We have to recompile it
			FShaderCodeReader ShaderCode(Code);
			FMemoryReader Ar(Code, true);
			Ar.SetLimitSize(ShaderCode.GetActualShaderCodeSize());
				
			// was the shader already compiled offline?
			uint8 OfflineCompiledFlag;
			Ar << OfflineCompiledFlag;
			check(OfflineCompiledFlag == 0 || OfflineCompiledFlag == 1);
				
			// get the header
			FMetalCodeHeader Header = { 0 };
			Ar << Header;
				
			// remember where the header ended and code (precompiled or source) begins
			int32 CodeOffset = Ar.Tell();
			uint32 CodeSize = ShaderCode.GetActualShaderCodeSize() - CodeOffset;
			const uint8* SourceCodePtr = (uint8*)Code.GetData() + CodeOffset;
				
			// Copy the non-optional shader bytecode
			TArray<uint8> SourceCode;
			SourceCode.Append(SourceCodePtr, ShaderCode.GetActualShaderCodeSize() - CodeOffset);
				
			// store data we can pickup later with ShaderCode.FindOptionalData('n'), could be removed for shipping
			ANSICHAR const* Text = ShaderCode.FindOptionalData('c');
			ANSICHAR const* Path = ShaderCode.FindOptionalData('p');
			ANSICHAR const* Name = ShaderCode.FindOptionalData('n');
				
			int32 ObjectSize = 0;
			uint8 const* Object = ShaderCode.FindOptionalDataAndSize('o', ObjectSize);
				
			int32 DebugSize = 0;
			uint8 const* Debug = ShaderCode.FindOptionalDataAndSize('z', DebugSize);
				
			int32 UncSize = 0;
			uint8 const* UncData = ShaderCode.FindOptionalDataAndSize('u', UncSize);
				
			// Replace the shader name.
			if (Header.ShaderName.Len())
			{
				Header.ShaderName = Input.GenerateShaderName();
			}
				
			// Write out the header and shader source code.
			FMemoryWriter WriterAr(Output.ShaderCode.GetWriteAccess(), true);
			WriterAr << OfflineCompiledFlag;
			WriterAr << Header;
			WriterAr.Serialize((void*)SourceCodePtr, CodeSize);
				
			if (Name)
			{
				Output.ShaderCode.AddOptionalData('n', TCHAR_TO_UTF8(*Input.GenerateShaderName()));
			}
			if (Path)
			{
				Output.ShaderCode.AddOptionalData('p', Path);
			}
			if (Text)
			{
				Output.ShaderCode.AddOptionalData('c', Text);
			}
			if (Object && ObjectSize)
			{
				Output.ShaderCode.AddOptionalData('o', Object, ObjectSize);
			}
			if (Debug && DebugSize && UncSize && UncData)
			{
				Output.ShaderCode.AddOptionalData('z', Debug, DebugSize);
				Output.ShaderCode.AddOptionalData('u', UncData, UncSize);
			}
				
			Output.ParameterMap = TestOutput.ParameterMap;
			Output.Errors = TestOutput.Errors;
			Output.Target = TestOutput.Target;
			Output.NumInstructions = TestOutput.NumInstructions;
			Output.NumTextureSamplers = TestOutput.NumTextureSamplers;
			Output.bSucceeded = TestOutput.bSucceeded;
			Output.bFailedRemovingUnused = TestOutput.bFailedRemovingUnused;
			Output.bSupportsQueryingUsedAttributes = TestOutput.bSupportsQueryingUsedAttributes;
			Output.UsedAttributes = TestOutput.UsedAttributes;
		}
	}
}

bool StripShader_Metal(TArray<uint8>& Code, class FString const& DebugPath, bool const bNative)
{
	bool bSuccess = false;
	
	FShaderCodeReader ShaderCode(Code);
	FMemoryReader Ar(Code, true);
	Ar.SetLimitSize(ShaderCode.GetActualShaderCodeSize());
	
	// was the shader already compiled offline?
	uint8 OfflineCompiledFlag;
	Ar << OfflineCompiledFlag;
	
	if(bNative && OfflineCompiledFlag == 1)
	{
		// get the header
		FMetalCodeHeader Header = { 0 };
		Ar << Header;
		
		// Must be compiled for archiving or something is very wrong.
		if(bNative == false || Header.CompileFlags & (1 << CFLAG_Archive))
		{
			bSuccess = true;
			
			// remember where the header ended and code (precompiled or source) begins
			int32 CodeOffset = Ar.Tell();
			const uint8* SourceCodePtr = (uint8*)Code.GetData() + CodeOffset;
			
			// Copy the non-optional shader bytecode
			TArray<uint8> SourceCode;
			SourceCode.Append(SourceCodePtr, ShaderCode.GetActualShaderCodeSize() - CodeOffset);
			
			const ANSICHAR* ShaderSource = ShaderCode.FindOptionalData('c');
			bool const bHasShaderSource = (ShaderSource && FCStringAnsi::Strlen(ShaderSource) > 0);
			
			const ANSICHAR* ShaderPath = ShaderCode.FindOptionalData('p');
			bool const bHasShaderPath = (ShaderPath && FCStringAnsi::Strlen(ShaderPath) > 0);
			
			if (bHasShaderSource && bHasShaderPath)
			{
				FString DebugFilePath = DebugPath / FString(ShaderPath);
				FString DebugFolderPath = FPaths::GetPath(DebugFilePath);
				if (IFileManager::Get().MakeDirectory(*DebugFolderPath, true))
				{
					FString TempPath = FPaths::CreateTempFilename(*DebugFolderPath, TEXT("MetalShaderFile-"), TEXT(".metal"));
					FFileHelper::SaveStringToFile(FString(ShaderSource), *TempPath);
					IFileManager::Get().Move(*DebugFilePath, *TempPath, false, false, true, false);
					IFileManager::Get().Delete(*TempPath);
				}
			}
			
			if (bNative)
			{
				int32 ObjectSize = 0;
				const uint8* ShaderObject = ShaderCode.FindOptionalDataAndSize('o', ObjectSize);
				check(ShaderObject && ObjectSize);

				TArray<uint8> ObjectCodeArray;
				ObjectCodeArray.Append(ShaderObject, ObjectSize);
				SourceCode = ObjectCodeArray;
			}
			
			// Strip any optional data
			if (bNative || ShaderCode.GetOptionalDataSize() > 0)
			{
				// Write out the header and compiled shader code
				FShaderCode NewCode;
				FMemoryWriter NewAr(NewCode.GetWriteAccess(), true);
				NewAr << OfflineCompiledFlag;
				NewAr << Header;
				
				// jam it into the output bytes
				NewAr.Serialize(SourceCode.GetData(), SourceCode.Num());
				
				Code = NewCode.GetReadAccess();
			}
		}
		else
		{
			UE_LOG(LogShaders, Error, TEXT("Shader stripping failed: shader %s (Len: %0.8x, CRC: %0.8x) was not compiled for archiving into a native library (Native: %s, Compile Flags: %0.8x)!"), *Header.ShaderName, Header.SourceLen, Header.SourceCRC, bNative ? TEXT("true") : TEXT("false"), (uint32)Header.CompileFlags);
		}
	}
	else
	{
		UE_LOG(LogShaders, Error, TEXT("Shader stripping failed: shader %s (Native: %s, Offline Compiled: %d) was not compiled to bytecode for native archiving!"), *DebugPath, bNative ? TEXT("true") : TEXT("false"), OfflineCompiledFlag);
	}
	
	return bSuccess;
}

EShaderPlatform MetalShaderFormatToLegacyShaderPlatform(FName ShaderFormat)
{
	static FName NAME_SF_METAL(TEXT("SF_METAL"));
	static FName NAME_SF_METAL_MRT(TEXT("SF_METAL_MRT"));
	static FName NAME_SF_METAL_SM4(TEXT("SF_METAL_SM4"));
	static FName NAME_SF_METAL_SM5(TEXT("SF_METAL_SM5"));
	static FName NAME_SF_METAL_MRT_MAC(TEXT("SF_METAL_MRT_MAC"));
	static FName NAME_SF_METAL_MACES3_1(TEXT("SF_METAL_MACES3_1"));
	static FName NAME_SF_METAL_MACES2(TEXT("SF_METAL_MACES2"));
	
	if (ShaderFormat == NAME_SF_METAL)				return SP_METAL;
	if (ShaderFormat == NAME_SF_METAL_MRT)			return SP_METAL_MRT;
	if (ShaderFormat == NAME_SF_METAL_MRT_MAC)		return SP_METAL_MRT_MAC;
	if (ShaderFormat == NAME_SF_METAL_SM5)			return SP_METAL_SM5;
	if (ShaderFormat == NAME_SF_METAL_SM4)			return SP_METAL_SM4;
	if (ShaderFormat == NAME_SF_METAL_MACES3_1)		return SP_METAL_MACES3_1;
	if (ShaderFormat == NAME_SF_METAL_MACES2)		return SP_METAL_MACES2;
	
	return SP_NumPlatforms;
}

uint64 AppendShader_Metal(FName const& Format, FString const& WorkingDir, const FSHAHash& Hash, TArray<uint8>& InShaderCode)
{
	uint64 Id = 0;
	
#if METAL_OFFLINE_COMPILE

	// Remote building needs to run through the check code for the Metal tools paths to be available for remotes (ensures this will work on incremental launches if there are no shaders to build)
	bool bRemoteBuildingConfigured = IsRemoteBuildingConfigured();
	
	EShaderPlatform Platform = MetalShaderFormatToLegacyShaderPlatform(Format);
	FString MetalPath = GetMetalBinaryPath(Platform);
	FString MetalToolsPath = GetMetalToolsPath(Platform);
	if (MetalPath.Len() > 0 && MetalToolsPath.Len() > 0)
	{
		// Parse the existing data and extract the source code. We have to recompile it
		FShaderCodeReader ShaderCode(InShaderCode);
		FMemoryReader Ar(InShaderCode, true);
		Ar.SetLimitSize(ShaderCode.GetActualShaderCodeSize());
		
		// was the shader already compiled offline?
		uint8 OfflineCompiledFlag;
		Ar << OfflineCompiledFlag;
		if (OfflineCompiledFlag == 1)
		{
			// get the header
			FMetalCodeHeader Header = { 0 };
			Ar << Header;
			
			// Must be compiled for archiving or something is very wrong.
			if(Header.CompileFlags & (1 << CFLAG_Archive))
			{
				// remember where the header ended and code (precompiled or source) begins
				int32 CodeOffset = Ar.Tell();
				const uint8* SourceCodePtr = (uint8*)InShaderCode.GetData() + CodeOffset;
				
				// Copy the non-optional shader bytecode
				int32 ObjectCodeDataSize = 0;
				uint8 const* Object = ShaderCode.FindOptionalDataAndSize('o', ObjectCodeDataSize);
				TArrayView<const uint8> ObjectCodeArray(Object, ObjectCodeDataSize);
				
				// Object code segment
				FString ObjFilename = WorkingDir / FString::Printf(TEXT("Main_%0.8x_%0.8x.o"), Header.SourceLen, Header.SourceCRC);
				
				bool const bHasObjectData = (ObjectCodeDataSize > 0) || IFileManager::Get().FileExists(*ObjFilename);
				if (bHasObjectData)
				{
					// metal commandlines
					int32 ReturnCode = 0;
					FString Results;
					FString Errors;
					
					bool bHasObjectFile = IFileManager::Get().FileExists(*ObjFilename);
					if (ObjectCodeDataSize > 0)
					{
						// write out shader object code source (IR) for archiving to a single library file later
						if( FFileHelper::SaveArrayToFile(ObjectCodeArray, *ObjFilename) )
						{
							bHasObjectFile = true;
						}
					}
					
					if (bHasObjectFile)
					{
						Id = ((uint64)Header.SourceLen << 32) | Header.SourceCRC;
						
						// This is going to get serialised into the shader resource archive we don't anything but the header info now with the archive flag set
						Header.CompileFlags |= (1 << CFLAG_Archive);
						
						// Write out the header and compiled shader code
						FShaderCode NewCode;
						FMemoryWriter NewAr(NewCode.GetWriteAccess(), true);
						NewAr << OfflineCompiledFlag;
						NewAr << Header;
						
						InShaderCode = NewCode.GetReadAccess();
						
						UE_LOG(LogShaders, Display, TEXT("Archiving succeeded: shader %s (Len: %0.8x, CRC: %0.8x, SHA: %s)"), *Header.ShaderName, Header.SourceLen, Header.SourceCRC, *Hash.ToString());
					}
					else
					{
						UE_LOG(LogShaders, Error, TEXT("Archiving failed: failed to write temporary file %s for shader %s (Len: %0.8x, CRC: %0.8x, SHA: %s)"), *ObjFilename, *Header.ShaderName, Header.SourceLen, Header.SourceCRC, *Hash.ToString());
					}
				}
				else
				{
					UE_LOG(LogShaders, Error, TEXT("Archiving failed: shader %s (Len: %0.8x, CRC: %0.8x, SHA: %s) has no object data"), *Header.ShaderName, Header.SourceLen, Header.SourceCRC, *Hash.ToString());
				}
			}
			else
			{
				UE_LOG(LogShaders, Error, TEXT("Archiving failed: shader %s (Len: %0.8x, CRC: %0.8x, SHA: %s) was not compiled for archiving (Compile Flags: %0.8x)!"), *Header.ShaderName, Header.SourceLen, Header.SourceCRC, *Hash.ToString(), (uint32)Header.CompileFlags);
			}
		}
		else
		{
			UE_LOG(LogShaders, Error, TEXT("Archiving failed: shader SHA: %s was not compiled to bytecode (%d)!"), *Hash.ToString(), OfflineCompiledFlag);
		}
	}
	else
#endif
	{
		UE_LOG(LogShaders, Error, TEXT("Archiving failed: no Xcode install on the local machine or a remote Mac."));
	}
	return Id;
}

bool FinalizeLibrary_Metal(FName const& Format, FString const& WorkingDir, FString const& LibraryPath, TSet<uint64> const& Shaders, class FString const& DebugOutputDir)
{
	bool bOK = false;
	
#if METAL_OFFLINE_COMPILE

	// Check remote building before the Metal tools paths to ensure configured
	bool bRemoteBuildingConfigured = IsRemoteBuildingConfigured();

	EShaderPlatform Platform = MetalShaderFormatToLegacyShaderPlatform(Format);
	FString MetalPath = GetMetalBinaryPath(Platform);
	FString MetalToolsPath = GetMetalToolsPath(Platform);
	if (MetalPath.Len() > 0 && MetalToolsPath.Len() > 0)
	{
		int32 ReturnCode = 0;
		FString Results;
		FString Errors;
		
		FString ArchivePath = WorkingDir + TEXT(".metalar");
		
		IFileManager::Get().Delete(*ArchivePath);
		IFileManager::Get().Delete(*LibraryPath);
	
		// Check and init remote handling
		const bool bBuildingRemotely = (!PLATFORM_MAC || UNIXLIKE_TO_MAC_REMOTE_BUILDING) && bRemoteBuildingConfigured;
		FString RemoteDestination = TEXT("/tmp");
		if(bBuildingRemotely)
		{
			RemoteDestination = MakeRemoteTempFolder(TEXT("/tmp"));
			ArchivePath = LocalPathToRemote(ArchivePath, RemoteDestination);
		}
		
		bool bArchiveFileValid = false;
		
		// Archive build phase - like unix ar, build metal archive from all the object files
		{
			// Metal commandlines
			UE_LOG(LogShaders, Display, TEXT("Archiving %d shaders for shader platform: %s"), Shaders.Num(), *Format.GetPlainNameString());
			if(bRemoteBuildingConfigured)
			{
				UE_LOG(LogShaders, Display, TEXT("Attempting to Archive using remote at '%s@%s' with ssh identity '%s'"), *GRemoteBuildServerUser, *GRemoteBuildServerHost, *GRemoteBuildServerSSHKey);
			}
			
			int32 Index = 0;
			FString MetalArPath = MetalToolsPath + TEXT("/metal-ar");
			FString Params = FString::Printf(TEXT("q \"%s\""), *ArchivePath);
			
			const uint32 ArgCommandMax = GetMaxArgLength();
			const uint32 ArchiveOperationCommandLength = bBuildingRemotely ? GSSHPath.Len() + MetalArPath.Len() : MetalArPath.Len();  

			for (auto Shader : Shaders)
			{
				uint32 Len = (Shader >> 32);
				uint32 CRC = (Shader & 0xffffffff);
				
				// Build source file name path
				UE_LOG(LogShaders, Display, TEXT("[%d/%d] %s Main_%0.8x_%0.8x.o"), ++Index, Shaders.Num(), *Format.GetPlainNameString(), Len, CRC);
				FString SourceFileNameParam = FString::Printf(TEXT("\"%s/Main_%0.8x_%0.8x.o\""), *WorkingDir, Len, CRC);
				
				// Remote builds copy file and swizzle Source File Name param
				if(bBuildingRemotely)
				{
					FString DestinationFileNameParam = FString::Printf(TEXT("%s/Main_%0.8x_%0.8x.o"), *RemoteDestination, Len, CRC);
					if(!CopyLocalFileToRemote(SourceFileNameParam, DestinationFileNameParam))
					{
						UE_LOG(LogShaders, Error, TEXT("Archiving failed: Copy object file to remote failed for file:%s"), *SourceFileNameParam);
						Params.Empty();
						break;
					}
					SourceFileNameParam = FString::Printf(TEXT("\"%s\""), *DestinationFileNameParam);		// Wrap each param in it's own string
				}
				
				// Have we gone past sensible argument length - incremently archive
				if (Params.Len() + SourceFileNameParam.Len() + ArchiveOperationCommandLength + 3 >= (ArgCommandMax / 2))
				{
					ExecRemoteProcess( *MetalArPath, *Params, &ReturnCode, &Results, &Errors );
					bArchiveFileValid = RemoteFileExists(*ArchivePath);
					
					if (ReturnCode != 0 || !bArchiveFileValid)
					{
						UE_LOG(LogShaders, Error, TEXT("Archiving failed: metal-ar failed with code %d: %s"), ReturnCode, *Errors);
						Params.Empty();
						break;
					}
					
					// Reset params
					Params = FString::Printf(TEXT("q \"%s\""), *ArchivePath);
				}
				
				// Safe to add this file
				Params += TEXT(" ");
				Params += SourceFileNameParam;
			}
		
			// Any left over files - incremently archive again
			if (!Params.IsEmpty())
			{
				ExecRemoteProcess( *MetalArPath, *Params, &ReturnCode, &Results, &Errors );
                bArchiveFileValid = RemoteFileExists(*ArchivePath);
				
				if (ReturnCode != 0 || !bArchiveFileValid)
				{
					UE_LOG(LogShaders, Error, TEXT("Archiving failed: metal-ar failed with code %d: %s"), ReturnCode, *Errors);
				}
			}
			
			// If remote, leave the archive file where it is - we don't actually need it locally
		}
		
		// Lib build phase, metalar to metallib 
		{
			// handle compile error
			if (ReturnCode == 0 && bArchiveFileValid)
			{
				UE_LOG(LogShaders, Display, TEXT("Post-processing archive for shader platform: %s"), *Format.GetPlainNameString());
				
				FString MetalLibPath = MetalToolsPath + TEXT("/metallib");
				
				FString RemoteLibPath = LocalPathToRemote(LibraryPath, RemoteDestination);
				FString Params = FString::Printf(TEXT("-o=\"%s\" \"%s\""), *RemoteLibPath, *ArchivePath);
					
				ExecRemoteProcess( *MetalLibPath, *Params, &ReturnCode, &Results, &Errors );
				
				if(ReturnCode == 0)
                {
                    // There is problem going to location with spaces using remote copy (at least on Mac no combination of \ and/or "" works) - work around this issue @todo investigate this further
                    FString LocalCopyLocation = FPaths::Combine(TEXT("/tmp"),FPaths::GetCleanFilename(LibraryPath));
						
                    if(bBuildingRemotely && CopyRemoteFileToLocal(RemoteLibPath, LocalCopyLocation))
                    {
                        IFileManager::Get().Move(*LibraryPath, *LocalCopyLocation);
                    }
                }
				
				// handle compile error
				if (ReturnCode == 0 && IFileManager::Get().FileSize(*LibraryPath) > 0)
				{
					bOK = true;
				}
				else
				{
					UE_LOG(LogShaders, Error, TEXT("Archiving failed: metallib failed with code %d: %s"), ReturnCode, *Errors);
				}
			}
			else
			{
				UE_LOG(LogShaders, Error, TEXT("Archiving failed: no valid input for metallib."));
			}
		}
	}
	else
#endif
	{
		UE_LOG(LogShaders, Error, TEXT("Archiving failed: no Xcode install."));
	}
	
#if METAL_OFFLINE_COMPILE
#if PLATFORM_MAC && !UNIXLIKE_TO_MAC_REMOTE_BUILDING
	if(bOK)
	{
		//TODO add a check in here - this will only work if we have shader archiving with debug info set.
		
		//We want to archive all the metal shader source files so that they can be unarchived into a debug location
		//This allows the debugging of optimised metal shaders within the xcode tool set
		//Currently using the 'tar' system tool to create a compressed tape archive
		
		//Place the archive in the same position as the .metallib file
		FString CompressedPath = LibraryPath;
		
		//Strip an trailing path extension if it has one
		int32 TrailingSlashIdx;
		if(LibraryPath.FindLastChar('.', TrailingSlashIdx))
		{
			CompressedPath = LibraryPath.Mid( 0, TrailingSlashIdx );
		}
		
		//Add our preferred archive extension
		CompressedPath += ".tgz";
		
		//Due to the limitations of the 'tar' command and running through NSTask,
		//the most reliable way is to feed it a list of local file name (-T) with a working path set (-C)
		//if we built the list with absolute paths without -C then we'd get the full folder structure in the archive
		//I don't think we want this here
		
		//Build a file list that 'tar' can access
		const FString FileListPath = DebugOutputDir / TEXT("ArchiveInput.txt");
		IFileManager::Get().Delete( *FileListPath );
		
		{
			//Find the metal source files
			TArray<FString> FilesToArchive;
			IFileManager::Get().FindFilesRecursive( FilesToArchive, *DebugOutputDir, TEXT("*.metal"), true, false, false );
			
			//Write the local file names into the target file
			FArchive* FileListHandle = IFileManager::Get().CreateFileWriter( *FileListPath );
			if(FileListHandle)
			{
				const FString NewLine = TEXT("\n");
				
				const FString DebugDir = DebugOutputDir / *Format.GetPlainNameString();
				
				for(FString FileName : FilesToArchive)
				{
					FPaths::MakePathRelativeTo(FileName, *DebugDir);
					
					FString TextLine = FileName + NewLine;
					
					//We don't want the string to archive through the << operator otherwise we'd be creating a binary file - we need text
					auto AnsiFullPath = StringCast<ANSICHAR>( *TextLine );
					FileListHandle->Serialize( (ANSICHAR*)AnsiFullPath.Get(), AnsiFullPath.Length() );
				}
				
				//Clean up
				FileListHandle->Close();
				delete FileListHandle;
			}
		}
		
		//Setup the NSTask command and parameter list, Archive (-c) and Compress (-z) to target file (-f) the metal file list (-T) using a local dir in archive (-C).
		FString ArchiveCommand = TEXT("/usr/bin/tar");
		FString ArchiveCommandParams = FString::Printf( TEXT("czf \"%s\" -C \"%s\" -T \"%s\""), *CompressedPath, *DebugOutputDir, *FileListPath );
		
		int32 ReturnCode = -1;
		FString Result;
		FString Errors;
		
		//Execute command, this should end up with a .tgz file in the same location at the .metallib file
		if(!FPlatformProcess::ExecProcess( *ArchiveCommand, *ArchiveCommandParams, &ReturnCode, &Result, &Errors ) || ReturnCode != 0)
		{
			UE_LOG(LogShaders, Error, TEXT("Archive Shader Source failed %d: %s"), ReturnCode, *Errors);
		}
	}
#endif
#endif
	
	return bOK;
}
