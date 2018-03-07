// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraModule.h"
#include "Modules/ModuleManager.h"
#include "NiagaraTypes.h"
#include "NiagaraEvents.h"
#include "NiagaraSettings.h"
#include "NiagaraDataInterfaceCurlNoise.h"
#include "Class.h"
#include "Package.h"
#include "NiagaraWorldManager.h"
#include "VectorVM.h"

IMPLEMENT_MODULE(INiagaraModule, Niagara);

void INiagaraModule::StartupModule()
{
	VectorVM::Init();
	FNiagaraTypeDefinition::Init();

	FWorldDelegates::OnPreWorldInitialization.AddRaw(this, &INiagaraModule::OnWorldInit);
	FWorldDelegates::OnWorldCleanup.AddRaw(this, &INiagaraModule::OnWorldCleanup);
	FWorldDelegates::OnPreWorldFinishDestroy.AddRaw(this, &INiagaraModule::OnPreWorldFinishDestroy);

	FWorldDelegates::OnWorldPostActorTick.AddRaw(this, &INiagaraModule::TickWorld);
#if WITH_EDITOR	
	// This is done so that the editor classes are available to load in the cooker on editor builds even though it doesn't load the editor directly.
	// UMG does something similar for similar reasons.
	// @TODO We should remove this once Niagara is fully a plug-in.
	FModuleManager::Get().LoadModule(TEXT("NiagaraEditor"));
#endif
	UNiagaraDataInterfaceCurlNoise::InitNoiseLUT();
}

void INiagaraModule::ShutdownModule()
{
	//Should have cleared up all world managers by now.
	check(WorldManagers.Num() == 0);
	for (TPair<UWorld*, FNiagaraWorldManager*> Pair : WorldManagers)
	{
		delete Pair.Value;
		Pair.Value = nullptr;
	}
}

FNiagaraWorldManager* INiagaraModule::GetWorldManager(UWorld* World)
{
	return WorldManagers.FindChecked(World);
}

void INiagaraModule::DestroyAllSystemSimulations(class UNiagaraSystem* System)
{
	for (TPair<UWorld*, FNiagaraWorldManager*>& Pair : WorldManagers)
	{
		Pair.Value->DestroySystemSimulation(System);
	}
}

void INiagaraModule::OnWorldInit(UWorld* World, const UWorld::InitializationValues IVS)
{
	check(WorldManagers.Find(World) == nullptr);
	WorldManagers.Add(World) = new FNiagaraWorldManager(World);
}

void INiagaraModule::OnWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources)
{
	//Cleanup world manager contents but not the manager itself.
}

void INiagaraModule::OnPreWorldFinishDestroy(UWorld* World)
{
	FNiagaraWorldManager*& Manager = WorldManagers.FindChecked(World);
	delete Manager;
	WorldManagers.Remove(World);
}

void INiagaraModule::TickWorld(UWorld* World, ELevelTick TickType, float DeltaSeconds)
{
	GetWorldManager(World)->Tick(DeltaSeconds);
}



//////////////////////////////////////////////////////////////////////////

const UScriptStruct* FNiagaraTypeDefinition::ParameterMapStruct;
const UScriptStruct* FNiagaraTypeDefinition::NumericStruct;
const UScriptStruct* FNiagaraTypeDefinition::FloatStruct;
const UScriptStruct* FNiagaraTypeDefinition::BoolStruct;
const UScriptStruct* FNiagaraTypeDefinition::IntStruct;
const UScriptStruct* FNiagaraTypeDefinition::Matrix4Struct;
const UScriptStruct* FNiagaraTypeDefinition::Vec4Struct;
const UScriptStruct* FNiagaraTypeDefinition::Vec3Struct;
const UScriptStruct* FNiagaraTypeDefinition::Vec2Struct;
const UScriptStruct* FNiagaraTypeDefinition::ColorStruct;

const UEnum* FNiagaraTypeDefinition::ExecutionStateEnum;

