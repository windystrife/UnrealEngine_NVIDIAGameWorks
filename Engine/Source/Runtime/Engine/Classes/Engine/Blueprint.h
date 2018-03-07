// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"
#include "Misc/Guid.h"
#include "UObject/Class.h"
#include "Templates/SubclassOf.h"
#include "Engine/EngineTypes.h"
#include "EdGraph/EdGraphPin.h"
#include "Engine/BlueprintCore.h"
#include "UObject/SoftObjectPath.h"
#include "Blueprint.generated.h"

class FCompilerResultsLog;
class ITargetPlatform;
class UActorComponent;
class UEdGraph;
class UInheritableComponentHandler;

/**
 * Enumerates states a blueprint can be in.
 */
UENUM()
enum EBlueprintStatus
{
	/** Blueprint is in an unknown state. */
	BS_Unknown,
	/** Blueprint has been modified but not recompiled. */
	BS_Dirty,
	/** Blueprint tried but failed to be compiled. */
	BS_Error,
	/** Blueprint has been compiled since it was last modified. */
	BS_UpToDate,
	/** Blueprint is in the process of being created for the first time. */
	BS_BeingCreated,
	/** Blueprint has been compiled since it was last modified. There are warnings. */
	BS_UpToDateWithWarnings,
	BS_MAX,
};


/** Enumerates types of blueprints. */
UENUM()
enum EBlueprintType
{
	/** Normal blueprint. */
	BPTYPE_Normal				UMETA(DisplayName="Blueprint Class"),
	/** Blueprint that is const during execution (no state graph and methods cannot modify member variables). */
	BPTYPE_Const				UMETA(DisplayName="Const Blueprint Class"),
	/** Blueprint that serves as a container for macros to be used in other blueprints. */
	BPTYPE_MacroLibrary			UMETA(DisplayName="Blueprint Macro Library"),
	/** Blueprint that serves as an interface to be implemented by other blueprints. */
	BPTYPE_Interface			UMETA(DisplayName="Blueprint Interface"),
	/** Blueprint that handles level scripting. */
	BPTYPE_LevelScript			UMETA(DisplayName="Level Blueprint"),
	/** Blueprint that serves as a container for functions to be used in other blueprints. */
	BPTYPE_FunctionLibrary		UMETA(DisplayName="Blueprint Function Library"),

	BPTYPE_MAX,
};


/** Type of compilation. */
namespace EKismetCompileType
{
	enum Type
	{
		SkeletonOnly,
		Full,
		StubAfterFailure, 
		BytecodeOnly,
		Cpp,
	};
};

/** Compile modes. */
UENUM()
enum class EBlueprintCompileMode : uint8
{
	Default UMETA(DisplayName="Use Default", ToolTip="Use the default setting."),
	Development UMETA(ToolTip="Always compile in development mode (even when cooking)."),
	FinalRelease UMETA(ToolTip="Always compile in final release mode.")
};

USTRUCT()
struct FCompilerNativizationOptions
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FName PlatformName;

	UPROPERTY()
	bool ServerOnlyPlatform;

	UPROPERTY()
	bool ClientOnlyPlatform;

	UPROPERTY()
	TArray<FName> ExcludedModules;

	// Individually excluded assets
	UPROPERTY()
	TSet<FSoftObjectPath> ExcludedAssets;

	// Excluded folders. It excludes only BPGCs, enums and structures are still converted.
	UPROPERTY()
	TArray<FString> ExcludedFolderPaths;

	FCompilerNativizationOptions()
		: ServerOnlyPlatform(false)
		, ClientOnlyPlatform(false)
	{}
};

/** Cached 'cosmetic' information about a macro graph (this is transient and is computed at load) */
USTRUCT()
struct FBlueprintMacroCosmeticInfo
{
	GENERATED_BODY()

	// Does this macro contain one or more latent nodes?
	bool bContainsLatentNodes;

	FBlueprintMacroCosmeticInfo()
		: bContainsLatentNodes(false)
	{
	}
};

struct FKismetCompilerOptions
{
public:
	/** The compile type to perform (full compile, skeleton pass only, etc) */
	EKismetCompileType::Type	CompileType;

	/** Whether or not to save intermediate build products (temporary graphs and expanded macros) for debugging */
	bool bSaveIntermediateProducts;

