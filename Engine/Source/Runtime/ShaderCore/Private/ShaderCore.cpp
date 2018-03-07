// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ShaderCore.h: Shader core module implementation.
=============================================================================*/

#include "ShaderCore.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeLock.h"
#include "Stats/StatsMisc.h"
#include "Misc/ScopedSlowTask.h"
#include "Shader.h"
#include "VertexFactory.h"
#include "Modules/ModuleManager.h"

FSHAHash GGlobalShaderMapHash;

static TAutoConsoleVariable<int32> CVarShaderDevelopmentMode(
	TEXT("r.ShaderDevelopmentMode"),
	0,
	TEXT("0: Default, 1: Enable various shader development utilities, such as the ability to retry on failed shader compile, and extra logging as shaders are compiled."),
	ECVF_Default);

void UpdateShaderDevelopmentMode()
{
	// Keep LogShaders verbosity in sync with r.ShaderDevelopmentMode
	// r.ShaderDevelopmentMode==1 results in all LogShaders log messages being displayed
	bool bLogShadersUnsuppressed = UE_LOG_ACTIVE(LogShaders, Log);
	bool bDesiredLogShadersUnsuppressed = CVarShaderDevelopmentMode.GetValueOnGameThread() == 1;

	if (bLogShadersUnsuppressed != bDesiredLogShadersUnsuppressed)
	{
		if (bDesiredLogShadersUnsuppressed)
		{
			UE_SET_LOG_VERBOSITY(LogShaders, Log);
		}
		else
		{
			UE_SET_LOG_VERBOSITY(LogShaders, Error);
		}
	}
}

class FShaderCoreModule : public FDefaultModuleImpl
{
public:

	/**
	 * Called right after the module DLL has been loaded and the module object has been created
	 */
	virtual void StartupModule()
	{
		// Create the global shader map hash
		{
			FSHA1 HashState;
			const TCHAR* GlobalShaderString = TEXT("GlobalShaderMap");
			HashState.UpdateWithString(GlobalShaderString, FCString::Strlen(GlobalShaderString));
			HashState.Final();
			HashState.GetHash(&GGlobalShaderMapHash.Hash[0]);
		}

		IConsoleManager::Get().RegisterConsoleVariableSink_Handle(FConsoleCommandDelegate::CreateStatic(&UpdateShaderDevelopmentMode));
	}
};

IMPLEMENT_MODULE( FShaderCoreModule, ShaderCore );

//
// Shader stats
//

DEFINE_STAT(STAT_ShaderCompiling_NiagaraShaders);
DEFINE_STAT(STAT_ShaderCompiling_NumTotalNiagaraShaders);

DEFINE_STAT(STAT_ShaderCompiling_MaterialShaders);
DEFINE_STAT(STAT_ShaderCompiling_GlobalShaders);
DEFINE_STAT(STAT_ShaderCompiling_RHI);
DEFINE_STAT(STAT_ShaderCompiling_HashingShaderFiles);
DEFINE_STAT(STAT_ShaderCompiling_LoadingShaderFiles);
DEFINE_STAT(STAT_ShaderCompiling_HLSLTranslation);
DEFINE_STAT(STAT_ShaderCompiling_DDCLoading);
DEFINE_STAT(STAT_ShaderCompiling_MaterialLoading);
DEFINE_STAT(STAT_ShaderCompiling_MaterialCompiling);

DEFINE_STAT(STAT_ShaderCompiling_NumTotalMaterialShaders);
DEFINE_STAT(STAT_ShaderCompiling_NumSpecialMaterialShaders);
DEFINE_STAT(STAT_ShaderCompiling_NumParticleMaterialShaders);
DEFINE_STAT(STAT_ShaderCompiling_NumSkinnedMaterialShaders);
DEFINE_STAT(STAT_ShaderCompiling_NumLitMaterialShaders);
DEFINE_STAT(STAT_ShaderCompiling_NumUnlitMaterialShaders);
DEFINE_STAT(STAT_ShaderCompiling_NumTransparentMaterialShaders);
DEFINE_STAT(STAT_ShaderCompiling_NumOpaqueMaterialShaders);
DEFINE_STAT(STAT_ShaderCompiling_NumMaskedMaterialShaders);


DEFINE_STAT(STAT_Shaders_NumShadersLoaded);
DEFINE_STAT(STAT_Shaders_NumShaderResourcesLoaded);
DEFINE_STAT(STAT_Shaders_NumShaderMaps);
DEFINE_STAT(STAT_Shaders_RTShaderLoadTime);
DEFINE_STAT(STAT_Shaders_NumShadersUsedForRendering);
DEFINE_STAT(STAT_Shaders_TotalRTShaderInitForRenderingTime);
DEFINE_STAT(STAT_Shaders_FrameRTShaderInitForRenderingTime);
DEFINE_STAT(STAT_Shaders_ShaderMemory);
DEFINE_STAT(STAT_Shaders_ShaderResourceMemory);
DEFINE_STAT(STAT_Shaders_ShaderMapMemory);

