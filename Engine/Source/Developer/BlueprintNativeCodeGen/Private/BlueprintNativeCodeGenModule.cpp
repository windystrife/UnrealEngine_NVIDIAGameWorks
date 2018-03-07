// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlueprintNativeCodeGenModule.h"
#include "Engine/Blueprint.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/UObjectHash.h"
#include "UObject/Package.h"
#include "Templates/Greater.h"
#include "Components/ActorComponent.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "AssetData.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/UserDefinedStruct.h"
#include "Settings/ProjectPackagingSettings.h"
#include "PlatformInfo.h"
#include "AssetRegistryModule.h"
#include "BlueprintNativeCodeGenManifest.h"
#include "Blueprint/BlueprintSupport.h"
#include "BlueprintCompilerCppBackendInterface.h"
#include "IBlueprintCompilerCppBackendModule.h"
#include "BlueprintNativeCodeGenUtils.h"
#include "Engine/SCS_Node.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Engine/InheritableComponentHandler.h"
#include "Animation/AnimBlueprint.h"
#include "Interfaces/ITargetPlatform.h"

/*******************************************************************************
 * FBlueprintNativeCodeGenModule
 ******************************************************************************/
 
class FBlueprintNativeCodeGenModule : public IBlueprintNativeCodeGenModule
									, public IBlueprintNativeCodeGenCore
{
public:
	FBlueprintNativeCodeGenModule()
	{
	}

	//~ Begin IModuleInterface interface
	virtual void ShutdownModule();
	//~ End IModuleInterface interface

	//~ Begin IBlueprintNativeCodeGenModule interface
	virtual void Convert(UPackage* Package, ESavePackageResult ReplacementType, const FName PlatformName) override;
	virtual void SaveManifest() override;
	virtual void MergeManifest(int32 ManifestIdentifier) override;
	virtual void FinalizeManifest() override;
	virtual void GenerateStubs() override;
	virtual void GenerateFullyConvertedClasses() override;
	void MarkUnconvertedBlueprintAsNecessary(TSoftObjectPtr<UBlueprint> BPPtr, const FCompilerNativizationOptions& NativizationOptions) override;
	virtual const TMultiMap<FName, TSoftClassPtr<UObject>>& GetFunctionsBoundToADelegate() override;

	FFileHelper::EEncodingOptions ForcedEncoding() const
	{
		return FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM;
	}
	virtual const FCompilerNativizationOptions& GetNativizationOptionsForPlatform(const ITargetPlatform* Platform) const override;
	virtual void FillPlatformNativizationDetails(const ITargetPlatform* Platform, FPlatformNativizationDetails& OutDetails) override;
protected:
	virtual void Initialize(const FNativeCodeGenInitData& InitData) override;
	virtual void InitializeForRerunDebugOnly(const TArray<FPlatformNativizationDetails>& CodegenTargets) override;
	//~ End IBlueprintNativeCodeGenModule interface

	//~ Begin FScriptCookReplacmentCoordinator interface
	virtual EReplacementResult IsTargetedForReplacement(const UPackage* Package, const FCompilerNativizationOptions& NativizationOptions) const override;
	virtual EReplacementResult IsTargetedForReplacement(const UObject* Object, const FCompilerNativizationOptions& NativizationOptions) const override;
	virtual UClass* FindReplacedClassForObject(const UObject* Object, const FCompilerNativizationOptions& NativizationOptions) const override;
	virtual UObject* FindReplacedNameAndOuter(UObject* Object, FName& OutName, const FCompilerNativizationOptions& NativizationOptions) const override;
	//~ End FScriptCookReplacmentCoordinator interface
private:
	void ReadConfig();
	void FillTargetedForReplacementQuery();
	void FillIsFunctionUsedInADelegate();
	FBlueprintNativeCodeGenManifest& GetManifest(const FName PlatformName);
	void GenerateSingleStub(UBlueprint* BP, const FName PlatformName);
	void CollectBoundFunctions(UBlueprint* BP);
	void GenerateSingleAsset(UField* ForConversion, const FName PlatformName, TSharedPtr<FNativizationSummary> NativizationSummary = TSharedPtr<FNativizationSummary>());

	struct FStatePerPlatform
	{
		// A stub-wrapper must be generated only if the BP is really accessed/required by some other generated code.
		TSet<TSoftObjectPtr<UBlueprint>> StubsRequiredByGeneratedCode;

		TSet<TSoftObjectPtr<UStruct>> UDSAssetsToGenerate;
		TSet<TSoftObjectPtr<UBlueprint>> BPAssetsToGenerate;

		// Cached values from IsTargetedForReplacement
		mutable TMap<FSoftObjectPath, EReplacementResult> CachedIsTargetedForReplacement;
	};

	TMap< FName, FStatePerPlatform > StatesPerPlatform;

	TMap< FName, TUniquePtr<FBlueprintNativeCodeGenManifest> > Manifests;

	// Children of these classes won't be nativized
	TArray<FString> ExcludedAssetTypes;
	// Eg: +ExcludedBlueprintTypes=/Script/Engine.AnimBlueprint
	TArray<TSoftClassPtr<UBlueprint>> ExcludedBlueprintTypes;
	// Individually excluded assets
	TSet<FSoftObjectPath> ExcludedAssets;
	// Excluded folders. It excludes only BPGCs, enums and structures are still converted.
	TArray<FString> ExcludedFolderPaths;

	TArray<FName> TargetPlatformNames;

	TMultiMap<FName, TSoftClassPtr<UObject>> FunctionsBoundToADelegate; // is a function could be bound to a delegate, then it must have UFUNCTION macro. So we cannot optimize it.
};

const FCompilerNativizationOptions& FBlueprintNativeCodeGenModule::GetNativizationOptionsForPlatform(const ITargetPlatform* Platform) const
{
	const FName PlatformName = ensure(Platform) ? Platform->GetPlatformInfo().PlatformInfoName : NAME_None;

	const TUniquePtr<FBlueprintNativeCodeGenManifest>* Result = Manifests.Find(PlatformName);
	if (ensure(Result && Result->IsValid()))
	{
		const FBlueprintNativeCodeGenManifest& Manifest = **Result;
		return Manifest.GetCompilerNativizationOptions();
	}
	UE_LOG(LogBlueprintCodeGen, Error, TEXT("Cannot find manifest for platform: %s"), *PlatformName.ToString());
	static const FCompilerNativizationOptions FallbackNativizationOptions{};
	return FallbackNativizationOptions;
}

