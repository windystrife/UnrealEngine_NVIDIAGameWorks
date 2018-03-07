// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sound/DialogueWave.h"
#include "Internationalization/InternationalizationMetadata.h"
#include "Internationalization/GatherableTextData.h"
#include "Serialization/PropertyLocalizationDataGathering.h"
#include "Engine/EngineTypes.h"
#include "Engine/Engine.h"
#include "ActiveSound.h"
#include "Sound/AudioSettings.h"
#include "Sound/SoundWave.h"
#include "Sound/DialogueSoundWaveProxy.h"
#include "Sound/DialogueVoice.h"
#include "SubtitleManager.h"

const FString FDialogueConstants::DialogueNamespace						= TEXT("Dialogue");
const FString FDialogueConstants::DialogueNotesNamespace				= TEXT("DialogueNotes");
const FString FDialogueConstants::SubtitleKeySuffix						= TEXT("_Subtitle");
#if WITH_EDITORONLY_DATA
const FString FDialogueConstants::ActingDirectionKeySuffix				= TEXT("_ActingDirection");
const FString FDialogueConstants::PropertyName_AudioFile				= TEXT("AudioFile");
const FString FDialogueConstants::PropertyName_VoiceActorDirection		= TEXT("VoiceActorDirection");
const FString FDialogueConstants::PropertyName_Speaker					= TEXT("Speaker");
const FString FDialogueConstants::PropertyName_Targets					= TEXT("Targets");
const FString FDialogueConstants::PropertyName_GrammaticalGender		= TEXT("Gender");
const FString FDialogueConstants::PropertyName_GrammaticalPlurality		= TEXT("Plurality");
const FString FDialogueConstants::PropertyName_TargetGrammaticalGender	= TEXT("TargetGender");
const FString FDialogueConstants::PropertyName_TargetGrammaticalNumber	= TEXT("TargetPlurality");
const FString FDialogueConstants::PropertyName_DialogueContext			= TEXT("Context");
const FString FDialogueConstants::PropertyName_IsMature					= FLocMetadataObject::COMPARISON_MODIFIER_PREFIX + TEXT("IsMature");
#endif //WITH_EDITORONLY_DATA

#if WITH_EDITORONLY_DATA
namespace
{
	// Class that takes DialogueWaves and prepares internationalization manifest entries 
	class FDialogueHelper
	{
	public:
		bool ProcessDialogueWave(const UDialogueWave* DialogueWave);
		const TArray<FTextSourceSiteContext>& GetContextSpecificVariations() const { return ContextSpecificVariations; }

		static void SetMetaDataFromContext(const UDialogueWave* DialogueWave, const FDialogueContextMapping& ContextMapping, FLocMetadataObject& OutInfoMetaData, FLocMetadataObject& OutKeyMetaData);

	private:
		static TSharedPtr<FLocMetadataValue> GetVoicesMetadata(const FString& SpeakerName, const TArray<FString>& TargetNames, bool bCompact = true);
		static FString GetDialogueVoiceName(const UDialogueVoice* DialogueVoice);
		static FString GetGrammaticalGenderString(const EGrammaticalGender::Type Gender);
		static FString GetGrammaticalNumberString(const EGrammaticalNumber::Type Plurality);

	private:
		// Context specific entries
		TArray<FTextSourceSiteContext> ContextSpecificVariations;
	};

	bool FDialogueHelper::ProcessDialogueWave(const UDialogueWave* DialogueWave)
	{
		if (!DialogueWave)
		{
			return false;
		}

		const FString SourceLocation = DialogueWave->GetPathName();

		for (const FDialogueContextMapping& ContextMapping : DialogueWave->ContextMappings)
		{
			const FDialogueContext& DialogueContext = ContextMapping.Context;

			// Skip over entries with invalid speaker
			if (!DialogueContext.Speaker)
			{
				continue;
			}

			FTextSourceSiteContext& Context = ContextSpecificVariations[ContextSpecificVariations.AddDefaulted()];

			// Setup the variation context
			Context.KeyName = DialogueWave->GetContextLocalizationKey(ContextMapping);
			Context.SiteDescription = SourceLocation;
			Context.IsOptional = false;
			SetMetaDataFromContext(DialogueWave, ContextMapping, Context.InfoMetaData, Context.KeyMetaData);
		}

		return true;
	}

