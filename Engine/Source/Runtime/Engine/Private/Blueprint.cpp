// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Engine/Blueprint.h"
#include "Misc/CoreMisc.h"
#include "Misc/ConfigCacheIni.h"
#include "UObject/BlueprintsObjectVersion.h"
#include "UObject/UObjectHash.h"
#include "Serialization/PropertyLocalizationDataGathering.h"
#include "UObject/UnrealType.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Misc/SecureHash.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "EdGraph/EdGraph.h"
#include "Engine/Breakpoint.h"
#include "Components/TimelineComponent.h"

#if WITH_EDITOR
#include "Blueprint/BlueprintSupport.h"
#include "BlueprintCompilationManager.h"
#include "Blueprint/BlueprintSupport.h"
#include "Editor/UnrealEd/Classes/Settings/ProjectPackagingSettings.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SCS_Node.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/StructureEditorUtils.h"
#include "FindInBlueprintManager.h"
#include "CookerSettings.h"
#include "Editor.h"
#include "Logging/MessageLog.h"
#include "Settings/EditorLoadingSavingSettings.h"
#include "Engine/TimelineTemplate.h"
#include "Curves/CurveBase.h"
#include "Interfaces/ITargetPlatform.h"
#include "MetaData.h"
#endif
#include "Engine/InheritableComponentHandler.h"

DEFINE_LOG_CATEGORY(LogBlueprint);

//////////////////////////////////////////////////////////////////////////
// Static Helpers

/**
 * Updates the blueprint's OwnedComponents, such that they reflect changes made 
 * natively since the blueprint was last saved (a change in AttachParents, etc.)
 * 
 * @param  Blueprint	The blueprint whose components you wish to vet.
 */
static void ConformNativeComponents(UBlueprint* Blueprint)
{
#if WITH_EDITOR
	if (UClass* const BlueprintClass = Blueprint->GeneratedClass)
	{
		if (AActor* BlueprintCDO = Cast<AActor>(BlueprintClass->ClassDefaultObject))
		{
			TInlineComponentArray<UActorComponent*> OldNativeComponents;
			// collect the native components that this blueprint was serialized out 
			// with (the native components it had last time it was saved)
			BlueprintCDO->GetComponents(OldNativeComponents);

			UClass* const NativeSuperClass = FBlueprintEditorUtils::FindFirstNativeClass(BlueprintClass);
			AActor* NativeCDO = CastChecked<AActor>(NativeSuperClass->ClassDefaultObject);
			// collect the more up to date native components (directly from the 
			// native super-class)
			TInlineComponentArray<UActorComponent*> NewNativeComponents;
			NativeCDO->GetComponents(NewNativeComponents);

			// utility lambda for finding named components in a supplied list
			auto FindNamedComponentLambda = [](FName const ComponentName, TInlineComponentArray<UActorComponent*> const& ComponentList)->UActorComponent*
			{
				UActorComponent* FoundComponent = nullptr;
				for (UActorComponent* Component : ComponentList)
				{
					if (Component->GetFName() == ComponentName)
					{
						FoundComponent = Component;
						break;
					}
				}
				return FoundComponent;
			};

			// utility lambda for finding matching components in the NewNativeComponents list
			auto FindNativeComponentLambda = [&NewNativeComponents, &FindNamedComponentLambda](UActorComponent* BlueprintComponent)->UActorComponent*
			{
				UActorComponent* MatchingComponent = nullptr;
				if (BlueprintComponent != nullptr)
				{
					FName const ComponentName = BlueprintComponent->GetFName();
					MatchingComponent = FindNamedComponentLambda(ComponentName, NewNativeComponents);
				}
				return MatchingComponent;
			};

			// loop through all components that this blueprint thinks come from its
			// native super-class (last time it was saved)
			for (UActorComponent* Component : OldNativeComponents)
			{
				// if we found this component also listed for the native class
				if (UActorComponent* NativeComponent = FindNativeComponentLambda(Component))
				{
					USceneComponent* BlueprintSceneComponent = Cast<USceneComponent>(Component);
					if (BlueprintSceneComponent == nullptr)
					{
						// if this isn't a scene-component, then we don't care
						// (we're looking to fixup scene-component parents)
						continue;
					}
					UActorComponent* OldNativeParent = FindNativeComponentLambda(BlueprintSceneComponent->GetAttachParent());

					USceneComponent* NativeSceneComponent = CastChecked<USceneComponent>(NativeComponent);
					// if this native component has since been reparented, we need
					// to make sure that this blueprint reflects that change
					if (OldNativeParent != NativeSceneComponent->GetAttachParent())
					{
						USceneComponent* NewParent = nullptr;
						if (NativeSceneComponent->GetAttachParent() != nullptr)
						{
							NewParent = CastChecked<USceneComponent>(FindNamedComponentLambda(NativeSceneComponent->GetAttachParent()->GetFName(), OldNativeComponents));
						}
						BlueprintSceneComponent->SetupAttachment(NewParent, BlueprintSceneComponent->GetAttachSocketName());
					}
				}
				else // the component has been removed from the native class
				{
					// @TODO: I think we already handle removed native components elsewhere, so maybe we should error here?
// 				BlueprintCDO->RemoveOwnedComponent(Component);
// 
// 				USimpleConstructionScript* BlueprintSCS = Blueprint->SimpleConstructionScript;
// 				USCS_Node* ComponentNode = BlueprintSCS->CreateNode(Component, Component->GetFName());
// 
// 				BlueprintSCS->AddNode(ComponentNode);
				}
			}
		}
	}
#endif // #if WITH_EDITOR
}


//////////////////////////////////////////////////////////////////////////
// FBPVariableDescription

int32 FBPVariableDescription::FindMetaDataEntryIndexForKey(const FName& Key) const
{
	for(int32 i=0; i<MetaDataArray.Num(); i++)
	{
		if(MetaDataArray[i].DataKey == Key)
		{
			return i;
		}
	}
	return INDEX_NONE;
}

bool FBPVariableDescription::HasMetaData(const FName& Key) const
{
	return FindMetaDataEntryIndexForKey(Key) != INDEX_NONE;
}

/** Gets a metadata value on the variable; asserts if the value isn't present.  Check for validiy using FindMetaDataEntryIndexForKey. */
FString FBPVariableDescription::GetMetaData(const FName& Key) const
{
	int32 EntryIndex = FindMetaDataEntryIndexForKey(Key);
	check(EntryIndex != INDEX_NONE);
	return MetaDataArray[EntryIndex].DataValue;
}

void FBPVariableDescription::SetMetaData(const FName& Key, const FString& Value)
{
	int32 EntryIndex = FindMetaDataEntryIndexForKey(Key);
	if(EntryIndex != INDEX_NONE)
	{
		MetaDataArray[EntryIndex].DataValue = Value;
	}
	else
	{
		MetaDataArray.Add( FBPVariableMetaDataEntry(Key, Value) );
	}
}

void FBPVariableDescription::RemoveMetaData(const FName& Key)
{
	int32 EntryIndex = FindMetaDataEntryIndexForKey(Key);
	if(EntryIndex != INDEX_NONE)
	{
		MetaDataArray.RemoveAt(EntryIndex);
	}
}

//////////////////////////////////////////////////////////////////////////
// UBlueprintCore

