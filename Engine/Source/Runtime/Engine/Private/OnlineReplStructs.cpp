// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	OnlineReplStructs.cpp: Unreal networking serialization helpers
=============================================================================*/

#include "GameFramework/OnlineReplStructs.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/MemoryReader.h"
#include "UObject/PropertyPortFlags.h"
#include "Dom/JsonValue.h"
#include "EngineLogs.h"
#include "Net/OnlineEngineInterface.h"

FArchive& operator<<( FArchive& Ar, FUniqueNetIdRepl& UniqueNetId)
{
	int32 Size = UniqueNetId.IsValid() ? UniqueNetId->GetSize() : 0;
	Ar << Size;

	if (Size > 0)
	{
		if (Ar.IsSaving())
		{
			check(UniqueNetId.IsValid());
			FString Contents = UniqueNetId->ToString();
			Ar << Contents;
		}
		else if (Ar.IsLoading())
		{
			FString Contents;
			Ar << Contents;	// that takes care about possible overflow

			UniqueNetId.UniqueIdFromString(Contents);
		}
	}
	else if (Ar.IsLoading())
	{
		// @note: replicated a nullptr unique id
		UniqueNetId.SetUniqueNetId(nullptr);
	}

	return Ar;
}

void FUniqueNetIdRepl::UniqueIdFromString(const FString& Contents)
{
	// Don't need to distinguish OSS interfaces here with world because we just want the create function below
	TSharedPtr<const FUniqueNetId> UniqueNetIdPtr = UOnlineEngineInterface::Get()->CreateUniquePlayerId(Contents);
	SetUniqueNetId(UniqueNetIdPtr);
}

bool FUniqueNetIdRepl::NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess)
{
	Ar << *this;
	bOutSuccess = true;
	return true;
}

bool FUniqueNetIdRepl::Serialize(FArchive& Ar)
{
	Ar << *this;
	return true;
}

bool FUniqueNetIdRepl::ExportTextItem(FString& ValueStr, FUniqueNetIdRepl const& DefaultValue, UObject* Parent, int32 PortFlags, UObject* ExportRootScope) const
{
	if (0 != (PortFlags & EPropertyPortFlags::PPF_ExportCpp))
	{
		return false;
	}

	ValueStr += UniqueNetId.IsValid() ? UniqueNetId->ToString() : TEXT("INVALID");
	return true;
}

bool FUniqueNetIdRepl::ImportTextItem(const TCHAR*& Buffer, int32 PortFlags, UObject* Parent, FOutputDevice* ErrorText)
{
	SetUniqueNetId(nullptr);

	bool bShouldWarn = true;
	if (Buffer)
	{
		static FString InvalidString(TEXT("INVALID"));
		if (Buffer[0] == TEXT('\0') || Buffer == InvalidString)
		{
			// An empty string or the word invalid are just considered expected invalid FUniqueNetIdRepls. No need to warn about those.
			bShouldWarn = false;
		}
		else
		{
			checkf(UOnlineEngineInterface::Get() && UOnlineEngineInterface::Get()->IsLoaded(), TEXT("Attempted to ImportText to FUniqueNetIdRepl while OSS is not loaded. Parent:%s"), *GetPathNameSafe(Parent));
			FString Contents(Buffer);
			UniqueIdFromString(Contents);
		}
	}

	if (bShouldWarn && !IsValid())
	{
#if !NO_LOGGING
		ErrorText->CategorizedLogf(LogNet.GetCategoryName(), ELogVerbosity::Warning, TEXT("Failed to import text to FUniqueNetIdRepl Parent:%s"), *GetPathNameSafe(Parent));
#endif
	}

	return true;
}

TSharedRef<FJsonValue> FUniqueNetIdRepl::ToJson() const
{
	if (IsValid())
	{
		return MakeShareable(new FJsonValueString(ToString()));
	}
	else
	{
		return MakeShareable(new FJsonValueString(TEXT("INVALID")));
	}
}

void FUniqueNetIdRepl::FromJson(const FString& Json)
{
	SetUniqueNetId(nullptr);

	if (!Json.IsEmpty())
	{
		UniqueIdFromString(Json);
	}
}

void TestUniqueIdRepl(UWorld* InWorld)
{
#if !UE_BUILD_SHIPPING
	bool bSuccess = true;

	TSharedPtr<const FUniqueNetId> UserId = UOnlineEngineInterface::Get()->GetUniquePlayerId(InWorld, 0);

	FUniqueNetIdRepl EmptyIdIn;
	if (EmptyIdIn.IsValid())
	{
		UE_LOG(LogNet, Warning, TEXT("EmptyId is valid."), *EmptyIdIn->ToString());
		bSuccess = false;
	}

	FUniqueNetIdRepl ValidIdIn(UserId);
	if (!ValidIdIn.IsValid() || UserId != ValidIdIn.GetUniqueNetId() || *UserId != *ValidIdIn)
	{
		UE_LOG(LogNet, Warning, TEXT("UserId input %s != UserId output %s"), *UserId->ToString(), *ValidIdIn->ToString());
		bSuccess = false;
	}

	if (bSuccess)
	{
		TArray<uint8> Buffer;
		for (int32 i = 0; i < 2; i++)
		{
			Buffer.Empty();
			FMemoryWriter TestWriteUniqueId(Buffer);

			if (i == 0)
			{
				// Normal serialize
				TestWriteUniqueId << EmptyIdIn;
				TestWriteUniqueId << ValidIdIn;
			}
			else
			{
				// Net serialize
				bool bOutSuccess = false;
				EmptyIdIn.NetSerialize(TestWriteUniqueId, NULL, bOutSuccess);
				ValidIdIn.NetSerialize(TestWriteUniqueId, NULL, bOutSuccess);
			}

			FMemoryReader TestReadUniqueId(Buffer);

			FUniqueNetIdRepl EmptyIdOut;
			TestReadUniqueId << EmptyIdOut;
			if (EmptyIdOut.GetUniqueNetId().IsValid())
			{
				UE_LOG(LogNet, Warning, TEXT("EmptyId %s should have been invalid"), *EmptyIdOut->ToString());
				bSuccess = false;
			}

			FUniqueNetIdRepl ValidIdOut;
			TestReadUniqueId << ValidIdOut;
			if (*UserId != *ValidIdOut.GetUniqueNetId())
			{
				UE_LOG(LogNet, Warning, TEXT("UserId input %s != UserId output %s"), *ValidIdIn->ToString(), *ValidIdOut->ToString());
				bSuccess = false;
			}
		}
	}

	if (bSuccess)
	{
		FString OutString;
		TSharedRef<FJsonValue> JsonValue = ValidIdIn.ToJson();
		bSuccess = JsonValue->TryGetString(OutString);
		if (bSuccess)
		{
			FUniqueNetIdRepl NewIdOut;
			NewIdOut.FromJson(OutString);
			bSuccess = NewIdOut.IsValid();
		}
	}


	if (!bSuccess)
	{
		UE_LOG(LogNet, Warning, TEXT("TestUniqueIdRepl test failure!"));
	}
#endif
}




