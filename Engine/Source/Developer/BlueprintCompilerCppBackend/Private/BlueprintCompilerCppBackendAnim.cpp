// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "BlueprintCompilerCppBackendUtils.h"
#include "Animation/AnimClassData.h"

void FBackendHelperAnim::AddHeaders(FEmitterLocalContext& Context)
{
	if (Cast<UAnimBlueprintGeneratedClass>(Context.GetCurrentlyGeneratedClass()))
	{
		Context.Header.AddLine(TEXT("#include \"Animation/AnimClassData.h\""));
		Context.Body.AddLine(TEXT("#include \"Animation/BlendProfile.h\""));
	}
}

void FBackendHelperAnim::CreateAnimClassData(FEmitterLocalContext& Context)
{
	if (auto AnimClass = Cast<UAnimBlueprintGeneratedClass>(Context.GetCurrentlyGeneratedClass()))
	{
		const FString LocalNativeName = Context.GenerateUniqueLocalName();
		Context.AddLine(FString::Printf(TEXT("auto %s = NewObject<UAnimClassData>(InDynamicClass, TEXT(\"AnimClassData\"));"), *LocalNativeName));

		auto AnimClassData = NewObject<UAnimClassData>(GetTransientPackage(), TEXT("AnimClassData"));
		AnimClassData->CopyFrom(AnimClass);

		auto ObjectArchetype = AnimClassData->GetArchetype();
		for (auto Property : TFieldRange<const UProperty>(UAnimClassData::StaticClass()))
		{
			FEmitDefaultValueHelper::OuterGenerate(Context, Property, LocalNativeName
				, reinterpret_cast<const uint8*>(AnimClassData)
				, reinterpret_cast<const uint8*>(ObjectArchetype)
				, FEmitDefaultValueHelper::EPropertyAccessOperator::Pointer);
		}

		Context.AddLine(FString::Printf(TEXT("InDynamicClass->%s = %s;"), GET_MEMBER_NAME_STRING_CHECKED(UDynamicClass, AnimClassImplementation), *LocalNativeName));
	}
}

