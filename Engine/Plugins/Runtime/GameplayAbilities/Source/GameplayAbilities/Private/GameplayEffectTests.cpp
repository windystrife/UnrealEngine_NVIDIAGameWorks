// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CoreMinimal.h"
#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"
#include "Engine/EngineBaseTypes.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "AttributeSet.h"
#include "GameplayEffectTypes.h"
#include "GameplayEffect.h"
#include "GameplayAbilitiesModule.h"
#include "GameplayTagsManager.h"
#include "GameplayTagsModule.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemTestPawn.h"
#include "AbilitySystemTestAttributeSet.h"
#include "AbilitySystemGlobals.h"

#define SKILL_TEST_TEXT( Format, ... ) FString::Printf(TEXT("%s - %d: %s"), TEXT(__FILE__) , __LINE__ , *FString::Printf(TEXT(Format), ##__VA_ARGS__) )

#if WITH_EDITOR

static UDataTable* CreateGameplayDataTable()
{
	FString CSV(TEXT("---,Tag,DevComment"));
	CSV.Append(TEXT("\r\n0,Damage,"));
	CSV.Append(TEXT("\r\n1,Damage.Basic,"));
	CSV.Append(TEXT("\r\n2,Damage.Type1,"));
	CSV.Append(TEXT("\r\n3,Damage.Type2,"));
	CSV.Append(TEXT("\r\n4,Damage.Reduce,"));
	CSV.Append(TEXT("\r\n5,Damage.Buffable,"));
	CSV.Append(TEXT("\r\n6,Damage.Buff,"));
	CSV.Append(TEXT("\r\n7,Damage.Physical,"));
	CSV.Append(TEXT("\r\n8,Damage.Fire,"));
	CSV.Append(TEXT("\r\n9,Damage.Buffed.FireBuff,"));
	CSV.Append(TEXT("\r\n10,Damage.Mitigated.Armor,"));
	CSV.Append(TEXT("\r\n11,Lifesteal,"));
	CSV.Append(TEXT("\r\n12,Shield,"));
	CSV.Append(TEXT("\r\n13,Buff,"));
	CSV.Append(TEXT("\r\n14,Immune,"));
	CSV.Append(TEXT("\r\n15,FireDamage,"));
	CSV.Append(TEXT("\r\n16,ShieldAbsorb,"));
	CSV.Append(TEXT("\r\n17,Stackable,"));
	CSV.Append(TEXT("\r\n18,Stack,"));
	CSV.Append(TEXT("\r\n19,Stack.CappedNumber,"));
	CSV.Append(TEXT("\r\n20,Stack.DiminishingReturns,"));
	CSV.Append(TEXT("\r\n21,Protect.Damage,"));
	CSV.Append(TEXT("\r\n22,SpellDmg.Buff,"));
	CSV.Append(TEXT("\r\n23,GameplayCue.Burning,"));

	auto DataTable = NewObject<UDataTable>(GetTransientPackage(), FName(TEXT("TempDataTable")));
	DataTable->RowStruct = FGameplayTagTableRow::StaticStruct();
	DataTable->CreateTableFromCSVString(CSV);

	FGameplayTagTableRow * Row = (FGameplayTagTableRow*)DataTable->RowMap["0"];
	if (Row)
	{
		check(Row->Tag == TEXT("Damage"));
	}
	return DataTable;
}

#define GET_FIELD_CHECKED(Class, Field) FindFieldChecked<UProperty>(Class::StaticClass(), GET_MEMBER_NAME_CHECKED(Class, Field))
#define CONSTRUCT_CLASS(Class, Name) Class* Name = NewObject<Class>(GetTransientPackage(), FName(TEXT(#Name)))

