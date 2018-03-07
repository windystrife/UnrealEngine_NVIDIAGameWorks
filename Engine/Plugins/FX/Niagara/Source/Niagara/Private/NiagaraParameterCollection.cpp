// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "NiagaraParameterCollection.h"
#include "NiagaraDataInterface.h"

//////////////////////////////////////////////////////////////////////////

UNiagaraParameterCollectionInstance::UNiagaraParameterCollectionInstance(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ParameterStorage(this)
{
	//Bind(ParameterStorage);
}

UNiagaraParameterCollectionInstance::~UNiagaraParameterCollectionInstance()
{
	//Unbind(ParameterStorage);
}

void UNiagaraParameterCollectionInstance::PostLoad()
{
	Super::PostLoad();
	//Ensure we're synced up with our collection. TODO: Do conditionally via a version number/guid?
	SyncWithCollection();
}

void UNiagaraParameterCollectionInstance::SetParent(UNiagaraParameterCollection* InParent)
{
	Collection = InParent;
	SyncWithCollection();
}

bool UNiagaraParameterCollectionInstance::IsDefaultInstance()const
{
	return GetParent() && GetParent()->GetDefaultInstance() == this; 
}

bool UNiagaraParameterCollectionInstance::AddParameter(const FNiagaraVariable& Parameter)
{
	Modify();
	return ParameterStorage.AddParameter(Parameter);
}

bool UNiagaraParameterCollectionInstance::RemoveParameter(const FNiagaraVariable& Parameter)
{
	Modify();
	return ParameterStorage.RemoveParameter(Parameter);
}

void UNiagaraParameterCollectionInstance::RenameParameter(const FNiagaraVariable& Parameter, FName NewName)
{
	Modify();
	ParameterStorage.RenameParameter(Parameter, NewName);
}

void UNiagaraParameterCollectionInstance::Empty()
{
	Modify();
	ParameterStorage.Empty();
}

void UNiagaraParameterCollectionInstance::GetParameters(TArray<FNiagaraVariable>& OutParameters)
{
	ParameterStorage.GetParameters(OutParameters);
}

void UNiagaraParameterCollectionInstance::Tick()
{
	//Push our parameter changes to any bound stores.
	ParameterStorage.Tick();
}

void UNiagaraParameterCollectionInstance::SyncWithCollection()
{
	FNiagaraParameterStore OldStore = ParameterStorage;
	ParameterStorage.Empty(false);

	for (FNiagaraVariable& Param : Collection->GetParameters())
	{
		int32 Offset = OldStore.IndexOf(Param);
		if (Offset != INDEX_NONE && OverridesParameter(Param))
		{
			//If this paramter is in the old store and we're overriding it, use the existing value in the store.
			ParameterStorage.AddParameter(Param, false);
			if (Param.IsDataInterface())
			{
				ParameterStorage.SetDataInterface(OldStore.GetDataInterface(Offset), Param);
			}
			else
			{
				ParameterStorage.SetParameterData(OldStore.GetParameterData(Offset), ParameterStorage.IndexOf(Param), Param.GetSizeInBytes());
			}
		}
		else
		{
			//If the parameter did not exist in the old store or we don't override this parameter, sync it up to the parent collection.
			FNiagaraParameterStore& DefaultStore = Collection->GetDefaultInstance()->GetParameterStore();
			Offset = DefaultStore.IndexOf(Param);
			check(Offset != INDEX_NONE);

			ParameterStorage.AddParameter(Param, false);
			if (Param.IsDataInterface())
			{
				ParameterStorage.SetDataInterface(CastChecked<UNiagaraDataInterface>(StaticDuplicateObject(DefaultStore.GetDataInterface(Offset), this)), Param);
			}
			else
			{
				ParameterStorage.SetParameterData(DefaultStore.GetParameterData(Offset), ParameterStorage.IndexOf(Param), Param.GetSizeInBytes());
			}
		}
	}

	ParameterStorage.Rebind();
}

bool UNiagaraParameterCollectionInstance::OverridesParameter(const FNiagaraVariable& Parameter)const
{ 
	return IsDefaultInstance() || OverridenParameters.Contains(Parameter); 
}

void UNiagaraParameterCollectionInstance::SetOverridesParameter(const FNiagaraVariable& Parameter, bool bOverrides)
{
	if (bOverrides)
	{
		OverridenParameters.AddUnique(Parameter);
	}
	else
	{
		OverridenParameters.Remove(Parameter);
	}
}

//Blueprint Accessors
bool UNiagaraParameterCollectionInstance::GetBoolParameter(const FString& InVariableName)
{
	return ParameterStorage.GetParameterValue<bool>(FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), *Collection->ParameterNameFromFriendlyName(InVariableName)));
}

