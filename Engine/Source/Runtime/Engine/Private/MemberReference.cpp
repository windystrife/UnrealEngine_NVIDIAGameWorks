// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/MemberReference.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/CoreRedirects.h"

#if WITH_EDITOR
#include "Kismet2/BlueprintEditorUtils.h"
#include "BlueprintCompilationManager.h"
#endif

extern COREUOBJECT_API bool GBlueprintUseCompilationManager;

//////////////////////////////////////////////////////////////////////////
// FMemberReference

void FMemberReference::SetExternalMember(FName InMemberName, TSubclassOf<class UObject> InMemberParentClass)
{
	MemberName = InMemberName;
#if WITH_EDITOR
	MemberParent = (InMemberParentClass != nullptr) ? InMemberParentClass->GetAuthoritativeClass() : nullptr;
#else
	MemberParent = *InMemberParentClass;
#endif
	MemberScope.Empty();
	bSelfContext = false;
	bWasDeprecated = false;
}

void FMemberReference::SetExternalMember(FName InMemberName, TSubclassOf<class UObject> InMemberParentClass, FGuid& InMemberGuid)
{
	SetExternalMember(InMemberName, InMemberParentClass);
	MemberGuid = InMemberGuid;
}

void FMemberReference::SetGlobalField(FName InFieldName, UPackage* InParentPackage)
{
	MemberName = InFieldName;
	MemberParent = InParentPackage;
	MemberScope.Empty();
	bSelfContext = false;
	bWasDeprecated = false;
}

void FMemberReference::SetExternalDelegateMember(FName InMemberName)
{
	SetExternalMember(InMemberName, nullptr);
}

void FMemberReference::SetSelfMember(FName InMemberName)
{
	MemberName = InMemberName;
	MemberParent = NULL;
	MemberScope.Empty();
	bSelfContext = true;
	bWasDeprecated = false;
}

void FMemberReference::SetSelfMember(FName InMemberName, FGuid& InMemberGuid)
{
	SetSelfMember(InMemberName);
	MemberGuid = InMemberGuid;
}

void FMemberReference::SetDirect(const FName InMemberName, const FGuid InMemberGuid, TSubclassOf<class UObject> InMemberParentClass, bool bIsConsideredSelfContext)
{
	MemberName = InMemberName;
	MemberGuid = InMemberGuid;
	bSelfContext = bIsConsideredSelfContext;
	bWasDeprecated = false;
	MemberParent = InMemberParentClass;
	MemberScope.Empty();
}

void FMemberReference::SetGivenSelfScope(const FName InMemberName, const FGuid InMemberGuid, TSubclassOf<class UObject> InMemberParentClass, TSubclassOf<class UObject> SelfScope) const
{
	MemberName = InMemberName;
	MemberGuid = InMemberGuid;
	MemberParent = (InMemberParentClass != nullptr) ? InMemberParentClass->GetAuthoritativeClass() : nullptr;
	MemberScope.Empty();

	// SelfScope should always be valid, but if it's not ensure and move on, the node will be treated as if it's not self scoped.
	ensure(SelfScope);
	bSelfContext = SelfScope && ((SelfScope->IsChildOf(InMemberParentClass)) || (SelfScope->ClassGeneratedBy == InMemberParentClass->ClassGeneratedBy));
	bWasDeprecated = false;

	if (bSelfContext)
	{
		MemberParent = NULL;
	}
}

void FMemberReference::SetLocalMember(FName InMemberName, UStruct* InScope, const FGuid InMemberGuid)
{
	SetLocalMember(InMemberName, InScope->GetName(), InMemberGuid);
}

void FMemberReference::SetLocalMember(FName InMemberName, FString InScopeName, const FGuid InMemberGuid)
{
	MemberName = InMemberName;
	MemberScope = InScopeName;
	MemberGuid = InMemberGuid;
	bSelfContext = false;
}

void FMemberReference::InvalidateScope()
{
	if( IsSelfContext() )
	{
		MemberParent = NULL;
	}
	else if(IsLocalScope())
	{
		MemberScope.Empty();

		// Make it into a member reference since we are clearing the local context
		bSelfContext = true;
	}
}

#if WITH_EDITOR