/** Protects GShaderFileCache from simultaneous access by multiple threads. */
FCriticalSection FileCacheCriticalSection;

/** The shader file cache, used to minimize shader file reads */
TMap<FString, FString> GShaderFileCache;

/** The shader file hash cache, used to minimize loading and hashing shader files; it includes also hashes for multiple filenames
	by making the key the concatenated list of filenames. */
TMap<FString, FSHAHash> GShaderHashCache;

static TAutoConsoleVariable<int32> CVarForceDebugViewModes(
	TEXT("r.ForceDebugViewModes"),
	0,
	TEXT("0: Setting has no effect.\n")
	TEXT("1: Forces debug view modes to be available, even on cooked builds.")
	TEXT("2: Forces debug view modes to be unavailable, even on editor builds.  Removes many shader permutations for faster shader iteration."),
	ECVF_RenderThreadSafe | ECVF_ReadOnly);

/** Returns true if debug viewmodes are allowed for the current platform. */
bool AllowDebugViewmodes()
{	
	int32 ForceDebugViewValue = CVarForceDebugViewModes.GetValueOnAnyThread();

	// To use debug viewmodes on consoles, r.ForceDebugViewModes must be set to 2 in ConsoleVariables.ini
	// And EngineDebugMaterials must be in the StartupPackages for the target platform.
	bool bForceEnable = ForceDebugViewValue == 1;
	bool bForceDisable = ForceDebugViewValue == 2;

	//bool bEnabled = (!IsRunningCommandlet() && !FPlatformProperties::RequiresCookedData());	
	//bEnabled |= bForceEnable;
	//bEnabled &= !bForceDisable;

	return (!bForceDisable) && (bForceEnable || (!IsRunningCommandlet() && !FPlatformProperties::RequiresCookedData()));
}

bool FShaderParameterMap::FindParameterAllocation(const TCHAR* ParameterName,uint16& OutBufferIndex,uint16& OutBaseIndex,uint16& OutSize) const
{
	const FParameterAllocation* Allocation = ParameterMap.Find(ParameterName);
	if(Allocation)
	{
		OutBufferIndex = Allocation->BufferIndex;
		OutBaseIndex = Allocation->BaseIndex;
		OutSize = Allocation->Size;

		if (Allocation->bBound)
		{
			// Can detect copy-paste errors in binding parameters.  Need to fix all the false positives before enabling.
			//UE_LOG(LogShaders, Warning, TEXT("Parameter %s was bound multiple times. Code error?"), ParameterName);
		}

		Allocation->bBound = true;
		return true;
	}
	else
	{
		return false;
	}
}

bool FShaderParameterMap::ContainsParameterAllocation(const TCHAR* ParameterName) const
{
	return ParameterMap.Find(ParameterName) != NULL;
}

void FShaderParameterMap::AddParameterAllocation(const TCHAR* ParameterName,uint16 BufferIndex,uint16 BaseIndex,uint16 Size)
{
	FParameterAllocation Allocation;
	Allocation.BufferIndex = BufferIndex;
	Allocation.BaseIndex = BaseIndex;
	Allocation.Size = Size;
	ParameterMap.Add(ParameterName,Allocation);
}

void FShaderParameterMap::RemoveParameterAllocation(const TCHAR* ParameterName)
{
	ParameterMap.Remove(ParameterName);
}

void FShaderCompilerOutput::GenerateOutputHash()
{
	FSHA1 HashState;
	
	const TArray<uint8>& Code = ShaderCode.GetReadAccess();

	// we don't hash the optional attachments as they would prevent sharing (e.g. many material share the save VS)
	uint32 ShaderCodeSize = ShaderCode.GetShaderCodeSize();

	HashState.Update(Code.GetData(), ShaderCodeSize * Code.GetTypeSize());
	ParameterMap.UpdateHash(HashState);

	// NVCHANGE_BEGIN: Add VXGI
#if WITH_GFSDK_VXGI
	HashState.Update(VxgiGSCode.GetData(), VxgiGSCode.Num() * VxgiGSCode.GetTypeSize());
	if (bIsVxgiPS)
	{
		for (int32 Index = 0; Index < ParameterMapForVxgiPSPermutation.Num(); ++Index)
		{
			ParameterMapForVxgiPSPermutation[Index].UpdateHash(HashState);
		}
		for (int32 Index = 0; Index < ShaderResouceTableVxgiPSPermutation.Num(); ++Index)
		{
			const TArray<uint8> &SRT = ShaderResouceTableVxgiPSPermutation[Index];
			HashState.Update(SRT.GetData(), SRT.Num() * SRT.GetTypeSize());
		}
		for (int32 Index = 0; Index < UsesGlobalCBForVxgiPSPermutation.Num(); ++Index)
		{
			const uint8 bUsesGlobalCB = UsesGlobalCBForVxgiPSPermutation[Index];
			HashState.Update(&bUsesGlobalCB, sizeof(bUsesGlobalCB));
		}
	}
#endif
	// NVCHANGE_END: Add VXGI

	HashState.Final();
	HashState.GetHash(&OutputHash.Hash[0]);
}

