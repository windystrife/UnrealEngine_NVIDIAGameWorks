// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/IndirectArray.h"
#include "Stats/Stats.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Guid.h"
#include "UObject/Class.h"
#include "UObject/UnrealType.h"
#include "EdGraph/EdGraphPin.h"
#include "BlueprintGeneratedClass.generated.h"

class AActor;
class UActorComponent;
class UDynamicBlueprintBinding;
class UInheritableComponentHandler;
class UTimelineTemplate;

DECLARE_MEMORY_STAT_EXTERN(TEXT("Persistent Uber Graph Frame memory"), STAT_PersistentUberGraphFrameMemory, STATGROUP_Memory, );

class UEdGraphPin;


USTRUCT()
struct FNodeToCodeAssociation
{
	GENERATED_USTRUCT_BODY()

public:
	TWeakObjectPtr<UEdGraphNode> Node;
	TWeakObjectPtr<UFunction> Scope;
	int32 Offset;
public:
	FNodeToCodeAssociation()
	: Node(nullptr)
	, Scope(nullptr)
	, Offset(0)
	{
	}

	FNodeToCodeAssociation(UEdGraphNode* InNode, UFunction* InFunction, int32 InOffset)
		: Node(InNode)
		, Scope(InFunction)
		, Offset(InOffset)
	{
	}
};


USTRUCT()
struct FDebuggingInfoForSingleFunction
{
	GENERATED_USTRUCT_BODY()

public:
	// Reverse map from code offset to source node
	TMap< int32, TWeakObjectPtr<UEdGraphNode> > LineNumberToSourceNodeMap;

	// Reverse map from code offset to macro source node
	TMap< int32, TWeakObjectPtr<UEdGraphNode> > LineNumberToMacroSourceNodeMap;

	// Reverse map from code offset to macro instance node(s)
	TMultiMap< int32, TWeakObjectPtr<UEdGraphNode> > LineNumberToMacroInstanceNodeMap;

	// Reverse map from code offset to source pin
	TMap< int32, FEdGraphPinReference > LineNumberToSourcePinMap;

	// Reverse map from source pin to mapped code offset(s)
	TMultiMap< FEdGraphPinReference, int32 > SourcePinToLineNumbersMap;

	// Map from source node (impure) to pure node script code range
	TMap< TWeakObjectPtr<UEdGraphNode>, FInt32Range > PureNodeScriptCodeRangeMap;

public:
	FDebuggingInfoForSingleFunction()
	{
	}
};


USTRUCT()
struct FPointerToUberGraphFrame
{
	GENERATED_USTRUCT_BODY()

public:
	uint8* RawPointer;

	FPointerToUberGraphFrame() 
		: RawPointer(nullptr)
	{}

	~FPointerToUberGraphFrame()
	{
		check(!RawPointer);
	}
};


template<>
struct TStructOpsTypeTraits<FPointerToUberGraphFrame> : public TStructOpsTypeTraitsBase2<FPointerToUberGraphFrame>
{
	enum
	{
		WithZeroConstructor = true,
		WithCopy = false,
	};
};


//////////////////////////////////////////////////////////////////////////
// TSimpleRingBuffer

template<typename ElementType>
class TSimpleRingBuffer
{
public:
	TSimpleRingBuffer(int32 MaxItems)
		: WriteIndex(0)
	{
		Storage.Empty(MaxItems);
	}

	int32 Num() const
	{
		return Storage.Num();
	}

	ElementType& operator()(int32 i)
	{
		// First element is at WriteIndex-1, second at 2, etc...
		const int32 RingIndex = (WriteIndex - 1 - i);
		const int32 WrappedRingIndex = (RingIndex < 0) ? (RingIndex + ArrayMax()) : RingIndex;

		return Storage[WrappedRingIndex];
	}

	const ElementType& operator()(int32 i) const
	{
		// First element is at WriteIndex-1, second at 2, etc...
		const int32 RingIndex = (WriteIndex - 1 - i);
		const int32 WrappedRingIndex = (RingIndex < 0) ? (RingIndex + ArrayMax()) : RingIndex;

		return Storage[WrappedRingIndex];
	}

	int32 ArrayMax() const
	{
		return Storage.GetSlack() + Storage.Num();
	}

