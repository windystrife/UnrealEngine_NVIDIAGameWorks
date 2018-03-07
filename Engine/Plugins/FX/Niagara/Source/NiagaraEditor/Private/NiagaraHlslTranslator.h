// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Kismet2/CompilerResultsLog.h"
#include "NiagaraScript.h"
#include "NiagaraParameterMapHistory.h"
#include "EdGraphUtilities.h"
#include "UObject/UObjectHash.h"
#include "ComponentReregisterContext.h"
#include "NiagaraShaderCompilationManager.h"
#include "NiagaraDataInterface.h"
#include "TickableEditorObject.h"

class Error;
class UNiagaraGraph;
class UNiagaraNode;
class UNiagaraNodeFunctionCall;
class UNiagaraNodeOutput;
class UNiagaraScriptSource;
class UNiagaraNodeEmitter;


// handles finished shader compile jobs, applying of the shaders to their scripts, and some error handling
//
class FNiagaraShaderProcessorTickable : FTickableEditorObject
{
	virtual bool IsTickable() const override
	{
		return true;
	}

	virtual void Tick(float DeltaSeconds) override
	{
		GNiagaraShaderCompilationManager.Tick(DeltaSeconds);
		GNiagaraShaderCompilationManager.ProcessAsyncResults();
	}

	virtual TStatId GetStatId() const
	{
		RETURN_QUICK_DECLARE_CYCLE_STAT(FNiagaraShaderQueueTickable, STATGROUP_Tickables);
	}
};


UENUM()
enum class ENiagaraDataSetAccessMode : uint8
{
	/** Data set reads and writes use shared counters to add and remove the end of available data. Writes are conditional and read */
	AppendConsume,
	/** Data set is accessed directly at a specific index. */
	Direct,

	Num UMETA(Hidden),
};


/** Defines information about the results of a Niagara script compile. */
struct FNiagaraTranslateResults
{
	/** Whether or not HLSL generation was successful */
	bool bHLSLGenSucceeded;

	/** A results log with messages, warnings, and errors which occurred during the compile. */
	FCompilerResultsLog* MessageLog;

	/** A string representation of the compilation output. */
	FString OutputHLSL;

	FNiagaraTranslateResults(FCompilerResultsLog* InMessageLog = nullptr)
		: MessageLog(InMessageLog)
	{
	}

	static ENiagaraScriptCompileStatus TranslateResultsToSummary(const FNiagaraTranslateResults* CompileResults);
};


/** Data which is generated from the hlsl by the VectorVMBackend and fed back into the */
struct FNiagaraTranslatorOutput
{
	FNiagaraTranslatorOutput() : NumUserPtrs(0), bReadsAttributeData(false){}

	/** All external parameters used in the graph. */
	FNiagaraParameters Parameters;

	/** All internal constants used in the graph. */
	FNiagaraParameters InternalParameters;

	/** Data sets this script reads. */
	TArray<FNiagaraDataSetProperties> DataSetReads;

	/** Data sets this script writes. */
	TArray<FNiagaraDataSetProperties> DataSetWrites;

	/* Per instance attributes */
	TArray<FNiagaraVariable> Attributes;

	FNiagaraScriptDataUsageInfo DataUsage;

	TArray<FNiagaraScriptDataInterfaceInfo> DataInterfaceInfo;

	/** Ordered table of functions actually called by the VM script. */
	struct FCalledVMFunction
	{
		FString Name;
		TArray<bool> InputParamLocations;
		int32 NumOutputs;
		FCalledVMFunction() :NumOutputs(0) {}
	};
	TArray<FCalledVMFunction> CalledVMFunctionTable;

	FString Errors;

	/** Number of user pointers. For passing things like data interface instance data etc. */
	int32 NumUserPtrs;

	bool bReadsAttributeData;
};



enum class ENiagaraCodeChunkMode : uint8
{
	Uniform,
	Source,
	Body,
	Num,
};

FORCEINLINE uint32 GetTypeHash(const FNiagaraFunctionSignature& Sig)
{
	uint32 Hash = GetTypeHash(Sig.Name);
	for (const FNiagaraVariable& Var : Sig.Inputs)
	{
		Hash = HashCombine(Hash, GetTypeHash(Var));
	}
	for (const FNiagaraVariable& Var : Sig.Outputs)
	{
		Hash = HashCombine(Hash, GetTypeHash(Var));
	}
	Hash = HashCombine(Hash, GetTypeHash(Sig.OwnerName));
	return Hash;
}

