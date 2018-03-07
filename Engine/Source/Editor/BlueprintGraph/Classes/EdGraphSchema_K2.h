// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/EnumClassFlags.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Class.h"
#include "UObject/SoftObjectPath.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "AssetData.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.generated.h"

class AActor;
class FMenuBuilder;
class UBlueprint;
class UK2Node;
struct FTypesDatabase;
class UEnum;
class UClass;
class UScriptStruct;

/** Reference to an structure (only used in 'docked' palette) */
USTRUCT()
struct BLUEPRINTGRAPH_API FEdGraphSchemaAction_K2Struct : public FEdGraphSchemaAction
{
	GENERATED_USTRUCT_BODY()

	// Simple type info
	static FName StaticGetTypeId() {static FName Type("FEdGraphSchemaAction_K2Struct"); return Type;}
	virtual FName GetTypeId() const override { return StaticGetTypeId(); } 

	UStruct* Struct;

	void AddReferencedObjects( FReferenceCollector& Collector ) override
	{
		if( Struct )
		{
			Collector.AddReferencedObject(Struct);
		}
	}

	FName GetPathName() const
	{
		return Struct ? FName(*Struct->GetPathName()) : NAME_None;
	}

	FEdGraphSchemaAction_K2Struct() 
		: FEdGraphSchemaAction()
		, Struct(nullptr)
	{}

	FEdGraphSchemaAction_K2Struct(FText InNodeCategory, FText InMenuDesc, FText InToolTip, const int32 InGrouping)
		: FEdGraphSchemaAction(MoveTemp(InNodeCategory), MoveTemp(InMenuDesc), MoveTemp(InToolTip), InGrouping)
		, Struct(nullptr)
	{}
};

// Constants used for metadata, etc... in blueprints
struct BLUEPRINTGRAPH_API FBlueprintMetadata
{
public:
	// Struct/Enum/Class:
	// If true, this class, struct, or enum is a valid type for use as a variable in a blueprint
	static const FName MD_AllowableBlueprintVariableType;

	// If true, this class, struct, or enum is not valid for use as a variable in a blueprint
	static const FName MD_NotAllowableBlueprintVariableType;

	// Class:
	// [ClassMetadata] If present, the component class can be spawned by a blueprint
	static const FName MD_BlueprintSpawnableComponent;

	/** If true, the class will be usable as a base for blueprints */
	static const FName MD_IsBlueprintBase;
	
	/** A listing of classes that this class is accessible from (and only those classes, if present).  Note that this determines the GRAPH CONTEXTS in which the node cannot be placed (e.g. right click menu, palette), and does NOT control menus when dragging off of a context pin (i.e. contextual drag) */
	static const FName MD_RestrictedToClasses;

	/// [ClassMetadata] Used for Actor and Component classes. If the native class cannot tick, Blueprint generated classes based this Actor or Component can have bCanEverTick flag overridden even if bCanBlueprintsTickByDefault is false.
	static const FName MD_ChildCanTick;

	/// [ClassMetadata] Used for Actor and Component classes. If the native class cannot tick, Blueprint generated classes based this Actor or Component can never tick even if bCanBlueprintsTickByDefault is true.
	static const FName MD_ChildCannotTick;

	/// [ClassMetadata] Used to make the first subclass of a class ignore all inherited showCategories and hideCategories commands
	static const FName MD_IgnoreCategoryKeywordsInSubclasses;

	//    function metadata
	/** Specifies a UFUNCTION as Kismet protected, which can only be called from itself */
	static const FName MD_Protected;

	/** Marks a UFUNCTION as latent execution */
	static const FName MD_Latent;

	/** Marks a UFUNCTION as unsafe for use in the UCS, which prevents it from being called from the UCS.  Useful for things that spawn actors, etc that should never happen in the UCS */
	static const FName MD_UnsafeForConstructionScripts;

	// The category that a function appears under in the palette
	static const FName MD_FunctionCategory;

	// [FunctionMetadata] Indicates that the function is deprecated
	static const FName MD_DeprecatedFunction;

	// [FunctionMetadata] Supplies the custom message to use for deprecation
	static const FName MD_DeprecationMessage;

	// [FunctionMetadata] Indicates that the function should be drawn as a compact node with the specified body title
	static const FName MD_CompactNodeTitle;

	// [FunctionMetadata] Indicates that the function should be drawn with this title over the function name
	static const FName MD_DisplayName;

	// [FunctionMetadata] Indicates that a particular function parameter is for internal use only, which means it will be both hidden and not connectible.
	static const FName MD_InternalUseParam;

	//    property metadata

	/** UPROPERTY will be exposed on "Spawn Blueprint" nodes as an input  */
	static const FName MD_ExposeOnSpawn;

	/** UPROPERTY uses the specified function as a getter rather than reading from the property directly */
	static const FName MD_PropertyGetFunction;

	/** UPROPERTY uses the specified function as a setter rather than writing to the property directly */
	static const FName MD_PropertySetFunction;

	/** UPROPERTY cannot be modified by other blueprints */
	static const FName MD_Private;

	/** If true, the self pin should not be shown or connectable regardless of purity, const, etc. similar to InternalUseParam */
	static const FName MD_HideSelfPin;

	/** If true, the specified UObject parameter will default to "self" if nothing is connected */
	static const FName MD_DefaultToSelf;

	/** The specified parameter should be used as the context object when retrieving a UWorld pointer (implies hidden and default-to-self) */
	static const FName MD_WorldContext;

	/** For functions that have the MD_WorldContext metadata but are safe to be called from contexts that do not have the ability to provide the world context (either through GetWorld() or ShowWorldContextPin class metadata */
	static const FName MD_CallableWithoutWorldContext;

	/** For functions that should be compiled in development mode only */
	static const FName MD_DevelopmentOnly;

	/** If true, an unconnected pin will generate a UPROPERTY under the hood to connect as the input, which will be set to the literal value for the pin.  Only valid for reference parameters. */
	static const FName MD_AutoCreateRefTerm;

	/** If true, the hidden world context pin will be visible when the function is placed in a child blueprint of the class. */
	static const FName MD_ShowWorldContextPin;

	static const FName MD_BlueprintInternalUseOnly;
	static const FName MD_NeedsLatentFixup;

	static const FName MD_LatentCallbackTarget;

	/** If true, properties defined in the C++ private scope will be accessible to blueprints */
	static const FName MD_AllowPrivateAccess;

	/** Categories of functions to expose on this property */
	static const FName MD_ExposeFunctionCategories;

	// [InterfaceMetadata]
	static const FName MD_CannotImplementInterfaceInBlueprint;
	static const FName MD_ProhibitedInterfaces;

	/** Keywords used when searching for functions */
	static const FName MD_FunctionKeywords;