class GameplayEffectsTestSuite
{
public:
	GameplayEffectsTestSuite(UWorld* WorldIn, FAutomationTestBase* TestIn)
	: World(WorldIn)
	, Test(TestIn)
	{
		// run before each test

		const float StartingHealth = 100.f;
		const float StartingMana = 200.f;

		// set up the source actor
		SourceActor = World->SpawnActor<AAbilitySystemTestPawn>();
		SourceComponent = SourceActor->GetAbilitySystemComponent();
		SourceComponent->GetSet<UAbilitySystemTestAttributeSet>()->Health = StartingHealth;
		SourceComponent->GetSet<UAbilitySystemTestAttributeSet>()->MaxHealth = StartingHealth;
		SourceComponent->GetSet<UAbilitySystemTestAttributeSet>()->Mana = StartingMana;
		SourceComponent->GetSet<UAbilitySystemTestAttributeSet>()->MaxMana = StartingMana;

		// set up the destination actor
		DestActor = World->SpawnActor<AAbilitySystemTestPawn>();
		DestComponent = DestActor->GetAbilitySystemComponent();
		DestComponent->GetSet<UAbilitySystemTestAttributeSet>()->Health = StartingHealth;
		DestComponent->GetSet<UAbilitySystemTestAttributeSet>()->MaxHealth = StartingHealth;
		DestComponent->GetSet<UAbilitySystemTestAttributeSet>()->Mana = StartingMana;
		DestComponent->GetSet<UAbilitySystemTestAttributeSet>()->MaxMana = StartingMana;
	}

	~GameplayEffectsTestSuite()
	{
		// run after each test

		// destroy the actors
		if (SourceActor)
		{
			World->EditorDestroyActor(SourceActor, false);
		}
		if (DestActor)
		{
			World->EditorDestroyActor(DestActor, false);
		}
	}

public: // the tests

	void Test_InstantDamage()
	{
		const float DamageValue = 5.f;
		const float StartingHealth = DestComponent->GetSet<UAbilitySystemTestAttributeSet>()->Health;

		// just try and reduce the health attribute
		{
			
			CONSTRUCT_CLASS(UGameplayEffect, BaseDmgEffect);
			AddModifier(BaseDmgEffect, GET_FIELD_CHECKED(UAbilitySystemTestAttributeSet, Health), EGameplayModOp::Additive, FScalableFloat(-DamageValue));
			BaseDmgEffect->DurationPolicy = EGameplayEffectDurationType::Instant;
			
			SourceComponent->ApplyGameplayEffectToTarget(BaseDmgEffect, DestComponent, 1.f);
		}

		// make sure health was reduced
		Test->TestEqual(TEXT("Health Reduced"), DestComponent->GetSet<UAbilitySystemTestAttributeSet>()->Health, StartingHealth - DamageValue);
	}

	void Test_InstantDamageRemap()
	{
		const float DamageValue = 5.f;
		const float StartingHealth = DestComponent->GetSet<UAbilitySystemTestAttributeSet>()->Health;

		// This is the same as GameplayEffectsTest_InstantDamage but modifies the Damage attribute and confirms it is remapped to -Health by UAbilitySystemTestAttributeSet::PostAttributeModify
		{
			CONSTRUCT_CLASS(UGameplayEffect, BaseDmgEffect);
			AddModifier(BaseDmgEffect, GET_FIELD_CHECKED(UAbilitySystemTestAttributeSet, Damage), EGameplayModOp::Additive, FScalableFloat(DamageValue));
			BaseDmgEffect->DurationPolicy = EGameplayEffectDurationType::Instant;

			SourceComponent->ApplyGameplayEffectToTarget(BaseDmgEffect, DestComponent, 1.f);
		}

		// Now we should have lost some health
		Test->TestEqual(TEXT("Health Reduced"), DestComponent->GetSet<UAbilitySystemTestAttributeSet>()->Health, StartingHealth - DamageValue);

		// Confirm the damage attribute itself was reset to 0 when it was applied to health
		Test->TestEqual(TEXT("Damage Applied"), DestComponent->GetSet<UAbilitySystemTestAttributeSet>()->Damage, 0.f);
	}