struct FNiagaraCodeChunk
{
	/** Symbol name for the chunk. Cam be empty for some types of chunk. */
	FString SymbolName;
	/** Format definition for incorporating SourceChunks into the final code for this chunk. */
	FString Definition;
	/** The returned data type of this chunk. */
	FNiagaraTypeDefinition Type;
	/** If this chunk should declare it's symbol name. */
	bool bDecl;
	/** If the chunk is unterminated (no semicolon, because it's a scope or similar */
	bool bIsTerminated;
	/** Chunks used as input for this chunk. */
	TArray<int32> SourceChunks;

	ENiagaraCodeChunkMode Mode;

	FNiagaraCodeChunk()
		: bDecl(true)
		, bIsTerminated(true)
		, Mode(ENiagaraCodeChunkMode::Num)
	{
		Type = FNiagaraTypeDefinition::GetFloatDef();
	}

	void AddSourceChunk(int32 ChunkIdx)
	{
		SourceChunks.Add(ChunkIdx);
	}

	int32 GetSourceChunk(int32 i)
	{
		return SourceChunks[i];
	}

	void ReplaceSourceIndex(int32 SourceIdx, int32 NewIdx)
	{
		SourceChunks[SourceIdx] = NewIdx;
	}

	bool operator==(const FNiagaraCodeChunk& Other)
	{
		return SymbolName == Other.SymbolName &&
			Definition == Other.Definition &&
			Mode == Other.Mode &&
			Type == Other.Type &&
			bDecl == Other.bDecl &&
			SourceChunks == Other.SourceChunks;
	}
};

class NIAGARAEDITOR_API FHlslNiagaraTranslatorOptions
{
public:
	FHlslNiagaraTranslatorOptions()
		: SimTarget(ENiagaraSimTarget::CPUSim)
	{

	}

	ENiagaraSimTarget SimTarget;

	/** Any parameters in these namespaces will be pulled from an "InstanceParameters" dataset rather than from the uniform table. */
	TArray<FString> InstanceParameterNamespaces;
};

class NIAGARAEDITOR_API FHlslNiagaraTranslator
{
public:

	struct FDataSetAccessInfo
	{
		//Variables accessed.
		TArray<FNiagaraVariable> Variables;
		/** Code chunks relating to this access. */
		TArray<int32> CodeChunks;
	};

protected:

	/** The script we are compiling. */
	UNiagaraScript* Script;

	FHlslNiagaraTranslatorOptions TranslationOptions;

	const class UEdGraphSchema_Niagara* Schema;

	/** The set of all generated code chunks for this script. */
	TArray<FNiagaraCodeChunk> CodeChunks;

	/** Array of code chunks of each different type. */
	TArray<int32> ChunksByMode[(int32)ENiagaraCodeChunkMode::Num];

	/**
	Map of Pins to compiled code chunks. Allows easy reuse of previously compiled pins.
	A stack so that we can track pin reuse within function calls but not have cached pins cross talk with subsequent calls to the same funciton.
	*/
	TArray<TMap<UEdGraphPin*, int32>> PinToCodeChunks;

	/** The combined output of the compilation of this script. This is temporary and will be reworked soon. */
	FNiagaraTranslatorOutput CompilationOutput;

	/** Message log. Automatically handles marking the NodeGraph with errors. */
	FCompilerResultsLog MessageLog;

	/** Captures information about a script compile. */
	FNiagaraTranslateResults TranslateResults;

	TMap<FName, uint32> GeneratedSymbolCounts;

	FDataSetAccessInfo InstanceRead;
	FDataSetAccessInfo InstanceWrite;

	TMap<FNiagaraDataSetID, TMap<int32, FDataSetAccessInfo>> DataSetReadInfo[(int32)ENiagaraDataSetAccessMode::Num];
	TMap<FNiagaraDataSetID, TMap<int32, FDataSetAccessInfo>> DataSetWriteInfo[(int32)ENiagaraDataSetAccessMode::Num];
	TMap<FNiagaraDataSetID, int32> DataSetWriteConditionalInfo[(int32)ENiagaraDataSetAccessMode::Num];

