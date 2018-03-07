// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "UObject/Package.h"
#include "Misc/AutomationTest.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "GameplayTagsManager.h"
#include "GameplayTagsModule.h"
#include "Stats/StatsMisc.h"

#if WITH_DEV_AUTOMATION_TESTS

class FGameplayTagTestBase : public FAutomationTestBase
{
public:
	FGameplayTagTestBase(const FString& InName, const bool bInComplexTask)
		: FAutomationTestBase(InName, bInComplexTask)
	{
	}

	UDataTable* CreateGameplayDataTable()
	{	
		TArray<FString> TestTags;

		TestTags.Add(TEXT("Effect.Damage"));
		TestTags.Add(TEXT("Effect.Damage.Basic"));
		TestTags.Add(TEXT("Effect.Damage.Type1"));
		TestTags.Add(TEXT("Effect.Damage.Type2"));
		TestTags.Add(TEXT("Effect.Damage.Reduce"));
		TestTags.Add(TEXT("Effect.Damage.Buffable"));
		TestTags.Add(TEXT("Effect.Damage.Buff"));
		TestTags.Add(TEXT("Effect.Damage.Physical"));
		TestTags.Add(TEXT("Effect.Damage.Fire"));
		TestTags.Add(TEXT("Effect.Damage.Buffed.FireBuff"));
		TestTags.Add(TEXT("Effect.Damage.Mitigated.Armor"));
		TestTags.Add(TEXT("Effect.Lifesteal"));
		TestTags.Add(TEXT("Effect.Shield"));
		TestTags.Add(TEXT("Effect.Buff"));
		TestTags.Add(TEXT("Effect.Immune"));
		TestTags.Add(TEXT("Effect.FireDamage"));
		TestTags.Add(TEXT("Effect.Shield.Absorb"));
		TestTags.Add(TEXT("Effect.Protect.Damage"));
		TestTags.Add(TEXT("Stackable"));
		TestTags.Add(TEXT("Stack.DiminishingReturns"));
		TestTags.Add(TEXT("GameplayCue.Burning"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.1"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.2"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.3"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.4"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.5"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.6"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.7"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.8"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.9"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.10"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.11"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.12"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.13"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.14"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.15"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.16"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.17"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.18"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.19"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.20"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.21"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.22"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.23"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.24"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.25"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.26"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.27"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.28"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.29"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.30"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.31"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.32"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.33"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.34"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.35"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.36"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.37"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.38"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.39"));
		TestTags.Add(TEXT("Expensive.Status.Tag.Type.40"));

		auto DataTable = NewObject<UDataTable>(GetTransientPackage(), FName(TEXT("TempDataTable")));
		DataTable->RowStruct = FGameplayTagTableRow::StaticStruct();

#if WITH_EDITOR
		FString CSV(TEXT(",Tag,CategoryText,"));
		for (int32 Count = 0; Count < TestTags.Num(); Count++)
		{
			CSV.Append(FString::Printf(TEXT("\r\n%d,%s"), Count, *TestTags[Count]));
		}

		DataTable->CreateTableFromCSVString(CSV);
#else
		// Construct the data table manually
		for (int32 Count = 0; Count < TestTags.Num(); Count++)
		{
			uint8* RowData = (uint8*)FMemory::Malloc(DataTable->RowStruct->PropertiesSize);
			DataTable->RowStruct->InitializeStruct(RowData);

			FGameplayTagTableRow* TagRow = (FGameplayTagTableRow*)RowData;
			TagRow->Tag = FName(*TestTags[Count]);

			DataTable->RowMap.Add(FName(*FString::FromInt(Count)), RowData);
		}
#endif
	
		FGameplayTagTableRow * Row = (FGameplayTagTableRow*)DataTable->RowMap["0"];
		if (Row)
		{
			check(Row->Tag == TEXT("Effect.Damage"));
		}
		return DataTable;
	}

	FGameplayTag GetTagForString(const FString& String)
	{
		return UGameplayTagsManager::Get().RequestGameplayTag(FName(*String));
	}

	void GameplayTagTest_SimpleTest()
	{
		FName TagName = FName(TEXT("Stack.DiminishingReturns"));
		FGameplayTag Tag = UGameplayTagsManager::Get().RequestGameplayTag(TagName);
		TestTrueExpr(Tag.GetTagName() == TagName);
	}

	void GameplayTagTest_TagComparisonTest()
	{
		FGameplayTag EffectDamageTag = GetTagForString(TEXT("Effect.Damage"));
		FGameplayTag EffectDamage1Tag = GetTagForString(TEXT("Effect.Damage.Type1"));
		FGameplayTag EffectDamage2Tag = GetTagForString(TEXT("Effect.Damage.Type2"));
		FGameplayTag CueTag = GetTagForString(TEXT("GameplayCue.Burning"));
		FGameplayTag EmptyTag;

		TestTrueExpr(EffectDamage1Tag == EffectDamage1Tag);
		TestTrueExpr(EffectDamage1Tag != EffectDamage2Tag);
		TestTrueExpr(EffectDamage1Tag != EffectDamageTag);

		TestTrueExpr(EffectDamage1Tag.MatchesTag(EffectDamageTag));
		TestTrueExpr(!EffectDamage1Tag.MatchesTagExact(EffectDamageTag));
		TestTrueExpr(!EffectDamage1Tag.MatchesTag(EmptyTag));
		TestTrueExpr(!EffectDamage1Tag.MatchesTagExact(EmptyTag));
		TestTrueExpr(!EmptyTag.MatchesTag(EmptyTag));
		TestTrueExpr(!EmptyTag.MatchesTagExact(EmptyTag));

		TestTrueExpr(EffectDamage1Tag.RequestDirectParent() == EffectDamageTag);
	}

	void GameplayTagTest_TagContainerTest()
	{
		FGameplayTag EffectDamageTag = GetTagForString(TEXT("Effect.Damage"));
		FGameplayTag EffectDamage1Tag = GetTagForString(TEXT("Effect.Damage.Type1"));
		FGameplayTag EffectDamage2Tag = GetTagForString(TEXT("Effect.Damage.Type2"));
		FGameplayTag CueTag = GetTagForString(TEXT("GameplayCue.Burning"));
		FGameplayTag EmptyTag;

		FGameplayTagContainer EmptyContainer;

		FGameplayTagContainer TagContainer;
		TagContainer.AddTag(EffectDamage1Tag);
		TagContainer.AddTag(CueTag);

		FGameplayTagContainer ReverseTagContainer;
		ReverseTagContainer.AddTag(CueTag);
		ReverseTagContainer.AddTag(EffectDamage1Tag);
	
		FGameplayTagContainer TagContainer2;
		TagContainer2.AddTag(EffectDamage2Tag);
		TagContainer2.AddTag(CueTag);

		TestTrueExpr(TagContainer == TagContainer);
		TestTrueExpr(TagContainer == ReverseTagContainer);
		TestTrueExpr(TagContainer != TagContainer2);

		FGameplayTagContainer TagContainerCopy = TagContainer;

		TestTrueExpr(TagContainerCopy == TagContainer);
		TestTrueExpr(TagContainerCopy != TagContainer2);

		TagContainerCopy.Reset();
		TagContainerCopy.AppendTags(TagContainer);

		TestTrueExpr(TagContainerCopy == TagContainer);
		TestTrueExpr(TagContainerCopy != TagContainer2);

		TestTrueExpr(TagContainer.HasAny(TagContainer2));
		TestTrueExpr(TagContainer.HasAnyExact(TagContainer2));
		TestTrueExpr(!TagContainer.HasAll(TagContainer2));
		TestTrueExpr(!TagContainer.HasAllExact(TagContainer2));
		TestTrueExpr(TagContainer.HasAll(TagContainerCopy));
		TestTrueExpr(TagContainer.HasAllExact(TagContainerCopy));

		TestTrueExpr(TagContainer.HasAll(EmptyContainer));
		TestTrueExpr(TagContainer.HasAllExact(EmptyContainer));
		TestTrueExpr(!TagContainer.HasAny(EmptyContainer));
		TestTrueExpr(!TagContainer.HasAnyExact(EmptyContainer));

		TestTrueExpr(EmptyContainer.HasAll(EmptyContainer));
		TestTrueExpr(EmptyContainer.HasAllExact(EmptyContainer));
		TestTrueExpr(!EmptyContainer.HasAny(EmptyContainer));
		TestTrueExpr(!EmptyContainer.HasAnyExact(EmptyContainer));

		TestTrueExpr(!EmptyContainer.HasAll(TagContainer));
		TestTrueExpr(!EmptyContainer.HasAllExact(TagContainer));
		TestTrueExpr(!EmptyContainer.HasAny(TagContainer));
		TestTrueExpr(!EmptyContainer.HasAnyExact(TagContainer));

		TestTrueExpr(TagContainer.HasTag(EffectDamageTag));
		TestTrueExpr(!TagContainer.HasTagExact(EffectDamageTag));
		TestTrueExpr(!TagContainer.HasTag(EmptyTag));
		TestTrueExpr(!TagContainer.HasTagExact(EmptyTag));

		TestTrueExpr(EffectDamage1Tag.MatchesAny(FGameplayTagContainer(EffectDamageTag)));
		TestTrueExpr(!EffectDamage1Tag.MatchesAnyExact(FGameplayTagContainer(EffectDamageTag)));

		TestTrueExpr(EffectDamage1Tag.MatchesAny(TagContainer));

		FGameplayTagContainer FilteredTagContainer = TagContainer.FilterExact(TagContainer2);
		TestTrueExpr(FilteredTagContainer.HasTagExact(CueTag));
		TestTrueExpr(!FilteredTagContainer.HasTagExact(EffectDamage1Tag));

		FilteredTagContainer = TagContainer.Filter(FGameplayTagContainer(EffectDamageTag));
		TestTrueExpr(!FilteredTagContainer.HasTagExact(CueTag));
		TestTrueExpr(FilteredTagContainer.HasTagExact(EffectDamage1Tag));

		FilteredTagContainer.Reset();
		FilteredTagContainer.AppendMatchingTags(TagContainer, TagContainer2);
	
		TestTrueExpr(FilteredTagContainer.HasTagExact(CueTag));
		TestTrueExpr(!FilteredTagContainer.HasTagExact(EffectDamage1Tag));
	}

	void GameplayTagTest_PerfTest()
	{
		FGameplayTag EffectDamageTag = GetTagForString(TEXT("Effect.Damage"));
		FGameplayTag EffectDamage1Tag = GetTagForString(TEXT("Effect.Damage.Type1"));
		FGameplayTag EffectDamage2Tag = GetTagForString(TEXT("Effect.Damage.Type2"));
		FGameplayTag CueTag = GetTagForString(TEXT("GameplayCue.Burning"));

		FGameplayTagContainer TagContainer;

		bool bResult = true;

		{
			FScopeLogTime LogTimePtr(*FString::Printf(TEXT("10000 get tag")), nullptr, FScopeLogTime::ScopeLog_Milliseconds);
			for (int32 i = 0; i < 10000; i++)
			{
				UGameplayTagsManager::Get().RequestGameplayTag(FName(TEXT("Effect.Damage")));
			}
		}

		{
			FScopeLogTime LogTimePtr(*FString::Printf(TEXT("1000 container constructions")), nullptr, FScopeLogTime::ScopeLog_Milliseconds);
			for (int32 i = 0; i < 1000; i++)
			{
				TagContainer = FGameplayTagContainer();
				TagContainer.AddTag(EffectDamage1Tag);
				TagContainer.AddTag(EffectDamage2Tag);
				TagContainer.AddTag(CueTag);
				for (int32 j = 1; j <= 40; j++)
				{
					TagContainer.AddTag(GetTagForString(FString::Printf(TEXT("Expensive.Status.Tag.Type.%d"), j)));
				}
			}
		}

		{
			FScopeLogTime LogTimePtr(*FString::Printf(TEXT("1000 container copies")), nullptr, FScopeLogTime::ScopeLog_Milliseconds);
			for (int32 i = 0; i < 1000; i++)
			{
				FGameplayTagContainer TagContainerNew;

				for (auto It = TagContainer.CreateConstIterator(); It; ++It)
				{
					TagContainerNew.AddTag(*It);
				}
			}
		}

		{
			FScopeLogTime LogTimePtr(*FString::Printf(TEXT("1000 container appends")), nullptr, FScopeLogTime::ScopeLog_Milliseconds);
			for (int32 i = 0; i < 1000; i++)
			{
				FGameplayTagContainer TagContainerNew;

				TagContainerNew.AppendTags(TagContainer);
			}
		}
	
		FGameplayTagContainer TagContainer2;
		TagContainer2.AddTag(EffectDamage1Tag);
		TagContainer2.AddTag(EffectDamage2Tag);
		TagContainer2.AddTag(CueTag);

		{
			FScopeLogTime LogTimePtr(*FString::Printf(TEXT("10000 MatchesAnyExact checks")), nullptr, FScopeLogTime::ScopeLog_Milliseconds);
			for (int32 i = 0; i < 10000; i++)
			{
				bResult &= EffectDamage1Tag.MatchesAnyExact(TagContainer);
			}
		}

		{
			FScopeLogTime LogTimePtr(*FString::Printf(TEXT("10000 MatchesAny checks")), nullptr, FScopeLogTime::ScopeLog_Milliseconds);
			for (int32 i = 0; i < 10000; i++)
			{
				bResult &= EffectDamage1Tag.MatchesAny(TagContainer);
			}
		}

		{
			FScopeLogTime LogTimePtr(*FString::Printf(TEXT("10000 HasTagExact checks")), nullptr, FScopeLogTime::ScopeLog_Milliseconds);
			for (int32 i = 0; i < 10000; i++)
			{
				bResult &= TagContainer.HasTagExact(EffectDamage1Tag);
			}
		}

		{
			FScopeLogTime LogTimePtr(*FString::Printf(TEXT("10000 HasTag checks")), nullptr, FScopeLogTime::ScopeLog_Milliseconds);
			for (int32 i = 0; i < 10000; i++)
			{
				bResult &= TagContainer.HasTag(EffectDamage1Tag);
			}
		}

		{
			FScopeLogTime LogTimePtr(*FString::Printf(TEXT("10000 HasAll checks")), nullptr, FScopeLogTime::ScopeLog_Milliseconds);
			for (int32 i = 0; i < 10000; i++)
			{
				bResult &= TagContainer.HasAll(TagContainer2);
			}
		}

		{
			FScopeLogTime LogTimePtr(*FString::Printf(TEXT("10000 HasAny checks")), nullptr, FScopeLogTime::ScopeLog_Milliseconds);
			for (int32 i = 0; i < 10000; i++)
			{
				bResult &= TagContainer.HasAny(TagContainer2);
			}
		}

		TestTrue(TEXT("Performance Tests succeeded"), bResult);
	}

};

IMPLEMENT_CUSTOM_SIMPLE_AUTOMATION_TEST(FGameplayTagTest, FGameplayTagTestBase, "System.GameplayTags.GameplayTag", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::EngineFilter)
bool FGameplayTagTest::RunTest(const FString& Parameters)
{
	// Create Test Data 
	UDataTable* DataTable = CreateGameplayDataTable();

	UGameplayTagsManager::Get().PopulateTreeFromDataTable(DataTable);

	// Run Tests
	GameplayTagTest_SimpleTest();
	GameplayTagTest_TagComparisonTest();
	GameplayTagTest_TagContainerTest();
	GameplayTagTest_PerfTest();

	return !HasAnyErrors();
}

#endif //WITH_DEV_AUTOMATION_TESTS
