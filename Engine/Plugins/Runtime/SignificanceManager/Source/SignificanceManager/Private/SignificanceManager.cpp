// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "SignificanceManager.h"
#include "EngineGlobals.h"
#include "Engine/Canvas.h"
#include "GameFramework/HUD.h"
#include "Engine/Engine.h"
#include "Async/ParallelFor.h"
#include "Engine/Console.h"
#include "ConsoleSettings.h"

IMPLEMENT_MODULE( FSignificanceManagerModule, SignificanceManager );

DECLARE_CYCLE_STAT(TEXT("Update Total"), STAT_SignificanceManager_Update, STATGROUP_SignificanceManager);
DECLARE_CYCLE_STAT(TEXT("Significance Update"), STAT_SignificanceManager_SignificanceUpdate, STATGROUP_SignificanceManager);
DECLARE_CYCLE_STAT(TEXT("Significance Sort"), STAT_SignificanceManager_SignificanceSort, STATGROUP_SignificanceManager);
DECLARE_CYCLE_STAT(TEXT("Unregister Object"), STAT_SignificanceManager_UnregisterObject, STATGROUP_SignificanceManager);
DECLARE_CYCLE_STAT(TEXT("Significance Check"), STAT_SignificanceManager_SignificanceCheck, STATGROUP_SignificanceManager);
DECLARE_CYCLE_STAT(TEXT("Register Object"), STAT_SignificanceManager_RegisterObject, STATGROUP_SignificanceManager);
DECLARE_CYCLE_STAT(TEXT("Initial Significance Update"), STAT_SignificanceManager_InitialSignificanceUpdate, STATGROUP_SignificanceManager);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Managed Objects"), STAT_SignificanceManager_NumObjects, STATGROUP_SignificanceManager);

bool CompareBySignificanceAscending(const USignificanceManager::FManagedObjectInfo& A, const USignificanceManager::FManagedObjectInfo& B) 
{ 
	return A.GetSignificance() < B.GetSignificance(); 
}

bool CompareBySignificanceDescending(const USignificanceManager::FManagedObjectInfo& A, const USignificanceManager::FManagedObjectInfo& B) 
{ 
	return A.GetSignificance() > B.GetSignificance(); 
}

typedef bool CompareFunctionType(const USignificanceManager::FManagedObjectInfo&,const USignificanceManager::FManagedObjectInfo&);

CompareFunctionType& PickCompareBySignificance(const bool bAscending)
{
	return (bAscending ? CompareBySignificanceAscending : CompareBySignificanceDescending);
}

TMap<const UWorld*, USignificanceManager*> FSignificanceManagerModule::WorldSignificanceManagers;
TSubclassOf<USignificanceManager> FSignificanceManagerModule::SignificanceManagerClass;

void FSignificanceManagerModule::StartupModule()
{
	FWorldDelegates::OnPreWorldInitialization.AddStatic(&FSignificanceManagerModule::OnWorldInit);
	FWorldDelegates::OnPostWorldCleanup.AddStatic(&FSignificanceManagerModule::OnWorldCleanup);
	if (!IsRunningDedicatedServer())
	{
		AHUD::OnShowDebugInfo.AddStatic(&FSignificanceManagerModule::OnShowDebugInfo);
	}

	UConsole::RegisterConsoleAutoCompleteEntries.AddStatic(&FSignificanceManagerModule::PopulateAutoCompleteEntries);
}

void FSignificanceManagerModule::AddReferencedObjects( FReferenceCollector& Collector )
{
	for (TPair<const UWorld*, USignificanceManager*>& WorldSignificanceManagerPair : WorldSignificanceManagers)
	{
		Collector.AddReferencedObject(WorldSignificanceManagerPair.Value, WorldSignificanceManagerPair.Key);
	}
	UClass* SignificanceManagerClassPtr = *SignificanceManagerClass;
	Collector.AddReferencedObject(SignificanceManagerClassPtr);
	SignificanceManagerClass = SignificanceManagerClassPtr; // Since pointer can be modified by AddReferencedObject
}

void FSignificanceManagerModule::OnWorldInit(UWorld* World, const UWorld::InitializationValues IVS)
{
	if (World->IsGameWorld())
	{
		if (*SignificanceManagerClass == nullptr)
		{
			SignificanceManagerClass = LoadClass<USignificanceManager>(nullptr, *GetDefault<USignificanceManager>()->SignificanceManagerClassName.ToString());
		}

		if (*SignificanceManagerClass != nullptr)
		{
			const USignificanceManager* ManagerToCreateDefault = SignificanceManagerClass->GetDefaultObject<USignificanceManager>();
			if ((ManagerToCreateDefault->bCreateOnServer && !IsRunningClientOnly()) || (ManagerToCreateDefault->bCreateOnClient && !IsRunningDedicatedServer()))
			{
				WorldSignificanceManagers.Add(World, NewObject<USignificanceManager>(World, SignificanceManagerClass));
			}
		}
	}
}