	void FDialogueHelper::SetMetaDataFromContext(const UDialogueWave* DialogueWave, const FDialogueContextMapping& ContextMapping, FLocMetadataObject& OutInfoMetaData, FLocMetadataObject& OutKeyMetaData)
	{
		const FDialogueContext& DialogueContext = ContextMapping.Context;
		const UDialogueVoice* SpeakerDialogueVoice = DialogueContext.Speaker;

		check(SpeakerDialogueVoice);

		// Collect speaker info
		const FString SpeakerDisplayName = GetDialogueVoiceName(SpeakerDialogueVoice);
		const FString SpeakerGender = GetGrammaticalGenderString(SpeakerDialogueVoice->Gender);
		const FString SpeakerPlurality = GetGrammaticalNumberString(SpeakerDialogueVoice->Plurality);
		const FString SpeakerGuid = SpeakerDialogueVoice->LocalizationGUID.ToString();

		TOptional<EGrammaticalGender::Type> AccumulatedTargetGender;
		TOptional<EGrammaticalNumber::Type> AccumulatedTargetPlurality;

		TArray<FString> TargetGuidsList;
		TArray<FString> TargetDisplayNameList;

		// Collect info on all the targets
		for (const UDialogueVoice* TargetDialogueVoice : DialogueContext.Targets)
		{
			if (TargetDialogueVoice)
			{
				const FString TargetDisplayName = GetDialogueVoiceName(TargetDialogueVoice);
				const FString TargetGender = GetGrammaticalGenderString(TargetDialogueVoice->Gender);
				const FString TargetPlurality = GetGrammaticalNumberString(TargetDialogueVoice->Plurality);
				const FString TargetGuid = TargetDialogueVoice->LocalizationGUID.ToString();

				TargetDisplayNameList.AddUnique(TargetDisplayName);
				TargetGuidsList.AddUnique(TargetGuid);

				if (!AccumulatedTargetGender.IsSet())
				{
					AccumulatedTargetGender = TargetDialogueVoice->Gender;
				}
				else if (AccumulatedTargetGender.GetValue() != TargetDialogueVoice->Gender)
				{
					AccumulatedTargetGender = EGrammaticalGender::Mixed;
				}

				if (!AccumulatedTargetPlurality.IsSet())
				{
					AccumulatedTargetPlurality = TargetDialogueVoice->Plurality;
				}
				else if (AccumulatedTargetPlurality.GetValue() == EGrammaticalNumber::Singular)
				{
					AccumulatedTargetPlurality = EGrammaticalNumber::Plural;
				}
			}
		}

		const FString FinalTargetGender = AccumulatedTargetGender.IsSet() ? GetGrammaticalGenderString(AccumulatedTargetGender.GetValue()) : TEXT("");
		const FString FinalTargetPlurality = AccumulatedTargetPlurality.IsSet() ? GetGrammaticalNumberString(AccumulatedTargetPlurality.GetValue()) : TEXT("");

		// Setup a loc metadata object with all the context specific keys.
		{
			if (!SpeakerGender.IsEmpty())
			{
				OutKeyMetaData.SetStringField(FDialogueConstants::PropertyName_GrammaticalGender, SpeakerGender);
			}

			if (!SpeakerPlurality.IsEmpty())
			{
				OutKeyMetaData.SetStringField(FDialogueConstants::PropertyName_GrammaticalPlurality, SpeakerPlurality);
			}

			if (!SpeakerGuid.IsEmpty())
			{
				OutKeyMetaData.SetStringField(FDialogueConstants::PropertyName_Speaker, SpeakerGuid);
			}

			if (!FinalTargetGender.IsEmpty())
			{
				OutKeyMetaData.SetStringField(FDialogueConstants::PropertyName_TargetGrammaticalGender, FinalTargetGender);
			}

			if (!FinalTargetPlurality.IsEmpty())
			{
				OutKeyMetaData.SetStringField(FDialogueConstants::PropertyName_TargetGrammaticalNumber, FinalTargetPlurality);
			}

			{
				TArray<TSharedPtr<FLocMetadataValue>> TargetGuidsMetadata;
				for (const FString& TargetGuid : TargetGuidsList)
				{
					TargetGuidsMetadata.Add(MakeShareable(new FLocMetadataValueString(TargetGuid)));
				}

				if (TargetGuidsMetadata.Num() > 0)
				{
					OutKeyMetaData.SetArrayField(FDialogueConstants::PropertyName_Targets, TargetGuidsMetadata);
				}
			}
		}

		// Setup a loc metadata object with all the context specific info.  This usually includes human readable descriptions of the dialogue
		{
			// Create the human readable info that describes the source and target voices of this dialogue
			{
				TSharedPtr<FLocMetadataValue> VoicesMetadata = GetVoicesMetadata(SpeakerDisplayName, TargetDisplayNameList);
				if (VoicesMetadata.IsValid())
				{
					OutInfoMetaData.SetField(FDialogueConstants::PropertyName_DialogueContext, VoicesMetadata);
				}
			}

			if (!DialogueWave->VoiceActorDirection.IsEmpty())
			{
				OutInfoMetaData.SetStringField(FDialogueConstants::PropertyName_VoiceActorDirection, DialogueWave->VoiceActorDirection);
			}

			{
				const FString AudioFile = DialogueWave->GetContextRecordedAudioFilename(ContextMapping);
				if (!AudioFile.IsEmpty())
				{
					OutInfoMetaData.SetStringField(FDialogueConstants::PropertyName_AudioFile, AudioFile);
				}
			}

			//OutInfoMetaData.SetBoolField(FDialogueConstants::PropertyName_IsMature, DialogueWave->bIsMature);
		}
	}

