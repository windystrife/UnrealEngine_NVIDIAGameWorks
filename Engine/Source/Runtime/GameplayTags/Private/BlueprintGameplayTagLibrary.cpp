// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlueprintGameplayTagLibrary.h"
#include "GameplayTagsModule.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"

UBlueprintGameplayTagLibrary::UBlueprintGameplayTagLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

bool UBlueprintGameplayTagLibrary::MatchesTag(FGameplayTag TagOne, FGameplayTag TagTwo, bool bExactMatch)
{
	if (bExactMatch)
	{
		return TagOne.MatchesTagExact(TagTwo);
	}

	return TagOne.MatchesTag(TagTwo);
}

bool UBlueprintGameplayTagLibrary::MatchesAnyTags(FGameplayTag TagOne, const FGameplayTagContainer& OtherContainer, bool bExactMatch)
{
	if (bExactMatch)
	{
		return TagOne.MatchesAnyExact(OtherContainer);
	}
	return TagOne.MatchesAny(OtherContainer);
}

bool UBlueprintGameplayTagLibrary::EqualEqual_GameplayTag(FGameplayTag A, FGameplayTag B)
{
	return A == B;
}

bool UBlueprintGameplayTagLibrary::NotEqual_GameplayTag(FGameplayTag A, FGameplayTag B)
{
	return A != B;
}

bool UBlueprintGameplayTagLibrary::IsGameplayTagValid(FGameplayTag GameplayTag)
{
	return GameplayTag.IsValid();
}

FName UBlueprintGameplayTagLibrary::GetTagName(const FGameplayTag& GameplayTag)
{
	return GameplayTag.GetTagName();
}

FGameplayTag UBlueprintGameplayTagLibrary::MakeLiteralGameplayTag(FGameplayTag Value)
{
	return Value;
}

int32 UBlueprintGameplayTagLibrary::GetNumGameplayTagsInContainer(const FGameplayTagContainer& TagContainer)
{
	return TagContainer.Num();
}

bool UBlueprintGameplayTagLibrary::HasTag(const FGameplayTagContainer& TagContainer, FGameplayTag Tag, bool bExactMatch)
{
	if (bExactMatch)
	{
		return TagContainer.HasTagExact(Tag);
	}
	return TagContainer.HasTag(Tag);
}

bool UBlueprintGameplayTagLibrary::HasAnyTags(const FGameplayTagContainer& TagContainer, const FGameplayTagContainer& OtherContainer, bool bExactMatch)
{
	if (bExactMatch)
	{
		return TagContainer.HasAnyExact(OtherContainer);
	}
	return TagContainer.HasAny(OtherContainer);
}

bool UBlueprintGameplayTagLibrary::HasAllTags(const FGameplayTagContainer& TagContainer, const FGameplayTagContainer& OtherContainer, bool bExactMatch)
{
	if (bExactMatch)
	{
		return TagContainer.HasAllExact(OtherContainer);
	}
	return TagContainer.HasAll(OtherContainer);
}

bool UBlueprintGameplayTagLibrary::DoesContainerMatchTagQuery(const FGameplayTagContainer& TagContainer, const FGameplayTagQuery& TagQuery)
{
	return TagQuery.Matches(TagContainer);
}

void UBlueprintGameplayTagLibrary::GetAllActorsOfClassMatchingTagQuery(UObject* WorldContextObject,
																	   TSubclassOf<AActor> ActorClass,
																	   const FGameplayTagQuery& GameplayTagQuery,
																	   TArray<AActor*>& OutActors)
{
	OutActors.Empty();

	UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);

	// We do nothing if not class provided, rather than giving ALL actors!
	if (ActorClass && World)
	{
		bool bHasLoggedMissingInterface = false;
		for (TActorIterator<AActor> It(World, ActorClass); It; ++It)
		{
			AActor* Actor = *It;
			check(Actor != nullptr);
			if (!Actor->IsPendingKill())
			{
				IGameplayTagAssetInterface* GameplayTagAssetInterface = Cast<IGameplayTagAssetInterface>(Actor);
				if (GameplayTagAssetInterface != nullptr)
				{
					FGameplayTagContainer OwnedGameplayTags;
					GameplayTagAssetInterface->GetOwnedGameplayTags(OwnedGameplayTags);

					if (OwnedGameplayTags.MatchesQuery(GameplayTagQuery))
					{
						OutActors.Add(Actor);
					}
				}
				else
				{
					if (!bHasLoggedMissingInterface)
					{
						UE_LOG(LogGameplayTags, Warning,
							TEXT("At least one actor (%s) of class %s does not implement IGameplTagAssetInterface.  Unable to find owned tags, so cannot determine if actor matches gameplay tag query.  Presuming it does not."),
							*Actor->GetName(), *ActorClass->GetName());
						bHasLoggedMissingInterface = true;
					}
				}
			}
		}
	}
}

bool UBlueprintGameplayTagLibrary::EqualEqual_GameplayTagContainer(const FGameplayTagContainer& A, const FGameplayTagContainer& B)
{
	return A == B;
}

bool UBlueprintGameplayTagLibrary::NotEqual_GameplayTagContainer(const FGameplayTagContainer& A, const FGameplayTagContainer& B)
{
	return A != B;
}

FGameplayTagContainer UBlueprintGameplayTagLibrary::MakeLiteralGameplayTagContainer(FGameplayTagContainer Value)
{
	return Value;
}

FGameplayTagContainer UBlueprintGameplayTagLibrary::MakeGameplayTagContainerFromArray(const TArray<FGameplayTag>& GameplayTags)
{
	return FGameplayTagContainer::CreateFromArray(GameplayTags);
}