	FString GetDataSetAccessSymbol(FNiagaraDataSetID DataSet, int32 IndexChunk, bool bRead);
	FORCEINLINE FNiagaraDataSetID GetInstanceDataSetID()const { return FNiagaraDataSetID(TEXT("DataInstance"), ENiagaraDataSetType::ParticleData); }
	FORCEINLINE FNiagaraDataSetID GetSystemEngineDataSetID()const { return FNiagaraDataSetID(TEXT("Engine"), ENiagaraDataSetType::ParticleData); }
	FORCEINLINE FNiagaraDataSetID GetSystemUserDataSetID()const { return FNiagaraDataSetID(TEXT("User"), ENiagaraDataSetType::ParticleData); }

	/** All functions called in the script. */
	TMap<FNiagaraFunctionSignature, FString> Functions;
	/** Map of function graphs we've seen before and already pre-processed. */
	TMap<const UNiagaraGraph*, UNiagaraGraph*> PreprocessedFunctions;

	void RegisterFunctionCall(ENiagaraScriptUsage ScriptUsage, const FString& InName, const FString& InFullName, UNiagaraScriptSource* Source, FNiagaraFunctionSignature& InSignature, TArray<int32>& Inputs, const TArray<UEdGraphPin*>& CallInputs, const TArray<UEdGraphPin*>& CallOutputs,
		FNiagaraFunctionSignature& OutSignature);
	void GenerateFunctionCall(FNiagaraFunctionSignature& FunctionSignature, TArray<int32>& Inputs, TArray<int32>& Outputs);
	FString GetFunctionSignature(const FNiagaraFunctionSignature& Sig);

	/** Compiles an output Pin on a graph node. Caches the result for any future inputs connected to it. */
	int32 CompileOutputPin(UEdGraphPin* Pin);

	void WriteDataSetContextVars(TMap<FNiagaraDataSetID, TMap<int32, FDataSetAccessInfo>>& DataSetAccessInfo, bool bRead, FString &HlslOutput);
	void WriteDataSetStructDeclarations(TMap<FNiagaraDataSetID, TMap<int32, FDataSetAccessInfo>>& DataSetAccessInfo, bool bRead, FString &HlslOutput);
	void DecomposeVariableAccess(const UStruct* Struct, bool bRead, FString IndexSymbol, FString HLSLString);

	FString GetUniqueSymbolName(FName BaseName);

	/** Stack of all function params. */
	struct FFunctionContext
	{
		FString Name;
		FNiagaraFunctionSignature& Signature;
		TArray<int32>& Inputs;
		FFunctionContext(const FString& InName, FNiagaraFunctionSignature& InSig, TArray<int32>& InInputs)
			: Name(InName)
			, Signature(InSig)
			, Inputs(InInputs)
		{}
	};
	TArray<FFunctionContext> FunctionContextStack;
	const FFunctionContext* FunctionCtx()const { return FunctionContextStack.Num() > 0 ? &FunctionContextStack.Last() : nullptr; }
	void EnterFunction(const FString& Name, FNiagaraFunctionSignature& Signature, TArray<int32>& Inputs);
	void ExitFunction();
	FString GetCallstack();

	void EnterStatsScope(FNiagaraStatScope StatScope);
	void ExitStatsScope();

	FString GeneratedConstantString(float Constant);
	FString GeneratedConstantString(FVector4 Constant);

	/* Add a chunk that is not written to the source, only used as a source chunk for others. */
	int32 AddSourceChunk(FString SymbolName, const FNiagaraTypeDefinition& Type);

	/** Add a chunk defining a uniform value. */
	int32 AddUniformChunk(FString SymbolName, const FNiagaraTypeDefinition& Type);

	/* Add a chunk that is written to the body of the shader code. */
	int32 AddBodyChunk(FString SymbolName, FString Definition, const FNiagaraTypeDefinition& Type, TArray<int32>& SourceChunks, bool bDecl = true, bool bIsTerminated = true);
	int32 AddBodyChunk(FString SymbolName, FString Definition, const FNiagaraTypeDefinition& Type, int32 SourceChunk, bool bDecl = true, bool bIsTerminated = true);
	int32 AddBodyChunk(FString SymbolName, FString Definition, const FNiagaraTypeDefinition& Type, bool bDecl = true, bool bIsTerminated = true);