void FBlueprintNativeCodeGenModule::ReadConfig()
{
	GConfig->GetArray(TEXT("BlueprintNativizationSettings"), TEXT("ExcludedAssetTypes"), ExcludedAssetTypes, GEditorIni);

	{
		TArray<FString> ExcludedBlueprintTypesPath;
		GConfig->GetArray(TEXT("BlueprintNativizationSettings"), TEXT("ExcludedBlueprintTypes"), ExcludedBlueprintTypesPath, GEditorIni);
		for (FString& Path : ExcludedBlueprintTypesPath)
		{
			TSoftClassPtr<UBlueprint> ClassPtr;
			ClassPtr = FSoftObjectPath(Path);
			ClassPtr.LoadSynchronous();
			ExcludedBlueprintTypes.Add(ClassPtr);
		}
	}

	TArray<FString> ExcludedAssetPaths;
	GConfig->GetArray(TEXT("BlueprintNativizationSettings"), TEXT("ExcludedAssets"), ExcludedAssetPaths, GEditorIni);
	for (FString& Path : ExcludedAssetPaths)
	{
		ExcludedAssets.Add(FSoftObjectPath(Path));
	}

	GConfig->GetArray(TEXT("BlueprintNativizationSettings"), TEXT("ExcludedFolderPaths"), ExcludedFolderPaths, GEditorIni);
}

void FBlueprintNativeCodeGenModule::MarkUnconvertedBlueprintAsNecessary(TSoftObjectPtr<UBlueprint> BPPtr, const FCompilerNativizationOptions& NativizationOptions)
{
	FStatePerPlatform* StateForCurrentPlatform = StatesPerPlatform.Find(NativizationOptions.PlatformName);
	if (ensure(StateForCurrentPlatform))
	{
		StateForCurrentPlatform->StubsRequiredByGeneratedCode.Add(BPPtr);
	}
}

void FBlueprintNativeCodeGenModule::FillTargetedForReplacementQuery()
{
	IBlueprintCompilerCppBackendModule& BackEndModule = (IBlueprintCompilerCppBackendModule&)IBlueprintCompilerCppBackendModule::Get();
	auto& ConversionQueryDelegate = BackEndModule.OnIsTargetedForConversionQuery();
	auto ShouldConvert = [](const UObject* AssetObj, const FCompilerNativizationOptions& NativizationOptions)
	{
		if (ensure(IBlueprintNativeCodeGenCore::Get()))
		{
			EReplacementResult ReplacmentResult = IBlueprintNativeCodeGenCore::Get()->IsTargetedForReplacement(AssetObj, NativizationOptions);
			return ReplacmentResult == EReplacementResult::ReplaceCompletely;
		}
		return false;
	};
	ConversionQueryDelegate.BindStatic(ShouldConvert);

	auto LocalMarkUnconvertedBlueprintAsNecessary = [](TSoftObjectPtr<UBlueprint> BPPtr, const FCompilerNativizationOptions& NativizationOptions)
	{
		IBlueprintNativeCodeGenModule::Get().MarkUnconvertedBlueprintAsNecessary(BPPtr, NativizationOptions);
	};
	BackEndModule.OnIncludingUnconvertedBP().BindStatic(LocalMarkUnconvertedBlueprintAsNecessary);
}

namespace 
{
	void GetFieldFormPackage(const UPackage* Package, UStruct*& OutStruct, UEnum*& OutEnum, EObjectFlags ExcludedFlags = RF_Transient)
	{
		TArray<UObject*> Objects;
		GetObjectsWithOuter(Package, Objects, false);
		for (UObject* Entry : Objects)
		{
			if (Entry->HasAnyFlags(ExcludedFlags))
			{
				continue;
			}

			if (FBlueprintSupport::IsDeferredDependencyPlaceholder(Entry))
			{
				continue;
			}

			// Not a skeleton class
			if (UClass* AsClass = Cast<UClass>(Entry))
			{
				if (UBlueprint* GeneratingBP = Cast<UBlueprint>(AsClass->ClassGeneratedBy))
				{
					if (AsClass != GeneratingBP->GeneratedClass)
					{
						continue;
					}
				}
			}

			OutStruct = Cast<UStruct>(Entry);
			if (OutStruct)
			{
				break;
			}

			OutEnum = Cast<UEnum>(Entry);
			if (OutEnum)
			{
				break;
			}
		}
	}
}

void FBlueprintNativeCodeGenModule::CollectBoundFunctions(UBlueprint* BP)
{
	TArray<UFunction*> Functions = IBlueprintCompilerCppBackendModule::CollectBoundFunctions(BP);
	for (UFunction* Func : Functions)
	{
		if (Func)
		{
			FunctionsBoundToADelegate.AddUnique(Func->GetFName(), Func->GetOwnerClass());
		}
	}
}

const TMultiMap<FName, TSoftClassPtr<UObject>>& FBlueprintNativeCodeGenModule::GetFunctionsBoundToADelegate()
{
	return FunctionsBoundToADelegate;
}

void FBlueprintNativeCodeGenModule::FillIsFunctionUsedInADelegate()
{
	IBlueprintCompilerCppBackendModule& BackEndModule = (IBlueprintCompilerCppBackendModule&)IBlueprintCompilerCppBackendModule::Get();

	auto IsFunctionUsed = [](const UFunction* InFunction) -> bool
	{
		auto& TargetFunctionsBoundToADelegate = IBlueprintNativeCodeGenModule::Get().GetFunctionsBoundToADelegate();
		return InFunction && (nullptr != TargetFunctionsBoundToADelegate.FindPair(InFunction->GetFName(), InFunction->GetOwnerClass()));
	};

	BackEndModule.GetIsFunctionUsedInADelegateCallback().BindStatic(IsFunctionUsed);
}