FNiagaraTypeDefinition FNiagaraTypeDefinition::ParameterMapDef;
FNiagaraTypeDefinition FNiagaraTypeDefinition::NumericDef;
FNiagaraTypeDefinition FNiagaraTypeDefinition::FloatDef;
FNiagaraTypeDefinition FNiagaraTypeDefinition::BoolDef;
FNiagaraTypeDefinition FNiagaraTypeDefinition::IntDef;
FNiagaraTypeDefinition FNiagaraTypeDefinition::Matrix4Def;
FNiagaraTypeDefinition FNiagaraTypeDefinition::Vec4Def;
FNiagaraTypeDefinition FNiagaraTypeDefinition::Vec3Def;
FNiagaraTypeDefinition FNiagaraTypeDefinition::Vec2Def;
FNiagaraTypeDefinition FNiagaraTypeDefinition::ColorDef;

TSet<const UScriptStruct*> FNiagaraTypeDefinition::NumericStructs;
TArray<FNiagaraTypeDefinition> FNiagaraTypeDefinition::OrderedNumericTypes;

TSet<const UScriptStruct*> FNiagaraTypeDefinition::ScalarStructs;

TSet<const UStruct*> FNiagaraTypeDefinition::FloatStructs;
TSet<const UStruct*> FNiagaraTypeDefinition::IntStructs;
TSet<const UStruct*> FNiagaraTypeDefinition::BoolStructs;

FNiagaraTypeDefinition FNiagaraTypeDefinition::CollisionEventDef;


TArray<FNiagaraTypeDefinition> FNiagaraTypeRegistry::RegisteredTypes;
TArray<FNiagaraTypeDefinition> FNiagaraTypeRegistry::RegisteredParamTypes;
TArray<FNiagaraTypeDefinition> FNiagaraTypeRegistry::RegisteredPayloadTypes;
TArray<FNiagaraTypeDefinition> FNiagaraTypeRegistry::RegisteredUserDefinedTypes;
TArray<FNiagaraTypeDefinition> FNiagaraTypeRegistry::RegisteredNumericTypes;

bool FNiagaraTypeDefinition::IsDataInterface()const
{
	return Struct->IsChildOf(UNiagaraDataInterface::StaticClass());
}

void FNiagaraTypeDefinition::Init()
{
	static auto* CoreUObjectPkg = FindObjectChecked<UPackage>(nullptr, TEXT("/Script/CoreUObject"));
	static auto* NiagaraPkg = FindObjectChecked<UPackage>(nullptr, TEXT("/Script/Niagara"));
	FNiagaraTypeDefinition::ParameterMapStruct = FindObjectChecked<UScriptStruct>(NiagaraPkg, TEXT("NiagaraParameterMap"));
	FNiagaraTypeDefinition::NumericStruct = FindObjectChecked<UScriptStruct>(NiagaraPkg, TEXT("NiagaraNumeric"));
	FNiagaraTypeDefinition::FloatStruct = FindObjectChecked<UScriptStruct>(NiagaraPkg, TEXT("NiagaraFloat"));
	FNiagaraTypeDefinition::BoolStruct = FindObjectChecked<UScriptStruct>(NiagaraPkg, TEXT("NiagaraBool"));
	FNiagaraTypeDefinition::IntStruct = FindObjectChecked<UScriptStruct>(NiagaraPkg, TEXT("NiagaraInt32"));
	FNiagaraTypeDefinition::Matrix4Struct = FindObjectChecked<UScriptStruct>(NiagaraPkg, TEXT("NiagaraMatrix"));

	FNiagaraTypeDefinition::Vec2Struct = FindObjectChecked<UScriptStruct>(CoreUObjectPkg, TEXT("Vector2D"));
	FNiagaraTypeDefinition::Vec3Struct = FindObjectChecked<UScriptStruct>(CoreUObjectPkg, TEXT("Vector"));
	FNiagaraTypeDefinition::Vec4Struct = FindObjectChecked<UScriptStruct>(CoreUObjectPkg, TEXT("Vector4"));
	FNiagaraTypeDefinition::ColorStruct = FindObjectChecked<UScriptStruct>(CoreUObjectPkg, TEXT("LinearColor"));
	
	ParameterMapDef = FNiagaraTypeDefinition(ParameterMapStruct);
	NumericDef = FNiagaraTypeDefinition(NumericStruct);
	FloatDef = FNiagaraTypeDefinition(FloatStruct);
	BoolDef = FNiagaraTypeDefinition(BoolStruct);
	IntDef = FNiagaraTypeDefinition(IntStruct);
	Vec2Def = FNiagaraTypeDefinition(Vec2Struct);
	Vec3Def = FNiagaraTypeDefinition(Vec3Struct);
	Vec4Def = FNiagaraTypeDefinition(Vec4Struct);
	ColorDef = FNiagaraTypeDefinition(ColorStruct);
	Matrix4Def = FNiagaraTypeDefinition(Matrix4Struct);

	CollisionEventDef = FNiagaraTypeDefinition(FNiagaraCollisionEventPayload::StaticStruct());
	NumericStructs.Add(NumericStruct);
	NumericStructs.Add(FloatStruct);
	NumericStructs.Add(IntStruct);
	NumericStructs.Add(Vec2Struct);
	NumericStructs.Add(Vec3Struct);
	NumericStructs.Add(Vec4Struct);
	NumericStructs.Add(ColorStruct);
	//Make matrix a numeric type?

	FloatStructs.Add(FloatStruct);
	FloatStructs.Add(Vec2Struct);
	FloatStructs.Add(Vec3Struct);
	FloatStructs.Add(Vec4Struct);
	//FloatStructs.Add(Matrix4Struct)??
	FloatStructs.Add(ColorStruct);

	IntStructs.Add(IntStruct);

	BoolStructs.Add(BoolStruct);

	OrderedNumericTypes.Add(IntStruct);
	OrderedNumericTypes.Add(FloatStruct);
	OrderedNumericTypes.Add(Vec2Struct);
	OrderedNumericTypes.Add(Vec3Struct);
	OrderedNumericTypes.Add(Vec4Struct);
	OrderedNumericTypes.Add(ColorStruct);

	ScalarStructs.Add(BoolStruct);
	ScalarStructs.Add(IntStruct);
	ScalarStructs.Add(FloatStruct);

	ExecutionStateEnum = FindObject<UEnum>(ANY_PACKAGE, TEXT("ENiagaraExecutionState"), true);
	
	RecreateUserDefinedTypeRegistry();
}