static void ReportVirtualShaderFilePathError(TArray<FShaderCompilerError>* CompileErrors, FString ErrorString)
{
	if (CompileErrors)
	{
		CompileErrors->Add(FShaderCompilerError(*ErrorString));
	}

	UE_LOG(LogShaders, Error, TEXT("%s"), *ErrorString);
}

bool CheckVirtualShaderFilePath(const FString& VirtualFilePath, TArray<FShaderCompilerError>* CompileErrors /*= nullptr*/)
{
	bool bSuccess = true;

	if (!VirtualFilePath.StartsWith(TEXT("/")))
	{
		FString Error = FString::Printf(TEXT("Virtual shader source file name \"%s\" should be absolute from the virtual root directory \"/\"."), *VirtualFilePath);
		ReportVirtualShaderFilePathError(CompileErrors, Error);
		bSuccess = false;
	}

	if (VirtualFilePath.Contains(TEXT("\\")))
	{
		FString Error = FString::Printf(TEXT("Backslashes are not permitted in virtual shader source file name \"%s\""), *VirtualFilePath);
		ReportVirtualShaderFilePathError(CompileErrors, Error);
		bSuccess = false;
	}

	FString Extension = FPaths::GetExtension(VirtualFilePath);
	if ((Extension != TEXT("usf") && Extension != TEXT("ush")) || VirtualFilePath.EndsWith(TEXT(".usf.usf")))
	{
		FString Error = FString::Printf(TEXT("Extension on virtual shader source file name \"%s\" is wrong. Only .usf or .ush allowed."), *VirtualFilePath);
		ReportVirtualShaderFilePathError(CompileErrors, Error);
		bSuccess = false;
	}

	return bSuccess;
}

/**
* Add a new entry to the list of shader source files
* Only unique entries which can be loaded are added as well as their #include files
*
* @param OutVirtualFilePaths - [out] list of shader source files to add to
* @param ShaderFilename - shader file to add
*/
static void AddShaderSourceFileEntry(TArray<FString>& OutVirtualFilePaths, FString VirtualFilePath)
{
	check(CheckVirtualShaderFilePath(VirtualFilePath));
	if (!OutVirtualFilePaths.Contains(VirtualFilePath))
	{
		OutVirtualFilePaths.Add(VirtualFilePath);

		TArray<FString> ShaderIncludes;
		GetShaderIncludes(*VirtualFilePath, *VirtualFilePath,OutVirtualFilePaths);
		for( int32 IncludeIdx=0; IncludeIdx < ShaderIncludes.Num(); IncludeIdx++ )
		{
			OutVirtualFilePaths.AddUnique(ShaderIncludes[IncludeIdx]);
		}
	}
}

/**
* Generates a list of virtual paths of all shader source that engine needs to load.
*
* @param OutVirtualFilePaths - [out] list of shader source files to add to
*/
static void GetAllVirtualShaderSourcePaths(TArray<FString>& OutVirtualFilePaths)
{
	// add all shader source files for hashing
	for( TLinkedList<FVertexFactoryType*>::TIterator FactoryIt(FVertexFactoryType::GetTypeList()); FactoryIt; FactoryIt.Next() )
	{
		FVertexFactoryType* VertexFactoryType = *FactoryIt;
		if( VertexFactoryType )
		{
			FString ShaderFilename(VertexFactoryType->GetShaderFilename());
			AddShaderSourceFileEntry(OutVirtualFilePaths, ShaderFilename);
		}
	}
	for( TLinkedList<FShaderType*>::TIterator ShaderIt(FShaderType::GetTypeList()); ShaderIt; ShaderIt.Next() )
	{
		FShaderType* ShaderType = *ShaderIt;
		if(ShaderType)
		{
			FString ShaderFilename(ShaderType->GetShaderFilename());
			AddShaderSourceFileEntry(OutVirtualFilePaths, ShaderFilename);
		}
	}

	//#todo-rco: No need to loop through Shader Pipeline Types (yet)

	// also always add the MaterialTemplate.usf shader file
	AddShaderSourceFileEntry(OutVirtualFilePaths, FString(TEXT("/Engine/Private/MaterialTemplate.ush")));
	AddShaderSourceFileEntry(OutVirtualFilePaths, FString(TEXT("/Engine/Private/Common.ush")));
	AddShaderSourceFileEntry(OutVirtualFilePaths, FString(TEXT("/Engine/Private/Definitions.usf")));	
}

/**
* Kick off SHA verification for all shader source files
*/
void VerifyShaderSourceFiles()
{
	if (!FPlatformProperties::RequiresCookedData())
	{
		// get the list of shader files that can be used
		TArray<FString> VirtualShaderSourcePaths;
		GetAllVirtualShaderSourcePaths(VirtualShaderSourcePaths);
		FScopedSlowTask SlowTask(VirtualShaderSourcePaths.Num());
		for( int32 ShaderFileIdx=0; ShaderFileIdx < VirtualShaderSourcePaths.Num(); ShaderFileIdx++ )
		{
			SlowTask.EnterProgressFrame(1);
			FString FileContents;
			// load each shader source file. This will cache the shader source data after it has been verified
			LoadShaderSourceFile(*VirtualShaderSourcePaths[ShaderFileIdx], FileContents, nullptr);
		}
	}
}