#if WITH_EDITORONLY_DATA
namespace
{
	void GatherBlueprintForLocalization(const UObject* const Object, FPropertyLocalizationDataGatherer& PropertyLocalizationDataGatherer, const EPropertyLocalizationGathererTextFlags GatherTextFlags)
	{
		const UBlueprintCore* const BlueprintCore = CastChecked<UBlueprintCore>(Object);

		// Blueprint assets never exist at runtime, so treat all of their properties as editor-only, but allow their script (which is available at runtime) to be gathered by a game
		EPropertyLocalizationGathererTextFlags BlueprintGatherFlags = GatherTextFlags | EPropertyLocalizationGathererTextFlags::ForceEditorOnlyProperties;

#if WITH_EDITOR
		if (const UBlueprint* const Blueprint = Cast<UBlueprint>(Object))
		{
			// Force non-data-only blueprints to set the HasScript flag, as they may not currently have bytecode due to a compilation error
			bool bForceHasScript = !FBlueprintEditorUtils::IsDataOnlyBlueprint(Blueprint);
			if (!bForceHasScript)
			{
				// Also do this for blueprints that derive from something containing text properties, as these may propagate default values from their parent class on load
				if (UClass* BlueprintParentClass = Blueprint->ParentClass.Get())
				{
					TArray<UStruct*> TypesToCheck;
					TypesToCheck.Add(BlueprintParentClass);

					TSet<UStruct*> TypesChecked;
					while (!bForceHasScript && TypesToCheck.Num() > 0)
					{
						UStruct* TypeToCheck = TypesToCheck.Pop(/*bAllowShrinking*/false);
						TypesChecked.Add(TypeToCheck);

						for (TFieldIterator<const UProperty> PropIt(TypeToCheck, EFieldIteratorFlags::IncludeSuper, EFieldIteratorFlags::ExcludeDeprecated, EFieldIteratorFlags::IncludeInterfaces); !bForceHasScript && PropIt; ++PropIt)
						{
							auto ProcessInnerProperty = [&bForceHasScript, &TypesToCheck, &TypesChecked](const UProperty* InProp) -> bool
							{
								if (const UTextProperty* TextProp = Cast<const UTextProperty>(InProp))
								{
									bForceHasScript = true;
									return true;
								}
								if (const UStructProperty* StructProp = Cast<const UStructProperty>(InProp))
								{
									if (!TypesChecked.Contains(StructProp->Struct))
									{
										TypesToCheck.Add(StructProp->Struct);
									}
									return true;
								}
								return false;
							};

							if (!ProcessInnerProperty(*PropIt))
							{
								if (const UArrayProperty* ArrayProp = Cast<const UArrayProperty>(*PropIt))
								{
									ProcessInnerProperty(ArrayProp->Inner);
								}
								if (const UMapProperty* MapProp = Cast<const UMapProperty>(*PropIt))
								{
									ProcessInnerProperty(MapProp->KeyProp);
									ProcessInnerProperty(MapProp->ValueProp);
								}
								if (const USetProperty* SetProp = Cast<const USetProperty>(*PropIt))
								{
									ProcessInnerProperty(SetProp->ElementProp);
								}
							}
						}
					}
				}
			}

			if (bForceHasScript)
			{
				BlueprintGatherFlags |= EPropertyLocalizationGathererTextFlags::ForceHasScript;
			}
		}
#endif

		PropertyLocalizationDataGatherer.GatherLocalizationDataFromObject(BlueprintCore, BlueprintGatherFlags);
	}
}
#endif

UBlueprintCore::UBlueprintCore(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
#if WITH_EDITORONLY_DATA
	{ static const FAutoRegisterLocalizationDataGatheringCallback AutomaticRegistrationOfLocalizationGatherer(UBlueprintCore::StaticClass(), &GatherBlueprintForLocalization); }
#endif

	bLegacyGeneratedClassIsAuthoritative = false;
	bLegacyNeedToPurgeSkelRefs = true;
}

void UBlueprintCore::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_EDITOR
	Ar.UsingCustomVersion(FBlueprintsObjectVersion::GUID);
#endif

	Ar << bLegacyGeneratedClassIsAuthoritative;	

	if ((Ar.UE4Ver() < VER_UE4_BLUEPRINT_SKEL_CLASS_TRANSIENT_AGAIN)
		&& (Ar.UE4Ver() != VER_UE4_BLUEPRINT_SKEL_TEMPORARY_TRANSIENT))
	{
		Ar << SkeletonGeneratedClass;
		if( SkeletonGeneratedClass )
		{
			// If we serialized in a skeleton class, make sure it and all its children are updated to be transient
			SkeletonGeneratedClass->SetFlags(RF_Transient);
			TArray<UObject*> SubObjs;
			GetObjectsWithOuter(SkeletonGeneratedClass, SubObjs, true);
			for(auto SubObjIt = SubObjs.CreateIterator(); SubObjIt; ++SubObjIt)
			{
				(*SubObjIt)->SetFlags(RF_Transient);
			}
		}

		// We only want to serialize in the GeneratedClass if the SkeletonClass didn't trigger a recompile
		bool bSerializeGeneratedClass = true;
		if (UBlueprint* BP = Cast<UBlueprint>(this))
		{
			bSerializeGeneratedClass = !Ar.IsLoading() || !BP->bHasBeenRegenerated;
		}

		if (bSerializeGeneratedClass)
		{
			Ar << GeneratedClass;
		}
		else if (Ar.IsLoading())
		{
			UClass* DummyClass = NULL;
			Ar << DummyClass;
		}
	}

	if( Ar.ArIsLoading && !BlueprintGuid.IsValid() )
	{
		GenerateDeterministicGuid();
	}
}

#if WITH_EDITORONLY_DATA
void UBlueprintCore::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	Super::GetAssetRegistryTags(OutTags);

	FString GeneratedClassVal;
	if ( GeneratedClass != NULL )
	{
		GeneratedClassVal = FString::Printf(TEXT("%s'%s'"), *GeneratedClass->GetClass()->GetName(), *GeneratedClass->GetPathName());
	}
	else
	{
		GeneratedClassVal = TEXT("None");
	}

	OutTags.Add( FAssetRegistryTag("GeneratedClass", GeneratedClassVal, FAssetRegistryTag::TT_Hidden) );
}
#endif

void UBlueprintCore::GenerateDeterministicGuid()
{
	FString HashString = GetPathName();
	HashString.Shrink();
	ensure( HashString.Len() );

	uint32 HashBuffer[ 5 ];
	uint32 BufferLength = HashString.Len() * sizeof( HashString[0] );
	FSHA1::HashBuffer(*HashString, BufferLength, reinterpret_cast< uint8* >( HashBuffer ));
	BlueprintGuid.A = HashBuffer[ 1 ];
	BlueprintGuid.B = HashBuffer[ 2 ];
	BlueprintGuid.C = HashBuffer[ 3 ];
	BlueprintGuid.D = HashBuffer[ 4 ];
}

UBlueprint::UBlueprint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
#if WITH_EDITOR
	, bRunConstructionScriptOnDrag(true)
	, bRunConstructionScriptInSequencer(false)
	, bGenerateConstClass(false)
#endif
#if WITH_EDITORONLY_DATA
	, bDuplicatingReadOnly(false)
	, bCachedDependenciesUpToDate(false)
	, bHasAnyNonReducibleFunction(EIsBPNonReducible::Unkown)
#endif
{
}

#if WITH_EDITORONLY_DATA
void UBlueprint::PreSave(const class ITargetPlatform* TargetPlatform)
{
	Super::PreSave(TargetPlatform);

	// Clear all upgrade notes, the user has saved and should not see them anymore
	UpgradeNotesLog.Reset();

	if (!TargetPlatform || TargetPlatform->HasEditorOnlyData())
	{
		// Cache the BP for use
		FFindInBlueprintSearchManager::Get().AddOrUpdateBlueprintSearchMetadata(this);
	}
}
#endif // WITH_EDITORONLY_DATA

void UBlueprint::GetPreloadDependencies(TArray<UObject*>& OutDeps)
{
	Super::GetPreloadDependencies(OutDeps);
	for (UClass* ClassIt = ParentClass; (ClassIt != NULL) && !(ClassIt->HasAnyClassFlags(CLASS_Native)); ClassIt = ClassIt->GetSuperClass())
	{
		if (ClassIt->ClassGeneratedBy)
		{
			OutDeps.Add(ClassIt->ClassGeneratedBy);
		}
	}
}

