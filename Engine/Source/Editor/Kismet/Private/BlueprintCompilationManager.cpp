// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintCompilationManager.h"

#include "BlueprintEditorUtils.h"
#include "BlueprintEditorSettings.h"
#include "Blueprint/BlueprintSupport.h"
#include "CompilerResultsLog.h"
#include "Components/TimelineComponent.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/TimelineTemplate.h"
#include "FileHelpers.h"
#include "FindInBlueprintManager.h"
#include "IMessageLogListing.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/KismetReinstanceUtilities.h"
#include "KismetCompiler.h"
#include "ProfilingDebugging/ScopedTimers.h"
#include "Serialization/ArchiveHasReferences.h"
#include "Serialization/ArchiveReplaceOrClearExternalReferences.h"
#include "Settings/EditorProjectSettings.h"
#include "TickableEditorObject.h"
#include "UObject/MetaData.h"
#include "UObject/ReferenceChainSearch.h"
#include "UObject/UObjectHash.h"
#include "WidgetBlueprint.h"
#include "Kismet2/KismetDebugUtilities.h"

#define LOCTEXT_NAMESPACE "BlueprintCompilationManager"

/*
	BLUEPRINT COMPILATION MANAGER IMPLEMENTATION NOTES

	INPUTS: UBlueprint, UEdGraph, UEdGraphNode, UEdGraphPin, references to UClass, UProperties
	INTERMEDIATES: Cloned Graph, Nodes, Pins
	OUPUTS: UClass, UProperties

	The blueprint compilation manager addresses shortcomings of compilation 
	behavior (performance, correctness) that occur when compiling blueprints 
	that are inter-dependent. If you are using blueprints and there are no dependencies
	between blueprint compilation outputs and inputs, then this code is completely
	unnecessary and you can directly interface with FKismetCompilerContext and its
	derivatives.

	In order to handle compilation correctly the manager splits compilation into
	the following stages (implemented below in FlushCompilationQueueImpl):

	STAGE I: GATHER
	STAGE II: FILTER
	STAGE III: SORT
	STAGE IV: SET TEMPORARY BLUEPRINT FLAGS
	STAGE V: VALIDATE
	STAGE VI: PURGE (LOAD ONLY)
	STAGE VII: DISCARD SKELETON CDO
	STAGE VIII: RECOMPILE SKELETON
	STAGE IX: RECONSTRUCT NODES, REPLACE DEPRECATED NODES (LOAD ONLY)
	STAGE X: CREATE REINSTANCER (DISCARD 'OLD' CLASS)
	STAGE XI: CREATE UPDATED CLASS HIERARCHY
	STAGE XII: COMPILE CLASS LAYOUT
	STAGE XIII: COMPILE CLASS FUNCTIONS
	STAGE XIV: REINSTANCE
	STAGE XV: CLEAR TEMPORARY FLAGS

	The code that implements these stages are labeled below. At some later point a final
	reinstancing operation will occur, unless the client is using CompileSynchronously, 
	in which case the expensive object graph find and replace will occur immediately
*/

// Debugging switches:
#define VERIFY_NO_STALE_CLASS_REFERENCES 0
#define VERIFY_NO_BAD_SKELETON_REFERENCES 0

struct FReinstancingJob;

struct FBlueprintCompilationManagerImpl : public FGCObject
{
	FBlueprintCompilationManagerImpl();
	virtual ~FBlueprintCompilationManagerImpl();

	// FGCObject:
	virtual void AddReferencedObjects(FReferenceCollector& Collector);

	void QueueForCompilation(const FBPCompileRequest& CompileJob);
	void CompileSynchronouslyImpl(const FBPCompileRequest& Request);
	void FlushCompilationQueueImpl(TArray<UObject*>* ObjLoaded, bool bSuppressBroadcastCompiled, TArray<UBlueprint*>* BlueprintsCompiled);
	void FlushReinstancingQueueImpl();
	bool HasBlueprintsToCompile() const;
	bool IsGeneratedClassLayoutReady() const;
	void GetDefaultValue(const UClass* ForClass, const UProperty* Property, FString& OutDefaultValueAsString) const;
	static void ReinstanceBatch(TArray<FReinstancingJob>& Reinstancers, TMap< UClass*, UClass* >& InOutOldToNewClassMap, TArray<UObject*>* ObjLoaded);
	static UClass* FastGenerateSkeletonClass(UBlueprint* BP, FKismetCompilerContext& CompilerContext);
	static bool IsQueuedForCompilation(UBlueprint* BP);
	
	// Declaration of archive to fix up bytecode references of blueprints that are actively compiled:
	class FFixupBytecodeReferences : public FArchiveUObject
	{
	public:
		FFixupBytecodeReferences(UObject* InObject);

	private:
		virtual FArchive& operator<<( UObject*& Obj ) override;
	};

	// Queued requests to be processed in the next FlushCompilationQueueImpl call:
	TArray<FBPCompileRequest> QueuedRequests;
	
	// Data stored for reinstancing, which finishes much later than compilation,
	// populated by FlushCompilationQueueImpl, cleared by FlushReinstancingQueueImpl:
	TMap<UClass*, UClass*> ClassesToReinstance;
	
	// Map to old default values, useful for providing access to this data throughout
	// the compilation process:
	TMap<UBlueprint*, UObject*> OldCDOs;
	
	// Blueprints that should be saved after the compilation pass is complete:
	TArray<UBlueprint*> CompiledBlueprintsToSave;

	// State stored so that we can check what stage of compilation we're in:
	bool bGeneratedClassLayoutReady;
};

// free function that we use to cross a module boundary (from CoreUObject to here)
void FlushReinstancingQueueImplWrapper();
void MoveSkelCDOAside(UClass* Class, TMap<UClass*, UClass*>& OldToNewMap);

FBlueprintCompilationManagerImpl::FBlueprintCompilationManagerImpl()
{
	FBlueprintSupport::SetFlushReinstancingQueueFPtr(&FlushReinstancingQueueImplWrapper);
	bGeneratedClassLayoutReady = true;
}

FBlueprintCompilationManagerImpl::~FBlueprintCompilationManagerImpl() 
{ 
	FBlueprintSupport::SetFlushReinstancingQueueFPtr(nullptr); 
}

void FBlueprintCompilationManagerImpl::AddReferencedObjects(FReferenceCollector& Collector)
{
	for( FBPCompileRequest& Job : QueuedRequests )
	{
		Collector.AddReferencedObject(Job.BPToCompile);
	}

	Collector.AddReferencedObjects(ClassesToReinstance);
}

void FBlueprintCompilationManagerImpl::QueueForCompilation(const FBPCompileRequest& CompileJob)
{
	if(!CompileJob.BPToCompile->bQueuedForCompilation)
	{
		CompileJob.BPToCompile->bQueuedForCompilation = true;
		QueuedRequests.Add(CompileJob);
	}
}

void FBlueprintCompilationManagerImpl::CompileSynchronouslyImpl(const FBPCompileRequest& Request)
{
	Request.BPToCompile->bQueuedForCompilation = true;

	const bool bIsRegeneratingOnLoad		= (Request.CompileOptions & EBlueprintCompileOptions::IsRegeneratingOnLoad		) != EBlueprintCompileOptions::None;
	const bool bSkipGarbageCollection		= (Request.CompileOptions & EBlueprintCompileOptions::SkipGarbageCollection		) != EBlueprintCompileOptions::None;
	const bool bBatchCompile				= (Request.CompileOptions & EBlueprintCompileOptions::BatchCompile				) != EBlueprintCompileOptions::None;
	const bool bSkipReinstancing			= (Request.CompileOptions & EBlueprintCompileOptions::SkipReinstancing			) != EBlueprintCompileOptions::None;
	const bool bSkipSaving					= (Request.CompileOptions & EBlueprintCompileOptions::SkipSave					) != EBlueprintCompileOptions::None;

	// Wipe the PreCompile log, any generated messages are now irrelevant
	Request.BPToCompile->PreCompileLog.Reset();

	// Reset the flag, so if the user tries to use PIE it will warn them if the BP did not compile
	Request.BPToCompile->bDisplayCompilePIEWarning = true;
	
	// Do not want to run this code without the editor present nor when running commandlets.
	if (GEditor && GIsEditor)
	{
		// We do not want to regenerate a search Guid during loads, nothing has changed in the Blueprint and it is cached elsewhere
		if (!bIsRegeneratingOnLoad)
		{
			FFindInBlueprintSearchManager::Get().AddOrUpdateBlueprintSearchMetadata(Request.BPToCompile);
		}
	}

	ensure(!bIsRegeneratingOnLoad); // unexpected code path, compile on load handled with different function call
	ensure(!bSkipReinstancing); // This is an internal option, should not go through CompileSynchronouslyImpl

	ensure(QueuedRequests.Num() == 0);
	QueuedRequests.Add(Request);
	// We suppress normal compilation broadcasts because the old code path 
	// did this after GC and we want to match the old behavior:
	const bool bSuppressBroadcastCompiled = true;
	TArray<UBlueprint*> CompiledBlueprints;
	FlushCompilationQueueImpl(nullptr, bSuppressBroadcastCompiled, &CompiledBlueprints);
	FlushReinstancingQueueImpl();
	
	if (FBlueprintEditorUtils::IsLevelScriptBlueprint(Request.BPToCompile))
	{
		// When the Blueprint is recompiled, then update the bound events for level scripting
		ULevelScriptBlueprint* LevelScriptBP = CastChecked<ULevelScriptBlueprint>(Request.BPToCompile);

		// ULevel::OnLevelScriptBlueprintChanged needs to be run after the CDO has
		// been updated as it respawns the actor:
		if (ULevel* BPLevel = LevelScriptBP->GetLevel())
		{
			BPLevel->OnLevelScriptBlueprintChanged(LevelScriptBP);
		}
	}

	if ( GEditor )
	{
		GEditor->BroadcastBlueprintReinstanced();
	}
	
	ensure(Request.BPToCompile->bQueuedForCompilation == false);

	if(!bSkipGarbageCollection)
	{
		CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
	}
	
	if (!bBatchCompile)
	{
		for(UBlueprint* BP : CompiledBlueprints)
		{
			BP->BroadcastCompiled();
		}

		if(GEditor)
		{
			GEditor->BroadcastBlueprintCompiled();	
		}
	}

	if (CompiledBlueprintsToSave.Num() > 0)
	{
		if (!bSkipSaving)
		{
			TArray<UPackage*> PackagesToSave;
			for (UBlueprint* BP : CompiledBlueprintsToSave)
			{
				PackagesToSave.Add(BP->GetOutermost());
			}

			FEditorFileUtils::PromptForCheckoutAndSave(PackagesToSave, /*bCheckDirty =*/true, /*bPromptToSave =*/false);
		}
		CompiledBlueprintsToSave.Empty();
	}
}

static double GTimeCompiling = 0.f;
static double GTimeReinstancing = 0.f;

enum class ECompilationManagerJobType
{
	Normal,
	SkeletonOnly,
	RelinkOnly,
};