static void LogShaderSourceDirectoryMappings()
{
	const TMap<FString, FString>& ShaderSourceDirectoryMappings = FPlatformProcess::AllShaderSourceDirectoryMappings();
	for (const auto& Iter : ShaderSourceDirectoryMappings)
	{
		UE_LOG(LogShaders, Log, TEXT("Shader directory mapping %s -> %s"), *Iter.Key, *Iter.Value);
	}
}

static FString GetShaderSourceFilePath(const FString& VirtualFilePath, TArray<FShaderCompilerError>* CompileErrors)
{
	// Make sure the .usf extension is correctly set.
	if (!CheckVirtualShaderFilePath(VirtualFilePath, CompileErrors))
	{
		return FString();
	}

	// We don't cache the output of this function because only used in LoadShaderSourceFile that is cached, or when there
	// is shader compilation errors.
	
	FString RealFilePath;

	// Look if this virtual shader source file match any directory mapping.
	const TMap<FString, FString>& ShaderSourceDirectoryMappings = FPlatformProcess::AllShaderSourceDirectoryMappings();
	FString ParentVirtualDirectoryPath = FPaths::GetPath(VirtualFilePath);
	FString RelativeVirtualDirectoryPath = FPaths::GetCleanFilename(VirtualFilePath);
	while (!ParentVirtualDirectoryPath.IsEmpty())
	{
		if (ShaderSourceDirectoryMappings.Contains(ParentVirtualDirectoryPath))
		{
			RealFilePath = FPaths::Combine(
				*ShaderSourceDirectoryMappings.Find(ParentVirtualDirectoryPath),
				RelativeVirtualDirectoryPath);
			break;
		}

		RelativeVirtualDirectoryPath = FPaths::GetCleanFilename(ParentVirtualDirectoryPath) / RelativeVirtualDirectoryPath;
		ParentVirtualDirectoryPath = FPaths::GetPath(ParentVirtualDirectoryPath);
	}

	// Make sure a directory mapping has matched.
	if (RealFilePath.IsEmpty())
	{
		FString Error = FString::Printf(TEXT("Can't map virtual shader source path \"%s\"."), *VirtualFilePath);
		Error += TEXT("\nDirectory mappings are:");
		for (const auto& Iter : ShaderSourceDirectoryMappings)
		{
			Error += FString::Printf(TEXT("\n  %s -> %s"), *Iter.Key, *Iter.Value);
		}

		ReportVirtualShaderFilePathError(CompileErrors, Error);
	}
	
	return RealFilePath;
}

FString ParseVirtualShaderFilename(const FString& InFilename)
{
	FString ShaderDir = FString(FPlatformProcess::ShaderDir());
	ShaderDir.ReplaceInline(TEXT("\\"), TEXT("/"));
	int32 CharIndex = ShaderDir.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromEnd, ShaderDir.Len() - 1);
	if (CharIndex != INDEX_NONE)
	{
		ShaderDir = ShaderDir.Right(ShaderDir.Len() - CharIndex);
	}

	FString RelativeFilename = InFilename.Replace(TEXT("\\"), TEXT("/"));
	// remove leading "/" because this makes path absolute on Linux (and Mac).
	if (RelativeFilename.Len() > 0 && RelativeFilename[0] == TEXT('/'))
	{
		RelativeFilename = RelativeFilename.Right(RelativeFilename.Len() - 1);
	}
	RelativeFilename = IFileManager::Get().ConvertToRelativePath(*RelativeFilename);
	CharIndex = RelativeFilename.Find(ShaderDir);
	if (CharIndex != INDEX_NONE)
	{
		CharIndex += ShaderDir.Len();
		if (RelativeFilename.GetCharArray()[CharIndex] == TEXT('/'))
		{
			CharIndex++;
		}
		if (RelativeFilename.Contains(TEXT("WorkingDirectory")))
		{
			const int32 NumDirsToSkip = 3;
			int32 NumDirsSkipped = 0;
			int32 NewCharIndex = CharIndex;

			do
			{
				NewCharIndex = RelativeFilename.Find(TEXT("/"), ESearchCase::CaseSensitive, ESearchDir::FromStart, CharIndex);
				CharIndex = (NewCharIndex == INDEX_NONE) ? CharIndex : NewCharIndex + 1;
			}
			while (NewCharIndex != INDEX_NONE && ++NumDirsSkipped < NumDirsToSkip);
		}
		RelativeFilename = RelativeFilename.Mid(CharIndex, RelativeFilename.Len() - CharIndex);
	}

	// add leading "/" to the relative filename because that's what virtual shader path expects
	FString OutputFilename;
	if (RelativeFilename.Len() > 0 && RelativeFilename[0] != TEXT('/'))
	{
		OutputFilename = TEXT("/") + RelativeFilename;
	}
	else
	{
		OutputFilename = RelativeFilename;
	}
	check(CheckVirtualShaderFilePath(OutputFilename));
	return OutputFilename;
}