	TSharedPtr<FLocMetadataValue> FDialogueHelper::GetVoicesMetadata(const FString& SpeakerName, const TArray<FString>& TargetNames, bool bCompact)
	{
		/*
		This function can support two different formats.

		The first format is compact and results in string entries that will later be combined into something like this
		"Variations": [
			"Jenny -> Audience",
			"Zak -> Audience"
		]

		The second format is verbose and results in object entries that will later be combined into something like this
		"VariationsTest": [
			{
				"Speaker": "Jenny",
				"Targets": [
					"Audience"
					]
			},
			{
				"Speaker": "Zak",
				"Targets": [
					"Audience"
					]
			}
		]
		*/

		TSharedPtr<FLocMetadataValue> Result;
		if (bCompact)
		{
			TArray<FString> SortedTargetNames = TargetNames;
			SortedTargetNames.Sort();
			FString TargetNamesString;
			for (const FString& StrEntry : SortedTargetNames)
			{
				if (TargetNamesString.Len())
				{
					TargetNamesString += TEXT(",");
				}
				TargetNamesString += StrEntry;
			}
			Result = MakeShareable(new FLocMetadataValueString(FString::Printf(TEXT("%s -> %s"), *SpeakerName, *TargetNamesString)));
		}
		else
		{
			TArray<TSharedPtr<FLocMetadataValue>> TargetNamesMetadataList;
			for (const FString& StrEntry : TargetNames)
			{
				TargetNamesMetadataList.Add(MakeShareable(new FLocMetadataValueString(StrEntry)));
			}

			TSharedPtr<FLocMetadataObject> MetadataObj = MakeShareable(new FLocMetadataObject());
			MetadataObj->SetStringField(FDialogueConstants::PropertyName_Speaker, SpeakerName);
			MetadataObj->SetArrayField(FDialogueConstants::PropertyName_Targets, TargetNamesMetadataList);

			Result = MakeShareable(new FLocMetadataValueObject(MetadataObj.ToSharedRef()));
		}
		return Result;
	}

	FString FDialogueHelper::GetDialogueVoiceName(const UDialogueVoice* DialogueVoice)
	{
		return DialogueVoice->GetName();
	}

	FString FDialogueHelper::GetGrammaticalGenderString(const EGrammaticalGender::Type Gender)
	{
		switch (Gender)
		{
		case EGrammaticalGender::Neuter:
			return TEXT("Neuter");
		case EGrammaticalGender::Masculine:
			return TEXT("Masculine");
		case EGrammaticalGender::Feminine:
			return TEXT("Feminine");
		case EGrammaticalGender::Mixed:
			return TEXT("Mixed");
		default:
			return FString();
		}
	}