	/** Whether to regenerate the skeleton first, when compiling on load we don't need to regenerate the skeleton. */
	bool bRegenerateSkelton;

	/** Whether or not this compile is for a duplicated blueprint */
	bool bIsDuplicationInstigated;

	/** Whether or not to reinstance and stub if the blueprint fails to compile */
	bool bReinstanceAndStubOnFailure;

	TSharedPtr<FString> OutHeaderSourceCode;
	TSharedPtr<FString> OutCppSourceCode;
	FCompilerNativizationOptions NativizationOptions;

	bool DoesRequireCppCodeGeneration() const
	{
		return (CompileType == EKismetCompileType::Cpp);
	}

	bool DoesRequireBytecodeGeneration() const
	{
		return (CompileType == EKismetCompileType::Full) 
			|| (CompileType == EKismetCompileType::BytecodeOnly) 
			|| (CompileType == EKismetCompileType::Cpp);
	}

	/** Whether or not this compile type should operate on the generated class of the blueprint, as opposed to just the skeleton */
	bool IsGeneratedClassCompileType() const
	{
		return (CompileType != EKismetCompileType::SkeletonOnly);
	}

	FKismetCompilerOptions()
		: CompileType(EKismetCompileType::Full)
		, bSaveIntermediateProducts(false)
		, bRegenerateSkelton(true)
		, bIsDuplicationInstigated(false)
		, bReinstanceAndStubOnFailure(true)
	{
	};
};


/** One metadata entry for a variable */
USTRUCT()
struct FBPVariableMetaDataEntry
{
	GENERATED_USTRUCT_BODY()

	/** Name of metadata key */
	UPROPERTY(EditAnywhere, Category=BPVariableMetaDataEntry)
	FName DataKey;

	/** Name of metadata value */
	UPROPERTY(EditAnywhere, Category=BPVariableMetaDataEntry)
	FString DataValue;



		FBPVariableMetaDataEntry() {}
		FBPVariableMetaDataEntry(const FName& InKey, const FString& InValue)
		: DataKey(InKey)
		, DataValue(InValue)
		{}
	
};


/** Struct indicating a variable in the generated class */
USTRUCT()
struct FBPVariableDescription
{
	GENERATED_USTRUCT_BODY()

	/** Name of the variable */
	UPROPERTY(EditAnywhere, Category=BPVariableDescription)
	FName VarName;

	/** A Guid that will remain constant even if the VarName changes */
	UPROPERTY()
	FGuid VarGuid;

	/** Type of the variable */
	UPROPERTY(EditAnywhere, Category=BPVariableDescription)
	struct FEdGraphPinType VarType;

	/** Friendly name of the variable */
	UPROPERTY(EditAnywhere, Category=BPVariableDescription)
	FString FriendlyName;

	/** Category this variable should be in */
	UPROPERTY(EditAnywhere, Category=BPVariableDescription)
	FText Category;

	/** Property flags for this variable - Changed from int32 to uint64*/
	UPROPERTY(EditAnywhere, Category=BPVariableDescription)
	uint64 PropertyFlags;

	UPROPERTY(EditAnywhere, Category=BPVariableRepNotify)
	FName RepNotifyFunc;

	UPROPERTY(EditAnywhere, Category=BPVariableDescription)
	TEnumAsByte<ELifetimeCondition> ReplicationCondition;

	/** Metadata information for this variable */
	UPROPERTY(EditAnywhere, Category=BPVariableDescription)
	TArray<struct FBPVariableMetaDataEntry> MetaDataArray;

	/** Optional new default value stored as string*/
	UPROPERTY(EditAnywhere, Category=BPVariableDescription)
	FString DefaultValue;

	FBPVariableDescription()
		: PropertyFlags(CPF_Edit)
	{
	}

	/** Set a metadata value on the variable */
	ENGINE_API void SetMetaData(const FName& Key, const FString& Value);
	/** Gets a metadata value on the variable; asserts if the value isn't present.  Check for validiy using FindMetaDataEntryIndexForKey. */
	ENGINE_API FString GetMetaData(const FName& Key) const;
	/** Clear metadata value on the variable */
	ENGINE_API void RemoveMetaData(const FName& Key);
	/** Find the index in the array of a metadata entry */
	ENGINE_API int32 FindMetaDataEntryIndexForKey(const FName& Key) const;
	/** Checks if there is metadata for a key */
	ENGINE_API bool HasMetaData(const FName& Key) const;
	
};