struct FCompilerData
{
	explicit FCompilerData(UBlueprint* InBP, ECompilationManagerJobType InJobType, FCompilerResultsLog* InResultsLogOverride, EBlueprintCompileOptions UserOptions, bool bBytecodeOnly)
	{
		check(InBP);
		BP = InBP;
		JobType = InJobType;
		UPackage* Package = BP->GetOutermost();
		bPackageWasDirty = Package ? Package->IsDirty() : false;

		ActiveResultsLog = InResultsLogOverride;
		if(InResultsLogOverride == nullptr)
		{
			ResultsLog = MakeUnique<FCompilerResultsLog>();
			ResultsLog->BeginEvent(TEXT("BlueprintCompilationManager Compile"));
			ResultsLog->SetSourcePath(InBP->GetPathName());
			ActiveResultsLog = ResultsLog.Get();
		}
		
		static const FBoolConfigValueHelper IgnoreCompileOnLoadErrorsOnBuildMachine(TEXT("Kismet"), TEXT("bIgnoreCompileOnLoadErrorsOnBuildMachine"), GEngineIni);
		ActiveResultsLog->bLogInfoOnly = !BP->bHasBeenRegenerated && GIsBuildMachine && IgnoreCompileOnLoadErrorsOnBuildMachine;

		InternalOptions.bRegenerateSkelton = false;
		InternalOptions.bReinstanceAndStubOnFailure = false;
		InternalOptions.bSaveIntermediateProducts = (UserOptions & EBlueprintCompileOptions::SaveIntermediateProducts ) != EBlueprintCompileOptions::None;
		InternalOptions.CompileType = bBytecodeOnly ? EKismetCompileType::BytecodeOnly : EKismetCompileType::Full;

		if( UWidgetBlueprint* WidgetBP = Cast<UWidgetBlueprint>(BP))
		{
			Compiler = UWidgetBlueprint::GetCompilerForWidgetBP(WidgetBP, *ActiveResultsLog, InternalOptions);
		}
		else
		{
			Compiler = FKismetCompilerContext::GetCompilerForBP(BP, *ActiveResultsLog, InternalOptions);
		}
	}

	bool IsSkeletonOnly() const { return JobType == ECompilationManagerJobType::SkeletonOnly; }
	bool ShouldResetClassMembers() const { return JobType != ECompilationManagerJobType::RelinkOnly; }
	bool ShouldSetTemporaryBlueprintFlags() const { return JobType != ECompilationManagerJobType::RelinkOnly; }
	bool ShouldValidateVariableNames() const { return JobType == ECompilationManagerJobType::Normal; }
	bool ShouldRegenerateSkeleton() const { return JobType != ECompilationManagerJobType::RelinkOnly; }
	bool ShouldMarkUpToDateAfterSkeletonStage() const { return IsSkeletonOnly(); }
	bool ShouldReconstructNodes() const { return JobType == ECompilationManagerJobType::Normal; }
	bool ShouldSkipReinstancerCreation() const { return (IsSkeletonOnly() && BP->ParentClass->IsNative()); }
	bool ShouldCompileClassLayout() const { return JobType == ECompilationManagerJobType::Normal; }
	bool ShouldCompileClassFunctions() const { return JobType == ECompilationManagerJobType::Normal; }
	bool ShouldRegisterCompilerResults() const { return JobType == ECompilationManagerJobType::Normal; }
	bool ShouldRelinkAfterSkippingCompile() const { return JobType == ECompilationManagerJobType::RelinkOnly; }

	UBlueprint* BP;
	FCompilerResultsLog* ActiveResultsLog;
	TUniquePtr<FCompilerResultsLog> ResultsLog;
	TSharedPtr<FKismetCompilerContext> Compiler;
	FKismetCompilerOptions InternalOptions;
	TSharedPtr<FBlueprintCompileReinstancer> Reinstancer;

	ECompilationManagerJobType JobType;
	bool bPackageWasDirty;
};

struct FReinstancingJob
{
	TSharedPtr<FBlueprintCompileReinstancer> Reinstancer;
	TSharedPtr<FKismetCompilerContext> Compiler;
};