	void Test_ManaBuff()
	{
		const float BuffValue = 30.f;
		const float StartingMana = DestComponent->GetSet<UAbilitySystemTestAttributeSet>()->Mana;

		FActiveGameplayEffectHandle BuffHandle;

		// apply the buff
		{
			CONSTRUCT_CLASS(UGameplayEffect, DamageBuffEffect);
			AddModifier(DamageBuffEffect, GET_FIELD_CHECKED(UAbilitySystemTestAttributeSet, Mana), EGameplayModOp::Additive, FScalableFloat(BuffValue));
			DamageBuffEffect->DurationPolicy = EGameplayEffectDurationType::Infinite;

			BuffHandle = SourceComponent->ApplyGameplayEffectToTarget(DamageBuffEffect, DestComponent, 1.f);
		}

		// check that the value changed
		Test->TestEqual(TEXT("Mana Buffed"), DestComponent->GetSet<UAbilitySystemTestAttributeSet>()->Mana, StartingMana + BuffValue);

		// remove the effect
		{
			DestComponent->RemoveActiveGameplayEffect(BuffHandle);
		}

		// check that the value changed back
		Test->TestEqual(TEXT("Mana Restored"), DestComponent->GetSet<UAbilitySystemTestAttributeSet>()->Mana, StartingMana);
	}

	void Test_PeriodicDamage()
	{
		const int32 NumPeriods = 10;
		const float PeriodSecs = 1.0f;
		const float DamagePerPeriod = 5.f; 
		const float StartingHealth = DestComponent->GetSet<UAbilitySystemTestAttributeSet>()->Health;

		// just try and reduce the health attribute
		{
			CONSTRUCT_CLASS(UGameplayEffect, BaseDmgEffect);
			AddModifier(BaseDmgEffect, GET_FIELD_CHECKED(UAbilitySystemTestAttributeSet, Health), EGameplayModOp::Additive, FScalableFloat(-DamagePerPeriod));
			BaseDmgEffect->DurationPolicy = EGameplayEffectDurationType::HasDuration;
			BaseDmgEffect->DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(NumPeriods * PeriodSecs));
			BaseDmgEffect->Period.Value = PeriodSecs;

			SourceComponent->ApplyGameplayEffectToTarget(BaseDmgEffect, DestComponent, 1.f);
		}

		int32 NumApplications = 0;

		// Tick a small number to verify the application tick
		TickWorld(SMALL_NUMBER);
		++NumApplications;

		Test->TestEqual(TEXT("Health Reduced"), DestComponent->GetSet<UAbilitySystemTestAttributeSet>()->Health, StartingHealth - (DamagePerPeriod * NumApplications));

		// Tick a bit more to address possible floating point issues
		TickWorld(PeriodSecs * .1f);

		for (int32 i = 0; i < NumPeriods; ++i)
		{
			// advance time by one period
			TickWorld(PeriodSecs);

			++NumApplications;

			// check that health has been reduced
			Test->TestEqual(TEXT("Health Reduced"), DestComponent->GetSet<UAbilitySystemTestAttributeSet>()->Health, StartingHealth - (DamagePerPeriod * NumApplications));
		}

		// advance time by one extra period
		TickWorld(PeriodSecs);

		// should not have reduced further
		Test->TestEqual(TEXT("Health Reduced"), DestComponent->GetSet<UAbilitySystemTestAttributeSet>()->Health, StartingHealth - (DamagePerPeriod * NumApplications));

		// TODO: test that the effect is no longer applied
	}

private: // test helpers

	template<typename MODIFIER_T>
	FGameplayModifierInfo& AddModifier(UGameplayEffect* Effect, UProperty* Property, EGameplayModOp::Type Op, const MODIFIER_T& Magnitude)
	{
		int32 Idx = Effect->Modifiers.Num();
		Effect->Modifiers.SetNum(Idx+1);
		FGameplayModifierInfo& Info = Effect->Modifiers[Idx];
		Info.ModifierMagnitude = Magnitude;
		Info.ModifierOp = Op;
		Info.Attribute.SetUProperty(Property);
		return Info;
	}

	void TickWorld(float Time)
	{
		const float step = 0.1f;
		while (Time > 0.f)
		{
			World->Tick(ELevelTick::LEVELTICK_All, FMath::Min(Time, step));
			Time -= step;

			// This is terrible but required for subticking like this.
			// we could always cache the real GFrameCounter at the start of our tests and restore it when finished.
			GFrameCounter++;
		}
	}