	FString GetFunctionDefinitions();
public:

	FHlslNiagaraTranslator();
	virtual ~FHlslNiagaraTranslator() {}

	virtual const FNiagaraTranslateResults &Translate(UNiagaraScript* InScript, FHlslNiagaraTranslatorOptions Options);
	FNiagaraTranslatorOutput &GetTranslateOutput() { return CompilationOutput; }

	virtual int32 CompilePin(UEdGraphPin* Pin);

	virtual int32 RegisterDataInterface(FNiagaraVariable& Var, UNiagaraDataInterface* DataInterface);

	virtual void Operation(class UNiagaraNodeOp* Operation, TArray<int32>& Inputs, TArray<int32>& Outputs);
	virtual void Output(UNiagaraNodeOutput* OutputNode, const TArray<int32>& ComputedInputs);

	virtual int32 GetParameter(const FNiagaraVariable& Parameter);

	virtual int32 GetAttribute(const FNiagaraVariable& Attribute);

	virtual int32 GetConstant(const FNiagaraVariable& Constant);
	
	virtual void ReadDataSet(const FNiagaraDataSetID DataSet, const TArray<FNiagaraVariable>& Variable, ENiagaraDataSetAccessMode AccessMode, int32 InputChunk, TArray<int32>& Outputs);
	virtual void WriteDataSet(const FNiagaraDataSetID DataSet, const TArray<FNiagaraVariable>& Variable, ENiagaraDataSetAccessMode AccessMode, const TArray<int32>& Inputs);
	virtual void ParameterMapSet(class UNiagaraNodeParameterMapSet* SetNode, TArray<int32>& Inputs, TArray<int32>& Outputs);
	virtual void ParameterMapGet(class UNiagaraNodeParameterMapGet* GetNode, TArray<int32>& Inputs, TArray<int32>& Outputs);
	virtual void ParameterCollection(class UNiagaraNodeParameterCollection* CollectionNode, TArray<int32>& Outputs);
	virtual void Emitter(class UNiagaraNodeEmitter* GetNode, TArray<int32>& Inputs, TArray<int32>& Outputs);

	void DefineInterpolatedParametersFunction(FString &HlslOutput);
	void DefineDataSetReadFunction(FString &HlslOutput, TArray<FNiagaraDataSetID> &ReadDataSets);
	void DefineDataSetWriteFunction(FString &HlslOutput, TArray<FNiagaraDataSetProperties> &WriteDataSets, TArray<int32>& WriteConditionVarIndices);
	void DefineMain(FString &HLSLOutput, TArray<TArray<FNiagaraVariable>* > &InstanceReadVars, TArray<FNiagaraDataSetID>& ReadIds, TArray<TArray<FNiagaraVariable>* > &InstanceWriteVars, TArray<FNiagaraDataSetID>& WriteIds);
	void DefineDataSetVariableReads(FString &HLSLOutput, FNiagaraDataSetID& Id, int32 DataSetIndex, TArray<FNiagaraVariable> & ReadVars);
	void DefineDataSetVariableWrites(FString &HlslOutput, FNiagaraDataSetID& Id, int32 DataSetIndex, TArray<FNiagaraVariable>& WriteVars);
	void DefineDataInterfaceHLSL(FString &HlslOutput);
	TArray< TArray<DIGPUBufferParamDescriptor> >&GetDatainterfaceBufferDescriptors() { return DIBufferDescriptors; }

	// Format string should have up to 5 entries, {{0} = Computed Variable Suffix, {1} = Float or Int, {2} = Data Set Index, {3} = Register Index, {4} Default value for that type.
	void GatherVariableForDataSetAccess(const FNiagaraVariable& Variable, FString Format, int32& RegisterIdx, int32 DataSetIndex, FString InstanceIdxSymbol, FString &HlslOutput);
	void GatherComponentsForDataSetAccess(const UScriptStruct* Struct, FString VariableSymbol, bool bMatrixRoot, TArray<FString>& Components, TArray<ENiagaraBaseTypes>& Types);

	FString CompileDataInterfaceFunction(UNiagaraDataInterface* DataInterface, FNiagaraFunctionSignature& Signature);