/** Struct containing information about what interfaces are implemented in this blueprint */
USTRUCT()
struct FBPInterfaceDescription
{
	GENERATED_USTRUCT_BODY()

	/** Reference to the interface class we're adding to this blueprint */
	UPROPERTY()
	TSubclassOf<class UInterface>  Interface;

	/** References to the graphs associated with the required functions for this interface */
	UPROPERTY()
	TArray<UEdGraph*> Graphs;


	FBPInterfaceDescription()
		: Interface(nullptr)
	{ }
};


USTRUCT()
struct FEditedDocumentInfo
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	UObject* EditedObject;

	/** Saved view position */
	UPROPERTY()
	FVector2D SavedViewOffset;

	/** Saved zoom amount */
	UPROPERTY()
	float SavedZoomAmount;

	FEditedDocumentInfo()
		: EditedObject(nullptr)
		, SavedViewOffset(0.0f, 0.0f)
		, SavedZoomAmount(-1.0f)
	{ }

	FEditedDocumentInfo(UObject* InEditedObject)
		: EditedObject(InEditedObject)
		, SavedViewOffset(0.0f, 0.0f)
		, SavedZoomAmount(-1.0f)
	{ }

	FEditedDocumentInfo(UObject* InEditedObject, FVector2D& InSavedViewOffset, float InSavedZoomAmount)
		: EditedObject(InEditedObject)
		, SavedViewOffset(InSavedViewOffset)
		, SavedZoomAmount(InSavedZoomAmount)
	{ }

	friend bool operator==( const FEditedDocumentInfo& LHS, const FEditedDocumentInfo& RHS )
	{
		return LHS.EditedObject == RHS.EditedObject && LHS.SavedViewOffset == RHS.SavedViewOffset && LHS.SavedZoomAmount == RHS.SavedZoomAmount;
	}
};


UENUM()
enum class EBlueprintNativizationFlag : uint8
{
	Disabled,
	Dependency, // conditionally enabled (set from sub-class as a dependency)
	ExplicitlyEnabled
};

/**
 * Blueprints are special assets that provide an intuitive, node-based interface that can be used to create new types of Actors
 * and script level events; giving designers and gameplay programmers the tools to quickly create and iterate gameplay from
 * within Unreal Editor without ever needing to write a line of code.
 */
UCLASS(config=Engine)
class ENGINE_API UBlueprint : public UBlueprintCore
{
	GENERATED_UCLASS_BODY()

	/** Whether or not this blueprint should recompile itself on load */
	UPROPERTY(config)
	uint32 bRecompileOnLoad:1;

	/** 
		Pointer to the parent class that the generated class should derive from. This *can* be null under rare circumstances, 
		one such case can be created by creating a blueprint (A) based on another blueprint (B), shutting down the editor, and
		deleting the parent blueprint.
	*/
	UPROPERTY(AssetRegistrySearchable)
	TSubclassOf<class UObject> ParentClass;

	UPROPERTY(transient)
	UObject* PRIVATE_InnermostPreviousCDO;

	/** When the class generated by this blueprint is loaded, it will be recompiled the first time.  After that initial recompile, subsequent loads will skip the regeneration step */
	UPROPERTY(transient)
	uint32 bHasBeenRegenerated:1;

	/** State flag to indicate whether or not the Blueprint is currently being regenerated on load */
	UPROPERTY(transient)
	uint32 bIsRegeneratingOnLoad:1;

#if WITH_EDITORONLY_DATA
	/** Whether or not this blueprint is newly created, and hasn't been opened in an editor yet */
	UPROPERTY(transient)
	uint32 bIsNewlyCreated : 1;

	/** Whether to force opening the full (non data-only) editor for this blueprint. */
	UPROPERTY(transient)
	uint32 bForceFullEditor : 1;

	UPROPERTY(transient)
	uint32 bQueuedForCompilation : 1 ;

	/**whether or not you want to continuously rerun the construction script for an actor as you drag it in the editor, or only when the drag operation is complete*/
	UPROPERTY(EditAnywhere, Category=BlueprintOptions)
	uint32 bRunConstructionScriptOnDrag : 1;