void UBlueprint::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_EDITORONLY_DATA
	if(Ar.IsLoading() && Ar.UE4Ver() < VER_UE4_BLUEPRINT_VARS_NOT_READ_ONLY)
	{
		// Allow all blueprint defined vars to be read/write.  undoes previous convention of making exposed variables read-only
		for (int32 i = 0; i < NewVariables.Num(); ++i)
		{
			FBPVariableDescription& Variable = NewVariables[i];
			Variable.PropertyFlags &= ~CPF_BlueprintReadOnly;
		}
	}

	if (Ar.UE4Ver() < VER_UE4_K2NODE_REFERENCEGUIDS)
	{
		for (int32 Index = 0; Index < NewVariables.Num(); ++Index)
		{
			NewVariables[Index].VarGuid = FGuid::NewGuid();
		}
	}

	// Preload our parent blueprints
	if (Ar.IsLoading())
	{
		for (UClass* ClassIt = ParentClass; (ClassIt != NULL) && !(ClassIt->HasAnyClassFlags(CLASS_Native)); ClassIt = ClassIt->GetSuperClass())
		{
			if (!ensure(ClassIt->ClassGeneratedBy != nullptr))
			{
				UE_LOG(LogBlueprint, Error, TEXT("Cannot preload parent blueprint from null ClassGeneratedBy field (for '%s')"), *ClassIt->GetName());
			}
			else if (ClassIt->ClassGeneratedBy->HasAnyFlags(RF_NeedLoad))
			{
				ClassIt->ClassGeneratedBy->GetLinker()->Preload(ClassIt->ClassGeneratedBy);
			}
		}
	}

	for (int32 i = 0; i < NewVariables.Num(); ++i)
	{
		FBPVariableDescription& Variable = NewVariables[i];

		// Actor variables can't have default values (because Blueprint templates are library elements that can 
		// bridge multiple levels and different levels might not have the actor that the default is referencing).
		if (Ar.UE4Ver() < VER_UE4_FIX_BLUEPRINT_VARIABLE_FLAGS)
		{
			const FEdGraphPinType& VarType = Variable.VarType;

			bool bDisableEditOnTemplate = false;
			if (VarType.PinSubCategoryObject.IsValid()) // ignore variables that don't have associated objects
			{
				const UClass* ClassObject = Cast<UClass>(VarType.PinSubCategoryObject.Get());
				// if the object type is an actor...
				if ((ClassObject != NULL) && ClassObject->IsChildOf(AActor::StaticClass()))
				{
					// hide the default value field
					bDisableEditOnTemplate = true;
				}
			}

			if (bDisableEditOnTemplate)
			{
				Variable.PropertyFlags |= CPF_DisableEditOnTemplate;
			}
			else
			{
				Variable.PropertyFlags &= ~CPF_DisableEditOnTemplate;
			}
		}

		if (Ar.IsLoading())
		{
			// Validate metadata keys/values on load only
			FBlueprintEditorUtils::FixupVariableDescription(this, Variable);
		}
	}

	if (Ar.IsPersistent())
	{
		bool bSettingsChanged = false;
		const FString PackageName = GetOutermost()->GetName();
		UProjectPackagingSettings* PackagingSettings = GetMutableDefault<UProjectPackagingSettings>();

		if (Ar.IsLoading())
		{
			if (bNativize_DEPRECATED)
			{
				// Migrate to the new transient flag.
				bNativize_DEPRECATED = false;

				NativizationFlag = EBlueprintNativizationFlag::ExplicitlyEnabled;
				// Add this Blueprint asset to the exclusive list in the Project Settings (in case it doesn't exist).
				bSettingsChanged |= PackagingSettings->AddBlueprintAssetToNativizationList(this);
			}
			else
			{
				// Cache whether or not this Blueprint asset was selected for exclusive nativization in the Project Settings.
				for (int AssetIndex = 0; AssetIndex < PackagingSettings->NativizeBlueprintAssets.Num(); ++AssetIndex)
				{
					if (PackagingSettings->NativizeBlueprintAssets[AssetIndex].FilePath.Equals(PackageName, ESearchCase::IgnoreCase))
					{
						NativizationFlag = EBlueprintNativizationFlag::ExplicitlyEnabled;
						break;
					}
				}
			}
		}
		else if (Ar.IsSaving())
		{
			bSettingsChanged |= FBlueprintEditorUtils::PropagateNativizationSetting(this);
		}

		if (bSettingsChanged)
		{
			// Update cached config settings and save.
			PackagingSettings->SaveConfig();
			PackagingSettings->UpdateDefaultConfigFile();
		}
	}
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITOR

bool UBlueprint::RenameGeneratedClasses( const TCHAR* InName, UObject* NewOuter, ERenameFlags Flags )
{
	const bool bRenameGeneratedClasses = !(Flags & REN_SkipGeneratedClasses );

	if(bRenameGeneratedClasses)
	{
		FName SkelClassName, GenClassName;
		GetBlueprintClassNames(GenClassName, SkelClassName, FName(InName));

		UPackage* NewTopLevelObjectOuter = NewOuter ? NewOuter->GetOutermost() : NULL;
		if (GeneratedClass != NULL)
		{
			bool bMovedOK = GeneratedClass->Rename(*GenClassName.ToString(), NewTopLevelObjectOuter, Flags);
			if (!bMovedOK)
			{
				return false;
			}
		}

		// Also move skeleton class, if different from generated class, to new package (again, to create redirector)
		if (SkeletonGeneratedClass != NULL && SkeletonGeneratedClass != GeneratedClass)
		{
			bool bMovedOK = SkeletonGeneratedClass->Rename(*SkelClassName.ToString(), NewTopLevelObjectOuter, Flags);
			if (!bMovedOK)
			{
				return false;
			}
		}
	}
	return true;
}

bool UBlueprint::Rename( const TCHAR* InName, UObject* NewOuter, ERenameFlags Flags )
{
	const FName OldName = GetFName();

	// Move generated class/CDO to the new package, to create redirectors
	if ( !RenameGeneratedClasses(InName, NewOuter, Flags) )
	{
		return false;
	}

	bool bSuccess = Super::Rename( InName, NewOuter, Flags );

	// Finally, do a compile, but only if the new name differs from before
	if(bSuccess && !(Flags & REN_Test) && !(Flags & REN_DoNotDirty) && InName && InName != OldName)
	{
		// Gather all blueprints that currently depend on this one.
		TArray<UBlueprint*> Dependents;
		FBlueprintEditorUtils::GetDependentBlueprints(this, Dependents);

		FKismetEditorUtilities::CompileBlueprint(this);

		// Recompile dependent blueprints after compiling this one. Otherwise, we can end up with a GLEO during the internal package save, which will include referencers as well.
		for (UBlueprint* DependentBlueprint : Dependents)
		{
			FKismetEditorUtilities::CompileBlueprint(DependentBlueprint);
		}
	}

	return bSuccess;
}

void UBlueprint::PostDuplicate(bool bDuplicateForPIE)
{
	Super::PostDuplicate(bDuplicateForPIE);
	if( !bDuplicatingReadOnly )
	{
		FBlueprintEditorUtils::PostDuplicateBlueprint(this, bDuplicateForPIE);
	}
}

extern COREUOBJECT_API bool GBlueprintUseCompilationManager;

UClass* UBlueprint::RegenerateClass(UClass* ClassToRegenerate, UObject* PreviousCDO, TArray<UObject*>& ObjLoaded)
{
	if(GBlueprintUseCompilationManager)
	{
		// ensure that we have UProperties for any properties declared in the blueprint:
		if(!GeneratedClass || !HasAnyFlags(RF_BeingRegenerated) || bIsRegeneratingOnLoad || bHasBeenRegenerated)
		{
			return GeneratedClass;
		}
		
		// tag ourself as bIsRegeneratingOnLoad so that any reentrance via ForceLoad calls doesn't recurse:
		bIsRegeneratingOnLoad = true;
		
		UPackage* Package = GetOutermost();
		bool bIsPackageDirty = Package ? Package->IsDirty() : false;

		UClass* GeneratedClassResolved = GeneratedClass;

		UBlueprint::ForceLoadMetaData(this);
		if (ensure(GeneratedClassResolved->ClassDefaultObject ))
		{
			UBlueprint::ForceLoadMembers(GeneratedClassResolved);
			UBlueprint::ForceLoadMembers(GeneratedClassResolved->ClassDefaultObject);
		}
		UBlueprint::ForceLoadMembers(this);
		
		FBlueprintEditorUtils::PreloadConstructionScript( this );

		FBlueprintEditorUtils::LinkExternalDependencies( this );

		FBlueprintEditorUtils::RefreshVariables(this);
		
		// Preload Overridden Components
		if (InheritableComponentHandler)
		{
			InheritableComponentHandler->PreloadAll();
		}

		FBlueprintCompilationManager::NotifyBlueprintLoaded( this ); 
		
		FBlueprintEditorUtils::PreloadBlueprintSpecificData( this );

		// clear this now that we're not in a re-entrrant context - bHasBeenRegenerated will guard against 'real' 
		// double regeneration calls:
		bIsRegeneratingOnLoad = false;

		if( Package )
		{
			Package->SetDirtyFlag(bIsPackageDirty);
		}

		return GeneratedClassResolved;
	}
	else
	{
		return FBlueprintEditorUtils::RegenerateBlueprintClass(this, ClassToRegenerate, PreviousCDO, ObjLoaded);
	}
}

void UBlueprint::RemoveGeneratedClasses()
{
	FBlueprintEditorUtils::RemoveGeneratedClasses(this);
}

