// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "NiagaraTypes.h"
#include "NiagaraCommon.generated.h"

class UNiagaraSystem;
class UNiagaraScript;
class UNiagaraDataInterface;
class UNiagaraEmitter;
class FNiagaraSystemInstance;
class UNiagaraParameterCollection;

const uint32 NIAGARA_COMPUTE_THREADGROUP_SIZE = 16;
const uint32 NIAGARA_MAX_COMPUTE_THREADGROUPS = 2048;

enum ENiagaraBaseTypes
{
	NBT_Float,
	NBT_Int32,
	NBT_Bool,
	NBT_Max,
};

UENUM()
enum class ENiagaraSimTarget : uint8
{
	CPUSim,
	GPUComputeSim,
	DynamicLoadBalancedSim
};



UENUM()
enum class ENiagaraDataSetType : uint8
{
	ParticleData,
	Shared,
	Event,
};


UENUM()
enum class ENiagaraInputNodeUsage : uint8
{
	Undefined = 0,
	Parameter,
	Attribute,
	SystemConstant
};

/**
* Enumerates states a Niagara script can be in.
*/
UENUM()
enum class ENiagaraScriptCompileStatus : uint8
{
	/** Niagara script is in an unknown state. */
	NCS_Unknown,
	/** Niagara script has been modified but not recompiled. */
	NCS_Dirty,
	/** Niagara script tried but failed to be compiled. */
	NCS_Error,
	/** Niagara script has been compiled since it was last modified. */
	NCS_UpToDate,
	/** Niagara script is in the process of being created for the first time. */
	NCS_BeingCreated,
	/** Niagara script has been compiled since it was last modified. There are warnings. */
	NCS_UpToDateWithWarnings,
	/** Niagara script has been compiled for compute since it was last modified. There are warnings. */
	NCS_ComputeUpToDateWithWarnings,
	NCS_MAX,
};

USTRUCT()
struct FNiagaraDataSetID
{
	GENERATED_USTRUCT_BODY()

	FNiagaraDataSetID()
	: Name(NAME_None)
	, Type(ENiagaraDataSetType::Event)
	{}

	FNiagaraDataSetID(FName InName, ENiagaraDataSetType InType)
		: Name(InName)
		, Type(InType)
	{}

	UPROPERTY(EditAnywhere, Category = "Data Set")
	FName Name;

	UPROPERTY()
	ENiagaraDataSetType Type;

	FORCEINLINE bool operator==(const FNiagaraDataSetID& Other)const
	{
		return Name == Other.Name && Type == Other.Type;
	}

	FORCEINLINE bool operator!=(const FNiagaraDataSetID& Other)const
	{
		return !(*this == Other);
	}
};


FORCEINLINE FArchive& operator<<(FArchive& Ar, FNiagaraDataSetID& VarInfo)
{
	Ar << VarInfo.Name << VarInfo.Type;
	return Ar;
}

FORCEINLINE uint32 GetTypeHash(const FNiagaraDataSetID& Var)
{
	return HashCombine(GetTypeHash(Var.Name), (uint32)Var.Type);
}

USTRUCT()
struct FNiagaraDataSetProperties
{
	GENERATED_BODY()
	
	UPROPERTY(VisibleAnywhere, Category = "Data Set")
	FNiagaraDataSetID ID;

	UPROPERTY()
	TArray<FNiagaraVariable> Variables;
};

/** Information about an input or output of a Niagara operation node. */
class FNiagaraOpInOutInfo
{
public:
	FName Name;
	FNiagaraTypeDefinition DataType;
	FText FriendlyName;
	FText Description;
	FString Default;
	FString HlslSnippet;

	FNiagaraOpInOutInfo(FName InName, FNiagaraTypeDefinition InType, FText InFriendlyName, FText InDescription, FString InDefault, FString InHlslSnippet = TEXT(""))
		: Name(InName)
		, DataType(InType)
		, FriendlyName(InFriendlyName)
		, Description(InDescription)
		, Default(InDefault)
		, HlslSnippet(InHlslSnippet)
	{

	}
};


