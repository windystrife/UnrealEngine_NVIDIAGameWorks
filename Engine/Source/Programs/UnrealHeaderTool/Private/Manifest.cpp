// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Manifest.h"
#include "UnrealHeaderTool.h"
#include "Misc/DateTime.h"
#include "Logging/LogMacros.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/ErrorException.h"
#include "Misc/PackageName.h"
#include "UnrealHeaderToolGlobals.h"
#include "Serialization/JsonTypes.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	template <typename T> struct TJsonFieldType;
	template <>           struct TJsonFieldType<double>                         { static const EJson Value = EJson::Number;  };
	template <>           struct TJsonFieldType<FString>                        { static const EJson Value = EJson::String;  };
	template <>           struct TJsonFieldType<bool>                           { static const EJson Value = EJson::Boolean; };
	template <>           struct TJsonFieldType<TArray<TSharedPtr<FJsonValue>>> { static const EJson Value = EJson::Array;   };
	template <>           struct TJsonFieldType<TSharedPtr<FJsonObject>>        { static const EJson Value = EJson::Object;  };

	template <typename T>
	void GetJsonValue(T& OutVal, const TSharedPtr<FJsonValue>& JsonValue, const TCHAR* Outer)
	{
		if (JsonValue->Type != TJsonFieldType<T>::Value)
		{
			FError::Throwf(TEXT("'%s' is the wrong type"), Outer);
		}

		JsonValue->AsArgumentType(OutVal);
	}

	template <typename T>
	void GetJsonFieldValue(T& OutVal, const TSharedPtr<FJsonObject>& JsonObject, const TCHAR* FieldName, const TCHAR* Outer)
	{
		auto JsonValue = JsonObject->Values.Find(FieldName);

		if (!JsonValue)
		{
			FError::Throwf(TEXT("Unable to find field '%s' in '%s'"), FieldName, Outer);
		}

		if ((*JsonValue)->Type != TJsonFieldType<T>::Value)
		{
			FError::Throwf(TEXT("Field '%s' in '%s' is the wrong type"), Outer);
		}

		(*JsonValue)->AsArgumentType(OutVal);
	}

	void ProcessHeaderArray(FString* OutStringArray, const TArray<TSharedPtr<FJsonValue>>& InJsonArray, const TCHAR* Outer)
	{
		for (int32 Index = 0, Count = InJsonArray.Num(); Index != Count; ++Index)
		{
			GetJsonValue(*OutStringArray++, InJsonArray[Index], *FString::Printf(TEXT("%s[%d]"), Outer, Index));
		}
	}
}