void UBlueprint::PostLoad()
{
	Super::PostLoad();

	// Can't use TGuardValue here as bIsRegeneratingOnLoad is a bitfield
	struct FScopedRegeneratingOnLoad
	{
		UBlueprint& Blueprint;
		bool bPreviousValue;
		FScopedRegeneratingOnLoad(UBlueprint& InBlueprint)
			: Blueprint(InBlueprint)
			, bPreviousValue(InBlueprint.bIsRegeneratingOnLoad)
		{
			// if the blueprint's package is still in the midst of loading, then
			// bIsRegeneratingOnLoad needs to be set to prevent UObject renames
			// from resetting loaders
			Blueprint.bIsRegeneratingOnLoad = true;
			if (UPackage* Package = Blueprint.GetOutermost())
			{
				// checking (Package->LinkerLoad != nullptr) ensures this 
				// doesn't get set when duplicating blueprints (which also calls 
				// PostLoad), and checking RF_WasLoaded makes sure we only 
				// forcefully set bIsRegeneratingOnLoad for blueprints that need 
				// it (ones still actively loading)
				Blueprint.bIsRegeneratingOnLoad = bPreviousValue || ((Package->LinkerLoad != nullptr) && !Package->HasAnyFlags(RF_WasLoaded));
			}
		}
		~FScopedRegeneratingOnLoad()
		{
			Blueprint.bIsRegeneratingOnLoad = bPreviousValue;
		}
	} GuardIsRegeneratingOnLoad(*this);

	// Mark the blueprint as in error if there has been a major version bump
	if (BlueprintSystemVersion < UBlueprint::GetCurrentBlueprintSystemVersion())
	{
		Status = BS_Error;
	}

	// Purge any NULL graphs
	FBlueprintEditorUtils::PurgeNullGraphs(this);

	// Remove stale breakpoints
	for (int32 i = 0; i < Breakpoints.Num(); ++i)
	{
		UBreakpoint* Breakpoint = Breakpoints[i];
		if (!Breakpoint || !Breakpoint->GetLocation())
		{
			Breakpoints.RemoveAt(i);
			--i;
		}
	}

	// Make sure we have an SCS and ensure it's transactional
	if( FBlueprintEditorUtils::SupportsConstructionScript(this) )
	{
		if(SimpleConstructionScript == NULL)
		{
			check(NULL != GeneratedClass);
			SimpleConstructionScript = NewObject<USimpleConstructionScript>(GeneratedClass);
			SimpleConstructionScript->SetFlags(RF_Transactional);

			UBlueprintGeneratedClass* BPGClass = Cast<UBlueprintGeneratedClass>(*GeneratedClass);
			if(BPGClass)
			{
				BPGClass->SimpleConstructionScript = SimpleConstructionScript;
			}
		}
		else
		{
			if (!SimpleConstructionScript->HasAnyFlags(RF_Transactional))
			{
				SimpleConstructionScript->SetFlags(RF_Transactional);
			}
		}
	}

	// Make sure the CDO's scene root component is valid
	FBlueprintEditorUtils::UpdateRootComponentReference(this);

	// Make sure all the components are used by this blueprint
	FBlueprintEditorUtils::UpdateComponentTemplates(this);

	// Make sure that all of the parent function calls are valid
	FBlueprintEditorUtils::ConformCallsToParentFunctions(this);

	// Make sure that all of the events this BP implements are valid
	FBlueprintEditorUtils::ConformImplementedEvents(this);

	// Make sure that all of the interfaces this BP implements have all required graphs
	FBlueprintEditorUtils::ConformImplementedInterfaces(this);

	// Make sure that there are no function graphs that are marked as bAllowDeletion=false 
	// (possible if a blueprint was reparented prior to 4.11):
	if (GetLinkerCustomVersion(FBlueprintsObjectVersion::GUID) < FBlueprintsObjectVersion::AllowDeletionConformed)
	{
		FBlueprintEditorUtils::ConformAllowDeletionFlag(this);
	}

	// Update old Anim Blueprints
	FBlueprintEditorUtils::UpdateOutOfDateAnimBlueprints(this);

#if WITH_EDITORONLY_DATA
	// Ensure all the pin watches we have point to something useful
	FBlueprintEditorUtils::UpdateStalePinWatches(this);
#endif // WITH_EDITORONLY_DATA

	FStructureEditorUtils::RemoveInvalidStructureMemberVariableFromBlueprint(this);

#if WITH_EDITOR
	// Do not want to run this code without the editor present nor when running commandlets.
	if(GEditor && GIsEditor && !IsRunningCommandlet())
	{
		// Gathers Find-in-Blueprint data, makes sure that it is fresh and ready, especially if the asset did not have any available.
		FFindInBlueprintSearchManager::Get().AddOrUpdateBlueprintSearchMetadata(this);
	}
#endif
}

void UBlueprint::DebuggingWorldRegistrationHelper(UObject* ObjectProvidingWorld, UObject* ValueToRegister)
{
	if (ObjectProvidingWorld != NULL)
	{
		// Fix up the registration with the world
		UWorld* ObjWorld = NULL;
		UObject* ObjOuter = ObjectProvidingWorld->GetOuter();
		while (ObjOuter != NULL)
		{
			ObjWorld = Cast<UWorld>(ObjOuter);
			if (ObjWorld != NULL)
			{
				break;
			}

			ObjOuter = ObjOuter->GetOuter();
		}

		if (ObjWorld != NULL)
		{
			ObjWorld->NotifyOfBlueprintDebuggingAssociation(this, ValueToRegister);
			OnSetObjectBeingDebuggedDelegate.Broadcast(ValueToRegister);
		}
	}
}

UClass* UBlueprint::GetBlueprintClass() const
{
	return UBlueprintGeneratedClass::StaticClass();
}

void UBlueprint::SetObjectBeingDebugged(UObject* NewObject)
{
	// Unregister the old object
	if (UObject* OldObject = CurrentObjectBeingDebugged.Get())
	{
		if (OldObject == NewObject)
		{
			// Nothing changed
			return;
		}

		DebuggingWorldRegistrationHelper(OldObject, NULL);
	}

	// Note that we allow macro Blueprints to bypass this check
	if ((NewObject != NULL) && !GCompilingBlueprint && BlueprintType != BPTYPE_MacroLibrary)
	{
		// You can only debug instances of this!
		if (!ensureMsgf(
				NewObject->IsA(this->GeneratedClass), 
				TEXT("Type mismatch: Expected %s, Found %s"), 
				this->GeneratedClass ? *(this->GeneratedClass->GetName()) : TEXT("NULL"), 
				NewObject->GetClass() ? *(NewObject->GetClass()->GetName()) : TEXT("NULL")))
		{
			NewObject = NULL;
		}
	}

	// Update the current object being debugged
	CurrentObjectBeingDebugged = NewObject;

	// Register the new object
	if (NewObject != NULL)
	{
		DebuggingWorldRegistrationHelper(NewObject, NewObject);
	}
}

void UBlueprint::SetWorldBeingDebugged(UWorld *NewWorld)
{
	CurrentWorldBeingDebugged = NewWorld;
}

void UBlueprint::GetReparentingRules(TSet< const UClass* >& AllowedChildrenOfClasses, TSet< const UClass* >& DisallowedChildrenOfClasses) const
{

}

UObject* UBlueprint::GetObjectBeingDebugged()
{
	UObject* DebugObj = CurrentObjectBeingDebugged.Get();
	if(DebugObj)
	{
		//Check whether the object has been deleted.
		if(DebugObj->IsPendingKill())
		{
			SetObjectBeingDebugged(NULL);
			DebugObj = NULL;
		}
	}
	return DebugObj;
}

UWorld* UBlueprint::GetWorldBeingDebugged()
{
	UWorld* DebugWorld = CurrentWorldBeingDebugged.Get();
	if (DebugWorld)
	{
		if(DebugWorld->IsPendingKill())
		{
			SetWorldBeingDebugged(NULL);
			DebugWorld = NULL;
		}
	}

	return DebugWorld;
}

