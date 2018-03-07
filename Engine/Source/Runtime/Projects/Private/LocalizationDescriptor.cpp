// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "LocalizationDescriptor.h"
#include "Misc/App.h"
#include "Misc/ConfigCacheIni.h"
#include "Dom/JsonObject.h"

#define LOCTEXT_NAMESPACE "LocalizationDescriptor"

ELocalizationTargetDescriptorLoadingPolicy::Type ELocalizationTargetDescriptorLoadingPolicy::FromString(const TCHAR *String)
{
	ELocalizationTargetDescriptorLoadingPolicy::Type TestType = (ELocalizationTargetDescriptorLoadingPolicy::Type)0;
	for(; TestType < ELocalizationTargetDescriptorLoadingPolicy::Max; TestType = (ELocalizationTargetDescriptorLoadingPolicy::Type)(TestType + 1))
	{
		const TCHAR *TestString = ToString(TestType);
		if (FCString::Stricmp(String, TestString) == 0)
		{
			break;
		}
	}
	return TestType;
}

const TCHAR* ELocalizationTargetDescriptorLoadingPolicy::ToString(const ELocalizationTargetDescriptorLoadingPolicy::Type Value)
{
	switch (Value)
	{
	case Never:
		return TEXT("Never");

	case Always:
		return TEXT("Always");

	case Editor:
		return TEXT("Editor");

	case Game:
		return TEXT("Game");

	case PropertyNames:
		return TEXT("PropertyNames");

	case ToolTips:
		return TEXT("ToolTips");

	default:
		ensureMsgf(false, TEXT("ELocalizationTargetDescriptorLoadingPolicy::ToString - Unrecognized ELocalizationTargetDescriptorLoadingPolicy value: %i"), Value);
		return nullptr;
	}
}

FLocalizationTargetDescriptor::FLocalizationTargetDescriptor(FString InName, ELocalizationTargetDescriptorLoadingPolicy::Type InLoadingPolicy)
	: Name(MoveTemp(InName))
	, LoadingPolicy(InLoadingPolicy)
{
}

bool FLocalizationTargetDescriptor::Read(const FJsonObject& InObject, FText& OutFailReason)
{
	// Read the target name
	TSharedPtr<FJsonValue> NameValue = InObject.TryGetField(TEXT("Name"));
	if (!NameValue.IsValid() || NameValue->Type != EJson::String)
	{
		OutFailReason = LOCTEXT("TargetWithoutAName", "Found a 'Localization Target' entry with a missing 'Name' field");
		return false;
	}
	Name = NameValue->AsString();

	// Read the target loading policy
	TSharedPtr<FJsonValue> LoadingPolicyValue = InObject.TryGetField(TEXT("LoadingPolicy"));
	if (LoadingPolicyValue.IsValid() && LoadingPolicyValue->Type == EJson::String)
	{
		LoadingPolicy = ELocalizationTargetDescriptorLoadingPolicy::FromString(*LoadingPolicyValue->AsString());
		if (LoadingPolicy == ELocalizationTargetDescriptorLoadingPolicy::Max)
		{
			OutFailReason = FText::Format(LOCTEXT("TargetWithInvalidLoadingPolicy", "Localization Target entry '{0}' specified an unrecognized target LoadingPolicy '{1}'"), FText::FromString(Name), FText::FromString(LoadingPolicyValue->AsString()));
			return false;
		}
	}

	return true;
}

bool FLocalizationTargetDescriptor::ReadArray(const FJsonObject& InObject, const TCHAR* InName, TArray<FLocalizationTargetDescriptor>& OutTargets, FText& OutFailReason)
{
	bool bResult = true;

	TSharedPtr<FJsonValue> TargetsArrayValue = InObject.TryGetField(InName);
	if (TargetsArrayValue.IsValid() && TargetsArrayValue->Type == EJson::Array)
	{
		const TArray<TSharedPtr<FJsonValue>>& TargetsArray = TargetsArrayValue->AsArray();
		for (const TSharedPtr<FJsonValue>& TargetValue : TargetsArray)
		{
			if (TargetValue.IsValid() && TargetValue->Type == EJson::Object)
			{
				FLocalizationTargetDescriptor Descriptor;
				if (Descriptor.Read(*TargetValue->AsObject().Get(), OutFailReason))
				{
					OutTargets.Add(Descriptor);
				}
				else
				{
					bResult = false;
				}
			}
			else
			{
				OutFailReason = LOCTEXT("TargetWithInvalidTargetsArray", "The 'Localization Targets' array has invalid contents and was not able to be loaded.");
				bResult = false;
			}
		}
	}

	return bResult;
}

void FLocalizationTargetDescriptor::Write(TJsonWriter<>& InWriter) const
{
	InWriter.WriteObjectStart();
	InWriter.WriteValue(TEXT("Name"), Name);
	InWriter.WriteValue(TEXT("LoadingPolicy"), FString(ELocalizationTargetDescriptorLoadingPolicy::ToString(LoadingPolicy)));
	InWriter.WriteObjectEnd();
}

void FLocalizationTargetDescriptor::WriteArray(TJsonWriter<>& InWriter, const TCHAR* InName, const TArray<FLocalizationTargetDescriptor>& InTargets)
{
	if (InTargets.Num() > 0)
	{
		InWriter.WriteArrayStart(InName);
		for (const FLocalizationTargetDescriptor& Target : InTargets)
		{
			Target.Write(InWriter);
		}
		InWriter.WriteArrayEnd();
	}
}

bool FLocalizationTargetDescriptor::ShouldLoadLocalizationTarget() const
{
	switch (LoadingPolicy)
	{
	case ELocalizationTargetDescriptorLoadingPolicy::Never:
		return false;

	case ELocalizationTargetDescriptorLoadingPolicy::Always:
		return true;

	case ELocalizationTargetDescriptorLoadingPolicy::Editor:
		return WITH_EDITOR;

	case ELocalizationTargetDescriptorLoadingPolicy::Game:
		return FApp::IsGame();

	case ELocalizationTargetDescriptorLoadingPolicy::PropertyNames:
#if WITH_EDITOR
		{
			bool bShouldLoadLocalizedPropertyNames = true;
			if (!GConfig->GetBool(TEXT("Internationalization"), TEXT("ShouldLoadLocalizedPropertyNames"), bShouldLoadLocalizedPropertyNames, GEditorSettingsIni))
			{
				GConfig->GetBool(TEXT("Internationalization"), TEXT("ShouldLoadLocalizedPropertyNames"), bShouldLoadLocalizedPropertyNames, GEngineIni);
			}
			return bShouldLoadLocalizedPropertyNames;
		}
#else	// WITH_EDITOR
		return false;
#endif	// WITH_EDITOR

	case ELocalizationTargetDescriptorLoadingPolicy::ToolTips:
		return WITH_EDITOR;

	default:
		ensureMsgf(false, TEXT("FLocalizationTargetDescriptor::ShouldLoadLocalizationTarget - Unrecognized ELocalizationTargetDescriptorLoadingPolicy value: %i"), LoadingPolicy);
		return false;
	}
}

#undef LOCTEXT_NAMESPACE