	// Note: Doesn't call the constructor on objects stored in it; make sure to properly initialize the memory returned by WriteNewElementUninitialized
	ElementType& WriteNewElementUninitialized()
	{
		const int32 OldWriteIndex = WriteIndex;
		WriteIndex = (WriteIndex + 1) % ArrayMax();

		if (Storage.GetSlack())
		{
			Storage.AddUninitialized(1);
		}
		return Storage[OldWriteIndex];
	}

	ElementType& WriteNewElementInitialized()
	{
		const int32 OldWriteIndex = WriteIndex;
		WriteIndex = (WriteIndex + 1) % ArrayMax();

		if (Storage.GetSlack())
		{
			Storage.Add(ElementType());
		}
		else
		{
			Storage[OldWriteIndex] = ElementType();
		}
		return Storage[OldWriteIndex];
	}

private:
	TArray<ElementType> Storage;
	int32 WriteIndex;
};

USTRUCT()
struct ENGINE_API FBlueprintDebugData
{
	GENERATED_USTRUCT_BODY()
	FBlueprintDebugData()
	{
	}

	~FBlueprintDebugData()
	{ }
#if WITH_EDITORONLY_DATA

protected:
	// Lookup table from UUID to nodes that were allocated that UUID
	TMap<int32, TWeakObjectPtr<UEdGraphNode> > DebugNodesAllocatedUniqueIDsMap;

	// Lookup table from impure node to entry in DebugNodeLineNumbers
	TMultiMap<TWeakObjectPtr<UEdGraphNode>, int32> DebugNodeIndexLookup;

	// List of debug site information for each node that ended up contributing to codegen
	//   This contains a tracepoint for each impure node after all pure antecedent logic has executed but before the impure function call
	//   It does *not* contain the wire tracepoint placed after the impure function call
	TArray<struct FNodeToCodeAssociation> DebugNodeLineNumbers;

	// List of entry points that contributed to the ubergraph
	TMap<int32, FName> EntryPoints;

	// Acceleration structure for execution wire highlighting at runtime
	TMap<TWeakObjectPtr<UFunction>, FDebuggingInfoForSingleFunction> PerFunctionLineNumbers;

	// Map from objects to class properties they created
	TMap<TWeakObjectPtr<UObject>, UProperty*> DebugObjectToPropertyMap;

	// Map from pins or nodes to class properties they created
	TMap<FEdGraphPinReference, UProperty*> DebugPinToPropertyMap;

public:

	// Returns the UEdGraphNode associated with the UUID, or nullptr if there isn't one.
	UEdGraphNode* FindNodeFromUUID(int32 UUID) const
	{
		if (const TWeakObjectPtr<UEdGraphNode>* pParentNode = DebugNodesAllocatedUniqueIDsMap.Find(UUID))
		{
			return pParentNode->Get();
		}
	
		return nullptr;
	}

	bool IsValid() const
	{
		return DebugNodeLineNumbers.Num() > 0;
	}

	// Finds the UEdGraphNode associated with the code location Function+CodeOffset, or nullptr if there isn't one
	UEdGraphNode* FindSourceNodeFromCodeLocation(UFunction* Function, int32 CodeOffset, bool bAllowImpreciseHit) const
	{
		if (const FDebuggingInfoForSingleFunction* pFuncInfo = PerFunctionLineNumbers.Find(Function))
		{
			UEdGraphNode* Result = pFuncInfo->LineNumberToSourceNodeMap.FindRef(CodeOffset).Get();

			if ((Result == nullptr) && bAllowImpreciseHit)
			{
				for (int32 TrialOffset = CodeOffset + 1; (Result == nullptr) && (TrialOffset < Function->Script.Num()); ++TrialOffset)
				{
					Result = pFuncInfo->LineNumberToSourceNodeMap.FindRef(TrialOffset).Get();
				}
			}

			return Result;
		}

		return nullptr;
	}

	// Finds the macro source node associated with the code location Function+CodeOffset, or nullptr if there isn't one
	UEdGraphNode* FindMacroSourceNodeFromCodeLocation(UFunction* Function, int32 CodeOffset) const
	{
		if (const FDebuggingInfoForSingleFunction* pFuncInfo = PerFunctionLineNumbers.Find(Function))
		{
			return pFuncInfo->LineNumberToMacroSourceNodeMap.FindRef(CodeOffset).Get();
		}

		return nullptr;
	}

