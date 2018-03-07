// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.


#include "CoreMinimal.h"

#include "BlueprintCompilationManager.h"
#include "Misc/CoreMisc.h"
#include "Stats/StatsMisc.h"
#include "Stats/Stats.h"
#include "Modules/ModuleManager.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Serialization/ArchiveUObject.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/MetaData.h"
#include "Templates/SubclassOf.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectHash.h"
#include "Serialization/FindObjectReferencers.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "Misc/PackageName.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Textures/SlateIcon.h"
#include "Framework/Commands/UIAction.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/Blueprint.h"
#include "EdGraph/EdGraph.h"
#include "Editor/EditorEngine.h"
#include "Settings/EditorExperimentalSettings.h"
#include "Animation/AnimBlueprint.h"
#include "Engine/MemberReference.h"
#include "GeneralProjectSettings.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/LevelScriptBlueprint.h"
#include "EdGraphSchema_K2.h"
#include "EdGraphSchema_K2_Actions.h"
#include "K2Node_Event.h"
#include "K2Node_ActorBoundEvent.h"
#include "K2Node_CallParentFunction.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_Composite.h"
#include "K2Node_FunctionEntry.h"
#include "AnimationGraph.h"
#include "AnimationGraphSchema.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/KismetReinstanceUtilities.h"
#include "Engine/SCS_Node.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetDebugUtilities.h"
#include "Framework/MultiBox/MultiBoxBuilder.h"
#include "Editor.h"
#include "Toolkits/AssetEditorManager.h"
#include "Editor/Kismet/Public/BlueprintEditorModule.h"
#include "Editor/Kismet/Public/FindInBlueprintManager.h"
#include "Toolkits/ToolkitManager.h"
#include "Editor/KismetCompiler/Public/KismetCompilerModule.h"
#include "Kismet2/CompilerResultsLog.h"
#include "IAssetTools.h"
#include "AssetRegistryModule.h"
#include "Layers/ILayers.h"
#include "ScopedTransaction.h"
#include "AssetToolsModule.h"
#include "EngineAnalytics.h"
#include "AnalyticsEventAttribute.h"
#include "Interfaces/IAnalyticsProvider.h"
#include "ActorEditorUtils.h"
#include "ObjectEditorUtils.h"
#include "Dialogs/DlgPickAssetPath.h"
#include "ComponentAssetBroker.h"
#include "BlueprintEditorSettings.h"
#include "Editor/UnrealEd/Public/PackageTools.h"
#include "Framework/Notifications/NotificationManager.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Engine/InheritableComponentHandler.h"

DECLARE_CYCLE_STAT(TEXT("Compile Blueprint"), EKismetCompilerStats_CompileBlueprint, STATGROUP_KismetCompiler);
DECLARE_CYCLE_STAT(TEXT("Broadcast Precompile"), EKismetCompilerStats_BroadcastPrecompile, STATGROUP_KismetCompiler);
DECLARE_CYCLE_STAT(TEXT("Update Search Metadata"), EKismetCompilerStats_UpdateSearchMetaData, STATGROUP_KismetCompiler);
DECLARE_CYCLE_STAT(TEXT("Garbage Collection"), EKismetCompilerStats_GarbageCollection, STATGROUP_KismetCompiler);
DECLARE_CYCLE_STAT(TEXT("Refresh Dependent Blueprints"), EKismetCompilerStats_RefreshDependentBlueprints, STATGROUP_KismetCompiler);
DECLARE_CYCLE_STAT(TEXT("Validate Generated Class"), EKismetCompilerStats_ValidateGeneratedClass, STATGROUP_KismetCompiler);

#define LOCTEXT_NAMESPACE "UnrealEd.Editor"

extern COREUOBJECT_API bool GBlueprintUseCompilationManager;

//////////////////////////////////////////////////////////////////////////
// FArchiveInvalidateTransientRefs

/**
 * Archive built to go through and find any references to objects in the transient package, and then NULL those references
 */
class FArchiveInvalidateTransientRefs : public FArchiveUObject
{
public:
	FArchiveInvalidateTransientRefs()
	{
		ArIsObjectReferenceCollector = true;
		ArIsPersistent = false;
		ArIgnoreArchetypeRef = false;
	}
protected:
	/** 
	 * UObject serialize operator implementation
	 *
	 * @param Object	reference to Object reference
	 * @return reference to instance of this class
	 */
	FArchive& operator<<( UObject*& Object )
	{
		// Check if this is a reference to an object existing in the transient package, and if so, NULL it.
		if ((Object != NULL) && (Object->GetOutermost() == GetTransientPackage()) )
		{
			check( Object->IsValidLowLevel() );
			Object = NULL;
		}

		return *this;
	}
};


//////////////////////////////////////////////////////////////////////////
// FBlueprintObjectsBeingDebuggedIterator

FBlueprintObjectsBeingDebuggedIterator::FBlueprintObjectsBeingDebuggedIterator(UBlueprint* InBlueprint)
	: Blueprint(InBlueprint)
{
}

UObject* FBlueprintObjectsBeingDebuggedIterator::operator* () const
{
	return Blueprint->GetObjectBeingDebugged();
}

UObject* FBlueprintObjectsBeingDebuggedIterator::operator-> () const
{
	return Blueprint->GetObjectBeingDebugged();
}

FBlueprintObjectsBeingDebuggedIterator& FBlueprintObjectsBeingDebuggedIterator::operator++()
{
	Blueprint = NULL;
	return *this;
}

bool FBlueprintObjectsBeingDebuggedIterator::IsValid() const
{
	return Blueprint != NULL;
}



//////////////////////////////////////////////////////////////////////////
// FObjectsBeingDebuggedIterator

FObjectsBeingDebuggedIterator::FObjectsBeingDebuggedIterator()
	: SelectedActorsIter(*GEditor->GetSelectedActors())
	, LevelScriptActorIndex(INDEX_NONE)
{
	FindNextLevelScriptActor();
}

UWorld* FObjectsBeingDebuggedIterator::GetWorld() const
{
	return (GEditor->PlayWorld != NULL) ? GEditor->PlayWorld : GWorld;
}

UObject* FObjectsBeingDebuggedIterator::operator* () const
{
	return SelectedActorsIter ? *SelectedActorsIter : (UObject*)(GetWorld()->GetLevel(LevelScriptActorIndex)->GetLevelScriptActor());
}

UObject* FObjectsBeingDebuggedIterator::operator-> () const
{
	return SelectedActorsIter ? *SelectedActorsIter : (UObject*)(GetWorld()->GetLevel(LevelScriptActorIndex)->GetLevelScriptActor());
}

FObjectsBeingDebuggedIterator& FObjectsBeingDebuggedIterator::operator++()
{
	if (SelectedActorsIter)
	{
		++SelectedActorsIter;
	}
	else
	{
		FindNextLevelScriptActor();
	}

	return *this;
}

bool FObjectsBeingDebuggedIterator::IsValid() const
{
	return SelectedActorsIter || (LevelScriptActorIndex < GetWorld()->GetNumLevels());
}

