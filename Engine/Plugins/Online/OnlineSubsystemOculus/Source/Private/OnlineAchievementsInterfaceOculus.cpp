// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "OnlineAchievementsInterfaceOculus.h"
#include "OnlineSubsystemOculusPrivate.h"
#include "OnlineIdentityOculus.h"
#include "OnlineMessageMultiTaskOculus.h"
#include "OnlineSubsystemOculusPackage.h"

class FOnlineMessageMultiTaskOculusWriteAchievements : public FOnlineMessageMultiTaskOculus
{
private:

	FUniqueNetIdOculus PlayerId;
	FOnlineAchievementsWriteRef WriteObject;
	FOnAchievementsWrittenDelegate AchievementDelegate;

PACKAGE_SCOPE:

	FOnlineMessageMultiTaskOculusWriteAchievements(
		FOnlineSubsystemOculus& InOculusSubsystem,
		const FUniqueNetIdOculus& InPlayerId,
		FOnlineAchievementsWriteRef& InWriteObject,
		const FOnAchievementsWrittenDelegate& InAchievementDelegate) :
		FOnlineMessageMultiTaskOculus(InOculusSubsystem, FOnlineMessageMultiTaskOculus::FFinalizeDelegate::CreateRaw(this, &FOnlineMessageMultiTaskOculusWriteAchievements::Finalize)),
		PlayerId(InPlayerId),
		WriteObject(InWriteObject),
		AchievementDelegate(InAchievementDelegate)
	{
	}

	void Finalize()
	{
		WriteObject->WriteState = (bDidAllRequestsFinishedSuccessfully) ? EOnlineAsyncTaskState::Done : EOnlineAsyncTaskState::Failed;
		AchievementDelegate.ExecuteIfBound(PlayerId, true);
	}
};

FOnlineAchievementsOculus::FOnlineAchievementsOculus(class FOnlineSubsystemOculus& InSubsystem)
: OculusSubsystem(InSubsystem)
{
}

void FOnlineAchievementsOculus::WriteAchievements(const FUniqueNetId& PlayerId, FOnlineAchievementsWriteRef& WriteObject, const FOnAchievementsWrittenDelegate& Delegate)
{
	if (AchievementDescriptions.Num() == 0)
	{
		// we don't have achievements
		WriteObject->WriteState = EOnlineAsyncTaskState::Failed;
		Delegate.ExecuteIfBound(PlayerId, false);
		return;
	}

	auto LoggedInPlayerId = OculusSubsystem.GetIdentityInterface()->GetUniquePlayerId(0);
	if (!(LoggedInPlayerId.IsValid() && PlayerId == *LoggedInPlayerId))
	{
		UE_LOG_ONLINE(Error, TEXT("Can only write achievements for logged in player id"));
		WriteObject->WriteState = EOnlineAsyncTaskState::Failed;
		Delegate.ExecuteIfBound(PlayerId, false);
		return;
	}

	// Nothing to write
	if (WriteObject->Properties.Num() == 0)
	{
		WriteObject->WriteState = EOnlineAsyncTaskState::Done;
		Delegate.ExecuteIfBound(PlayerId, true);
		return;
	}

	WriteObject->WriteState = EOnlineAsyncTaskState::InProgress;
	auto MultiTask = new FOnlineMessageMultiTaskOculusWriteAchievements(OculusSubsystem, static_cast<FUniqueNetIdOculus>(PlayerId), WriteObject, Delegate);

	// treat each achievement as unlocked
	for (FStatPropertyArray::TConstIterator It(WriteObject->Properties); It; ++It)
	{
		const FString AchievementId = It.Key().ToString();
		auto VariantData = It.Value();

		auto AchievementDesc = AchievementDescriptions.Find(AchievementId);
		if (AchievementDesc == nullptr)
		{
			WriteObject->WriteState = EOnlineAsyncTaskState::Failed;
			Delegate.ExecuteIfBound(PlayerId, false);
			return;
		}

		UE_LOG_ONLINE(Verbose, TEXT("WriteObject AchievementId: '%s'"), *AchievementId);

		ovrRequest RequestId = 0;

		switch (AchievementDesc->Type)
		{
			case EAchievementType::Simple:
			{
				RequestId = ovr_Achievements_Unlock(TCHAR_TO_ANSI(*AchievementId));
				break;
			}
			case EAchievementType::Count:
			{
				uint64 Count;
				GetWriteAchievementCountValue(VariantData, Count);
				RequestId = ovr_Achievements_AddCount(TCHAR_TO_ANSI(*AchievementId), Count);
				break;
			}
			case EAchievementType::Bitfield:
			{
				FString Bitfield;
				GetWriteAchievementBitfieldValue(VariantData, Bitfield, AchievementDesc->BitfieldLength);
				RequestId = ovr_Achievements_AddFields(TCHAR_TO_ANSI(*AchievementId), TCHAR_TO_ANSI(*Bitfield));
				break;
			}
			default:
			{
				UE_LOG_ONLINE(Warning, TEXT("Unknown achievement type"));
				break;
			}
		}

		if (RequestId != 0)
		{
			MultiTask->AddNewRequest(RequestId);
		}
	}
};