	/**whether or not you want to continuously rerun the construction script for an actor in sequencer*/
	UPROPERTY(EditAnywhere, Category=BlueprintOptions)
	uint32 bRunConstructionScriptInSequencer : 1;

	/** Whether or not this blueprint's class is a const class or not.  Should set CLASS_Const in the KismetCompiler. */
	UPROPERTY(EditAnywhere, Category=ClassOptions, AdvancedDisplay)
	uint32 bGenerateConstClass : 1;

	/** Whether or not this blueprint's class is a abstract class or not.  Should set CLASS_Abstract in the KismetCompiler. */
	UPROPERTY(EditAnywhere, Category = ClassOptions, AdvancedDisplay)
	uint32 bGenerateAbstractClass : 1;

	/**shows up in the content browser when the blueprint is hovered */
	UPROPERTY(EditAnywhere, Category=BlueprintOptions, meta=(MultiLine=true))
	FString BlueprintDescription;

	/** The category of the Blueprint, used to organize this Blueprint class when displayed in palette windows */
	UPROPERTY(EditAnywhere, Category=BlueprintOptions)
	FString BlueprintCategory;

	/** Additional HideCategories. These are added to HideCategories from parent. */
	UPROPERTY(EditAnywhere, Category=BlueprintOptions)
	TArray<FString> HideCategories;
	 
	/** When exclusive nativization is enabled, then this asset will be nativized. All super classes must be also nativized. */
	UPROPERTY(transient)
	EBlueprintNativizationFlag NativizationFlag;

	/** TRUE to show a warning when attempting to start in PIE and there is a compiler error on this Blueprint */
	UPROPERTY(transient)
	bool bDisplayCompilePIEWarning;

	/** Guid key for finding searchable data for Blueprint in the DDC */
	UPROPERTY()
	FGuid SearchGuid;

	/** Deprecates the Blueprint, marking the generated class with the CLASS_Deprecated flag */
	UPROPERTY(EditAnywhere, Category=ClassOptions, AdvancedDisplay)
	bool bDeprecate;

	/** The mode that will be used when compiling this class. */
	UPROPERTY(EditAnywhere, Category=ClassOptions, AdvancedDisplay)
	EBlueprintCompileMode CompileMode;
#endif //WITH_EDITORONLY_DATA

	/** 'Simple' construction script - graph of components to instance */
	UPROPERTY()
	class USimpleConstructionScript* SimpleConstructionScript;

#if WITH_EDITORONLY_DATA
	/** Set of pages that combine into a single uber-graph */
	UPROPERTY()
	TArray<UEdGraph*> UbergraphPages;

	/** Set of functions implemented for this class graphically */
	UPROPERTY()
	TArray<UEdGraph*> FunctionGraphs;

	/** Graphs of signatures for delegates */
	UPROPERTY()
	TArray<UEdGraph*> DelegateSignatureGraphs;

	/** Set of macros implemented for this class */
	UPROPERTY()
	TArray<UEdGraph*> MacroGraphs;

	/** Set of functions actually compiled for this class */
	UPROPERTY(transient, duplicatetransient)
	TArray<UEdGraph*> IntermediateGeneratedGraphs;

	/** Set of functions actually compiled for this class */
	UPROPERTY(transient, duplicatetransient)
	TArray<UEdGraph*> EventGraphs;

	/** Cached cosmetic information about macro graphs, use GetCosmeticInfoForMacro() to access */
	UPROPERTY(Transient)
	TMap<UEdGraph*, FBlueprintMacroCosmeticInfo> PRIVATE_CachedMacroInfo;

	/** 
	 * Flag indicating that a read only duplicate of this blueprint is being created, used to disable logic in ::PostDuplicate,
	 *
	 * This flag needs to be copied on duplication (because it's the duplicated object that we're disabling on PostDuplicate),
	 * but we don't *need* to serialize it for permanent objects.
	 *
	 * Without setting this flag a blueprint will be marked dirty when it is duplicated and if saved while in this dirty
	 * state you will not be able to open the blueprint. More specifically, UClass::Rename (called by DestroyGeneratedClass)
	 * sets a dirty flag on the package. Once saved the package will fail to open because some unnamed objects are present in
	 * the pacakge.
	 *
	 * This flag can be used to avoid the package being marked as dirty in the first place. Ideally PostDuplicateObject
	 * would not rename classes that are still in use by the original object.
	 */
	UPROPERTY()
	mutable bool bDuplicatingReadOnly;
#endif // WITH_EDITORONLY_DATA