	// Finds the macro instance node(s) associated with the code location Function+CodeOffset. The returned set can be empty.
	void FindMacroInstanceNodesFromCodeLocation(UFunction* Function, int32 CodeOffset, TArray<UEdGraphNode*>& MacroInstanceNodes) const
	{
		MacroInstanceNodes.Empty();
		if (const FDebuggingInfoForSingleFunction* pFuncInfo = PerFunctionLineNumbers.Find(Function))
		{
			TArray<TWeakObjectPtr<UEdGraphNode>> MacroInstanceNodePtrs;
			pFuncInfo->LineNumberToMacroInstanceNodeMap.MultiFind(CodeOffset, MacroInstanceNodePtrs);
			for(auto MacroInstanceNodePtrIt = MacroInstanceNodePtrs.CreateConstIterator(); MacroInstanceNodePtrIt; ++MacroInstanceNodePtrIt)
			{
				TWeakObjectPtr<UEdGraphNode> MacroInstanceNodePtr = *MacroInstanceNodePtrIt;
				if(UEdGraphNode* MacroInstanceNode = MacroInstanceNodePtr.Get())
				{
					MacroInstanceNodes.AddUnique(MacroInstanceNode);
				}
			}
		}
	}

	// Finds the source pin associated with the code location Function+CodeOffset, or nullptr if there isn't one
	UEdGraphPin* FindSourcePinFromCodeLocation(UFunction* Function, int32 CodeOffset) const
	{
		if (const FDebuggingInfoForSingleFunction* pFuncInfo = PerFunctionLineNumbers.Find(Function))
		{
			return pFuncInfo->LineNumberToSourcePinMap.FindRef(CodeOffset).Get();
		}

		return nullptr;
	}

	// Finds all code locations (Function+CodeOffset) associated with the source pin
	void FindAllCodeLocationsFromSourcePin(UEdGraphPin const* SourcePin, UFunction* InFunction, TArray<int32>& OutPinToCodeAssociations) const
	{
		OutPinToCodeAssociations.Empty();

		if (const FDebuggingInfoForSingleFunction* pFuncInfo = PerFunctionLineNumbers.Find(InFunction))
		{
			pFuncInfo->SourcePinToLineNumbersMap.MultiFind(SourcePin, OutPinToCodeAssociations, true);
		}
	}

	// Finds the first code location (Function+CodeOffset) associated with the source pin within the given range, or INDEX_NONE if there isn't one
	int32 FindCodeLocationFromSourcePin(UEdGraphPin const* SourcePin, UFunction* InFunction, FInt32Range InRange = FInt32Range()) const
	{
		TArray<int32> PinToCodeAssociations;
		FindAllCodeLocationsFromSourcePin(SourcePin, InFunction, PinToCodeAssociations);

		for (int32 i = 0; i < PinToCodeAssociations.Num(); ++i)
		{
			if (InRange.Contains(PinToCodeAssociations[i]))
			{
				return PinToCodeAssociations[i];
			}
		}

		return INDEX_NONE;
	}

	// Finds all code locations (Function+CodeOffset) associated with the source node
	void FindAllCodeLocationsFromSourceNode(UEdGraphNode* SourceNode, UFunction* InFunction, TArray<int32>& OutNodeToCodeAssociations) const
	{
		OutNodeToCodeAssociations.Empty();

		if (const FDebuggingInfoForSingleFunction* pFuncInfo = PerFunctionLineNumbers.Find(InFunction))
		{
			for (auto CodeLocation : pFuncInfo->LineNumberToSourceNodeMap)
			{
				if (CodeLocation.Value == SourceNode)
				{
					OutNodeToCodeAssociations.Add(CodeLocation.Key);
				}
			}
			for (auto CodeLocation : pFuncInfo->LineNumberToMacroSourceNodeMap)
			{
				if (CodeLocation.Value == SourceNode)
				{
					OutNodeToCodeAssociations.Add(CodeLocation.Key);
				}
			}
		}
	}