	FString FDialogueHelper::GetGrammaticalNumberString(const EGrammaticalNumber::Type Plurality)
	{
		switch (Plurality)
		{
		case EGrammaticalNumber::Singular:
			return TEXT("Singular");
		case EGrammaticalNumber::Plural:
			return TEXT("Plural");
		default:
			return FString();
		}
	}

	void GatherDialogueWaveForLocalization(const UObject* const Object, FPropertyLocalizationDataGatherer& PropertyLocalizationDataGatherer, const EPropertyLocalizationGathererTextFlags GatherTextFlags)
	{
		const UDialogueWave* const DialogueWave = CastChecked<UDialogueWave>(Object);

		PropertyLocalizationDataGatherer.GatherLocalizationDataFromObject(DialogueWave, GatherTextFlags);

		FDialogueHelper DialogueHelper;
		if (DialogueHelper.ProcessDialogueWave(DialogueWave))
		{
			auto FindOrAddDialogueTextData = [&PropertyLocalizationDataGatherer](const FString& InText, const FString& InNamespace) -> FGatherableTextData&
			{
				check(!InText.IsEmpty());

				FTextSourceData SourceData;
				SourceData.SourceString = InText;

				auto& GatherableTextDataArray = PropertyLocalizationDataGatherer.GetGatherableTextDataArray();
				FGatherableTextData* GatherableTextData = GatherableTextDataArray.FindByPredicate([&](const FGatherableTextData& Candidate)
				{
					return Candidate.NamespaceName.Equals(InNamespace, ESearchCase::CaseSensitive)
						&& Candidate.SourceData.SourceString.Equals(SourceData.SourceString, ESearchCase::CaseSensitive)
						&& Candidate.SourceData.SourceStringMetaData == SourceData.SourceStringMetaData;
				});
				if (!GatherableTextData)
				{
					GatherableTextData = &GatherableTextDataArray[GatherableTextDataArray.AddDefaulted()];
					GatherableTextData->NamespaceName = InNamespace;
					GatherableTextData->SourceData = SourceData;
				}

				return *GatherableTextData;
			};

			// Gather the Spoken Text for each context
			if (!DialogueWave->SpokenText.IsEmpty())
			{
				FGatherableTextData& GatherableTextData = FindOrAddDialogueTextData(DialogueWave->SpokenText, FDialogueConstants::DialogueNamespace);

				const TArray<FTextSourceSiteContext>& Variations = DialogueHelper.GetContextSpecificVariations();
				for (const FTextSourceSiteContext& Variation : Variations)
				{
					GatherableTextData.SourceSiteContexts.Add(Variation);
				}
			}

			// Gather the Subtitle Override for each context
			if (!DialogueWave->SubtitleOverride.IsEmpty())
			{
				FGatherableTextData& GatherableTextData = FindOrAddDialogueTextData(DialogueWave->SubtitleOverride, FDialogueConstants::DialogueNamespace);

				const TArray<FTextSourceSiteContext>& Variations = DialogueHelper.GetContextSpecificVariations();
				for (const FTextSourceSiteContext& Variation : Variations)
				{
					FTextSourceSiteContext SubtitleVariation = Variation;
					SubtitleVariation.KeyName += FDialogueConstants::SubtitleKeySuffix;
					SubtitleVariation.InfoMetaData.RemoveField(FDialogueConstants::PropertyName_AudioFile);
					GatherableTextData.SourceSiteContexts.Add(SubtitleVariation);
				}
			}

			// Gather the Voice Acting Direction
			if (!DialogueWave->VoiceActorDirection.IsEmpty())
			{
				FGatherableTextData& GatherableTextData = FindOrAddDialogueTextData(DialogueWave->VoiceActorDirection, FDialogueConstants::DialogueNotesNamespace);

				FTextSourceSiteContext& SourceSiteContext = GatherableTextData.SourceSiteContexts[GatherableTextData.SourceSiteContexts.AddDefaulted()];
				SourceSiteContext.KeyName = DialogueWave->LocalizationGUID.ToString() + FDialogueConstants::ActingDirectionKeySuffix;
				SourceSiteContext.SiteDescription = DialogueWave->GetPathName();
				SourceSiteContext.IsEditorOnly = true;
				SourceSiteContext.IsOptional = false;
			}
		}
	}
}
#endif //WITH_EDITORONLY_DATA

bool operator==(const FDialogueContextMapping& LHS, const FDialogueContextMapping& RHS)
{
	return	LHS.Context == RHS.Context &&
		LHS.SoundWave == RHS.SoundWave;
}