FManifest FManifest::LoadFromFile(const FString& Filename)
{
	FManifest Result;
	FString FilenamePath = FPaths::GetPath(Filename);
	FString Json;

	if (!FFileHelper::LoadFileToString(Json, *Filename))
	{
		FError::Throwf(TEXT("Unable to load manifest: %s"), *Filename);
	}

	auto RootObject = TSharedPtr<FJsonObject>();
	auto Reader = TJsonReaderFactory<TCHAR>::Create(Json);

	if (!FJsonSerializer::Deserialize(Reader, RootObject))
	{
		FError::Throwf(TEXT("Manifest is malformed: %s"), *Filename);
	}

	TArray<TSharedPtr<FJsonValue>> ModulesArray;

	GetJsonFieldValue(Result.IsGameTarget,             RootObject, TEXT("IsGameTarget"),             TEXT("{manifest root}"));
	GetJsonFieldValue(Result.RootLocalPath,            RootObject, TEXT("RootLocalPath"),            TEXT("{manifest root}"));
	GetJsonFieldValue(Result.RootBuildPath,            RootObject, TEXT("RootBuildPath"),            TEXT("{manifest root}"));
	GetJsonFieldValue(Result.TargetName,               RootObject, TEXT("TargetName"),               TEXT("{manifest root}"));
	GetJsonFieldValue(Result.ExternalDependenciesFile, RootObject, TEXT("ExternalDependenciesFile"), TEXT("{manifest root}"));
	GetJsonFieldValue(ModulesArray,                    RootObject, TEXT("Modules"),                  TEXT("{manifest root}"));

	UE_LOG(LogCompile, Log, TEXT("Loaded manifest: %s"),       *Filename);
	UE_LOG(LogCompile, Log, TEXT("Manifest.IsGameTarget=%s"),  Result.IsGameTarget ? TEXT("True") : TEXT("False"));
	UE_LOG(LogCompile, Log, TEXT("Manifest.RootLocalPath=%s"), *Result.RootLocalPath);
	UE_LOG(LogCompile, Log, TEXT("Manifest.RootBuildPath=%s"), *Result.RootBuildPath);
	UE_LOG(LogCompile, Log, TEXT("Manifest.TargetName=%s"),    *Result.TargetName);
	UE_LOG(LogCompile, Log, TEXT("Manifest.Modules=%d"),       ModulesArray.Num());

	Result.RootLocalPath = FPaths::ConvertRelativePathToFull(FilenamePath, Result.RootLocalPath);
	Result.RootBuildPath = FPaths::ConvertRelativePathToFull(FilenamePath, Result.RootBuildPath);

	// Ensure directories end with a slash, because this aids their use with FPaths::MakePathRelativeTo.
	if (!Result.RootLocalPath.EndsWith(TEXT("/")))
	{
		Result.RootLocalPath += TEXT("/");
	}

	if (!Result.RootBuildPath.EndsWith(TEXT("/")))
	{
		Result.RootBuildPath += TEXT("/");
	}

	int32 ModuleIndex = 0;

	for (auto Module : ModulesArray)
	{
		auto ModuleObj = Module->AsObject();

		TArray<TSharedPtr<FJsonValue>> ClassesHeaders;
		TArray<TSharedPtr<FJsonValue>> PublicHeaders;
		TArray<TSharedPtr<FJsonValue>> PrivateHeaders;

		Result.Modules.AddZeroed();
		auto& KnownModule = Result.Modules.Last();

		FString Outer = FString::Printf(TEXT("Modules[%d]"), ModuleIndex);
		FString GeneratedCodeVersionString;
		GetJsonFieldValue(KnownModule.Name,                      ModuleObj, TEXT("Name"),                     *Outer);
		GetJsonFieldValue(KnownModule.BaseDirectory,             ModuleObj, TEXT("BaseDirectory"),            *Outer);
		GetJsonFieldValue(KnownModule.IncludeBase,               ModuleObj, TEXT("IncludeBase"),              *Outer);
		GetJsonFieldValue(KnownModule.GeneratedIncludeDirectory, ModuleObj, TEXT("OutputDirectory"),          *Outer);
		GetJsonFieldValue(KnownModule.SaveExportedHeaders,       ModuleObj, TEXT("SaveExportedHeaders"),      *Outer);
		GetJsonFieldValue(ClassesHeaders,                        ModuleObj, TEXT("ClassesHeaders"),           *Outer);
		GetJsonFieldValue(PublicHeaders,                         ModuleObj, TEXT("PublicHeaders"),            *Outer);
		GetJsonFieldValue(PrivateHeaders,                        ModuleObj, TEXT("PrivateHeaders"),           *Outer);
		GetJsonFieldValue(KnownModule.PCH,                       ModuleObj, TEXT("PCH"),                      *Outer);
		GetJsonFieldValue(KnownModule.GeneratedCPPFilenameBase,  ModuleObj, TEXT("GeneratedCPPFilenameBase"), *Outer);
		GetJsonFieldValue(GeneratedCodeVersionString, ModuleObj, TEXT("UHTGeneratedCodeVersion"), *Outer);
		KnownModule.GeneratedCodeVersion = ToGeneratedCodeVersion(GeneratedCodeVersionString);

		FString ModuleTypeText;
		GetJsonFieldValue(ModuleTypeText, ModuleObj, TEXT("ModuleType"), *Outer);
		KnownModule.ModuleType = EBuildModuleType::Parse(*ModuleTypeText);

		KnownModule.LongPackageName = FPackageName::ConvertToLongScriptPackageName(*KnownModule.Name);

		// Convert relative paths
		KnownModule.BaseDirectory             = FPaths::ConvertRelativePathToFull(FilenamePath, KnownModule.BaseDirectory);
		KnownModule.IncludeBase               = FPaths::ConvertRelativePathToFull(FilenamePath, KnownModule.IncludeBase);
		KnownModule.GeneratedIncludeDirectory = FPaths::ConvertRelativePathToFull(FilenamePath, KnownModule.GeneratedIncludeDirectory);
		KnownModule.GeneratedCPPFilenameBase  = FPaths::ConvertRelativePathToFull(FilenamePath, KnownModule.GeneratedCPPFilenameBase);

		// Ensure directories end with a slash, because this aids their use with FPaths::MakePathRelativeTo.
		if (!KnownModule.BaseDirectory            .EndsWith(TEXT("/"))) { KnownModule.BaseDirectory            .AppendChar(TEXT('/')); }
		if (!KnownModule.IncludeBase              .EndsWith(TEXT("/"))) { KnownModule.IncludeBase              .AppendChar(TEXT('/')); }
		if (!KnownModule.GeneratedIncludeDirectory.EndsWith(TEXT("/"))) { KnownModule.GeneratedIncludeDirectory.AppendChar(TEXT('/')); }

		KnownModule.PublicUObjectClassesHeaders.AddZeroed(ClassesHeaders.Num());
		KnownModule.PublicUObjectHeaders       .AddZeroed(PublicHeaders .Num());
		KnownModule.PrivateUObjectHeaders      .AddZeroed(PrivateHeaders.Num());

		ProcessHeaderArray(KnownModule.PublicUObjectClassesHeaders.GetData(), ClassesHeaders, *(Outer + TEXT(".ClassHeaders")));
		ProcessHeaderArray(KnownModule.PublicUObjectHeaders       .GetData(), PublicHeaders , *(Outer + TEXT(".PublicHeaders" )));
		ProcessHeaderArray(KnownModule.PrivateUObjectHeaders      .GetData(), PrivateHeaders, *(Outer + TEXT(".PrivateHeaders")));

		// Sort the headers alphabetically.  This is just to add determinism to the compilation dependency order, since we currently
		// don't rely on explicit includes (but we do support 'dependson')
		// @todo uht: Ideally, we should sort these by sensical order before passing them in -- or better yet, follow include statements ourselves in here.
		KnownModule.PublicUObjectClassesHeaders.Sort();
		KnownModule.PublicUObjectHeaders       .Sort();
		KnownModule.PrivateUObjectHeaders      .Sort();

		UE_LOG(LogCompile, Log, TEXT("  %s"), *KnownModule.Name);
		UE_LOG(LogCompile, Log, TEXT("  .BaseDirectory=%s"), *KnownModule.BaseDirectory);
		UE_LOG(LogCompile, Log, TEXT("  .IncludeBase=%s"), *KnownModule.IncludeBase);
		UE_LOG(LogCompile, Log, TEXT("  .GeneratedIncludeDirectory=%s"), *KnownModule.GeneratedIncludeDirectory);
		UE_LOG(LogCompile, Log, TEXT("  .SaveExportedHeaders=%s"), KnownModule.SaveExportedHeaders ? TEXT("True") : TEXT("False"));
		UE_LOG(LogCompile, Log, TEXT("  .GeneratedCPPFilenameBase=%s"), *KnownModule.GeneratedCPPFilenameBase);
		UE_LOG(LogCompile, Log, TEXT("  .ModuleType=%s"), *ModuleTypeText);

		++ModuleIndex;
	}

	return Result;
}