	// Finds the pure node script code range associated with the [impure] source node, or FInt32Range(INDEX_NONE) if there is no existing association
	FInt32Range FindPureNodeScriptCodeRangeFromSourceNode(const UEdGraphNode* SourceNode, UFunction* InFunction) const
	{
		FInt32Range Result = FInt32Range(INDEX_NONE);

		if (const FDebuggingInfoForSingleFunction* DebugInfoPtr = PerFunctionLineNumbers.Find(InFunction))
		{
			if (const FInt32Range* ValuePtr = DebugInfoPtr->PureNodeScriptCodeRangeMap.Find(SourceNode))
			{
				Result = *ValuePtr;
			}
		}

		return Result;
	}

	// Finds the breakpoint injection site(s) in bytecode if any were associated with the given node
	void FindBreakpointInjectionSites(UEdGraphNode* Node, TArray<uint8*>& InstallSites) const
	{
		TArray<int32> RecordIndices;
		DebugNodeIndexLookup.MultiFind(Node, RecordIndices, true);
		for(int i = 0; i < RecordIndices.Num(); ++i)
		{
			int32 RecordIndex = RecordIndices[i];
			if (DebugNodeLineNumbers.IsValidIndex(RecordIndex))
			{
				const FNodeToCodeAssociation& Record = DebugNodeLineNumbers[RecordIndex];
				if (UFunction* Scope = Record.Scope.Get())
				{
					if (Scope->Script.IsValidIndex(Record.Offset))
					{
						InstallSites.Add(&(Scope->Script[Record.Offset]));
					}
				}
			}
		}
	}

	// Looks thru the debugging data for any class variables associated with the node
	UProperty* FindClassPropertyForPin(const UEdGraphPin* Pin) const
	{
		if (!Pin)
		{
			return nullptr;
		}

		UProperty* PropertyPtr = DebugPinToPropertyMap.FindRef(Pin);
		if ((PropertyPtr == nullptr) && (Pin->LinkedTo.Num() > 0))
		{
			// Try checking the other side of the connection
			PropertyPtr = DebugPinToPropertyMap.FindRef(Pin->LinkedTo[0]);
		}

		return PropertyPtr;
	}

	// Looks thru the debugging data for any class variables associated with the node (e.g., temporary variables or timelines)
	UProperty* FindClassPropertyForNode(const UEdGraphNode* Node) const
	{
		return DebugObjectToPropertyMap.FindRef(Node);
	}

	// Adds a debug record for a source node and destination in the bytecode of a specified function
	void RegisterNodeToCodeAssociation(UEdGraphNode* TrueSourceNode, UEdGraphNode* MacroSourceNode, const TArray<TWeakObjectPtr<UEdGraphNode>>& MacroInstanceNodes, UFunction* InFunction, int32 CodeOffset, bool bBreakpointSite)
	{
		//@TODO: Nasty expansion behavior during compile time
		if (bBreakpointSite)
		{
			new (DebugNodeLineNumbers) FNodeToCodeAssociation(TrueSourceNode, InFunction, CodeOffset);
			DebugNodeIndexLookup.Add(TrueSourceNode, DebugNodeLineNumbers.Num() - 1);
		}

		FDebuggingInfoForSingleFunction& PerFuncInfo = PerFunctionLineNumbers.FindOrAdd(InFunction);
		PerFuncInfo.LineNumberToSourceNodeMap.Add(CodeOffset, TrueSourceNode);

		if (MacroSourceNode)
		{
			PerFuncInfo.LineNumberToMacroSourceNodeMap.Add(CodeOffset, MacroSourceNode);
		}

		for(auto MacroInstanceNodePtrIt = MacroInstanceNodes.CreateConstIterator(); MacroInstanceNodePtrIt; ++MacroInstanceNodePtrIt)
		{
			PerFuncInfo.LineNumberToMacroInstanceNodeMap.Add(CodeOffset, *MacroInstanceNodePtrIt);
		}
	}

	void RegisterPureNodeScriptCodeRange(UEdGraphNode* TrueSourceNode, UFunction* InFunction, FInt32Range InPureNodeScriptCodeRange)
	{
		FDebuggingInfoForSingleFunction& PerFuncInfo = PerFunctionLineNumbers.FindOrAdd(InFunction);
		PerFuncInfo.PureNodeScriptCodeRangeMap.Add(TrueSourceNode, InPureNodeScriptCodeRange);
	}

