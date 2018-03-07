// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/UnrealString.h"

class FUnrealSourceFile;

enum class EHeaderProviderSourceType
{
	ClassName,
	FileName,
	Resolved,
	Invalid,
};

class FHeaderProvider
{
	friend bool operator==(const FHeaderProvider& A, const FHeaderProvider& B);
public:
	FHeaderProvider(EHeaderProviderSourceType Type, FString&& Id);
	FHeaderProvider()
		: Type(EHeaderProviderSourceType::Invalid)
		, Id(FString())
		, Cache(nullptr)
	{ }

	FUnrealSourceFile* Resolve();

	FString ToString() const;

	const FString& GetId() const;

	EHeaderProviderSourceType GetType() const;

private:
	EHeaderProviderSourceType Type;
	FString Id;
	FUnrealSourceFile* Cache;
};

bool operator==(const FHeaderProvider& A, const FHeaderProvider& B);