FString FMemberReference::GetReferenceSearchString(UClass* InFieldOwner) const
{
	if (!IsLocalScope())
	{
		if (InFieldOwner)
		{
			if (MemberGuid.IsValid())
			{
				return FString::Printf(TEXT("Nodes(VariableReference(MemberName=+\"%s\" && MemberGuid(A=%i && B=%i && C=%i && D=%i) ))"), *MemberName.ToString(), MemberGuid.A, MemberGuid.B, MemberGuid.C, MemberGuid.D);
			}
			else
			{
				FString ExportMemberParentName;
				ExportMemberParentName = InFieldOwner->GetClass()->GetName();
				ExportMemberParentName.AppendChar('\'');
				ExportMemberParentName += InFieldOwner->GetAuthoritativeClass()->GetPathName();
				ExportMemberParentName.AppendChar('\'');

				return FString::Printf(TEXT("Nodes(VariableReference(MemberName=+\"%s\" && (MemberParent=\"%s\" || bSelfContext=true) ))"), *MemberName.ToString(), *ExportMemberParentName);
			}
		}
		else if (MemberGuid.IsValid())
		{
			return FString::Printf(TEXT("Nodes(VariableReference(MemberName=+\"%s\" && MemberGuid(A=%i && B=%i && C=%i && D=%i)))"), *MemberName.ToString(), MemberGuid.A, MemberGuid.B, MemberGuid.C, MemberGuid.D);
		}
		else
		{
			return FString::Printf(TEXT("Nodes(VariableReference(MemberName=+\"%s\"))"), *MemberName.ToString());
		}
	}
	else
	{
		return FString::Printf(TEXT("Nodes(VariableReference(MemberName=+\"%s\" && MemberScope=+\"%s\"))"), *MemberName.ToString(), *GetMemberScopeName());
	}
}

FName FMemberReference::RefreshLocalVariableName(UClass* InSelfScope) const
{
	TArray<UBlueprint*> Blueprints;
	UBlueprint::GetBlueprintHierarchyFromClass(InSelfScope, Blueprints);

	FName RenamedMemberName = NAME_None;
	for (int32 BPIndex = 0; BPIndex < Blueprints.Num(); ++BPIndex)
	{
		RenamedMemberName = FBlueprintEditorUtils::FindLocalVariableNameByGuid(Blueprints[BPIndex], MemberGuid);
		if (RenamedMemberName != NAME_None)
		{
			MemberName = RenamedMemberName;
			break;
		}
	}
	return RenamedMemberName;
}

bool FMemberReference::bFieldRedirectMapInitialized = false;

void FMemberReference::InitFieldRedirectMap()
{
	// Soft deprecated, replaced by FCoreRedirects but will read the old ini format for the foreseeable future
	if (!bFieldRedirectMapInitialized)
	{
		if (GConfig)
		{
			TArray<FCoreRedirect> NewRedirects;

			FConfigSection* PackageRedirects = GConfig->GetSectionPrivate( TEXT("/Script/Engine.Engine"), false, true, GEngineIni );
			for (FConfigSection::TIterator It(*PackageRedirects); It; ++It)
			{
				if (It.Key() == TEXT("K2FieldRedirects"))
				{
					FString OldFieldPathString;
					FString NewFieldPathString;

					FParse::Value( *It.Value().GetValue(), TEXT("OldFieldName="), OldFieldPathString );
					FParse::Value( *It.Value().GetValue(), TEXT("NewFieldName="), NewFieldPathString );

					// Add both a Property and Function redirect, as we don't know which it's trying to be with the old syntax
					FCoreRedirect* PropertyRedirect = new (NewRedirects) FCoreRedirect(ECoreRedirectFlags::Type_Property, OldFieldPathString, NewFieldPathString);
					FCoreRedirect* FunctionRedirect = new (NewRedirects) FCoreRedirect(ECoreRedirectFlags::Type_Function, OldFieldPathString, NewFieldPathString);
				}			
				if (It.Key() == TEXT("K2ParamRedirects"))
				{
					// Ignore NodeName/Title as it's not useful
					FName OldParam = NAME_None;
					FName NewParam = NAME_None;

					FString OldParamValues;
					FString NewParamValues;
					FString CustomValueMapping;

					FParse::Value( *It.Value().GetValue(), TEXT("OldParamName="), OldParam );
					FParse::Value( *It.Value().GetValue(), TEXT("NewParamName="), NewParam );
					FParse::Value( *It.Value().GetValue(), TEXT("OldParamValues="), OldParamValues );
					FParse::Value( *It.Value().GetValue(), TEXT("NewParamValues="), NewParamValues );
					FParse::Value( *It.Value().GetValue(), TEXT("CustomValueMapping="), CustomValueMapping );

					TArray<FString> OldParamValuesList;
					TArray<FString> NewParamValuesList;
					OldParamValues.ParseIntoArray(OldParamValuesList, TEXT(";"), false);
					NewParamValues.ParseIntoArray(NewParamValuesList, TEXT(";"), false);

					if (OldParamValuesList.Num() != NewParamValuesList.Num())
					{
						UE_LOG(LogBlueprint, Warning, TEXT("Unequal lengths for old and new param values for  param redirect '%s' to '%s'."), *(OldParam.ToString()), *(NewParam.ToString()));
					}

					if (CustomValueMapping.Len() > 0 && (OldParamValuesList.Num() > 0 || NewParamValuesList.Num() > 0))
					{
						UE_LOG(LogBlueprint, Warning, TEXT("Both Custom and Automatic param value remapping specified for param redirect '%s' to '%s'.  Only Custom will be applied."), *(OldParam.ToString()), *(NewParam.ToString()));
					}

					FCoreRedirect* Redirect = new (NewRedirects) FCoreRedirect(ECoreRedirectFlags::Type_Property, OldParam.ToString(), NewParam.ToString());

					for (int32 i = FMath::Min(OldParamValuesList.Num(), NewParamValuesList.Num()) - 1; i >= 0; --i)
					{
						int32 CurSize = Redirect->ValueChanges.Num();
						Redirect->ValueChanges.Add(OldParamValuesList[i], NewParamValuesList[i]);
						if (CurSize == Redirect->ValueChanges.Num())
						{
							UE_LOG(LogBlueprint, Warning, TEXT("Duplicate old param value '%s' for param redirect '%s' to '%s'."), *(OldParamValuesList[i]), *(OldParam.ToString()), *(NewParam.ToString()));
						}
					}
				}			
			}

			FCoreRedirects::AddRedirectList(NewRedirects, GEngineIni);
			bFieldRedirectMapInitialized = true;
		}
	}
}

