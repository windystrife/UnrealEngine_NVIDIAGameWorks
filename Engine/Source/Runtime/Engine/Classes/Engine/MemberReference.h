// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "UObject/Class.h"
#include "Templates/SubclassOf.h"
#include "UObject/UnrealType.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"
#include "Engine/Blueprint.h"
#include "MemberReference.generated.h"

/** Helper struct to allow us to redirect properties and functions through renames and additionally between classes if necessary */
struct FFieldRemapInfo
{
	/** The new name of the field after being renamed */
	FName FieldName;

	/** The new name of the field's outer class if different from its original location, or NAME_None if it hasn't moved */
	FName FieldClass;

	bool operator==(const FFieldRemapInfo& Other) const
	{
		return FieldName == Other.FieldName && FieldClass == Other.FieldClass;
	}

	friend uint32 GetTypeHash(const FFieldRemapInfo& RemapInfo)
	{
		return GetTypeHash(RemapInfo.FieldName) + GetTypeHash(RemapInfo.FieldClass) * 23;
	}

	FFieldRemapInfo()
		: FieldName(NAME_None)
		, FieldClass(NAME_None)
	{
	}
};

/** Helper struct to allow us to redirect pin name for node class */
struct FParamRemapInfo
{
	bool	bCustomValueMapping;
	FName	OldParam;
	FName	NewParam;
	FName	NodeTitle;
	TMap<FString, FString> ParamValueMap;

	// constructor
	FParamRemapInfo()
		: OldParam(NAME_None)
		, NewParam(NAME_None)
		, NodeTitle(NAME_None)
	{
	}
};

// @TODO: this can encapsulate globally defined fields as well (like with native 
//        delegate signatures); consider renaming to FFieldReference
USTRUCT()
struct FMemberReference
{
	GENERATED_USTRUCT_BODY()

protected:
	/** 
	 * Most often the Class that this member is defined in. Could be a UPackage 
	 * if it is a native delegate signature function (declared globally). Should 
	 * be NULL if bSelfContext is true.  
	 */
	UPROPERTY()
	mutable UObject* MemberParent;

	/**  */
	UPROPERTY()
	mutable FString MemberScope;

	/** Name of variable */
	UPROPERTY()
	mutable FName MemberName;

	/** The Guid of the variable */
	UPROPERTY()
	mutable FGuid MemberGuid;

	/** Whether or not this should be a "self" context */
	UPROPERTY()
	mutable bool bSelfContext;

	/** Whether or not this property has been deprecated */
	UPROPERTY()
	mutable bool bWasDeprecated;
	
public:
	FMemberReference()
		: MemberParent(NULL)
		, MemberName(NAME_None)
		, bSelfContext(false)
	{
	}

	/** Set up this reference from a supplied field */
	template<class TFieldType>
	void SetFromField(const UField* InField, const bool bIsConsideredSelfContext)
	{
		UClass* OwnerClass = InField->GetOwnerClass();
		MemberParent = OwnerClass;

		if (bIsConsideredSelfContext)
		{
			MemberParent = nullptr;
		}
		else if ((MemberParent == nullptr) && InField->GetName().EndsWith(HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX))
		{
			MemberParent = InField->GetOutermost();
		}

		MemberName = InField->GetFName();
		bSelfContext = bIsConsideredSelfContext;
		bWasDeprecated = false;

#if WITH_EDITOR
		if (UClass* ParentAsClass = GetMemberParentClass())
		{
			MemberParent = ParentAsClass->GetAuthoritativeClass();
		}

		MemberGuid.Invalidate();
		if (OwnerClass != nullptr)
		{
			UBlueprint::GetGuidFromClassByFieldName<TFieldType>(OwnerClass, InField->GetFName(), MemberGuid);
		}
#endif
	}

	template<class TFieldType>
	void SetFromField(const UField* InField, UClass* SelfScope)
	{
		UClass* OwnerClass = InField->GetOwnerClass();

		FGuid FieldGuid;
#if WITH_EDITOR
		if (OwnerClass != nullptr)
		{
			UBlueprint::GetGuidFromClassByFieldName<TFieldType>(OwnerClass, InField->GetFName(), FieldGuid);
		}
#endif

		SetGivenSelfScope(InField->GetFName(), FieldGuid, OwnerClass, SelfScope);
	}