bool LoadShaderSourceFile(const TCHAR* VirtualFilePath, FString& OutFileContents, TArray<FShaderCompilerError>* OutCompileErrors) // TODO: const FString&
{
	// it's not expected that cooked platforms get here, but if they do, this is the final out
	if (FPlatformProperties::RequiresCookedData())
	{
		return false;
	}

	bool bResult = false;

	STAT(double ShaderFileLoadingTime = 0);
	{
		SCOPE_SECONDS_COUNTER(ShaderFileLoadingTime);

		// Protect GShaderFileCache from simultaneous access by multiple threads
		FScopeLock ScopeLock(&FileCacheCriticalSection);

		FString* CachedFile = GShaderFileCache.Find(VirtualFilePath);

		//if this file has already been loaded and cached, use that
		if (CachedFile)
		{
			OutFileContents = *CachedFile;
			bResult = true;
		}
		else
		{
			FString ShaderFilePath = GetShaderSourceFilePath(VirtualFilePath, OutCompileErrors);

			// verify SHA hash of shader files on load. missing entries trigger an error
			if (!ShaderFilePath.IsEmpty() && FFileHelper::LoadFileToString(OutFileContents, *ShaderFilePath, FFileHelper::EHashOptions::EnableVerify|FFileHelper::EHashOptions::ErrorMissingHash) )
			{
				//update the shader file cache
				GShaderFileCache.Add(VirtualFilePath, *OutFileContents);
				bResult = true;
			}
		}
	}
	INC_FLOAT_STAT_BY(STAT_ShaderCompiling_LoadingShaderFiles,(float)ShaderFileLoadingTime);

	return bResult;
}

void LoadShaderSourceFileChecked(const TCHAR* VirtualFilePath, FString& OutFileContents)
{
	if (!LoadShaderSourceFile(VirtualFilePath, OutFileContents, nullptr))
	{
		UE_LOG(LogShaders, Fatal, TEXT("Couldn't find source file of virtual shader path \'%s\'"), VirtualFilePath);
	}
}

/**
 * Walks InStr until we find either an end-of-line or TargetChar.
 */
const TCHAR* SkipToCharOnCurrentLine(const TCHAR* InStr, TCHAR TargetChar)
{
	const TCHAR* Str = InStr;
	if (Str)
	{
		while (*Str && *Str != TargetChar && *Str != TEXT('\n'))
		{
			++Str;
		}
		if (*Str != TargetChar)
		{
			Str = NULL;
		}
	}
	return Str;
}

/**
 * Recursively populates IncludeFilenames with the unique include filenames found in the shader file named Filename.
 */
