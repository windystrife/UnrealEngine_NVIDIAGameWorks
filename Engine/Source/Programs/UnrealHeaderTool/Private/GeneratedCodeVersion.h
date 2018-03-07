// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Containers/UnrealString.h"

class FArchive;
class FString;

//
// This MUST be kept in sync with EGeneratedBodyVersion in UBT defined in ExternalExecution.cs
// and with ToGeneratedBodyVersion function below
//
enum class EGeneratedCodeVersion : uint8
{
	None,
	V1,
	V2,
	VLatest = V2
};

inline FArchive& operator<<(FArchive& Ar, EGeneratedCodeVersion& Type)
{
	if (Ar.IsLoading())
	{
		uint8 Value;
		Ar << Value;
		Type = (EGeneratedCodeVersion)Value;
	}
	else if (Ar.IsSaving())
	{
		uint8 Value = (uint8)Type;
		Ar << Value;
	}
	return Ar;
}

inline EGeneratedCodeVersion ToGeneratedCodeVersion(const FString& InString)
{
	if (InString.Compare(TEXT("V1")) == 0)
	{
		return EGeneratedCodeVersion::V1;
	}

	if (InString.Compare(TEXT("V2")) == 0)
	{
		return EGeneratedCodeVersion::V2;
	}

	if (InString.Compare(TEXT("VLatest")) == 0)
	{
		return EGeneratedCodeVersion::VLatest;
	}

	return EGeneratedCodeVersion::None;
}