void FOnlineAchievementsOculus::QueryAchievements(const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate& Delegate)
{
	auto LoggedInPlayerId = OculusSubsystem.GetIdentityInterface()->GetUniquePlayerId(0);
	if (!(LoggedInPlayerId.IsValid() && PlayerId == *LoggedInPlayerId))
	{
		UE_LOG_ONLINE(Error, TEXT("Can only query for logged in player id"));
		Delegate.ExecuteIfBound(PlayerId, false);
		return;
	}

	auto OculusPlayerId = static_cast<FUniqueNetIdOculus>(PlayerId);
	OculusSubsystem.AddRequestDelegate(
		ovr_Achievements_GetAllProgress(),
		FOculusMessageOnCompleteDelegate::CreateLambda([this, OculusPlayerId, Delegate](ovrMessageHandle Message, bool bIsError)
	{
		if (bIsError)
		{
			Delegate.ExecuteIfBound(OculusPlayerId, false);
			return;
		}

		auto AchievementProgressArray = ovr_Message_GetAchievementProgressArray(Message);
		const size_t AchievementProgressNum = ovr_AchievementProgressArray_GetSize(AchievementProgressArray);

		// new array
		TArray<FOnlineAchievement> AchievementsForPlayer;

		// keep track of existing achievements
		TSet<FString> InProgressAchievements;

		for (size_t AchievementProgressIndex = 0; AchievementProgressIndex < AchievementProgressNum; ++AchievementProgressIndex)
		{
			auto AchievementProgress = ovr_AchievementProgressArray_GetElement(AchievementProgressArray, AchievementProgressIndex);
			FOnlineAchievementOculus NewAchievement(AchievementProgress);
			NewAchievement.Progress = CalculatePlayerAchievementProgress(NewAchievement);

			AchievementsForPlayer.Add(NewAchievement);
			InProgressAchievements.Add(NewAchievement.Id);
		}

		// if there are any achievements that the player has not made any progress toward,
		// fill them out as empty achievements
		for (auto const &it : AchievementDescriptions)
		{
			auto bFoundAchievement = InProgressAchievements.Find(it.Key);
			if (bFoundAchievement == nullptr)
			{
				FOnlineAchievementOculus NewAchievement(it.Value);
				AchievementsForPlayer.Add(NewAchievement);
			}
		}

		// should replace any already existing values
		PlayerAchievements.Add(OculusPlayerId, AchievementsForPlayer);

		Delegate.ExecuteIfBound(OculusPlayerId, true);
	}));
}