void FBlueprintNativeCodeGenModule::Initialize(const FNativeCodeGenInitData& InitData)
{
	StatesPerPlatform.Reset();
	for (const FPlatformNativizationDetails& Platform : InitData.CodegenTargets)
	{
		StatesPerPlatform.Add(Platform.PlatformName);
	}

	ReadConfig();

	IBlueprintNativeCodeGenCore::Register(this);

	// Each platform will need a manifest, because each platform could cook different assets:
	for (const FPlatformNativizationDetails& Platform : InitData.CodegenTargets)
	{
		const FString TargetPath = FBlueprintNativeCodeGenPaths::GetDefaultPluginPath(Platform.PlatformName);
		const FBlueprintNativeCodeGenManifest& Manifest = *Manifests.Add(Platform.PlatformName, TUniquePtr<FBlueprintNativeCodeGenManifest>(new FBlueprintNativeCodeGenManifest(TargetPath, Platform.CompilerNativizationOptions, InitData.ManifestIdentifier)));

		TargetPlatformNames.Add(Platform.PlatformName);

		// Clear source code folder
		const FString SourceCodeDir = Manifest.GetTargetPaths().PluginRootDir();
		UE_LOG(LogBlueprintCodeGen, Log, TEXT("Clear nativized source code directory: %s"), *SourceCodeDir);
		IFileManager::Get().DeleteDirectory(*SourceCodeDir, false, true);
	}

	FillTargetedForReplacementQuery();

	FillIsFunctionUsedInADelegate();
}

void FBlueprintNativeCodeGenModule::InitializeForRerunDebugOnly(const TArray<FPlatformNativizationDetails>& CodegenTargets)
{
	StatesPerPlatform.Reset();
	for (const FPlatformNativizationDetails& Platform : CodegenTargets)
	{
		StatesPerPlatform.Add(Platform.PlatformName);
	}
	ReadConfig();
	IBlueprintNativeCodeGenCore::Register(this);
	FillTargetedForReplacementQuery();
	FillIsFunctionUsedInADelegate();

	for (const FPlatformNativizationDetails& Platform : CodegenTargets)
	{
		// load the old manifest:
		FString OutputPath = FBlueprintNativeCodeGenPaths::GetDefaultManifestFilePath(Platform.PlatformName);
		Manifests.Add(Platform.PlatformName, TUniquePtr<FBlueprintNativeCodeGenManifest>(new FBlueprintNativeCodeGenManifest(FPaths::ConvertRelativePathToFull(OutputPath))));
		//FBlueprintNativeCodeGenManifest OldManifest(FPaths::ConvertRelativePathToFull(OutputPath));
		// reconvert every assets listed in the manifest:
		for (const auto& ConversionTarget : GetManifest(Platform.PlatformName).GetConversionRecord())
		{
			// load the package:
			UPackage* Package = LoadPackage(nullptr, *ConversionTarget.Value.TargetObjPath, LOAD_None);

			if (!Package)
			{
				UE_LOG(LogBlueprintCodeGen, Error, TEXT("Unable to load the package: %s"), *ConversionTarget.Value.TargetObjPath);
				continue;
			}

			// reconvert it
			Convert(Package, ESavePackageResult::ReplaceCompletely, Platform.PlatformName);
		}

		// reconvert every unconverted dependency listed in the manifest:
		for (const auto& ConversionTarget : GetManifest(Platform.PlatformName).GetUnconvertedDependencies())
		{
			// load the package:
			UPackage* Package = LoadPackage(nullptr, *ConversionTarget.Key.GetPlainNameString(), LOAD_None);

			UStruct* Struct = nullptr;
			UEnum* Enum = nullptr;
			GetFieldFormPackage(Package, Struct, Enum);
			UBlueprint* BP = Cast<UBlueprint>(CastChecked<UClass>(Struct)->ClassGeneratedBy);
			if (ensure(BP))
			{
				CollectBoundFunctions(BP);
				GenerateSingleStub(BP, Platform.PlatformName);
			}
		}
		FStatePerPlatform* State = StatesPerPlatform.Find(Platform.PlatformName);
		check(State);

		for (TSoftObjectPtr<UStruct>& UDSPtr : State->UDSAssetsToGenerate)
		{
			UStruct* UDS = UDSPtr.LoadSynchronous();
			if (ensure(UDS))
			{
				GenerateSingleAsset(UDS, Platform.PlatformName);
			}
		}

		for (TSoftObjectPtr<UBlueprint>& BPPtr : State->BPAssetsToGenerate)
		{
			UBlueprint* BP = BPPtr.LoadSynchronous();
			if (ensure(BP))
			{
				GenerateSingleAsset(BP->GeneratedClass, Platform.PlatformName);
			}
		}
	}
}

void FBlueprintNativeCodeGenModule::ShutdownModule()
{
	// Clear the current coordinator reference.
	IBlueprintNativeCodeGenCore::Register(nullptr);

	// Reset compiler module delegate function bindings.
	IBlueprintCompilerCppBackendModule& BackEndModule = (IBlueprintCompilerCppBackendModule&)IBlueprintCompilerCppBackendModule::Get();
	BackEndModule.GetIsFunctionUsedInADelegateCallback().Unbind();
	BackEndModule.OnIsTargetedForConversionQuery().Unbind();
	BackEndModule.OnIncludingUnconvertedBP().Unbind();
}