	void RegisterPinToCodeAssociation(UEdGraphPin const* SourcePin, UFunction* InFunction, int32 CodeOffset)
	{
		FDebuggingInfoForSingleFunction& PerFuncInfo = PerFunctionLineNumbers.FindOrAdd(InFunction);
		PerFuncInfo.LineNumberToSourcePinMap.Add(CodeOffset, SourcePin);
		PerFuncInfo.SourcePinToLineNumbersMap.Add(SourcePin, CodeOffset);
	}

	const TMap<int32, FName>& GetEntryPoints() const { return EntryPoints; }

	bool IsValidEntryPoint(const int32 LinkId) const { return EntryPoints.Contains(LinkId); }

	void RegisterEntryPoint(const int32 ScriptOffset, const FName FunctionName)
	{
		EntryPoints.Add(ScriptOffset, FunctionName);
	}

	// Registers an association between an object (pin or node typically) and an associated class member property
	void RegisterClassPropertyAssociation(class UObject* TrueSourceObject, class UProperty* AssociatedProperty)
	{
		DebugObjectToPropertyMap.Add(TrueSourceObject, AssociatedProperty);
	}

	void RegisterClassPropertyAssociation(const UEdGraphPin* TrueSourcePin, class UProperty* AssociatedProperty)
	{
		if (TrueSourcePin)
		{
			DebugPinToPropertyMap.Add(TrueSourcePin, AssociatedProperty);
		}
	}

	// Registers an association between a UUID and a node
	void RegisterUUIDAssociation(UEdGraphNode* TrueSourceNode, int32 UUID)
	{
		DebugNodesAllocatedUniqueIDsMap.Add(UUID, TrueSourceNode);
	}

	// Returns the object that caused the specified property to be created (can return nullptr if the association is unknown)
	UObject* FindObjectThatCreatedProperty(class UProperty* AssociatedProperty) const
	{
		if (const TWeakObjectPtr<UObject>* pValue = DebugObjectToPropertyMap.FindKey(AssociatedProperty))
		{
			return pValue->Get();
		}
		else
		{
			return nullptr;
		}
	}

	// Returns the pin that caused the specified property to be created (can return nullptr if the association is unknown or the association is from an object instead)
	UEdGraphPin* FindPinThatCreatedProperty(class UProperty* AssociatedProperty) const
	{
		if (const FEdGraphPinReference* pValue = DebugPinToPropertyMap.FindKey(AssociatedProperty))
		{
			return pValue->Get();
		}
		else
		{
			return nullptr;
		}
	}

	void GenerateReversePropertyMap(TMap<UProperty*, UObject*>& PropertySourceMap)
	{
		for (TMap<TWeakObjectPtr<UObject>, UProperty*>::TIterator MapIt(DebugObjectToPropertyMap); MapIt; ++MapIt)
		{
			if (UObject* SourceObj = MapIt.Key().Get())
			{
				PropertySourceMap.Add(MapIt.Value(), SourceObj);
			}
		}
	}
#endif
};

USTRUCT()
struct ENGINE_API FEventGraphFastCallPair
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY()
	UFunction* FunctionToPatch;

	UPROPERTY()
	int32 EventGraphCallOffset;
};

/** A single changed Blueprint component property. */
USTRUCT()
struct ENGINE_API FBlueprintComponentChangedPropertyInfo
{
	GENERATED_USTRUCT_BODY()

	/** The name of the changed property. */
	UPROPERTY()
	FName PropertyName;

	/** The array index of the changed property. */
	UPROPERTY()
	int32 ArrayIndex;

	/** The parent struct (owner) of the changed property. */
	UPROPERTY()
	UStruct* PropertyScope;

	/** Default constructor. */
	FBlueprintComponentChangedPropertyInfo()
	{
		ArrayIndex = 0;
		PropertyScope = nullptr;
	}
};

/** Cooked data for a Blueprint component template. */
USTRUCT()
struct ENGINE_API FBlueprintCookedComponentInstancingData
{
	GENERATED_USTRUCT_BODY()

	/** Flag indicating whether or not this contains valid cooked data. Note that an empty changed property list can also be a valid template data context. */
	UPROPERTY()
	bool bIsValid;

	/** List of property info records with values that differ between the template and the component class CDO. This list will be generated at cook time. */
	UPROPERTY()
	TArray<struct FBlueprintComponentChangedPropertyInfo> ChangedPropertyList;