bool FNiagaraTypeDefinition::IsValidNumericInput(const FNiagaraTypeDefinition& TypeDef)
{
	if (NumericStructs.Contains(TypeDef.GetScriptStruct()))
	{
		return true;
	}
	return false;
}

void FNiagaraTypeDefinition::RecreateUserDefinedTypeRegistry()
{
	static auto* CoreUObjectPkg = FindObjectChecked<UPackage>(nullptr, TEXT("/Script/CoreUObject"));
	static auto* NiagaraPkg = FindObjectChecked<UPackage>(nullptr, TEXT("/Script/Niagara"));

	FNiagaraTypeRegistry::ClearUserDefinedRegistry();

	FNiagaraTypeRegistry::Register(CollisionEventDef, false, true, false);

	FNiagaraTypeRegistry::Register(ParameterMapDef, true, false, false);
	FNiagaraTypeRegistry::Register(NumericDef, true, false, false);
	FNiagaraTypeRegistry::Register(FloatDef, true, true, false);
	FNiagaraTypeRegistry::Register(IntDef, true, true, false);
	FNiagaraTypeRegistry::Register(BoolDef, true, true, false);
	FNiagaraTypeRegistry::Register(Vec2Def, true, true, false);
	FNiagaraTypeRegistry::Register(Vec3Def, true, true, false);
	FNiagaraTypeRegistry::Register(Vec4Def, true, true, false);
	FNiagaraTypeRegistry::Register(ColorDef, true, true, false);
	FNiagaraTypeRegistry::Register(Matrix4Def, true, false, false);

	FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(ExecutionStateEnum), true, true, false);

	UScriptStruct* TestStruct = FindObjectChecked<UScriptStruct>(NiagaraPkg, TEXT("NiagaraTestStruct"));
	FNiagaraTypeDefinition TestDefinition(TestStruct);
	FNiagaraTypeRegistry::Register(TestDefinition, true, false, false);

	UScriptStruct* SpawnInfoStruct = FindObjectChecked<UScriptStruct>(NiagaraPkg, TEXT("NiagaraSpawnInfo"));
	FNiagaraTypeRegistry::Register(FNiagaraTypeDefinition(SpawnInfoStruct), true, false, false);

	const UNiagaraSettings* Settings = GetDefault<UNiagaraSettings>();
	check(Settings);
	TArray<FSoftObjectPath> TotalStructAssets;
	for (FSoftObjectPath AssetRef : Settings->AdditionalParameterTypes)
	{
		TotalStructAssets.AddUnique(AssetRef);
	}

	for (FSoftObjectPath AssetRef : Settings->AdditionalPayloadTypes)
	{
		TotalStructAssets.AddUnique(AssetRef);
	}

	for (FSoftObjectPath AssetRef : TotalStructAssets)
	{
		UObject* Obj = AssetRef.ResolveObject();
		if (Obj == nullptr)
		{
			Obj = AssetRef.TryLoad();
		}

		if (Obj != nullptr)
		{
			const FSoftObjectPath* ParamRefFound = Settings->AdditionalParameterTypes.FindByPredicate([&](const FSoftObjectPath& Ref) { return Ref.ToString() == AssetRef.ToString(); });
			const FSoftObjectPath* PayloadRefFound = Settings->AdditionalPayloadTypes.FindByPredicate([&](const FSoftObjectPath& Ref) { return Ref.ToString() == AssetRef.ToString(); });
			UScriptStruct* ScriptStruct = Cast<UScriptStruct>(Obj);
			if (ScriptStruct != nullptr)
			{
				FNiagaraTypeRegistry::Register(ScriptStruct, ParamRefFound != nullptr, PayloadRefFound != nullptr, true);
			}
		}
		else
		{
			UE_LOG(LogNiagara, Warning, TEXT("Could not find additional parameter/payload type: %s"), *AssetRef.ToString());
		}
	}


	for (FStringAssetReference AssetRef : Settings->AdditionalParameterEnums)
	{
		UObject* Obj = AssetRef.ResolveObject();
		if (Obj == nullptr)
		{
			Obj = AssetRef.TryLoad();
		}

		if (Obj != nullptr)
		{
			const FStringAssetReference* ParamRefFound = Settings->AdditionalParameterEnums.FindByPredicate([&](const FStringAssetReference& Ref) { return Ref.ToString() == AssetRef.ToString(); });
			const FStringAssetReference* PayloadRefFound = nullptr;
			UEnum* Enum = Cast<UEnum>(Obj);
			if (Enum != nullptr)
			{
				FNiagaraTypeRegistry::Register(Enum, ParamRefFound != nullptr, PayloadRefFound != nullptr, true);
			}
		}
		else
		{
			UE_LOG(LogNiagara, Warning, TEXT("Could not find additional parameter/payload enum: %s"), *AssetRef.ToString());
		}
	}

}

