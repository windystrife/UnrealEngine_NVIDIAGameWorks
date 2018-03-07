// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "AndroidJSScripting.h"
#include "JsonStructDeserializerBackend.h"
#include "Serialization/MemoryReader.h"

class FAndroidJSStructDeserializerBackend
	: public FJsonStructDeserializerBackend
{
public:
	FAndroidJSStructDeserializerBackend(FAndroidJSScriptingRef InScripting, const FString& JsonString);

	virtual bool ReadProperty( UProperty* Property, UProperty* Outer, void* Data, int32 ArrayIndex ) override;

private:
	FAndroidJSScriptingRef Scripting;
	TArray<uint8> JsonData;
	FMemoryReader Reader;
};