	/** Array of component template objects, used by AddComponent function */
	UPROPERTY()
	TArray<class UActorComponent*> ComponentTemplates;

	/** Array of templates for timelines that should be created */
	UPROPERTY()
	TArray<class UTimelineTemplate*> Timelines;

	/** Stores data to override (in children classes) components (created by SCS) from parent classes */
	UPROPERTY()
	class UInheritableComponentHandler* InheritableComponentHandler;

	/** The type of this blueprint */
	UPROPERTY(AssetRegistrySearchable)
	TEnumAsByte<enum EBlueprintType> BlueprintType;

#if WITH_EDITORONLY_DATA
	/** The current status of this blueprint */
	UPROPERTY(transient)
	TEnumAsByte<enum EBlueprintStatus> Status;

	/** Array of new variables to be added to generated class */
	UPROPERTY()
	TArray<struct FBPVariableDescription> NewVariables;

	/** Array of user sorted categories */
	UPROPERTY()
	TArray<FName> CategorySorting;

	/** Array of info about the interfaces we implement in this blueprint */
	UPROPERTY(AssetRegistrySearchable)
	TArray<struct FBPInterfaceDescription> ImplementedInterfaces;
	
#endif // WITH_EDITORONLY_DATA

	/** The version of the blueprint system that was used to  create this blueprint */
	UPROPERTY()
	int32 BlueprintSystemVersion;

#if WITH_EDITORONLY_DATA
	/** Set of documents that were being edited in this blueprint, so we can open them right away */
	UPROPERTY()
	TArray<struct FEditedDocumentInfo> LastEditedDocuments;

	/** Persistent debugging options */
	UPROPERTY()
	TArray<class UBreakpoint*> Breakpoints;

	UPROPERTY()
	TArray<FEdGraphPinReference> WatchedPins;

	UPROPERTY()
	TArray<class UEdGraphPin_Deprecated*> DeprecatedPinWatches;

	/** Index map for component template names */
	UPROPERTY()
	TMap<FName, int32> ComponentTemplateNameIndex;

	/** Maps old to new component template names */
	UPROPERTY(transient)
	TMap<FName, FName> OldToNewComponentTemplateNames;
#endif // WITH_EDITORONLY_DATA

public:

	/** Broadcasts a notification whenever the blueprint has changed. */
	DECLARE_EVENT_OneParam( UBlueprint, FChangedEvent, class UBlueprint* );
	FChangedEvent& OnChanged() { return ChangedEvent; }

	/**	This should NOT be public */
	void BroadcastChanged() { ChangedEvent.Broadcast(this); }

	/** Broadcasts a notification whenever the blueprint has changed. */
	DECLARE_EVENT_OneParam(UBlueprint, FCompiledEvent, class UBlueprint*);
	FCompiledEvent& OnCompiled() { return CompiledEvent; }
	void BroadcastCompiled() { CompiledEvent.Broadcast(this); }

	/** Whether or not this blueprint can be considered for a bytecode only compile */
	virtual bool IsValidForBytecodeOnlyRecompile() const { return true; }

#if WITH_EDITORONLY_DATA
	/** Delegate called when the debug object is set */
	DECLARE_EVENT_OneParam(UBlueprint, FOnSetObjectBeingDebugged, UObject* /*InDebugObj*/);
	FOnSetObjectBeingDebugged& OnSetObjectBeingDebugged() { return OnSetObjectBeingDebuggedDelegate; }

protected:
	/** Current object being debugged for this blueprint */
	TWeakObjectPtr< class UObject > CurrentObjectBeingDebugged;

	/** Current world being debugged for this blueprint */
	TWeakObjectPtr< class UWorld > CurrentWorldBeingDebugged;

	/** Delegate called when the debug object is set */
	FOnSetObjectBeingDebugged OnSetObjectBeingDebuggedDelegate;

public:

	/** Information for thumbnail rendering */
	UPROPERTY(VisibleAnywhere, Instanced, Category=Thumbnail)
	class UThumbnailInfo* ThumbnailInfo;