	/** Indicates that during compile we want to create multiple exec pins from an enum param */
	static const FName MD_ExpandEnumAsExecs;

	static const FName MD_CommutativeAssociativeBinaryOperator;

	/** Metadata string that indicates to use the MaterialParameterCollectionFunction node. */
	static const FName MD_MaterialParameterCollectionFunction;

	/** Metadata string that sets the tooltip */
	static const FName MD_Tooltip;

	/** Metadata string that indicates the specified event can be triggered in editor */
	static const FName MD_CallInEditor;

	/** Metadata to identify an DataTable Pin. Depending on which DataTable is selected, we display different RowName options */
	static const FName MD_DataTablePin;

	/** Metadata that flags make/break functions for specific struct types. */
	static const FName MD_NativeMakeFunction;
	static const FName MD_NativeBreakFunction;

	/** Metadata that flags function params that govern what type of object the function returns */
	static const FName MD_DynamicOutputType;
	/** Metadata that flags the function output param that will be controlled by the "MD_DynamicOutputType" pin */
	static const FName MD_DynamicOutputParam;

	static const FName MD_ArrayParam;
	static const FName MD_ArrayDependentParam;

	/** Metadata that flags TSet parameters that will have their type determined at blueprint compile time */
	static const FName MD_SetParam;

	/** Metadata that flags TMap function parameters that will have their type determined at blueprint compile time */
	static const FName MD_MapParam;
	static const FName MD_MapKeyParam;
	static const FName MD_MapValueParam;

	/** Metadata that identifies an integral property as a bitmask. */
	static const FName MD_Bitmask;
	/** Metadata that associates a bitmask property with a bitflag enum. */
	static const FName MD_BitmaskEnum;
	/** Metadata that identifies an enum as a set of explicitly-named bitflags. */
	static const FName MD_Bitflags;
	/** Metadata that signals to the editor that enum values correspond to mask values instead of bitshift (index) values. */
	static const FName MD_UseEnumValuesAsMaskValuesInEditor;
	
private:
	// This class should never be instantiated
	FBlueprintMetadata() {}
};

USTRUCT()
// Structure used to automatically convert blueprintcallable functions (that have blueprint parameter) calls (in bp graph) 
// into their never versions (with class param instead of blueprint).
struct FBlueprintCallableFunctionRedirect
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	FString ClassName;

	UPROPERTY()
	FString OldFunctionName;

	UPROPERTY()
	FString NewFunctionName;

	UPROPERTY()
	FString BlueprintParamName;

	UPROPERTY()
	FString ClassParamName;
};

enum class EObjectReferenceType : uint8
{
	NotAnObject		= 0x00,
	ObjectReference = 0x01,
	ClassReference	= 0x02,
	SoftObject		= 0x04,
	SoftClass		= 0x08,
	AllTypes		= 0x0f,
};

/**
* Filter flags for GetVariableTypeTree
*/
enum class ETypeTreeFilter : uint8
{
	None			= 0x00, // No Exec or Wildcards
	AllowExec		= 0x01, // Include Executable pins
	AllowWildcard	= 0x02, // Include Wildcard pins
	IndexTypesOnly	= 0x04, // Exclude all pins that aren't index types
	RootTypesOnly	= 0x08	// Exclude all pins that aren't root types
};

ENUM_CLASS_FLAGS(ETypeTreeFilter);

struct FTypesDatabase;

UCLASS(config=Editor)
class BLUEPRINTGRAPH_API UEdGraphSchema_K2 : public UEdGraphSchema
{
	GENERATED_UCLASS_BODY()

	// Allowable PinType.PinCategory values
	static const FString PC_Exec;
	static const FString PC_Boolean;
	static const FString PC_Byte;
	static const FString PC_Class;    // SubCategoryObject is the MetaClass of the Class passed thru this pin, or SubCategory can be 'self'. The DefaultValue string should always be empty, use DefaultObject.
	static const FString PC_SoftClass;
	static const FString PC_Int;
	static const FString PC_Float;
	static const FString PC_Name;
	static const FString PC_Delegate;    // SubCategoryObject is the UFunction of the delegate signature
	static const FString PC_MCDelegate;  // SubCategoryObject is the UFunction of the delegate signature
	static const FString PC_Object;    // SubCategoryObject is the Class of the object passed thru this pin, or SubCategory can be 'self'. The DefaultValue string should always be empty, use DefaultObject.
	static const FString PC_Interface;	// SubCategoryObject is the Class of the object passed thru this pin.
	static const FString PC_SoftObject;		// SubCategoryObject is the Class of the SoftObjectPtr passed thru this pin.
	static const FString PC_String;
	static const FString PC_Text;
	static const FString PC_Struct;    // SubCategoryObject is the ScriptStruct of the struct passed thru this pin, 'self' is not a valid SubCategory. DefaultObject should always be empty, the DefaultValue string may be used for supported structs.
	static const FString PC_Wildcard;    // Special matching rules are imposed by the node itself
	static const FString PC_Enum;    // SubCategoryObject is the UEnum object passed thru this pin

	// Common PinType.PinSubCategory values
	static const FString PSC_Self;    // Category=PC_Object or PC_Class, indicates the class being compiled

	static const FString PSC_Index;	// Category=PC_Wildcard, indicates the wildcard will only accept Int, Bool, Byte and Enum pins (used when a pin represents indexing a list)
	static const FString PSC_Bitmask;	// Category=PC_Byte or PC_Int, indicates that the pin represents a bitmask field. SubCategoryObject is either NULL or the UEnum object to which the bitmap is linked for bitflag name specification.

	// Pin names that have special meaning and required types in some contexts (depending on the node type)
	static const FString PN_Execute;    // Category=PC_Exec, singleton, input
	static const FString PN_Then;    // Category=PC_Exec, singleton, output
	static const FString PN_Completed;    // Category=PC_Exec, singleton, output
	static const FString PN_DelegateEntry;    // Category=PC_Exec, singleton, output; entry point for a dynamically bound delegate
	static const FString PN_EntryPoint;	// entry point to the ubergraph
	static const FString PN_Self;    // Category=PC_Object, singleton, input
	static const FString PN_Else;    // Category=PC_Exec, singleton, output
	static const FString PN_Loop;    // Category=PC_Exec, singleton, output
	static const FString PN_After;    // Category=PC_Exec, singleton, output
	static const FString PN_ReturnValue;		// Category=PC_Object, singleton, output
	static const FString PN_ObjectToCast;    // Category=PC_Object, singleton, input
	static const FString PN_Condition;    // Category=PC_Boolean, singleton, input
	static const FString PN_Start;    // Category=PC_Int, singleton, input
	static const FString PN_Stop;    // Category=PC_Int, singleton, input
	static const FString PN_Index;    // Category=PC_Int, singleton, output
	static const FString PN_Item;    // Category=PC_Int, singleton, output
	static const FString PN_CastSucceeded;    // Category=PC_Exec, singleton, output
	static const FString PN_CastFailed;    // Category=PC_Exec, singleton, output
	static const FString PN_CastedValuePrefix;    // Category=PC_Object, singleton, output; actual pin name varies depending on the type to be casted to, this is just a prefix
	static const FString PN_MatineeFinished;    // Category=PC_Exec, singleton, output