void FObjectsBeingDebuggedIterator::FindNextLevelScriptActor()
{
	while (++LevelScriptActorIndex < GetWorld()->GetNumLevels())
	{
		ULevel* Level = GetWorld()->GetLevel(LevelScriptActorIndex);
		if ((Level != NULL) && (Level->GetLevelScriptActor() != NULL))
		{
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// FBlueprintUnloader

/** Utility struct, used to aid in unloading and replacing a specific blueprint. */
struct FBlueprintUnloader
{
public:
	FBlueprintUnloader(UBlueprint* OldBlueprint);

	/** 
	 * Unloads the specified Blueprint (marking it pending-kill, and removing it 
	 * from its outer package). Optionally, will unload the package as well.
	 *
	 * @param  bResetPackage	Whether or not this should unload the entire package.
	 */
	void UnloadBlueprint(const bool bResetPackage);
	
	/** 
	 * Replaces all old references to the original blueprints (its class/CDO/etc.)
	 * @param  NewBlueprint	The blueprint to replace old references with
	 */
	void ReplaceStaleRefs(UBlueprint* NewBlueprint);

private:
	TWeakObjectPtr<UBlueprint> OldBlueprint;
	UClass*  OldGeneratedClass;
	UObject* OldCDO;
	UClass*  OldSkeletonClass;
	UObject* OldSkelCDO;
};

FBlueprintUnloader::FBlueprintUnloader(UBlueprint* OldBlueprintIn)
	: OldBlueprint(OldBlueprintIn)
	, OldGeneratedClass(OldBlueprint->GeneratedClass)
	, OldCDO(nullptr)
	, OldSkeletonClass(OldBlueprint->SkeletonGeneratedClass)
	, OldSkelCDO(nullptr)
{
	if (OldGeneratedClass != nullptr)
	{
		OldCDO = OldGeneratedClass->GetDefaultObject(/*bCreateIfNeeded =*/false);
	}
	if (OldSkeletonClass != nullptr)
	{
		OldSkelCDO = OldSkeletonClass->GetDefaultObject(/*bCreateIfNeeded =*/false);
	}
	OldBlueprint = OldBlueprintIn;
}

void FBlueprintUnloader::UnloadBlueprint(const bool bResetPackage)
{
	if (OldBlueprint.IsValid())
	{
		UBlueprint* UnloadingBp = OldBlueprint.Get();

		UPackage* const OldPackage = UnloadingBp->GetOutermost();
		bool const bIsDirty = OldPackage->IsDirty();

		UPackage* const TransientPackage = GetTransientPackage();
		check(OldPackage != TransientPackage); // is the blueprint already unloaded?
		
		FName const BlueprintName = UnloadingBp->GetFName();
		// move the blueprint to the transient package (to be picked up by garbage collection later)
		FName UnloadedName = MakeUniqueObjectName(TransientPackage, UBlueprint::StaticClass(), BlueprintName);
		UnloadingBp->Rename(*UnloadedName.ToString(), TransientPackage, REN_DontCreateRedirectors | REN_DoNotDirty);
		// @TODO: currently, REN_DoNotDirty does not guarantee that the package 
		//        will not be marked dirty
		OldPackage->SetDirtyFlag(bIsDirty);

		// make sure the blueprint is properly trashed (remove it from the package)
		UnloadingBp->SetFlags(RF_Transient);
		UnloadingBp->ClearFlags(RF_Standalone | RF_Transactional);
		UnloadingBp->RemoveFromRoot();
		UnloadingBp->MarkPendingKill();
		// if it's in the undo buffer, then we have to clear that...
		if (FKismetEditorUtilities::IsReferencedByUndoBuffer(UnloadingBp))
		{
			GEditor->Trans->Reset(LOCTEXT("UnloadedBlueprint", "Unloaded Blueprint"));
		}

		if (bResetPackage)
		{
			TArray<UPackage*> PackagesToUnload;
			PackagesToUnload.Add(OldPackage);

			FText PackageUnloadError;
			PackageTools::UnloadPackages(PackagesToUnload, PackageUnloadError);

			if (!PackageUnloadError.IsEmpty())
			{
				const FText ErrorMessage = FText::Format(LOCTEXT("UnloadBpPackageError", "Failed to unload Bluprint '{0}': {1}"),
					FText::FromName(BlueprintName), PackageUnloadError);
				FSlateNotificationManager::Get().AddNotification(FNotificationInfo(ErrorMessage));

				// fallback to manually setting up the package so it can reload 
				// the blueprint 
				ResetLoaders(OldPackage);
				OldPackage->ClearFlags(RF_WasLoaded);
				OldPackage->bHasBeenFullyLoaded = false;
				OldPackage->GetMetaData()->RemoveMetaDataOutsidePackage();
			}
		}

		FKismetEditorUtilities::OnBlueprintUnloaded.Broadcast(UnloadingBp);

		// handled in FBlueprintEditor (from the OnBlueprintUnloaded event)
// 		IAssetEditorInstance* EditorInst = FAssetEditorManager::Get().FindEditorForAsset(UnloadingBp, /*bFocusIfOpen =*/false);
// 		if (EditorInst != nullptr)
// 		{
// 			EditorInst->CloseWindow();
// 		}
	}
}

void FBlueprintUnloader::ReplaceStaleRefs(UBlueprint* NewBlueprint)
{
	//--------------------------------------
	// Construct redirects
	//--------------------------------------

	TMap<UObject*, UObject*> Redirects;
	TArray<UObject*> OldObjsNeedingReplacing;

	if (OldBlueprint.IsValid(/*bEvenIfPendingKill =*/true))
	{
		UBlueprint* ToBeReplaced = OldBlueprint.Get(/*bEvenIfPendingKill =*/true);
		if (OldGeneratedClass != nullptr)
		{
			OldObjsNeedingReplacing.Add(OldGeneratedClass);
			Redirects.Add(OldGeneratedClass, NewBlueprint->GeneratedClass);
		}
		if (OldCDO != nullptr)
		{
			OldObjsNeedingReplacing.Add(OldCDO);
			Redirects.Add(OldCDO, NewBlueprint->GeneratedClass->GetDefaultObject());
		}
		if (OldSkeletonClass != nullptr)
		{
			OldObjsNeedingReplacing.Add(OldSkeletonClass);
			Redirects.Add(OldSkeletonClass, NewBlueprint->SkeletonGeneratedClass);
		}
		if (OldSkelCDO != nullptr)
		{
			OldObjsNeedingReplacing.Add(OldSkelCDO);
			Redirects.Add(OldSkelCDO, NewBlueprint->SkeletonGeneratedClass->GetDefaultObject());
		}

		OldObjsNeedingReplacing.Add(ToBeReplaced);
		Redirects.Add(ToBeReplaced, NewBlueprint);

		// clear the object being debugged; otherwise ReplaceInstancesOfClass()  
		// trys to reset it with a new level instance, and OldBlueprint won't 
		// match the new instance's type (it's now a NewBlueprint)
		ToBeReplaced->SetObjectBeingDebugged(nullptr);
	}

	//--------------------------------------
	// Replace old references
	//--------------------------------------

	TArray<UObject*> Referencers;
	// find all objects, still referencing the old blueprint/class/cdo/etc.
	for (auto Referencer : TFindObjectReferencers<UObject>(OldObjsNeedingReplacing, /*PackageToCheck =*/nullptr, /*bIgnoreTemplates =*/false))
	{
		Referencers.Add(Referencer.Value);
	}

	FBlueprintCompileReinstancer::ReplaceInstancesOfClass(OldGeneratedClass, NewBlueprint->GeneratedClass);

	for (UObject* Referencer : Referencers)
	{
		FArchiveReplaceObjectRef<UObject>(Referencer, Redirects, /*bNullPrivateRefs=*/false, /*bIgnoreOuterRef=*/false, /*bIgnoreArchetypeRef=*/false);
	}
}

//////////////////////////////////////////////////////////////////////////

// Static variable definition
TArray<FString> FKismetEditorUtilities::TrackedBlueprintParentList;
FKismetEditorUtilities::FOnBlueprintUnloaded FKismetEditorUtilities::OnBlueprintUnloaded;
TMultiMap<void*, FKismetEditorUtilities::FDefaultEventNodeData> FKismetEditorUtilities::AutoGeneratedDefaultEventsMap;
TMultiMap<void*, FKismetEditorUtilities::FOnBlueprintCreatedData> FKismetEditorUtilities::OnBlueprintCreatedCallbacks;

/** Create the correct event graphs for this blueprint */
void FKismetEditorUtilities::CreateDefaultEventGraphs(UBlueprint* Blueprint)
{
	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	UEdGraph* Ubergraph = FBlueprintEditorUtils::CreateNewGraph(Blueprint, K2Schema->GN_EventGraph, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
	Ubergraph->bAllowDeletion = false; //@TODO: Really, just want to make sure we never drop below 1, not that you cannot delete any particular one!
	FBlueprintEditorUtils::AddUbergraphPage(Blueprint, Ubergraph);

	Blueprint->LastEditedDocuments.AddUnique(Ubergraph);
}

extern UNREALED_API FSecondsCounterData BlueprintCompileAndLoadTimerData;

/** Create a new Blueprint and initialize it to a valid state. */
UBlueprint* FKismetEditorUtilities::CreateBlueprint(UClass* ParentClass, UObject* Outer, const FName NewBPName, EBlueprintType BlueprintType, TSubclassOf<UBlueprint> BlueprintClassType, TSubclassOf<UBlueprintGeneratedClass> BlueprintGeneratedClassType, FName CallingContext)
{
	FSecondsCounterScope Timer(BlueprintCompileAndLoadTimerData);
	check(FindObject<UBlueprint>(Outer, *NewBPName.ToString()) == NULL); 

	// Not all types are legal for all parent classes, if the parent class is const then the blueprint cannot be an ubergraph-bearing one
	if ((BlueprintType == BPTYPE_Normal) && (ParentClass->HasAnyClassFlags(CLASS_Const)))
	{
		BlueprintType = BPTYPE_Const;
	}
	
	// Create new UBlueprint object
	UBlueprint* NewBP = NewObject<UBlueprint>(Outer, *BlueprintClassType, NewBPName, RF_Public | RF_Standalone | RF_Transactional | RF_LoadCompleted);
	NewBP->Status = BS_BeingCreated;
	NewBP->BlueprintType = BlueprintType;
	NewBP->ParentClass = ParentClass;
	NewBP->BlueprintSystemVersion = UBlueprint::GetCurrentBlueprintSystemVersion();
	NewBP->bIsNewlyCreated = true;
	NewBP->bLegacyNeedToPurgeSkelRefs = false;
	NewBP->GenerateNewGuid();

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	// Create SimpleConstructionScript and UserConstructionScript
	if (FBlueprintEditorUtils::SupportsConstructionScript(NewBP))
	{ 
		// >>> Temporary workaround, before a BlueprintGeneratedClass is the main asset.
		FName NewSkelClassName, NewGenClassName;
		NewBP->GetBlueprintClassNames(NewGenClassName, NewSkelClassName);
		UBlueprintGeneratedClass* NewClass = NewObject<UBlueprintGeneratedClass>(
			NewBP->GetOutermost(), *BlueprintGeneratedClassType, NewGenClassName, RF_Public | RF_Transactional);
		NewBP->GeneratedClass = NewClass;
		NewClass->ClassGeneratedBy = NewBP;
		NewClass->SetSuperStruct(ParentClass);
		// <<< Temporary workaround

		NewBP->SimpleConstructionScript = NewObject<USimpleConstructionScript>(NewClass);
		NewBP->SimpleConstructionScript->SetFlags(RF_Transactional);
		NewBP->LastEditedDocuments.Add(NewBP->SimpleConstructionScript);

		UEdGraph* UCSGraph = FBlueprintEditorUtils::CreateNewGraph(NewBP, K2Schema->FN_UserConstructionScript, UEdGraph::StaticClass(), UEdGraphSchema_K2::StaticClass());
		FBlueprintEditorUtils::AddFunctionGraph(NewBP, UCSGraph, /*bIsUserCreated=*/ false, AActor::StaticClass());

		// If the blueprint is derived from another blueprint, add in a super-call automatically
		if( NewBP->ParentClass && NewBP->ParentClass->ClassGeneratedBy )
		{
			check( UCSGraph->Nodes.Num() > 0 );
			UK2Node_FunctionEntry* UCSEntry = CastChecked<UK2Node_FunctionEntry>(UCSGraph->Nodes[0]);
			FGraphNodeCreator<UK2Node_CallParentFunction> FunctionNodeCreator(*UCSGraph);
			UK2Node_CallParentFunction* ParentFunctionNode = FunctionNodeCreator.CreateNode();
			ParentFunctionNode->FunctionReference.SetExternalMember(K2Schema->FN_UserConstructionScript, NewBP->ParentClass);
			ParentFunctionNode->NodePosX = 200;
			ParentFunctionNode->NodePosY = 0;
			ParentFunctionNode->AllocateDefaultPins();
			FunctionNodeCreator.Finalize();

			// Wire up the new node
			UEdGraphPin* ExecPin = UCSEntry->FindPin(K2Schema->PN_Then);
			UEdGraphPin* SuperPin = ParentFunctionNode->FindPin(K2Schema->PN_Execute);
			ExecPin->MakeLinkTo(SuperPin);
		}

		NewBP->LastEditedDocuments.Add(UCSGraph);
		UCSGraph->bAllowDeletion = false;
	}

	// Create default event graph(s)
	if (FBlueprintEditorUtils::DoesSupportEventGraphs(NewBP))
	{
		check(NewBP->UbergraphPages.Num() == 0);
		CreateDefaultEventGraphs(NewBP);
	}

	//@TODO: ANIMREFACTOR 1: This kind of code should be on a per-blueprint basis; not centralized here
	if (UAnimBlueprint* AnimBP = Cast<UAnimBlueprint>(NewBP))
	{
		UAnimBlueprint* RootAnimBP = UAnimBlueprint::FindRootAnimBlueprint(AnimBP);
		if (RootAnimBP == NULL)
		{
			// Only allow an anim graph if there isn't one in a parent blueprint
			UEdGraph* NewGraph = FBlueprintEditorUtils::CreateNewGraph(AnimBP, K2Schema->GN_AnimGraph, UAnimationGraph::StaticClass(), UAnimationGraphSchema::StaticClass());
			FBlueprintEditorUtils::AddDomainSpecificGraph(NewBP, NewGraph);
			NewBP->LastEditedDocuments.Add(NewGraph);
			NewGraph->bAllowDeletion = false;
		}
		else
		{
			// Make sure the anim blueprint targets the same skeleton as the parent
			AnimBP->TargetSkeleton = RootAnimBP->TargetSkeleton;
		}
	}

	// Create initial UClass
	IKismetCompilerInterface& Compiler = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>(KISMET_COMPILER_MODULENAME);
	
	FCompilerResultsLog Results;
	const bool bReplaceExistingInstances = false;
	NewBP->Status = BS_Dirty;
	FKismetCompilerOptions CompileOptions;
	Compiler.CompileBlueprint(NewBP, CompileOptions, Results);

	// Mark the BP as being regenerated, so it will not be confused as needing to be loaded and regenerated when a referenced BP loads.
	NewBP->bHasBeenRegenerated = true;

	UBlueprintEditorSettings* Settings = GetMutableDefault<UBlueprintEditorSettings>();
	if(Settings && Settings->bSpawnDefaultBlueprintNodes)
	{
		// Only add default events if there is an ubergraph and they are supported
		if(NewBP->UbergraphPages.Num() && FBlueprintEditorUtils::DoesSupportEventGraphs(NewBP))
		{
			// Based on the Blueprint type we are constructing, place some starting events.
			// Note, this cannot happen in the Factories for constructing these Blueprint types due to the fact that creating child BPs circumvent the factories
			UClass* WidgetClass = FindObject<UClass>(ANY_PACKAGE, TEXT("UserWidget"));
			UClass* GameplayAbilityClass = FindObject<UClass>(ANY_PACKAGE, TEXT("GameplayAbility"));

			TArray<FName> AutoSpawnedEventNames;
			int32 NodePositionY = 0;

			// Spawn any defined auto generated default events for the class.  Only do this for the most senior class specified, so
			// that subclasses may have an entirely different set of default nodes if they wish.
			UClass* DefaultNodesClass = NewBP->GeneratedClass;
			while ( DefaultNodesClass )
			{
				bool bFoundDefaultNodes = false;
				for ( TMultiMap<void*, FDefaultEventNodeData>::TIterator DataIt(AutoGeneratedDefaultEventsMap); DataIt; ++DataIt )
				{
					FDefaultEventNodeData Data = DataIt.Value();
					if ( DefaultNodesClass == Data.TargetClass )
					{
						bFoundDefaultNodes = true;
						FKismetEditorUtilities::AddDefaultEventNode(NewBP, NewBP->UbergraphPages[0], Data.EventName, Data.TargetClass, NodePositionY);
					}
				}

				if ( bFoundDefaultNodes )
				{
					break;
				}

				DefaultNodesClass = DefaultNodesClass->GetSuperClass();
			}
		}

		// Give anyone who wants to do more advanced BP modification post-creation a chance to do so.
		// Anim Blueprints, for example, adds a non-event node to the main ubergraph.
		for (TMultiMap<void*, FKismetEditorUtilities::FOnBlueprintCreatedData>::TIterator DataIt(OnBlueprintCreatedCallbacks); DataIt; ++DataIt)
		{
			FOnBlueprintCreatedData Data = DataIt.Value();
			if (NewBP->GeneratedClass->IsChildOf(Data.TargetClass))
			{
				FKismetEditorUtilities::FOnBlueprintCreated BlueprintCreatedDelegate = Data.OnBlueprintCreated;
				BlueprintCreatedDelegate.Execute(NewBP);
			}
		}
	}
	

	// Report blueprint creation to analytics
	if (FEngineAnalytics::IsAvailable())
	{
		TArray<FAnalyticsEventAttribute> Attribs;

		// translate the CallingContext into a string for analytics
		if (CallingContext != NAME_None)
		{
			Attribs.Add(FAnalyticsEventAttribute(FString("Context"), CallingContext.ToString()));
		}
		
		Attribs.Add(FAnalyticsEventAttribute(FString("ParentType"), ParentClass->ClassGeneratedBy == NULL ? FString("Native") : FString("Blueprint")));

		if(IsTrackedBlueprintParent(ParentClass))
		{
			Attribs.Add(FAnalyticsEventAttribute(FString("ParentClass"), ParentClass->GetName()));
		}

		const UGeneralProjectSettings& ProjectSettings = *GetDefault<UGeneralProjectSettings>();
		Attribs.Add(FAnalyticsEventAttribute(FString("ProjectId"), ProjectSettings.ProjectID.ToString()));
		Attribs.Add(FAnalyticsEventAttribute(FString("BlueprintId"), NewBP->GetBlueprintGuid().ToString()));

		FEngineAnalytics::GetProvider().RecordEvent(FString("Editor.Usage.BlueprintCreated"), Attribs);
	}

	return NewBP;
}

void FKismetEditorUtilities::RegisterAutoGeneratedDefaultEvent(void* InOwner, UClass* InTargetClass, FName InEventName)
{
	FDefaultEventNodeData Data;
	Data.TargetClass = InTargetClass;
	Data.EventName = InEventName;
	AutoGeneratedDefaultEventsMap.Add(InOwner, Data);
}

void FKismetEditorUtilities::RegisterOnBlueprintCreatedCallback(void* InOwner, UClass* InTargetClass, FOnBlueprintCreated InOnBlueprintCreatedCallback)
{
	FOnBlueprintCreatedData Data;
	Data.TargetClass = InTargetClass;
	Data.OnBlueprintCreated = InOnBlueprintCreatedCallback;
	OnBlueprintCreatedCallbacks.Add(InOwner, Data);
}

void FKismetEditorUtilities::UnregisterAutoBlueprintNodeCreation(void* InOwner)
{
	AutoGeneratedDefaultEventsMap.Remove(InOwner);
	OnBlueprintCreatedCallbacks.Remove(InOwner);
}

UK2Node_Event* FKismetEditorUtilities::AddDefaultEventNode(UBlueprint* InBlueprint, UEdGraph* InGraph, FName InEventName, UClass* InEventClass, int32& InOutNodePosY)
{
	UK2Node_Event* NewEventNode = nullptr;

	FMemberReference EventReference;
	EventReference.SetExternalMember(InEventName, InEventClass);

	// Prevent events that are hidden in the Blueprint's class from being auto-generated.
	if(!FObjectEditorUtils::IsFunctionHiddenFromClass(EventReference.ResolveMember<UFunction>(InBlueprint), InBlueprint->ParentClass))
	{
		const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

		// Add the event
		NewEventNode = NewObject<UK2Node_Event>(InGraph);
		NewEventNode->EventReference = EventReference;

		// add update event graph
		NewEventNode->bOverrideFunction=true;
		NewEventNode->CreateNewGuid();
		NewEventNode->PostPlacedNewNode();
		NewEventNode->SetFlags(RF_Transactional);
		NewEventNode->AllocateDefaultPins();
		NewEventNode->bCommentBubblePinned = true;
		NewEventNode->bCommentBubbleVisible = true;
		NewEventNode->NodePosY = InOutNodePosY;
		UEdGraphSchema_K2::SetNodeMetaData(NewEventNode, FNodeMetadata::DefaultGraphNode);
		InOutNodePosY = NewEventNode->NodePosY + NewEventNode->NodeHeight + 200;

		InGraph->AddNode(NewEventNode);

		// Get the function that the event node or function entry represents
		FFunctionFromNodeHelper FunctionFromNode(NewEventNode);
		if (FunctionFromNode.Function && Schema->GetCallableParentFunction(FunctionFromNode.Function))
		{
			UFunction* ValidParent = Schema->GetCallableParentFunction(FunctionFromNode.Function);
			FGraphNodeCreator<UK2Node_CallParentFunction> FunctionNodeCreator(*InGraph);
			UK2Node_CallParentFunction* ParentFunctionNode = FunctionNodeCreator.CreateNode();
			ParentFunctionNode->SetFromFunction(ValidParent);
			ParentFunctionNode->AllocateDefaultPins();

			for (UEdGraphPin* EventPin : NewEventNode->Pins)
			{
				if (UEdGraphPin* ParentPin = ParentFunctionNode->FindPin(EventPin->PinName, EGPD_Input))
				{
					ParentPin->MakeLinkTo(EventPin);
				}
			}
			ParentFunctionNode->GetExecPin()->MakeLinkTo(NewEventNode->FindPin(Schema->PN_Then));

			ParentFunctionNode->NodePosX = FunctionFromNode.Node->NodePosX + FunctionFromNode.Node->NodeWidth + 200;
			ParentFunctionNode->NodePosY = FunctionFromNode.Node->NodePosY;
			UEdGraphSchema_K2::SetNodeMetaData(ParentFunctionNode, FNodeMetadata::DefaultGraphNode);
			FunctionNodeCreator.Finalize();

			ParentFunctionNode->MakeAutomaticallyPlacedGhostNode();
		}

		NewEventNode->MakeAutomaticallyPlacedGhostNode();
	}

	return NewEventNode;
}

UBlueprint* FKismetEditorUtilities::ReloadBlueprint(UBlueprint* StaleBlueprint)
{
	check(StaleBlueprint->IsAsset());
	FSoftObjectPath BlueprintAssetRef(StaleBlueprint);

	FBlueprintUnloader Unloader(StaleBlueprint);
	Unloader.UnloadBlueprint(/*bResetPackage =*/true);

	UBlueprint* ReloadedBlueprint = Cast<UBlueprint>(StaticLoadObject(UBlueprint::StaticClass(), /*Outer =*/nullptr, *BlueprintAssetRef.ToString()));

	Unloader.ReplaceStaleRefs(ReloadedBlueprint);
	return ReloadedBlueprint;
}

UBlueprint* FKismetEditorUtilities::ReplaceBlueprint(UBlueprint* Target, UBlueprint const* ReplacementArchetype)
{
	if (Target == ReplacementArchetype)
	{
		return Target;
	}

	FName DesiredName = Target->GetFName();

	UPackage* BlueprintPackage = Target->GetOutermost();
	check(BlueprintPackage != GetTransientPackage());

	FBlueprintUnloader Unloader(Target);
	Unloader.UnloadBlueprint(/*bResetPackage =*/false);

	UBlueprint* Replacement = Cast<UBlueprint>(StaticDuplicateObject(ReplacementArchetype, BlueprintPackage, DesiredName));
	
	Unloader.ReplaceStaleRefs(Replacement);
	return Replacement;
}

bool FKismetEditorUtilities::IsReferencedByUndoBuffer(UBlueprint* Blueprint)
{
	UObject* BlueprintObj = Blueprint;
	FReferencerInformationList ReferencesIncludingUndo;
	IsReferenced(BlueprintObj, GARBAGE_COLLECTION_KEEPFLAGS, EInternalObjectFlags::GarbageCollectionKeepFlags, /*bCheckSubObjects =*/true, &ReferencesIncludingUndo);

	FReferencerInformationList ReferencesExcludingUndo;
	// Determine the in-memory references, *excluding* the undo buffer
	GEditor->Trans->DisableObjectSerialization();
	IsReferenced(BlueprintObj, GARBAGE_COLLECTION_KEEPFLAGS, EInternalObjectFlags::GarbageCollectionKeepFlags, /*bCheckSubObjects =*/true, &ReferencesExcludingUndo);
	GEditor->Trans->EnableObjectSerialization();

	// see if this object is the transaction buffer - set a flag so we know we need to clear the undo stack
	const int32 TotalReferenceCount   = ReferencesIncludingUndo.ExternalReferences.Num() + ReferencesIncludingUndo.InternalReferences.Num();
	const int32 NonUndoReferenceCount = ReferencesExcludingUndo.ExternalReferences.Num() + ReferencesExcludingUndo.InternalReferences.Num();

	return (TotalReferenceCount > NonUndoReferenceCount);
}

void FKismetEditorUtilities::CompileBlueprint(UBlueprint* BlueprintObj, EBlueprintCompileOptions CompileFlags, FCompilerResultsLog* pResults)
{
	if(GBlueprintUseCompilationManager)
	{
		FBlueprintCompilationManager::CompileSynchronously(FBPCompileRequest(BlueprintObj, CompileFlags, pResults));
		return;
	}

	const bool bIsRegeneratingOnLoad		= (CompileFlags & EBlueprintCompileOptions::IsRegeneratingOnLoad		) != EBlueprintCompileOptions::None;
	const bool bSkipGarbageCollection		= (CompileFlags & EBlueprintCompileOptions::SkipGarbageCollection		) != EBlueprintCompileOptions::None;
	const bool bSaveIntermediateProducts	= (CompileFlags & EBlueprintCompileOptions::SaveIntermediateProducts	) != EBlueprintCompileOptions::None;
	const bool bSkeletonUpToDate			= (CompileFlags & EBlueprintCompileOptions::SkeletonUpToDate			) != EBlueprintCompileOptions::None;
	const bool bBatchCompile				= (CompileFlags & EBlueprintCompileOptions::BatchCompile				) != EBlueprintCompileOptions::None;
	const bool bSkipReinstancing			= (CompileFlags & EBlueprintCompileOptions::SkipReinstancing			) != EBlueprintCompileOptions::None;

	FSecondsCounterScope Timer(BlueprintCompileAndLoadTimerData); 
	BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_CompileBlueprint);

	// Wipe the PreCompile log, any generated messages are now irrelevant
	BlueprintObj->PreCompileLog.Reset();

	// Broadcast pre-compile
#if WITH_EDITOR
	{
		BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_BroadcastPrecompile);
		if(GEditor && GIsEditor)
		{
			GEditor->BroadcastBlueprintPreCompile(BlueprintObj);
		}
	}
#endif

	// Reset the flag, so if the user tries to use PIE it will warn them if the BP did not compile
	BlueprintObj->bDisplayCompilePIEWarning = true;

	UPackage* const BlueprintPackage = BlueprintObj->GetOutermost();
	// compiling the blueprint will inherently dirty the package, but if there 
	// weren't any changes to save before, there shouldn't be after
	bool const bStartedWithUnsavedChanges = (BlueprintPackage != NULL) ? BlueprintPackage->IsDirty() : true;
#if WITH_EDITOR
	// Do not want to run this code without the editor present nor when running commandlets.
	if (GEditor && GIsEditor)
	{
		BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_UpdateSearchMetaData);
		// We do not want to regenerate a search Guid during loads, nothing has changed in the Blueprint and it is cached elsewhere
		if (!bIsRegeneratingOnLoad)
		{
			FFindInBlueprintSearchManager::Get().AddOrUpdateBlueprintSearchMetadata(BlueprintObj);
		}
	}
#endif

	// The old class is either the GeneratedClass if we had an old successful compile, or the SkeletonGeneratedClass stub if there were previously fatal errors
	UClass* OldClass = (BlueprintObj->GeneratedClass != NULL && (BlueprintObj->GeneratedClass != BlueprintObj->SkeletonGeneratedClass)) ? BlueprintObj->GeneratedClass : NULL;

	// Load the compiler
	IKismetCompilerInterface& Compiler = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>(KISMET_COMPILER_MODULENAME);

	// Prepare old objects for reinstancing
	TGuardValue<bool> GuardTemplateNameFlag(GCompilingBlueprint, true);

	// Compile
	FCompilerResultsLog LocalResults;
	FCompilerResultsLog& Results = (pResults != NULL) ? *pResults : LocalResults;

	// Monitoring UE-20486, the OldClass->ClassGeneratedBy is NULL or otherwise not a UBlueprint.
	if (OldClass && (OldClass->ClassGeneratedBy != BlueprintObj))
	{
		ensureMsgf(OldClass->ClassGeneratedBy == BlueprintObj, TEXT("Generated Class '%s' has an invalid ClassGeneratedBy '%s' while the expected is Blueprint '%s'"), *OldClass->GetPathName(), *GetPathNameSafe(OldClass->ClassGeneratedBy), *BlueprintObj->GetPathName());
		OldClass->ClassGeneratedBy = BlueprintObj;
	}
	TSharedPtr<class FBlueprintCompileReinstancer> ReinstanceHelper;
	if(!bSkipReinstancing)
	{
		ReinstanceHelper = FBlueprintCompileReinstancer::Create(OldClass);
	}

	// If enabled, suppress errors/warnings in the log if we're recompiling on load on a build machine
	static const FBoolConfigValueHelper IgnoreCompileOnLoadErrorsOnBuildMachine(TEXT("Kismet"), TEXT("bIgnoreCompileOnLoadErrorsOnBuildMachine"), GEngineIni);
	Results.bLogInfoOnly = BlueprintObj->bIsRegeneratingOnLoad && GIsBuildMachine && IgnoreCompileOnLoadErrorsOnBuildMachine;

	FKismetCompilerOptions CompileOptions;
	CompileOptions.bSaveIntermediateProducts = bSaveIntermediateProducts;
	CompileOptions.bRegenerateSkelton = !bSkeletonUpToDate;
	CompileOptions.bReinstanceAndStubOnFailure = !bSkipReinstancing;
	Compiler.CompileBlueprint(BlueprintObj, CompileOptions, Results, ReinstanceHelper);

	FBlueprintEditorUtils::UpdateDelegatesInBlueprint(BlueprintObj);

	if (FBlueprintEditorUtils::IsLevelScriptBlueprint(BlueprintObj))
	{
		// When the Blueprint is recompiled, then update the bound events for level scripting
		ULevelScriptBlueprint* LevelScriptBP = CastChecked<ULevelScriptBlueprint>(BlueprintObj);

		if (ULevel* BPLevel = LevelScriptBP->GetLevel())
		{
			BPLevel->OnLevelScriptBlueprintChanged(LevelScriptBP);
		}
	}

	if(!bSkipReinstancing)
	{
		ReinstanceHelper->UpdateBytecodeReferences();
	}
	
	// in case any errors/warnings have been added since the call to Compiler.CompileBlueprint()
	if (Results.NumErrors > 0)
	{
		BlueprintObj->Status = BS_Error;
	}
	else if (Results.NumWarnings > 0)
	{
		BlueprintObj->Status = BS_UpToDateWithWarnings;
	}

	const bool bIsInterface = FBlueprintEditorUtils::IsInterfaceBlueprint(BlueprintObj);
	const bool bLetReinstancerRefreshDependBP = !bIsRegeneratingOnLoad && (OldClass != NULL) && !bIsInterface;
	if (bLetReinstancerRefreshDependBP)
	{
		BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_RefreshDependentBlueprints);

		TArray<UBlueprint*> DependentBPs;
		FBlueprintEditorUtils::GetDependentBlueprints(BlueprintObj, DependentBPs);
		if(!bSkipReinstancing)
		{
			ReinstanceHelper->ListDependentBlueprintsToRefresh(DependentBPs);
		}
	}

	if ( (!bIsRegeneratingOnLoad) && (OldClass != NULL))
	{
		// Strip off any external components from the CDO, if needed because of reparenting, etc
		FKismetEditorUtilities::StripExternalComponents(BlueprintObj);

		// Ensure that external SCS node references match up with the generated class
		if(BlueprintObj->SimpleConstructionScript)
		{
			BlueprintObj->SimpleConstructionScript->FixupRootNodeParentReferences();
		}

		// Replace instances of this class
		if(!bSkipReinstancing)
		{
			ReinstanceHelper->ReinstanceObjects();
		}
		
		// Notify everyone a blueprint has been compiled and reinstanced, but before GC so they can perform any final cleanup.
		if ( GEditor )
		{
			GEditor->BroadcastBlueprintReinstanced();
		}

		if (!bSkipGarbageCollection)
		{
			BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_GarbageCollection);

			// Garbage collect to make sure the old class and actors are disposed of
			CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
		}

		// If you need to verify that all old instances are taken care of, uncomment this!
		// ReinstanceHelper.VerifyReplacement();
	}

	if (!bBatchCompile)
	{
		BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_NotifyBlueprintChanged);

		BlueprintObj->BroadcastCompiled();

		if(GEditor)
		{
			GEditor->BroadcastBlueprintCompiled();	
		}
	}

	if (!bLetReinstancerRefreshDependBP && (bIsInterface || !BlueprintObj->bIsRegeneratingOnLoad) && !bSkipReinstancing)
	{
		BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_RefreshDependentBlueprints);

		TArray<UBlueprint*> DependentBPs;
		FBlueprintEditorUtils::GetDependentBlueprints(BlueprintObj, DependentBPs);

		// refresh each dependent blueprint
		for (UBlueprint* Dependent : DependentBPs)
		{
			// for interface changes, auto-refresh nodes on any dependent blueprints
			// note: RefreshAllNodes() will internally send a change notification event to the dependent blueprint
			if (bIsInterface)
			{
				bool bPreviousRegenValue = Dependent->bIsRegeneratingOnLoad;
				Dependent->bIsRegeneratingOnLoad = Dependent->bIsRegeneratingOnLoad || BlueprintObj->bIsRegeneratingOnLoad;
				FBlueprintEditorUtils::RefreshAllNodes(Dependent);
				Dependent->bIsRegeneratingOnLoad = bPreviousRegenValue;
			}
			else if(!BlueprintObj->bIsRegeneratingOnLoad)
			{
				// for non-interface changes, nodes with an external dependency have already been refreshed, and it is now safe to send a change notification event
				Dependent->BroadcastChanged();
			}
		}
	}

	if(!bIsRegeneratingOnLoad && BlueprintObj->GeneratedClass)
	{
		BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_ValidateGeneratedClass);
		UBlueprint::ValidateGeneratedClass(BlueprintObj->GeneratedClass);
	}

	if (BlueprintPackage != NULL)
	{
		BlueprintPackage->SetDirtyFlag(bStartedWithUnsavedChanges);
	}	

	UEdGraphPin::Purge();
}