void FOnlineAchievementsOculus::QueryAchievementDescriptions(const FUniqueNetId& PlayerId, const FOnQueryAchievementsCompleteDelegate& Delegate)
{
	auto OculusPlayerId = static_cast<FUniqueNetIdOculus>(PlayerId);
	OculusSubsystem.AddRequestDelegate(
		ovr_Achievements_GetAllDefinitions(),
		FOculusMessageOnCompleteDelegate::CreateLambda([this, OculusPlayerId, Delegate](ovrMessageHandle Message, bool bIsError)
	{
		if (bIsError)
		{
			Delegate.ExecuteIfBound(OculusPlayerId, false);
			return;
		}

		auto AchievementDefinitionArray = ovr_Message_GetAchievementDefinitionArray(Message);
		const size_t AchievementDefinitionNum = ovr_AchievementDefinitionArray_GetSize(AchievementDefinitionArray);
		for (size_t AchievementDefinitionIndex = 0; AchievementDefinitionIndex < AchievementDefinitionNum; ++AchievementDefinitionIndex)
		{
			auto AchievementDefinition = ovr_AchievementDefinitionArray_GetElement(AchievementDefinitionArray, AchievementDefinitionIndex);
			FOnlineAchievementDescOculus NewAchievementDesc;
			FString Title(ovr_AchievementDefinition_GetName(AchievementDefinition));
			NewAchievementDesc.Title = FText::FromString(Title);
			NewAchievementDesc.bIsHidden = false;
			auto AchievementType = ovr_AchievementDefinition_GetType(AchievementDefinition);
			NewAchievementDesc.Type = static_cast<EAchievementType>(AchievementType);
			NewAchievementDesc.Target = ovr_AchievementDefinition_GetTarget(AchievementDefinition);
			NewAchievementDesc.BitfieldLength = ovr_AchievementDefinition_GetBitfieldLength(AchievementDefinition);

			AchievementDescriptions.Add(Title, NewAchievementDesc);
		}

		Delegate.ExecuteIfBound(OculusPlayerId, true);
	}));
}

EOnlineCachedResult::Type FOnlineAchievementsOculus::GetCachedAchievement(const FUniqueNetId& PlayerId, const FString& AchievementId, FOnlineAchievement& OutAchievement)
{
	if (AchievementDescriptions.Num() == 0)
	{
		// we don't have achievements
		return EOnlineCachedResult::NotFound;
	}

	auto OculusPlayerId = static_cast<const FUniqueNetIdOculus&>(PlayerId);
	const TArray<FOnlineAchievement> * PlayerAch = PlayerAchievements.Find(OculusPlayerId);
	if (PlayerAch == nullptr)
	{
		// achievements haven't been read for a player
		return EOnlineCachedResult::NotFound;
	}

	const int32 AchNum = PlayerAch->Num();
	for (int32 AchIdx = 0; AchIdx < AchNum; ++AchIdx)
	{
		if ((*PlayerAch)[AchIdx].Id == AchievementId)
		{
			OutAchievement = (*PlayerAch)[AchIdx];
			return EOnlineCachedResult::Success;
		}
	}

	// no such achievement
	return EOnlineCachedResult::NotFound;
};

EOnlineCachedResult::Type FOnlineAchievementsOculus::GetCachedAchievements(const FUniqueNetId& PlayerId, TArray<FOnlineAchievement> & OutAchievements)
{
	if (AchievementDescriptions.Num() == 0)
	{
		// we don't have achievements
		return EOnlineCachedResult::NotFound;
	}

	auto OculusPlayerId = static_cast<const FUniqueNetIdOculus&>(PlayerId);
	const TArray<FOnlineAchievement> * PlayerAch = PlayerAchievements.Find(OculusPlayerId);
	if (PlayerAch == nullptr)
	{
		// achievements haven't been read for a player
		return EOnlineCachedResult::NotFound;
	}

	OutAchievements = *PlayerAch;
	return EOnlineCachedResult::Success;
};