void FBlueprintCompilationManagerImpl::FlushCompilationQueueImpl(TArray<UObject*>* ObjLoaded, bool bSuppressBroadcastCompiled, TArray<UBlueprint*>* BlueprintsCompiled)
{
	TGuardValue<bool> GuardTemplateNameFlag(GCompilingBlueprint, true);
	ensure(bGeneratedClassLayoutReady);

	if( QueuedRequests.Num() == 0 )
	{
		return;
	}

	TArray<FCompilerData> CurrentlyCompilingBPs;
	{ // begin GTimeCompiling scope 
		FScopedDurationTimer SetupTimer(GTimeCompiling); 

		// STAGE I: Add any related blueprints that were not compiled, then add any children so that they will be relinked:
		TArray<UBlueprint*> BlueprintsToRecompile;
		for(const FBPCompileRequest& ComplieJob : QueuedRequests)
		{
			// Add any dependent blueprints for a bytecode compile, this is needed because we 
			// have no way to keep bytecode safe when a function is renamed or parameters are
			// added or removed - strictly speaking we only need to do this when function 
			// parameters changed, but that's a somewhat dubious optimization - ideally this
			// work *never* needs to happen:
			if(!FBlueprintEditorUtils::IsInterfaceBlueprint(ComplieJob.BPToCompile))
			{
				TArray<UBlueprint*> DependentBlueprints;
				FBlueprintEditorUtils::GetDependentBlueprints(ComplieJob.BPToCompile, DependentBlueprints);
				for(UBlueprint* DependentBlueprint : DependentBlueprints)
				{
					if(!IsQueuedForCompilation(DependentBlueprint))
					{
						DependentBlueprint->bQueuedForCompilation = true;
						// Because we're adding this as a bytecode only blueprint compile we don't need to 
						// recursively recompile dependencies. The assumption is that a bytecode only compile
						// will not change the class layout. @todo: add an ensure to detect class layout changes
						CurrentlyCompilingBPs.Add(
							FCompilerData(
								DependentBlueprint, 
								ECompilationManagerJobType::Normal, 
								nullptr, 
								EBlueprintCompileOptions::None, 
								true
							)
						);
						BlueprintsToRecompile.Add(DependentBlueprint);
					}
				}
			}
		}

		// STAGE II: Filter out data only and interface blueprints:
		for(int32 I = 0; I < QueuedRequests.Num(); ++I)
		{
			bool bSkipCompile = false;
			FBPCompileRequest& QueuedJob = QueuedRequests[I];
			UBlueprint* QueuedBP = QueuedJob.BPToCompile;

			ensure(!QueuedBP->GeneratedClass || !(QueuedBP->GeneratedClass->ClassDefaultObject->HasAnyFlags(RF_NeedLoad)));
			bool bDefaultComponentMustBeAdded = false;
			bool bHasPendingUberGraphFrame = false;

			if(UBlueprintGeneratedClass* BPGC = Cast<UBlueprintGeneratedClass>(QueuedBP->GeneratedClass))
			{
				if( BPGC->SimpleConstructionScript &&
					BPGC->SimpleConstructionScript->GetSceneRootComponentTemplate() == nullptr)
				{
					bDefaultComponentMustBeAdded = true;
				}
				
				bHasPendingUberGraphFrame = BPGC->UberGraphFramePointerProperty || BPGC->UberGraphFunction;
			}
			
			if( FBlueprintEditorUtils::IsDataOnlyBlueprint(QueuedBP) && !QueuedBP->bHasBeenRegenerated && !bDefaultComponentMustBeAdded && !bHasPendingUberGraphFrame)
			{
				const UClass* ParentClass = QueuedBP->ParentClass;
				if (ParentClass && ParentClass->HasAllClassFlags(CLASS_Native))
				{
					bSkipCompile = true;
				}
				else if (const UClass* CurrentClass = QueuedBP->GeneratedClass)
				{
					if(FStructUtils::TheSameLayout(CurrentClass, CurrentClass->GetSuperStruct()))
					{
						bSkipCompile = true;
					}
				}
			}

			if(bSkipCompile)
			{
				CurrentlyCompilingBPs.Add(FCompilerData(QueuedBP, ECompilationManagerJobType::SkeletonOnly, QueuedJob.ClientResultsLog, QueuedJob.CompileOptions, false));
				if (QueuedBP->IsGeneratedClassAuthoritative() && (QueuedBP->GeneratedClass != nullptr))
				{
					// set bIsRegeneratingOnLoad so that we don't reset loaders:
					QueuedBP->bIsRegeneratingOnLoad = true;
					FBlueprintEditorUtils::RemoveStaleFunctions(Cast<UBlueprintGeneratedClass>(QueuedBP->GeneratedClass), QueuedBP);
					QueuedBP->bIsRegeneratingOnLoad = false;
				}

				// No actual compilation work to be done, but try to conform the class and fix up anything that might need to be updated if the native base class has changed in any way
				FKismetEditorUtilities::ConformBlueprintFlagsAndComponents(QueuedBP);

				if (QueuedBP->GeneratedClass)
				{
					FBlueprintEditorUtils::RecreateClassMetaData(QueuedBP, QueuedBP->GeneratedClass, true);
				}

				QueuedRequests.RemoveAtSwap(I);
				--I;
			}
			else
			{
				CurrentlyCompilingBPs.Add(FCompilerData(QueuedBP, ECompilationManagerJobType::Normal, QueuedJob.ClientResultsLog, QueuedJob.CompileOptions, false));
				BlueprintsToRecompile.Add(QueuedBP);
			}
		}
			
		for(UBlueprint* BP : BlueprintsToRecompile)
		{
			// make sure all children are at least re-linked:
			if(UClass* OldSkeletonClass = BP->SkeletonGeneratedClass)
			{
				TArray<UClass*> SkeletonClassesToReparentList;
				GetDerivedClasses(OldSkeletonClass, SkeletonClassesToReparentList);
		
				for(UClass* ChildClass : SkeletonClassesToReparentList)
				{
					if(UBlueprint* ChildBlueprint = UBlueprint::GetBlueprintFromClass(ChildClass))
					{
						if(!IsQueuedForCompilation(ChildBlueprint))
						{
							ChildBlueprint->bQueuedForCompilation = true;
							ensure(ChildBlueprint->bHasBeenRegenerated);
							CurrentlyCompilingBPs.Add(FCompilerData(ChildBlueprint, ECompilationManagerJobType::RelinkOnly, nullptr, EBlueprintCompileOptions::None, false));
						}
					}
				}
			}
		}

		BlueprintsToRecompile.Empty();
		QueuedRequests.Empty();

		// STAGE III: Sort into correct compilation order. We want to compile root types before their derived (child) types:
		auto HierarchyDepthSortFn = [](const FCompilerData& CompilerDataA, const FCompilerData& CompilerDataB)
		{
			UBlueprint& A = *(CompilerDataA.BP);
			UBlueprint& B = *(CompilerDataB.BP);

			bool bAIsInterface = FBlueprintEditorUtils::IsInterfaceBlueprint(&A);
			bool bBIsInterface = FBlueprintEditorUtils::IsInterfaceBlueprint(&B);

			if(bAIsInterface && !bBIsInterface)
			{
				return true;
			}
			else if(bBIsInterface && !bAIsInterface)
			{
				return false;
			}

			int32 DepthA = 0;
			int32 DepthB = 0;
			UStruct* Iter = *(A.GeneratedClass) ? A.GeneratedClass->GetSuperStruct() : nullptr;
			while (Iter)
			{
				++DepthA;
				Iter = Iter->GetSuperStruct();
			}

			Iter = *(B.GeneratedClass) ? B.GeneratedClass->GetSuperStruct() : nullptr;
			while (Iter)
			{
				++DepthB;
				Iter = Iter->GetSuperStruct();
			}

			if (DepthA == DepthB)
			{
				return A.GetFName() < B.GetFName(); 
			}
			return DepthA < DepthB;
		};
		CurrentlyCompilingBPs.Sort( HierarchyDepthSortFn );

		// STAGE IV: Set UBlueprint flags (bBeingCompiled, bIsRegeneratingOnLoad)
		for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
		{
			if(CompilerData.ShouldSetTemporaryBlueprintFlags())
			{
				UBlueprint* BP = CompilerData.BP;
				BP->bBeingCompiled = true;
				BP->CurrentMessageLog = CompilerData.ActiveResultsLog;
				BP->bIsRegeneratingOnLoad = !BP->bHasBeenRegenerated && BP->GetLinker();
				if(BP->bIsRegeneratingOnLoad)
				{
					// we may have cached dependencies before being fully loaded:
					BP->bCachedDependenciesUpToDate = false;
				}
			}
		}

		// STAGE V: Validate Variable Names
		for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
		{
			if(!CompilerData.ShouldValidateVariableNames())
			{
				continue;
			}

			CompilerData.Compiler->ValidateVariableNames();
		}

		// STAGE VI: Purge null graphs, could be done only on load
		for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
		{
			UBlueprint* BP = CompilerData.BP;
			FBlueprintEditorUtils::PurgeNullGraphs(BP);
		}

		// STAGE VII: safely throw away old skeleton CDOs:
		{
			TMap<UClass*, UClass*> NewSkeletonToOldSkeleton;
			for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
			{
				UBlueprint* BP = CompilerData.BP;
				UClass* OldSkeletonClass = BP->SkeletonGeneratedClass;
				if(OldSkeletonClass)
				{
					MoveSkelCDOAside(OldSkeletonClass, NewSkeletonToOldSkeleton);
				}
			}
		
		
			// STAGE VIII: recompile skeleton

			// if any function signatures have changed in this skeleton class we will need to recompile all dependencies, but if not
			// then we can avoid dependency recompilation:
			TSet<UBlueprint*> BlueprintsWithSignatureChanges;
			const UBlueprintEditorProjectSettings* EditorProjectSettings = GetDefault<UBlueprintEditorProjectSettings>();
			bool bSkipUnneededDependencyCompilation = EditorProjectSettings->bSkipUnneededDependencyCompilation;

			for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
			{
				UBlueprint* BP = CompilerData.BP;
		
				if(CompilerData.ShouldRegenerateSkeleton())
				{
					BP->SkeletonGeneratedClass = FastGenerateSkeletonClass(BP, *(CompilerData.Compiler) );
					UBlueprintGeneratedClass* AuthoritativeClass = Cast<UBlueprintGeneratedClass>(BP->GeneratedClass);
					if(AuthoritativeClass && bSkipUnneededDependencyCompilation)
					{
						if(CompilerData.InternalOptions.CompileType == EKismetCompileType::Full )
						{
							for (TFieldIterator<UFunction> FuncIt(AuthoritativeClass, EFieldIteratorFlags::ExcludeSuper); FuncIt; ++FuncIt)
							{
								// We assume that if the func is FUNC_BlueprintCallable that it will be present in the Skeleton class.
								// If it is not in the skeleton class we will always think that this blueprints public interface has 
								// changed. Not a huge deal, but will mean we recompile dependencies more often than necessary.
								if(FuncIt->HasAnyFunctionFlags(EFunctionFlags::FUNC_BlueprintCallable))
								{
									UFunction* NewFunction = BP->SkeletonGeneratedClass->FindFunctionByName((*FuncIt)->GetFName());
									if(NewFunction == nullptr || !NewFunction->IsSignatureCompatibleWith(*FuncIt))
									{
										BlueprintsWithSignatureChanges.Add(BP);
										break;
									}
								}
							}
						}
					}
				}
				else
				{
					// Just relink, note that UProperties that reference *other* types may be stale until
					// we fixup below:
					UClass* SkeletonToRelink = BP->SkeletonGeneratedClass;

					// CDO needs to be moved aside already:
					ensure(SkeletonToRelink->ClassDefaultObject == nullptr);
					ensure(!SkeletonToRelink->GetSuperClass()->HasAnyClassFlags(CLASS_NewerVersionExists));

					SkeletonToRelink->Bind();
					SkeletonToRelink->ClearFunctionMapsCaches();
					SkeletonToRelink->StaticLink(true);
				}

				if(CompilerData.ShouldMarkUpToDateAfterSkeletonStage())
				{
					// Flag data only blueprints as being up-to-date
					BP->Status = BS_UpToDate;
					BP->bHasBeenRegenerated = true;
					BP->GeneratedClass->ClearFunctionMapsCaches();
				}
			}

			// Skip further compilation for blueprints that are being bytecode compiled as a dependency of something that has
			// not had a change in its function parameters:
			auto DependenciesAreCompiled = [&BlueprintsWithSignatureChanges](FCompilerData& Data)
			{
				if(Data.InternalOptions.CompileType == EKismetCompileType::BytecodeOnly )
				{
					// if our parent is still being compiled, then we still need to be compiled:
					UClass* Iter = Data.BP->ParentClass;
					while(Iter)
					{
						if(UBlueprint* BP = Cast<UBlueprint>(Iter->ClassGeneratedBy))
						{
							if(BP->bBeingCompiled)
							{
								return false;
							}
						}
						Iter = Iter->GetSuperClass();
					}

					// otherwise if we're dependent on a blueprint that had a function signature change, we still need to be compiled:
					ensure(Data.BP->bCachedDependenciesUpToDate);
					ensure(Data.BP->CachedDependencies.Num() > 0); // why are we bytecode compiling a blueprint with no dependencies?
					for(const TWeakObjectPtr<UBlueprint>& Dependency : Data.BP->CachedDependencies)
					{
						if (UBlueprint* DependencyBP = Dependency.Get())
						{
							if(DependencyBP->bBeingCompiled && BlueprintsWithSignatureChanges.Contains(DependencyBP))
							{
								return false;
							}
						}
					}
					
					Data.BP->bBeingCompiled = false;
					Data.BP->CurrentMessageLog = nullptr;
					if(UPackage* Package = Data.BP->GetOutermost())
					{
						Package->SetDirtyFlag(Data.bPackageWasDirty);
					}
					if(Data.ResultsLog)
					{
						Data.ResultsLog->EndEvent();
					}
					Data.BP->bQueuedForCompilation = false;
					return true;
				}
			
				return false;
			};

			if(bSkipUnneededDependencyCompilation)
			{
				// Order very much matters, but we could RemoveAllSwap and re-sort:
				CurrentlyCompilingBPs.RemoveAll(DependenciesAreCompiled);
			}
		}

		// STAGE IX: Reconstruct nodes and replace deprecated nodes, then broadcast 'precompile
		for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
		{
			if(!CompilerData.ShouldReconstructNodes())
			{
				continue;
			}

			UBlueprint* BP = CompilerData.BP;

			// Some nodes are set up to do things during reconstruction only when this flag is NOT set.
			if(BP->bIsRegeneratingOnLoad)
			{
				FBlueprintEditorUtils::ReconstructAllNodes(BP);
				FBlueprintEditorUtils::ReplaceDeprecatedNodes(BP);
			}
			else
			{
				// matching existing behavior, when compiling a BP not on load we refresh nodes
				// before compiling:
				FBlueprintCompileReinstancer::OptionallyRefreshNodes(BP);
				TArray<UBlueprint*> DependentBlueprints;
				FBlueprintEditorUtils::GetDependentBlueprints(BP, DependentBlueprints);

				for (UBlueprint* CurrentBP : DependentBlueprints)
				{
					const EBlueprintStatus OriginalStatus = CurrentBP->Status;
					UPackage* const Package = CurrentBP->GetOutermost();
					const bool bStartedWithUnsavedChanges = Package != nullptr ? Package->IsDirty() : true;

					FBlueprintEditorUtils::RefreshExternalBlueprintDependencyNodes(CurrentBP, BP->GeneratedClass);

					CurrentBP->Status = OriginalStatus;
					if(Package != nullptr && Package->IsDirty() && !bStartedWithUnsavedChanges)
					{
						Package->SetDirtyFlag(false);
					}
				}
			}
			
			// Broadcast pre-compile
			{
				if(GEditor && GIsEditor)
				{
					GEditor->BroadcastBlueprintPreCompile(BP);
				}
			}

			// Do not want to run this code without the editor present nor when running commandlets.
			if (GEditor && GIsEditor)
			{
				// We do not want to regenerate a search Guid during loads, nothing has changed in the Blueprint and it is cached elsewhere
				if (!BP->bIsRegeneratingOnLoad)
				{
					FFindInBlueprintSearchManager::Get().AddOrUpdateBlueprintSearchMetadata(BP);
				}
			}

			// we are regenerated, tag ourself as such so that
			// old logic to 'fix' circular dependencies doesn't
			// cause redundant regeneration (e.g. bForceRegenNodes
			// in ExpandTunnelsAndMacros):
			BP->bHasBeenRegenerated = true;
		}
	
		// STAGE X: reinstance every blueprint that is queued, note that this means classes in the hierarchy that are *not* being 
		// compiled will be parented to REINST versions of the class, so type checks (IsA, etc) involving those types
		// will be incoherent!
		{
			for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
			{
				// we including skeleton only compilation jobs for reinstancing because we need UpdateCustomPropertyListForPostConstruction
				// to happen (at the right time) for those generated classes as well. This means we *don't* need to reinstance if 
				// the parent is a native type (unless we hot reload, but that should not need to be handled here):
				if(CompilerData.ShouldSkipReinstancerCreation())
				{
					continue;
				}

				UBlueprint* BP = CompilerData.BP;

				if(BP->GeneratedClass)
				{
					OldCDOs.Add(BP, BP->GeneratedClass->ClassDefaultObject);
				}
				CompilerData.Reinstancer = TSharedPtr<FBlueprintCompileReinstancer>( 
					new FBlueprintCompileReinstancer(
						BP->GeneratedClass, 
						EBlueprintCompileReinstancerFlags::AutoInferSaveOnCompile|EBlueprintCompileReinstancerFlags::AvoidCDODuplication
					)
				);
			}
		}

		// STAGE XI: Reinstancing done, lets fix up child->parent pointers
		for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
		{
			UBlueprint* BP = CompilerData.BP;
			if(BP->GeneratedClass && BP->GeneratedClass->GetSuperClass()->HasAnyClassFlags(CLASS_NewerVersionExists))
			{
				BP->GeneratedClass->SetSuperStruct(BP->GeneratedClass->GetSuperClass()->GetAuthoritativeClass());

				if(CompilerData.ShouldResetClassMembers())
				{
					BP->GeneratedClass->Children = NULL;
					BP->GeneratedClass->Script.Empty();
					BP->GeneratedClass->MinAlignment = 0;
					BP->GeneratedClass->RefLink = NULL;
					BP->GeneratedClass->PropertyLink = NULL;
					BP->GeneratedClass->DestructorLink = NULL;
					BP->GeneratedClass->ScriptObjectReferences.Empty();
					BP->GeneratedClass->PropertyLink = NULL;
				}
			}
		}

		// STAGE XII: Recompile every blueprint
		bGeneratedClassLayoutReady = false;
		for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
		{
			UBlueprint* BP = CompilerData.BP;
			if(CompilerData.ShouldCompileClassLayout())
			{
				ensure( BP->GeneratedClass == nullptr ||
						BP->GeneratedClass->ClassDefaultObject == nullptr || 
						BP->GeneratedClass->ClassDefaultObject->GetClass() != BP->GeneratedClass);
				// default value propagation occurs in ReinstaneBatch, CDO will be created via CompileFunctions call:
				if(BP->GeneratedClass)
				{
					BP->GeneratedClass->ClassDefaultObject = nullptr;
					// Reset the flag, so if the user tries to use PIE it will warn them if the BP did not compile
					BP->bDisplayCompilePIEWarning = true;
		
					FKismetCompilerContext& CompilerContext = *(CompilerData.Compiler);
					CompilerContext.CompileClassLayout(EInternalCompilerFlags::PostponeLocalsGenerationUntilPhaseTwo);
				}
				else
				{
					CompilerData.ActiveResultsLog->Error(*LOCTEXT("KismetCompileError_MalformedParentClasss", "Blueprint @@ has missing or NULL parent class.").ToString(), BP);
				}
			}
			else if(CompilerData.Compiler.IsValid() && BP->GeneratedClass)
			{
				CompilerData.Compiler->SetNewClass( CastChecked<UBlueprintGeneratedClass>(BP->GeneratedClass) );
			}
		}
		bGeneratedClassLayoutReady = true;
	
		// STAGE XIII: Compile functions
		UBlueprintEditorSettings* Settings = GetMutableDefault<UBlueprintEditorSettings>();
		
		const bool bSaveBlueprintsAfterCompile = Settings->SaveOnCompile == SoC_Always;
		const bool bSaveBlueprintAfterCompileSucceeded = Settings->SaveOnCompile == SoC_SuccessOnly;

		for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
		{
			UBlueprint* BP = CompilerData.BP;
			if(!CompilerData.ShouldCompileClassFunctions())
			{
				if( BP->GeneratedClass &&
					(	BP->GeneratedClass->ClassDefaultObject == nullptr || 
						BP->GeneratedClass->ClassDefaultObject->GetClass() != BP->GeneratedClass) )
				{
					// relink, generate CDO:
					UClass* BPGC = BP->GeneratedClass;
					BPGC->Bind();
					BPGC->StaticLink(true);
					BPGC->ClassDefaultObject = nullptr;
					BPGC->GetDefaultObject(true);
				}
			}
			else
			{
				ensure( BP->GeneratedClass == nullptr ||
						BP->GeneratedClass->ClassDefaultObject == nullptr || 
						BP->GeneratedClass->ClassDefaultObject->GetClass() != BP->GeneratedClass);
				
				// default value propagation occurrs below:
				if(BP->GeneratedClass)
				{
					BP->GeneratedClass->ClassDefaultObject = nullptr;
				
		
					FKismetCompilerContext& CompilerContext = *(CompilerData.Compiler);
					CompilerContext.CompileFunctions(
						EInternalCompilerFlags::PostponeLocalsGenerationUntilPhaseTwo
						|EInternalCompilerFlags::PostponeDefaultObjectAssignmentUntilReinstancing
						|EInternalCompilerFlags::SkipRefreshExternalBlueprintDependencyNodes
					);
				}

				if (CompilerData.ActiveResultsLog->NumErrors == 0)
				{
					// Blueprint is error free.  Go ahead and fix up debug info
					BP->Status = (0 == CompilerData.ActiveResultsLog->NumWarnings) ? BS_UpToDate : BS_UpToDateWithWarnings;

					BP->BlueprintSystemVersion = UBlueprint::GetCurrentBlueprintSystemVersion();

					// Reapply breakpoints to the bytecode of the new class
					for (UBreakpoint* Breakpoint  : BP->Breakpoints)
					{
						FKismetDebugUtilities::ReapplyBreakpoint(Breakpoint);
					}
				}
				else
				{
					BP->Status = BS_Error; // do we still have the old version of the class?
				}

				// SOC settings only apply after compile on load:
				if(!BP->bIsRegeneratingOnLoad)
				{
					if(bSaveBlueprintsAfterCompile || (bSaveBlueprintAfterCompileSucceeded && BP->Status == BS_UpToDate))
					{
						CompiledBlueprintsToSave.Add(BP);
					}
				}
			}

			if(BP->GeneratedClass)
			{
				extern COREUOBJECT_API void SetUpRuntimeReplicationData(UClass* Class);
				SetUpRuntimeReplicationData(BP->GeneratedClass);
			}
			
			ensure(BP->GeneratedClass == nullptr || BP->GeneratedClass->ClassDefaultObject->GetClass() == *(BP->GeneratedClass));
		}
	} // end GTimeCompiling scope

	// STAGE XIV: Now we can finish the first stage of the reinstancing operation, moving old classes to new classes:
	{
		{
			TArray<FReinstancingJob> Reinstancers;
			// Set up reinstancing jobs - we need a reference to the compiler in order to honor 
			// CopyTermDefaultsToDefaultObject
			for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
			{
				if(CompilerData.Reinstancer.IsValid() && CompilerData.Reinstancer->ClassToReinstance)
				{
					Reinstancers.Push(
						FReinstancingJob {
							CompilerData.Reinstancer,
							CompilerData.Compiler
						}
					);
				}
			}

			FScopedDurationTimer ReinstTimer(GTimeReinstancing);
			ReinstanceBatch(Reinstancers, ClassesToReinstance, ObjLoaded);

			OldCDOs.Empty();
		}
		
		// STAGE XV: CLEAR TEMPORARY FLAGS
		for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
		{
			UBlueprint* BP = CompilerData.BP;
			FBlueprintEditorUtils::UpdateDelegatesInBlueprint(BP);
			if(!BP->bIsRegeneratingOnLoad && BP->GeneratedClass)
			{
				FKismetEditorUtilities::StripExternalComponents(BP);
				
				if(BP->SimpleConstructionScript)
				{
					BP->SimpleConstructionScript->FixupRootNodeParentReferences();
				}

				const bool bIsInterface = FBlueprintEditorUtils::IsInterfaceBlueprint(BP);

				TArray<UBlueprint*> DependentBPs;
				FBlueprintEditorUtils::GetDependentBlueprints(BP, DependentBPs);

				// refresh each dependent blueprint
				for (UBlueprint* Dependent : DependentBPs)
				{
					if(!BP->bIsRegeneratingOnLoad)
					{
						// Some logic (e.g. UObject::ProcessInternal) uses this flag to suppress warnings:
						TGuardValue<bool> ReinstancingGuard(GIsReinstancing, true);
						// for non-interface changes, nodes with an external dependency have already been refreshed, and it is now safe to send a change notification event
						Dependent->BroadcastChanged();
					}
				}
				
				UBlueprint::ValidateGeneratedClass(BP->GeneratedClass);
			}

			if(CompilerData.ShouldRegisterCompilerResults())
			{
				// This helper structure registers the results log messages with the UI control that displays them:
				FScopedBlueprintMessageLog MessageLog(BP);
				MessageLog.Log->ClearMessages();
				MessageLog.Log->AddMessages(CompilerData.ActiveResultsLog->Messages, false);
			}

			if(CompilerData.ShouldSetTemporaryBlueprintFlags())
			{
				BP->bBeingCompiled = false;
				BP->CurrentMessageLog = nullptr;
				BP->bIsRegeneratingOnLoad = false;
			}

			if(UPackage* Package = BP->GetOutermost())
			{
				Package->SetDirtyFlag(CompilerData.bPackageWasDirty);
			}
		}

		// Make sure no junk in bytecode, this can happen only for blueprints that were in CurrentlyCompilingBPs because
		// the reinstancer can detect all other references (see UpdateBytecodeReferences):
		for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
		{
			if(CompilerData.ShouldCompileClassFunctions())
			{
				if(BlueprintsCompiled)
				{
					BlueprintsCompiled->Add(CompilerData.BP);
				}
				
				if(!bSuppressBroadcastCompiled)
				{
					// Some logic (e.g. UObject::ProcessInternal) uses this flag to suppress warnings:
					TGuardValue<bool> ReinstancingGuard(GIsReinstancing, true);
					CompilerData.BP->BroadcastCompiled();
				}

				continue;
			}

			UBlueprint* BP = CompilerData.BP;
			for( TFieldIterator<UFunction> FuncIter(BP->GeneratedClass, EFieldIteratorFlags::ExcludeSuper); FuncIter; ++FuncIter )
			{
				UFunction* CurrentFunction = *FuncIter;
				if( CurrentFunction->Script.Num() > 0 )
				{
					FFixupBytecodeReferences ValidateAr(CurrentFunction);
				}
			}
		}

		if (!bSuppressBroadcastCompiled)
		{
			if(GEditor)
			{
				GEditor->BroadcastBlueprintCompiled();	
			}
		}
	}

	for (FCompilerData& CompilerData : CurrentlyCompilingBPs)
	{
		if(CompilerData.ResultsLog)
		{
			CompilerData.ResultsLog->EndEvent();
		}
		CompilerData.BP->bQueuedForCompilation = false;
	}

	UEdGraphPin::Purge();

	UE_LOG(LogBlueprint, Display, TEXT("Time Compiling: %f, Time Reinstancing: %f"),  GTimeCompiling, GTimeReinstancing);
	//GTimeCompiling = 0.0;
	//GTimeReinstancing = 0.0;
	ensure(QueuedRequests.Num() == 0);
}