	/** Update given a new self */
	template<class TFieldType>
	void RefreshGivenNewSelfScope(UClass* SelfScope)
	{
		UClass* ParentAsClass = GetMemberParentClass();
		if ((ParentAsClass != NULL) && (SelfScope != NULL))
		{
#if WITH_EDITOR
			UBlueprint::GetGuidFromClassByFieldName<TFieldType>((ParentAsClass), MemberName, MemberGuid);
#endif
			SetGivenSelfScope(MemberName, MemberGuid, ParentAsClass, SelfScope);
		}
		else
		{
			// We no longer have enough information to known if we've done the right thing, and just have to hope...
		}
	}

	/** Set to a non-'self' member, so must include reference to class owning the member. */
	ENGINE_API void SetExternalMember(FName InMemberName, TSubclassOf<class UObject> InMemberParentClass);
	ENGINE_API void SetExternalMember(FName InMemberName, TSubclassOf<class UObject> InMemberParentClass, FGuid& InMemberGuid);

	/** Set to reference a global field (intended for things like natively defined delegate signatures) */
	ENGINE_API void SetGlobalField(FName InFieldName, UPackage* InParentPackage);

	/** Set to a non-'self' delegate member, this is not self-context but is not given a parent class */
	ENGINE_API void SetExternalDelegateMember(FName InMemberName);

	/** Set up this reference to a 'self' member name */
	ENGINE_API void SetSelfMember(FName InMemberName);
	ENGINE_API void SetSelfMember(FName InMemberName, FGuid& InMemberGuid);

	/** Set up this reference to a 'self' member name, scoped to a struct */
	ENGINE_API void SetLocalMember(FName InMemberName, UStruct* InScope, const FGuid InMemberGuid);

	/** Set up this reference to a 'self' member name, scoped to a struct name */
	ENGINE_API void SetLocalMember(FName InMemberName, FString InScopeName, const FGuid InMemberGuid);

	/** Only intended for backwards compat! */
	ENGINE_API void SetDirect(const FName InMemberName, const FGuid InMemberGuid, TSubclassOf<class UObject> InMemberParentClass, bool bIsConsideredSelfContext);

	/** Invalidate the current MemberParent, if this is a self scope, or the MemberScope if it is not (and set).  Intended for PostDuplication fixups */
	ENGINE_API void InvalidateScope();

	/** Get the name of this member */
	FName GetMemberName() const
	{
		return MemberName;
	}

	FGuid GetMemberGuid() const
	{
		return MemberGuid;
	}

	UClass* GetMemberParentClass() const
	{
		return Cast<UClass>(MemberParent);
	}

	UPackage* GetMemberParentPackage() const
	{
		if (UPackage* ParentAsPackage = Cast<UPackage>(MemberParent))
		{
			return ParentAsPackage;
		}
		else if (MemberParent != nullptr)
		{
			return MemberParent->GetOutermost();
		}
		return nullptr;
	}

	/** Returns if this is a 'self' context. */
	bool IsSelfContext() const
	{
		return bSelfContext;
	}

	/** Returns if this is a local scope. */
	bool IsLocalScope() const
	{
		return !MemberScope.IsEmpty();
	}

#if WITH_EDITOR
	/**
	 * Returns a search string to submit to Find-in-Blueprints to find references to this reference
	 *
	 * @param InFieldOwner		The owner of the field, cannot be resolved internally
	 * @return					Search string to find this reference in other Blueprints
	 */
	ENGINE_API FString GetReferenceSearchString(UClass* InFieldOwner) const;
#endif

private:
#if WITH_EDITOR
	/**
	 * Refreshes a local variable reference name if it has changed
	 *
	 * @param InSelfScope		Scope to lookup the variable in, to see if it has changed
	 */
	ENGINE_API FName RefreshLocalVariableName(UClass* InSelfScope) const;
#endif

protected:
	/** Only intended for backwards compat! */
	ENGINE_API void SetGivenSelfScope(const FName InMemberName, const FGuid InMemberGuid, TSubclassOf<class UObject> InMemberParentClass, TSubclassOf<class UObject> SelfScope) const;

public:

	/** Get the class that owns this member */
	UClass* GetMemberParentClass(UClass* SelfScope) const
	{
		// Local variables with a MemberScope act much the same as being SelfContext, their parent class is SelfScope.
		return (bSelfContext || !MemberScope.IsEmpty())? SelfScope : GetMemberParentClass();
	}

	/** Get the scope of this member */
	UStruct* GetMemberScope(UClass* InMemberParentClass) const
	{
		return FindField<UStruct>(InMemberParentClass, *MemberScope);
	}

	/** Get the name of the scope of this member */
	FString GetMemberScopeName() const
	{
		return MemberScope;
	}