void FBlueprintNativeCodeGenModule::GenerateFullyConvertedClasses()
{
	TSharedPtr<FNativizationSummary> NativizationSummary(new FNativizationSummary());
	{
		IBlueprintCompilerCppBackendModule& CodeGenBackend = (IBlueprintCompilerCppBackendModule&)IBlueprintCompilerCppBackendModule::Get();
		CodeGenBackend.NativizationSummary() = NativizationSummary;
	}

	for (const FName& PlatformName : TargetPlatformNames)
	{
		FStatePerPlatform* State = StatesPerPlatform.Find(PlatformName);
		check(State);

		for (TSoftObjectPtr<UStruct>& UDSPtr : State->UDSAssetsToGenerate)
		{
			UStruct* UDS = UDSPtr.LoadSynchronous();
			if (ensure(UDS))
			{
				GenerateSingleAsset(UDS, PlatformName, NativizationSummary);
			}
		}

		for (TSoftObjectPtr<UBlueprint>& BPPtr : State->BPAssetsToGenerate)
		{
			UBlueprint* BP = BPPtr.LoadSynchronous();
			if (ensure(BP))
			{
				GenerateSingleAsset(BP->GeneratedClass, PlatformName, NativizationSummary);
			}
		}
	}
	
	if (NativizationSummary->InaccessiblePropertyStat.Num())
	{
		UE_LOG(LogBlueprintCodeGen, Display, TEXT("Nativization Summary - Inaccessible Properties:"));
		NativizationSummary->InaccessiblePropertyStat.ValueSort(TGreater<int32>());
		for (auto& Iter : NativizationSummary->InaccessiblePropertyStat)
		{
			UE_LOG(LogBlueprintCodeGen, Display, TEXT("\t %s \t - %d"), *Iter.Key.ToString(), Iter.Value);
		}
	}
	{
		UE_LOG(LogBlueprintCodeGen, Display, TEXT("Nativization Summary - AnimBP:"));
		UE_LOG(LogBlueprintCodeGen, Display, TEXT("Name, Children, Non-empty Functions (Empty Functions), Variables, FunctionUsage, VariableUsage"));
		for (auto& Iter : NativizationSummary->AnimBlueprintStat)
		{
			UE_LOG(LogBlueprintCodeGen, Display
				, TEXT("%s, %d, %d (%d), %d, %d, %d")
				, *Iter.Key.ToString()
				, Iter.Value.Children
				, Iter.Value.Functions - Iter.Value.ReducibleFunctions
				, Iter.Value.ReducibleFunctions
				, Iter.Value.Variables
				, Iter.Value.FunctionUsage
				, Iter.Value.VariableUsage);
		}
	}
	UE_LOG(LogBlueprintCodeGen, Display, TEXT("Nativization Summary - Shared Variables From Graph: %d"), NativizationSummary->MemberVariablesFromGraph);
}

FBlueprintNativeCodeGenManifest& FBlueprintNativeCodeGenModule::GetManifest(const FName PlatformName)
{
	TUniquePtr<FBlueprintNativeCodeGenManifest>* Result = Manifests.Find(PlatformName);
	check(Result->IsValid());
	return **Result;
}

void FBlueprintNativeCodeGenModule::GenerateSingleStub(UBlueprint* BP, const FName PlatformName)
{
	if (!ensure(BP))
	{
		return;
	}

	UClass* Class = BP->GeneratedClass;
	if (!ensure(Class))
	{
		return;
	}

	// no PCHFilename should be necessary
	const IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	FAssetData AssetInfo = Registry.GetAssetByObjectPath(*Class->GetPathName());
	FString FileContents;
	TUniquePtr<IBlueprintCompilerCppBackend> Backend_CPP(IBlueprintCompilerCppBackendModuleInterface::Get().Create());
	// Apparently we can only generate wrappers for classes, so any logic that results in non classes requesting
	// wrappers will fail here:

	FileContents = Backend_CPP->GenerateWrapperForClass(Class, GetManifest(PlatformName).GetCompilerNativizationOptions());

	if (!FileContents.IsEmpty())
	{
		FFileHelper::SaveStringToFile(FileContents
			, *(GetManifest(PlatformName).CreateUnconvertedDependencyRecord(AssetInfo.PackageName, AssetInfo).GeneratedWrapperPath)
			, ForcedEncoding());
	}
	// The stub we generate still may have dependencies on other modules, so make sure the module dependencies are 
	// still recorded so that the .build.cs is generated correctly. Without this you'll get include related errors 
	// (or possibly linker errors) in stub headers:
	GetManifest(PlatformName).GatherModuleDependencies(BP->GetOutermost());
}

void FBlueprintNativeCodeGenModule::GenerateSingleAsset(UField* ForConversion, const FName PlatformName, TSharedPtr<FNativizationSummary> NativizationSummary)
{
	IBlueprintCompilerCppBackendModule& BackEndModule = (IBlueprintCompilerCppBackendModule&)IBlueprintCompilerCppBackendModule::Get();
	auto& BackendPCHQuery = BackEndModule.OnPCHFilenameQuery();
	const IAssetRegistry& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>("AssetRegistry").Get();
	FAssetData AssetInfo = Registry.GetAssetByObjectPath(*ForConversion->GetPathName());

	FBlueprintNativeCodeGenPaths TargetPaths = GetManifest(PlatformName).GetTargetPaths();
	BackendPCHQuery.BindLambda([TargetPaths]()->FString
	{
		return TargetPaths.RuntimePCHFilename();
	});

	FConvertedAssetRecord& ConversionRecord = GetManifest(PlatformName).CreateConversionRecord(*ForConversion->GetPathName(), AssetInfo);
	TSharedPtr<FString> HeaderSource(new FString());
	TSharedPtr<FString> CppSource(new FString());

	FBlueprintNativeCodeGenUtils::GenerateCppCode(ForConversion, HeaderSource, CppSource, NativizationSummary, GetManifest(PlatformName).GetCompilerNativizationOptions());
	bool bSuccess = !HeaderSource->IsEmpty() || !CppSource->IsEmpty();
	// Run the cpp first, because we cue off of the presence of a header for a valid conversion record (see
	// FConvertedAssetRecord::IsValid)
	if (!CppSource->IsEmpty())
	{
		if (!FFileHelper::SaveStringToFile(*CppSource, *ConversionRecord.GeneratedCppPath, ForcedEncoding()))
		{
			bSuccess &= false;
			ConversionRecord.GeneratedCppPath.Empty();
		}
		CppSource->Empty(CppSource->Len());
	}
	else
	{
		ConversionRecord.GeneratedCppPath.Empty();
	}

	if (bSuccess && !HeaderSource->IsEmpty())
	{
		if (!FFileHelper::SaveStringToFile(*HeaderSource, *ConversionRecord.GeneratedHeaderPath, ForcedEncoding()))
		{
			bSuccess &= false;
			ConversionRecord.GeneratedHeaderPath.Empty();
		}
		HeaderSource->Empty(HeaderSource->Len());
	}
	else
	{
		ConversionRecord.GeneratedHeaderPath.Empty();
	}

	if (bSuccess)
	{
		GetManifest(PlatformName).GatherModuleDependencies(ForConversion->GetOutermost());
	}
	else
	{
		UE_LOG(LogBlueprintCodeGen, Error, TEXT("FBlueprintNativeCodeGenModule::GenerateSingleAsset error: %s"), *GetPathNameSafe(ForConversion));
	}

	BackendPCHQuery.Unbind();
}