void FBlueprintCompilationManagerImpl::FlushReinstancingQueueImpl()
{
	if(GCompilingBlueprint)
	{
		return;
	}

	TGuardValue<bool> GuardTemplateNameFlag(GCompilingBlueprint, true);
	// we can finalize reinstancing now:
	if(ClassesToReinstance.Num() == 0)
	{
		return;
	}

	{
		FScopedDurationTimer ReinstTimer(GTimeReinstancing);
		
		TGuardValue<bool> ReinstancingGuard(GIsReinstancing, true);
		FBlueprintCompileReinstancer::BatchReplaceInstancesOfClass(ClassesToReinstance);

		ClassesToReinstance.Empty();
	}
	
#if VERIFY_NO_STALE_CLASS_REFERENCES
	FBlueprintSupport::ValidateNoRefsToOutOfDateClasses();
#endif

#if VERIFY_NO_BAD_SKELETON_REFERENCES
	FBlueprintSupport::ValidateNoExternalRefsToSkeletons();
#endif

	UE_LOG(LogBlueprint, Display, TEXT("Time Compiling: %f, Time Reinstancing: %f"),  GTimeCompiling, GTimeReinstancing);
}

bool FBlueprintCompilationManagerImpl::HasBlueprintsToCompile() const
{
	return QueuedRequests.Num() != 0;
}

