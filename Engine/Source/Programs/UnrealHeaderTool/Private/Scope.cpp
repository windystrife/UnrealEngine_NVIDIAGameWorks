// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Scope.h"
#include "UnrealHeaderTool.h"
#include "UObject/ErrorException.h"
#include "ParserHelper.h"

extern FCompilerMetadataManager GScriptHelper;

FScope::FScope(FScope* InParent)
	: Parent(InParent)
{ }

FScope::FScope()
	: Parent(nullptr)
{

}

void FScope::AddType(UField* Type)
{
	TypeMap.Add(Type->GetFName(), Type);
}

/**
 * Dispatch type to one of three arrays Enums, Structs and DelegateFunctions.
 *
 * @param Type Input type.
 * @param Enums (Output parameter) Array to fill with enums.
 * @param Structs (Output parameter) Array to fill with structs.
 * @param DelegateFunctions (Output parameter) Array to fill with delegate functions.
 */
void DispatchType(UField* Type, TArray<UEnum*> &Enums, TArray<UScriptStruct*> &Structs, TArray<UDelegateFunction*> &DelegateFunctions)
{
	UClass* TypeClass = Type->GetClass();

	if (TypeClass == UClass::StaticClass() || TypeClass == UStruct::StaticClass())
	{
		// Inner scopes.
		FScope::GetTypeScope((UStruct*)Type)->SplitTypesIntoArrays(Enums, Structs, DelegateFunctions);
	}
	else if (TypeClass == UEnum::StaticClass())
	{
		UEnum* Enum = (UEnum*)Type;
		Enums.Add(Enum);
	}
	else if (TypeClass == UScriptStruct::StaticClass())
	{
		UScriptStruct* Struct = (UScriptStruct*)Type;
		Structs.Add(Struct);
	}
	else if (TypeClass == UDelegateFunction::StaticClass())
	{
		bool bAdded = false;
		UDelegateFunction* Function = (UDelegateFunction*)Type;

		if (Function->GetSuperFunction() == NULL)
		{
			DelegateFunctions.Add(Function);
			bAdded = true;
		}

		check(bAdded);
	}
}

void FScope::SplitTypesIntoArrays(TArray<UEnum*>& Enums, TArray<UScriptStruct*>& Structs, TArray<UDelegateFunction*>& DelegateFunctions)
{
	for (auto& TypePair : TypeMap)
	{
		auto* Type = TypePair.Value;
		DispatchType(Type, Enums, Structs, DelegateFunctions);
	}
}

TSharedRef<FScope> FScope::GetTypeScope(UStruct* Type)
{
	auto* ScopeRefPtr = ScopeMap.Find(Type);
	if (!ScopeRefPtr)
	{
		FError::Throwf(TEXT("Couldn't find scope for the type %s."), *Type->GetName());
	}

	return *ScopeRefPtr;
}

TSharedRef<FScope> FScope::AddTypeScope(UStruct* Type, FScope* ParentScope)
{
	FStructScope* ScopePtr = new FStructScope(Type, ParentScope);
	TSharedRef<FScope> Scope = MakeShareable(ScopePtr);

	ScopeMap.Add(Type, Scope);

	return Scope;
}

UField* FScope::FindTypeByName(FName Name)
{
	auto TypeIterator = TDeepScopeTypeIterator<UField, false>(this);

	while (TypeIterator.MoveNext())
	{
		auto* Type = *TypeIterator;
		if (Type->GetFName() == Name)
		{
			return Type;
		}
	}

	return nullptr;
}

const UField* FScope::FindTypeByName(FName Name) const
{
	auto TypeIterator = GetTypeIterator();

	while (TypeIterator.MoveNext())
	{
		auto* Type = *TypeIterator;
		if (Type->GetFName() == Name)
		{
			return Type;
		}
	}

	return nullptr;
}

bool FScope::ContainsType(UField* Type)
{
	return FindTypeByName(Type->GetFName()) != nullptr;
}

bool FScope::IsFileScope() const
{
	return Parent == nullptr;
}

bool FScope::ContainsTypes() const
{
	return TypeMap.Num() > 0;
}

FFileScope* FScope::GetFileScope()
{
	FScope* CurrentScope = this;
	while (!CurrentScope->IsFileScope())
	{
		CurrentScope = const_cast<FScope*>(CurrentScope->GetParent());
	}

	return CurrentScope->AsFileScope();
}

TMap<UStruct*, TSharedRef<FScope> > FScope::ScopeMap;

FFileScope::FFileScope(FName InName, FUnrealSourceFile* InSourceFile)
	: SourceFile(InSourceFile), Name(InName)
{ }

void FFileScope::IncludeScope(FFileScope* IncludedScope)
{
	IncludedScopes.Add(IncludedScope);
}

FUnrealSourceFile* FFileScope::GetSourceFile() const
{
	return SourceFile;
}

FName FFileScope::GetName() const
{
	return Name;
}

UStruct* FStructScope::GetStruct() const
{
	return Struct;
}

FName FStructScope::GetName() const
{
	return Struct->GetFName();
}

FStructScope::FStructScope(UStruct* InStruct, FScope* InParent)
	: FScope(InParent), Struct(InStruct)
{

}