	// construction script function names
	static const FName FN_UserConstructionScript;
	static const FName FN_ExecuteUbergraphBase;

	// metadata keys

	//   class metadata


	// graph names
	static const FName GN_EventGraph;
	static const FName GN_AnimGraph;

	// variable names
	static const FText VR_DefaultCategory;

	// action grouping values
	static const int32 AG_LevelReference;

	// Somewhat hacky mechanism to prevent tooltips created for pins from including the display name and type when generating BP API documentation
	static bool bGeneratingDocumentation;

	// ID for checking dirty status of node titles against, increases every compile
	static int32 CurrentCacheRefreshID;

	// Pin Selector category for all object types
	static const FString AllObjectTypes;

	UPROPERTY(globalconfig)
	TArray<FBlueprintCallableFunctionRedirect> EditoronlyBPFunctionRedirects;

public:

	//////////////////////////////////////////////////////////////////////////
	// FPinTypeInfo
	/** Class used for creating type tree selection info, which aggregates the various PC_* and PinSubtypes in the schema into a heirarchy */
	class BLUEPRINTGRAPH_API FPinTypeTreeInfo
	{
	private:
		/** The pin type corresponding to the schema type */
		FEdGraphPinType PinType;
		uint8 PossibleObjectReferenceTypes;

		/** Asset Reference, used when PinType.PinSubCategoryObject is not loaded yet */
		FSoftObjectPath SubCategoryObjectAssetReference;

		FText CachedDescription;

	public:
		/** The children of this pin type */
		TArray< TSharedPtr<FPinTypeTreeInfo> > Children;

		/** Whether or not this pin type is selectable as an actual type, or is just a category, with some subtypes */
		bool bReadOnly;

		/** Friendly display name of pin type; also used to see if it has subtypes */
		FText FriendlyName;

		/** Text for regular tooltip */
		FText Tooltip;

	public:
		const FEdGraphPinType& GetPinType(bool bForceLoadedSubCategoryObject);
		void SetPinSubTypeCategory(const FString& SubCategory)
		{
			PinType.PinSubCategory = SubCategory;
		}

		FPinTypeTreeInfo(const FText& InFriendlyName, const FString& CategoryName, const UEdGraphSchema_K2* Schema, const FText& InTooltip, bool bInReadOnly = false, FTypesDatabase* TypesDatabase = nullptr);
		FPinTypeTreeInfo(const FString& CategoryName, UObject* SubCategoryObject, const FText& InTooltip, bool bInReadOnly = false, uint8 InPossibleObjectReferenceTypes = 0);
		FPinTypeTreeInfo(const FText& InFriendlyName, const FString& CategoryName, const FSoftObjectPath& SubCategoryObject, const FText& InTooltip, bool bInReadOnly = false, uint8 InPossibleObjectReferenceTypes = 0);

		FPinTypeTreeInfo(TSharedPtr<FPinTypeTreeInfo> InInfo)
		{
			PinType = InInfo->PinType;
			bReadOnly = InInfo->bReadOnly;
			FriendlyName = InInfo->FriendlyName;
			Tooltip = InInfo->Tooltip;
			SubCategoryObjectAssetReference = InInfo->SubCategoryObjectAssetReference;
			CachedDescription = InInfo->CachedDescription;
			PossibleObjectReferenceTypes = InInfo->PossibleObjectReferenceTypes;
		}
		
		/** Returns a succinct menu description of this type */
		FText GetDescription() const;

		FText GetToolTip() const
		{
			if (PinType.PinSubCategoryObject.IsValid())
			{
				if (Tooltip.IsEmpty())
				{
					if ( (PinType.PinCategory == UEdGraphSchema_K2::PC_Struct) && PinType.PinSubCategoryObject->IsA(UScriptStruct::StaticClass()) )
					{
						return FText::FromString(PinType.PinSubCategoryObject->GetPathName());
					}
				}
			}
			return Tooltip;
		}

		uint8 GetPossibleObjectReferenceTypes() const
		{
			return PossibleObjectReferenceTypes;
		}

	private:

		FPinTypeTreeInfo()
			: PossibleObjectReferenceTypes(0)
			, bReadOnly(false)
		{}

		void Init(const FText& FriendlyCategoryName, const FString& CategoryName, const UEdGraphSchema_K2* Schema, const FText& InTooltip, bool bInReadOnly, FTypesDatabase* TypesDatabase);

		FText GenerateDescription();
	};

public:
	void SelectAllNodesInDirection(TEnumAsByte<enum EEdGraphPinDirection> InDirection, UEdGraph* Graph, UEdGraphPin* InGraphPin);