	/** The blueprint is currently compiled */
	UPROPERTY(transient)
	uint32 bBeingCompiled:1;

	/** CRC for CDO calculated right after the latest compilation used by Reinstancer to check if default values were changed */
	UPROPERTY(transient, duplicatetransient)
	uint32 CrcLastCompiledCDO;

	UPROPERTY(transient, duplicatetransient)
	uint32 CrcLastCompiledSignature;

	bool bCachedDependenciesUpToDate;
	TSet<TWeakObjectPtr<UBlueprint>> CachedDependencies;

	// User Defined Structures, the blueprint depends on
	TSet<TWeakObjectPtr<UStruct>> CachedUDSDependencies;

	enum class EIsBPNonReducible : uint8
	{
		Unkown,
		Yes,
		No,
	};

	// Cached information if the BP contains any non-reducible functions (that can benefit from nativization).
	EIsBPNonReducible bHasAnyNonReducibleFunction;

	// If this BP is just a duplicate created for a specific compilation, the reference to original GeneratedClass is needed
	UPROPERTY(transient, duplicatetransient)
	UClass* OriginalClass;

	bool IsUpToDate() const
	{
		return BS_UpToDate == Status || BS_UpToDateWithWarnings == Status;
	}

	bool IsPossiblyDirty() const
	{
		return (BS_Dirty == Status) || (BS_Unknown == Status);
	}
#endif // WITH_EDITORONLY_DATA

#if WITH_EDITOR
	static bool ForceLoad(UObject* Obj);

	static void ForceLoadMembers(UObject* InObject);

	static void ForceLoadMetaData(UObject* InObject);

	static bool ValidateGeneratedClass(const UClass* InClass);

	/** Find the object in the TemplateObjects array with the supplied name */
	UActorComponent* FindTemplateByName(const FName& TemplateName) const;

	/** Rename the component template in the TemplateObjects array with the supplied name */
	bool RenameComponentTemplate(const FName& OldTemplateName, const FName& NewTemplateName);

	/** Find a timeline by name */
	class UTimelineTemplate* FindTimelineTemplateByVariableName(const FName& TimelineName);	

	/** Find a timeline by name */
	const class UTimelineTemplate* FindTimelineTemplateByVariableName(const FName& TimelineName) const;	

	void GetBlueprintClassNames(FName& GeneratedClassName, FName& SkeletonClassName, FName NameOverride = NAME_None) const
	{
		FName NameToUse = (NameOverride != NAME_None) ? NameOverride : GetFName();

		const FString GeneratedClassNameString = FString::Printf(TEXT("%s_C"), *NameToUse.ToString());
		GeneratedClassName = FName(*GeneratedClassNameString);

		const FString SkeletonClassNameString = FString::Printf(TEXT("SKEL_%s_C"), *NameToUse.ToString());
		SkeletonClassName = FName(*SkeletonClassNameString);
	}
	
	void GetBlueprintCDONames(FName& GeneratedClassName, FName& SkeletonClassName, FName NameOverride = NAME_None) const
	{
		FName NameToUse = (NameOverride != NAME_None) ? NameOverride : GetFName();

		const FString GeneratedClassNameString = FString::Printf(TEXT("Default__%s_C"), *NameToUse.ToString());
		GeneratedClassName = FName(*GeneratedClassNameString);

		const FString SkeletonClassNameString = FString::Printf(TEXT("Default__SKEL_%s_C"), *NameToUse.ToString());
		SkeletonClassName = FName(*SkeletonClassNameString);
	}

	/** Gets the class generated when this blueprint is compiled. */
	virtual UClass* GetBlueprintClass() const;

	// Should the generic blueprint factory work for this blueprint?
	virtual bool SupportedByDefaultBlueprintFactory() const
	{
		return true;
	}

	/** Sets the current object being debugged */
	virtual void SetObjectBeingDebugged(UObject* NewObject);

	virtual void SetWorldBeingDebugged(UWorld* NewWorld);

	virtual void GetReparentingRules(TSet< const UClass* >& AllowedChildrenOfClasses, TSet< const UClass* >& DisallowedChildrenOfClasses) const;

	/**
	* Allows derived blueprints to require compilation on load, otherwise they may get treated as data only and not compiled on load.
	*/
	virtual bool AlwaysCompileOnLoad() const { return false; }