bool operator!=(const FDialogueContextMapping& LHS, const FDialogueContextMapping& RHS)
{
	return	LHS.Context != RHS.Context ||
		LHS.SoundWave != RHS.SoundWave;
}

FDialogueContextMapping::FDialogueContextMapping()
	: SoundWave(nullptr)
	, LocalizationKeyFormat(TEXT("{ContextHash}"))
	, Proxy(nullptr)
{

}

FString FDialogueContextMapping::GetLocalizationKey() const
{
	TMap<FString, FStringFormatArg> Args;
	Args.Add(TEXT("ContextHash"), Context.GetContextHash());
	return FString::Format(*LocalizationKeyFormat, Args);
}

FString FDialogueContextMapping::GetLocalizationKey(const FString& InOwnerDialogueWaveKey) const
{
	const FString ContextKey = GetLocalizationKey();
	return FString::Printf(TEXT("%s_%s"), *InOwnerDialogueWaveKey, *ContextKey);
}

UDialogueSoundWaveProxy::UDialogueSoundWaveProxy(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UDialogueSoundWaveProxy::IsPlayable() const
{
	return SoundWave->IsPlayable();
}

const FSoundAttenuationSettings* UDialogueSoundWaveProxy::GetAttenuationSettingsToApply() const
{
	return SoundWave->GetAttenuationSettingsToApply();
}

float UDialogueSoundWaveProxy::GetMaxAudibleDistance()
{
	return SoundWave->GetMaxAudibleDistance();
}

float UDialogueSoundWaveProxy::GetDuration()
{
	return SoundWave->GetDuration();
}

float UDialogueSoundWaveProxy::GetVolumeMultiplier()
{
	return SoundWave->GetVolumeMultiplier();
}

float UDialogueSoundWaveProxy::GetPitchMultiplier()
{
	return SoundWave->GetPitchMultiplier();
}

void UDialogueSoundWaveProxy::Parse(class FAudioDevice* AudioDevice, const UPTRINT NodeWaveInstanceHash, FActiveSound& ActiveSound, const FSoundParseParameters& ParseParams, TArray<FWaveInstance*>& WaveInstances)
{
	int OldWaveInstanceCount = WaveInstances.Num();
	bool bHasSubtitles = (Subtitles.Num() > 0);

	ActiveSound.bHasExternalSubtitles = bHasSubtitles; // Need to set this so the sound will virtualize when silent if necessary.
	SoundWave->Parse(AudioDevice, NodeWaveInstanceHash, ActiveSound, ParseParams, WaveInstances);
	int NewWaveInstanceCount = WaveInstances.Num();

	FWaveInstance* NewWaveInstance = nullptr;
	if (NewWaveInstanceCount == OldWaveInstanceCount + 1)
	{
		NewWaveInstance = WaveInstances[WaveInstances.Num() - 1];
	}

	// Only Queue subtitles once per each playback of a wave instance
	if (NewWaveInstance != nullptr && NewWaveInstance != CurrentWaveInstance)
	{
		CurrentWaveInstance = NewWaveInstance;
		// Add in the subtitle if they exist
		if (ActiveSound.bHandleSubtitles && bHasSubtitles)
		{
			FQueueSubtitleParams QueueSubtitleParams(Subtitles);
			{
				QueueSubtitleParams.AudioComponentID = ActiveSound.GetAudioComponentID();
				QueueSubtitleParams.WorldPtr = ActiveSound.GetWeakWorld();
				QueueSubtitleParams.WaveInstance = (PTRINT)CurrentWaveInstance;
				QueueSubtitleParams.SubtitlePriority = ActiveSound.SubtitlePriority;
				QueueSubtitleParams.Duration = GetDuration();
				QueueSubtitleParams.bManualWordWrap = false;
				QueueSubtitleParams.bSingleLine = false;
				QueueSubtitleParams.RequestedStartTime = ActiveSound.RequestedStartTime;
			}

			FSubtitleManager::QueueSubtitles(QueueSubtitleParams);
		}
	}
}

USoundClass* UDialogueSoundWaveProxy::GetSoundClass() const
{
	return SoundWave->GetSoundClass();
}

UDialogueWave::UDialogueWave(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, LocalizationGUID(FGuid::NewGuid())
{
#if WITH_EDITORONLY_DATA
	{ static const FAutoRegisterLocalizationDataGatheringCallback AutomaticRegistrationOfLocalizationGatherer(UDialogueWave::StaticClass(), &GatherDialogueWaveForLocalization); }
#endif

	bOverride_SubtitleOverride = false;
	ContextMappings.Add(FDialogueContextMapping());
}

// Begin UObject interface. 
void UDialogueWave::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar.ThisRequiresLocalizationGather();

	bool bCooked = Ar.IsCooking();
	Ar << bCooked;

	if (FPlatformProperties::RequiresCookedData() && !bCooked && Ar.IsLoading())
	{
		UE_LOG(LogAudio, Fatal, TEXT("This platform requires cooked packages, and audio data was not cooked into %s."), *GetFullName());
	}
}

bool UDialogueWave::IsReadyForFinishDestroy()
{
	return true;
}

FString UDialogueWave::GetDesc()
{
	return FString::Printf(TEXT("Dialogue Wave Description"));
}

void UDialogueWave::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);
}