	//~ Begin EdGraphSchema Interface
	virtual void GetContextMenuActions(const UEdGraph* CurrentGraph, const UEdGraphNode* InGraphNode, const UEdGraphPin* InGraphPin, class FMenuBuilder* MenuBuilder, bool bIsDebugging) const override;
	virtual const FPinConnectionResponse CanCreateConnection(const UEdGraphPin* A, const UEdGraphPin* B) const override;
	virtual bool TryCreateConnection(UEdGraphPin* A, UEdGraphPin* B) const override;
	virtual bool CreateAutomaticConversionNodeAndConnections(UEdGraphPin* A, UEdGraphPin* B) const override;
	virtual FString IsPinDefaultValid(const UEdGraphPin* Pin, const FString& NewDefaultValue, UObject* NewDefaultObject, const FText& InNewDefaultText) const override;
	virtual bool DoesSupportPinWatching() const	override;
	virtual bool IsPinBeingWatched(UEdGraphPin const* Pin) const override;
	virtual void ClearPinWatch(UEdGraphPin const* Pin) const override;
	virtual void TrySetDefaultValue(UEdGraphPin& Pin, const FString& NewDefaultValue) const override;
	virtual void TrySetDefaultObject(UEdGraphPin& Pin, UObject* NewDefaultObject) const override;
	virtual void TrySetDefaultText(UEdGraphPin& InPin, const FText& InNewDefaultText) const override;
	virtual bool DoesDefaultValueMatchAutogenerated(const UEdGraphPin& InPin) const override;
	virtual void ResetPinToAutogeneratedDefaultValue(UEdGraphPin* Pin, bool bCallModifyCallbacks = true) const override;
	virtual bool ShouldHidePinDefaultValue(UEdGraphPin* Pin) const override;
	virtual bool ShouldShowAssetPickerForPin(UEdGraphPin* Pin) const override;
	virtual FLinearColor GetPinTypeColor(const FEdGraphPinType& PinType) const override;
	FLinearColor GetSecondaryPinTypeColor(const FEdGraphPinType& PinType) const;
	virtual FText GetPinDisplayName(const UEdGraphPin* Pin) const override;
	virtual void ConstructBasicPinTooltip(const UEdGraphPin& Pin, const FText& PinDescription, FString& TooltipOut) const override;
	virtual EGraphType GetGraphType(const UEdGraph* TestEdGraph) const override;
	virtual bool IsTitleBarPin(const UEdGraphPin& Pin) const override;
	virtual void BreakNodeLinks(UEdGraphNode& TargetNode) const override;
	virtual void BreakPinLinks(UEdGraphPin& TargetPin, bool bSendsNodeNotifcation) const override;
	virtual void BreakSinglePinLink(UEdGraphPin* SourcePin, UEdGraphPin* TargetPin) override;
	virtual void ReconstructNode(UEdGraphNode& TargetNode, bool bIsBatchRequest=false) const override;
	virtual bool CanEncapuslateNode(UEdGraphNode const& TestNode) const override;
	virtual void HandleGraphBeingDeleted(UEdGraph& GraphBeingRemoved) const override;
	virtual void GetGraphDisplayInformation(const UEdGraph& Graph, /*out*/ FGraphDisplayInfo& DisplayInfo) const override;
	virtual void DroppedAssetsOnGraph(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraph* Graph) const override;
	virtual void DroppedAssetsOnNode(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraphNode* Node) const override;
	virtual void DroppedAssetsOnPin(const TArray<FAssetData>& Assets, const FVector2D& GraphPosition, UEdGraphPin* Pin) const override;
	virtual void GetAssetsNodeHoverMessage(const TArray<FAssetData>& Assets, const UEdGraphNode* HoverNode, FString& OutTooltipText, bool& OutOkIcon) const override;
	virtual void GetAssetsPinHoverMessage(const TArray<FAssetData>& Assets, const UEdGraphPin* HoverPin, FString& OutTooltipText, bool& OutOkIcon) const override;
	virtual void GetAssetsGraphHoverMessage(const TArray<FAssetData>& Assets, const UEdGraph* HoverGraph, FString& OutTooltipText, bool& OutOkIcon) const override;
	virtual bool CanDuplicateGraph(UEdGraph* InSourceGraph) const override;
	virtual UEdGraph* DuplicateGraph(UEdGraph* GraphToDuplicate) const override;
	virtual UEdGraphNode* CreateSubstituteNode(UEdGraphNode* Node, const UEdGraph* Graph, FObjectInstancingGraph* InstanceGraph, TSet<FName>& InOutExtraNames) const override;
	virtual int32 GetNodeSelectionCount(const UEdGraph* Graph) const override;
	virtual TSharedPtr<FEdGraphSchemaAction> GetCreateCommentAction() const override;
	virtual bool FadeNodeWhenDraggingOffPin(const UEdGraphNode* Node, const UEdGraphPin* Pin) const override;
	virtual void BackwardCompatibilityNodeConversion(UEdGraph* Graph, bool bOnlySafeChanges) const override;
	virtual bool ShouldAlwaysPurgeOnModification() const override { return false; }
	virtual void SplitPin(UEdGraphPin* Pin, bool bNotify = true) const override;
	virtual void RecombinePin(UEdGraphPin* Pin) const override;
	virtual void OnPinConnectionDoubleCicked(UEdGraphPin* PinA, UEdGraphPin* PinB, const FVector2D& GraphPosition) const override;
	virtual UEdGraphPin* DropPinOnNode(UEdGraphNode* InTargetNode, const FString& InSourcePinName, const FEdGraphPinType& InSourcePinType, EEdGraphPinDirection InSourcePinDirection) const override;
	virtual bool SupportsDropPinOnNode(UEdGraphNode* InTargetNode, const FEdGraphPinType& InSourcePinType, EEdGraphPinDirection InSourcePinDirection, FText& OutErrorMessage) const override;
	virtual bool IsCacheVisualizationOutOfDate(int32 InVisualizationCacheID) const override;
	virtual int32 GetCurrentVisualizationCacheID() const override;
	virtual void ForceVisualizationCacheClear() const override;
	virtual bool SafeDeleteNodeFromGraph(UEdGraph* Graph, UEdGraphNode* NodeToDelete) const override;
	//~ End EdGraphSchema Interface

	/**
	 *
	 *	Determine if this graph supports event dispatcher
	 * 
	 *	@return True if this schema supports Event Dispatcher 
	 */
	virtual bool DoesSupportEventDispatcher() const { return true; }

	/**
	* Configure the supplied variable node based on the supplied info
	*
	* @param	InVarNode			The variable node to be configured
	* @param	InVariableName		The name of the current variable
	* @param	InVaraiableSource	The source of the variable
	* @param	InTargetBlueprint	The blueprint this node will be used on
	*/
	static void ConfigureVarNode(class UK2Node_Variable* InVarNode, FName InVariableName, UStruct* InVariableSource, UBlueprint* InTargetBlueprint);

	/**
	* Creates a new variable getter node and adds it to ParentGraph
	*
	* @param	GraphPosition		The location of the new node inside the graph
	* @param	ParentGraph			The graph to spawn the new node in
	* @param	VariableName		The name of the variable
	* @param	Source				The source of the variable
	* @return	A pointer to the newly spawned node
	*/
	virtual class UK2Node_VariableGet* SpawnVariableGetNode(const FVector2D GraphPosition, class UEdGraph* ParentGraph, FName VariableName, UStruct* Source) const;

	/**
	* Creates a new variable setter node and adds it to ParentGraph
	*
	* @param	GraphPosition		The location of the new node inside the graph
	* @param	ParentGraph			The graph to spawn the new node in
	* @param	VariableName		The name of the variable
	* @param	Source				The source of the variable
	* @return	A pointer to the newly spawned node
	*/
	virtual class UK2Node_VariableSet* SpawnVariableSetNode(const FVector2D GraphPosition, class UEdGraph* ParentGraph, FName VariableName, UStruct* Source) const;

	// Returns whether the supplied Pin is a splittable struct
	bool PinHasSplittableStructType(const UEdGraphPin* InGraphPin) const;

	/** Returns true if the pin has a value field that can be edited inline */
	bool PinDefaultValueIsEditable(const UEdGraphPin& InGraphPin) const;