void FBlueprintNativeCodeGenModule::GenerateStubs()
{
	for (FName& PlatformName : TargetPlatformNames)
	{
		FStatePerPlatform* StateForCurrentPlatform = StatesPerPlatform.Find(PlatformName);
		if (!ensure(StateForCurrentPlatform))
		{
			continue;
		}
		TSet<TSoftObjectPtr<UBlueprint>> AlreadyGenerated;
		while (AlreadyGenerated.Num() < StateForCurrentPlatform->StubsRequiredByGeneratedCode.Num())
		{
			const int32 OldGeneratedNum = AlreadyGenerated.Num();
			TSet<TSoftObjectPtr<UBlueprint>> LocalCopyStubsRequiredByGeneratedCode = StateForCurrentPlatform->StubsRequiredByGeneratedCode;
			for (TSoftObjectPtr<UBlueprint>& BPPtr : LocalCopyStubsRequiredByGeneratedCode)
			{
				bool bAlreadyGenerated = false;
				AlreadyGenerated.Add(BPPtr, &bAlreadyGenerated);
				if (bAlreadyGenerated)
				{
					continue;
				}

				GenerateSingleStub(BPPtr.LoadSynchronous(), PlatformName);
			}
			// make sure there was any progress
			if (!ensure(OldGeneratedNum != AlreadyGenerated.Num()))
			{
				break;
			}
		}
	}
}

void FBlueprintNativeCodeGenModule::Convert(UPackage* Package, ESavePackageResult CookResult, const FName PlatformName)
{
	// Find the struct/enum to convert:
	UStruct* Struct = nullptr;
	UEnum* Enum = nullptr;
	GetFieldFormPackage(Package, Struct, Enum);

	// First we gather information about bound functions.
	UClass* AsClass = Cast<UClass>(Struct);
	UBlueprint* BP = AsClass ? Cast<UBlueprint>(AsClass->ClassGeneratedBy) : nullptr;
	if (BP)
	{
		CollectBoundFunctions(BP);
	}

	if (CookResult != ESavePackageResult::ReplaceCompletely && CookResult != ESavePackageResult::GenerateStub)
	{
		// nothing to convert
		return;
	}

	if (Struct == nullptr && Enum == nullptr)
	{
		ensure(false);
		return;
	}

	FStatePerPlatform* State = StatesPerPlatform.Find(PlatformName);
	if (!ensure(State))
	{
		return;
	}
	if (CookResult == ESavePackageResult::GenerateStub)
	{
		// No stub is generated for structs and enums.
		ensure(!BP || !State->BPAssetsToGenerate.Contains(BP));
	}
	else
	{
		check(CookResult == ESavePackageResult::ReplaceCompletely);
		if (AsClass)
		{
			if (ensure(BP))
			{
				State->BPAssetsToGenerate.Add(BP);
			}
		}
		else if (Struct)
		{
			State->UDSAssetsToGenerate.Add(Struct);
		}
		else
		{
			GenerateSingleAsset((UField*)Enum, PlatformName);
		}
	}
}

void FBlueprintNativeCodeGenModule::SaveManifest()
{
	for (FName& PlatformName : TargetPlatformNames)
	{
		GetManifest(PlatformName).Save();
	}
}

void FBlueprintNativeCodeGenModule::MergeManifest(int32 ManifestIdentifier)
{
	for (FName& PlatformName : TargetPlatformNames)
	{
		FBlueprintNativeCodeGenManifest& CurrentManifest = GetManifest(PlatformName);
		FBlueprintNativeCodeGenManifest OtherManifest = FBlueprintNativeCodeGenManifest(CurrentManifest.GetTargetPaths().ManifestFilePath(ManifestIdentifier));
		CurrentManifest.Merge(OtherManifest);
	}
} 

void FBlueprintNativeCodeGenModule::FinalizeManifest()
{
	IBlueprintCompilerCppBackendModule& CodeGenBackend = (IBlueprintCompilerCppBackendModule&)IBlueprintCompilerCppBackendModule::Get();
	TSharedPtr<FNativizationSummary> NativizationSummary = CodeGenBackend.NativizationSummary();
	for(FName& PlatformName : TargetPlatformNames)
	{
		FBlueprintNativeCodeGenManifest& Manifest = GetManifest(PlatformName);
		if (Manifest.GetConversionRecord().Num() > 0)
		{
			if (NativizationSummary.IsValid())
			{
				TSet<TSoftObjectPtr<UPackage>>* RequiredModules = NativizationSummary->ModulesRequiredByPlatform.Find(Manifest.GetCompilerNativizationOptions().PlatformName);
				if (RequiredModules)
				{
					for (TSoftObjectPtr<UPackage> ItPackage : *RequiredModules)
					{
						if (UPackage* Pkg = ItPackage.Get())
						{
							Manifest.AddSingleModuleDependency(Pkg);
						}
					}
				}
			}
			ensure(Manifest.GetManifestChunkId() == -1); // ensure this was intended to be the root manifest
			Manifest.Save();
			check(FBlueprintNativeCodeGenUtils::FinalizePlugin(Manifest));
		}
	}
}