void UDialogueWave::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
	if (!bDuplicateForPIE)
	{
		LocalizationGUID = FGuid::NewGuid();
	}
}

void UDialogueWave::PostLoad()
{
	Super::PostLoad();

	for (FDialogueContextMapping& ContextMapping : ContextMappings)
	{
		UpdateMappingProxy(ContextMapping);
	}
}

#if WITH_EDITOR
void UDialogueWave::PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent)
{
	Super::PostEditChangeChainProperty(PropertyChangedEvent);

	int32 NewArrayIndex = PropertyChangedEvent.GetArrayIndex(TEXT("ContextMappings"));

	if (ContextMappings.IsValidIndex(NewArrayIndex))
	{
		UpdateMappingProxy(ContextMappings[NewArrayIndex]);
	}
	else if (PropertyChangedEvent.Property && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UDialogueWave, SpokenText))
	{
		for (FDialogueContextMapping& ContextMapping : ContextMappings)
		{
			UpdateMappingProxy(ContextMapping);
		}
	}
}
#endif

// End UObject interface. 

// Begin UDialogueWave interface.
bool UDialogueWave::SupportsContext(const FDialogueContext& Context) const
{
	for (const FDialogueContextMapping& ContextMapping : ContextMappings)
	{
		if (ContextMapping.Context == Context)
		{
			return true;
		}
	}

	return false;
}

USoundBase* UDialogueWave::GetWaveFromContext(const FDialogueContext& Context) const
{
	if (Context.Speaker == nullptr)
	{
		UE_LOG(LogAudio, Warning, TEXT("UDialogueWave::GetWaveFromContext requires a Context.Speaker (%s)."), *GetPathName());
		return nullptr;
	}

	UDialogueSoundWaveProxy* Proxy = nullptr;
	for (const FDialogueContextMapping& ContextMapping : ContextMappings)
	{
		if (ContextMapping.Context == Context)
		{
			Proxy = ContextMapping.Proxy;
			break;
		}
	}

	return Proxy;
}

USoundBase* UDialogueWave::GetWaveFromContext(const FDialogueContextMapping& ContextMapping) const
{
	if (ContextMapping.Context.Speaker == nullptr)
	{
		UE_LOG(LogAudio, Warning, TEXT("UDialogueWave::GetWaveFromContext requires a Context.Speaker (%s)."), *GetPathName());
		return nullptr;
	}

	return ContextMapping.Proxy;
}

FString UDialogueWave::GetContextLocalizationKey(const FDialogueContext& Context) const
{
	for (const FDialogueContextMapping& ContextMapping : ContextMappings)
	{
		if (ContextMapping.Context == Context)
		{
			return GetContextLocalizationKey(ContextMapping);
		}
	}
	return FString();
}

FString UDialogueWave::GetContextLocalizationKey(const FDialogueContextMapping& ContextMapping) const
{
	return ContextMapping.GetLocalizationKey(LocalizationGUID.ToString());
}

FString UDialogueWave::GetContextRecordedAudioFilename(const FDialogueContext& Context) const
{
	for (const FDialogueContextMapping& ContextMapping : ContextMappings)
	{
		if (ContextMapping.Context == Context)
		{
			return GetContextRecordedAudioFilename(ContextMapping);
		}
	}
	return FString();
}