static void GetShaderIncludes(const TCHAR* EntryPointVirtualFilePath, const TCHAR* VirtualFilePath, TArray<FString>& IncludeVirtualFilePaths, uint32 DepthLimit, bool AddToIncludeFile)
{
	FString FileContents;
	LoadShaderSourceFile(VirtualFilePath, FileContents, nullptr);

	//avoid an infinite loop with a 0 length string
	if (FileContents.Len() > 0)
	{
		if (AddToIncludeFile)
		{
			IncludeVirtualFilePaths.Add(VirtualFilePath);
		}

		//find the first include directive
		const TCHAR* IncludeBegin = FCString::Strstr(*FileContents, TEXT("#include "));

		uint32 SearchCount = 0;
		const uint32 MaxSearchCount = 200;
		//keep searching for includes as long as we are finding new ones and haven't exceeded the fixed limit
		while (IncludeBegin != NULL && SearchCount < MaxSearchCount && DepthLimit > 0)
		{
			//find the first double quotation after the include directive
			const TCHAR* IncludeFilenameBegin = SkipToCharOnCurrentLine(IncludeBegin, TEXT('\"'));

			if (IncludeFilenameBegin)
			{
				//find the trailing double quotation
				const TCHAR* IncludeFilenameEnd = SkipToCharOnCurrentLine(IncludeFilenameBegin + 1, TEXT('\"'));

				if (IncludeFilenameEnd)
				{
					//construct a string between the double quotations
					FString ExtractedIncludeFilename(FString((int32)(IncludeFilenameEnd - IncludeFilenameBegin - 1), IncludeFilenameBegin + 1));

					// If the include is relative, then it must be relative to the current virtual file path.
					if (!ExtractedIncludeFilename.StartsWith(TEXT("/")))
					{
						ExtractedIncludeFilename = FPaths::GetPath(VirtualFilePath) / ExtractedIncludeFilename;
					}

					//CRC the template, not the filled out version so that this shader's CRC will be independent of which material references it.
					if (ExtractedIncludeFilename == TEXT("/Engine/Generated/Material.ush"))
					{
						ExtractedIncludeFilename = TEXT("/Engine/Private/MaterialTemplate.ush");
					}

					// Ignore uniform buffer, vertex factory and instanced stereo includes
					bool bIgnoreInclude = ExtractedIncludeFilename.StartsWith(TEXT("/Engine/Generated/"));

					// Check virtual.
					bIgnoreInclude |= !CheckVirtualShaderFilePath(ExtractedIncludeFilename);
			
					// Ignore Niagara generated shader code
					bIgnoreInclude |= (ExtractedIncludeFilename == TEXT("/Engine/Private/NiagaraEmitterInstance.usf"));
					bIgnoreInclude |= (ExtractedIncludeFilename == TEXT("/Engine/Private/NiagaraSimulation.usf"));

					// Some headers aren't required to be found (platforms that the user doesn't have access to)
					// @todo: Is there some way to generalize this"
					const bool bIsOptionalInclude = (ExtractedIncludeFilename == TEXT("/Engine/Public/PS4/PS4Common.ush") 
						|| ExtractedIncludeFilename == TEXT("/Engine/Private/PS4/PostProcessHMDMorpheus.usf")
						|| ExtractedIncludeFilename == TEXT("/Engine/Private/PS4/RTWriteMaskProcessing.usf")
						|| ExtractedIncludeFilename == TEXT("/Engine/Private/XboxOne/RTWriteMaskProcessing.usf")
						|| ExtractedIncludeFilename == TEXT("/Engine/Private/PS4/RGBAToYUV420.ush")
						|| ExtractedIncludeFilename == TEXT("/Engine/Public/XboxOne/XboxOneCommon.ush")
						);
					// ignore the header if it's optional and doesn't exist
					if (bIsOptionalInclude)
					{
						// Search in the default engine shader folder and in the same folder as the shader (in case its a project or plugin shader)
						FString StrippedString = ExtractedIncludeFilename.Replace(TEXT("/Engine"), TEXT(""), ESearchCase::CaseSensitive);
						FString EngineShaderFilename = FPaths::Combine(FPlatformProcess::BaseDir(), FPlatformProcess::ShaderDir(), *StrippedString);
						FString LocalShaderFilename = FPaths::Combine(FPaths::GetPath(ExtractedIncludeFilename), *ExtractedIncludeFilename);
						if (!FPaths::FileExists(EngineShaderFilename) && !FPaths::FileExists(LocalShaderFilename))
						{
							bIgnoreInclude = true;
						}
					}

					//vertex factories need to be handled separately
					if (!bIgnoreInclude)
					{
						if (!IncludeVirtualFilePaths.Contains(ExtractedIncludeFilename))
						{
							GetShaderIncludes(EntryPointVirtualFilePath, *ExtractedIncludeFilename, IncludeVirtualFilePaths, DepthLimit - 1, true);
						}
					}
				}
			}

			// Skip to the end of the line.
			IncludeBegin = SkipToCharOnCurrentLine(IncludeBegin, TEXT('\n'));
		
			//find the next include directive
			if (IncludeBegin && *IncludeBegin != 0)
			{
				IncludeBegin = FCString::Strstr(IncludeBegin + 1, TEXT("#include "));
			}
			SearchCount++;
		}

		if (SearchCount == MaxSearchCount || DepthLimit == 0)
		{
			UE_LOG(LogShaders, Warning, TEXT("GetShaderIncludes parsing terminated early to avoid infinite looping!\n Entrypoint \'%s\' CurrentInclude \'%s\' SearchCount %u Depth %u"), 
				EntryPointVirtualFilePath, 
				VirtualFilePath,
				SearchCount, 
				DepthLimit);
		}
	}
}

void GetShaderIncludes(const TCHAR* EntryPointVirtualFilePath, const TCHAR* VirtualFilePath, TArray<FString>& IncludeVirtualFilePaths, uint32 DepthLimit)
{
	GetShaderIncludes(EntryPointVirtualFilePath, VirtualFilePath, IncludeVirtualFilePaths, DepthLimit, false);
}

static void UpdateSingleShaderFilehash(FSHA1& InOutHashState, const TCHAR* VirtualFilePath)
{
	// Get the list of includes this file contains
	TArray<FString> IncludeVirtualFilePaths;
	GetShaderIncludes(VirtualFilePath, VirtualFilePath, IncludeVirtualFilePaths);

	for (int32 IncludeIndex = 0; IncludeIndex < IncludeVirtualFilePaths.Num(); IncludeIndex++)
	{
		// Load the include file and hash it
		FString IncludeFileContents;
		LoadShaderSourceFileChecked(*IncludeVirtualFilePaths[IncludeIndex], IncludeFileContents);
		InOutHashState.UpdateWithString(*IncludeFileContents, IncludeFileContents.Len());
	}

	// Load the source file and hash it
	FString FileContents;
	LoadShaderSourceFileChecked(VirtualFilePath, FileContents);
	InOutHashState.UpdateWithString(*FileContents, FileContents.Len());
}

/**
 * Calculates a Hash for the given filename and its includes if it does not already exist in the Hash cache.
 * @param Filename - shader file to Hash
 */