void UBlueprint::GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const
{
	// We use Generated instead of Skeleton because the CDO data is more accurate on Generated
	if (GeneratedClass)
	{
		if (UObject* CDO = GeneratedClass->GetDefaultObject())
		{
			CDO->GetAssetRegistryTags(OutTags);
		}
	}

	Super::GetAssetRegistryTags(OutTags);

	FString ParentClassPackageName;
	FString NativeParentClassName;
	if ( ParentClass )
	{
		ParentClassPackageName = ParentClass->GetOutermost()->GetName();

		// Walk up until we find a native class (ie 'while they are BP classes')
		UClass* NativeParentClass = ParentClass;
		while (Cast<UBlueprintGeneratedClass>(NativeParentClass) != nullptr) // can't use IsA on UClass
		{
			NativeParentClass = NativeParentClass->GetSuperClass();
		}
		NativeParentClassName = FString::Printf(TEXT("%s'%s'"), *UClass::StaticClass()->GetName(), *NativeParentClass->GetPathName());
	}
	else
	{
		ParentClassPackageName = TEXT("None");
		NativeParentClassName = TEXT("None");
	}

	//NumReplicatedProperties
	int32 NumReplicatedProperties = 0;
	UBlueprintGeneratedClass* BlueprintClass = Cast<UBlueprintGeneratedClass>(SkeletonGeneratedClass);
	if (BlueprintClass)
	{
		NumReplicatedProperties = BlueprintClass->NumReplicatedProperties;
	}

	OutTags.Add(FAssetRegistryTag("NumReplicatedProperties", FString::FromInt(NumReplicatedProperties), FAssetRegistryTag::TT_Numerical));
	OutTags.Add(FAssetRegistryTag("ParentClassPackage", ParentClassPackageName, FAssetRegistryTag::TT_Hidden));
	OutTags.Add(FAssetRegistryTag("NativeParentClass", NativeParentClassName, FAssetRegistryTag::TT_Alphabetical));
	OutTags.Add(FAssetRegistryTag(GET_MEMBER_NAME_CHECKED(UBlueprint, BlueprintDescription), BlueprintDescription, FAssetRegistryTag::TT_Hidden));

	uint32 ClassFlagsTagged = 0;
	if (BlueprintClass)
	{
		ClassFlagsTagged = BlueprintClass->GetClassFlags();
	}
	else
	{
		ClassFlagsTagged = GetClass()->GetClassFlags();
	}
	OutTags.Add( FAssetRegistryTag("ClassFlags", FString::FromInt(ClassFlagsTagged), FAssetRegistryTag::TT_Hidden) );

	OutTags.Add( FAssetRegistryTag( "IsDataOnly",
			FBlueprintEditorUtils::IsDataOnlyBlueprint(this) ? TEXT("True") : TEXT("False"),
			FAssetRegistryTag::TT_Alphabetical ) );

	// Only add the FiB tags in the editor, this now gets run for standalone uncooked games
	if ( ParentClass && GIsEditor)
	{
		OutTags.Add( FAssetRegistryTag("FiBData", FFindInBlueprintSearchManager::Get().QuerySingleBlueprint((UBlueprint*)this, false), FAssetRegistryTag::TT_Hidden) );
	}

	// Only show for strict blueprints (not animation or widget blueprints)
	// Note: Can't be an Actor specific check on the gen class, as CB only queries the CDO for the majority type to determine which columns are shown in the view
	if (ExactCast<UBlueprint>(this) != nullptr)
	{
		// Determine how many inherited native components exist
		int32 NumNativeComponents = 0;
		if (BlueprintClass != nullptr)
		{
			TArray<UObject*> PotentialComponents;
			BlueprintClass->GetDefaultObjectSubobjects(/*out*/ PotentialComponents);

			for (UObject* TestSubObject : PotentialComponents)
			{
				if (Cast<UActorComponent>(TestSubObject) != nullptr)
				{
					++NumNativeComponents;
				}
			}
		}
		OutTags.Add(FAssetRegistryTag("NativeComponents", FString::FromInt(NumNativeComponents), UObject::FAssetRegistryTag::TT_Numerical));

		// Determine how many components are added via a SimpleConstructionScript (both newly introduced and inherited from parent BPs)
		int32 NumAddedComponents = 0;
		for (UBlueprintGeneratedClass* TestBPClass = BlueprintClass; TestBPClass != nullptr; TestBPClass = Cast<UBlueprintGeneratedClass>(TestBPClass->GetSuperClass()))
		{
			const UBlueprint* AssociatedBP = Cast<const UBlueprint>(TestBPClass->ClassGeneratedBy);
			if (AssociatedBP->SimpleConstructionScript != nullptr)
			{
				NumAddedComponents += AssociatedBP->SimpleConstructionScript->GetAllNodesConst().Num();
			}
		}
		OutTags.Add(FAssetRegistryTag("BlueprintComponents", FString::FromInt(NumAddedComponents), UObject::FAssetRegistryTag::TT_Numerical));
	}
}

FPrimaryAssetId UBlueprint::GetPrimaryAssetId() const
{
	// Forward to our Class, which will forward to CDO if needed
	// We use Generated instead of Skeleton because the CDO data is more accurate on Generated
	if (GeneratedClass)
	{
		return GeneratedClass->GetPrimaryAssetId();
	}

	return FPrimaryAssetId();
}

FString UBlueprint::GetFriendlyName() const
{
	return GetName();
}

bool UBlueprint::AllowsDynamicBinding() const
{
	return FBlueprintEditorUtils::IsActorBased(this);
}

bool UBlueprint::SupportsInputEvents() const
{
	return FBlueprintEditorUtils::IsActorBased(this);
}

struct FBlueprintInnerHelper
{
	template<typename TOBJ, typename TARR>
	static TOBJ* FindObjectByName(TARR& Array, const FName& TimelineName)
	{
		for(int32 i=0; i<Array.Num(); i++)
		{
			TOBJ* Obj = Array[i];
			if((NULL != Obj) && (Obj->GetFName() == TimelineName))
			{
				return Obj;
			}
		}
		return NULL;
	}
};

UActorComponent* UBlueprint::FindTemplateByName(const FName& TemplateName) const
{
	return FBlueprintInnerHelper::FindObjectByName<UActorComponent>(ComponentTemplates, TemplateName);
}

const UTimelineTemplate* UBlueprint::FindTimelineTemplateByVariableName(const FName& TimelineName) const
{
	const FName TimelineTemplateName = *UTimelineTemplate::TimelineVariableNameToTemplateName(TimelineName);
	const UTimelineTemplate* Timeline =  FBlueprintInnerHelper::FindObjectByName<const UTimelineTemplate>(Timelines, TimelineTemplateName);

	// >>> Backwards Compatibility:  VER_UE4_EDITORONLY_BLUEPRINTS
	if(Timeline)
	{
		ensure(Timeline->GetOuter() && Timeline->GetOuter()->IsA(UClass::StaticClass()));
	}
	else
	{
		Timeline = FBlueprintInnerHelper::FindObjectByName<const UTimelineTemplate>(Timelines, TimelineName);
		if(Timeline)
		{
			ensure(Timeline->GetOuter() && Timeline->GetOuter() == this);
		}
	}
	// <<< End Backwards Compatibility

	return Timeline;
}

UTimelineTemplate* UBlueprint::FindTimelineTemplateByVariableName(const FName& TimelineName)
{
	const FName TimelineTemplateName = *UTimelineTemplate::TimelineVariableNameToTemplateName(TimelineName);
	UTimelineTemplate* Timeline = FBlueprintInnerHelper::FindObjectByName<UTimelineTemplate>(Timelines, TimelineTemplateName);
	
	// >>> Backwards Compatibility:  VER_UE4_EDITORONLY_BLUEPRINTS
	if(Timeline)
	{
		ensure(Timeline->GetOuter() && Timeline->GetOuter()->IsA(UClass::StaticClass()));
	}
	else
	{
		Timeline = FBlueprintInnerHelper::FindObjectByName<UTimelineTemplate>(Timelines, TimelineName);
		if(Timeline)
		{
			ensure(Timeline->GetOuter() && Timeline->GetOuter() == this);
		}
	}
	// <<< End Backwards Compatibility

	return Timeline;
}

bool UBlueprint::ForceLoad(UObject* Obj)
{
	FLinkerLoad* Linker = Obj->GetLinker();
	if (Linker && !Obj->HasAnyFlags(RF_LoadCompleted))
	{
		check(!GEventDrivenLoaderEnabled);
		Obj->SetFlags(RF_NeedLoad);
		Linker->Preload(Obj);
		return true;
	}
	return false;
}

void UBlueprint::ForceLoadMembers(UObject* InObject)
{
	// Collect a list of all things this element owns
	TArray<UObject*> MemberReferences;
	FReferenceFinder ComponentCollector(MemberReferences, InObject, false, true, true, true);
	ComponentCollector.FindReferences(InObject);

	// Iterate over the list, and preload everything so it is valid for refreshing
	for (TArray<UObject*>::TIterator it(MemberReferences); it; ++it)
	{
		UObject* CurrentObject = *it;
		if (ForceLoad(CurrentObject))
		{
			ForceLoadMembers(CurrentObject);
		}
	}
}

void UBlueprint::ForceLoadMetaData(UObject* InObject)
{
	checkSlow(InObject);
	UPackage* Package = InObject->GetOutermost();
	checkSlow(Package);
	UMetaData* MetaData = Package->GetMetaData();
	checkSlow(MetaData);
	ForceLoad(MetaData);
}