UClass* FBlueprintNativeCodeGenModule::FindReplacedClassForObject(const UObject* Object, const FCompilerNativizationOptions& NativizationOptions) const
{
	// we're only looking to replace class types:
	if (Object && Object->IsA<UField>())
	{
		if (IsTargetedForReplacement(Object, NativizationOptions) == EReplacementResult::ReplaceCompletely)
		{
			for (const UClass* Class = Object->GetClass(); Class; Class = Class->GetSuperClass())
			{
				if (Class == UUserDefinedEnum::StaticClass())
				{
					return UEnum::StaticClass();
				}
				if (Class == UUserDefinedStruct::StaticClass())
				{
					return UScriptStruct::StaticClass();
				}
				if (Class == UBlueprintGeneratedClass::StaticClass())
				{
					return UDynamicClass::StaticClass();
				}
			}
		}
		else if (const UByteProperty* ByteProperty = Cast<UByteProperty>(Object))
		{
			// User-Defined Enum values are compiled as Byte properties, but get converted to Enum class properties during nativization. Thus,
			// we have to account for that here and switch the property class to be an Enum property, since that's what will be generated by UHT.
			// If we don't do this, then a dependent asset's import table will contain the incorrect property class for this value, if referenced.
			if (ByteProperty->Enum && IsTargetedForReplacement(ByteProperty->Enum, NativizationOptions) == EReplacementResult::ReplaceCompletely)
			{
				return UEnumProperty::StaticClass();
			}
		}
	}
	return nullptr;
}

UObject* FBlueprintNativeCodeGenModule::FindReplacedNameAndOuter(UObject* Object, FName& OutName, const FCompilerNativizationOptions& NativizationOptions) const
{
	OutName = NAME_None;

	auto GetOuterBPGC = [](UObject* FirstOuter) -> UBlueprintGeneratedClass*
	{
		UBlueprintGeneratedClass* BPGC = nullptr;
		for (UObject* OuterObject = FirstOuter; OuterObject && !BPGC; OuterObject = OuterObject->GetOuter())
		{
			if (OuterObject->HasAnyFlags(RF_ClassDefaultObject))
			{
				return nullptr;
			}
			BPGC = Cast<UBlueprintGeneratedClass>(OuterObject);
		}
		return BPGC;
	};

	UActorComponent* ActorComponent = Cast<UActorComponent>(Object);
	if (ActorComponent)
	{
		//if is child of a BPGC and not child of a CDO
		UBlueprintGeneratedClass* BPGC = GetOuterBPGC(ActorComponent->GetOuter());
		FName NewName = NAME_None;
		UObject* OuterCDO = nullptr;
		for (UBlueprintGeneratedClass* SuperBPGC = BPGC; SuperBPGC && (NewName == NAME_None); SuperBPGC = Cast<UBlueprintGeneratedClass>(SuperBPGC->GetSuperClass()))
		{
			if (SuperBPGC->InheritableComponentHandler)
			{
				FComponentKey FoundKey = SuperBPGC->InheritableComponentHandler->FindKey(ActorComponent);
				if (FoundKey.IsValid())
				{
					NewName = FoundKey.IsSCSKey() ? FoundKey.GetSCSVariableName() : ActorComponent->GetFName();
					OuterCDO = BPGC->GetDefaultObject(false);
					break;
				}
			}
			if (SuperBPGC->SimpleConstructionScript)
			{
				for (auto Node : SuperBPGC->SimpleConstructionScript->GetAllNodes())
				{
					if (Node->ComponentTemplate == ActorComponent)
					{
						NewName = Node->GetVariableName();
						if (NewName != NAME_None)
						{
							OuterCDO = BPGC->GetDefaultObject(false);
							break;
						}
					}
				}
			}
		}

		if (OuterCDO && (EReplacementResult::ReplaceCompletely == IsTargetedForReplacement(OuterCDO->GetClass(), NativizationOptions)))
		{
			OutName = NewName;
			UE_LOG(LogBlueprintCodeGen, Log, TEXT("Object '%s' has replaced name '%s' and outer: '%s'"), *GetPathNameSafe(Object), *OutName.ToString(), *GetPathNameSafe(OuterCDO));
			return OuterCDO;
		}
	}
	else
	{
		UChildActorComponent* OuterCAC = Cast<UChildActorComponent>(Object->GetOuter());
		if (OuterCAC && OuterCAC->GetChildActorTemplate() == Object)
		{
			UBlueprintGeneratedClass* BPGC = GetOuterBPGC(OuterCAC->GetOuter());
			if (BPGC && (EReplacementResult::ReplaceCompletely == IsTargetedForReplacement(BPGC, NativizationOptions)))
			{
				return BPGC;
			}
		}
	}

	return nullptr;
}

EReplacementResult FBlueprintNativeCodeGenModule::IsTargetedForReplacement(const UPackage* Package, const FCompilerNativizationOptions& NativizationOptions) const
{
	// non-native packages with enums and structs should be converted, unless they are blacklisted:
	UStruct* Struct = nullptr;
	UEnum* Enum = nullptr;
	GetFieldFormPackage(Package, Struct, Enum, RF_NoFlags);

	UObject* Target = Struct;
	if (Target == nullptr)
	{
		Target = Enum;
	}
	return IsTargetedForReplacement(Target, NativizationOptions);
}