	/** Source template object name (recorded at load time and used for instancing). */
	FName ComponentTemplateName;

	/** Source template object class (recorded at load time and used for instancing). */
	UClass* ComponentTemplateClass;

	/** Source template object flags (recorded at load time and used for instancing). */
	EObjectFlags ComponentTemplateFlags;

	/** Default constructor. */
	FBlueprintCookedComponentInstancingData()
	{
		bIsValid = false;
		ComponentTemplateClass = nullptr;
		ComponentTemplateFlags = RF_NoFlags;
	}

	/** Builds/returns the internal property list that's used for serialization. This is a linked list of UProperty references. */
	const FCustomPropertyListNode* GetCachedPropertyListForSerialization() const;

	/** Called at load time to generate the internal cached property data stream from serialization of the source template object. */
	void LoadCachedPropertyDataForSerialization(UActorComponent* SourceTemplate);

	/** Returns the internal property data stream that's used for fast binary object serialization when instancing components at runtime. */
	const TArray<uint8>& GetCachedPropertyDataForSerialization() const { return CachedPropertyDataForSerialization; }

protected:
	/** Internal method used to help recursively build the cached property list for serialization. */
	void BuildCachedPropertyList(FCustomPropertyListNode** CurrentNode, const UStruct* CurrentScope, int32* CurrentSourceIdx = nullptr) const;

	/** Internal method used to help recursively build a cached sub property list from an array property for serialization. */
	void BuildCachedArrayPropertyList(const UArrayProperty* ArraySubPropertyNode, FCustomPropertyListNode** CurrentNode, int32* CurrentSourceIdx) const;

private:
	/** Internal property list that's used in binary object serialization at component instancing time. */
	mutable TIndirectArray<FCustomPropertyListNode> CachedPropertyListForSerialization;

	/** Internal property data stream that's used in binary object serialization at component instancing time. */
	TArray<uint8> CachedPropertyDataForSerialization;
};

UCLASS()
class ENGINE_API UBlueprintGeneratedClass : public UClass
{
	GENERATED_UCLASS_BODY()

public:

	UPROPERTY(AssetRegistrySearchable)
	int32	NumReplicatedProperties;

	/** Flag used to indicate if this class has a nativized parent in a cooked build. */
	UPROPERTY()
	uint8 bHasNativizedParent:1;

private:
	/** Flag to make sure the custom property list has been initialized */
	uint8 bCustomPropertyListForPostConstructionInitialized:1;

public:
	/** Array of objects containing information for dynamically binding delegates to functions in this blueprint */
	UPROPERTY()
	TArray<class UDynamicBlueprintBinding*> DynamicBindingObjects;

	/** Array of component template objects, used by AddComponent function */
	UPROPERTY()
	TArray<class UActorComponent*> ComponentTemplates;

	/** Array of templates for timelines that should be created */
	UPROPERTY()
	TArray<class UTimelineTemplate*> Timelines;

	/** 'Simple' construction script - graph of components to instance */
	UPROPERTY()
	class USimpleConstructionScript* SimpleConstructionScript;

	/** Stores data to override (in children classes) components (created by SCS) from parent classes */
	UPROPERTY()
	class UInheritableComponentHandler* InheritableComponentHandler;

	UPROPERTY()
	UStructProperty* UberGraphFramePointerProperty;

	UPROPERTY()
	UFunction* UberGraphFunction;

#if WITH_EDITORONLY_DATA
	// This is a list of event graph call function nodes that are simple (no argument) thunks into the event graph (typically used for animation delegates, etc...)
	// It is a deprecated list only used for backwards compatibility prior to VER_UE4_SERIALIZE_BLUEPRINT_EVENTGRAPH_FASTCALLS_IN_UFUNCTION.
	UPROPERTY()
	TArray<FEventGraphFastCallPair> FastCallPairs_DEPRECATED;
#endif

#if WITH_EDITORONLY_DATA
	UPROPERTY(Transient)
	UObject* OverridenArchetypeForCDO;

	/** Property guid map */
	UPROPERTY()
	TMap<FName,FGuid> PropertyGuids;
#endif //WITH_EDITORONLY_DATA