	struct FCreateSplitPinNodeParams
	{
		FCreateSplitPinNodeParams(const bool bInTransient)
			: CompilerContext(nullptr)
			, SourceGraph(nullptr)
			, bTransient(bInTransient)
		{}

		FCreateSplitPinNodeParams(class FKismetCompilerContext* InCompilerContext, UEdGraph* InSourceGraph)
			: CompilerContext(InCompilerContext)
			, SourceGraph(InSourceGraph)
			, bTransient(false)
		{}

		FKismetCompilerContext* CompilerContext;
		UEdGraph* SourceGraph;
		bool bTransient;
	};

	/** Helper function to create the expansion node.  
		If the CompilerContext is specified this will be created as an intermediate node */
	UK2Node* CreateSplitPinNode(UEdGraphPin* Pin, const FCreateSplitPinNodeParams& Params) const;

	/** Reads in a FString and gets the values of the pin defaults for that type. This can be passed to DefaultValueSimpleValidation to validate. OwningObject can be null */
	virtual void GetPinDefaultValuesFromString(const FEdGraphPinType& PinType, UObject* OwningObject, const FString& NewValue, FString& UseDefaultValue, UObject*& UseDefaultObject, FText& UseDefaultText) const;

	/** Do validation, that doesn't require a knowledge about actual pin */
	virtual bool DefaultValueSimpleValidation(const FEdGraphPinType& PinType, const FString& PinName, const FString& NewDefaultValue, UObject* NewDefaultObject, const FText& InText, FString* OutMsg = nullptr) const;

	/** Returns true if the owning node is a function with AutoCreateRefTerm meta data */
	bool IsAutoCreateRefTerm(const UEdGraphPin* Pin) const;

	/** See if a class has any members that are accessible by a blueprint */
	bool ClassHasBlueprintAccessibleMembers(const UClass* InClass) const;

	/**
	 * Checks to see if the specified graph is a construction script
	 *
	 * @param	TestEdGraph		Graph to test
	 * @return	true if this is a construction script
	 */
	static bool IsConstructionScript(const UEdGraph* TestEdGraph);

	/**
	 * Checks to see if the specified graph is a composite graph
	 *
	 * @param	TestEdGraph		Graph to test
	 * @return	true if this is a composite graph
	 */
	bool IsCompositeGraph(const UEdGraph* TestEdGraph) const;

	/**
	 * Checks to see if the specified graph is a const function graph
	 *
	 * @param	TestEdGraph		Graph to test
	 * @param	bOutIsEnforcingConstCorrectness (Optional) Whether or not this graph is enforcing const correctness during compilation
	 * @return	true if this is a const function graph
	 */
	bool IsConstFunctionGraph(const UEdGraph* TestEdGraph, bool* bOutIsEnforcingConstCorrectness = nullptr) const;

	/**
	 * Checks to see if the specified graph is a static function graph
	 *
	 * @param	TestEdGraph		Graph to test
	 * @return	true if this is a const function graph
	 */
	bool IsStaticFunctionGraph(const UEdGraph* TestEdGraph) const;

	/**
	 * Checks to see if a pin is an execution pin.
	 *
	 * @param	Pin	The pin to check.
	 * @return	true if it is an execution pin.
	 */
	static inline bool IsExecPin(const UEdGraphPin& Pin)
	{
		return Pin.PinType.PinCategory == PC_Exec;
	}

	/**
	 * Checks to see if a pin is a Self pin (indicating the calling context for the node)
	 *
	 * @param	Pin	The pin to check.
	 * @return	true if it is a Self pin.
	 */
	virtual bool IsSelfPin(const UEdGraphPin& Pin) const override;

	/**
	 * Checks to see if a pin is a meta-pin (either a Self or Exec pin)
	 *
	 * @param	Pin	The pin to check.
	 * @return	true if it is a Self or Exec pin.
	 */
	inline bool IsMetaPin(const UEdGraphPin& Pin) const
	{
		return IsExecPin(Pin) || IsSelfPin(Pin);
	}

	/** Is given string a delegate category name ? */
	virtual bool IsDelegateCategory(const FString& Category) const override;

	/** Returns whether a pin category is compatible with an Index Wildcard (PC_Wildcard and PSC_Index) */
	inline bool IsIndexWildcardCompatible(const FEdGraphPinType& PinType) const
	{
		return (!PinType.IsContainer()) && 
			(
				PinType.PinCategory == PC_Boolean || 
				PinType.PinCategory == PC_Int || 
				PinType.PinCategory == PC_Byte ||
				(PinType.PinCategory == PC_Wildcard && PinType.PinSubCategory == PSC_Index)
			);
	}

	/**
	 * Searches for the first execution pin with the specified direction on the node
	 *
	 * @param	Node			The node to search.
	 * @param	PinDirection	The required pin direction.
	 *
	 * @return	the first found execution pin with the correct direction or null if there were no matching pins.
	 */
	UEdGraphPin* FindExecutionPin(const UEdGraphNode& Node, EEdGraphPinDirection PinDirection) const
	{
		for (int32 PinIndex = 0; PinIndex < Node.Pins.Num(); ++PinIndex)
		{
			UEdGraphPin* Pin = Node.Pins[PinIndex];

			if ((Pin->Direction == PinDirection) && IsExecPin(*Pin))
			{
				return Pin;
			}
		}

		return NULL;
	}

	/**
	 * Searches for the first Self pin with the specified direction on the node
	 *
	 * @param	Node			The node to search.
	 * @param	PinDirection	The required pin direction.
	 *
	 * @return	the first found self pin with the correct direction or null if there were no matching pins.
	 */
	UEdGraphPin* FindSelfPin(const UEdGraphNode& Node, EEdGraphPinDirection PinDirection) const
	{
		for (int32 PinIndex = 0; PinIndex < Node.Pins.Num(); ++PinIndex)
		{
			UEdGraphPin* Pin = Node.Pins[PinIndex];

			if ((Pin->Direction == PinDirection) && IsSelfPin(*Pin))
			{
				return Pin;
			}
		}

		return NULL;
	}

	/** Can Pin be promoted to a variable? */
	bool CanPromotePinToVariable (const UEdGraphPin& Pin) const;

	/** Can Pin be split in to its component elements */
	bool CanSplitStructPin(const UEdGraphPin& Pin) const;

	/** Can Pin be recombined back to its original form */
	bool CanRecombineStructPin(const UEdGraphPin& Pin) const;

	/** 
	 * Helper function for filling out Category, SubCategory, and SubCategoryObject based on a UProperty 
	 * 
	 * @return	true on success, false if the property is unsupported or invalid.
	 */
	static bool GetPropertyCategoryInfo(const UProperty* TestProperty, FString& OutCategory, FString& OutSubCategory, UObject*& OutSubCategoryObject, bool& bOutIsWeakPointer);

