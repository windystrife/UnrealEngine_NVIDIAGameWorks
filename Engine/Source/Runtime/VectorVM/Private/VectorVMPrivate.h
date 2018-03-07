// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "VectorVM.h"

#define ENABLE_VM_DEBUGGING 0

struct FVectorVMContext;

namespace VectorVM
{
	/** Constants. */
	enum
	{
		InstancesPerChunk = 128,
		MaxInstanceSizeBytes = 4,
	};
}

struct FDummyHandler
{
	FORCEINLINE void Advance(){ }
	FORCEINLINE VectorRegister Get(){ return GlobalVectorConstants::FloatZero; }
	FORCEINLINE VectorRegister GetValue(){ return GlobalVectorConstants::FloatZero; }
};
extern FDummyHandler DummyHandler;

//////////////////////////////////////////////////////////////////////////
// Debugger
#if ENABLE_VM_DEBUGGING

class FVectorVMDebuggerImpl
{
	friend class VectorVM::FVectorVMDebugger;

	FVectorVMDebuggerImpl()
		: CurrOp(EVectorVMOp::done)
		, OpType(VectorVM::EVMType::Vector4)
		, CurrNumArgs(0)
		, CurrInstanceBase(0)
		, NumInstancesPerOp(0)
		, StartInstance(0)
	{

	}
	//Map of instance index to debug info gathered for that instance.
	TMap<int32, TArray<VectorVM::FOpDebugInfo>> DebugInfo;

	EVectorVMOp CurrOp;
	VectorVM::EVMType OpType;
	int32 CurrNumArgs;
	int32 CurrInstanceBase;
	int32 NumInstancesPerOp;
	int32 StartInstance;

	VectorVM::FDebugValue CachedPreOpData[NUM_VM_OP_DEBUG_VALUES];

public:

	void InitForScriptRun(int32 InStartInstance, const TArray<int32> InstancesToDebug)
	{
		StartInstance = InStartInstance;
		DebugInfo.Empty(InstancesToDebug.Num());
		for (int32 i : InstancesToDebug)
		{
			DebugInfo.Add(i);
		}
	}

	void BeginOp(FVectorVMContext& Context, VectorVM::EVMType InType, int32 InNumArgs, int32 InNumInstancesPerOp);

	template<typename DstHandler, typename Arg0Handler, typename Arg1Handler = FDummyHandler, typename Arg2Handler = FDummyHandler, typename Arg3Handler = FDummyHandler>
	void PreOp(FVectorVMContext& Context, DstHandler& Dst, Arg0Handler& Arg, Arg1Handler& Arg1 = DummyHandler, Arg2Handler& Arg2 = DummyHandler, Arg3Handler& Arg3 = DummyHandler);

	template<typename DstHandler, typename Arg0Handler, typename Arg1Handler = FDummyHandler, typename Arg2Handler = FDummyHandler, typename Arg3Handler = FDummyHandler>
	void PostOp(FVectorVMContext& Context, DstHandler& Dst, Arg0Handler& Arg, Arg1Handler& Arg1 = DummyHandler, Arg2Handler& Arg2 = DummyHandler, Arg3Handler& Arg3 = DummyHandler);
};

#endif