	// Mapping of changed properties & data to apply when instancing components in a cooked build (one entry per named AddComponent node template for fast lookup at runtime).
	// Note: This is not currently utilized by the editor; it is a runtime optimization for cooked builds only. It assumes that the component class structure does not change.
	UPROPERTY()
	TMap<FName, struct FBlueprintCookedComponentInstancingData> CookedComponentInstancingData;

	/** 
	 * Gets an array of all BPGeneratedClasses (including InClass as 0th element) parents of given generated class 
	 *
	 * @param InClass				The class to get the blueprint lineage for
	 * @param OutBlueprintParents	Array with the blueprints used to generate this class and its parents.  0th = this, Nth = least derived BP-based parent
	 * @return						true if there were no status errors in any of the parent blueprints, otherwise false
	 */
	static bool GetGeneratedClassesHierarchy(const UClass* InClass, TArray<const UBlueprintGeneratedClass*>& OutBPGClasses);

	UInheritableComponentHandler* GetInheritableComponentHandler(const bool bCreateIfNecessary = false);

	/** Find the object in the TemplateObjects array with the supplied name */
	UActorComponent* FindComponentTemplateByName(const FName& TemplateName) const;

	/** Create Timeline objects for this Actor based on the Timelines array*/
	static void CreateComponentsForActor(const UClass* ThisClass, AActor* Actor);
	static void CreateTimelineComponent(AActor* Actor, const UTimelineTemplate* TimelineTemplate);

	/** Check for and handle manual application of default value overrides to instanced component subobjects that were inherited from a nativized parent class */
	static void CheckAndApplyComponentTemplateOverrides(AActor* Actor);

	// UObject interface
	virtual void Serialize(FArchive& Ar) override;
	virtual void PostLoad() override;
	virtual void PostInitProperties() override;
	virtual void GetPreloadDependencies(TArray<UObject*>& OutDeps) override;
	virtual FPrimaryAssetId GetPrimaryAssetId() const override;
	virtual bool NeedsLoadForServer() const override;
	virtual bool NeedsLoadForClient() const override;
	virtual bool NeedsLoadForEditorGame() const override;
	virtual bool CanBeClusterRoot() const override;
	// End UObject interface
	
	// UClass interface
#if WITH_EDITOR
	virtual UClass* GetAuthoritativeClass() override;
	virtual void ConditionalRecompileClass(TArray<UObject*>* ObjLoaded) override;
	virtual void FlushCompilationQueueForLevel() override;
	virtual UObject* GetArchetypeForCDO() const override;
#endif //WITH_EDITOR
	virtual void SerializeDefaultObject(UObject* Object, FArchive& Ar) override;
	virtual void PostLoadDefaultObject(UObject* Object) override;
	virtual bool IsFunctionImplementedInBlueprint(FName InFunctionName) const override;
	virtual uint8* GetPersistentUberGraphFrame(UObject* Obj, UFunction* FuncToCheck) const override;
	virtual void CreatePersistentUberGraphFrame(UObject* Obj, bool bCreateOnlyIfEmpty = false, bool bSkipSuperClass = false, UClass* OldClass = nullptr) const override;
	virtual void DestroyPersistentUberGraphFrame(UObject* Obj, bool bSkipSuperClass = false) const override;
	virtual void Link(FArchive& Ar, bool bRelinkExistingProperties) override;
	virtual void PurgeClass(bool bRecompilingOnLoad) override;
	virtual void Bind() override;
	virtual void GetRequiredPreloadDependencies(TArray<UObject*>& DependenciesOut) override;
	virtual UObject* FindArchetype(UClass* ArchetypeClass, const FName ArchetypeName) const override;

	virtual void InitPropertiesFromCustomList(uint8* DataPtr, const uint8* DefaultDataPtr) override;

protected:

	virtual FName FindPropertyNameFromGuid(const FGuid& PropertyGuid) const override;
	virtual FGuid FindPropertyGuidFromName(const FName InName) const override;
	virtual bool ArePropertyGuidsAvailable() const override;
	// End UClass interface

	/**
	* Returns a linked list of properties with default values that differ from the parent default object. If non-NULL, only these properties will
	* be copied post-construction. Otherwise, all properties will be copied to the new instance, even if the default value matches the inherited default value.
	*/
	const FCustomPropertyListNode* GetCustomPropertyListForPostConstruction() const
	{
		return CustomPropertyListForPostConstruction.Num() > 0 ? *CustomPropertyListForPostConstruction.GetData() : nullptr;
	}