EReplacementResult FBlueprintNativeCodeGenModule::IsTargetedForReplacement(const UObject* Object, const FCompilerNativizationOptions& NativizationOptions) const
{
	if (Object == nullptr)
	{
		return EReplacementResult::DontReplace;
	}

	const UUserDefinedStruct* const UDStruct = Cast<const UUserDefinedStruct>(Object);
	const UUserDefinedEnum* const UDEnum = Cast<const UUserDefinedEnum>(Object);
	const UBlueprintGeneratedClass* const BlueprintClass = Cast<const UBlueprintGeneratedClass>(Object);
	if (UDStruct == nullptr && UDEnum == nullptr && BlueprintClass == nullptr)
	{
		return EReplacementResult::DontReplace;
	}

	const FStatePerPlatform* StateForCurrentPlatform = StatesPerPlatform.Find(NativizationOptions.PlatformName);
	check(StateForCurrentPlatform);
	const FSoftObjectPath ObjectKey(Object);
	{
		const EReplacementResult* const CachedValue = StateForCurrentPlatform->CachedIsTargetedForReplacement.Find(ObjectKey); //THe referenced returned by FindOrAdd could be invalid later, when filled.
		if (CachedValue)
		{
			return *CachedValue;
		}
	}

	const UBlueprint* const Blueprint = BlueprintClass ? Cast<UBlueprint>(BlueprintClass->ClassGeneratedBy) : nullptr;


	const UProjectPackagingSettings* const PackagingSettings = GetDefault<UProjectPackagingSettings>();
	const bool bNativizeOnlySelectedBPs = PackagingSettings && PackagingSettings->BlueprintNativizationMethod == EProjectPackagingBlueprintNativizationMethod::Exclusive;


	auto ObjectIsNotReplacedAtAll = [&]() -> bool
	{
		// EDITOR ON DEVELOPMENT OBJECT
		{
			auto IsDeveloperObject = [](const UObject* Obj) -> bool
			{
				auto IsObjectFromDeveloperPackage = [](const UObject* InObj) -> bool
				{
					return InObj && InObj->GetOutermost()->HasAllPackagesFlags(PKG_Developer);
				};

				if (Obj)
				{
					if (IsObjectFromDeveloperPackage(Obj))
					{
						return true;
					}
					const UStruct* StructToTest = Obj->IsA<UStruct>() ? CastChecked<const UStruct>(Obj) : Obj->GetClass();
					for (; StructToTest; StructToTest = StructToTest->GetSuperStruct())
					{
						if (IsObjectFromDeveloperPackage(StructToTest))
						{
							return true;
						}
					}
				}
				return false;
			};
			if (Object && (IsEditorOnlyObject(Object) || IsDeveloperObject(Object)))
			{
				UE_LOG(LogBlueprintCodeGen, Warning, TEXT("Object %s depends on Editor or Development stuff. It shouldn't be cooked."), *GetPathNameSafe(Object));
				return true;
			}
		}
		// DATA ONLY BP
		{
			static const FBoolConfigValueHelper DontNativizeDataOnlyBP(TEXT("BlueprintNativizationSettings"), TEXT("bDontNativizeDataOnlyBP"));
			if (DontNativizeDataOnlyBP && !bNativizeOnlySelectedBPs && Blueprint && FBlueprintEditorUtils::IsDataOnlyBlueprint(Blueprint))
			{
				return true;
			}
		}
		// Don't convert objects like Default__WidgetBlueprintGeneratedClass
		if (Object && (Object->HasAnyFlags(RF_ClassDefaultObject)))
		{
			return true;
		}
		return false;
	};
	if (ObjectIsNotReplacedAtAll())
	{
		StateForCurrentPlatform->CachedIsTargetedForReplacement.Add(ObjectKey, EReplacementResult::DontReplace);
		return EReplacementResult::DontReplace;
	}

	auto ObjectGenratesOnlyStub = [&]() -> bool
	{
		// ExcludedFolderPaths
		{
			const FString ObjPathName = Object->GetPathName();
			for (const FString& ExcludedPath : ExcludedFolderPaths)
			{
				if (ObjPathName.StartsWith(ExcludedPath))
				{
					return true;
				}
			}
			for (const FString& ExcludedPath : NativizationOptions.ExcludedFolderPaths)
			{
				if (ObjPathName.StartsWith(ExcludedPath))
				{
					return true;
				}
			}
		}

		// ExcludedAssetTypes
		{
			// we can't use FindObject, because we may be converting a type while saving
			if (UDEnum && ExcludedAssetTypes.Find(UDEnum->GetPathName()) != INDEX_NONE)
			{
				return true;
			}

			const UStruct* LocStruct = Cast<const UStruct>(Object);
			while (LocStruct)
			{
				if (ExcludedAssetTypes.Find(LocStruct->GetPathName()) != INDEX_NONE)
				{
					return true;
				}
				LocStruct = LocStruct->GetSuperStruct();
			}
		}

		// ExcludedAssets
		{
			if (ExcludedAssets.Contains(Object->GetOutermost()))
			{
				return true;
			}
			if (NativizationOptions.ExcludedAssets.Contains(Object->GetOutermost()))
			{
				return true;
			}
		}


		if (Blueprint && BlueprintClass)
		{
			// Reducible AnimBP
			{
				static const FBoolConfigValueHelper NativizeAnimBPOnlyWhenNonReducibleFuncitons(TEXT("BlueprintNativizationSettings"), TEXT("bNativizeAnimBPOnlyWhenNonReducibleFuncitons"));
				if (NativizeAnimBPOnlyWhenNonReducibleFuncitons)
				{
					if (const UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(Blueprint))
					{
						ensure(AnimBlueprint->bHasBeenRegenerated);
						if (AnimBlueprint->bHasAnyNonReducibleFunction == UBlueprint::EIsBPNonReducible::No)
						{
							UE_LOG(LogBlueprintCodeGen, Log, TEXT("AnimBP %s without non-reducible functions is excluded from nativization"), *GetPathNameSafe(Blueprint));
							return true;
						}
					}
				}
			}

			// Unconvertable Blueprint
			{
				const EBlueprintType UnconvertableBlueprintTypes[] = {
					//BPTYPE_Const,		// What is a "const" Blueprint?
					BPTYPE_MacroLibrary,
					BPTYPE_LevelScript,
				};
				const EBlueprintType BlueprintType = Blueprint->BlueprintType;
				for (int32 TypeIndex = 0; TypeIndex < ARRAY_COUNT(UnconvertableBlueprintTypes); ++TypeIndex)
				{
					if (BlueprintType == UnconvertableBlueprintTypes[TypeIndex])
					{
						return true;
					}
				}
			}

			// ExcludedBlueprintTypes
			for (TSoftClassPtr<UBlueprint> ExcludedBlueprintTypeAsset : ExcludedBlueprintTypes)
			{
				UClass* ExcludedBPClass = ExcludedBlueprintTypeAsset.Get();
				if (!ExcludedBPClass)
				{
					ExcludedBPClass = ExcludedBlueprintTypeAsset.LoadSynchronous();
				}
				if (ExcludedBPClass && Blueprint->IsA(ExcludedBPClass))
				{
					return true;
				}
			}

			const bool bFlaggedForNativization = (Blueprint->NativizationFlag == EBlueprintNativizationFlag::Dependency) ?
				PackagingSettings->IsBlueprintAssetInNativizationList(Blueprint) :
				(Blueprint->NativizationFlag == EBlueprintNativizationFlag::ExplicitlyEnabled);
			// Blueprint is not selected
			if (bNativizeOnlySelectedBPs && !bFlaggedForNativization && !FBlueprintEditorUtils::ShouldNativizeImplicitly(Blueprint))
			{
				return true;
			}

			// Parent Class in not converted
			for (const UBlueprintGeneratedClass* ParentClassIt = Cast<UBlueprintGeneratedClass>(BlueprintClass->GetSuperClass())
				; ParentClassIt; ParentClassIt = Cast<UBlueprintGeneratedClass>(ParentClassIt->GetSuperClass()))
			{
				const EReplacementResult ParentResult = IsTargetedForReplacement(ParentClassIt, NativizationOptions);
				if (ParentResult != EReplacementResult::ReplaceCompletely)
				{
					if (bNativizeOnlySelectedBPs)
					{
						UE_LOG(LogBlueprintCodeGen, Error, TEXT("BP %s is selected for nativization, but its parent class %s is not nativized."), *GetPathNameSafe(Blueprint), *GetPathNameSafe(ParentClassIt));
					}
					return true;
				}
			}

			// Interface class not converted
			TArray<UClass*> InterfaceClasses;
			FBlueprintEditorUtils::FindImplementedInterfaces(Blueprint, false, InterfaceClasses);
			for (const UClass* InterfaceClassIt : InterfaceClasses)
			{
				const UBlueprintGeneratedClass* InterfaceBPGC = Cast<const UBlueprintGeneratedClass>(InterfaceClassIt);
				if (InterfaceBPGC)
				{
					const EReplacementResult InterfaceResult = IsTargetedForReplacement(InterfaceBPGC, NativizationOptions);
					if (InterfaceResult != EReplacementResult::ReplaceCompletely)
					{
						if (bNativizeOnlySelectedBPs)
						{
							UE_LOG(LogBlueprintCodeGen, Error, TEXT("BP %s is selected for nativization, but BP interface class %s is not nativized."), *GetPathNameSafe(Blueprint), *GetPathNameSafe(InterfaceClassIt));
						}
						return true;
					}
				}
				else if (InterfaceClassIt->GetCppTypeInfo()->IsAbstract())
				{
					UE_LOG(LogBlueprintCodeGen, Error, TEXT("BP %s is selected for nativization, but it cannot be nativized because it currently implements an interface class (%s) that declares one or more pure virtual functions."), *GetPathNameSafe(Blueprint), *GetPathNameSafe(InterfaceClassIt));
					return true;
				}
			}
		}
		return false;
	};
	if (ObjectGenratesOnlyStub())
	{
		StateForCurrentPlatform->CachedIsTargetedForReplacement.Add(ObjectKey, EReplacementResult::GenerateStub);
		return EReplacementResult::GenerateStub;
	}

	StateForCurrentPlatform->CachedIsTargetedForReplacement.Add(ObjectKey, EReplacementResult::ReplaceCompletely);
	return EReplacementResult::ReplaceCompletely;
}