	/** Some Blueprints (and classes) can recompile while we are debugging a live session. This function controls whether this can occur. */
	virtual bool CanRecompileWhilePlayingInEditor() const { return false; }

private:

	/** Sets the current object being debugged */
	void DebuggingWorldRegistrationHelper(UObject* ObjectProvidingWorld, UObject* ValueToRegister);
	
public:

	/** @return the current object being debugged, which can be nullptr */
	virtual UObject* GetObjectBeingDebugged();

	virtual class UWorld* GetWorldBeingDebugged();

	/** Renames only the generated classes. Should only be used internally or when testing for rename. */
	virtual bool RenameGeneratedClasses(const TCHAR* NewName = nullptr, UObject* NewOuter = nullptr, ERenameFlags Flags = REN_None);

	//~ Begin UObject Interface (WITH_EDITOR)
	virtual void PostDuplicate(bool bDuplicateForPIE) override;
	virtual bool Rename(const TCHAR* NewName = nullptr, UObject* NewOuter = nullptr, ERenameFlags Flags = REN_None) override;
	virtual UClass* RegenerateClass(UClass* ClassToRegenerate, UObject* PreviousCDO, TArray<UObject*>& ObjLoaded) override;
	virtual void PostLoad() override;
	virtual void PostLoadSubobjects( FObjectInstancingGraph* OuterInstanceGraph ) override;
	virtual bool Modify(bool bAlwaysMarkDirty = true) override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	virtual void BeginCacheForCookedPlatformData(const ITargetPlatform *TargetPlatform) override;
	virtual bool IsCachedCookedPlatformDataLoaded(const ITargetPlatform* TargetPlatform) override;
	virtual void ClearAllCachedCookedPlatformData() override;
	//~ End UObject Interface

	/** Consigns the GeneratedClass and the SkeletonGeneratedClass to oblivion, and nulls their references */
	void RemoveGeneratedClasses();

	/** @return the user-friendly name of the blueprint */
	virtual FString GetFriendlyName() const;

	/** @return true if the blueprint supports event binding for multicast delegates */
	virtual bool AllowsDynamicBinding() const;

	/** @return true if the blueprint supports event binding for input events */
	virtual bool SupportsInputEvents() const;

	bool ChangeOwnerOfTemplates();

	UInheritableComponentHandler* GetInheritableComponentHandler(bool bCreateIfNecessary);

	/** Collect blueprints that depend on this blueprint. */
	virtual void GatherDependencies(TSet<TWeakObjectPtr<UBlueprint>>& InDependencies) const;

	virtual void ReplaceDeprecatedNodes();

#endif	//#if WITH_EDITOR

	//~ Begin UObject Interface
#if WITH_EDITORONLY_DATA
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
#endif // WITH_EDITORONLY_DATA
	virtual void Serialize(FArchive& Ar) override;
	virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;
	virtual FString GetDesc(void) override;
	virtual void TagSubobjects(EObjectFlags NewFlags) override;
	virtual bool NeedsLoadForClient() const override;
	virtual bool NeedsLoadForServer() const override;
	virtual bool NeedsLoadForEditorGame() const override;
	//~ End UObject Interface

	/** Get the Blueprint object that generated the supplied class */
	static UBlueprint* GetBlueprintFromClass(const UClass* InClass);

	/** 
	 * Gets an array of all blueprints used to generate this class and its parents.  0th elements is the BP used to generate InClass
	 *
	 * @param InClass				The class to get the blueprint lineage for
	 * @param OutBlueprintParents	Array with the blueprints used to generate this class and its parents.  0th = this, Nth = least derived BP-based parent
	 * @return						true if there were no status errors in any of the parent blueprints, otherwise false
	 */
	static bool GetBlueprintHierarchyFromClass(const UClass* InClass, TArray<UBlueprint*>& OutBlueprintParents);

#if WITH_EDITOR
	template<class TFieldType>
	static FName GetFieldNameFromClassByGuid(const UClass* InClass, const FGuid VarGuid)
	{
		UProperty* AssertPropertyType = (TFieldType*)0;

		TArray<UBlueprint*> Blueprints;
		UBlueprint::GetBlueprintHierarchyFromClass(InClass, Blueprints);

		for (int32 BPIndex = 0; BPIndex < Blueprints.Num(); ++BPIndex)
		{
			UBlueprint* Blueprint = Blueprints[BPIndex];
			for (int32 VarIndex = 0; VarIndex < Blueprint->NewVariables.Num(); ++VarIndex)
			{
				const FBPVariableDescription& BPVarDesc = Blueprint->NewVariables[VarIndex];
				if (BPVarDesc.VarGuid == VarGuid)
				{
					return BPVarDesc.VarName;
				}
			}
		}

		return NAME_None;
	}