	/**
	 * Convert the type of a UProperty to the corresponding pin type.
	 *
	 * @param		Property	The property to convert.
	 * @param [out]	TypeOut		The resulting pin type.
	 *
	 * @return	true on success, false if the property is unsupported or invalid.
	 */
	bool ConvertPropertyToPinType(const UProperty* Property, /*out*/ FEdGraphPinType& TypeOut) const;

	/** Determines if the specified param property is intended to be used as a wildcard (for custom thunk functions, like in our array library, etc.)*/
	static bool IsWildcardProperty(const UProperty* ParamProperty);

	/** Flags to indicate different types of blueprint callable functions */
	enum EFunctionType
	{
		FT_Imperative	= 0x01,
		FT_Pure			= 0x02,
		FT_Const		= 0x04,
		FT_Protected	= 0x08,
	};

	/**
	 * Finds the parent function for the specified function, if any
	 *
	 * @param	Function			The function to find a parent function for
	 * @return	The UFunction parentfunction, if any.
	 */
	UFunction* GetCallableParentFunction(UFunction* Function) const;

	/** Whether or not the specified actor is a valid target for bound events and literal references (in the right level, not a builder brush, etc */
	bool IsActorValidForLevelScriptRefs(const AActor* TestActor, const UBlueprint* Blueprint) const;

	/** 
	 *	Generate a list of replaceable nodes for context menu based on the editor's current selection 
	 *
	 *	@param	Reference to graph node
	 *	@param	Reference to context menu builder
	 */
	void AddSelectedReplaceableNodes( UBlueprint* Blueprint, const UEdGraphNode* InGraphNode, FMenuBuilder* MenuBuilder ) const;

	/**
	 *	Function to replace current graph node reference object with a new object
	 *
	 *	@param Reference to graph node
	 *	@param Reference to new reference object
	 */
	void ReplaceSelectedNode(UEdGraphNode* SourceNode, AActor* TargetActor);

	/** Returns whether a function is marked 'override' and doesn't have any out parameters */
	static bool FunctionCanBePlacedAsEvent(const UFunction* InFunction);

	/** Can this function be called by kismet delegate */
	static bool FunctionCanBeUsedInDelegate(const UFunction* InFunction);

	/** Can this function be called by kismet code */
	static bool CanUserKismetCallFunction(const UFunction* Function);

	/** Returns if hunction has output parameter(s) */
	static bool HasFunctionAnyOutputParameter(const UFunction* Function);

	enum EDelegateFilterMode
	{
		CannotBeDelegate,
		MustBeDelegate,
		VariablesAndDelegates
	};

	/** Can this variable be accessed by kismet code */
	static bool CanUserKismetAccessVariable(const UProperty* Property, const UClass* InClass, EDelegateFilterMode FilterMode);

	/** Can this function be overridden by kismet (either placed as event or new function graph created) */
	static bool CanKismetOverrideFunction(const UFunction* Function);

	/** returns friendly signature name if possible or Removes any mangling to get the unmangled signature name of the function */
	static FText GetFriendlySignatureName(const UFunction* Function);

	static bool IsAllowableBlueprintVariableType(const UEnum* InEnum);
	static bool IsAllowableBlueprintVariableType(const UClass* InClass);
	static bool IsAllowableBlueprintVariableType(const UScriptStruct *InStruct, bool bForInternalUse = false);

	static bool IsPropertyExposedOnSpawn(const UProperty* Property);

	/**
	 * Returns a list of parameters for the function that are specified as automatically emitting terms for unconnected ref parameters in the compiler (MD_AutoCreateRefTerm)
	 *
	 * @param	Function				The function to check for auto-emitted ref terms on
	 * @param	AutoEmitParameterNames	(out) Returns an array of param names that should be auto-emitted if nothing is connected
	 */
	void GetAutoEmitTermParameters(const UFunction* Function, TArray<FString>& AutoEmitParameterNames) const;

	/**
	 * Determine if a function has a parameter of a specific type.
	 *
	 * @param	InFunction	  	The function to search.
	 * @param	InGraph			The graph that you're looking to call the function from (some functions hide different pins depending on the graph they're in)
	 * @param	DesiredPinType	The type that at least one function parameter needs to be.
	 * @param	bWantOutput   	The direction that the parameter needs to be.
	 *
	 * @return	true if at least one parameter is of the correct type and direction.
	 */
	bool FunctionHasParamOfType(const UFunction* InFunction, UEdGraph const* InGraph, const FEdGraphPinType& DesiredPinType, bool bWantOutput) const;

	/**
	 * Add the specified flags to the function entry node of the graph, to make sure they get compiled in to the generated function
	 *
	 * @param	CurrentGraph	The graph of the function to modify the flags for
	 * @param	ExtraFlags		The flags to add to the function entry node
	 */
	void AddExtraFunctionFlags(const UEdGraph* CurrentGraph, int32 ExtraFlags) const;

	/**
	 * Marks the function entry of a graph as editable via function editor or not-editable
	 *
	 * @param	CurrentGraph	The graph of the function to modify the entry node for
	 * @param	bNewEditable	Whether or not the function entry for the graph should be editable via the function editor
	 */
	void MarkFunctionEntryAsEditable(const UEdGraph* CurrentGraph, bool bNewEditable) const;

	/** 
	 * Populate new macro graph with entry and possibly return node
	 * 
	 * @param	Graph			Graph to add the function terminators to
	 * @param	ContextClass	If specified, the graph terminators will use this class to search for context for signatures (i.e. interface functions)
	 */
	virtual void CreateMacroGraphTerminators(UEdGraph& Graph, UClass* Class) const;

	/** 
	 * Populate new function graph with entry and possibly return node
	 * 
	 * @param	Graph			Graph to add the function terminators to
	 * @param	ContextClass	If specified, the graph terminators will use this class to search for context for signatures (i.e. interface functions)
	 */
	virtual void CreateFunctionGraphTerminators(UEdGraph& Graph, UClass* Class) const;

	/**
	 * Populate new function graph with entry and possibly return node
	 * 
	 * @param	Graph			Graph to add the function terminators to
	 * @param	FunctionSignature	The function signature to mimic when creating the inputs and outputs for the function.
	 */
	virtual void CreateFunctionGraphTerminators(UEdGraph& Graph, UFunction* FunctionSignature) const;

	/**
	 * Converts the type of a property into a fully qualified string (e.g., object'ObjectName').
	 *
	 * @param	Property	The property to convert into a string.
	 *
	 * @return	The converted type string.
	 */
	static FText TypeToText(UProperty* const Property);