EOnlineCachedResult::Type FOnlineAchievementsOculus::GetCachedAchievementDescription(const FString& AchievementId, FOnlineAchievementDesc& OutAchievementDesc)
{
	if (AchievementDescriptions.Num() == 0)
	{
		// we don't have achievements
		return EOnlineCachedResult::NotFound;
	}

	FOnlineAchievementDesc * AchDesc = AchievementDescriptions.Find(AchievementId);
	if (AchDesc == nullptr)
	{
		// no such achievement
		return EOnlineCachedResult::NotFound;
	}

	OutAchievementDesc = *AchDesc;
	return EOnlineCachedResult::Success;
};

#if !UE_BUILD_SHIPPING
bool FOnlineAchievementsOculus::ResetAchievements(const FUniqueNetId& PlayerId)
{
	// We cannot reset achievements from the client
	UE_LOG_ONLINE(Error, TEXT("Achievements cannot be reset here"));
	return false;
};
#endif // !UE_BUILD_SHIPPING

void FOnlineAchievementsOculus::GetWriteAchievementCountValue(FVariantData VariantData, uint64& OutData) const
{
	switch (VariantData.GetType())
	{
		case EOnlineKeyValuePairDataType::Int32:
		{
			int32 Value;
			VariantData.GetValue(Value);
			OutData = static_cast<uint64>(Value);
			break;
		}
		case EOnlineKeyValuePairDataType::Int64:
		{
			int64 Value;
			VariantData.GetValue(Value);
			OutData = static_cast<uint64>(Value);
			break;
		}
		case EOnlineKeyValuePairDataType::UInt32:
		{
			uint32 Value;
			VariantData.GetValue(Value);
			OutData = static_cast<uint64>(Value);
			break;
		}
		case EOnlineKeyValuePairDataType::UInt64:
		{
			VariantData.GetValue(OutData);
			break;
		}
		default:
		{
			UE_LOG_ONLINE(Warning, TEXT("Could not %s convert to uint64"), VariantData.GetTypeString());
			OutData = 0;
			break;
		}
	}
}
void FOnlineAchievementsOculus::GetWriteAchievementBitfieldValue(FVariantData VariantData, FString& OutData, uint32 BitfieldLength) const
{
	switch (VariantData.GetType())
	{
		case EOnlineKeyValuePairDataType::Int32:
		{
			int32 Value;
			VariantData.GetValue(Value);
			auto UnpaddedBitfield = FString::FromInt(Value);
			auto PaddingLength = BitfieldLength - UnpaddedBitfield.Len();
			OutData = TEXT("");
			for (uint32 i = 0; i < PaddingLength; ++i)
			{
				OutData.AppendChar('0');
			}
			OutData.Append(UnpaddedBitfield);
			break;
		}
		case EOnlineKeyValuePairDataType::String:
		{
			VariantData.GetValue(OutData);
			break;
		}
		default:
		{
			UE_LOG_ONLINE(Warning, TEXT("Could not %s convert to string"), VariantData.GetTypeString());
			break;
		}
	}
}

double FOnlineAchievementsOculus::CalculatePlayerAchievementProgress(const FOnlineAchievementOculus Achievement)
{
	if (Achievement.bIsUnlocked)
	{
		return 100.0;
	}

	auto Desc = AchievementDescriptions.Find(Achievement.Id);
	if (Desc == nullptr)
	{
		UE_LOG_ONLINE(Warning, TEXT("Could not calculate progress for Achievement: '%s'"), *Achievement.Id);
		return 0.0;
	}

	double Progress;
	switch (Desc->Type)
	{
		case EAchievementType::Count:
		{
			Progress = Achievement.Count * 100.0 / Desc->Target;
			break;
		}
		case EAchievementType::Bitfield:
		{
			int BitfieldCount = 0;
			for (int32 i = 0; i < Achievement.Bitfield.Len(); ++i)
			{
				if (Achievement.Bitfield[i] == '1')
				{
					++BitfieldCount;
				}
			}
			Progress = BitfieldCount * 100.0 / Desc->Target;
			break;
		}
		default:
		{
			Progress = 0.0;
			break;
		}
	}

	// Cap the progress to 100
	return (Progress <= 100.0) ? Progress : 100.0;
}