const FSHAHash& GetShaderFileHash(const TCHAR* VirtualFilePath)
{
	// Make sure we are only accessing GShaderHashCache from one thread
	//check(IsInGameThread() || IsAsyncLoading());
	STAT(double HashTime = 0);
	{
		SCOPE_SECONDS_COUNTER(HashTime);

		FSHAHash* CachedHash = GShaderHashCache.Find(VirtualFilePath);

		// If a hash for this filename has been cached, use that
		if (CachedHash)
		{
			return *CachedHash;
		}

		FSHA1 HashState;
		UpdateSingleShaderFilehash(HashState, VirtualFilePath);
		HashState.Final();

		// Update the hash cache
		FSHAHash& NewHash = GShaderHashCache.Add(VirtualFilePath, FSHAHash());
		HashState.GetHash(&NewHash.Hash[0]);
		INC_FLOAT_STAT_BY(STAT_ShaderCompiling_HashingShaderFiles, (float)HashTime);
		return NewHash;
	}
}

/**
 * Calculates a Hash for the given filename and its includes if it does not already exist in the Hash cache.
 * @param Filename - shader file to Hash
 */
const FSHAHash& GetShaderFilesHash(const TArray<FString>& VirtualFilePaths)
{
	// Make sure we are only accessing GShaderHashCache from one thread
	//check(IsInGameThread() || IsAsyncLoading());
	STAT(double HashTime = 0);
	{
		SCOPE_SECONDS_COUNTER(HashTime);

		FString Key;
		for (const FString& Filename : VirtualFilePaths)
		{
			Key += Filename;
		}

		FSHAHash* CachedHash = GShaderHashCache.Find(Key);

		// If a hash for this filename has been cached, use that
		if (CachedHash)
		{
			return *CachedHash;
		}

		FSHA1 HashState;
		for (const FString& VirtualFilePath : VirtualFilePaths)
		{
			UpdateSingleShaderFilehash(HashState, *VirtualFilePath);
		}
		HashState.Final();

		// Update the hash cache
		FSHAHash& NewHash = GShaderHashCache.Add(Key, FSHAHash());
		HashState.GetHash(&NewHash.Hash[0]);

		INC_FLOAT_STAT_BY(STAT_ShaderCompiling_HashingShaderFiles, (float)HashTime);
		return NewHash;
	}
}

void BuildShaderFileToUniformBufferMap(TMap<FString, TArray<const TCHAR*> >& ShaderFileToUniformBufferVariables)
{
	if (!FPlatformProperties::RequiresCookedData())
	{
		TArray<FString> ShaderSourceFiles;
		GetAllVirtualShaderSourcePaths(ShaderSourceFiles);

		FScopedSlowTask SlowTask(ShaderSourceFiles.Num());

		// Cache UB access strings, make it case sensitive for faster search
		struct FShaderVariable
		{
			FShaderVariable(const TCHAR* ShaderVariable) :
				OriginalShaderVariable(ShaderVariable), SearchKey(FString(ShaderVariable).ToUpper() + TEXT("."))
			{
			}
			const TCHAR* OriginalShaderVariable;
			FString SearchKey;
		};
		// Cache each UB
		TArray<FShaderVariable> SearchKeys;
		for (TLinkedList<FUniformBufferStruct*>::TIterator StructIt(FUniformBufferStruct::GetStructList()); StructIt; StructIt.Next())
		{
			SearchKeys.Add(FShaderVariable(StructIt->GetShaderVariableName()));
		}

		// Find for each shader file which UBs it needs
		for (int32 FileIndex = 0; FileIndex < ShaderSourceFiles.Num(); FileIndex++)
		{
			SlowTask.EnterProgressFrame(1);

 			FString ShaderFileContents;
			LoadShaderSourceFileChecked(*ShaderSourceFiles[FileIndex], ShaderFileContents);

			// To allow case sensitive search which is way faster on some platforms (no need to look up locale, etc)
			ShaderFileContents.ToUpperInline();

			TArray<const TCHAR*>& ReferencedUniformBuffers = ShaderFileToUniformBufferVariables.FindOrAdd(ShaderSourceFiles[FileIndex]);

			for (int32 SearchKeyIndex = 0; SearchKeyIndex < SearchKeys.Num(); ++SearchKeyIndex)
			{
				// Searching for the uniform buffer shader variable being accessed with '.'
				if (ShaderFileContents.Contains(SearchKeys[SearchKeyIndex].SearchKey, ESearchCase::CaseSensitive))
				{
					ReferencedUniformBuffers.AddUnique(SearchKeys[SearchKeyIndex].OriginalShaderVariable);
				}
			}
		}
	}
}

void InitializeShaderTypes()
{
	UE_LOG(LogShaders, Log, TEXT("InitializeShaderTypes() begin"));

	LogShaderSourceDirectoryMappings();

	TMap<FString, TArray<const TCHAR*> > ShaderFileToUniformBufferVariables;
	BuildShaderFileToUniformBufferMap(ShaderFileToUniformBufferVariables);

	FShaderType::Initialize(ShaderFileToUniformBufferVariables);
	FVertexFactoryType::Initialize(ShaderFileToUniformBufferVariables);

	FShaderPipelineType::Initialize();

	UE_LOG(LogShaders, Log, TEXT("InitializeShaderTypes() end"));
}