/** Generates a blueprint skeleton only.  Minimal compile, no notifications will be sent, no GC, etc.  Only successful if there isn't already a skeleton generated */
bool FKismetEditorUtilities::GenerateBlueprintSkeleton(UBlueprint* BlueprintObj, bool bForceRegeneration)
{
	bool bRegeneratedSkeleton = false;
	FSecondsCounterScope Timer(BlueprintCompileAndLoadTimerData); 
	check(BlueprintObj);

	if( BlueprintObj->SkeletonGeneratedClass == NULL || bForceRegeneration )
	{
		UPackage* Package = BlueprintObj->GetOutermost();
		bool bIsPackageDirty = Package ? Package->IsDirty() : false;
					
		IKismetCompilerInterface& Compiler = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>(KISMET_COMPILER_MODULENAME);

		TGuardValue<bool> GuardTemplateNameFlag(GCompilingBlueprint, true);
		FCompilerResultsLog Results;

		FKismetCompilerOptions CompileOptions;
		CompileOptions.CompileType = EKismetCompileType::SkeletonOnly;
		Compiler.CompileBlueprint(BlueprintObj, CompileOptions, Results);
		bRegeneratedSkeleton = true;

		// Restore the package dirty flag here
		if( Package != NULL )
		{
			Package->SetDirtyFlag(bIsPackageDirty);
		}
	}
	return bRegeneratedSkeleton;
}

