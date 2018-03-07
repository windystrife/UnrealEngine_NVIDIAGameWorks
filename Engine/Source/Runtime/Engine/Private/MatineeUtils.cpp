// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MatineeUtils.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/LinkerLoad.h"
#include "AnimationUtils.h"

/////////////////////////////////////////////////////
// FInterpPropertyGatherer

// This gathers all properties inside of a root object, contained structs, and components (recursively) that
// match the IsDesiredProperty filter and have the Interp flag.  It is a base class, designed to be overridden
struct FInterpPropertyGatherer
{
public:
	FInterpPropertyGatherer(TArray<FName>& OutNames)
		: GatheredPropertyPaths(OutNames)
	{
	}

	void Execute(UObject* RootObject)
	{
		GetInterpPropertyNames(RootObject, TEXT(""));
	}

protected:
	TArray<FName>& GatheredPropertyPaths;

protected:
	virtual bool IsDesiredProperty(UProperty* Property) const
	{
		return false;
	}

	void GetInterpPropertyNames(UObject* InObject, const FString& Prefix)
	{
		UClass* ObjectClass = InObject->GetClass();

		// First search for any properties in this object
		for (TFieldIterator<UProperty> ClassFieldIt(ObjectClass); ClassFieldIt; ++ClassFieldIt)
		{
			UProperty* ClassMemberProperty = *ClassFieldIt;
			if (ClassMemberProperty->HasAnyPropertyFlags(CPF_Interp))
			{
				// Is this property the desired type?
				if (IsDesiredProperty(ClassMemberProperty))
				{
					const FString QualifiedFullPath = FString::Printf(TEXT("%s%s"), *Prefix, *ClassMemberProperty->GetName());
					GatheredPropertyPaths.Add(*QualifiedFullPath);
				}

				// If this is a struct, look for any desired properties inside of it
				if (UStructProperty* OuterStructProperty = Cast<UStructProperty>(ClassMemberProperty))
				{
					for (TFieldIterator<UProperty> StructFieldIt(OuterStructProperty->Struct); StructFieldIt; ++StructFieldIt)
					{
						UProperty* StructMemberProperty = *StructFieldIt;
						if (StructMemberProperty->HasAnyPropertyFlags(CPF_Interp) && IsDesiredProperty(StructMemberProperty))
						{
							const FString QualifiedFullPath = FString::Printf(TEXT("%s%s.%s"), *Prefix, *OuterStructProperty->GetName(), *StructMemberProperty->GetName());
							GatheredPropertyPaths.Add(*QualifiedFullPath);
						}
					}
				}
			}
		}

		// Then iterate over each subobject of this object looking for interp properties.
		TArray<UObject*> DefaultSubObjects;
		ObjectClass->GetDefaultObjectSubobjects(DefaultSubObjects);
		for (int32 SubObjectIndex = 0; SubObjectIndex < DefaultSubObjects.Num(); ++SubObjectIndex)
		{
			UObject* Component = DefaultSubObjects[SubObjectIndex];
			const FString ComponentPrefix = Component->GetName() + TEXT(".");

			GetInterpPropertyNames(Component, ComponentPrefix); 
		}
	}
};

/////////////////////////////////////////////////////
// FBasicInterpPropertyGatherer

// This template will gather any properties that have a specified basic type (e.g., UFloatProperty or UBoolProperty)
template <typename BasicPropertyType>
struct FBasicInterpPropertyGatherer : public FInterpPropertyGatherer
{
public:
	FBasicInterpPropertyGatherer(TArray<FName>& OutNames)
		: FInterpPropertyGatherer(OutNames)
	{
	}

protected:
	virtual bool IsDesiredProperty(UProperty* Property) const override
	{
		return Cast<BasicPropertyType>(Property) != NULL;
	}
};

/////////////////////////////////////////////////////
// FStructInterpPropertyGatherer

// This struct will gather any struct properties of a specified type (e.g., LinearColor or Rotator)
struct FStructInterpPropertyGatherer : public FInterpPropertyGatherer
{
public:
	FStructInterpPropertyGatherer(FName InDesiredStructName, TArray<FName>& OutNames)
		: FInterpPropertyGatherer(OutNames)
		, DesiredStructName(InDesiredStructName)
	{
	}

protected:
	FName DesiredStructName;
protected:
	virtual bool IsDesiredProperty(UProperty* Property) const override
	{
		if (UStructProperty* StructProperty = Cast<UStructProperty>(Property))
		{
			if (StructProperty->Struct->GetFName() == DesiredStructName)
			{
				return true;
			}
		}
		
		return false;
	}
};