	/** Compares with another MemberReference to see if they are identical */
	bool IsSameReference(FMemberReference& InReference)
	{
		return 
			bSelfContext == InReference.bSelfContext &&
			MemberParent == InReference.MemberParent &&
			MemberName == InReference.MemberName &&
			MemberGuid == InReference.MemberGuid &&
			MemberScope == InReference.MemberScope;
	}

	/** Returns whether or not the variable has been deprecated */
	bool IsDeprecated() const
	{
		return bWasDeprecated;
	}

	/** 
	 *	Returns the member UProperty/UFunction this reference is pointing to, or NULL if it no longer exists 
	 *	Derives 'self' scope from supplied Blueprint node if required
	 *	Will check for redirects and fix itself up if one is found.
	 */
	template<class TFieldType>
	TFieldType* ResolveMember(UClass* SelfScope) const
	{
		TFieldType* ReturnField = NULL;

		bool bUseUpToDateClass = SelfScope && SelfScope->GetAuthoritativeClass() != SelfScope;

		if(bSelfContext && SelfScope == NULL)
		{
			UE_LOG(LogBlueprint, Warning, TEXT("FMemberReference::ResolveMember (%s) bSelfContext == true, but no scope supplied!"), *MemberName.ToString() );
		}

		// Check if the member reference is function scoped
		if(IsLocalScope())
		{
			UStruct* MemberScopeStruct = FindField<UStruct>(SelfScope, *MemberScope);

			// Find in target scope
			ReturnField = FindField<TFieldType>(MemberScopeStruct, MemberName);

#if WITH_EDITOR
			if(ReturnField == NULL)
			{
				// If the property was not found, refresh the local variable name and try again
				const FName RenamedMemberName = RefreshLocalVariableName(SelfScope);
				if (RenamedMemberName != NAME_None)
				{
					ReturnField = FindField<TFieldType>(MemberScopeStruct, MemberName);
				}
			}
#endif
		}
		else
		{
			// Look for remapped member
			UClass* TargetScope = bSelfContext ? SelfScope : GetMemberParentClass();
#if WITH_EDITOR
			if( TargetScope != NULL &&  !GIsSavingPackage )
			{
				ReturnField = FindRemappedField<TFieldType>(TargetScope, MemberName, true);
			}

			if(ReturnField != NULL)
			{
				// Fix up this struct, we found a redirect
				MemberName = ReturnField->GetFName();
				MemberParent = Cast<UClass>(ReturnField->GetOuter());

				MemberGuid.Invalidate();
				UBlueprint::GetGuidFromClassByFieldName<TFieldType>(TargetScope, MemberName, MemberGuid);

				if (UClass* ParentAsClass = GetMemberParentClass())
				{
					ParentAsClass = ParentAsClass->GetAuthoritativeClass();
					MemberParent  = ParentAsClass;

					// Re-evaluate self-ness against the redirect if we were given a valid SelfScope
					if (SelfScope != NULL)
					{
						SetGivenSelfScope(MemberName, MemberGuid, ParentAsClass, SelfScope);
					}
				}	
			}
			else
#endif
			if(TargetScope != NULL)
			{
#if WITH_EDITOR
				TargetScope = GetClassToUse(TargetScope, bUseUpToDateClass);
#endif
				// Find in target scope
				ReturnField = FindField<TFieldType>(TargetScope, MemberName);

#if WITH_EDITOR

				// If the reference variable is valid we need to make sure that our GUID matches
				if (ReturnField != NULL)
				{
					UBlueprint::GetGuidFromClassByFieldName<TFieldType>(TargetScope, MemberName, MemberGuid);
				}
				// If we have a GUID find the reference variable and make sure the name is up to date and find the field again
				// For now only variable references will have valid GUIDs.  Will have to deal with finding other names subsequently
				else if (MemberGuid.IsValid())
				{
					const FName RenamedMemberName = UBlueprint::GetFieldNameFromClassByGuid<TFieldType>(TargetScope, MemberGuid);
					if (RenamedMemberName != NAME_None)
					{
						MemberName = RenamedMemberName;
						ReturnField = FindField<TFieldType>(TargetScope, MemberName);
					}
				}
#endif
			}
			else if (UPackage* TargetPackage = GetMemberParentPackage())
			{
				ReturnField = FindObject<TFieldType>(TargetPackage, *MemberName.ToString());
			}
			// For backwards compatibility: as of CL 2412156, delegate signatures 
			// could have had a null MemberParentClass (for those natively 
			// declared outside of a class), we used to rely on the following 
			// FindObject<>; however this was not reliable (hence the addition 
			// of GetMemberParentPackage(), etc.)
			else if ((TFieldType::StaticClass() == UFunction::StaticClass()) && MemberName.ToString().EndsWith(HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX))
			{			
				FString const StringName = MemberName.ToString();
				for (TObjectIterator<UPackage> PackageIt; PackageIt && (ReturnField == nullptr); ++PackageIt)
				{
					if (PackageIt->HasAnyPackageFlags(PKG_CompiledIn) == false)
					{
						continue;
					}

					// NOTE: this could return the wrong field (if there are 
					//       two like-named delegates defined in separate packages)
					ReturnField = FindObject<TFieldType>(*PackageIt, *StringName);
				}

				if (ReturnField != nullptr)
				{
					UE_LOG(LogBlueprint, Display, TEXT("Generic delegate signature ref (%s). Explicitly setting it to: '%s'. Make sure this is correct (there could be multiple native delegate types with this name)."), *StringName, *ReturnField->GetPathName());
					MemberParent = ReturnField->GetOutermost();
				}
			}
		}

		// Check to see if the member has been deprecated
		if (UProperty* Property = Cast<UProperty>(ReturnField))
		{
			bWasDeprecated = Property->HasAnyPropertyFlags(CPF_Deprecated);
		}

		return ReturnField;
	}