bool UBlueprint::ValidateGeneratedClass(const UClass* InClass)
{
	const UBlueprintGeneratedClass* GeneratedClass = Cast<const UBlueprintGeneratedClass>(InClass);
	if (!ensure(GeneratedClass))
	{
		return false;
	}
	const UBlueprint* Blueprint = GetBlueprintFromClass(GeneratedClass);
	if (!ensure(Blueprint))
	{
		return false;
	}

	for (auto CompIt = Blueprint->ComponentTemplates.CreateConstIterator(); CompIt; ++CompIt)
	{
		const UActorComponent* Component = (*CompIt);
		if (!ensure(Component && (Component->GetOuter() == GeneratedClass)))
		{
			return false;
		}
	}
	for (auto CompIt = GeneratedClass->ComponentTemplates.CreateConstIterator(); CompIt; ++CompIt)
	{
		const UActorComponent* Component = (*CompIt);
		if (!ensure(Component && (Component->GetOuter() == GeneratedClass)))
		{
			return false;
		}
	}

	for (auto CompIt = Blueprint->Timelines.CreateConstIterator(); CompIt; ++CompIt)
	{
		const UTimelineTemplate* Template = (*CompIt);
		if (!ensure(Template && (Template->GetOuter() == GeneratedClass)))
		{
			return false;
		}
	}
	for (auto CompIt = GeneratedClass->Timelines.CreateConstIterator(); CompIt; ++CompIt)
	{
		const UTimelineTemplate* Template = (*CompIt);
		if (!ensure(Template && (Template->GetOuter() == GeneratedClass)))
		{
			return false;
		}
	}

	if (const USimpleConstructionScript* SimpleConstructionScript = Blueprint->SimpleConstructionScript)
	{
		if (!ensure(SimpleConstructionScript->GetOuter() == GeneratedClass))
		{
			return false;
		}
	}
	if (const USimpleConstructionScript* SimpleConstructionScript = GeneratedClass->SimpleConstructionScript)
	{
		if (!ensure(SimpleConstructionScript->GetOuter() == GeneratedClass))
		{
			return false;
		}
	}

	if (const UInheritableComponentHandler* InheritableComponentHandler = Blueprint->InheritableComponentHandler)
	{
		if (!ensure(InheritableComponentHandler->GetOuter() == GeneratedClass))
		{
			return false;
		}
	}

	if (const UInheritableComponentHandler* InheritableComponentHandler = GeneratedClass->InheritableComponentHandler)
	{
		if (!ensure(InheritableComponentHandler->GetOuter() == GeneratedClass))
		{
			return false;
		}
	}

	return true;
}

void UBlueprint::BeginCacheForCookedPlatformData(const ITargetPlatform *TargetPlatform)
{
	Super::BeginCacheForCookedPlatformData(TargetPlatform);

	if (GeneratedClass && GeneratedClass->IsChildOf<AActor>())
	{
		int32 NumCookedComponents = 0;
		const double StartTime = FPlatformTime::Seconds();

		// If nativization is enabled and this Blueprint class will NOT be nativized, we need to determine if any of its parent Blueprints will be nativized and flag it for the runtime code.
		// Note: Currently, this flag is set on Actor-based Blueprint classes only. If it's ever needed for non-Actor-based Blueprint classes at runtime, then this needs to be updated to match.
		const IBlueprintNativeCodeGenCore* NativeCodeGenCore = IBlueprintNativeCodeGenCore::Get();
		if (GeneratedClass != nullptr && NativeCodeGenCore != nullptr)
		{
			ensure(TargetPlatform);
			const FCompilerNativizationOptions& NativizationOptions = NativeCodeGenCore->GetNativizationOptionsForPlatform(TargetPlatform);
			TArray<const UBlueprintGeneratedClass*> ParentBPClassStack;
			UBlueprintGeneratedClass::GetGeneratedClassesHierarchy(GeneratedClass->GetSuperClass(), ParentBPClassStack);
			for (const UBlueprintGeneratedClass *ParentBPClass : ParentBPClassStack)
			{
				if (NativeCodeGenCore->IsTargetedForReplacement(ParentBPClass, NativizationOptions) == EReplacementResult::ReplaceCompletely)
				{
					if (UBlueprintGeneratedClass* BPGC = CastChecked<UBlueprintGeneratedClass>(*GeneratedClass))
					{
						// Flag that this BP class will have a nativized parent class.
						BPGC->bHasNativizedParent = true;

						// Cache the IsTargetedForReplacement() result for the parent BP class that we know to be nativized.
						TMap<const UClass*, bool> ParentBPClassNativizationResultMap;
						ParentBPClassNativizationResultMap.Add(ParentBPClass, true);

						// Cook all overridden SCS component node templates inherited from parent BP classes that will be nativized.
						if (UInheritableComponentHandler* TargetInheritableComponentHandler = BPGC->GetInheritableComponentHandler())
						{
							for (auto RecordIt = TargetInheritableComponentHandler->CreateRecordIterator(); RecordIt; ++RecordIt)
							{
								if (!RecordIt->CookedComponentInstancingData.bIsValid)
								{
									// Get the original class that we're overriding a template from.
									const UClass* ComponentTemplateOwnerClass = RecordIt->ComponentKey.GetComponentOwner();

									// Check to see if we've already checked this class for nativization; if not, then cache the result of a new query and return the result.
									bool bIsOwnerClassTargetedForReplacement = false;
									bool* bCachedResult = ParentBPClassNativizationResultMap.Find(ComponentTemplateOwnerClass);
									if (bCachedResult)
									{
										bIsOwnerClassTargetedForReplacement = *bCachedResult;
									}
									else
									{
										bool bResult = (NativeCodeGenCore->IsTargetedForReplacement(RecordIt->ComponentKey.GetComponentOwner(), NativizationOptions) == EReplacementResult::ReplaceCompletely);
										bIsOwnerClassTargetedForReplacement = ParentBPClassNativizationResultMap.Add(ComponentTemplateOwnerClass, bResult);
									}

									if (bIsOwnerClassTargetedForReplacement)
									{
										// Use the template's archetype for the delta serialization here; remaining properties will have already been set via native subobject instancing at runtime.
										const bool bUseTemplateArchetype = true;
										FBlueprintEditorUtils::BuildComponentInstancingData(RecordIt->ComponentTemplate, RecordIt->CookedComponentInstancingData, bUseTemplateArchetype);
										++NumCookedComponents;
									}
								}
							}
						}
					}
					
					// All remaining antecedent classes should be native or nativized; no need to continue.
					break;
				}
			}
		}

		// Only cook component data if the setting is enabled and this is an Actor-based Blueprint class.
		if (GetDefault<UCookerSettings>()->bCookBlueprintComponentTemplateData)
		{
			if (UBlueprintGeneratedClass* BPGClass = CastChecked<UBlueprintGeneratedClass>(*GeneratedClass))
			{
				// Cook all overridden SCS component node templates inherited from the parent class hierarchy.
				if (UInheritableComponentHandler* TargetInheritableComponentHandler = BPGClass->GetInheritableComponentHandler())
				{
					for (auto RecordIt = TargetInheritableComponentHandler->CreateRecordIterator(); RecordIt; ++RecordIt)
					{
						if (!RecordIt->CookedComponentInstancingData.bIsValid)
						{
							// Note: This will currently block until finished.
							// @TODO - Make this an async task so we can potentially cook instancing data for multiple components in parallel.
							FBlueprintEditorUtils::BuildComponentInstancingData(RecordIt->ComponentTemplate, RecordIt->CookedComponentInstancingData);
							++NumCookedComponents;
						}
					}
				}

				// Cook all SCS component templates that are owned by this class.
				if (BPGClass->SimpleConstructionScript)
				{
					for (auto Node : BPGClass->SimpleConstructionScript->GetAllNodes())
					{
						if (!Node->CookedComponentInstancingData.bIsValid)
						{
							// Note: This will currently block until finished.
							// @TODO - Make this an async task so we can potentially cook instancing data for multiple components in parallel.
							FBlueprintEditorUtils::BuildComponentInstancingData(Node->ComponentTemplate, Node->CookedComponentInstancingData);
							++NumCookedComponents;
						}
					}
				}

				// Cook all UCS/AddComponent node templates that are owned by this class.
				for (UActorComponent* ComponentTemplate : BPGClass->ComponentTemplates)
				{
					FBlueprintCookedComponentInstancingData& CookedComponentInstancingData = BPGClass->CookedComponentInstancingData.FindOrAdd(ComponentTemplate->GetFName());
					if (!CookedComponentInstancingData.bIsValid)
					{
						// Note: This will currently block until finished.
						// @TODO - Make this an async task so we can potentially cook instancing data for multiple components in parallel.
						FBlueprintEditorUtils::BuildComponentInstancingData(ComponentTemplate, CookedComponentInstancingData);
						++NumCookedComponents;
					}
				}
			}
		}

		if (NumCookedComponents > 0)
		{
			UE_LOG(LogBlueprint, Log, TEXT("%s: Cooked %d component(s) in %.02g ms"), *GetName(), NumCookedComponents, (FPlatformTime::Seconds() - StartTime) * 1000.0);
		}
	}
}