/////////////////////////////////////////////////////
// FMatineeTrackRedirectionManager

// Track name redirection manager
struct FMatineeTrackRedirectionManager
{
public:
	static FName GetTrackNameRedirection(UClass* TargetClass, FName TrackName)
	{
		if (!bInitialized)
		{
			BuildRedirectionTable();
		}

		if (FTrackRemapInfo* ClassRemapInfo = TrackRedirectMap.Find(TargetClass->GetFName()))
		{
			// Scan thru prefixes to see if any match
			const FString OldTrackName = TrackName.ToString();
			for (auto PrefixIt = ClassRemapInfo->PrefixMap.CreateConstIterator(); PrefixIt; ++PrefixIt)
			{
				const FString& Prefix = PrefixIt.Key();

				if (OldTrackName.StartsWith(Prefix))
				{
					// Replace the prefix
					const FString Result = PrefixIt.Value() + OldTrackName.Mid(Prefix.Len());

					return FName(*Result);
				}
			}
		}

		return TrackName;
	}

private:
	FMatineeTrackRedirectionManager()
	{
		// Buried to prevent construction
	}

	static void BuildRedirectionTable()
	{
		check(GConfig);
		check(!bInitialized);

		const FName TrackRedirectsName(TEXT("MatineeTrackRedirects"));
		FConfigSection* PackageRedirects = GConfig->GetSectionPrivate( TEXT("/Script/Engine.Engine"), false, true, GEngineIni );
		for (FConfigSection::TIterator It(*PackageRedirects); It; ++It)
		{
			if (It.Key() == TrackRedirectsName)
			{
				FName TargetClassName = NAME_Object;
				FParse::Value(*It.Value().GetValue(), TEXT("TargetClassName="), TargetClassName);

				if (UClass* TargetClass = LoadClass<UObject>(NULL, *TargetClassName.ToString(), NULL, LOAD_None, NULL))
				{
					FTrackRemapInfo& ClassRemapInfo = TrackRedirectMap.FindOrAdd(TargetClass->GetFName());

					FString OldFieldName;
					FString NewFieldName;
					FParse::Value(*It.Value().GetValue(), TEXT("OldFieldName="), OldFieldName);
					FParse::Value(*It.Value().GetValue(), TEXT("NewFieldName="), NewFieldName);

					ClassRemapInfo.PrefixMap.Add(OldFieldName, NewFieldName);
				}
				else
				{
					UE_LOG(LogMatinee, Warning, TEXT("Unknown class named '%s' in %s table"), *TargetClassName.ToString(), *TrackRedirectsName.ToString());
				}
			}			
		}

		bInitialized = true;
	}

private:
	// Map from an old prefix to the new one to replace it with (could be a full string match)
	struct FTrackRemapInfo
	{
		TMap<FString, FString> PrefixMap;
	};
private:
	// A mapping from old track name to new track name, sorted by actor class being driven by the track.  Read in from MatineeTrackRedirects in ini files.
	static TMultiMap<FName, FTrackRemapInfo> TrackRedirectMap;

	static bool bInitialized;
};

// Static members of FMatineeTrackRedirectionManager
TMultiMap<FName, FMatineeTrackRedirectionManager::FTrackRemapInfo> FMatineeTrackRedirectionManager::TrackRedirectMap;
bool FMatineeTrackRedirectionManager::bInitialized = false;

/////////////////////////////////////////////////////
// FMatineeUtils

namespace FMatineeUtils
{
	float* GetInterpFloatPropertyRef( AActor* InActor, FName InPropName )
	{
		TArray<UClass*> ClassRequirements;
		ClassRequirements.Add(UFloatProperty::StaticClass());
		
		// get the property name and the Object its in. handles property being in a component.
		void* PropContainer = NULL;
		UProperty* Property = NULL;
		UObject* PropObject = FindObjectAndPropOffset(PropContainer, Property, InActor, InPropName, &ClassRequirements );
		
		if (PropObject != NULL)
		{
			return CastChecked<UFloatProperty>(Property)->GetPropertyValuePtr_InContainer(PropContainer);
		}
		return NULL;
	}