/** Struct containing usage information about a script. Things such as whether it reads attribute data, reads or writes events data etc.*/
USTRUCT()
struct FNiagaraScriptDataUsageInfo
{
	GENERATED_BODY()

		FNiagaraScriptDataUsageInfo()
		: bReadsAttriubteData(false)
	{}

	/** If true, this script reads attribute data. */
	UPROPERTY()
		bool bReadsAttriubteData;
};


USTRUCT()
struct NIAGARA_API FNiagaraFunctionSignature
{
	GENERATED_BODY()

	/** Name of the function. */
	UPROPERTY()
	FName Name;
	/** Input parameters to this function. */
	UPROPERTY()
	TArray<FNiagaraVariable> Inputs;
	/** Input parameters of this function. */
	UPROPERTY()
	TArray<FNiagaraVariable> Outputs;
	/** Id of the owner is this is a member function. */
	UPROPERTY()
	FName OwnerName;
	UPROPERTY()
	bool bRequiresContext;
	/** True if this is the signature for a "member" function of a data interface. If this is true, the first input is the owner. */
	UPROPERTY()
	bool bMemberFunction;

	/** Localized description of this node. Note that this is *not* used during the operator == below since it may vary from culture to culture.*/
#if WITH_EDITORONLY_DATA
	UPROPERTY()
	FText Description;
#endif

	FNiagaraFunctionSignature() {}
	FNiagaraFunctionSignature(FName InName, TArray<FNiagaraVariable>& InInputs, TArray<FNiagaraVariable>& InOutputs, FName InSource, bool bInRequiresContext, bool bInMemberFunction)
		: Name(InName)
		, Inputs(InInputs)
		, Outputs(InOutputs)
		, bRequiresContext(bInRequiresContext)
		, bMemberFunction(bInMemberFunction)
	{

	}

	bool operator==(const FNiagaraFunctionSignature& Other) const
	{
		return Name.ToString().Equals(Other.Name.ToString()) && Inputs == Other.Inputs && Outputs == Other.Outputs && bRequiresContext == Other.bRequiresContext && bMemberFunction == Other.bMemberFunction && OwnerName == Other.OwnerName;
	}

	FString GetName()const { return Name.ToString(); }

	void SetDescription(const FText& Desc)
	{
	#if WITH_EDITORONLY_DATA
		Description = Desc;
	#endif
	}
	FText GetDescription() const
	{
	#if WITH_EDITORONLY_DATA
		return Description;
	#else
		return FText::FromName(Name);
	#endif
	}
	bool IsValid()const { return Name != NAME_None && (Inputs.Num() > 0 || Outputs.Num() > 0); }
};



USTRUCT()
struct NIAGARA_API FNiagaraScriptDataInterfaceInfo
{
	GENERATED_USTRUCT_BODY()
public:
	FNiagaraScriptDataInterfaceInfo()
		: DataInterface(nullptr)
		, Name(NAME_None)
		, UserPtrIdx(INDEX_NONE)
	{

	}

	UPROPERTY(Instanced)
	class UNiagaraDataInterface* DataInterface;
	
	UPROPERTY()
	FName Name;

	/** Index of the user pointer for this data interface. */
	UPROPERTY()
	int32 UserPtrIdx;

	TArray<FNiagaraFunctionSignature> RegisteredFunctions;

	//TODO: Allow data interfaces to own datasets
	void CopyTo(FNiagaraScriptDataInterfaceInfo* Destination, UObject* Outer) const;
};

USTRUCT()
struct FNiagaraStatScope
{
	GENERATED_USTRUCT_BODY();

	FNiagaraStatScope() {}
	FNiagaraStatScope(FName InFullName, FText InFriendlyName):FullName(InFullName), FriendlyName(InFriendlyName){}

	UPROPERTY()
	FName FullName;