float UNiagaraParameterCollectionInstance::GetFloatParameter(const FString& InVariableName)
{
	return ParameterStorage.GetParameterValue<float>(FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), *Collection->ParameterNameFromFriendlyName(InVariableName)));
}

int32 UNiagaraParameterCollectionInstance::GetIntParameter(const FString& InVariableName)
{
	return ParameterStorage.GetParameterValue<int32>(FNiagaraVariable(FNiagaraTypeDefinition::GetIntStruct(), *Collection->ParameterNameFromFriendlyName(InVariableName)));
}

FVector2D UNiagaraParameterCollectionInstance::GetVector2DParameter(const FString& InVariableName)
{
	return ParameterStorage.GetParameterValue<FVector2D>(FNiagaraVariable(FNiagaraTypeDefinition::GetVec2Def(), *Collection->ParameterNameFromFriendlyName(InVariableName)));
}

FVector UNiagaraParameterCollectionInstance::GetVectorParameter(const FString& InVariableName)
{
	return ParameterStorage.GetParameterValue<FVector>(FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), *Collection->ParameterNameFromFriendlyName(InVariableName)));
}

FVector4 UNiagaraParameterCollectionInstance::GetVector4Parameter(const FString& InVariableName)
{
	return ParameterStorage.GetParameterValue<FVector4>(FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(), *Collection->ParameterNameFromFriendlyName(InVariableName)));
}

FLinearColor UNiagaraParameterCollectionInstance::GetColorParameter(const FString& InVariableName)
{
	return ParameterStorage.GetParameterValue<FLinearColor>(FNiagaraVariable(FNiagaraTypeDefinition::GetColorDef(), *Collection->ParameterNameFromFriendlyName(InVariableName)));
}

void UNiagaraParameterCollectionInstance::SetBoolParameter(const FString& InVariableName, bool InValue)
{
	ParameterStorage.SetParameterValue(InValue, FNiagaraVariable(FNiagaraTypeDefinition::GetBoolDef(), *Collection->ParameterNameFromFriendlyName(InVariableName)));
}

void UNiagaraParameterCollectionInstance::SetFloatParameter(const FString& InVariableName, float InValue)
{
	ParameterStorage.SetParameterValue(InValue, FNiagaraVariable(FNiagaraTypeDefinition::GetFloatDef(), *Collection->ParameterNameFromFriendlyName(InVariableName)));
}

void UNiagaraParameterCollectionInstance::SetIntParameter(const FString& InVariableName, int32 InValue)
{
	ParameterStorage.SetParameterValue(InValue, FNiagaraVariable(FNiagaraTypeDefinition::GetIntDef(), *Collection->ParameterNameFromFriendlyName(InVariableName)));
}

void UNiagaraParameterCollectionInstance::SetVector2DParameter(const FString& InVariableName, FVector2D InValue)
{
	ParameterStorage.SetParameterValue(InValue, FNiagaraVariable(FNiagaraTypeDefinition::GetVec2Def(), *Collection->ParameterNameFromFriendlyName(InVariableName)));
}

void UNiagaraParameterCollectionInstance::SetVectorParameter(const FString& InVariableName, FVector InValue)
{
	ParameterStorage.SetParameterValue(InValue, FNiagaraVariable(FNiagaraTypeDefinition::GetVec3Def(), *Collection->ParameterNameFromFriendlyName(InVariableName)));
}