void FSignificanceManagerModule::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	WorldSignificanceManagers.Remove(World);
}

void FSignificanceManagerModule::OnShowDebugInfo(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos)
{
	static const FName NAME_SignificanceManager("SignificanceManager");
	if (Canvas && HUD->ShouldDisplayDebug(NAME_SignificanceManager))
	{
		if (USignificanceManager* SignificanceManager = Get(HUD->GetWorld()))
		{
			SignificanceManager->OnShowDebugInfo(HUD, Canvas, DisplayInfo, YL, YPos);
		}
	}
}

void FSignificanceManagerModule::PopulateAutoCompleteEntries(TArray<FAutoCompleteCommand>& AutoCompleteList)
{
	const UConsoleSettings* ConsoleSettings = GetDefault<UConsoleSettings>();

	AutoCompleteList.AddDefaulted();
	
	FAutoCompleteCommand& AutoCompleteCommand = AutoCompleteList.Last();
	AutoCompleteCommand.Command = TEXT("showdebug SIGNIFICANCEMANAGER");
	AutoCompleteCommand.Desc = TEXT("Toggles display of significance manager calculations");
	AutoCompleteCommand.Color = ConsoleSettings->AutoCompleteCommandColor;
}

USignificanceManager::USignificanceManager()
	: Super()
{
	SignificanceManagerClassName = FSoftClassPath(GetClass()); 

	bCreateOnClient = true;
	bCreateOnServer = true;
	bSortSignificanceAscending = false;
}

void USignificanceManager::BeginDestroy()
{
	Super::BeginDestroy();

	for (const TPair<UObject*, FManagedObjectInfo*>& ObjectToObjectInfoPair : ManagedObjects)
	{
		delete ObjectToObjectInfoPair.Value;
	}
	ManagedObjects.Reset();
	ManagedObjectsByTag.Reset();
}

UWorld* USignificanceManager::GetWorld()const
{
	return CastChecked<UWorld>(GetOuter());
}

void USignificanceManager::RegisterObject(UObject* Object, const FName Tag, FSignificanceFunction SignificanceFunction, FPostSignificanceFunction InPostSignificanceFunction)
{
	INC_DWORD_STAT(STAT_SignificanceManager_NumObjects);
	SCOPE_CYCLE_COUNTER(STAT_SignificanceManager_RegisterObject);

	check(Object);
	checkf(!ManagedObjects.Contains(Object), TEXT("'%s' already added to significance manager. Original Tag: '%s' New Tag: '%s'"), *Object->GetName(), *ManagedObjects.FindChecked(Object)->GetTag().ToString(), *Tag.ToString());

	FManagedObjectInfo* ObjectInfo = new FManagedObjectInfo(Object, Tag, SignificanceFunction, InPostSignificanceFunction);
	
	// Calculate initial significance
	if (Viewpoints.Num())
	{
		SCOPE_CYCLE_COUNTER(STAT_SignificanceManager_InitialSignificanceUpdate);
		ObjectInfo->UpdateSignificance(Viewpoints,bSortSignificanceAscending);
	}

	ManagedObjects.Add(Object, ObjectInfo);
	TArray<const FManagedObjectInfo*>& ManagedObjectInfos = ManagedObjectsByTag.FindOrAdd(Tag);

	if (ManagedObjectInfos.Num() > 0)
	{
		// Insert in to the sorted list
		int32 LowIndex = 0;
		int32 HighIndex = ManagedObjectInfos.Num() - 1;
		auto CompareFunction = PickCompareBySignificance(bSortSignificanceAscending);
		while (true)
		{
			int32 MidIndex = LowIndex + (HighIndex - LowIndex) / 2;
			if (CompareFunction(*ObjectInfo, *ManagedObjectInfos[MidIndex]))
			{
				if (LowIndex == MidIndex)
				{
					ManagedObjectInfos.Insert(ObjectInfo, LowIndex);
					break;
				}
				else
				{
					HighIndex = MidIndex - 1;
				}
			}
			else if (LowIndex == HighIndex)
			{
				ManagedObjectInfos.Insert(ObjectInfo, LowIndex + 1);
				break;
			}
			else
			{
				LowIndex = MidIndex + 1;
			}
		}
	}
	else
	{
		ManagedObjectInfos.Add(ObjectInfo);
	}
}