private:
	UWorld* World;
	FAutomationTestBase* Test;

	AAbilitySystemTestPawn* SourceActor;
	UAbilitySystemComponent* SourceComponent;

	AAbilitySystemTestPawn* DestActor;
	UAbilitySystemComponent* DestComponent;
};

#define ADD_TEST(Name) \
	TestFunctions.Add(&GameplayEffectsTestSuite::Name); \
	TestFunctionNames.Add(TEXT(#Name))

class FGameplayEffectsTest : public FAutomationTestBase
{
public:
	typedef void (GameplayEffectsTestSuite::*TestFunc)();

	FGameplayEffectsTest(const FString& InName)
	: FAutomationTestBase(InName, false)
	{
		// list all test functions here
		ADD_TEST(Test_InstantDamage);
		ADD_TEST(Test_InstantDamageRemap);
		ADD_TEST(Test_ManaBuff);
		ADD_TEST(Test_PeriodicDamage);
	}

	virtual uint32 GetTestFlags() const override { return EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter; }
	virtual bool IsStressTest() const { return false; }
	virtual uint32 GetRequiredDeviceNum() const override { return 1; }

protected:
	virtual FString GetBeautifiedTestName() const override { return "System.AbilitySystem.GameplayEffects"; }
	virtual void GetTests(TArray<FString>& OutBeautifiedNames, TArray <FString>& OutTestCommands) const override
	{
		for (const FString& TestFuncName : TestFunctionNames)
		{
			OutBeautifiedNames.Add(TestFuncName);
			OutTestCommands.Add(TestFuncName);
		}
	}

	bool RunTest(const FString& Parameters) override
	{
		// find the matching test
		TestFunc TestFunction = nullptr;
		for (int32 i = 0; i < TestFunctionNames.Num(); ++i)
		{
			if (TestFunctionNames[i] == Parameters)
			{
				TestFunction = TestFunctions[i];
				break;
			}
		}
		if (TestFunction == nullptr)
		{
			return false;
		}

		// get the current curve and data table (to restore later)
		UCurveTable *CurveTable = IGameplayAbilitiesModule::Get().GetAbilitySystemGlobals()->GetGlobalCurveTable();
		UDataTable *DataTable = IGameplayAbilitiesModule::Get().GetAbilitySystemGlobals()->GetGlobalAttributeMetaDataTable();

		// setup required GameplayTags
		UDataTable* TagTable = CreateGameplayDataTable();

		UGameplayTagsManager::Get().PopulateTreeFromDataTable(TagTable);

		UWorld *World = UWorld::CreateWorld(EWorldType::Game, false);
		FWorldContext &WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		WorldContext.SetCurrentWorld(World);

		FURL URL;
		World->InitializeActorsForPlay(URL);
		World->BeginPlay();

		// run the matching test
		uint64 InitialFrameCounter = GFrameCounter;
		{
			GameplayEffectsTestSuite Tester(World, this);
			(Tester.*TestFunction)();
		}
		GFrameCounter = InitialFrameCounter;

		GEngine->DestroyWorldContext(World);
		World->DestroyWorld(false);

		IGameplayAbilitiesModule::Get().GetAbilitySystemGlobals()->AutomationTestOnly_SetGlobalCurveTable(CurveTable);
		IGameplayAbilitiesModule::Get().GetAbilitySystemGlobals()->AutomationTestOnly_SetGlobalAttributeDataTable(DataTable);
		return true;
	}

	TArray<TestFunc> TestFunctions;
	TArray<FString> TestFunctionNames;
};

namespace
{
	FGameplayEffectsTest FGameplayEffectsTestAutomationTestInstance(TEXT("FGameplayEffectsTest"));
}

#endif //WITH_EDITOR