UClass* FMemberReference::GetClassToUse(UClass* InClass, bool bUseUpToDateClass)
{
	if(GBlueprintUseCompilationManager && bUseUpToDateClass)
	{
		return FBlueprintEditorUtils::GetMostUpToDateClass(InClass);
	}
	else
	{
		return InClass;
	}
}

UField* FMemberReference::FindRemappedField(UClass *FieldClass, UClass* InitialScope, FName InitialName, bool bInitialScopeMustBeOwnerOfField)
{
	DECLARE_SCOPE_CYCLE_COUNTER(TEXT("FMemberReference::FindRemappedField"), STAT_LinkerLoad_FindRemappedField, STATGROUP_LoadTimeVerbose);

	InitFieldRedirectMap();

	// In the case of a bifurcation of a variable (e.g. moved from a parent into certain children), verify that we don't also define the variable in the current scope first
	if (FindField<UField>(InitialScope, InitialName) != nullptr)
	{
		return nullptr;
	}

	// Step up the class chain to check if us or any of our parents specify a redirect
	UClass* TestRemapClass = InitialScope;
	while (TestRemapClass != nullptr)
	{
		UClass* SearchClass = TestRemapClass;

		FName NewFieldName;

		FCoreRedirectObjectName OldRedirectName = FCoreRedirectObjectName(InitialName, TestRemapClass->GetFName(), TestRemapClass->GetOutermost()->GetFName());
		FCoreRedirectObjectName NewRedirectName = FCoreRedirects::GetRedirectedName(FCoreRedirects::GetFlagsForTypeClass(FieldClass), OldRedirectName);

		if (NewRedirectName != OldRedirectName)
		{
			NewFieldName = NewRedirectName.ObjectName;

			if (OldRedirectName.OuterName != NewRedirectName.OuterName)
			{
				// Try remapping class, this only works if class is in memory
				FString ClassName = NewRedirectName.OuterName.ToString();

				if (NewRedirectName.PackageName != NAME_Name)
				{
					// Use package if it's there
					ClassName = FString::Printf(TEXT("%s.%s"), *NewRedirectName.PackageName.ToString(), *NewRedirectName.OuterName.ToString());
				}

				SearchClass = (UClass*)StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *ClassName);

				if (!SearchClass)
				{
					UE_LOG(LogBlueprint, Log, TEXT("UK2Node:  Unable to find updated field name for '%s' on unknown class '%s'."), *InitialName.ToString(), *ClassName);
					return nullptr;
				}
			}
		}

		if (NewFieldName != NAME_None)
		{
			// Find the actual field specified by the redirector, so we can return it and update the node that uses it
			UField* NewField = FindField<UField>(SearchClass, NewFieldName);
			if (NewField != nullptr)
			{
				if (bInitialScopeMustBeOwnerOfField && !InitialScope->IsChildOf(SearchClass))
				{
					UE_LOG(LogBlueprint, Log, TEXT("UK2Node:  Unable to update field. Remapped field '%s' in not owned by given scope. Scope: '%s', Owner: '%s'."), *InitialName.ToString(), *InitialScope->GetName(), *NewFieldName.ToString());
				}
				else
				{
					UE_LOG(LogBlueprint, Log, TEXT("UK2Node:  Fixed up old field '%s' to new name '%s' on class '%s'."), *InitialName.ToString(), *NewFieldName.ToString(), *SearchClass->GetName());
					return NewField;
				}
			}
			else
			{
				UE_LOG(LogBlueprint, Log, TEXT("UK2Node:  Unable to find updated field name for '%s' on class '%s'."), *InitialName.ToString(), *SearchClass->GetName());
			}

			return nullptr;
		}

		TestRemapClass = TestRemapClass->GetSuperClass();
	}

	return nullptr;
}

#endif
