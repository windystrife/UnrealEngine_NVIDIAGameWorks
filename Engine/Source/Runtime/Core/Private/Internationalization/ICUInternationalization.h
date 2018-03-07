// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreTypes.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Containers/Set.h"
#include "Containers/Map.h"
#include "Internationalization/CulturePointer.h"
#include "Templates/UniquePtr.h"

class FInternationalization;

#if UE_ENABLE_ICU

THIRD_PARTY_INCLUDES_START
	#include <unicode/umachine.h>
	#include <unicode/gregocal.h>
THIRD_PARTY_INCLUDES_END

// This should be defined by ICU.build.cs
#ifndef NEEDS_ICU_DLLS
	#define NEEDS_ICU_DLLS 0
#endif

class FICUInternationalization
{
public:
	FICUInternationalization(FInternationalization* const I18N);

	bool Initialize();
	void Terminate();

	void LoadAllCultureData();

	bool IsCultureRemapped(const FString& Name, FString* OutMappedCulture);
	bool IsCultureDisabled(const FString& Name);

	void HandleLanguageChanged(const FString& Name);
	void GetCultureNames(TArray<FString>& CultureNames) const;
	TArray<FString> GetPrioritizedCultureNames(const FString& Name);
	FCulturePtr GetCulture(const FString& Name);

	UDate UEDateTimeToICUDate(const FDateTime& DateTime);

private:
#if NEEDS_ICU_DLLS
	void LoadDLLs();
	void UnloadDLLs();
#endif

	void InitializeAvailableCultures();
	void ConditionalInitializeCultureMappings();
	void ConditionalInitializeDisabledCultures();

	enum class EAllowDefaultCultureFallback : uint8 { No, Yes, };
	FCulturePtr FindOrMakeCulture(const FString& Name, const EAllowDefaultCultureFallback AllowDefaultFallback);

	void InitializeInvariantGregorianCalendar();

private:
	struct FICUCultureData
	{
		FString Name;
		FString LanguageCode;
		FString ScriptCode;
		FString CountryCode;

		bool operator==(const FICUCultureData& Other) const
		{
			return Name == Other.Name;
		}

		bool operator!=(const FICUCultureData& Other) const
		{
			return Name != Other.Name;
		}
	};

private:
	FInternationalization* const I18N;

	TArray< void* > DLLHandles;

	TArray<FICUCultureData> AllAvailableCultures;
	TMap<FString, int32> AllAvailableCulturesMap;
	TMap<FString, TArray<int32>> AllAvailableLanguagesToSubCulturesMap;

	bool bHasInitializedCultureMappings;
	TMap<FString, FString> CultureMappings;

	bool bHasInitializedDisabledCultures;
	TSet<FString> DisabledCultures;

	TMap<FString, FCultureRef> CachedCultures;
	FCriticalSection CachedCulturesCS;

	TUniquePtr<icu::GregorianCalendar> InvariantGregorianCalendar;
	FCriticalSection InvariantGregorianCalendarCS;

	static UBool OpenDataFile(const void* context, void** fileContext, void** contents, const char* path);
	static void CloseDataFile(const void* context, void* const fileContext, void* const contents);

	// Tidy class for storing the count of references for an ICU data file and the file's data itself.
	struct FICUCachedFileData
	{
		FICUCachedFileData(const int64 FileSize);
		FICUCachedFileData(const FICUCachedFileData& Source);
		FICUCachedFileData(FICUCachedFileData&& Source);
		~FICUCachedFileData();

		uint32 ReferenceCount;
		void* Buffer;
	};

	// Map for associating ICU data file paths with cached file data, to prevent multiple copies of immutable ICU data files from residing in memory.
	TMap<FString, FICUCachedFileData> PathToCachedFileDataMap;
};

#endif