	/**
	* Helper method to assist with initializing object properties from an explicit list.
	*
	* @param	InPropertyList		only these properties will be copied from defaults
	* @param	InStruct			the current scope for which the given property list applies
	* @param	DataPtr				destination address (where to start copying values to)
	* @param	DefaultDataPtr		source address (where to start copying the defaults data from)
	*/
	static void InitPropertiesFromCustomList(const FCustomPropertyListNode* InPropertyList, UStruct* InStruct, uint8* DataPtr, const uint8* DefaultDataPtr);

	/**
	* Helper method to assist with initializing from an array property with an explicit item list.
	*
	* @param	ArrayProperty		the array property for which the given property list applies
	* @param	InPropertyList		only these properties (indices) will be copied from defaults
	* @param	DataPtr				destination address (where to start copying values to)
	* @param	DefaultDataPtr		source address (where to start copying the defaults data from)
	*/
	static void InitArrayPropertyFromCustomList(const UArrayProperty* ArrayProperty, const FCustomPropertyListNode* InPropertyList, uint8* DataPtr, const uint8* DefaultDataPtr);

public:

	/** Called when the custom list of properties used during post-construct initialization needs to be rebuilt (e.g. after serialization and recompilation). */
	void UpdateCustomPropertyListForPostConstruction();

	static void AddReferencedObjectsInUbergraphFrame(UObject* InThis, FReferenceCollector& Collector);

	static FName GetUberGraphFrameName();
	static bool UsePersistentUberGraphFrame();

#if WITH_EDITORONLY_DATA
	FBlueprintDebugData DebugData;


	FBlueprintDebugData& GetDebugData()
	{
		return DebugData;
	}
#endif

	/** Bind functions on supplied actor to delegates */
	static void BindDynamicDelegates(const UClass* ThisClass, UObject* InInstance);

	// Finds the desired dynamic binding object for this blueprint generated class
	static UDynamicBlueprintBinding* GetDynamicBindingObject(const UClass* ThisClass, UClass* BindingClass);

#if WITH_EDITOR
	/** Unbind functions on supplied actor from delegates */
	static void UnbindDynamicDelegates(const UClass* ThisClass, UObject* InInstance);

	/** Unbind functions on supplied actor from delegates tied to a specific property */
	void UnbindDynamicDelegatesForProperty(UObject* InInstance, const UObjectProperty* InObjectProperty);
#endif

	/** called to gather blueprint replicated properties */
	virtual void GetLifetimeBlueprintReplicationList(TArray<class FLifetimeProperty>& OutLifetimeProps) const;
	/** called prior to replication of an instance of this BP class */
	virtual void InstancePreReplication(UObject* Obj, class IRepChangedPropertyTracker& ChangedPropertyTracker) const
	{
		UBlueprintGeneratedClass* SuperBPClass = Cast<UBlueprintGeneratedClass>(GetSuperStruct());
		if (SuperBPClass != NULL)
		{
			SuperBPClass->InstancePreReplication(Obj, ChangedPropertyTracker);
		}
	}

protected:
	/** Internal helper method used to recursively build the custom property list that's used for post-construct initialization. */
	bool BuildCustomPropertyListForPostConstruction(FCustomPropertyListNode*& InPropertyList, UStruct* InStruct, const uint8* DataPtr, const uint8* DefaultDataPtr);

	/** Internal helper method used to recursively build a custom property list from an array property used for post-construct initialization. */
	bool BuildCustomArrayPropertyListForPostConstruction(UArrayProperty* ArrayProperty, FCustomPropertyListNode*& InPropertyList, const uint8* DataPtr, const uint8* DefaultDataPtr, int32 StartIndex = 0);

private:
	/** List of native class-owned properties that differ from defaults. This is used to optimize property initialization during post-construction by minimizing the number of native class-owned property values that get copied to the new instance. */
	TIndirectArray<FCustomPropertyListNode> CustomPropertyListForPostConstruction;
	/** In some cases UObject::ConditionalPostLoad() code calls PostLoadDefaultObject() on a class that's still being serialized. */
	FCriticalSection SerializeAndPostLoadCritical;
};