bool UBlueprint::IsCachedCookedPlatformDataLoaded(const ITargetPlatform* TargetPlatform)
{
	// @TODO - Check async tasks for completion. For now just return TRUE since all tasks will currently block until finished.
	return true;
}

void UBlueprint::ClearAllCachedCookedPlatformData()
{
	Super::ClearAllCachedCookedPlatformData();

	if (UBlueprintGeneratedClass* BPGClass = Cast<UBlueprintGeneratedClass>(*GeneratedClass))
	{
		// Clear cooked data for overridden SCS component node templates inherited from the parent class hierarchy.
		if (UInheritableComponentHandler* TargetInheritableComponentHandler = BPGClass->GetInheritableComponentHandler())
		{
			for (auto RecordIt = TargetInheritableComponentHandler->CreateRecordIterator(); RecordIt; ++RecordIt)
			{
				RecordIt->CookedComponentInstancingData = FBlueprintCookedComponentInstancingData();
			}
		}

		// Clear cooked data for SCS component node templates.
		if (BPGClass->SimpleConstructionScript)
		{
			for (auto Node : BPGClass->SimpleConstructionScript->GetAllNodes())
			{
				Node->CookedComponentInstancingData = FBlueprintCookedComponentInstancingData();
			}
		}

		// Clear cooked data for UCS/AddComponent node templates.
		BPGClass->CookedComponentInstancingData.Empty();
	}
}

#endif // WITH_EDITOR

UBlueprint* UBlueprint::GetBlueprintFromClass(const UClass* InClass)
{
	UBlueprint* BP = NULL;
	if (InClass != NULL)
	{
		BP = Cast<UBlueprint>(InClass->ClassGeneratedBy);
	}
	return BP;
}

bool UBlueprint::GetBlueprintHierarchyFromClass(const UClass* InClass, TArray<UBlueprint*>& OutBlueprintParents)
{
	OutBlueprintParents.Empty();

	bool bNoErrors = true;
	const UClass* CurrentClass = InClass;
	while (UBlueprint* BP = UBlueprint::GetBlueprintFromClass(CurrentClass))
	{
		OutBlueprintParents.Add(BP);

#if WITH_EDITORONLY_DATA
		bNoErrors &= (BP->Status != BS_Error);
#endif // #if WITH_EDITORONLY_DATA

		// If valid, use stored ParentClass rather than the actual UClass::GetSuperClass(); handles the case when the class has not been recompiled yet after a reparent operation.
		if(const UClass* ParentClass = BP->ParentClass)
		{
			CurrentClass = ParentClass;
		}
		else
		{
			CurrentClass = CurrentClass->GetSuperClass();
		}
	}

	return bNoErrors;
}

ETimelineSigType UBlueprint::GetTimelineSignatureForFunctionByName(const FName& FunctionName, const FName& ObjectPropertyName)
{
	check(SkeletonGeneratedClass != NULL);
	
	UClass* UseClass = SkeletonGeneratedClass;

	// If an object property was specified, find the class of that property instead
	if(ObjectPropertyName != NAME_None)
	{
		UObjectPropertyBase* ObjProperty = FindField<UObjectPropertyBase>(SkeletonGeneratedClass, ObjectPropertyName);
		if(ObjProperty == NULL)
		{
			UE_LOG(LogBlueprint, Log, TEXT("GetTimelineSignatureForFunction: Object Property '%s' not found."), *ObjectPropertyName.ToString());
			return ETS_InvalidSignature;
		}

		UseClass = ObjProperty->PropertyClass;
	}

	UFunction* Function = FindField<UFunction>(UseClass, FunctionName);
	if(Function == NULL)
	{
		UE_LOG(LogBlueprint, Log, TEXT("GetTimelineSignatureForFunction: Function '%s' not found in class '%s'."), *FunctionName.ToString(), *UseClass->GetName());
		return ETS_InvalidSignature;
	}

	return UTimelineComponent::GetTimelineSignatureForFunction(Function);


	//UE_LOG(LogBlueprint, Log, TEXT("GetTimelineSignatureForFunction: No SkeletonGeneratedClass in Blueprint '%s'."), *GetName());
	//return ETS_InvalidSignature;
}

FString UBlueprint::GetDesc(void)
{
	FString BPType;
	switch (BlueprintType)
	{
		case BPTYPE_MacroLibrary:
			BPType = TEXT("macros for");
			break;
		case BPTYPE_Const:
			BPType = TEXT("const extends");
			break;
		case BPTYPE_Interface:
			// Always extends interface, so no extraneous information needed
			BPType = TEXT("");
			break;
		default:
			BPType = TEXT("extends");
			break;
	}
	const FString ResultString = FString::Printf(TEXT("%s %s"), *BPType, *ParentClass->GetName());

	return ResultString;
}

bool UBlueprint::NeedsLoadForClient() const
{
	return false;
}

bool UBlueprint::NeedsLoadForServer() const
{
	return false;
}

bool UBlueprint::NeedsLoadForEditorGame() const
{
	return true;
}

void UBlueprint::TagSubobjects(EObjectFlags NewFlags)
{
	Super::TagSubobjects(NewFlags);

	if (GeneratedClass && !GeneratedClass->HasAnyFlags(GARBAGE_COLLECTION_KEEPFLAGS))
	{
		GeneratedClass->SetFlags(NewFlags);
		GeneratedClass->TagSubobjects(NewFlags);
	}

	if (SkeletonGeneratedClass && SkeletonGeneratedClass != GeneratedClass && !SkeletonGeneratedClass->HasAnyFlags(GARBAGE_COLLECTION_KEEPFLAGS))
	{
		SkeletonGeneratedClass->SetFlags(NewFlags);
		SkeletonGeneratedClass->TagSubobjects(NewFlags);
	}
}

void UBlueprint::GetAllGraphs(TArray<UEdGraph*>& Graphs) const
{
#if WITH_EDITORONLY_DATA
	for (int32 i = 0; i < FunctionGraphs.Num(); ++i)
	{
		UEdGraph* Graph = FunctionGraphs[i];
		Graphs.Add(Graph);
		Graph->GetAllChildrenGraphs(Graphs);
	}
	for (int32 i = 0; i < MacroGraphs.Num(); ++i)
	{
		UEdGraph* Graph = MacroGraphs[i];
		Graphs.Add(Graph);
		Graph->GetAllChildrenGraphs(Graphs);
	}

	for (int32 i = 0; i < UbergraphPages.Num(); ++i)
	{
		UEdGraph* Graph = UbergraphPages[i];
		Graphs.Add(Graph);
		Graph->GetAllChildrenGraphs(Graphs);
	}

	for (int32 i = 0; i < DelegateSignatureGraphs.Num(); ++i)
	{
		UEdGraph* Graph = DelegateSignatureGraphs[i];
		Graphs.Add(Graph);
		Graph->GetAllChildrenGraphs(Graphs);
	}

	for (int32 BPIdx=0; BPIdx<ImplementedInterfaces.Num(); BPIdx++)
	{
		const FBPInterfaceDescription& InterfaceDesc = ImplementedInterfaces[BPIdx];
		for (int32 GraphIdx = 0; GraphIdx < InterfaceDesc.Graphs.Num(); GraphIdx++)
		{
			UEdGraph* Graph = InterfaceDesc.Graphs[GraphIdx];
			Graphs.Add(Graph);
			Graph->GetAllChildrenGraphs(Graphs);
		}
	}
#endif // WITH_EDITORONLY_DATA
}

#if WITH_EDITOR
void UBlueprint::Message_Note(const FString& MessageToLog)
{
	if( CurrentMessageLog )
	{
		CurrentMessageLog->Note(*MessageToLog);
	}
	else
	{
		UE_LOG(LogBlueprint, Log, TEXT("[%s] %s"), *GetName(), *MessageToLog);
	}
}

void UBlueprint::Message_Warn(const FString& MessageToLog)
{
	if( CurrentMessageLog )
	{
		CurrentMessageLog->Warning(*MessageToLog);
	}
	else
	{
		UE_LOG(LogBlueprint, Warning, TEXT("[%s] %s"), *GetName(), *MessageToLog);
	}
}

void UBlueprint::Message_Error(const FString& MessageToLog)
{
	if( CurrentMessageLog )
	{
		CurrentMessageLog->Error(*MessageToLog);
	}
	else
	{
		UE_LOG(LogBlueprint, Error, TEXT("[%s] %s"), *GetName(), *MessageToLog);
	}
}