	template<class TFieldType>
	TFieldType* ResolveMember(UBlueprint* SelfScope)
	{
		return ResolveMember<TFieldType>(SelfScope->SkeletonGeneratedClass);
	}

#if WITH_EDITOR
	/**
	 * Searches the field redirect map for the specified named field in the scope, and returns the remapped field if found
	 *
	 * @param	FieldClass		UClass of field type we are looking for
	 * @param	InitialScope	The scope the field was initially defined in.  The function will search up into parent scopes to attempt to find remappings
	 * @param	InitialName		The name of the field to attempt to find a redirector for
	 * @param	bInitialScopeMustBeOwnerOfField		if true the InitialScope must be Child of the field's owner
	 * @return	The remapped field, if one exists
	 */
	ENGINE_API static UField* FindRemappedField(UClass *FieldClass, UClass* InitialScope, FName InitialName, bool bInitialScopeMustBeOwnerOfField = false);

	/** Templated version of above, extracts FieldClass and Casts result */
	template<class TFieldType>
	static TFieldType* FindRemappedField(UClass* InitialScope, FName InitialName, bool bInitialScopeMustBeOwnerOfField = false)
	{
		return Cast<TFieldType>(FindRemappedField(TFieldType::StaticClass(), InitialScope, InitialName, bInitialScopeMustBeOwnerOfField));
	}

protected:

	/** Has the field map been initialized this run */
	static bool bFieldRedirectMapInitialized;

	/** Init the field redirect map (if not already done) from .ini file entries */
	ENGINE_API static void InitFieldRedirectMap();
	
	/** @return the 'real' generated class for blueprint classes, but only if we're already passed through CompileClassLayout */
	ENGINE_API static UClass* GetClassToUse(UClass* InClass, bool bUseUpToDateClass);
#endif

public:
	template<class TFieldType>
	static void FillSimpleMemberReference(const UField* InField, FSimpleMemberReference& OutReference)
	{
		OutReference.Reset();

		if (InField)
		{
			FMemberReference TempMemberReference;
			TempMemberReference.SetFromField<TFieldType>(InField, false);

			OutReference.MemberName = TempMemberReference.MemberName;
			OutReference.MemberParent = TempMemberReference.MemberParent;
			OutReference.MemberGuid = TempMemberReference.MemberGuid;
		}
	}

	template<class TFieldType>
	static TFieldType* ResolveSimpleMemberReference(const FSimpleMemberReference& Reference)
	{
		FMemberReference TempMemberReference;

		const FName Name = Reference.MemberGuid.IsValid() ? NAME_None : Reference.MemberName; // if the guid is valid don't check the name, it could be renamed 
		TempMemberReference.MemberName   = Name;
		TempMemberReference.MemberGuid   = Reference.MemberGuid;
		TempMemberReference.MemberParent = Reference.MemberParent;

		auto Result = TempMemberReference.ResolveMember<TFieldType>((UClass*)nullptr);
		if (!Result && (Name != Reference.MemberName))
		{
			TempMemberReference.MemberName = Reference.MemberName;
			Result = TempMemberReference.ResolveMember<TFieldType>((UClass*)nullptr);
		}

		return Result;
	}
};