/** Recompiles the bytecode of a blueprint only.  Should only be run for recompiling dependencies during compile on load */
void FKismetEditorUtilities::RecompileBlueprintBytecode(UBlueprint* BlueprintObj, TArray<UObject*>* ObjLoaded,  EBlueprintBytecodeRecompileOptions Flags)
{
	FSecondsCounterScope Timer(BlueprintCompileAndLoadTimerData); 

	if(FBlueprintEditorUtils::IsCompileOnLoadDisabled(BlueprintObj))
	{
		return;
	}

	bool bBatchCompile = (Flags & EBlueprintBytecodeRecompileOptions::BatchCompile) != EBlueprintBytecodeRecompileOptions::None;
	bool bSkipReinstancing = (Flags & EBlueprintBytecodeRecompileOptions::SkipReinstancing) != EBlueprintBytecodeRecompileOptions::None;

	check(BlueprintObj);
	checkf(BlueprintObj->GeneratedClass, TEXT("Invalid generated class for %s"), *BlueprintObj->GetName());

	UPackage* const BlueprintPackage = BlueprintObj->GetOutermost();
	bool const bStartedWithUnsavedChanges = (BlueprintPackage != NULL) ? BlueprintPackage->IsDirty() : true;

	IKismetCompilerInterface& Compiler = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>(KISMET_COMPILER_MODULENAME);

	TGuardValue<bool> GuardTemplateNameFlag(GCompilingBlueprint, true);

	TSharedPtr<FBlueprintCompileReinstancer> ReinstanceHelper;
	if(!bSkipReinstancing)
	{
		ReinstanceHelper = FBlueprintCompileReinstancer::Create(BlueprintObj->GeneratedClass, EBlueprintCompileReinstancerFlags::BytecodeOnly | EBlueprintCompileReinstancerFlags::AutoInferSaveOnCompile);
	}

	FKismetCompilerOptions CompileOptions;
	CompileOptions.CompileType = EKismetCompileType::BytecodeOnly;
	{
		FRecreateUberGraphFrameScope RecreateUberGraphFrameScope(BlueprintObj->GeneratedClass, true);
		FCompilerResultsLog Results;
		Compiler.CompileBlueprint(BlueprintObj, CompileOptions, Results, NULL, ObjLoaded);
	}
	
	if(!bSkipReinstancing)
	{
		ReinstanceHelper->UpdateBytecodeReferences();
	}

	if (BlueprintPackage != NULL)
	{
		BlueprintPackage->SetDirtyFlag(bStartedWithUnsavedChanges);
	}

	if (!BlueprintObj->bIsRegeneratingOnLoad && !bBatchCompile)
	{
		BP_SCOPED_COMPILER_EVENT_STAT(EKismetCompilerStats_NotifyBlueprintChanged);

		BlueprintObj->BroadcastCompiled();

		if (GEditor)
		{
			GEditor->BroadcastBlueprintCompiled();
		}
	}
}

namespace ConformComponentsUtils
{
	static void ConformRemovedNativeComponents(UObject* BpCdo);
	static UObject* FindNativeArchetype(UActorComponent* Component);
};

static void ConformComponentsUtils::ConformRemovedNativeComponents(UObject* BpCdo)
{
	UClass* BlueprintClass = BpCdo->GetClass();
	check(BpCdo->HasAnyFlags(RF_ClassDefaultObject) && BlueprintClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint));

	AActor* ActorCDO = Cast<AActor>(BpCdo);
	if (ActorCDO == nullptr)
	{
		return;
	}

	UClass* const NativeSuperClass = FBlueprintEditorUtils::FindFirstNativeClass(BlueprintClass);
	const AActor* NativeCDO = GetDefault<AActor>(NativeSuperClass);

	TInlineComponentArray<UActorComponent*> OldNativeComponents;
	TInlineComponentArray<UActorComponent*> NewNativeComponents;
	ActorCDO->GetComponents(OldNativeComponents);
	USceneComponent* OldNativeRootComponent = ActorCDO->GetRootComponent();

	TSet<UObject*> DestroyedComponents;
	for (UActorComponent* Component : OldNativeComponents)
	{
		UObject* NativeArchetype = FindNativeArchetype(Component);
		if ((NativeArchetype == nullptr) || !NativeArchetype->HasAnyFlags(RF_ClassDefaultObject))
		{
			// Keep track of components inherited from the native super class that are still valid.
			NewNativeComponents.Add(Component);

			continue;
		}
		// else, the component has been removed from our native super class

		Component->DestroyComponent(/*bPromoteChildren =*/false);
		DestroyedComponents.Add(Component);

		// The DestroyComponent() call above will clear the RootComponent value in this case.
		if(Component == OldNativeRootComponent)
		{
			// Restore it here so that it will be reassigned to match the native CDO's value below.
			ActorCDO->SetRootComponent(OldNativeRootComponent);
		}

		UClass* ComponentClass = Component->GetClass();
		for (TFieldIterator<UArrayProperty> ArrayPropIt(NativeSuperClass); ArrayPropIt; ++ArrayPropIt)
		{
			UArrayProperty* ArrayProp = *ArrayPropIt;

			UObjectProperty* ObjInnerProp = Cast<UObjectProperty>(ArrayProp->Inner);
			if ((ObjInnerProp == nullptr) || !ComponentClass->IsChildOf(ObjInnerProp->PropertyClass))
			{
				continue;
			}

			uint8* BpArrayPtr = ArrayProp->ContainerPtrToValuePtr<uint8>(ActorCDO);
			FScriptArrayHelper BpArrayHelper(ArrayProp, BpArrayPtr);
			// iterate backwards so we can remove as we go
			for (int32 ArrayIndex = BpArrayHelper.Num()-1; ArrayIndex >= 0; --ArrayIndex)
			{
				uint8* BpEntryPtr = BpArrayHelper.GetRawPtr(ArrayIndex);
				UObject* ObjEntryValue = ObjInnerProp->GetObjectPropertyValue(BpEntryPtr);

				if (ObjEntryValue == Component)
				{
					// NOTE: until we fixup UE-15224, then this may be undesirably diverging from the natively defined 
					//       array (think delta serialization); however, I think from Blueprint creation on we treat 
					//       instanced sub-object arrays as differing (just may be confusing to the user)
					BpArrayHelper.RemoveValues(ArrayIndex);
				}
			}
		}

		// @TODO: have to also remove from map properties now that they're available
	}

	auto FindComponentTemplateByNameInActorCDO = [&NewNativeComponents](FName ToFind) -> UActorComponent**
	{
		return NewNativeComponents.FindByPredicate([ToFind](const UActorComponent* ActorComponent) -> bool
		{
			return ActorComponent && ActorComponent->GetFName() == ToFind;
		});
	};

	// 
	for (TFieldIterator<UObjectProperty> ObjPropIt(NativeSuperClass); ObjPropIt; ++ObjPropIt)
	{
		UObjectProperty* ObjectProp = *ObjPropIt;
		UObject* PropObjValue = ObjectProp->GetObjectPropertyValue_InContainer(ActorCDO);

		if (DestroyedComponents.Contains(PropObjValue))
		{
			// Get the "new" value that's currently set on the native parent CDO. We need the Blueprint CDO to reflect this update in property value.
			UObject* SuperObjValue = ObjectProp->GetObjectPropertyValue_InContainer(NativeCDO);
			if (SuperObjValue && SuperObjValue->IsA<UActorComponent>())
			{
				// For components, make sure we use the instance that's owned by the Blueprint CDO and not the native parent CDO's instance.
				if (UActorComponent** ComponentTemplatePtr = FindComponentTemplateByNameInActorCDO(SuperObjValue->GetFName()))
				{
					SuperObjValue = *ComponentTemplatePtr;
				}
			}
			
			// Update the Blueprint CDO to match the native parent CDO.
			ObjectProp->SetObjectPropertyValue_InContainer(ActorCDO, SuperObjValue);
		}
	}

	// Fix up the attachment hierarchy for inherited scene components that are still valid.
	for (UActorComponent* Component : NewNativeComponents)
	{
		if (USceneComponent* SceneComponent = Cast<USceneComponent>(Component))
		{
			// If the component in the Blueprint CDO was attached to a component that's been removed, update the Blueprint's component instance to match the archetype in the native parent CDO.
			if (DestroyedComponents.Contains(SceneComponent->GetAttachParent()))
			{
				if (USceneComponent* NativeArchetype = Cast<USceneComponent>(FindNativeArchetype(SceneComponent)))
				{
					USceneComponent* NewAttachParent = NativeArchetype->GetAttachParent();
					if (NewAttachParent)
					{
						// Make sure we use the instance that's owned by the Blueprint CDO and not the native parent CDO's instance.
						if (UActorComponent** ComponentTemplatePtr = FindComponentTemplateByNameInActorCDO(NewAttachParent->GetFName()))
						{
							NewAttachParent = CastChecked<USceneComponent>(*ComponentTemplatePtr);
						}
					}

					SceneComponent->SetupAttachment(NewAttachParent);
				}
			}
		}
	}
}