	virtual void FunctionCall(UNiagaraNodeFunctionCall* FunctionNode, TArray<int32>& Inputs, TArray<int32>& Outputs);

	virtual void Convert(class UNiagaraNodeConvert* Convert, TArray <int32>& Inputs, TArray<int32>& Outputs);
	virtual void If(TArray<FNiagaraVariable>& Vars, int32 Condition, TArray<int32>& PathA, TArray<int32>& PathB, TArray<int32>& Outputs);

	virtual void Error(FText ErrorText, UNiagaraNode* Node, UEdGraphPin* Pin);
	virtual void Warning(FText WarningText, UNiagaraNode* Node, UEdGraphPin* Pin);

	virtual bool GetFunctionParameter(const FNiagaraVariable& Parameter, int32& OutParam)const;

	virtual bool CanReadAttributes()const;
	virtual ENiagaraScriptUsage GetTargetUsage() const;
	int32 GetTargetOccurrence() const;

	static bool IsBuiltInHlslType(FNiagaraTypeDefinition Type);
	static FString GetStructHlslTypeName(FNiagaraTypeDefinition Type);
	static FString GetPropertyHlslTypeName(const UProperty* Property);
	static FString BuildHLSLStructDecl(FNiagaraTypeDefinition Type);
	static FString GetHlslDefaultForType(FNiagaraTypeDefinition Type);
	static bool IsHlslBuiltinVector(FNiagaraTypeDefinition Type);
	static TArray<FName> ConditionPropertyPath(const FNiagaraTypeDefinition& Type, const TArray<FName>& InPath);


	static FString GetSanitizedSymbolName(FString SymbolName);

	bool AddStructToDefinitionSet(const FNiagaraTypeDefinition& TypeDef);

	FString &GetTranslatedHLSL()
	{
		return HlslOutput;
	}

	static FString GetFunctionSignatureSymbol(const FNiagaraFunctionSignature& Sig);

	/** ID to allow visitors over the node graph to mark nodes as visited. */
	int32 VisitID;

private:
	bool ShouldConsiderTargetParameterMap(ENiagaraScriptUsage InUsage) const;
	FString BuildParameterMapHlslDefinitions(TArray<FNiagaraVariable>& PrimaryDataSetOutputEntries, TArray<TPair<FNiagaraVariable, const UEdGraphPin*> >& MissingDefaults);
	FString BuildMissingDefaults(TArray<TPair<FNiagaraVariable, const UEdGraphPin*> >& MissingDefaults);

	void HandleNamespacedExternalVariablesToDataSetRead(TArray<FNiagaraVariable>& InDataSetVars, FString InNamespaceStr);

	FString ComputeMatrixColumnAccess(const FString& Name);
	FString ComputeMatrixRowAccess(const FString& Name);
	
	// Add a raw float constant chunk
	int32 GetConstantFloat(float InValue);

	FNiagaraTypeDefinition GetChildType(const FNiagaraTypeDefinition& BaseType, const FName& PropertyName);
	FString NamePathToString(const FString& Prefix, const FNiagaraTypeDefinition& RootType, const TArray<FName>& NamePath);
	FString GenerateAssignment(const FNiagaraTypeDefinition& SrcType, const TArray<FName>& SrcPath, const FNiagaraTypeDefinition& DestType, const TArray<FName>& DestPath);

	//Generates the code for the passed chunk.
	FString GetCode(FNiagaraCodeChunk& Chunk);
	FString GetCode(int32 ChunkIdx);
	//Retreives the code for this chunk being used as a source for another chunk
	FString GetCodeAsSource(int32 ChunkIdx);

	// Convert a variable with actual data into a constant string
	FString GenerateConstantString(const FNiagaraVariable& Constant);

	// Takes the current script state (interpolated or not) and determines the correct context variable.
	FString GetParameterMapInstanceName(int32 ParamMapHistoryIdx);
	
	// Register a System/Engine/read-only variable in its namespaced form
	bool ParameterMapRegisterExternalConstantNamespaceVariable(FNiagaraVariable InVariable, UNiagaraNode* InNode, int32 InParamMapHistoryIdx, int32& Output, UEdGraphPin* InDefaultPin);