bool FNiagaraTypeDefinition::IsScalarDefinition(const FNiagaraTypeDefinition& Type)
{
	return ScalarStructs.Contains(Type.GetScriptStruct()) || (Type.GetScriptStruct() == IntStruct && Type.GetEnum() != nullptr);
}

bool FNiagaraTypeDefinition::TypesAreAssignable(const FNiagaraTypeDefinition& TypeA, const FNiagaraTypeDefinition& TypeB)
{
	if (const UClass* AClass = TypeA.GetClass())
	{
		if (const UClass* BClass = TypeB.GetClass())
		{
			return AClass == BClass;
			return true;
		}
	}
	
	if (const UClass* BClass = TypeB.GetClass())
	{
		return false;
	}

	if (const UClass* AClass = TypeA.GetClass())
	{
		return false;
	}

	// Make sure that enums are not assignable to enums of different types or just plain ints
	if (TypeA.GetStruct() == TypeB.GetStruct() &&
		TypeA.GetEnum() != TypeB.GetEnum())
	{
		return false;
	}

	if (TypeA.GetStruct() == TypeB.GetStruct())
	{
		return true;
	}

	bool bIsSupportedConversion = false;
	if (IsScalarDefinition(TypeA) && IsScalarDefinition(TypeB))
	{
		bIsSupportedConversion = (TypeA == IntDef && TypeB == FloatDef) || (TypeB == IntDef && TypeA == FloatDef);
	}
	else
	{
		bIsSupportedConversion = (TypeA == ColorDef && TypeB == Vec4Def) || (TypeB == ColorDef && TypeA == Vec4Def);
	}

	if (bIsSupportedConversion)
	{
		return true;
	}

	return	(TypeA == NumericDef && NumericStructs.Contains(TypeB.GetScriptStruct())) ||
			(TypeB == NumericDef && NumericStructs.Contains(TypeA.GetScriptStruct())) ||
			(TypeA == NumericDef && (TypeB.GetStruct() == GetIntStruct()) && TypeB.GetEnum() != nullptr) ||
			(TypeB == NumericDef && (TypeA.GetStruct() == GetIntStruct()) && TypeA.GetEnum() != nullptr);
}