static UObject* ConformComponentsUtils::FindNativeArchetype(UActorComponent* Component)
{
	UActorComponent* Archetype = Cast<UActorComponent>(Component->GetArchetype());
	if (Archetype == nullptr)
	{
		return nullptr;
	}

	UObject* ArchetypeOwner = Archetype->GetOuter();
	UClass* OwnerClass = ArchetypeOwner->GetClass();

	const bool bOwnerIsNative = OwnerClass->HasAnyClassFlags(CLASS_Native);
	if (bOwnerIsNative)
	{
		return Archetype;
	}
	if (Archetype == Component)
	{
		return nullptr;
	}
	return FindNativeArchetype(Archetype);
}

/** Tries to make sure that a blueprint is conformed to its native parent, in case any native class flags have changed */
void FKismetEditorUtilities::ConformBlueprintFlagsAndComponents(UBlueprint* BlueprintObj)
{
	// Propagate native class flags to the children class.  This fixes up cases where native instanced components get added after BP creation, etc
	const UClass* ParentClass = BlueprintObj->ParentClass;

	if( UClass* SkelClass = BlueprintObj->SkeletonGeneratedClass )
	{
		SkelClass->ClassFlags |= (ParentClass->ClassFlags & CLASS_ScriptInherit);
		UObject* SkelCDO = SkelClass->GetDefaultObject();
		// NOTE: we don't need to call ConformRemovedNativeComponents() for skel
		//       classes, as they're generated on load (and not saved with stale 
		//       components)
		SkelCDO->InstanceSubobjectTemplates();
	}

	if( UClass* GenClass = BlueprintObj->GeneratedClass )
	{
		GenClass->ClassFlags |= (ParentClass->ClassFlags & CLASS_ScriptInherit);
		UObject* GenCDO = GenClass->GetDefaultObject();
		ConformComponentsUtils::ConformRemovedNativeComponents(GenCDO);
		GenCDO->InstanceSubobjectTemplates();
	}

	UInheritableComponentHandler* InheritableComponentHandler = BlueprintObj->GetInheritableComponentHandler(false);
	if (InheritableComponentHandler)
	{
		InheritableComponentHandler->ValidateTemplates();
	}
}

/** @return		true is it's possible to create a blueprint from the specified class */
bool FKismetEditorUtilities::CanCreateBlueprintOfClass(const UClass* Class)
{
	bool bAllowDerivedBlueprints = false;
	GConfig->GetBool(TEXT("Kismet"), TEXT("AllowDerivedBlueprints"), /*out*/ bAllowDerivedBlueprints, GEngineIni);

	const bool bCanCreateBlueprint =
		!Class->HasAnyClassFlags(CLASS_Deprecated)
		&& !Class->HasAnyClassFlags(CLASS_NewerVersionExists)
		&& (!Class->ClassGeneratedBy || (bAllowDerivedBlueprints && !IsClassABlueprintSkeleton(Class)));

	const bool bIsBPGC = (Cast<UBlueprintGeneratedClass>(Class) != nullptr);

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();
	const bool bIsValidClass = Class->GetBoolMetaDataHierarchical(FBlueprintMetadata::MD_IsBlueprintBase)
		|| (Class == UObject::StaticClass())
		|| (Class->HasAnyClassFlags(CLASS_CompiledFromBlueprint) || Class == USceneComponent::StaticClass() || Class == UActorComponent::StaticClass())
		|| bIsBPGC;  // BPs are always considered inheritable

	return bCanCreateBlueprint && bIsValidClass;
}

UBlueprint* FKismetEditorUtilities::CreateBlueprintFromActor(const FString& Path, AActor* Actor, const bool bReplaceActor, bool bKeepMobility /*= false*/)
{
	UBlueprint* NewBlueprint = nullptr;

	// Create a blueprint
	FString PackageName = Path;
	FString AssetName = FPackageName::GetLongPackageAssetName(Path);

	// If no AssetName was found, generate a unique asset name.
	if(AssetName.Len() == 0)
	{
		PackageName = FPackageName::GetLongPackagePath(Path);
		FString BasePath = PackageName + TEXT("/") + LOCTEXT("BlueprintName_Default", "NewBlueprint").ToString();
		FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
		AssetToolsModule.Get().CreateUniqueAssetName(BasePath, TEXT(""), PackageName, AssetName);
	}

	UPackage* Package = CreatePackage(NULL, *PackageName);

	if(Package)
	{
		NewBlueprint = CreateBlueprintFromActor(FName(*AssetName), Package, Actor, bReplaceActor, bKeepMobility);
	}

	return NewBlueprint;
}

void FKismetEditorUtilities::AddComponentsToBlueprint(UBlueprint* Blueprint, const TArray<UActorComponent*>& Components, bool bHarvesting, USCS_Node* OptionalNewRootNode, bool bKeepMobility /*= false*/)
{
	USimpleConstructionScript* SCS = Blueprint->SimpleConstructionScript;

	TArray<UBlueprint*> ParentBPStack;
	UBlueprint::GetBlueprintHierarchyFromClass(Blueprint->GeneratedClass, ParentBPStack);

	TMap<USceneComponent*, USCS_Node*> SceneComponentsToAdd;
	TMap<USceneComponent*, USCS_Node*> InstanceComponentToNodeMap;

	auto AddChildToSCSRootNodeLambda = [SCS, OptionalNewRootNode](USCS_Node* InSCSNode)
	{
		if (OptionalNewRootNode != nullptr)
		{
			OptionalNewRootNode->AddChildNode(InSCSNode);
		}
		else
		{
			// Continuation of convention from FCreateConstructionScriptFromSelectedActors::Execute, perhaps more elegant
			// to provide OptionalNewRootNode in both cases.
			SCS->GetRootNodes()[0]->AddChildNode(InSCSNode);
		}
	};

	struct FAddComponentsToBlueprintImpl
	{
		/** 
		 * Creates a new USCS_Node in the TargetSCS, duplicating the specified 
		 * component (leaving the new node unattached). If a copy was already 
		 * made (found  in NewSceneComponents) then that will be returned instead.
		 */
		static USCS_Node* MakeComponentCopy(UActorComponent* ActorComponent, USimpleConstructionScript* TargetSCS, TMap<USceneComponent*, USCS_Node*>& NewSceneComponents, bool bInternalKeepMobility)
		{
			USceneComponent* AsSceneComponent = Cast<USceneComponent>(ActorComponent);
			if (AsSceneComponent != nullptr)
			{
				USCS_Node** ExistingCopy = NewSceneComponents.Find(AsSceneComponent);
				if (ExistingCopy != nullptr)
				{
					return *ExistingCopy;
				}
			}

			USCS_Node* NewSCSNode = TargetSCS->CreateNode(ActorComponent->GetClass(), ActorComponent->GetFName());
			UEditorEngine::FCopyPropertiesForUnrelatedObjectsParams Params;
			Params.bDoDelta = false; // We need a deep copy of parameters here so the CDO values get copied as well
			UEditorEngine::CopyPropertiesForUnrelatedObjects(ActorComponent, NewSCSNode->ComponentTemplate, Params);

			// Clear the instance component flag
			NewSCSNode->ComponentTemplate->CreationMethod = EComponentCreationMethod::Native;

			if (AsSceneComponent != nullptr)
			{
				NewSceneComponents.Add(AsSceneComponent, NewSCSNode);
				if (!bInternalKeepMobility)
				{
					Cast<USceneComponent>(NewSCSNode->ComponentTemplate)->SetMobility(EComponentMobility::Movable);
				}
			}
			return NewSCSNode;
		}
	};

	AActor* Actor = nullptr;

	for (UActorComponent* ActorComponent : Components)
	{
		if (ActorComponent)
		{
			if (Actor)
			{
				check(Actor == ActorComponent->GetOwner());
			}
			else
			{
				Actor = ActorComponent->GetOwner();
				check(Actor);
			}

			if (!ActorComponent->GetClass()->HasMetaData(FBlueprintMetadata::MD_BlueprintSpawnableComponent))
			{
				continue;
			}

			USCS_Node* SCSNode = FAddComponentsToBlueprintImpl::MakeComponentCopy(ActorComponent, SCS, InstanceComponentToNodeMap, bKeepMobility);

			USceneComponent* SceneComponent = Cast<USceneComponent>(ActorComponent);
			// The easy part is non-scene component or the Root simply add it
			if (SceneComponent == nullptr)
			{
				SCS->AddNode(SCSNode);
			}
			else
			{
				if (ActorComponent == Actor->GetRootComponent())
				{
					if (OptionalNewRootNode != nullptr)
					{
						OptionalNewRootNode->AddChildNode(SCSNode);
					}
					else
					{
						SCS->AddNode(SCSNode);
					}
				}
				// If we're not attached to a blueprint component, add ourself to the root node or the SCS root component:
				else if (SceneComponent->GetAttachParent() == nullptr)
				{
					AddChildToSCSRootNodeLambda(SCSNode);
				}
				// If we're attached to a blueprint component look it up as the variable name is the component name
				else if (SceneComponent->GetAttachParent()->IsCreatedByConstructionScript())
				{
					USCS_Node* ParentSCSNode = nullptr;
					if (USCS_Node** ParentSCSNodePtr = InstanceComponentToNodeMap.Find(SceneComponent->GetAttachParent()))
					{
						ParentSCSNode = *ParentSCSNodePtr;
					}
					else if (Components.Contains(SceneComponent->GetAttachParent()))
					{
						// since you cannot rely on the order of the supplied  
						// Components array, we might be looking for a parent 
						// that hasn't been added yet
						ParentSCSNode = FAddComponentsToBlueprintImpl::MakeComponentCopy(SceneComponent->GetAttachParent(), SCS, InstanceComponentToNodeMap, bKeepMobility);
					}
					else
					{
						for (UBlueprint* ParentBlueprint : ParentBPStack)
						{
							ParentSCSNode = ParentBlueprint->SimpleConstructionScript->FindSCSNode(SceneComponent->GetAttachParent()->GetFName());
							if (ParentSCSNode)
							{
								break;
							}
						}
					}
					check(ParentSCSNode);

					if (ParentSCSNode->GetSCS() != SCS)
					{
						SCS->AddNode(SCSNode);
						SCSNode->SetParent(ParentSCSNode);
					}
					else
					{
						ParentSCSNode->AddChildNode(SCSNode);
					}
				}
				else if ((SceneComponent->GetAttachParent()->CreationMethod == EComponentCreationMethod::Native) && !bHarvesting)
				{
					// If we're attached to a component that will be native in the new blueprint
					SCS->AddNode(SCSNode);
					SCSNode->SetParent(SceneComponent->GetAttachParent());
				}
				else
				{
					// Otherwise check if we've already created the parents' new SCS node and attach to that or cache it off to do next pass
					USCS_Node** ParentSCSNode = InstanceComponentToNodeMap.Find(SceneComponent->GetAttachParent());
					if (ParentSCSNode)
					{
						(*ParentSCSNode)->AddChildNode(SCSNode);
					}
					else
					{
						SceneComponentsToAdd.Add(SceneComponent, SCSNode);
					}
				}
			}
		}
	}

	// Hook up the remaining components nodes that the parent's node was missing when it was processed
	for (auto ComponentIt = SceneComponentsToAdd.CreateConstIterator(); ComponentIt; ++ComponentIt)
	{
		// The AttachParent may or may not be BP-spawnable; if it's not, then we won't have created the parent node, so just add it as a child of the root node in that case.
		if (USCS_Node** ParentSCSNode = InstanceComponentToNodeMap.Find(ComponentIt.Key()->GetAttachParent()))
		{
			(*ParentSCSNode)->AddChildNode(ComponentIt.Value());
		}
		else
		{
			AddChildToSCSRootNodeLambda(ComponentIt.Value());
		}
	}
}