bool FBlueprintCompilationManagerImpl::IsGeneratedClassLayoutReady() const
{
	return bGeneratedClassLayoutReady;
}

void FBlueprintCompilationManagerImpl::GetDefaultValue(const UClass* ForClass, const UProperty* Property, FString& OutDefaultValueAsString) const
{
	if(!ForClass || !Property)
	{
		return;
	}

	if (ForClass->ClassDefaultObject)
	{
		FBlueprintEditorUtils::PropertyValueToString(Property, (uint8*)ForClass->ClassDefaultObject, OutDefaultValueAsString);
	}
	else
	{
		UBlueprint* BP = Cast<UBlueprint>(ForClass->ClassGeneratedBy);
		if(ensure(BP))
		{
			const UObject* const* OldCDO = OldCDOs.Find(BP);
			if(OldCDO && *OldCDO)
			{
				const UClass* OldClass = (*OldCDO)->GetClass();
				const UProperty* OldProperty = OldClass->FindPropertyByName(Property->GetFName());
				if(OldProperty)
				{
					FBlueprintEditorUtils::PropertyValueToString(OldProperty, (uint8*)*OldCDO, OutDefaultValueAsString);
				}
			}
		}
	}
}

void FBlueprintCompilationManagerImpl::ReinstanceBatch(TArray<FReinstancingJob>& Reinstancers, TMap< UClass*, UClass* >& InOutOldToNewClassMap, TArray<UObject*>* ObjLoaded)
{
	const auto FilterOutOfDateClasses = [](TArray<UClass*>& ClassList)
	{
		ClassList.RemoveAllSwap( [](UClass* Class) { return Class->HasAnyClassFlags(CLASS_NewerVersionExists); } );
	};

	const auto HasChildren = [FilterOutOfDateClasses](UClass* InClass) -> bool
	{
		TArray<UClass*> ChildTypes;
		GetDerivedClasses(InClass, ChildTypes, false);
		FilterOutOfDateClasses(ChildTypes);
		return ChildTypes.Num() > 0;
	};

	TSet<UClass*> ClassesToReparent;
	TSet<UClass*> ClassesToReinstance;

	// Reinstancers may contain *part* of a class hierarchy, so we first need to reparent any child types that 
	// haven't already been reinstanced:
	for (const FReinstancingJob& ReinstancingJob : Reinstancers)
	{
		const TSharedPtr<FBlueprintCompileReinstancer>& CurrentReinstancer = ReinstancingJob.Reinstancer;
		UClass* OldClass = CurrentReinstancer->DuplicatedClass;
		InOutOldToNewClassMap.Add(CurrentReinstancer->DuplicatedClass, CurrentReinstancer->ClassToReinstance);
		if(!OldClass)
		{
			continue;
		}

		if(!HasChildren(OldClass))
		{
			continue;
		}

		bool bParentLayoutChanged = !FStructUtils::TheSameLayout(OldClass, CurrentReinstancer->ClassToReinstance);
		if(bParentLayoutChanged)
		{
			// we need *all* derived types:
			TArray<UClass*> ClassesToReinstanceList;
			GetDerivedClasses(OldClass, ClassesToReinstanceList);
			FilterOutOfDateClasses(ClassesToReinstanceList);
			
			for(UClass* ClassToReinstance : ClassesToReinstanceList)
			{
				ClassesToReinstance.Add(ClassToReinstance);
			}
		}
		else
		{
			// parent layout did not change, we can just relink the direct children:
			TArray<UClass*> ClassesToReparentList;
			GetDerivedClasses(OldClass, ClassesToReparentList, false);
			FilterOutOfDateClasses(ClassesToReparentList);
			
			for(UClass* ClassToReparent : ClassesToReparentList)
			{
				ClassesToReparent.Add(ClassToReparent);
			}
		}
	}

	for(UClass* Class : ClassesToReparent)
	{
		UClass** NewParent = InOutOldToNewClassMap.Find(Class->GetSuperClass());
		check(NewParent && *NewParent);
		Class->SetSuperStruct(*NewParent);
		Class->Bind();
		Class->StaticLink(true);
	}

	// make new hierarchy
	for(UClass* Class : ClassesToReinstance)
	{
		UObject* OriginalCDO = Class->ClassDefaultObject;
		Reinstancers.Emplace(
			FReinstancingJob {
				TSharedPtr<FBlueprintCompileReinstancer>( 
					new FBlueprintCompileReinstancer(
						Class, 
						EBlueprintCompileReinstancerFlags::AutoInferSaveOnCompile|EBlueprintCompileReinstancerFlags::AvoidCDODuplication
					)
				),
				TSharedPtr<FKismetCompilerContext>()
			}
		);

		// make sure we have the newest parent now that CDO has been moved to duplicate class:
		TSharedPtr<FBlueprintCompileReinstancer>& NewestReinstancer = Reinstancers.Last().Reinstancer;

		UClass* SuperClass = NewestReinstancer->ClassToReinstance->GetSuperClass();
		if(ensure(SuperClass))
		{
			if(SuperClass->HasAnyClassFlags(CLASS_NewerVersionExists))
			{
				NewestReinstancer->ClassToReinstance->SetSuperStruct(SuperClass->GetAuthoritativeClass());
			}
		}
		
		// relink the new class:
		NewestReinstancer->ClassToReinstance->Bind();
		NewestReinstancer->ClassToReinstance->StaticLink(true);
	}

	// run UpdateBytecodeReferences:
	for (const FReinstancingJob& ReinstancingJob : Reinstancers)
	{
		const TSharedPtr<FBlueprintCompileReinstancer>& CurrentReinstancer = ReinstancingJob.Reinstancer;
		InOutOldToNewClassMap.Add( CurrentReinstancer->DuplicatedClass, CurrentReinstancer->ClassToReinstance );
			
		UBlueprint* CompiledBlueprint = UBlueprint::GetBlueprintFromClass(CurrentReinstancer->ClassToReinstance);
		CurrentReinstancer->UpdateBytecodeReferences();
	}
	
	// Now we can update templates and archetypes - note that we don't look for direct references to archetypes - doing
	// so is very expensive and it will be much faster to directly update anything that cares to cache direct references
	// to an archetype here (e.g. a UClass::ClassDefaultObject member):
	
	// 1. Sort classes so that most derived types are updated last - right now the only caller of this function
	// also sorts, but we don't want to make too many assumptions about caller. We could refine this API so that
	// we're not taking a raw list of reinstancers:
	Reinstancers.Sort(
		[](const FReinstancingJob& ReinstancingDataA, const FReinstancingJob& ReinstancingDataB)
		{
			const TSharedPtr<FBlueprintCompileReinstancer>& CompilerDataA = ReinstancingDataA.Reinstancer;
			const TSharedPtr<FBlueprintCompileReinstancer>& CompilerDataB = ReinstancingDataB.Reinstancer;

			UClass* A = CompilerDataA->ClassToReinstance;
			UClass* B = CompilerDataB->ClassToReinstance;
			int32 DepthA = 0;
			int32 DepthB = 0;
			UStruct* Iter = A ? A->GetSuperStruct() : nullptr;
			while (Iter)
			{
				++DepthA;
				Iter = Iter->GetSuperStruct();
			}

			Iter = B ? B->GetSuperStruct() : nullptr;
			while (Iter)
			{
				++DepthB;
				Iter = Iter->GetSuperStruct();
			}

			if (DepthA == DepthB && A && B)
			{
				return A->GetFName() < B->GetFName(); 
			}
			return DepthA < DepthB;
		}
	);

	// 2. Copy defaults from old CDO - CDO may be missing if this class was reinstanced and relinked here,
	// so use GetDefaultObject(true):
	for (const FReinstancingJob& ReinstancingJob : Reinstancers)
	{
		const TSharedPtr<FBlueprintCompileReinstancer>& CurrentReinstancer = ReinstancingJob.Reinstancer;
		UObject* OldCDO = CurrentReinstancer->DuplicatedClass->ClassDefaultObject;
		if(OldCDO)
		{
			UObject* NewCDO = CurrentReinstancer->ClassToReinstance->GetDefaultObject(true);
			FBlueprintCompileReinstancer::CopyPropertiesForUnrelatedObjects(OldCDO, NewCDO, true);

			if(ReinstancingJob.Compiler.IsValid())
			{
				ReinstancingJob.Compiler->PropagateValuesToCDO(NewCDO, OldCDO);
			}

			if (UBlueprintGeneratedClass* BPGClass = CastChecked<UBlueprintGeneratedClass>(CurrentReinstancer->ClassToReinstance))
			{
				BPGClass->UpdateCustomPropertyListForPostConstruction();

				// patch new cdo into linker table:
				if(ObjLoaded)
				{
					UBlueprint* CurrentBP = CastChecked<UBlueprint>(BPGClass->ClassGeneratedBy);
					if(FLinkerLoad* CurrentLinker = CurrentBP->GetLinker())
					{
						int32 OldCDOIndex = INDEX_NONE;

						for (int32 i = 0; i < CurrentLinker->ExportMap.Num(); i++)
						{
							FObjectExport& ThisExport = CurrentLinker->ExportMap[i];
							if (ThisExport.ObjectFlags & RF_ClassDefaultObject)
							{
								OldCDOIndex = i;
								break;
							}
						}

						if(OldCDOIndex != INDEX_NONE)
						{
							FBlueprintEditorUtils::PatchNewCDOIntoLinker(CurrentBP->GeneratedClass->ClassDefaultObject, CurrentLinker, OldCDOIndex, *ObjLoaded);
							FBlueprintEditorUtils::PatchCDOSubobjectsIntoExport(OldCDO, CurrentBP->GeneratedClass->ClassDefaultObject);
						}
					}
				}
			}
		}
	}

	TMap<UObject*, UObject*> OldArchetypeToNewArchetype;

	// 3. Update any remaining instances that are tagged as RF_ArchetypeObject or RF_InheritableComponentTemplate - 
	// we may need to do further sorting to ensure that interdependent archetypes are initialized correctly:
	TSet<UObject*> ArchetypeReferencers;
	for (const FReinstancingJob& ReinstancingJob : Reinstancers)
	{
		const TSharedPtr<FBlueprintCompileReinstancer>& CurrentReinstancer = ReinstancingJob.Reinstancer;
		UClass* OldClass = CurrentReinstancer->DuplicatedClass;
		if(ensure(OldClass))
		{
			TArray<UObject*> ArchetypeObjects;
			GetObjectsOfClass(OldClass, ArchetypeObjects, false);
			
			// filter out non-archetype instances, note that WidgetTrees and some component
			// archetypes do not have RF_ArchetypeObject or RF_InheritableComponentTemplate so
			// we simply detect that they are outered to a UBPGC or UBlueprint and assume that 
			// they are archetype objects in practice:
			ArchetypeObjects.RemoveAllSwap(
				[](UObject* Obj) 
				{ 
					bool bIsArchetype = 
						Obj->HasAnyFlags(RF_ArchetypeObject|RF_InheritableComponentTemplate)
						|| Obj->GetTypedOuter<UBlueprintGeneratedClass>()
						|| Obj->GetTypedOuter<UBlueprint>();
					// remove if this is not an archetype or its already in the transient package, note
					// that things that are not directly outered to the transient package will be 
					// 'reinst'd', this is specifically to handle components, which need to be up to date
					// on the REINST_ actor class:
					return !bIsArchetype || Obj->GetOuter() == GetTransientPackage(); 
				}
			);

			// for each archetype:
			for(UObject* Archetype : ArchetypeObjects )
			{
				// make sure we fix up references in the owner:
				{
					UObject* Iter = Archetype->GetOuter();
					while(Iter)
					{
						UBlueprintGeneratedClass* IterAsBPGC = Cast<UBlueprintGeneratedClass>(Iter);
						if(Iter->HasAnyFlags(RF_ClassDefaultObject)
							|| IterAsBPGC
							|| Cast<UBlueprint>(Iter) )
						{
							ArchetypeReferencers.Add(Iter);

							// Component templates are referenced by the UBlueprint, but are outered to the UBPGC. Both
							// will need to be updated. Realistically there is no reason to reference these in the 
							// UBlueprint, so there is no reason to generalize this behavior:
							if(IterAsBPGC)
							{
								ArchetypeReferencers.Add(IterAsBPGC->ClassGeneratedBy);
							}

							// this handles nested subobjects:
							TArray<UObject*> ContainedObjects;
							GetObjectsWithOuter(Iter, ContainedObjects);
							ArchetypeReferencers.Append(ContainedObjects);
						}
						Iter = Iter->GetOuter();
					}
				}
				
				// move aside:
				FName OriginalName = Archetype->GetFName();
				UObject* OriginalOuter = Archetype->GetOuter();
				EObjectFlags OriginalFlags = Archetype->GetFlags();
				Archetype->Rename(
					nullptr,
					// destination - this is the important part of this call. Moving the object 
					// out of the way so we can reuse its name:
					GetTransientPackage(), 
					// Rename options:
					REN_DoNotDirty | REN_DontCreateRedirectors | REN_ForceNoResetLoaders );

				// reconstruct
				FMakeClassSpawnableOnScope TemporarilySpawnable(CurrentReinstancer->ClassToReinstance);
				const EObjectFlags FlagMask = RF_Public | RF_ArchetypeObject | RF_Transactional | RF_Transient | RF_TextExportTransient | RF_InheritableComponentTemplate | RF_Standalone; //TODO: what about RF_RootSet?
				UObject* NewArchetype = NewObject<UObject>(OriginalOuter, CurrentReinstancer->ClassToReinstance, OriginalName, OriginalFlags & FlagMask);

				// copy old data:
				FBlueprintCompileReinstancer::CopyPropertiesForUnrelatedObjects(Archetype, NewArchetype, false);

				OldArchetypeToNewArchetype.Add(Archetype, NewArchetype);
				// Map old subobjects to new subobjects. This is needed by UMG right now, which allows owning archetypes to reference subobjects
				// in subwidgets:
				{
					TArray<UObject*> OldSubobjects;
					GetObjectsWithOuter(Archetype, OldSubobjects);
					TArray<UObject*> NewSubobjects;
					GetObjectsWithOuter(NewArchetype, NewSubobjects);

					TMap<FName, UObject*> OldNameMap;
					for(UObject* OldSubobject : OldSubobjects )
					{
						OldNameMap.Add( OldSubobject->GetFName(), OldSubobject );
					}

					TMap<FName, UObject*> NewNameMap;
					for(UObject* NewSubobject : NewSubobjects )
					{
						NewNameMap.Add( NewSubobject->GetFName(), NewSubobject );
					}

					for(TPair< FName, UObject* > OldSubobject : OldNameMap )
					{
						UObject** NewSubobject = NewNameMap.Find(OldSubobject.Key);
						OldArchetypeToNewArchetype.Add(OldSubobject.Value, NewSubobject ? *NewSubobject : nullptr );
					}
					
				}

				ArchetypeReferencers.Add(NewArchetype);

				Archetype->RemoveFromRoot();
				Archetype->MarkPendingKill();
			}
		}
	}

	// 4. update known references to archetypes (e.g. component templates, WidgetTree). We don't want to run the normal 
	// reference finder to update these because searching the entire object graph is time consuming. Instead we just replace
	// all references in our UBlueprint and its generated class:
	for (const FReinstancingJob& ReinstancingJob : Reinstancers)
	{
		const TSharedPtr<FBlueprintCompileReinstancer>& CurrentReinstancer = ReinstancingJob.Reinstancer;
		ArchetypeReferencers.Add(CurrentReinstancer->ClassToReinstance);
		ArchetypeReferencers.Add(CurrentReinstancer->ClassToReinstance->ClassGeneratedBy);
		if(UBlueprint* BP = Cast<UBlueprint>(CurrentReinstancer->ClassToReinstance->ClassGeneratedBy))
		{
			// The only known way to cause this ensure to trip is to enqueue bluerpints for compilation
			// while blueprints are already compiling:
			if( ensure(BP->SkeletonGeneratedClass) )
			{
				ArchetypeReferencers.Add(BP->SkeletonGeneratedClass);
			}
			ensure(BP->bCachedDependenciesUpToDate);
			for(const TWeakObjectPtr<UBlueprint>& Dependency : BP->CachedDependencies)
			{
				if (UBlueprint* DependencyBP = Dependency.Get())
				{
					ArchetypeReferencers.Add(DependencyBP);
				}
			}
		}
	}

	for(UObject* ArchetypeReferencer : ArchetypeReferencers)
	{
		UPackage* NewPackage = ArchetypeReferencer->GetOutermost();
		FArchiveReplaceOrClearExternalReferences<UObject> ReplaceInCDOAr(ArchetypeReferencer, OldArchetypeToNewArchetype, NewPackage);
	}
}