bool FNiagaraTypeDefinition::IsLossyConversion(const FNiagaraTypeDefinition& TypeA, const FNiagaraTypeDefinition& TypeB)
{
	return (TypeA == IntDef && TypeB == FloatDef) || (TypeB == IntDef && TypeA == FloatDef);
}

FNiagaraTypeDefinition FNiagaraTypeDefinition::GetNumericOutputType(const TArray<FNiagaraTypeDefinition> TypeDefinintions, ENiagaraNumericOutputTypeSelectionMode SelectionMode)
{
	checkf(SelectionMode != ENiagaraNumericOutputTypeSelectionMode::None, TEXT("Can not get numeric output type with selection mode none."));

	//This may need some work. Should work fine for now.
	if (SelectionMode == ENiagaraNumericOutputTypeSelectionMode::Scalar)
	{
		bool bHasFloats = false;
		bool bHasInts = false;
		bool bHasBools = false;
		for (const FNiagaraTypeDefinition& Type : TypeDefinintions)
		{
			bHasFloats |= FloatStructs.Contains(Type.GetStruct());
			bHasInts |= IntStructs.Contains(Type.GetStruct());
			bHasBools |= BoolStructs.Contains(Type.GetStruct());
		}
		//Not sure what to do if we have multiple different types here.
		//Possibly pick this up ealier and throw a compile error?
		if (bHasFloats) return FNiagaraTypeDefinition::GetFloatDef();
		if (bHasInts) return FNiagaraTypeDefinition::GetIntDef();
		if (bHasBools) return FNiagaraTypeDefinition::GetBoolDef();
	}
	// Always return the numeric type definition if it's included since this isn't a valid use case and we don't want to hide it.
	int32 NumericTypeDefinitionIndex = TypeDefinintions.IndexOfByKey(NumericDef);
	if (NumericTypeDefinitionIndex != INDEX_NONE)
	{
		// TODO: Warning here?
		return NumericDef;
	}

	TArray<FNiagaraTypeDefinition> SortedTypeDefinitions = TypeDefinintions;
	SortedTypeDefinitions.Sort([&](const FNiagaraTypeDefinition& TypeA, const FNiagaraTypeDefinition& TypeB)
	{
		int32 AIndex = OrderedNumericTypes.IndexOfByKey(TypeA);
		int32 BIndex = OrderedNumericTypes.IndexOfByKey(TypeB);
		return AIndex < BIndex;
	});

	if (SelectionMode == ENiagaraNumericOutputTypeSelectionMode::Largest)
	{
		return SortedTypeDefinitions.Last();
	}
	else // if (SelectionMode == ENiagaraNumericOutputTypeSelectionMode::Smallest)
	{
		return SortedTypeDefinitions[0];
	}
}

//////////////////////////////////////////////////////////////////////////

void FNiagaraScriptDataInterfaceInfo::CopyTo(FNiagaraScriptDataInterfaceInfo* Destination, UObject* Outer) const
{
	Destination->Name = Name;
	Destination->DataInterface = Cast<UNiagaraDataInterface>(StaticDuplicateObject(DataInterface, Outer, NAME_None, ~RF_Transient));
	Destination->UserPtrIdx = UserPtrIdx;
}



FDelegateHandle INiagaraModule::SetOnProcessShaderCompilationQueue(FOnProcessQueue InOnProcessQueue)
{
	checkf(OnProcessQueue.IsBound() == false, TEXT("Shader processing queue delegate already set."));
	OnProcessQueue = InOnProcessQueue;
	return OnProcessQueue.GetHandle();
}

void INiagaraModule::ResetOnProcessShaderCompilationQueue(FDelegateHandle DelegateHandle)
{
	checkf(OnProcessQueue.GetHandle() == DelegateHandle, TEXT("Can only reset the process compilation queue delegate with the handle it was created with."));
	OnProcessQueue.Unbind();
}

void INiagaraModule::ProcessShaderCompilationQueue()
{
	checkf(OnProcessQueue.IsBound(), TEXT("Can not process shader queue.  Delegate was never set."));
	return OnProcessQueue.Execute();
}