FGameplayTagContainer UBlueprintGameplayTagLibrary::MakeGameplayTagContainerFromTag(FGameplayTag SingleTag)
{
	return FGameplayTagContainer(SingleTag);
}

void UBlueprintGameplayTagLibrary::BreakGameplayTagContainer(const FGameplayTagContainer& GameplayTagContainer, TArray<FGameplayTag>& GameplayTags)
{
	GameplayTagContainer.GetGameplayTagArray(GameplayTags);
}

FGameplayTagQuery UBlueprintGameplayTagLibrary::MakeGameplayTagQuery(FGameplayTagQuery TagQuery)
{
	return TagQuery;
}

bool UBlueprintGameplayTagLibrary::HasAllMatchingGameplayTags(TScriptInterface<IGameplayTagAssetInterface> TagContainerInterface, const FGameplayTagContainer& OtherContainer)
{
	if (TagContainerInterface.GetInterface() == NULL)
	{
		return (OtherContainer.Num() == 0);
	}

	FGameplayTagContainer OwnedTags;
	TagContainerInterface->GetOwnedGameplayTags(OwnedTags);
	return (OwnedTags.HasAll(OtherContainer));
}

bool UBlueprintGameplayTagLibrary::DoesTagAssetInterfaceHaveTag(TScriptInterface<IGameplayTagAssetInterface> TagContainerInterface, FGameplayTag Tag)
{
	if (TagContainerInterface.GetInterface() == NULL)
	{
		return false;
	}

	FGameplayTagContainer OwnedTags;
	TagContainerInterface->GetOwnedGameplayTags(OwnedTags);
	return (OwnedTags.HasTag(Tag));
}

void UBlueprintGameplayTagLibrary::AppendGameplayTagContainers(FGameplayTagContainer& InOutTagContainer, const FGameplayTagContainer& InTagContainer)
{
	InOutTagContainer.AppendTags(InTagContainer);
}

void UBlueprintGameplayTagLibrary::AddGameplayTag(FGameplayTagContainer& TagContainer, FGameplayTag Tag)
{
	TagContainer.AddTag(Tag);
}

bool UBlueprintGameplayTagLibrary::RemoveGameplayTag(FGameplayTagContainer& TagContainer, FGameplayTag Tag)
{
	return TagContainer.RemoveTag(Tag);
}

bool UBlueprintGameplayTagLibrary::NotEqual_TagTag(FGameplayTag A, FString B)
{
	return A.ToString() != B;
}

bool UBlueprintGameplayTagLibrary::NotEqual_TagContainerTagContainer(FGameplayTagContainer A, FString B)
{
	FGameplayTagContainer TagContainer;

	const FString OpenParenthesesStr(TEXT("("));
	const FString CloseParenthesesStr(TEXT(")"));

	// Convert string to Tag Container before compare
	FString TagString = MoveTemp(B);
	if (TagString.StartsWith(OpenParenthesesStr, ESearchCase::CaseSensitive) && TagString.EndsWith(CloseParenthesesStr, ESearchCase::CaseSensitive))
	{
		TagString = TagString.LeftChop(1);
		TagString = TagString.RightChop(1);

		const FString EqualStr(TEXT("="));

		TagString.Split(EqualStr, nullptr, &TagString, ESearchCase::CaseSensitive);

		TagString = TagString.LeftChop(1);
		TagString = TagString.RightChop(1);

		FString ReadTag;
		FString Remainder;

		const FString CommaStr(TEXT(","));
		const FString QuoteStr(TEXT("\""));

		while (TagString.Split(CommaStr, &ReadTag, &Remainder, ESearchCase::CaseSensitive))
		{
			ReadTag.Split(EqualStr, nullptr, &ReadTag, ESearchCase::CaseSensitive);
			if (ReadTag.EndsWith(CloseParenthesesStr, ESearchCase::CaseSensitive))
			{
				ReadTag = ReadTag.LeftChop(1);
				if (ReadTag.StartsWith(QuoteStr, ESearchCase::CaseSensitive) && ReadTag.EndsWith(QuoteStr, ESearchCase::CaseSensitive))
				{
					ReadTag = ReadTag.LeftChop(1);
					ReadTag = ReadTag.RightChop(1);
				}
			}
			TagString = Remainder;

			const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*ReadTag));
			TagContainer.AddTag(Tag);
		}
		if (Remainder.IsEmpty())
		{
			Remainder = MoveTemp(TagString);
		}
		if (!Remainder.IsEmpty())
		{
			Remainder.Split(EqualStr, nullptr, &Remainder, ESearchCase::CaseSensitive);
			if (Remainder.EndsWith(CloseParenthesesStr, ESearchCase::CaseSensitive))
			{
				Remainder = Remainder.LeftChop(1);
				if (Remainder.StartsWith(QuoteStr, ESearchCase::CaseSensitive) && Remainder.EndsWith(QuoteStr, ESearchCase::CaseSensitive))
				{
					Remainder = Remainder.LeftChop(1);
					Remainder = Remainder.RightChop(1);
				}
			}
			const FGameplayTag Tag = FGameplayTag::RequestGameplayTag(FName(*Remainder));
			TagContainer.AddTag(Tag);
		}
	}

	return A != TagContainer;
}
FString UBlueprintGameplayTagLibrary::GetDebugStringFromGameplayTagContainer(const FGameplayTagContainer& TagContainer)
{
	return TagContainer.ToStringSimple();
}

FString UBlueprintGameplayTagLibrary::GetDebugStringFromGameplayTag(FGameplayTag GameplayTag)
{
	return GameplayTag.ToString();
}
