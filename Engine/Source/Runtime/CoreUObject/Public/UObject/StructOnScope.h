// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "UObject/WeakObjectPtr.h"
#include "UObject/Package.h"
#include "Casts.h"

class FStructOnScope
{
protected:
	TWeakObjectPtr<const UStruct> ScriptStruct;
	uint8* SampleStructMemory;
	TWeakObjectPtr<UPackage> Package;

	FStructOnScope()
		: SampleStructMemory(nullptr)
		, OwnsMemory(false)
	{
	}

	void Initialize()
	{
		if (ScriptStruct.IsValid())
		{
			SampleStructMemory = (uint8*)FMemory::Malloc(ScriptStruct->GetStructureSize() ? ScriptStruct->GetStructureSize() : 1);
			ScriptStruct.Get()->InitializeStruct(SampleStructMemory);
			OwnsMemory = true;
		}
	}

public:

	FStructOnScope(const UStruct* InScriptStruct)
		: ScriptStruct(InScriptStruct)
		, SampleStructMemory(nullptr)
		, OwnsMemory(false)
	{
		Initialize();
	}

	FStructOnScope(const UStruct* InScriptStruct, uint8* InData)
		: ScriptStruct(InScriptStruct)
		, SampleStructMemory(InData)
		, OwnsMemory(false)
	{
	}

	virtual uint8* GetStructMemory()
	{
		return SampleStructMemory;
	}

	virtual const uint8* GetStructMemory() const
	{
		return SampleStructMemory;
	}

	virtual const UStruct* GetStruct() const
	{
		return ScriptStruct.Get();
	}

	virtual UPackage* GetPackage() const
	{
		return Package.Get();
	}

	virtual void SetPackage(UPackage* InPackage)
	{
		Package = InPackage;
	}

	virtual bool IsValid() const
	{
		return ScriptStruct.IsValid() && SampleStructMemory;
	}

	virtual void Destroy()
	{
		if (!OwnsMemory)
		{
			return;
		}

		if (ScriptStruct.IsValid() && SampleStructMemory)
		{
			ScriptStruct.Get()->DestroyStruct(SampleStructMemory);
			ScriptStruct = NULL;
		}

		if (SampleStructMemory)
		{
			FMemory::Free(SampleStructMemory);
			SampleStructMemory = NULL;
		}
	}

	virtual ~FStructOnScope()
	{
		Destroy();
	}

	/** Re-initializes the scope with a specified UStruct */
	void Initialize(TWeakObjectPtr<const UStruct> InScriptStruct)
	{
		ScriptStruct = InScriptStruct;
		Initialize();
	}

private:

	FStructOnScope(const FStructOnScope&);
	FStructOnScope& operator=(const FStructOnScope&);

private:

	/** Whether the struct memory is owned by this instance. */
	bool OwnsMemory;
};