struct FResetSceneComponentAfterCopy
{
private:
	static void Reset(USceneComponent* Component)
	{
		Component->RelativeLocation = FVector::ZeroVector;
		Component->RelativeRotation = FRotator::ZeroRotator;

		// Clear out the attachment info after having copied the properties from the source actor
		Component->SetupAttachment(nullptr);
		FDirectAttachChildrenAccessor::Get(Component).Empty();

		// Ensure the light mass information is cleaned up
		Component->InvalidateLightingCache();
	}

	friend class FKismetEditorUtilities;
};

UBlueprint* FKismetEditorUtilities::CreateBlueprintFromActor(const FName BlueprintName, UObject* Outer, AActor* Actor, const bool bReplaceActor, bool bKeepMobility /*= false*/)
{
	UBlueprint* NewBlueprint = nullptr;

	if (Actor != nullptr)
	{
		if (Outer != nullptr)
		{
			// We don't have a factory, but we can still try to create a blueprint for this actor class
			NewBlueprint = FKismetEditorUtilities::CreateBlueprint( Actor->GetClass(), Outer, BlueprintName, EBlueprintType::BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), FName("CreateFromActor") );
		}

		if (NewBlueprint != nullptr)
		{
			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(NewBlueprint);

			// Mark the package dirty
			Outer->MarkPackageDirty();

			// If the source Actor has Instance Components we need to translate these in to SCS Nodes
			if (Actor->GetInstanceComponents().Num() > 0)
			{
				AddComponentsToBlueprint(NewBlueprint, Actor->GetInstanceComponents(), false,  (USCS_Node*)nullptr, bKeepMobility);
			}

			if (NewBlueprint->GeneratedClass != nullptr)
			{
				AActor* CDO = CastChecked<AActor>(NewBlueprint->GeneratedClass->GetDefaultObject());
				const auto CopyOptions = (EditorUtilities::ECopyOptions::Type)(EditorUtilities::ECopyOptions::OnlyCopyEditOrInterpProperties | EditorUtilities::ECopyOptions::PropagateChangesToArchetypeInstances);
				EditorUtilities::CopyActorProperties(Actor, CDO, CopyOptions);

				if (USceneComponent* DstSceneRoot = CDO->GetRootComponent())
				{
					FResetSceneComponentAfterCopy::Reset(DstSceneRoot);

					// Copy relative scale from source to target.
					if (USceneComponent* SrcSceneRoot = Actor->GetRootComponent())
					{
						DstSceneRoot->RelativeScale3D = SrcSceneRoot->RelativeScale3D;
					}
				}
			}

			FKismetEditorUtilities::CompileBlueprint(NewBlueprint);

			if (bReplaceActor)
			{
				TArray<AActor*> Actors;
				Actors.Add(Actor);

				FVector Location = Actor->GetActorLocation();
				FRotator Rotator = Actor->GetActorRotation();

				CreateBlueprintInstanceFromSelection(NewBlueprint, Actors, Location, Rotator);
			}
		}
	}

	if (NewBlueprint)
	{
		// Open the editor for the new blueprint
		FAssetEditorManager::Get().OpenEditorForAsset(NewBlueprint);
	}
	return NewBlueprint;
}

// This class cracks open the selected actors, harvests their components, and creates a new blueprint containing copies of them
class FCreateConstructionScriptFromSelectedActors
{
public:
	FCreateConstructionScriptFromSelectedActors()
		: Blueprint(nullptr)
		, SCS(nullptr)
	{
	}

	UBlueprint* Execute(FString Path, TArray<AActor*> SelectedActors, bool bReplaceInWorld)
	{
		if (SelectedActors.Num() > 0)
		{
			// Create a blueprint
			FString PackageName = Path;
			FString AssetName = FPackageName::GetLongPackageAssetName(Path);
			FString BasePath = PackageName + TEXT("/") + AssetName;

			// If no AssetName was found, generate a unique asset name.
			if(AssetName.Len() == 0)
			{
				BasePath = PackageName + TEXT("/") + LOCTEXT("BlueprintName_Default", "NewBlueprint").ToString();
				FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
				AssetToolsModule.Get().CreateUniqueAssetName(BasePath, TEXT(""), PackageName, AssetName);
			}

			UPackage* Package = CreatePackage(nullptr, *PackageName);
			Blueprint = FKismetEditorUtilities::CreateBlueprint(AActor::StaticClass(), Package, *AssetName, BPTYPE_Normal, UBlueprint::StaticClass(), UBlueprintGeneratedClass::StaticClass(), FName("HarvestFromActors"));

			check(Blueprint->SimpleConstructionScript != NULL);
			SCS = Blueprint->SimpleConstructionScript;

			// Create a common root if necessary
			USCS_Node* RootNodeOverride = nullptr;
			if ((SelectedActors.Num() == 1) && (SelectedActors[0]->GetRootComponent() != nullptr))
			{
				// We have a single actor that has a scene component, so no need to create a common root
			}
			else
			{
				// Add a new scene component to serve as the shared root node
				RootNodeOverride = SCS->CreateNode(USceneComponent::StaticClass(), TEXT("SharedRoot"));
				SCS->AddNode(RootNodeOverride);
			}

			// Harvest the components from each actor and clone them into the SCS
			for (const AActor* Actor : SelectedActors)
			{
				TArray<UActorComponent*> Components;
				Actor->GetComponents(Components);

				// Exclude any components created by other components
				for (int32 Index = Components.Num() - 1; Index >= 0; --Index)
				{
					UActorComponent* ComponentToConsider = Components[Index];
					if (ComponentToConsider->IsEditorOnly())
					{
						Components.RemoveAt(Index, 1, /*bAllowShrinking=*/ false);
					}
				}

				FKismetEditorUtilities::AddComponentsToBlueprint(Blueprint, Components, /*bHarvesting=*/ true, RootNodeOverride);
			}

			// Compute the average origin for all the actors, so it can be backed out when saving them in the blueprint
			FTransform NewActorTransform = FTransform::Identity;
			{
				// Find average location of all selected actors
				FVector AverageLocation = FVector::ZeroVector;
				for (const AActor* Actor : SelectedActors)
				{
					if (USceneComponent* RootComponent = Actor->GetRootComponent())
					{
						AverageLocation += Actor->GetActorLocation();
					}
				}
				AverageLocation /= (float)SelectedActors.Num();

				// Spawn the new BP at that location
				NewActorTransform.SetTranslation(AverageLocation);
			}

			// Reposition all of the children of the root node to recenter them around the new pivot
			for (USCS_Node* TopLevelNode : SCS->GetRootNodes())
			{
				if (USceneComponent* TestRoot = Cast<USceneComponent>(TopLevelNode->ComponentTemplate))
				{
					for (USCS_Node* ChildNode : TopLevelNode->GetChildNodes())
					{
						if (USceneComponent* ChildComponent = Cast<USceneComponent>(ChildNode->ComponentTemplate))
						{
							const FTransform OldChildToWorld(ChildComponent->RelativeRotation, ChildComponent->RelativeLocation);
							const FTransform NewRelativeTransform = OldChildToWorld.GetRelativeTransform(NewActorTransform);
							ChildComponent->RelativeLocation = NewRelativeTransform.GetLocation();
							ChildComponent->RelativeRotation = NewRelativeTransform.GetRotation().Rotator();
						}
					}
				}
			}

			// Regenerate skeleton class as components have been added since initial generation
			FKismetEditorUtilities::GenerateBlueprintSkeleton(Blueprint, /*bForceRegeneration=*/ true); 
			
			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(Blueprint);

			// Mark the package dirty
			Package->MarkPackageDirty();

			// Delete the old actors and create a new instance in the map
			if (bReplaceInWorld)
			{
				FVector Location = NewActorTransform.GetLocation();
				FRotator Rotator = NewActorTransform.Rotator();

				FKismetEditorUtilities::CreateBlueprintInstanceFromSelection(Blueprint, SelectedActors, Location, Rotator);
			}

			// Open the editor for the new blueprint
			FAssetEditorManager::Get().OpenEditorForAsset(Blueprint);

			return Blueprint;
		}
		return nullptr;
	}

protected:
	UBlueprint* Blueprint;
	USimpleConstructionScript* SCS;
};


UBlueprint* FKismetEditorUtilities::HarvestBlueprintFromActors(const FString& Path, const TArray<AActor*>& Actors, bool bReplaceInWorld)
{
	FCreateConstructionScriptFromSelectedActors Creator;
	return Creator.Execute(Path, Actors, bReplaceInWorld);
}

AActor* FKismetEditorUtilities::CreateBlueprintInstanceFromSelection(UBlueprint* Blueprint, TArray<AActor*>& SelectedActors, const FVector& Location, const FRotator& Rotator)
{
	check (SelectedActors.Num() > 0 );

	// Create transaction to cover conversion
	const FScopedTransaction Transaction( NSLOCTEXT("EditorEngine", "ConvertActorToBlueprint", "Replace Actor(s) with blueprint") );

	// Assume all selected actors are in the same world
	UWorld* World = SelectedActors[0]->GetWorld();

	GEditor->GetSelectedActors()->Modify();

	for(auto It(SelectedActors.CreateIterator());It;++It)
	{
		if (AActor* Actor = *It)
		{
			// Remove from active selection in editor
			GEditor->SelectActor(Actor, /*bSelected=*/ false, /*bNotify=*/ false);

			GEditor->Layers->DisassociateActorFromLayers(Actor);
			World->EditorDestroyActor(Actor, false);
		}
	}

	AActor* NewActor = World->SpawnActor(Blueprint->GeneratedClass, &Location, &Rotator);
	GEditor->Layers->InitializeNewActorLayers(NewActor);

	// Quietly ensure that no components are selected
	USelection* ComponentSelection = GEditor->GetSelectedComponents();
	ComponentSelection->BeginBatchSelectOperation();
	ComponentSelection->DeselectAll();
	ComponentSelection->EndBatchSelectOperation(false);

	// Update selection to new actor
	GEditor->SelectActor( NewActor, /*bSelected=*/ true, /*bNotify=*/ true );

	return NewActor;
}

UBlueprint* FKismetEditorUtilities::CreateBlueprintFromClass(FText InWindowTitle, UClass* InParentClass, FString NewNameSuggestion)
{
	check(FKismetEditorUtilities::CanCreateBlueprintOfClass(InParentClass));

	// Pre-generate a unique asset name to fill out the path picker dialog with.
	if (NewNameSuggestion.Len() == 0)
	{
		NewNameSuggestion = TEXT("NewBlueprint");
	}

	UClass* BlueprintClass = nullptr;
	UClass* BlueprintGeneratedClass = nullptr;

	IKismetCompilerInterface& KismetCompilerModule = FModuleManager::LoadModuleChecked<IKismetCompilerInterface>("KismetCompiler");
	KismetCompilerModule.GetBlueprintTypesForClass(InParentClass, BlueprintClass, BlueprintGeneratedClass);

	FString PackageName = FString(TEXT("/Game/Blueprints/")) + NewNameSuggestion;
	FString Name;
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");
	AssetToolsModule.Get().CreateUniqueAssetName(PackageName, TEXT(""), PackageName, Name);

	TSharedPtr<SDlgPickAssetPath> PickAssetPathWidget =
		SNew(SDlgPickAssetPath)
		.Title(InWindowTitle)
		.DefaultAssetPath(FText::FromString(PackageName));

	if (EAppReturnType::Ok == PickAssetPathWidget->ShowModal())
	{
		// Get the full name of where we want to create the physics asset.
		FString UserPackageName = PickAssetPathWidget->GetFullAssetPath().ToString();
		FName BPName(*FPackageName::GetLongPackageAssetName(UserPackageName));

		// Check if the user inputed a valid asset name, if they did not, give it the generated default name
		if (BPName == NAME_None)
		{
			// Use the defaults that were already generated.
			UserPackageName = PackageName;
			BPName = *Name;
		}

		// Then find/create it.
		UPackage* Package = CreatePackage(NULL, *UserPackageName);
		check(Package);

		// Create and init a new Blueprint
		UBlueprint* Blueprint = FKismetEditorUtilities::CreateBlueprint(InParentClass, Package, BPName, BPTYPE_Normal, BlueprintClass, BlueprintGeneratedClass, FName("LevelEditorActions"));
		if (Blueprint)
		{
			// Notify the asset registry
			FAssetRegistryModule::AssetCreated(Blueprint);

			// Mark the package dirty...
			Package->MarkPackageDirty();

			return Blueprint;
		}
	}
	return NULL;
}

