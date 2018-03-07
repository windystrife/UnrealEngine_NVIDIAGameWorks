// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CustomBuildSteps.h"
#include "Dom/JsonObject.h"

#define LOCTEXT_NAMESPACE "ModuleDescriptor"

bool FCustomBuildSteps::IsEmpty() const
{
	return HostPlatformToCommands.Num() == 0;
}

void FCustomBuildSteps::Read(const FJsonObject& Object, const FString& FieldName)
{
	TSharedPtr<FJsonValue> StepsValue = Object.TryGetField(FieldName);
	if(StepsValue.IsValid() && StepsValue->Type == EJson::Object)
	{
		const TSharedPtr<FJsonObject>& StepsObject = StepsValue->AsObject();
		for(const TPair<FString, TSharedPtr<FJsonValue>>& HostPlatformAndSteps : StepsObject->Values)
		{
			TArray<FString>& Commands = HostPlatformToCommands.FindOrAdd(HostPlatformAndSteps.Key);
			if(HostPlatformAndSteps.Value.IsValid() && HostPlatformAndSteps.Value->Type == EJson::Array)
			{
				const TArray<TSharedPtr<FJsonValue>>& CommandsArray = HostPlatformAndSteps.Value->AsArray();
				for(const TSharedPtr<FJsonValue>& CommandValue: CommandsArray)
				{
					if(CommandValue->Type == EJson::String)
					{
						Commands.Add(CommandValue->AsString());
					}
				}
			}
		}
	}
}

void FCustomBuildSteps::Write(TJsonWriter<>& Writer, const FString& FieldName) const
{
	Writer.WriteObjectStart(FieldName);
	for(const TPair<FString, TArray<FString>>& HostPlatformAndCommands: HostPlatformToCommands)
	{
		Writer.WriteArrayStart(HostPlatformAndCommands.Key);
		for(const FString& Command : HostPlatformAndCommands.Value)
		{
			Writer.WriteValue(*Command);
		}
		Writer.WriteArrayEnd();
	}
	Writer.WriteObjectEnd();
}

#undef LOCTEXT_NAMESPACE