	UPROPERTY()
	FText FriendlyName;

	bool operator==(const FNiagaraStatScope& Other) const { return FullName == Other.FullName; }
};

USTRUCT()
struct FVMExternalFunctionBindingInfo
{
	GENERATED_USTRUCT_BODY();

	UPROPERTY()
	FName Name;

	UPROPERTY()
	FName OwnerName;

	UPROPERTY()
	TArray<bool> InputParamLocations;

	UPROPERTY()
	int32 NumOutputs;

	FORCEINLINE int32 GetNumInputs()const { return InputParamLocations.Num(); }
	FORCEINLINE int32 GetNumOutputs()const { return NumOutputs; }
};

struct NIAGARA_API FNiagaraSystemUpdateContext
{
	FNiagaraSystemUpdateContext(const UNiagaraSystem* System, bool bReInit) { Add(System, bReInit); }
	FNiagaraSystemUpdateContext(const UNiagaraEmitter* Emitter, bool bReInit) { Add(Emitter, bReInit); }
	FNiagaraSystemUpdateContext(const UNiagaraScript* Script, bool bReInit) { Add(Script, bReInit); }
	//FNiagaraSystemUpdateContext(UNiagaraDataInterface* Interface, bool bReinit) : Add(Interface, bReinit) {}
	FNiagaraSystemUpdateContext(const UNiagaraParameterCollection* Collection, bool bReInit) { Add(Collection, bReInit); }

	~FNiagaraSystemUpdateContext();

	void Add(const UNiagaraSystem* System, bool bReInit);
	void Add(const UNiagaraEmitter* Emitter, bool bReInit);
	void Add(const UNiagaraScript* Script, bool bReInit);
	//void Add(UNiagaraDataInterface* Interface, bool bReinit);
	void Add(const UNiagaraParameterCollection* Collection, bool bReInit);

private:
	void AddInternal(class UNiagaraComponent* Comp, bool bReInit);

	FNiagaraSystemUpdateContext() { }
	FNiagaraSystemUpdateContext(FNiagaraSystemUpdateContext& Other) { }

	TArray<UNiagaraComponent*> ComponentsToReset;
	TArray<UNiagaraComponent*> ComponentsToReInit;

	//TODO: When we allow component less systems we'll also want to find and reset those.
};



/** Defines different usages for a niagara script. */
UENUM()
enum class ENiagaraScriptUsage : uint8
{
	/** The script defines a function for use in modules. */
	Function,
	/** The script defines a module for use in particle, emitter, or system scripts. */
	Module,
	/** The script defines a dynamic input for use in particle, emitter, or system scripts. */
	DynamicInput,
	/** The script is called when spawning particles. */
	ParticleSpawnScript UMETA(Hidden),
	/** Particle spawn script that handles intra-frame spawning and also pulls in the update script. */
	ParticleSpawnScriptInterpolated UMETA(Hidden),
	/** The script is called to update particles every frame. */
	ParticleUpdateScript UMETA(Hidden),
	/** The script is called to update particles in response to an event. */
	ParticleEventScript UMETA(Hidden),
	/** The script is called once when the emitter spawns. */
	EmitterSpawnScript UMETA(Hidden),
	/** The script is called every frame to tick the emitter. */
	EmitterUpdateScript UMETA(Hidden),
	/** The script is called once when the system spawns. */
	SystemSpawnScript UMETA(Hidden),
	/** The script is called every frame to tick the system. */
	SystemUpdateScript UMETA(Hidden),
};


/** Defines all you need to know about a variable.*/
USTRUCT()
struct FNiagaraVariableInfo
{
	GENERATED_USTRUCT_BODY();

	FNiagaraVariableInfo() : DataInterface(nullptr) {}

	UPROPERTY()
	FNiagaraVariable Variable;

	UPROPERTY()
	FText Definition;

	UPROPERTY()
	UNiagaraDataInterface* DataInterface;
};