UBlueprint* FKismetEditorUtilities::CreateBlueprintUsingAsset(UObject* Asset, bool bOpenInEditor)
{
	// Check we have an asset.
	if(Asset == NULL)
	{
		return NULL;
	}

	// Check we can create a component from this asset
	TSubclassOf<UActorComponent> ComponentClass = FComponentAssetBrokerage::GetPrimaryComponentForAsset(Asset->GetClass());
	if(ComponentClass != NULL)
	{
		// Create a new empty Actor BP
		UBlueprint* NewBP = CreateBlueprintFromClass(LOCTEXT("CreateBlueprint", "Create Blueprint"), AActor::StaticClass(), Asset->GetName());
		if(NewBP != NULL)
		{
			// Create a new SCS node
			check(NewBP->SimpleConstructionScript != NULL);
			USCS_Node* NewNode = NewBP->SimpleConstructionScript->CreateNode(ComponentClass);

			// Assign the asset to the template
			FComponentAssetBrokerage::AssignAssetToComponent(NewNode->ComponentTemplate, Asset);

			// Add node to the SCS
			NewBP->SimpleConstructionScript->AddNode(NewNode);

			// Recompile skeleton because of the new component we added (and 
			// broadcast the change to those that care, like the BP node database)
			FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(NewBP);

			// Open in BP editor if desired
			if(bOpenInEditor)
			{
				FAssetEditorManager::Get().OpenEditorForAsset(NewBP);
			}
		}

		return NewBP;
	}

	return NULL;
}

void FKismetEditorUtilities::AddToSelection(const class UEdGraph* Graph, UEdGraphNode* InNode)
{
	TSharedPtr<class IBlueprintEditor> BlueprintEditor = GetIBlueprintEditorForObject(Graph, false);
	if (BlueprintEditor.IsValid())
	{
		BlueprintEditor->AddToSelection(InNode);
	}
}

TSharedPtr<class IBlueprintEditor> FKismetEditorUtilities::GetIBlueprintEditorForObject( const UObject* ObjectToFocusOn, bool bOpenEditor )
{
	check(ObjectToFocusOn);

	// Find the associated blueprint
	UBlueprint* TargetBP = Cast<UBlueprint>(const_cast<UObject*>(ObjectToFocusOn));
	if (TargetBP == NULL)
	{
		for (UObject* TestOuter = ObjectToFocusOn->GetOuter(); TestOuter; TestOuter = TestOuter->GetOuter())
		{
			TargetBP = Cast<UBlueprint>(TestOuter);

			if(TargetBP == nullptr)
			{
				if(UBlueprintGeneratedClass* BPGeneratedClass = Cast<UBlueprintGeneratedClass>(TestOuter))
				{
					TargetBP = Cast<UBlueprint>(BPGeneratedClass->ClassGeneratedBy);
				}
			}

			if (TargetBP != NULL)
			{
				break;
			}
		}
	}

	TSharedPtr<IBlueprintEditor> BlueprintEditor;
	if (TargetBP != NULL)
	{
		if (bOpenEditor)
		{
			// @todo toolkit major: Needs world-centric support
			FAssetEditorManager::Get().OpenEditorForAsset(TargetBP);
		}

		TSharedPtr< IToolkit > FoundAssetEditor = FToolkitManager::Get().FindEditorForAsset(TargetBP);
		// If we found a BlueprintEditor
		if (FoundAssetEditor.IsValid() && FoundAssetEditor->IsBlueprintEditor())
		{
			BlueprintEditor = StaticCastSharedPtr<IBlueprintEditor>(FoundAssetEditor);
		}
	}
	return BlueprintEditor;
}

void FKismetEditorUtilities::PasteNodesHere( class UEdGraph* Graph, const FVector2D& Location )
{
	TSharedPtr<class IBlueprintEditor> Kismet = GetIBlueprintEditorForObject(Graph,false);
	if(Kismet.IsValid())
	{
		Kismet->PasteNodesHere(Graph,Location);
	}
}

bool FKismetEditorUtilities::CanPasteNodes( const class UEdGraph* Graph )
{
	bool bCanPaste = false;
	TSharedPtr<class IBlueprintEditor> Kismet = GetIBlueprintEditorForObject(Graph,false);
	if (Kismet.IsValid())
	{
		bCanPaste = Kismet->CanPasteNodes();
	}
	return bCanPaste;
}

bool FKismetEditorUtilities::GetBoundsForSelectedNodes(const class UBlueprint* Blueprint,  class FSlateRect& Rect, float Padding)
{
	bool bCanPaste = false;
	TSharedPtr<class IBlueprintEditor> Kismet = GetIBlueprintEditorForObject(Blueprint, false);
	if (Kismet.IsValid())
	{
		bCanPaste = Kismet->GetBoundsForSelectedNodes(Rect, Padding);
	}
	return bCanPaste;
}

int32 FKismetEditorUtilities::GetNumberOfSelectedNodes(const class UBlueprint* Blueprint)
{
	int32 NumberNodesSelected = 0;
	TSharedPtr<class IBlueprintEditor> Kismet = GetIBlueprintEditorForObject(Blueprint, false);
	if (Kismet.IsValid())
	{
		NumberNodesSelected = Kismet->GetNumberOfSelectedNodes();
	}
	return NumberNodesSelected;
}

/** Open a Kismet window, focusing on the specified object (either a pin, a node, or a graph).  Prefers existing windows, but will open a new application if required. */
void FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(const UObject* ObjectToFocusOn, bool bRequestRename)
{
	TSharedPtr<IBlueprintEditor> BlueprintEditor = GetIBlueprintEditorForObject(ObjectToFocusOn, true);
	if (BlueprintEditor.IsValid())
	{
		BlueprintEditor->FocusWindow();
		BlueprintEditor->JumpToHyperlink(ObjectToFocusOn, bRequestRename);
	}
}

void FKismetEditorUtilities::BringKismetToFocusAttentionOnPin(const UEdGraphPin* PinToFocusOn )
{
	TSharedPtr<IBlueprintEditor> BlueprintEditor = GetIBlueprintEditorForObject(PinToFocusOn->GetOwningNode(), true);
	if (BlueprintEditor.IsValid())
	{
		BlueprintEditor->FocusWindow();
		BlueprintEditor->JumpToPin(PinToFocusOn);
	}
}

void FKismetEditorUtilities::ShowActorReferencesInLevelScript(const AActor* Actor)
{
	if (Actor != NULL)
	{
		ULevelScriptBlueprint* LSB = Actor->GetLevel()->GetLevelScriptBlueprint();
		if (LSB != NULL)
		{
			// @todo toolkit major: Needs world-centric support.  Other spots, too?
			FAssetEditorManager::Get().OpenEditorForAsset(LSB);
			TSharedPtr<IToolkit> FoundAssetEditor = FToolkitManager::Get().FindEditorForAsset(LSB);
			if (FoundAssetEditor.IsValid())
			{
				TSharedRef<IBlueprintEditor> BlueprintEditor = StaticCastSharedRef<IBlueprintEditor>(FoundAssetEditor.ToSharedRef());
				BlueprintEditor->FocusWindow();

				const bool bSetFindWithinBlueprint = true;
				const bool bSelectFirstResult = true;
				BlueprintEditor->SummonSearchUI(bSetFindWithinBlueprint, Actor->GetActorLabel(), bSelectFirstResult);
			}
		}

	}
}

// Upgrade any cosmetically stale information in a blueprint (done when edited instead of PostLoad to make certain operations easier)
void FKismetEditorUtilities::UpgradeCosmeticallyStaleBlueprint(UBlueprint* Blueprint)
{
	// Rename the ubergraph page 'StateGraph' to be named 'EventGraph' if possible
	if (FBlueprintEditorUtils::DoesSupportEventGraphs(Blueprint))
	{
		const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

		UEdGraph* OldStateGraph = FindObject<UEdGraph>(Blueprint, TEXT("StateGraph"));
		UObject* CollidingObject = FindObject<UObject>(Blueprint, *(K2Schema->GN_EventGraph.ToString()));

		if ((OldStateGraph != NULL) && (CollidingObject == NULL))
		{
			check(!OldStateGraph->HasAnyFlags(RF_Public));
			OldStateGraph->Rename(*(K2Schema->GN_EventGraph.ToString()), OldStateGraph->GetOuter(), REN_DoNotDirty | REN_ForceNoResetLoaders);
			Blueprint->Status = BS_Dirty;
		}
	}
}

void FKismetEditorUtilities::CreateNewBoundEventForActor(AActor* Actor, FName EventName)
{
	if ((Actor != nullptr) && (EventName != NAME_None))
	{
		// First, find the property we want to bind to
		if (UMulticastDelegateProperty* DelegateProperty = FindField<UMulticastDelegateProperty>(Actor->GetClass(), EventName))
		{
			// Get the correct level script blueprint
			if (ULevelScriptBlueprint* LSB = Actor->GetLevel()->GetLevelScriptBlueprint())
			{
				if (UEdGraph* TargetGraph = LSB->GetLastEditedUberGraph())
				{
					// Figure out a decent place to stick the node
					const FVector2D NewNodePos = TargetGraph->GetGoodPlaceForNewNode();

					// Create a new event node
					UK2Node_ActorBoundEvent* EventNodeTemplate = NewObject<UK2Node_ActorBoundEvent>();
					EventNodeTemplate->InitializeActorBoundEventParams(Actor, DelegateProperty);

					// Finally, bring up kismet and jump to the new node
					if (UK2Node_ActorBoundEvent* EventNode = FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<UK2Node_ActorBoundEvent>(TargetGraph, EventNodeTemplate, NewNodePos))
					{
						BringKismetToFocusAttentionOnObject(EventNode);
					}
				}
			}
		}
	}
}

void FKismetEditorUtilities::CreateNewBoundEventForComponent(UObject* Component, FName EventName, UBlueprint* Blueprint, UObjectProperty* ComponentProperty)
{
	if ( Component != nullptr )
	{
		CreateNewBoundEventForClass(Component->GetClass(), EventName, Blueprint, ComponentProperty);
	}
}

void FKismetEditorUtilities::CreateNewBoundEventForClass(UClass* Class, FName EventName, UBlueprint* Blueprint, UObjectProperty* ComponentProperty)
{
	if ( ( Class != nullptr ) && ( EventName != NAME_None ) && ( Blueprint != nullptr ) && ( ComponentProperty != nullptr ) )
	{
		// First, find the property we want to bind to
		UMulticastDelegateProperty* DelegateProperty = FindField<UMulticastDelegateProperty>(Class, EventName);
		if ( DelegateProperty != nullptr )
		{
			UEdGraph* TargetGraph = Blueprint->GetLastEditedUberGraph();

			if ( TargetGraph != nullptr )
			{
				// Figure out a decent place to stick the node
				const FVector2D NewNodePos = TargetGraph->GetGoodPlaceForNewNode();

				// Create a new event node
				UK2Node_ComponentBoundEvent* EventNodeTemplate = NewObject<UK2Node_ComponentBoundEvent>();
				EventNodeTemplate->InitializeComponentBoundEventParams(ComponentProperty, DelegateProperty);

				UK2Node_ComponentBoundEvent* EventNode = FEdGraphSchemaAction_K2NewNode::SpawnNodeFromTemplate<UK2Node_ComponentBoundEvent>(TargetGraph, EventNodeTemplate, NewNodePos);

				// Finally, bring up kismet and jump to the new node
				if ( EventNode != nullptr )
				{
					BringKismetToFocusAttentionOnObject(EventNode);
				}
			}
		}
	}
}

const UK2Node_ActorBoundEvent* FKismetEditorUtilities::FindBoundEventForActor(AActor const* Actor, FName EventName)
{
	const UK2Node_ActorBoundEvent* Node = NULL;
	if(Actor != NULL && EventName != NAME_None)
	{
		ULevelScriptBlueprint* LSB = Actor->GetLevel()->GetLevelScriptBlueprint(true);
		if(LSB != NULL)
		{
			TArray<UK2Node_ActorBoundEvent*> EventNodes;
			FBlueprintEditorUtils::GetAllNodesOfClass(LSB, EventNodes);
			for(int32 i=0; i<EventNodes.Num(); i++)
			{
				UK2Node_ActorBoundEvent* BoundEvent = EventNodes[i];
				if(BoundEvent->EventOwner == Actor && BoundEvent->DelegatePropertyName == EventName)
				{
					Node = BoundEvent;
					break;
				}
			}
		}
	}
	return Node;
}