bool FManifestModule::NeedsRegeneration() const
{
	if (ShouldForceRegeneration())
	{
		return true;
	}
	FString Timestamp;
	TCHAR TimestampText[] = TEXT("Timestamp");
	Timestamp.Empty(GeneratedIncludeDirectory.Len() + ARRAY_COUNT(TimestampText));
	Timestamp += GeneratedIncludeDirectory;
	Timestamp += TEXT("Timestamp");

	if (!FPaths::FileExists(Timestamp))
	{
		// No timestamp, must regenerate.
		return true;
	}

	FDateTime TimestampFileLastModify = IFileManager::Get().GetTimeStamp(*Timestamp);

	for (const FString& Header : PublicUObjectClassesHeaders)
	{
		if (IFileManager::Get().GetTimeStamp(*Header) > TimestampFileLastModify)
		{
			UE_LOG(LogCompile, Log, TEXT("File %s is newer than last timestamp. Regenerating reflection data for module %s."), *Header, *Name);
			return true;
		}
	}

	for (const FString& Header : PublicUObjectHeaders)
	{
		if (IFileManager::Get().GetTimeStamp(*Header) > TimestampFileLastModify)
		{
			UE_LOG(LogCompile, Log, TEXT("File %s is newer than last timestamp. Regenerating reflection data for module %s."), *Header, *Name);
			return true;
		}
	}

	for (const FString& Header : PrivateUObjectHeaders)
	{
		if (IFileManager::Get().GetTimeStamp(*Header) > TimestampFileLastModify)
		{
			UE_LOG(LogCompile, Log, TEXT("File %s is newer than last timestamp. Regenerating reflection data for module %s."), *Header, *Name);
			return true;
		}
	}

	// No header is newer than timestamp, no need to regenerate.
	return false;
}

bool FManifestModule::IsCompatibleWith(const FManifestModule& ManifestModule)
{
	return Name == ManifestModule.Name
		&& ModuleType == ManifestModule.ModuleType
		&& LongPackageName == ManifestModule.LongPackageName
		&& BaseDirectory == ManifestModule.BaseDirectory
		&& IncludeBase == ManifestModule.IncludeBase
		&& GeneratedIncludeDirectory == ManifestModule.GeneratedIncludeDirectory
		&& PublicUObjectClassesHeaders == ManifestModule.PublicUObjectClassesHeaders
		&& PublicUObjectHeaders == ManifestModule.PublicUObjectHeaders
		&& PrivateUObjectHeaders == ManifestModule.PrivateUObjectHeaders
		&& PCH == ManifestModule.PCH
		&& GeneratedCPPFilenameBase == ManifestModule.GeneratedCPPFilenameBase
		&& SaveExportedHeaders == ManifestModule.SaveExportedHeaders
		&& GeneratedCodeVersion == ManifestModule.GeneratedCodeVersion;
}