bool UBlueprint::ChangeOwnerOfTemplates()
{
	struct FUniqueNewNameHelper
	{
	private:
		FString NewName;
		bool isValid;
	public:
		FUniqueNewNameHelper(const FString& Name, UObject* Outer) : isValid(false)
		{
			const uint32 Hash = FCrc::StrCrc32<TCHAR>(*Name);
			NewName = FString::Printf(TEXT("%s__%08X"), *Name, Hash);
			isValid = IsUniqueObjectName(FName(*NewName), Outer);
			if (!isValid)
			{
				check(Outer);
				UE_LOG(LogBlueprint, Warning, TEXT("ChangeOwnerOfTemplates: Cannot generate a deterministic new name. Old name: %s New outer: %s"), *Name, *Outer->GetName());
			}
		}

		const TCHAR* Get() const
		{
			return isValid ? *NewName : NULL;
		}
	};

	UBlueprintGeneratedClass* BPGClass = Cast<UBlueprintGeneratedClass>(*GeneratedClass);
	bool bIsStillStale = false;
	if (BPGClass)
	{
		check(!bIsRegeneratingOnLoad);

		// >>> Backwards Compatibility:  VER_UE4_EDITORONLY_BLUEPRINTS
		bool bMigratedOwner = false;
		TSet<class UCurveBase*> Curves;
		for( auto CompIt = ComponentTemplates.CreateIterator(); CompIt; ++CompIt )
		{
			UActorComponent* Component = (*CompIt);
			if (Component)
			{
				if (Component->GetOuter() == this)
				{
					const bool bRenamed = Component->Rename(*Component->GetName(), BPGClass, REN_ForceNoResetLoaders | REN_DoNotDirty);
					ensure(bRenamed);
					bIsStillStale |= !bRenamed;
					bMigratedOwner = true;
				}
				if (auto TimelineComponent = Cast<UTimelineComponent>(Component))
				{
					TimelineComponent->GetAllCurves(Curves);
				}
			}
		}

		for( auto CompIt = Timelines.CreateIterator(); CompIt; ++CompIt )
		{
			UTimelineTemplate* Template = (*CompIt);
			if (Template)
			{
				if(Template->GetOuter() == this)
				{
					const FString OldTemplateName = Template->GetName();
					ensure(!OldTemplateName.EndsWith(TEXT("_Template")));
					const bool bRenamed = Template->Rename(*UTimelineTemplate::TimelineVariableNameToTemplateName(Template->GetFName()), BPGClass, REN_ForceNoResetLoaders|REN_DoNotDirty);
					ensure(bRenamed);
					bIsStillStale |= !bRenamed;
					ensure(OldTemplateName == UTimelineTemplate::TimelineTemplateNameToVariableName(Template->GetFName()));
					bMigratedOwner = true;
				}
				Template->GetAllCurves(Curves);
			}
		}
		for (auto Curve : Curves)
		{
			if (Curve && (Curve->GetOuter() == this))
			{
				const bool bRenamed = Curve->Rename(FUniqueNewNameHelper(Curve->GetName(), BPGClass).Get(), BPGClass, REN_ForceNoResetLoaders | REN_DoNotDirty);
				ensure(bRenamed);
				bIsStillStale |= !bRenamed;
			}
		}

		if(USimpleConstructionScript* SCS = SimpleConstructionScript)
		{
			if(SCS->GetOuter() == this)
			{
				const bool bRenamed = SCS->Rename(FUniqueNewNameHelper(SCS->GetName(), BPGClass).Get(), BPGClass, REN_ForceNoResetLoaders | REN_DoNotDirty);
				ensure(bRenamed);
				bIsStillStale |= !bRenamed;
				bMigratedOwner = true;
			}

			for (USCS_Node* SCSNode : SCS->GetAllNodes())
			{
				UActorComponent* Component = SCSNode ? SCSNode->ComponentTemplate : NULL;
				if (Component && Component->GetOuter() == this)
				{
					const bool bRenamed = Component->Rename(FUniqueNewNameHelper(Component->GetName(), BPGClass).Get(), BPGClass, REN_ForceNoResetLoaders | REN_DoNotDirty);
					ensure(bRenamed);
					bIsStillStale |= !bRenamed;
					bMigratedOwner = true;
				}
			}
		}

		if (bMigratedOwner)
		{
			if( !HasAnyFlags( RF_Transient ))
			{
				// alert the user that blueprints gave been migrated and require re-saving to enable them to locate and fix them without nagging them.
				FMessageLog("BlueprintLog").Warning( FText::Format( NSLOCTEXT( "Blueprint", "MigrationWarning", "Blueprint {0} has been migrated and requires re-saving to avoid import errors" ), FText::FromString( *GetName() )));

				if( GetDefault<UEditorLoadingSavingSettings>()->bDirtyMigratedBlueprints )
				{
					UPackage* BPPackage = GetOutermost();

					if( BPPackage )
					{
						BPPackage->SetDirtyFlag( true );
					}
				}
			}

			BPGClass->ComponentTemplates = ComponentTemplates;
			BPGClass->Timelines = Timelines;
			BPGClass->SimpleConstructionScript = SimpleConstructionScript;
		}
		// <<< End Backwards Compatibility
	}
	else
	{
		UE_LOG(LogBlueprint, Log, TEXT("ChangeOwnerOfTemplates: No BlueprintGeneratedClass in %s"), *GetName());
	}
	return !bIsStillStale;
}

void UBlueprint::PostLoadSubobjects(FObjectInstancingGraph* OuterInstanceGraph)
{
	Super::PostLoadSubobjects(OuterInstanceGraph);
	ChangeOwnerOfTemplates();

	ConformNativeComponents(this);
}

bool UBlueprint::Modify(bool bAlwaysMarkDirty)
{
	bCachedDependenciesUpToDate = false;
	return Super::Modify(bAlwaysMarkDirty);
}

void UBlueprint::GatherDependencies(TSet<TWeakObjectPtr<UBlueprint>>& InDependencies) const
{

}

void UBlueprint::ReplaceDeprecatedNodes()
{
	TArray<UEdGraph*> Graphs;
	GetAllGraphs(Graphs);

	for ( auto It = Graphs.CreateIterator(); It; ++It )
	{
		UEdGraph* const Graph = *It;
		const UEdGraphSchema* Schema = Graph->GetSchema();
		Schema->BackwardCompatibilityNodeConversion(Graph, true);
	}
}

UInheritableComponentHandler* UBlueprint::GetInheritableComponentHandler(bool bCreateIfNecessary)
{
	static const FBoolConfigValueHelper EnableInheritableComponents(TEXT("Kismet"), TEXT("bEnableInheritableComponents"), GEngineIni);
	if (!EnableInheritableComponents)
	{
		return nullptr;
	}

	if (!InheritableComponentHandler && bCreateIfNecessary)
	{
		UBlueprintGeneratedClass* BPGC = CastChecked<UBlueprintGeneratedClass>(GeneratedClass);
		ensure(!BPGC->InheritableComponentHandler);
		InheritableComponentHandler = BPGC->GetInheritableComponentHandler(true);
	}
	return InheritableComponentHandler;
}

#endif

#if WITH_EDITOR
FName UBlueprint::GetFunctionNameFromClassByGuid(const UClass* InClass, const FGuid FunctionGuid)
{
	return FBlueprintEditorUtils::GetFunctionNameFromClassByGuid(InClass, FunctionGuid);
}

bool UBlueprint::GetFunctionGuidFromClassByFieldName(const UClass* InClass, const FName FunctionName, FGuid& FunctionGuid)
{
	return FBlueprintEditorUtils::GetFunctionGuidFromClassByFieldName(InClass, FunctionName, FunctionGuid);
}

UEdGraph* UBlueprint::GetLastEditedUberGraph() const
{
	for ( int32 LastEditedIndex = LastEditedDocuments.Num() - 1; LastEditedIndex >= 0; LastEditedIndex-- )
	{
		if ( UObject* Obj = LastEditedDocuments[LastEditedIndex].EditedObject )
		{
			if ( UEdGraph* Graph = Cast<UEdGraph>(Obj) )
			{
				for ( int32 GraphIndex = 0; GraphIndex < UbergraphPages.Num(); GraphIndex++ )
				{
					if ( Graph == UbergraphPages[GraphIndex] )
					{
						return UbergraphPages[GraphIndex];
					}
				}
			}
		}
	}

	if ( UbergraphPages.Num() > 0 )
	{
		return UbergraphPages[0];
	}

	return nullptr;
}

#endif //WITH_EDITOR
