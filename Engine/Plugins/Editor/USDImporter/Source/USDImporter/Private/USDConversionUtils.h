// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once 

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "USDImporter.h"

namespace USDUtils
{
	template<typename T>
	T* FindOrCreateObject(UObject* InParent, const FString& InName, EObjectFlags Flags)
	{
		T* Object = FindObject<T>(InParent, *InName);

		if (!Object)
		{
			Object = NewObject<T>(InParent, FName(*InName), Flags);
		}

		return Object;
	}
}

namespace USDToUnreal
{
	static FString ConvertString(const std::string& InString)
	{
		return FString(ANSI_TO_TCHAR(InString.c_str()));
	}

	static FString ConvertString(const char* InString)
	{
		return FString(ANSI_TO_TCHAR(InString));
	}

	static FName ConvertName(const char* InString)
	{
		return FName(InString);
	}

	static FName ConvertName(const std::string& InString)
	{
		return FName(InString.c_str());
	}

	static FMatrix ConvertMatrix(const FUsdMatrixData& Matrix)
	{
		FMatrix UnrealMtx(
			FPlane(Matrix[0][0], Matrix[0][1], Matrix[0][2], Matrix[0][3]),
			FPlane(Matrix[1][0], Matrix[1][1], Matrix[1][2], Matrix[1][3]),
			FPlane(Matrix[2][0], Matrix[2][1], Matrix[2][2], Matrix[2][3]),
			FPlane(Matrix[3][0], Matrix[3][1], Matrix[3][2], Matrix[3][3])
		);

		return UnrealMtx;
	}
}