	// Register an attribute in its non namespaced form
	bool ParameterMapRegisterNamespaceAttributeVariable(const FNiagaraVariable& InVariable, UNiagaraNode* InNode, int32 InParamMapHistoryIdx, int32& Output);
	// Register an attribute in its namespaced form
	bool ParameterMapRegisterUniformAttributeVariable(const FNiagaraVariable& InVariable, UNiagaraNode* InNode, int32 InParamMapHistoryIdx, int32& Output);

	bool ValidateTypePins(UNiagaraNode* NodeToValidate);
	void GenerateFunctionSignature(ENiagaraScriptUsage ScriptUsage, FString InName, const FString& InFullName, UNiagaraGraph* FuncGraph, TArray<int32>& Inputs, bool bHadNumericInputs, FNiagaraFunctionSignature& OutSig)const;

	UNiagaraGraph* CloneGraphAndPrepareForCompilation(UNiagaraScript* InScript, UNiagaraScriptSource* InSource, bool bClearErrors);

	bool ShouldInterpolateParameter(const FNiagaraVariable& Parameter);

	bool IsBulkSystemScript() const;

	/** Map of symbol names to count of times it's been used. Used for generating unique symbol names. */
	TMap<FName, uint32> SymbolCounts;

	//Set of non-builtin structs we have to define in hlsl.
	TArray<FNiagaraTypeDefinition> StructsToDefine;

	// Keep track of all the paths that the parameter maps can take through the graph.
	TArray<FNiagaraParameterMapHistory> ParamMapHistories;

	// Keep track of the other output nodes in the graph's histories so that we can make sure to 
	// create any variables that are needed downstream.
	TArray<FNiagaraParameterMapHistory> OtherOutputParamMapHistories;

	// Make sure that the function call names match up on the second traversal.
	FNiagaraParameterMapHistoryBuilder ActiveHistoryForFunctionCalls;

	// Synced to the ParamMapHistories.
	TArray<TArray<int32> > ParamMapSetVariablesToChunks;

	// Synced to the System uniforms encountered for parameter maps thus far.
	TMap<FName, int32> ParamMapDefinedSystemVarsToUniformChunks; // Map from the defined constants to the uniform chunk expressing them (i.e. have we encountered before in this graph?)
	TMap<FName, FNiagaraVariable> ParamMapDefinedSystemToNamespaceVars; // Map from defined constants to the Namespaced variable expressing it.

	// Synced to the EmitterParameter uniforms encountered for parameter maps thus far.
	TMap<FName, int32> ParamMapDefinedEmitterParameterVarsToUniformChunks; // Map from the variable name exposed by the emitter as a parameter to the uniform chunk expressing it (i.e. have we encountered before in this graph?)
	TMap<FName, FNiagaraVariable> ParamMapDefinedEmitterParameterToNamespaceVars; // Map from defined parameter to the Namespaced variable expressing it.

    // Synced to the Attributes encountered for parameter maps thus far.
	TMap<FName, int32> ParamMapDefinedAttributesToUniformChunks; // Map from the variable name exposed as a attribute to the uniform chunk expressing it (i.e. have we encountered before in this graph?)
	TMap<FName, FNiagaraVariable> ParamMapDefinedAttributesToNamespaceVars; // Map from defined parameter to the Namespaced variable expressing it.

	// Synced to the external variables used when bulk compiling system scripts.
	TArray<FNiagaraVariable> ExternalVariablesForBulkUsage;

	// Strings to be inserted within the main function
	TArray<FString> MainPreSimulateChunks;

	// read and write data set indices
	int32 ReadIdx;
	int32 WriteIdx;

	// Data interface buffer descriptors per data interface
	TArray< TArray<DIGPUBufferParamDescriptor> > DIBufferDescriptors;

	/**
	True if we're compiling the spawn section of an interpolated spawn script.
	This will cause us to read from interpolated parameters and write to temp outputs that are fed back into the update script as inputs.
	*/
	bool bInsideInterpolatedSpawnScript;

	/** Stack of currently tracked stats scopes. */
	TArray<int32> StatScopeStack;

	FString HlslOutput;

	ENiagaraSimTarget CompilationTarget;

	// Used to keep track of which output node we are working back from. This allows us 
	// to find the right parameter map.
	TArray<int32> CurrentParamMapIndices;
};