	/**
	* Converts a terminal type into a fully qualified FText (e.g., object'ObjectName').
	* Primarily used as a helper when converting containers to TypeToText.
	*
	* @param	Category					The category to convert into a FText.
	* @param	SubCategory					The subcategory to convert into FText
	* @param	SubCategoryObject			The SubcategoryObject to convert into FText
	* @param	bIsWeakPtr					Whether the type is a WeakPtr
	*
	* @return	The converted type text.
	*/
	static FText TerminalTypeToText(const FString& Category, const FString& SubCategory, UObject* SubCategoryObject, bool bIsWeakPtr);

	/**
	 * Converts a pin type into a fully qualified FText (e.g., object'ObjectName').
	 *
	 * @param	Type	The type to convert into a FText.
	 *
	 * @return	The converted type text.
	 */
	static FText TypeToText(const FEdGraphPinType& Type);

	/**
	 * Returns the FText to use for a given schema category
	 *
	 * @param	Category	The category to convert into a FText.
	 * @param	bForMenu	Indicates if this is for display in tooltips or menu
	 *
	 * @return	The text to display for the category.
	 */
	static FText GetCategoryText(const FString& Category, const bool bForMenu = false);

	/**
	 * Get the type tree for all of the property types valid for this schema
	 *
	 * @param	TypeTree		The array that will contain the type tree hierarchy for this schema upon returning
	 * @param	TypeTreeFilter	ETypeTreeFilter flags that determine how the TypeTree is populated.
	 */
	void GetVariableTypeTree(TArray< TSharedPtr<FPinTypeTreeInfo> >& TypeTree, ETypeTreeFilter TypeTreeFilter = ETypeTreeFilter::None) const;

	/**
	 * Returns whether or not the specified type has valid subtypes available
	 *
	 * @param	Type	The type to check for subtypes
	 */
	bool DoesTypeHaveSubtypes( const FString& FriendlyTypeName ) const;

	/**
	 * Returns true if the types and directions of two pins are schema compatible. Handles
	 * outputting a more derived type to an input pin expecting a less derived type.
	 *
	 * @param	PinA		  	The pin a.
	 * @param	PinB		  	The pin b.
	 * @param	CallingContext	(optional) The calling context (required to properly evaluate pins of type Self)
	 * @param	bIgnoreArray	(optional) Whether or not to ignore differences between array and non-array types
	 *
	 * @return	true if the pin types and directions are compatible.
	 */
	virtual bool ArePinsCompatible(const UEdGraphPin* PinA, const UEdGraphPin* PinB, const UClass* CallingContext = NULL, bool bIgnoreArray = false) const;

	/**
	 * Returns the connection response for connecting PinA to PinB, which have already been determined to be compatible
	 * types with a compatible direction.  InputPin and OutputPin are PinA and PinB or vis versa, indicating their direction.
	 *
	 * @param	PinA		  	The pin a.
	 * @param	PinB		  	The pin b.
	 * @param	InputPin	  	Either PinA or PinB, depending on which one is the input.
	 * @param	OutputPin	  	Either PinA or PinB, depending on which one is the output.
	 *
	 * @return	The message and action to take on trying to make this connection.
	 */
	virtual const FPinConnectionResponse DetermineConnectionResponseOfCompatibleTypedPins(const UEdGraphPin* PinA, const UEdGraphPin* PinB, const UEdGraphPin* InputPin, const UEdGraphPin* OutputPin) const;

	/**
	 * Returns true if the two pin types are schema compatible.  Handles outputting a more derived
	 * type to an input pin expecting a less derived type.
	 *
	 * @param	Output		  	The output type.
	 * @param	Input		  	The input type.
	 * @param	CallingContext	(optional) The calling context (required to properly evaluate pins of type Self)
	 * @param	bIgnoreArray	(optional) Whether or not to ignore differences between array and non-array types
	 *
	 * @return	true if the pin types are compatible.
	 */
	virtual bool ArePinTypesCompatible(const FEdGraphPinType& Output, const FEdGraphPinType& Input, const UClass* CallingContext = NULL, bool bIgnoreArray = false) const;

	/** Sets the autogenerated default value for a pin, optionally using the passed in function and parameter. This will also reset the current default value to the autogenerated one */
	virtual void SetPinAutogeneratedDefaultValue(UEdGraphPin* Pin, const FString& NewValue) const;

	/** Sets the autogenerated default value for a pin using the default for that type. This will also reset the current default value to the autogenerated one */
	virtual void SetPinAutogeneratedDefaultValueBasedOnType(UEdGraphPin* Pin) const;

	/** Sets the pin defaults, but not autogenerated defaults, at pin construction time. This is like TrySetDefaultValue but does not do validation or callbacks */
	virtual void SetPinDefaultValueAtConstruction(UEdGraphPin* Pin, const FString& DefaultValueString) const;

	/** Call to let blueprint and UI know that parameters have changed for a function/macro/etc */
	virtual void HandleParameterDefaultValueChanged(UK2Node* TargetNode) const;

	DEPRECATED(4.17, "SetPinDefaultValue is deprecated due to confusing name, call SetPinAutogeneratedDefaultValue")
	virtual void SetPinDefaultValue(UEdGraphPin* Pin, const UFunction* Function = nullptr, const UProperty* Param = nullptr) const;

	DEPRECATED(4.17, "SetPinDefaultValueBasedOnType is deprecated due to confusing name, call SetPinAutogeneratedDefaultValue")
	virtual void SetPinDefaultValueBasedOnType(UEdGraphPin* Pin) const;

	/** Given a function and property, return the default value */
	static bool FindFunctionParameterDefaultValue(const UFunction* Function, const UProperty* Param, FString& OutString);

	/** Utility that makes sure existing connections are valid, breaking any that are now illegal. */
	static void ValidateExistingConnections(UEdGraphPin* Pin);

	/** Find a 'set value by name' function for the specified pin, if it exists */
	static UFunction* FindSetVariableByNameFunction(const FEdGraphPinType& PinType);

	/** Find an appropriate function to call to perform an automatic cast operation */
	virtual bool SearchForAutocastFunction(const UEdGraphPin* OutputPin, const UEdGraphPin* InputPin, /*out*/ FName& TargetFunction, /*out*/ UClass*& FunctionOwner) const;

	/** Find an appropriate node that can convert from one pin type to another (not a cast; e.g. "MakeLiteralArray" node) */
	virtual bool FindSpecializedConversionNode(const UEdGraphPin* OutputPin, const UEdGraphPin* InputPin, bool bCreateNode, /*out*/ class UK2Node*& TargetNode) const;

	/** Get menu for breaking links to specific nodes*/
	void GetBreakLinkToSubMenuActions(class FMenuBuilder& MenuBuilder, class UEdGraphPin* InGraphPin);