	template<class TFieldType>
	static bool GetGuidFromClassByFieldName(const UClass* InClass, const FName VarName, FGuid& VarGuid)
	{
		UProperty* AssertPropertyType = (TFieldType*)0;

		TArray<UBlueprint*> Blueprints;
		UBlueprint::GetBlueprintHierarchyFromClass(InClass, Blueprints);

		for (int32 BPIndex = 0; BPIndex < Blueprints.Num(); ++BPIndex)
		{
			UBlueprint* Blueprint = Blueprints[BPIndex];
			for (int32 VarIndex = 0; VarIndex < Blueprint->NewVariables.Num(); ++VarIndex)
			{
				const FBPVariableDescription& BPVarDesc = Blueprint->NewVariables[VarIndex];
				if (BPVarDesc.VarName == VarName)
				{
					VarGuid = BPVarDesc.VarGuid;
					return true;
				}
			}
		}

		return false;
	}

	static FName GetFunctionNameFromClassByGuid(const UClass* InClass, const FGuid FunctionGuid);
	static bool GetFunctionGuidFromClassByFieldName(const UClass* InClass, const FName FunctionName, FGuid& FunctionGuid);

	/**
	 * Gets the last edited uber graph.  If no graph was found in the last edited document set, the first
	 * ubergraph is returned.  If there are no ubergraphs nullptr is returned.
	 */
	UEdGraph* GetLastEditedUberGraph() const;
#endif

	/** Find a function given its name and optionally an object property name within this Blueprint */
	ETimelineSigType GetTimelineSignatureForFunctionByName(const FName& FunctionName, const FName& ObjectPropertyName);

	/** Gets the current blueprint system version. Note- incrementing this version will invalidate ALL existing blueprints! */
	static int32 GetCurrentBlueprintSystemVersion()
	{
		return 2;
	}

	/** Get all graphs in this blueprint */
	void GetAllGraphs(TArray<UEdGraph*>& Graphs) const;

private:

	/** Broadcasts a notification whenever the blueprint has changed. */
	FChangedEvent ChangedEvent;

	/** Broadcasts a notification whenever the blueprint is compiled. */
	FCompiledEvent CompiledEvent;

	/** Deprecated properties. */
	UPROPERTY()
	bool bNativize_DEPRECATED;

#if WITH_EDITOR
public:
	/** If this blueprint is currently being compiled, the CurrentMessageLog will be the log currently being used to send logs to. */
	class FCompilerResultsLog* CurrentMessageLog;

	/** Message log for storing upgrade notes that were generated within the Blueprint, will be displayed to the compiler results each compiler and will remain until saving */
	TSharedPtr<class FCompilerResultsLog> UpgradeNotesLog;

	/** Message log for storing pre-compile errors/notes/warnings that will only last until the next Blueprint compile */
	TSharedPtr<class FCompilerResultsLog> PreCompileLog;

	/** 
	 * Sends a message to the CurrentMessageLog, if there is one available.  Otherwise, defaults to logging to the normal channels.
	 * Should use this for node and blueprint actions that happen during compilation!
	 */
	void Message_Note(const FString& MessageToLog);
	void Message_Warn(const FString& MessageToLog);
	void Message_Error(const FString& MessageToLog);
#endif

};


#if WITH_EDITOR
template<>
inline FName UBlueprint::GetFieldNameFromClassByGuid<UFunction>(const UClass* InClass, const FGuid FunctionGuid)
{
	return GetFunctionNameFromClassByGuid(InClass, FunctionGuid);
}

template<>
inline bool UBlueprint::GetGuidFromClassByFieldName<UFunction>(const UClass* InClass, const FName FunctionName, FGuid& FunctionGuid)
{
	return GetFunctionGuidFromClassByFieldName(InClass, FunctionName, FunctionGuid);
}

#endif // #if WITH_EDITOR