	uint32* GetInterpBoolPropertyRef( AActor* InActor, FName InPropName, UBoolProperty*& OutProperty )
	{
		TArray<UClass*> ClassRequirements;
		ClassRequirements.Add(UBoolProperty::StaticClass());
		// get the property name and the Object its in. handles property being in a component.
		void* PropContainer = NULL;
		UProperty* Property = NULL;
		UObject* PropObject = FindObjectAndPropOffset( PropContainer, Property, InActor, InPropName, &ClassRequirements );

		if( PropObject )
		{
			OutProperty = Cast<UBoolProperty>(Property);
			// Since booleans can be handled as bitfields in the engine, 
			// we have to return a bitfield pointer instead. 
			return Property->ContainerPtrToValuePtr<uint32>(PropContainer);
		}

		return NULL;
	}


	FVector* GetInterpVectorPropertyRef( AActor* InActor, FName InPropName )
	{
		TArray<UClass*> ClassRequirements;
		ClassRequirements.Add(UStructProperty::StaticClass());
		TArray<FName> StructTypes;
		StructTypes.Add(NAME_Vector);		
		// get the property name and the Object its in. handles property being in a component.
		void* PropContainer = NULL;
		UProperty* Property = NULL;
		UObject* PropObject = FindObjectAndPropOffset(PropContainer, Property, InActor, InPropName, &ClassRequirements, &StructTypes );
		if (PropObject != NULL)
		{
			return Property->ContainerPtrToValuePtr<FVector>(PropContainer);
		}
		return NULL;
	}


	FColor* GetInterpColorPropertyRef( AActor* InActor, FName InPropName )
	{
		TArray<UClass*> ClassRequirements;
		ClassRequirements.Add(UStructProperty::StaticClass());
		TArray<FName> StructTypes;
		StructTypes.Add(NAME_Color);
		// get the property name and the Object its in. handles property being in a component.
		void* PropContainer = NULL;
		UProperty* Property = NULL;
		UObject* PropObject = FindObjectAndPropOffset(PropContainer, Property, InActor, InPropName, &ClassRequirements, &StructTypes );
		if (PropObject != NULL)
		{
			return Property->ContainerPtrToValuePtr<FColor>(PropContainer);
		}
		return NULL;
	}



	FLinearColor* GetInterpLinearColorPropertyRef( AActor* InActor, FName InPropName )
	{
		TArray<UClass*> ClassRequirements;
		ClassRequirements.Add(UStructProperty::StaticClass());
		TArray<FName> StructTypes;
		StructTypes.Add(NAME_LinearColor);
		// get the property name and the Object its in. handles property being in a component.
		void* PropContainer = NULL;
		UProperty* Property = NULL;
		UObject* PropObject = FindObjectAndPropOffset(PropContainer, Property, InActor, InPropName, &ClassRequirements, &StructTypes );
		if (PropObject != NULL)
		{
			return Property->ContainerPtrToValuePtr<FLinearColor>(PropContainer);
		}
		return NULL;
	}


	struct FMatineePropertyQuery
	{
		void* OutPropContainer;
		UProperty* OutProperty;
		UObject* OutObject;

		FMatineePropertyQuery()
			: OutPropContainer(NULL)
			, OutProperty(NULL)
			, OutObject(NULL)
		{
		}

		bool IsValid() const { return OutProperty != NULL; }

		void PerformQuery(UObject* InObject, void* BasePointer, UStruct* InStruct, FString InPropertyName)
		{
			FString CompString;
			FString PropString;

			if (InPropertyName.Split(TEXT("."), &CompString, &PropString))
			{
				if (UStructProperty* StructProp = FindField<UStructProperty>(InStruct, *CompString))
				{
					// The first part of the path was a struct, look inside it
					void* StructContainer = StructProp->ContainerPtrToValuePtr<void>(InObject);

					PerformQuery(InObject, StructContainer, StructProp->Struct, PropString);
					return;
				}
				else
				{
					// Now look for a component
					const FName TrialCompName(*CompString);

					TArray<UObject*> Components;
					InObject->CollectDefaultSubobjects(Components, false);
					for (int32 ComponentIndex = 0; ComponentIndex < Components.Num(); ComponentIndex++)
					{
						UObject* Component = Components[ComponentIndex];

						const FName RemappedCompName = FLinkerLoad::FindSubobjectRedirectName(TrialCompName, Component->GetClass());
						const FName CompName = (RemappedCompName != NAME_None) ? RemappedCompName : TrialCompName;

						if (Component->GetFName() == CompName)
						{
							PerformQuery(Component, (void*)Component, Component->GetClass(), PropString);
							return;
						}
					}
				}
			}
			else
			{
				// Look for the property in the current scope
				if (UProperty* Prop = FindField<UProperty>(InStruct, *InPropertyName))
				{
					OutPropContainer = BasePointer;
					OutProperty = Prop;
					OutObject = InObject;
				}
				else
				{
					// Handle legacy tracks that have unqualified paths to properties that are now in components by searching thru each component for the property name
					TArray<UObject*> Components;
					InObject->CollectDefaultSubobjects(Components, false);
					for (int32 ComponentIndex = 0; ComponentIndex < Components.Num(); ComponentIndex++)
					{
						UObject* Component = Components[ComponentIndex];

						PerformQuery(Component, (void*)Component, Component->GetClass(), InPropertyName);
						if (IsValid())
						{
							return;
						}
					}
				}
			}
		}
	};