	/** Get menu for jumping to specific pin links */
	void GetJumpToConnectionSubMenuActions(class FMenuBuilder& MenuBuilder, class UEdGraphPin* InGraphPin);

	/** Get menu for straightening links to specific nodes*/
	void GetStraightenConnectionToSubMenuActions( class FMenuBuilder& MenuBuilder, UEdGraphPin* InGraphPin ) const;

	/** Get the destination pin for a straighten operation */
	static UEdGraphPin* GetAndResetStraightenDestinationPin();

	/** Create menu for variable get/set nodes which refer to a variable which does not exist. */
	void GetNonExistentVariableMenu(const UEdGraphNode* InGraphNode, UBlueprint* OwnerBlueprint, FMenuBuilder* MenuBuilder) const;

	/**
	 * Create menu for variable get/set nodes which allows for the replacement of variables
	 *
	 * @param InGraphNode					Variable node to replace
	 * @param InOwnerBlueprint				The owning Blueprint of the variable
	 * @param InMenuBuilder					MenuBuilder to place the menu items into
	 * @param bInReplaceExistingVariable	TRUE if replacing an existing variable, will keep the variable from appearing on the list
	 */
	void GetReplaceVariableMenu(const UEdGraphNode* InGraphNode, UBlueprint* InOwnerBlueprint, FMenuBuilder* InMenuBuilder, bool bInReplaceExistingVariable = false) const;

	// Calculates an average position between the nodes owning the two specified pins
	static FVector2D CalculateAveragePositionBetweenNodes(UEdGraphPin* InputPin, UEdGraphPin* OutputPin);

	// Tries to connect any pins with matching types and directions from the conversion node to the specified input and output pins
	void AutowireConversionNode(UEdGraphPin* InputPin, UEdGraphPin* OutputPin, UEdGraphNode* ConversionNode) const;

	/** Calculates an estimated height for the specified node */
	static float EstimateNodeHeight( UEdGraphNode* Node );

	/** 
	 * Checks if the graph supports impure functions
	 *
	 * @param InGraph		Graph to check
	 *
	 * @return				True if the graph supports impure functions
	 */
	bool DoesGraphSupportImpureFunctions(const UEdGraph* InGraph) const;

	/**
	 * Checks to see if the passed in function is valid in the graph for the current class
	 *
	 * @param	InClass  			Class being checked to see if the function is valid for
	 * @param	InFunction			Function being checked
	 * @param	InDestGraph			Graph we will be using action for (may be NULL)
	 * @param	InFunctionTypes		Combination of EFunctionType to indicate types of functions accepted
	 * @param	bInCalledForEach	Call for each element in an array (a node accepts array)
	 * @param	OutReason			Allows callers to receive a localized string containing more detail when the function is determined to be invalid (optional)
	 */
	bool CanFunctionBeUsedInGraph(const UClass* InClass, const UFunction* InFunction, const UEdGraph* InDestGraph, uint32 InFunctionTypes, bool bInCalledForEach, FText* OutReason = nullptr) const;

	/**
	 * Makes connections into/or out of the gateway node, connect directly to the associated networks on the opposite side of the tunnel
	 * When done, none of the pins on the gateway node will be connected to anything.
	 * Requires both this gateway node and it's associated node to be in the same graph already (post-merging)
	 *
	 * @param InGatewayNode			The function or tunnel node
	 * @param InEntryNode			The entry node in the inner graph
	 * @param InResultNode			The result node in the inner graph
	 *
	 * @return						Returns TRUE if successful
	 */
	bool CollapseGatewayNode(UK2Node* InNode, UEdGraphNode* InEntryNode, UEdGraphNode* InResultNode, class FKismetCompilerContext* CompilerContext = nullptr, TSet<UEdGraphNode*>* OutExpandedNodes = nullptr) const;

	/** 
	 * Connects all of the linked pins from PinA to all of the linked pins from PinB, removing
	 * both PinA and PinB from being linked to anything else
	 * Requires the nodes that own the pins to be in the same graph already (post-merging)
	 */
	void CombineTwoPinNetsAndRemoveOldPins(UEdGraphPin* InPinA, UEdGraphPin* InPinB) const;

	/**
	 * Make links from all data pins from InOutputNode output to InInputNode input.
	 */
	void LinkDataPinFromOutputToInput(UEdGraphNode* InOutputNode, UEdGraphNode* InInputNode) const;

	/** Moves all connections from the old node to the new one. Returns true and destroys OldNode on success. Fails if it cannot find a mapping from an old pin. */
	bool ReplaceOldNodeWithNew(UK2Node* OldNode, UK2Node* NewNode, const TMap<FString, FString>& OldPinToNewPinMap) const;

	/** Convert a deprecated node into a function call node, called from per-node ConvertDeprecatedNode */
	UK2Node* ConvertDeprecatedNodeToFunctionCall(UK2Node* OldNode, UFunction* NewFunction, TMap<FString, FString>& OldPinToNewPinMap, UEdGraph* Graph) const;

	/** some inherited schemas don't want anim-notify actions listed, so this is an easy way to check that */
	virtual bool DoesSupportAnimNotifyActions() const { return true; }

	///////////////////////////////////////////////////////////////////////////////////
	// NonExistent Variables: Broken Get/Set Nodes where the variable is does not exist 

	/** Create the variable that the broken node refers to */
	static void OnCreateNonExistentVariable(class UK2Node_Variable* Variable, UBlueprint* OwnerBlueprint);

	/** Create the local variable that the broken node refers to */
	static void OnCreateNonExistentLocalVariable(class UK2Node_Variable* Variable, UBlueprint* OwnerBlueprint);

	/** Replace the variable that a variable node refers to when the variable it refers to does not exist */
	static void OnReplaceVariableForVariableNode(class UK2Node_Variable* Variable, UBlueprint* OwnerBlueprint, FString VariableName, bool bIsSelfMember);

	/** Create sub menu that shows all possible variables that can be used to replace the existing variable reference */
	static void GetReplaceVariableMenu(class FMenuBuilder& MenuBuilder, class UK2Node_Variable* Variable, UBlueprint* OwnerBlueprint, bool bReplaceExistingVariable = false);

private:

	/**
	 * Returns true if the specified function has any out parameters
	 * @param [in] Function	The function to check for out parameters
	 * @return true if there are out parameters, else false
	 */
	bool DoesFunctionHaveOutParameters( const UFunction* Function ) const;

	static const UScriptStruct* VectorStruct;
	static const UScriptStruct* RotatorStruct;
	static const UScriptStruct* TransformStruct;
	static const UScriptStruct* LinearColorStruct;
	static const UScriptStruct* ColorStruct;
};