void UNiagaraParameterCollectionInstance::SetVector4Parameter(const FString& InVariableName, const FVector4& InValue)
{
	ParameterStorage.SetParameterValue(InValue, FNiagaraVariable(FNiagaraTypeDefinition::GetVec4Def(), *Collection->ParameterNameFromFriendlyName(InVariableName)));
}

void UNiagaraParameterCollectionInstance::SetColorParameter(const FString& InVariableName, FLinearColor InValue)
{
	ParameterStorage.SetParameterValue(InValue, FNiagaraVariable(FNiagaraTypeDefinition::GetColorDef(), *Collection->ParameterNameFromFriendlyName(InVariableName)));
}

//////////////////////////////////////////////////////////////////////////

UNiagaraParameterCollection::UNiagaraParameterCollection(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DefaultInstance = ObjectInitializer.CreateDefaultSubobject<UNiagaraParameterCollectionInstance>(this, TEXT("Default Instance"));
	DefaultInstance->SetParent(this);
}

void UNiagaraParameterCollection::PostInitProperties()
{
	Super::PostInitProperties();
	//Unique name is cached off just in case the hash function changes or our name does.
	UniqueName = GetName() + LexicalConversion::ToString(GetTypeHash(GetFullName()));
}

void UNiagaraParameterCollection::PostLoad()
{
	Super::PostLoad();
}

int32 UNiagaraParameterCollection::IndexOfParameter(const FNiagaraVariable& Var)
{
	return Parameters.IndexOfByPredicate([&](const FNiagaraVariable& Other)
	{
		return Var.IsEquivalent(Other);
	});
}

int32 UNiagaraParameterCollection::AddParameter(FName Name, FNiagaraTypeDefinition Type)
{
	Modify();

	int32 Idx = Parameters.AddDefaulted(1);

	Parameters[Idx].SetName(Name);
	Parameters[Idx].SetType(Type);

	DefaultInstance->AddParameter(Parameters[Idx]);

	return Idx;
}

//void UNiagaraParameterCollection::RemoveParameter(int32 ParamIdx)
void UNiagaraParameterCollection::RemoveParameter(const FNiagaraVariable& Parameter)
{
	Modify();
	DefaultInstance->RemoveParameter(Parameter);
	Parameters.Remove(Parameter);
}

void UNiagaraParameterCollection::RenameParameter(FNiagaraVariable& Parameter, FName NewName)
{
	Modify();

	int32 ParamIdx = Parameters.IndexOfByKey(Parameter);
	check(ParamIdx != INDEX_NONE);

	Parameters[ParamIdx].SetName(NewName);
	DefaultInstance->RenameParameter(Parameter, NewName);
}

FString UNiagaraParameterCollection::GetUniqueName()const
{
	return UniqueName;
}

FNiagaraVariable UNiagaraParameterCollection::CollectionParameterFromFriendlyParameter(const FNiagaraVariable& FriendlyParameter)const
{
	return FNiagaraVariable(FriendlyParameter.GetType(), *ParameterNameFromFriendlyName(FriendlyParameter.GetName().ToString()));
}

FNiagaraVariable UNiagaraParameterCollection::FriendlyParameterFromCollectionParameter(const FNiagaraVariable& CollectionParameter)const
{
	return FNiagaraVariable(CollectionParameter.GetType(), *FriendlyNameFromParameterName(CollectionParameter.GetName().ToString()));
}

FString UNiagaraParameterCollection::FriendlyNameFromParameterName(FString ParameterName)const
{
	return ParameterName.Replace(*(GetUniqueName() + TEXT("_")), TEXT(""), ESearchCase::CaseSensitive);
}

FString UNiagaraParameterCollection::ParameterNameFromFriendlyName(FString FriendlyName)const
{
	return GetUniqueName() + TEXT("_") + FriendlyName;
}