void USignificanceManager::UnregisterObject(UObject* Object)
{
	DEC_DWORD_STAT(STAT_SignificanceManager_NumObjects);
	SCOPE_CYCLE_COUNTER(STAT_SignificanceManager_UnregisterObject);

	FManagedObjectInfo* ObjectInfo;
	if (ManagedObjects.RemoveAndCopyValue(Object, ObjectInfo))
	{
		TArray<const FManagedObjectInfo*>& ObjectsWithTag = ManagedObjectsByTag.FindChecked(ObjectInfo->GetTag());
		if (ObjectsWithTag.Num() == 1)
		{
			check(ObjectsWithTag[0] == ObjectInfo);
			ManagedObjectsByTag.Remove(ObjectInfo->GetTag());
			if (ObjectInfo->PostSignificanceFunction != nullptr)
			{
				ObjectInfo->PostSignificanceFunction(ObjectInfo->GetObject(), ObjectInfo->Significance, 1.0f, true);
			}
		}
		else
		{
			ObjectsWithTag.RemoveSingle(ObjectInfo);
		}
		delete ObjectInfo;
	}
}

void USignificanceManager::UnregisterAll(FName Tag)
{
	if (TArray<const FManagedObjectInfo*>* ObjectsWithTag = ManagedObjectsByTag.Find(Tag))
	{
		for (const FManagedObjectInfo* ManagedObj : *ObjectsWithTag)
		{
			ManagedObjects.Remove(ManagedObj->GetObject());
			if (ManagedObj->PostSignificanceFunction != nullptr)
			{
				ManagedObj->PostSignificanceFunction(ManagedObj->GetObject(), ManagedObj->Significance, 1.0f, true);
			}
		}
		ManagedObjectsByTag.Remove(Tag);
	}
}

const TArray<const USignificanceManager::FManagedObjectInfo*>& USignificanceManager::GetManagedObjects(const FName Tag) const
{
	if (const TArray<const FManagedObjectInfo*>* ObjectsWithTag = ManagedObjectsByTag.Find(Tag))
	{
		return *ObjectsWithTag;
	}

	static const TArray<const FManagedObjectInfo*> EmptySet;
	return EmptySet;
}

USignificanceManager::FManagedObjectInfo* USignificanceManager::GetManagedObject(UObject* Object) const
{
	if (FManagedObjectInfo* const* Info = ManagedObjects.Find(Object))
	{
		return (*Info);
	}

	return nullptr;
}

void USignificanceManager::GetManagedObjects(TArray<const USignificanceManager::FManagedObjectInfo*>& OutManagedObjects, bool bInSignificanceOrder) const
{
	OutManagedObjects.Reserve(ManagedObjects.Num());
	for (const TPair<FName, TArray<const FManagedObjectInfo*>>& TagToObjectInfoArrayPair : ManagedObjectsByTag)
	{
		OutManagedObjects.Append(TagToObjectInfoArrayPair.Value);
	}
	if (bInSignificanceOrder)
	{
		OutManagedObjects.Sort(PickCompareBySignificance(bSortSignificanceAscending));
	}
}

float USignificanceManager::GetSignificance(const UObject* Object) const
{
	SCOPE_CYCLE_COUNTER(STAT_SignificanceManager_SignificanceCheck);
	float Significance = 0.f;
	if (FManagedObjectInfo* const* Info = ManagedObjects.Find(Object))
	{
		Significance = (*Info)->GetSignificance();
	}
	return Significance;
}

bool USignificanceManager::QuerySignificance(const UObject* Object, float& OutSignificance) const
{
	SCOPE_CYCLE_COUNTER(STAT_SignificanceManager_SignificanceCheck);
	if (FManagedObjectInfo* const* Info = ManagedObjects.Find(Object))
	{
		OutSignificance = (*Info)->GetSignificance();
		return true;
	}
	else
	{
		OutSignificance = 0.0f;
		return false;
	}
}

void USignificanceManager::FManagedObjectInfo::UpdateSignificance(const TArray<FTransform>& InViewpoints, const bool bSortAscending)
{
	float OldSignificance = Significance;
	if (InViewpoints.Num())
	{
		if (bSortAscending)
		{
			Significance = TNumericLimits<float>::Max();
			for (const FTransform& Viewpoint : InViewpoints)
			{
				const float ViewpointSignificance = SignificanceFunction(Object, Viewpoint);
				if (ViewpointSignificance < Significance)
				{
					Significance = ViewpointSignificance;
				}
			}
		}
		else
		{
			Significance = TNumericLimits<float>::Lowest();
			for (const FTransform& Viewpoint : InViewpoints)
			{
				const float ViewpointSignificance = SignificanceFunction(Object, Viewpoint);
				if (ViewpointSignificance > Significance)
				{
					Significance = ViewpointSignificance;
				}
			}
		}
	}
	else
	{
		Significance = 0.f;
	}

	if (PostSignificanceFunction != nullptr)
	{
		PostSignificanceFunction(Object, OldSignificance, Significance, false);
	}
}