/*
	This function completely replaces the 'skeleton only' compilation pass in the Kismet compiler. Long
	term that code path will be removed and clients will be redirected to this function.

	Notes to maintainers: any UObject created here and outered to the resulting class must be marked as transient
	or you will create a cook error!
*/
UClass* FBlueprintCompilationManagerImpl::FastGenerateSkeletonClass(UBlueprint* BP, FKismetCompilerContext& CompilerContext)
{
	const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();

	FCompilerResultsLog MessageLog;

	UClass* ParentClass = BP->ParentClass;
	if(ParentClass == nullptr)
	{
		return nullptr;
	}

	if(ParentClass->ClassGeneratedBy)
	{
		if(UBlueprint* ParentBP = Cast<UBlueprint>(ParentClass->ClassGeneratedBy))
		{
			if(ParentBP->SkeletonGeneratedClass)
			{
				ParentClass = ParentBP->SkeletonGeneratedClass;
			}
		}
	}

	UBlueprintGeneratedClass* Ret = nullptr;
	UBlueprintGeneratedClass* OriginalNewClass = CompilerContext.NewClass;
	FString SkelClassName = FString::Printf(TEXT("SKEL_%s_C"), *BP->GetName());

	if (BP->SkeletonGeneratedClass == nullptr)
	{
		// This might exist in the package because we are being reloaded in place
		BP->SkeletonGeneratedClass = FindObject<UBlueprintGeneratedClass>(BP->GetOutermost(), *SkelClassName);
	}

	if (BP->SkeletonGeneratedClass == nullptr)
	{
		CompilerContext.SpawnNewClass(SkelClassName);
		Ret = CompilerContext.NewClass;
		Ret->SetFlags(RF_Transient);
		CompilerContext.NewClass = OriginalNewClass;
	}
	else
	{
		Ret = CastChecked<UBlueprintGeneratedClass>(*(BP->SkeletonGeneratedClass));
		CompilerContext.CleanAndSanitizeClass(Ret, Ret->ClassDefaultObject);
	}
	
	Ret->ClassGeneratedBy = BP;

	// This is a version of PrecompileFunction that does not require 'terms' and graph cloning:
	const auto MakeFunction = [Ret, ParentClass, Schema, BP, &MessageLog]
		(	FName FunctionNameFName, 
			UField**& InCurrentFieldStorageLocation, 
			UField**& InCurrentParamStorageLocation, 
			EFunctionFlags InFunctionFlags, 
			const TArray<UK2Node_FunctionResult*>& ReturnNodes, 
			const TArray<UEdGraphPin*>& InputPins, 
			bool bIsStaticFunction, 
			bool bForceArrayStructRefsConst, 
			UFunction* SignatureOverride) -> UFunction*
	{
		if(!ensure(FunctionNameFName != FName())
			|| FindObjectFast<UField>(Ret, FunctionNameFName))
		{
			return nullptr;
		}
		
		UFunction* NewFunction = NewObject<UFunction>(Ret, FunctionNameFName, RF_Public|RF_Transient);
					
		Ret->AddFunctionToFunctionMap(NewFunction, NewFunction->GetFName());

		*InCurrentFieldStorageLocation = NewFunction;
		InCurrentFieldStorageLocation = &NewFunction->Next;

		if(bIsStaticFunction)
		{
			NewFunction->FunctionFlags |= FUNC_Static;
		}

		UFunction* ParentFn = ParentClass->FindFunctionByName(NewFunction->GetFName());
		if(ParentFn == nullptr)
		{
			// check for function in implemented interfaces:
			for(const FBPInterfaceDescription& BPID : BP->ImplementedInterfaces)
			{
				// we only want the *skeleton* version of the function:
				UClass* InterfaceClass = BPID.Interface;
				// We need to null check because FBlueprintEditorUtils::ConformImplementedInterfaces won't run until 
				// after the skeleton classes have been generated:
				if(InterfaceClass)
				{
					if(UBlueprint* Owner = Cast<UBlueprint>(InterfaceClass->ClassGeneratedBy))
					{
						if( ensure(Owner->SkeletonGeneratedClass) )
						{
							InterfaceClass = Owner->SkeletonGeneratedClass;
						}
					}

					if(UFunction* ParentInterfaceFn = InterfaceClass->FindFunctionByName(NewFunction->GetFName()))
					{
						ParentFn = ParentInterfaceFn;
						break;
					}
				}
			}
		}
		NewFunction->SetSuperStruct( ParentFn );
		
		InCurrentParamStorageLocation = &NewFunction->Children;

		// params:
		if(ParentFn || SignatureOverride)
		{
			UFunction* SignatureFn = ParentFn ? ParentFn : SignatureOverride;
			NewFunction->FunctionFlags |= (SignatureFn->FunctionFlags & (FUNC_FuncInherit | FUNC_Public | FUNC_Protected | FUNC_Private | FUNC_BlueprintPure));
			for (TFieldIterator<UProperty> PropIt(SignatureFn); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
			{
				UProperty* ClonedParam = CastChecked<UProperty>(StaticDuplicateObject(*PropIt, NewFunction, PropIt->GetFName(), RF_AllFlags, nullptr, EDuplicateMode::Normal, EInternalObjectFlags::AllFlags & ~(EInternalObjectFlags::Native) ));
				ClonedParam->PropertyFlags |= CPF_BlueprintVisible|CPF_BlueprintReadOnly;
				ClonedParam->Next = nullptr;
				*InCurrentParamStorageLocation = ClonedParam;
				InCurrentParamStorageLocation = &ClonedParam->Next;
			}
			UMetaData::CopyMetadata(SignatureFn, NewFunction);
		}
		else
		{
			NewFunction->FunctionFlags |= InFunctionFlags;
			for(UEdGraphPin* Pin : InputPins)
			{
				if(Pin->Direction == EEdGraphPinDirection::EGPD_Output && !Schema->IsExecPin(*Pin) && Pin->ParentPin == nullptr && Pin->GetName() != UK2Node_Event::DelegateOutputName)
				{
					// Reimplementation of FKismetCompilerContext::CreatePropertiesFromList without dependence on 'terms'
					UProperty* Param = FKismetCompilerUtilities::CreatePropertyOnScope(NewFunction, *(Pin->PinName), Pin->PinType, Ret, CPF_BlueprintVisible|CPF_BlueprintReadOnly, Schema, MessageLog);
					if(Param)
					{
						Param->SetFlags(RF_Transient);
						Param->PropertyFlags |= CPF_Parm;
						if(Pin->PinType.bIsReference)
						{
							Param->PropertyFlags |= CPF_ReferenceParm | CPF_OutParm;
						}

						if(Pin->PinType.bIsConst || (bForceArrayStructRefsConst && (Pin->PinType.IsArray() || Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Struct) && Pin->PinType.bIsReference))
						{
							Param->PropertyFlags |= CPF_ConstParm;
						}

						if (UObjectProperty* ObjProp = Cast<UObjectProperty>(Param))
						{
							UClass* EffectiveClass = nullptr;
							if (ObjProp->PropertyClass != nullptr)
							{
								EffectiveClass = ObjProp->PropertyClass;
							}
							else if (UClassProperty* ClassProp = Cast<UClassProperty>(ObjProp))
							{
								EffectiveClass = ClassProp->MetaClass;
							}

							if ((EffectiveClass != nullptr) && (EffectiveClass->HasAnyClassFlags(CLASS_Const)))
							{
								Param->PropertyFlags |= CPF_ConstParm;
							}
						}
						else if (UArrayProperty* ArrayProp = Cast<UArrayProperty>(Param))
						{
							Param->PropertyFlags |= CPF_ReferenceParm;

							// ALWAYS pass array parameters as out params, so they're set up as passed by ref
							Param->PropertyFlags |= CPF_OutParm;
						}

						*InCurrentParamStorageLocation = Param;
						InCurrentParamStorageLocation = &Param->Next;
					}
				}
			}
			
			if(ReturnNodes.Num() > 0)
			{
				// Gather all input pins on these nodes, these are 
				// the outputs of the function:
				TSet<FString> UsedPinNames;
				static const FName RetValName = FName(TEXT("ReturnValue"));
				for(UK2Node_FunctionResult* Node : ReturnNodes)
				{
					for(UEdGraphPin* Pin : Node->Pins)
					{
						if(!Schema->IsExecPin(*Pin) && Pin->ParentPin == nullptr)
						{								
							if(!UsedPinNames.Contains(Pin->PinName))
							{
								UsedPinNames.Add(Pin->PinName);
							
								UProperty* Param = FKismetCompilerUtilities::CreatePropertyOnScope(NewFunction, *(Pin->PinName), Pin->PinType, Ret, 0, Schema, MessageLog);
								if(Param)
								{
									Param->SetFlags(RF_Transient);
									// we only tag things as CPF_ReturnParm if the value is named ReturnValue.... this is *terrible* behavior:
									if(Param->GetFName() == RetValName)
									{
										Param->PropertyFlags |= CPF_ReturnParm;
									}
									Param->PropertyFlags |= CPF_Parm|CPF_OutParm;
									*InCurrentParamStorageLocation = Param;
									InCurrentParamStorageLocation = &Param->Next;
								}
							}
						}
					}
				}
			}
		}

		// We're linking the skeleton function because TProperty::LinkInternal
		// will assign add TTypeFundamentals::GetComputedFlagsPropertyFlags()
		// to PropertyFlags. PropertyFlags must (mostly) match in order for 
		// functions to be compatible:
		NewFunction->StaticLink(true);
		return NewFunction;
	};


	// helpers:
	const auto AddFunctionForGraphs = [Schema, &MessageLog, ParentClass, Ret, BP, MakeFunction](const TCHAR* FunctionNamePostfix, const TArray<UEdGraph*>& Graphs, UField**& InCurrentFieldStorageLocation, bool bIsStaticFunction)
	{
		for( const UEdGraph* Graph : Graphs )
		{
			TArray<UK2Node_FunctionEntry*> EntryNodes;
			Graph->GetNodesOfClass(EntryNodes);
			if(EntryNodes.Num() > 0)
			{
				TArray<UK2Node_FunctionResult*> ReturnNodes;
				Graph->GetNodesOfClass(ReturnNodes);
				UK2Node_FunctionEntry* EntryNode = EntryNodes[0];
				
				UField** CurrentParamStorageLocation = nullptr;
				UFunction* NewFunction = MakeFunction(
					FName(*(Graph->GetName() + FunctionNamePostfix)), 
					InCurrentFieldStorageLocation, 
					CurrentParamStorageLocation, 
					(EFunctionFlags)(EntryNode->GetFunctionFlags() & ~FUNC_Native),
					ReturnNodes, 
					EntryNode->Pins,
					bIsStaticFunction, 
					false,
					nullptr
				);

				if(NewFunction)
				{
					// locals:
					for( const FBPVariableDescription& BPVD : EntryNode->LocalVariables )
					{
						if(UProperty* LocalVariable = FKismetCompilerContext::CreateUserDefinedLocalVariableForFunction(BPVD, NewFunction, Ret, CurrentParamStorageLocation, Schema, MessageLog) )
						{
							LocalVariable->SetFlags(RF_Transient);
						}
					}

					// __WorldContext:
					if(bIsStaticFunction)
					{
						if( FindField<UObjectProperty>(NewFunction, TEXT("__WorldContext")) == nullptr )
						{
							FEdGraphPinType WorldContextPinType(Schema->PC_Object, FString(), UObject::StaticClass(), EPinContainerType::None, false, FEdGraphTerminalType());
							UProperty* Param = FKismetCompilerUtilities::CreatePropertyOnScope(NewFunction, TEXT("__WorldContext"), WorldContextPinType, Ret, 0, Schema, MessageLog);
							if(Param)
							{
								Param->SetFlags(RF_Transient);
								Param->PropertyFlags |= CPF_Parm;
								*CurrentParamStorageLocation = Param;
								CurrentParamStorageLocation = &Param->Next;
							}
						}
						
						// set the metdata:
						NewFunction->SetMetaData(FBlueprintMetadata::MD_WorldContext, TEXT("__WorldContext"));
					}

					FKismetCompilerContext::SetCalculatedMetaDataAndFlags(NewFunction, EntryNode, Schema);
				}
			}
		}
	};

	UField** CurrentFieldStorageLocation = &Ret->Children;
	
	// Helper function for making UFunctions generated for 'event' nodes, e.g. custom event and timelines
	const auto MakeEventFunction = [&CurrentFieldStorageLocation, MakeFunction, Schema]( FName InName, EFunctionFlags ExtraFnFlags, const TArray<UEdGraphPin*>& InputPins, UFunction* InSourceFN, bool bInCallInEditor, TArray< TSharedPtr<FUserPinInfo> >* UserAddedPins )
	{
		UField** CurrentParamStorageLocation = nullptr;

		UFunction* NewFunction = MakeFunction(
			InName, 
			CurrentFieldStorageLocation, 
			CurrentParamStorageLocation, 
			ExtraFnFlags|FUNC_BlueprintCallable|FUNC_BlueprintEvent,
			TArray<UK2Node_FunctionResult*>(), 
			InputPins,
			false, 
			true,
			InSourceFN
		);

		if(NewFunction)
		{
			for (UEdGraphPin* InputPin : InputPins)
			{
				// No defaults for object/class pins
				if(	!Schema->IsMetaPin(*InputPin) && 
					(InputPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Object) && 
					(InputPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Class) && 
					(InputPin->PinType.PinCategory != UEdGraphSchema_K2::PC_Interface) )
				{
					// First look in user defined pins. There appears to be an issue propagating the user defined pin's default value
					// to the actual input pins. This may only be a problem with old data, but it's easiest to go right to the source,
					// rather than rely on other editor logic to keep UserDefinedPins and Node::Pins in sync.
					bool bFoundDefaultInUserAddedPins = false;
					if(UserAddedPins )
					{
						const TSharedPtr<FUserPinInfo>* UserPin = UserAddedPins->FindByPredicate(
							[InputPin](const TSharedPtr<FUserPinInfo>& PinInfo ) 
							{ 
								return PinInfo.IsValid() && PinInfo->PinName == InputPin->PinName; 
							} 
						);

						if(UserPin)
						{
							bFoundDefaultInUserAddedPins = true;
							NewFunction->SetMetaData(*InputPin->PinName, *((*UserPin)->PinDefaultValue));
						}
					}

					if(!bFoundDefaultInUserAddedPins && !InputPin->DefaultValue.IsEmpty())
					{
						NewFunction->SetMetaData(*InputPin->PinName, *InputPin->DefaultValue);
					}
				}
			}

			if(bInCallInEditor)
			{
				NewFunction->SetMetaData(FBlueprintMetadata::MD_CallInEditor, TEXT( "true" ));
			}

			NewFunction->Bind();
			NewFunction->StaticLink(true);
		}
	};

	Ret->SetSuperStruct(ParentClass);
	
	Ret->ClassFlags |= (ParentClass->ClassFlags & CLASS_Inherit);
	Ret->ClassCastFlags |= ParentClass->ClassCastFlags;
	
	if (FBlueprintEditorUtils::IsInterfaceBlueprint(BP))
	{
		Ret->ClassFlags |= CLASS_Interface;
	}

	// link in delegate signatures, variables will reference these 
	AddFunctionForGraphs(HEADER_GENERATED_DELEGATE_SIGNATURE_SUFFIX, BP->DelegateSignatureGraphs, CurrentFieldStorageLocation, false);

	// handle event entry ponts (mostly custom events) - this replaces
	// the skeleton compile pass CreateFunctionStubForEvent call:
	TArray<UEdGraph*> AllEventGraphs;
	
	for (UEdGraph* UberGraph : BP->UbergraphPages)
	{
		AllEventGraphs.Add(UberGraph);
		UberGraph->GetAllChildrenGraphs(AllEventGraphs);
	}

	for( const UEdGraph* Graph : AllEventGraphs )
	{
		TArray<UK2Node_Event*> EventNodes;
		Graph->GetNodesOfClass(EventNodes);
		for( UK2Node_Event* Event : EventNodes )
		{
			bool bCallInEditor = false;
			TArray< TSharedPtr<FUserPinInfo> >* UserAddedPins = nullptr;
			if(UK2Node_CustomEvent* CustomEvent = Cast<UK2Node_CustomEvent>(Event))
			{
				bCallInEditor = CustomEvent->bCallInEditor;
				UserAddedPins = &CustomEvent->UserDefinedPins;
			}

			MakeEventFunction(
				CompilerContext.GetEventStubFunctionName(Event), 
				(EFunctionFlags)Event->FunctionFlags, 
				Event->Pins, 
				Event->FindEventSignatureFunction(),
				bCallInEditor,
				UserAddedPins
			);
		}
	}
	
	for ( const UTimelineTemplate* Timeline : BP->Timelines )
	{
		for(int32 EventTrackIdx=0; EventTrackIdx<Timeline->EventTracks.Num(); EventTrackIdx++)
		{
			MakeEventFunction(Timeline->GetEventTrackFunctionName(EventTrackIdx), EFunctionFlags::FUNC_None, TArray<UEdGraphPin*>(), nullptr, false, nullptr);
		}
		
		MakeEventFunction(Timeline->GetUpdateFunctionName(), EFunctionFlags::FUNC_None, TArray<UEdGraphPin*>(), nullptr, false, nullptr);
		MakeEventFunction(Timeline->GetFinishedFunctionName(), EFunctionFlags::FUNC_None, TArray<UEdGraphPin*>(), nullptr, false, nullptr);
	}

	CompilerContext.NewClass = Ret;
	CompilerContext.bAssignDelegateSignatureFunction = true;
	CompilerContext.bGenerateSubInstanceVariables = true;
	CompilerContext.CreateClassVariablesFromBlueprint();
	CompilerContext.bAssignDelegateSignatureFunction = false;
	CompilerContext.bGenerateSubInstanceVariables = false;
	CompilerContext.NewClass = OriginalNewClass;
	UField* Iter = Ret->Children;
	while(Iter)
	{
		CurrentFieldStorageLocation = &Iter->Next;
		Iter = Iter->Next;
	}
	
	AddFunctionForGraphs(TEXT(""), BP->FunctionGraphs, CurrentFieldStorageLocation, BPTYPE_FunctionLibrary == BP->BlueprintType);

	// Add interface functions, often these are added by normal detection of implemented functions, but they won't be
	// if the interface is added but the function is not implemented:
	for(const FBPInterfaceDescription& BPID : BP->ImplementedInterfaces)
	{
		UClass* InterfaceClass = BPID.Interface;
		// Again, once the skeleton has been created we will purge null ImplementedInterfaces entries,
		// but not yet:
		if(InterfaceClass)
		{
			if(UBlueprint* Owner = Cast<UBlueprint>(InterfaceClass->ClassGeneratedBy))
			{
				if( ensure(Owner->SkeletonGeneratedClass) )
				{
					InterfaceClass = Owner->SkeletonGeneratedClass;
				}
			}

			AddFunctionForGraphs(TEXT(""), BPID.Graphs, CurrentFieldStorageLocation, BPTYPE_FunctionLibrary == BP->BlueprintType);

			for (TFieldIterator<UFunction> FunctionIt(InterfaceClass, EFieldIteratorFlags::ExcludeSuper); FunctionIt; ++FunctionIt)
			{
				UFunction* Fn = *FunctionIt;
			
				UField** CurrentParamStorageLocation = nullptr;

				// Note that MakeFunction will early out if the function was created above:
				MakeFunction(
					Fn->GetFName(), 
					CurrentFieldStorageLocation, 
					CurrentParamStorageLocation, 
					Fn->FunctionFlags & ~FUNC_Native, 
					TArray<UK2Node_FunctionResult*>(), 
					TArray<UEdGraphPin*>(),
					false, 
					false,
					nullptr
				);
			}
		}
	}

	CompilerContext.NewClass = Ret;
	CompilerContext.bAssignDelegateSignatureFunction = true;
	CompilerContext.FinishCompilingClass(Ret);
	CompilerContext.bAssignDelegateSignatureFunction = false;
	CompilerContext.NewClass = OriginalNewClass;

	Ret->GetDefaultObject()->SetFlags(RF_Transient);

	return Ret;
}

bool FBlueprintCompilationManagerImpl::IsQueuedForCompilation(UBlueprint* BP)
{
	return BP->bQueuedForCompilation;
}

// FFixupBytecodeReferences Implementation:
FBlueprintCompilationManagerImpl::FFixupBytecodeReferences::FFixupBytecodeReferences(UObject* InObject)
{
	ArIsObjectReferenceCollector = true;
		
	InObject->Serialize(*this);
	class FArchiveProxyCollector : public FReferenceCollector
	{
		/** Archive we are a proxy for */
		FArchive& Archive;
	public:
		FArchiveProxyCollector(FArchive& InArchive)
			: Archive(InArchive)
		{
		}
		virtual void HandleObjectReference(UObject*& Object, const UObject* ReferencingObject, const UProperty* ReferencingProperty) override
		{
			Archive << Object;
		}
		virtual void HandleObjectReferences(UObject** InObjects, const int32 ObjectNum, const UObject* InReferencingObject, const UProperty* InReferencingProperty) override
		{
			for (int32 ObjectIndex = 0; ObjectIndex < ObjectNum; ++ObjectIndex)
			{
				UObject*& Object = InObjects[ObjectIndex];
				Archive << Object;
			}
		}
		virtual bool IsIgnoringArchetypeRef() const override
		{
			return false;
		}
		virtual bool IsIgnoringTransient() const override
		{
			return false;
		}
	} ArchiveProxyCollector(*this);
		
	InObject->GetClass()->CallAddReferencedObjects(InObject, ArchiveProxyCollector);
}

FArchive& FBlueprintCompilationManagerImpl::FFixupBytecodeReferences::operator<<( UObject*& Obj )
{
	if (Obj != NULL)
	{
		if(UClass* RelatedClass = Cast<UClass>(Obj))
		{
			UClass* NewClass = RelatedClass->GetAuthoritativeClass();
			ensure(NewClass);
			if(NewClass != RelatedClass)
			{
				Obj = NewClass;
			}
		}
		else if(UField* AsField = Cast<UField>(Obj))
		{
			UClass* OwningClass = AsField->GetOwnerClass();
			if(OwningClass)
			{
				UClass* NewClass = OwningClass->GetAuthoritativeClass();
				ensure(NewClass);
				if(NewClass != OwningClass)
				{
					// drill into new class finding equivalent object:
					TArray<FName> Names;
					UObject* Iter = Obj;
					while (Iter && Iter != OwningClass)
					{
						Names.Add(Iter->GetFName());
						Iter = Iter->GetOuter();
					}

					UObject* Owner = NewClass;
					UObject* Match = nullptr;
					for(int32 I = Names.Num() - 1; I >= 0; --I)
					{
						UObject* Next = StaticFindObjectFast( UObject::StaticClass(), Owner, Names[I]);
						if( Next )
						{
							if(I == 0)
							{
								Match = Next;
							}
							else
							{
								Owner = Match;
							}
						}
						else
						{
							break;
						}
					}
						
					if(Match)
					{
						Obj = Match;
					}
				}
			}
		}
	}
	return *this;
}

// Singleton boilerplate, simply forwarding to the implementation above:
FBlueprintCompilationManagerImpl* BPCMImpl = nullptr;

void FlushReinstancingQueueImplWrapper()
{
	BPCMImpl->FlushReinstancingQueueImpl();
}

// Recursive function to move CDOs aside to immutable versions of classes
// so that CDOs can be safely GC'd. Recursion is necessary to find REINST_ classes
// that are still parented to a valid SKEL (e.g. from MarkBlueprintAsStructurallyModified)
// and therefore need to be REINST_'d again before the SKEL is mutated... Normally
// these old REINST_ classes are GC'd but, there is no guarantee of that:
void MoveSkelCDOAside(UClass* Class, TMap<UClass*, UClass*>& OutOldToNewMap)
{
	UClass* CopyOfOldClass = FBlueprintCompileReinstancer::MoveCDOToNewClass(Class, OutOldToNewMap, true);
	OutOldToNewMap.Add(Class, CopyOfOldClass);

	// Child types that are associated with a BP will be compiled by the compilation
	// manager, but old REINST_ or TRASH_ types need to be handled explicitly:
	TArray<UClass*> Children;
	GetDerivedClasses(Class, Children);
	for(UClass* Child : Children)
	{
		if(UBlueprint* BP = Cast<UBlueprint>(Child->ClassGeneratedBy))
		{
			if(BP->SkeletonGeneratedClass != Child)
			{
				if(	ensureMsgf ( 
					BP->GeneratedClass != Child, 
					TEXT("Class in skeleton hierarchy is cached as GeneratedClass"))
				)
				{
					MoveSkelCDOAside(Child, OutOldToNewMap);
				}
			}
		}
	}
};

void FBlueprintCompilationManager::Initialize()
{
	if(!BPCMImpl)
	{
		BPCMImpl = new FBlueprintCompilationManagerImpl();
	}
}

void FBlueprintCompilationManager::Shutdown()
{
	delete BPCMImpl;
	BPCMImpl = nullptr;
}

// Forward to impl:
void FBlueprintCompilationManager::FlushCompilationQueue(TArray<UObject*>* ObjLoaded)
{
	if(BPCMImpl)
	{
		BPCMImpl->FlushCompilationQueueImpl(ObjLoaded, false, nullptr);

		// we can't support save on compile when reinstancing is deferred:
		BPCMImpl->CompiledBlueprintsToSave.Empty();
	}
}

void FBlueprintCompilationManager::FlushCompilationQueueAndReinstance()
{
	if(BPCMImpl)
	{
		BPCMImpl->FlushCompilationQueueImpl(nullptr, false, nullptr);
		BPCMImpl->FlushReinstancingQueueImpl();
	}
}

void FBlueprintCompilationManager::CompileSynchronously(const FBPCompileRequest& Request)
{
	if(BPCMImpl)
	{
		BPCMImpl->CompileSynchronouslyImpl(Request);
	}
}
	
void FBlueprintCompilationManager::NotifyBlueprintLoaded(UBlueprint* BPLoaded)
{
	// Blueprints can be loaded before editor modules are on line:
	if(!BPCMImpl)
	{
		FBlueprintCompilationManager::Initialize();
	}

	if(FBlueprintEditorUtils::IsCompileOnLoadDisabled(BPLoaded))
	{
		return;
	}

	check(BPLoaded->GetLinker());
	BPCMImpl->QueueForCompilation(FBPCompileRequest(BPLoaded, EBlueprintCompileOptions::IsRegeneratingOnLoad, nullptr));
}

void FBlueprintCompilationManager::QueueForCompilation(UBlueprint* BPLoaded)
{
	BPCMImpl->QueueForCompilation(FBPCompileRequest(BPLoaded, EBlueprintCompileOptions::None, nullptr));
}

bool FBlueprintCompilationManager::IsGeneratedClassLayoutReady()
{
	if(!BPCMImpl)
	{
		// legacy behavior: always assume generated class layout is good:
		return true;
	}
	return BPCMImpl->IsGeneratedClassLayoutReady();
}


bool FBlueprintCompilationManager::GetDefaultValue(const UClass* ForClass, const UProperty* Property, FString& OutDefaultValueAsString)
{
	if(!BPCMImpl)
	{
		// legacy behavior: can't provide CDO for classes currently being compiled
		return false;
	}

	BPCMImpl->GetDefaultValue(ForClass, Property, OutDefaultValueAsString);
	return true;
}

#undef LOCTEXT_NAMESPACE