FString UDialogueWave::GetContextRecordedAudioFilename(const FDialogueContextMapping& ContextMapping) const
{
	const UAudioSettings* AudioSettings = GetDefault<UAudioSettings>();
	const FString DialogueContextFilename = BuildRecordedAudioFilename(AudioSettings->DialogueFilenameFormat, LocalizationGUID, GetName(), ContextMapping.GetLocalizationKey(), ContextMappings.IndexOfByKey(ContextMapping));
	return FString::Printf(TEXT("%s.wav"), *DialogueContextFilename);
}

FString UDialogueWave::BuildRecordedAudioFilename(const FString& FormatString, const FGuid& DialogueGuid, const FString& DialogueName, const FString& ContextId, const int32 ContextIndex)
{
	static const FString DialogueWaveSuffix = TEXT("_DialogueWave");
	static const FString DialogueSuffix = TEXT("_Dialogue");

	const FString DialogueHash = FString::Printf(TEXT("%08X"), FCrc::MemCrc32(&DialogueGuid, sizeof(FGuid)));

	// Trim the asset name if it ends with a common leaf
	FString TrimmedDialogueName = DialogueName;
	if (!TrimmedDialogueName.RemoveFromEnd(DialogueWaveSuffix))
	{
		TrimmedDialogueName.RemoveFromEnd(DialogueSuffix);
	}

	TMap<FString, FStringFormatArg> Args;
	Args.Add(TEXT("DialogueGuid"), DialogueGuid.ToString());
	Args.Add(TEXT("DialogueHash"), DialogueHash);
	Args.Add(TEXT("DialogueName"), TrimmedDialogueName);
	Args.Add(TEXT("ContextId"), ContextId);
	Args.Add(TEXT("ContextIndex"), ContextIndex);
	
	return FString::Format(*FormatString, Args);
}

// End UDialogueWave interface.

void UDialogueWave::UpdateContext(FDialogueContextMapping& ContextMapping, USoundWave* SoundWave, UDialogueVoice* Speaker, const TArray<UDialogueVoice*>& Targets)
{
	ContextMapping.SoundWave = SoundWave;
	ContextMapping.Context.Speaker = Speaker;
	ContextMapping.Context.Targets = Targets;

	UpdateMappingProxy(ContextMapping);
}

void UDialogueWave::UpdateMappingProxy(FDialogueContextMapping& ContextMapping)
{
	if (ContextMapping.SoundWave)
	{
		if (!ContextMapping.Proxy)
		{
			ContextMapping.Proxy = NewObject<UDialogueSoundWaveProxy>();
		}
	}
	else
	{
		ContextMapping.Proxy = nullptr;
	}

	if (ContextMapping.Proxy)
	{
		// Copy the properties that the proxy shares with the sound in case it's used as a SoundBase
		ContextMapping.Proxy->SoundWave = ContextMapping.SoundWave;
		UEngine::CopyPropertiesForUnrelatedObjects(ContextMapping.SoundWave, ContextMapping.Proxy);

		FSubtitleCue NewSubtitleCue;

		// Do we have a subtitle override?
		FString Key = GetContextLocalizationKey(ContextMapping);
		if (bOverride_SubtitleOverride)
		{
			Key += FDialogueConstants::SubtitleKeySuffix;
		}

		// First try and find a context specific localization
		if (!FText::FindText(FDialogueConstants::DialogueNamespace, Key, /*OUT*/NewSubtitleCue.Text))
		{
			// Failing that, try and find a general dialogue wave localization
			Key = LocalizationGUID.ToString();
			if (bOverride_SubtitleOverride)
			{
				Key += FDialogueConstants::SubtitleKeySuffix;
			}

			if (!FText::FindText(FDialogueConstants::DialogueNamespace, Key, /*OUT*/NewSubtitleCue.Text))
			{
				NewSubtitleCue.Text = bOverride_SubtitleOverride ? FText::FromString(SubtitleOverride) : FText::FromString(SpokenText);
			}
		}
		NewSubtitleCue.Time = 0.0f;
		ContextMapping.Proxy->Subtitles.Empty();
		ContextMapping.Proxy->Subtitles.Add(NewSubtitleCue);
	}
}