void USignificanceManager::Update(const TArray<FTransform>& InViewpoints)
{
	Viewpoints = InViewpoints;

	SCOPE_CYCLE_COUNTER(STAT_SignificanceManager_Update);

	{
		SCOPE_CYCLE_COUNTER(STAT_SignificanceManager_SignificanceUpdate);

		TArray<FManagedObjectInfo*> ObjArray;
		ManagedObjects.GenerateValueArray(ObjArray);

		ParallelFor(ObjArray.Num(),
			[&](int32 Index)
		{
			FManagedObjectInfo* ObjectInfo = ObjArray[Index];

			checkSlow(ObjectInfo->GetObject()->IsValidLowLevel());

			ObjectInfo->UpdateSignificance(Viewpoints,bSortSignificanceAscending);
		});
	}

	{
		SCOPE_CYCLE_COUNTER(STAT_SignificanceManager_SignificanceSort);
		for (TPair<FName, TArray<const FManagedObjectInfo*>>& TagToObjectInfoArrayPair : ManagedObjectsByTag)
		{
			TagToObjectInfoArrayPair.Value.Sort(PickCompareBySignificance(bSortSignificanceAscending));
		}
	}
}

static int32 GSignificanceManagerObjectsToShow = 15;
static FAutoConsoleVariableRef CVarSignificanceManagerObjectsToShow(
	TEXT("SigMan.ObjectsToShow"),
	GSignificanceManagerObjectsToShow,
	TEXT("How many objects to display when ShowDebug SignificanceManager is enabled.\n"),
	ECVF_Cheat
	);

static FAutoConsoleVariable CVarSignificanceManagerFilterTag(
	TEXT("SigMan.FilterTag"),
	TEXT(""),
	TEXT("Only display objects with the specified filter tag.  If None objects with any will be displayed.\n"),
	ECVF_Cheat
	);


void USignificanceManager::OnShowDebugInfo(AHUD* HUD, UCanvas* Canvas, const FDebugDisplayInfo& DisplayInfo, float& YL, float& YPos)
{
	static const FName NAME_SignificanceManager("SignificanceManager");
	if (Canvas && HUD->ShouldDisplayDebug(NAME_SignificanceManager))
	{
		if (HasAnyFlags(RF_ClassDefaultObject))
		{
			if (USignificanceManager* SignificanceManager = Get(HUD->GetWorld()))
			{
				SignificanceManager->OnShowDebugInfo(HUD, Canvas, DisplayInfo, YL, YPos);
			}
		}
		else
		{
			FDisplayDebugManager& DisplayDebugManager = Canvas->DisplayDebugManager;
			DisplayDebugManager.SetFont(GEngine->GetSmallFont());
			DisplayDebugManager.SetDrawColor(FColor::Red);
			DisplayDebugManager.DrawString(FString::Printf(TEXT("SIGNIFICANCE MANAGER - %d Managed Objects"), ManagedObjects.Num()));

			const FName SignificanceManagerTag(*CVarSignificanceManagerFilterTag->GetString());
			TArray<const FManagedObjectInfo*> AllObjects;
			const TArray<const FManagedObjectInfo*>& ObjectsToShow = (SignificanceManagerTag.IsNone() ? AllObjects : GetManagedObjects(SignificanceManagerTag));
			if (SignificanceManagerTag.IsNone())
			{
				GetManagedObjects(AllObjects, true);
			}

			DisplayDebugManager.SetDrawColor(FColor::White);
			const int32 NumObjectsToShow = FMath::Min(GSignificanceManagerObjectsToShow, ObjectsToShow.Num());
			for (int32 Index = 0; Index < NumObjectsToShow; ++Index)
			{
				const FManagedObjectInfo* ObjectInfo = ObjectsToShow[Index];
				const FString Str = FString::Printf(TEXT("%6.3f - %s (%s)"), ObjectInfo->GetSignificance(), *ObjectInfo->GetObject()->GetName(), *ObjectInfo->GetTag().ToString());
				DisplayDebugManager.DrawString(Str);
			}
		}
	}
}