void UninitializeShaderTypes()
{
	UE_LOG(LogShaders, Log, TEXT("UninitializeShaderTypes() begin"));

	FShaderPipelineType::Uninitialize();

	FShaderType::Uninitialize();
	FVertexFactoryType::Uninitialize();

	UE_LOG(LogShaders, Log, TEXT("UninitializeShaderTypes() end"));
}

/**
 * Flushes the shader file and CRC cache, and regenerates the binary shader files if necessary.
 * Allows shader source files to be re-read properly even if they've been modified since startup.
 */
void FlushShaderFileCache()
{
	UE_LOG(LogShaders, Log, TEXT("FlushShaderFileCache() begin"));

	GShaderHashCache.Empty();
	GShaderFileCache.Empty();

	if (!FPlatformProperties::RequiresCookedData())
	{
		LogShaderSourceDirectoryMappings();

		TMap<FString, TArray<const TCHAR*> > ShaderFileToUniformBufferVariables;
		BuildShaderFileToUniformBufferMap(ShaderFileToUniformBufferVariables);

		for (TLinkedList<FShaderPipelineType*>::TConstIterator It(FShaderPipelineType::GetTypeList()); It; It.Next())
		{
			const auto& Stages = It->GetStages();
			for (const FShaderType* ShaderType : Stages)
			{
				((FShaderType*)ShaderType)->FlushShaderFileCache(ShaderFileToUniformBufferVariables);
			}
		}

		for(TLinkedList<FShaderType*>::TIterator It(FShaderType::GetTypeList()); It; It.Next())
		{
			It->FlushShaderFileCache(ShaderFileToUniformBufferVariables);
		}

		for(TLinkedList<FVertexFactoryType*>::TIterator It(FVertexFactoryType::GetTypeList()); It; It.Next())
		{
			It->FlushShaderFileCache(ShaderFileToUniformBufferVariables);
		}
	}

	UE_LOG(LogShaders, Log, TEXT("FlushShaderFileCache() end"));
}

void GenerateReferencedUniformBuffers(
	const TCHAR* SourceFilename, 
	const TCHAR* ShaderTypeName, 
	const TMap<FString, TArray<const TCHAR*> >& ShaderFileToUniformBufferVariables,
	TMap<const TCHAR*,FCachedUniformBufferDeclaration>& UniformBufferEntries)
{
	TArray<FString> FilesToSearch;
	GetShaderIncludes(SourceFilename, SourceFilename, FilesToSearch);
	FilesToSearch.Add(SourceFilename);

	for (int32 FileIndex = 0; FileIndex < FilesToSearch.Num(); FileIndex++)
	{
		const TArray<const TCHAR*>& FoundUniformBufferVariables = ShaderFileToUniformBufferVariables.FindChecked(FilesToSearch[FileIndex]);

		for (int32 VariableIndex = 0; VariableIndex < FoundUniformBufferVariables.Num(); VariableIndex++)
		{
			UniformBufferEntries.Add(FoundUniformBufferVariables[VariableIndex], FCachedUniformBufferDeclaration());
		}
	}
}

void SerializeUniformBufferInfo(FShaderSaveArchive& Ar, const TMap<const TCHAR*,FCachedUniformBufferDeclaration>& UniformBufferEntries)
{
	for (TMap<const TCHAR*,FCachedUniformBufferDeclaration>::TConstIterator It(UniformBufferEntries); It; ++It)
	{
		for (TLinkedList<FUniformBufferStruct*>::TIterator StructIt(FUniformBufferStruct::GetStructList()); StructIt; StructIt.Next())
		{
			if (It.Key() == StructIt->GetShaderVariableName())
			{
				// Serialize information about the struct layout so we can detect when it changes
				const FUniformBufferStruct& Struct = **StructIt;
				const TArray<FUniformBufferStruct::FMember>& Members = Struct.GetMembers();

				int32 NumMembers = Members.Num();
				// Serializing with NULL so that FShaderSaveArchive will record the length without causing an actual data serialization
				Ar.Serialize(NULL, NumMembers);

				for (int32 MemberIndex = 0; MemberIndex < Members.Num(); MemberIndex++)
				{
					const FUniformBufferStruct::FMember& Member = Members[MemberIndex];

					// Note: Only comparing number of floats used by each member and type, so this can be tricked (eg. swapping two equal size and type members)
					int32 MemberSize = Member.GetNumColumns() * Member.GetNumRows();
					Ar.Serialize(NULL, MemberSize);
					int32 MemberType = (int32)Member.GetBaseType();
					Ar.Serialize(NULL, MemberType);
				}
				break;
			}
		}
	}
}

FString FShaderCompilerError::GetShaderSourceFilePath() const
{
	if (IFileManager::Get().FileExists(*ErrorVirtualFilePath))
	{
		return ErrorVirtualFilePath;
	}
	else
	{
		return ::GetShaderSourceFilePath(ErrorVirtualFilePath, nullptr);
	}
}