void FBlueprintNativeCodeGenModule::FillPlatformNativizationDetails(const ITargetPlatform* Platform, FPlatformNativizationDetails& Details)
{
	check(Platform);
	const PlatformInfo::FPlatformInfo& PlatformInfo = Platform->GetPlatformInfo();

	Details.PlatformName = PlatformInfo.TargetPlatformName;
	Details.CompilerNativizationOptions.PlatformName = Details.PlatformName;
	Details.CompilerNativizationOptions.ClientOnlyPlatform = Platform->IsClientOnly();
	Details.CompilerNativizationOptions.ServerOnlyPlatform = Platform->IsServerOnly();

	auto GatherExcludedStuff = [&](const TCHAR* KeyForExcludedModules, const TCHAR* KeyForExcludedPaths, const TCHAR* KeyForExcludedAssets)
	{
		const TCHAR* ConfigSection = TEXT("BlueprintNativizationSettings");
		{
			TArray<FString> ExcludedModuls;
			GConfig->GetArray(ConfigSection, KeyForExcludedModules, ExcludedModuls, GEditorIni);
			for (const FString& NameStr : ExcludedModuls)
			{
				Details.CompilerNativizationOptions.ExcludedModules.Add(FName(*NameStr));
			}
		}
		GConfig->GetArray(ConfigSection, KeyForExcludedPaths, Details.CompilerNativizationOptions.ExcludedFolderPaths, GEditorIni);

		{
			TArray<FString> ExcludedAssetPaths;
			GConfig->GetArray(ConfigSection, KeyForExcludedAssets, ExcludedAssetPaths, GEditorIni);
			for (FString& Path : ExcludedAssetPaths)
			{
				Details.CompilerNativizationOptions.ExcludedAssets.Add(FSoftObjectPath(Path));
			}
		}
	};
	if (Details.CompilerNativizationOptions.ServerOnlyPlatform)
	{
		GatherExcludedStuff(TEXT("ModulsExcludedFromNativizedServer"), TEXT("ExcludedFolderPathsFromServer"), TEXT("ExcludedAssetsFromServer"));
	}
	if (Details.CompilerNativizationOptions.ClientOnlyPlatform)
	{
		GatherExcludedStuff(TEXT("ModulsExcludedFromNativizedClient"), TEXT("ExcludedFolderPathsFromClient"), TEXT("ExcludedAssetsFromClient"));
	}
}

IMPLEMENT_MODULE(FBlueprintNativeCodeGenModule, BlueprintNativeCodeGen);