const UK2Node_ComponentBoundEvent* FKismetEditorUtilities::FindBoundEventForComponent(const UBlueprint* Blueprint, FName EventName, FName PropertyName)
{
	const UK2Node_ComponentBoundEvent* Node = NULL;
	if ( Blueprint && EventName != NAME_None && PropertyName != NAME_None )
	{
		TArray<UK2Node_ComponentBoundEvent*> EventNodes;
		FBlueprintEditorUtils::GetAllNodesOfClass(Blueprint, EventNodes);
		for ( auto NodeIter = EventNodes.CreateIterator(); NodeIter; ++NodeIter )
		{
			UK2Node_ComponentBoundEvent* BoundEvent = *NodeIter;
			if ( ( BoundEvent->ComponentPropertyName == PropertyName ) && ( BoundEvent->DelegatePropertyName == EventName ) )
			{
				Node = *NodeIter;
				break;
			}
		}
	}
	return Node;
}

bool FKismetEditorUtilities::IsClassABlueprintInterface(const UClass* Class)
{
	if (Class->HasAnyClassFlags(CLASS_Interface) && !Class->HasAnyClassFlags(CLASS_NewerVersionExists))
	{
		return true;
	}
	return false;
}



bool FKismetEditorUtilities::CanBlueprintImplementInterface(UBlueprint const* Blueprint, UClass const* Class)
{
	bool bCanImplementInterface = false;

	// if the class is an actual implementable interface
	if (IsClassABlueprintInterface(Class) && !Class->HasMetaData(FBlueprintMetadata::MD_CannotImplementInterfaceInBlueprint))
	{
		bCanImplementInterface = true;

		UClass const* const ParentClass = Blueprint->ParentClass;
		// see if the parent class has any prohibited interfaces
		if ((ParentClass != NULL) && ParentClass->HasMetaData(FBlueprintMetadata::MD_ProhibitedInterfaces))
		{
			FString const& ProhibitedList = Blueprint->ParentClass->GetMetaData(FBlueprintMetadata::MD_ProhibitedInterfaces);
			
			TArray<FString> ProhibitedInterfaceNames;
			ProhibitedList.ParseIntoArray(ProhibitedInterfaceNames, TEXT(","), true);

			FString const& InterfaceName = Class->GetName();
			// loop over all the prohibited interfaces
			for (int32 ExclusionIndex = 0; ExclusionIndex < ProhibitedInterfaceNames.Num(); ++ExclusionIndex)
			{
				ProhibitedInterfaceNames[ExclusionIndex].TrimStartInline();
				FString const& Exclusion = ProhibitedInterfaceNames[ExclusionIndex];
				// if this interface matches one of the prohibited ones
				if (InterfaceName == Exclusion) 
				{
					bCanImplementInterface = false;
					break;
				}
			}
		}
	}

	return bCanImplementInterface;
}

bool FKismetEditorUtilities::IsClassABlueprintSkeleton(const UClass* Class)
{
	// Find generating blueprint for a class
	UBlueprint* GeneratingBP = Cast<UBlueprint>(Class->ClassGeneratedBy);
	if( GeneratingBP && GeneratingBP->SkeletonGeneratedClass )
	{
		return (Class == GeneratingBP->SkeletonGeneratedClass) && (GeneratingBP->SkeletonGeneratedClass != GeneratingBP->GeneratedClass);
	}
	return Class->HasAnyFlags(RF_Transient) && Class->HasAnyClassFlags(CLASS_CompiledFromBlueprint);
}

bool FKismetEditorUtilities::IsClassABlueprintMacroLibrary(const UClass* Class)
{
	// Find generating blueprint for a class
	UBlueprint* GeneratingBP = Cast<UBlueprint>(Class->ClassGeneratedBy);
	return (GeneratingBP && GeneratingBP->BlueprintType == BPTYPE_MacroLibrary);
}

/** Run over the components references, and then NULL any that fall outside this blueprint's scope (e.g. components brought over after reparenting from another class, which are now in the transient package) */
void FKismetEditorUtilities::StripExternalComponents(class UBlueprint* Blueprint)
{
	FArchiveInvalidateTransientRefs InvalidateRefsAr;
	
	UClass* SkeletonGeneratedClass = Blueprint->SkeletonGeneratedClass;
	if (SkeletonGeneratedClass)
	{
		UObject* SkeletonCDO = SkeletonGeneratedClass->GetDefaultObject();

		SkeletonCDO->Serialize(InvalidateRefsAr);
	}

	UClass* GeneratedClass = Blueprint->GeneratedClass;
	UObject* GeneratedCDO = GeneratedClass->GetDefaultObject();

	GeneratedCDO->Serialize(InvalidateRefsAr);
}

bool FKismetEditorUtilities::IsTrackedBlueprintParent(const UClass* ParentClass)
{
	if (ParentClass->ClassGeneratedBy == NULL)
	{
		// Always track native parent classes
		return true;
	}

	UBlueprint* ParentBlueprint = Cast<UBlueprint>(ParentClass->ClassGeneratedBy);

	// Cache the list of allowed blueprint names the first time it is requested
	if (TrackedBlueprintParentList.Num() == 0)
	{
		GConfig->GetArray(TEXT("Kismet"), TEXT("TrackedBlueprintParents"), /*out*/ TrackedBlueprintParentList, GEngineIni);
	}

	for (auto TrackedBlueprintIter = TrackedBlueprintParentList.CreateConstIterator(); TrackedBlueprintIter; ++TrackedBlueprintIter)
	{
		if (ParentBlueprint->GetName().EndsWith(*TrackedBlueprintIter))
		{
			return true;
		}
	}
	return false;
}

bool FKismetEditorUtilities::IsActorValidForLevelScript(const AActor* Actor)
{
	return Actor && !FActorEditorUtils::IsABuilderBrush(Actor);
}

bool FKismetEditorUtilities::AnyBoundLevelScriptEventForActor(AActor* Actor, bool bCouldAddAny)
{
	if (IsActorValidForLevelScript(Actor))
	{
		for (TFieldIterator<UMulticastDelegateProperty> PropertyIt(Actor->GetClass(), EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			UProperty* Property = *PropertyIt;
			// Check for multicast delegates that we can safely assign
			if (!Property->HasAnyPropertyFlags(CPF_Parm) && Property->HasAllPropertyFlags(CPF_BlueprintAssignable))
			{
				const FName EventName = Property->GetFName();
				const UK2Node_ActorBoundEvent* ExistingNode = FKismetEditorUtilities::FindBoundEventForActor(Actor, EventName);
				if ((NULL != ExistingNode) != bCouldAddAny)
				{
					return true;
				}
			}
		}
	}
	return false;
}

void FKismetEditorUtilities::AddLevelScriptEventOptionsForActor(class FMenuBuilder& MenuBuilder, TWeakObjectPtr<AActor> ActorPtr, bool bExistingEvents, bool bNewEvents, bool bOnlyEventName)
{
	struct FCreateEventForActorHelper
	{
		static void CreateEventForActor(TWeakObjectPtr<AActor> InActorPtr, FName EventName)
		{
			if (!GEditor->bIsSimulatingInEditor && GEditor->PlayWorld == NULL)
			{
				AActor* Actor = InActorPtr.Get();
				if (Actor != NULL && EventName != NAME_None)
				{
					const UK2Node_ActorBoundEvent* ExistingNode = FKismetEditorUtilities::FindBoundEventForActor(Actor, EventName);
					if (ExistingNode != NULL)
					{
						FKismetEditorUtilities::BringKismetToFocusAttentionOnObject(ExistingNode);
					}
					else
					{
						FKismetEditorUtilities::CreateNewBoundEventForActor(Actor, EventName);
					}
				}
			}
		}
	};

	AActor* Actor = ActorPtr.Get();
	if (IsActorValidForLevelScript(Actor))
	{
		// Struct to store event properties by category
		struct FEventCategory
		{
			FString CategoryName;
			TArray<UProperty*> EventProperties;
		};
		// ARray of event properties by category
		TArray<FEventCategory> CategorizedEvents;

		// Find all events we can assign
		for (TFieldIterator<UMulticastDelegateProperty> PropertyIt(Actor->GetClass(), EFieldIteratorFlags::IncludeSuper); PropertyIt; ++PropertyIt)
		{
			UProperty* Property = *PropertyIt;
			// Check for multicast delegates that we can safely assign
			if (!Property->HasAnyPropertyFlags(CPF_Parm) && Property->HasAllPropertyFlags(CPF_BlueprintAssignable))
			{
				// Get category for this property
				FString PropertyCategory = FObjectEditorUtils::GetCategory(Property);
				// See if we already have a list for this
				bool bFound = false;
				for (FEventCategory& Category : CategorizedEvents)
				{
					if(Category.CategoryName == PropertyCategory)
					{
						Category.EventProperties.Add(Property);
						bFound = true;
					}
				}
				// If not, create one
				if(!bFound)
				{
					FEventCategory NewCategory;
					NewCategory.CategoryName = PropertyCategory;
					NewCategory.EventProperties.Add(Property);
					CategorizedEvents.Add(NewCategory);
				}
			}
		}

		// Now build the menu
		for(FEventCategory& Category : CategorizedEvents)
		{
			MenuBuilder.BeginSection(NAME_None, FText::FromString(Category.CategoryName));

			for(UProperty* Property : Category.EventProperties)
			{
				const FName EventName = Property->GetFName();
				const UK2Node_ActorBoundEvent* ExistingNode = FKismetEditorUtilities::FindBoundEventForActor(Actor, EventName);

				if ((!ExistingNode && !bNewEvents) || (ExistingNode && !bExistingEvents))
				{
					continue;
				}

				FText EntryText;
				if (bOnlyEventName)
				{
					EntryText = FText::FromName(EventName);
				}
				else
				{
					FFormatNamedArguments Args;
					Args.Add(TEXT("EventName"), FText::FromName(EventName));

					if (NULL == ExistingNode)
					{
						EntryText = FText::Format(LOCTEXT("AddEvent_ToolTip", "Add {EventName}"), Args);
					}
					else
					{
						EntryText = FText::Format(LOCTEXT("ViewEvent_ToolTip", "View {EventName}"), Args);
					}
				}

				// create menu entry
				MenuBuilder.AddMenuEntry(
					EntryText,
					Property->GetToolTipText(),
					FSlateIcon(),
					FUIAction(FExecuteAction::CreateStatic(&FCreateEventForActorHelper::CreateEventForActor, ActorPtr, EventName))
					);
			}

			MenuBuilder.EndSection();
		}
	}
}

void FKismetEditorUtilities::GetInformationOnMacro(UEdGraph* MacroGraph, /*out*/ UK2Node_Tunnel*& EntryNode, /*out*/ UK2Node_Tunnel*& ExitNode, bool& bIsMacroPure)
{
	check(MacroGraph);

	// Look at the graph for the entry & exit nodes
	TArray<UK2Node_Tunnel*> TunnelNodes;
	MacroGraph->GetNodesOfClass(TunnelNodes);

	for (int32 i = 0; i < TunnelNodes.Num(); i++)
	{
		UK2Node_Tunnel* Node = TunnelNodes[i];

		// Composite nodes should never be considered for function entry / exit, since we're searching for a graph's terminals
		if (Node->IsEditable() && !Node->IsA(UK2Node_Composite::StaticClass()))
		{
			if (Node->bCanHaveOutputs)
			{
				check(!EntryNode);
				EntryNode = Node;
			}
			else if (Node->bCanHaveInputs)
			{
				check(!ExitNode);
				ExitNode = Node;
			}
		}
	}

	const UEdGraphSchema_K2* K2Schema = GetDefault<UEdGraphSchema_K2>();

	// Determine the macro's purity
	//@TODO: May want to check what is *inside* a macro too, to determine it's relative purity
	bIsMacroPure = true;

	if (EntryNode != NULL)
	{
		for (int32 PinIndex = 0; PinIndex < EntryNode->Pins.Num(); ++PinIndex)
		{
			if (K2Schema->IsExecPin(*(EntryNode->Pins[PinIndex])))
			{
				bIsMacroPure = false;
				break;
			}
		}
	}

	if (bIsMacroPure && (ExitNode != NULL))
	{
		for (int32 PinIndex = 0; PinIndex < ExitNode->Pins.Num(); ++PinIndex)
		{
			if (K2Schema->IsExecPin(*(ExitNode->Pins[PinIndex])))
			{
				bIsMacroPure = false;
				break;
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE 