	UObject* FindObjectAndPropOffset(void*& OutPropContainer, UProperty*& OutProperty, UObject* InObject, FName InPropName, const TArray<UClass*>* RequiredClasses, const TArray<FName>* StructTypes)
	{
		OutPropContainer = NULL;
		OutProperty = NULL;

		FString OldName = InPropName.ToString();

		// Check to see if any redirections are applied
		InPropName = FMatineeTrackRedirectionManager::GetTrackNameRedirection(InObject->GetClass(), InPropName);

		// Try to find the property
		FMatineePropertyQuery Query;
		Query.PerformQuery(InObject, (void*)InObject, InObject->GetClass(), InPropName.ToString());

		if (Query.IsValid() && PropertyMatchesClassRequirements(Query.OutProperty, RequiredClasses, StructTypes))
		{
			OutProperty = Query.OutProperty;
			OutPropContainer = Query.OutPropContainer;

			UE_LOG(LogMatinee, Verbose, TEXT("Found matinee property named '%s' (was '%s') in container %p (object %p | %s)"),
				*InPropName.ToString(),
				*OldName,
				OutPropContainer,
				Query.OutObject,
				*Query.OutObject->GetPathName());

			return Query.OutObject;
		}
		else
		{
			// Failed to resolve the property
			UE_LOG(LogMatinee, Warning, TEXT("Matinee track '%s' was not found as a property on '%s' (searching with property path '%s')"), *OldName, *InObject->GetPathName(), *InPropName.ToString());
			return NULL;
		}
	}

#if WITH_EDITOR
	void GetInterpFloatPropertyNames(AActor* InActor, TArray<FName>& OutNames)
	{
		FBasicInterpPropertyGatherer<UFloatProperty> GatherFunctor(OutNames);
		GatherFunctor.Execute(InActor);
	}

	void GetInterpBoolPropertyNames(AActor* InActor, TArray<FName>& OutNames)
	{
		FBasicInterpPropertyGatherer<UBoolProperty> GatherFunctor(OutNames);
		GatherFunctor.Execute(InActor);
	}

	void GetInterpVectorPropertyNames(AActor* InActor, TArray<FName>& OutNames)
	{
		FStructInterpPropertyGatherer GatherFunctor(NAME_Vector, OutNames);
		GatherFunctor.Execute(InActor);
	}

	void GetInterpColorPropertyNames(AActor* InActor, TArray<FName>& OutNames)
	{
		FStructInterpPropertyGatherer GatherFunctor(NAME_Color, OutNames);
		GatherFunctor.Execute(InActor);
	}

	void GetInterpLinearColorPropertyNames(AActor* InActor, TArray<FName>& OutNames)
	{
		FStructInterpPropertyGatherer GatherFunctor(NAME_LinearColor, OutNames);
		GatherFunctor.Execute(InActor);
	}
#endif


	bool PropertyMatchesClassRequirements( UProperty* Prop, const TArray<UClass*>* RequiredClasses, const TArray<FName>* StructTypes )
	{
		bool bMatches = false;
		if( RequiredClasses == NULL )
		{
			bMatches = true;
		}
		else
		{
			const TArray<UClass*>& Classes = *RequiredClasses;
			for (int32 iClass = 0; iClass < Classes.Num() ; iClass++)
			{
				UClass* EachClass = Classes[ iClass ];
				if(Prop->IsA(UStructProperty::StaticClass()))
				{
					if( StructTypes == NULL )
					{
						bMatches = true;
						break;
					}
					else
					{
						const TArray<FName>& Structs = *StructTypes;
						UStructProperty* StructProp = CastChecked<UStructProperty>( Prop );
						FName StructType = StructProp->Struct->GetFName();
						for (int32 iStructType = 0; iStructType < Structs.Num() ; iStructType++)
						{
							if(StructType == Structs[ iStructType ] )
							{
								bMatches = true;
								break;
							}
						}
					}
				}
				else if(Prop->IsA( EachClass ))
				{
					bMatches = true;
					break;
				}
			}
		}
		return bMatches;
	}
}


