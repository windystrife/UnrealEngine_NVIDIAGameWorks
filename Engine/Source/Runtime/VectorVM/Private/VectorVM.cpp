// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "VectorVM.h"
#include "Modules/ModuleManager.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "VectorVMPrivate.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, VectorVM);

DEFINE_LOG_CATEGORY_STATIC(LogVectorVM, All, All);

//#define VM_FORCEINLINE
#define VM_FORCEINLINE FORCEINLINE

#define OP_REGISTER (0)
#define OP0_CONST (1 << 0)
#define OP1_CONST (1 << 1)
#define OP2_CONST (1 << 2)

#define SRCOP_RRR (OP_REGISTER | OP_REGISTER | OP_REGISTER)
#define SRCOP_RRC (OP_REGISTER | OP_REGISTER | OP0_CONST)
#define SRCOP_RCR (OP_REGISTER | OP1_CONST | OP_REGISTER)
#define SRCOP_RCC (OP_REGISTER | OP1_CONST | OP0_CONST)
#define SRCOP_CRR (OP2_CONST | OP_REGISTER | OP_REGISTER)
#define SRCOP_CRC (OP2_CONST | OP_REGISTER | OP0_CONST)
#define SRCOP_CCR (OP2_CONST | OP1_CONST | OP_REGISTER)
#define SRCOP_CCC (OP2_CONST | OP1_CONST | OP0_CONST)

uint8 VectorVM::CreateSrcOperandMask(EVectorVMOperandLocation Type0, EVectorVMOperandLocation Type1, EVectorVMOperandLocation Type2)
{
	return	(Type0 == EVectorVMOperandLocation::Constant ? OP0_CONST : OP_REGISTER) |
		(Type1 == EVectorVMOperandLocation::Constant ? OP1_CONST : OP_REGISTER) |
		(Type2 == EVectorVMOperandLocation::Constant ? OP2_CONST : OP_REGISTER);
}

//////////////////////////////////////////////////////////////////////////
// Kernels
template<typename Kernel, typename DstHandler, typename Arg0Handler, uint32 NumInstancesPerOp>
struct TUnaryKernelHandler
{
	static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
	{
		Arg0Handler Arg0(Context);
		DstHandler Dst(Context);

		int32 LoopInstances = Align(Context.NumInstances, NumInstancesPerOp) / NumInstancesPerOp;
		for (int32 i = 0; i < LoopInstances; ++i)
		{
			Kernel::DoKernel(Dst.GetDest(), Arg0.Get());
			Dst.Advance();	Arg0.Advance();
		}
	}
};

template<typename Kernel, typename DstHandler, typename Arg0Handler, typename Arg1Handler, int32 NumInstancesPerOp>
struct TBinaryKernelHandler
{
	static void Exec(FVectorVMContext& Context)
	{
		Arg0Handler Arg0(Context); 
		Arg1Handler Arg1(Context);

		DstHandler Dst(Context);

		int32 LoopInstances = Align(Context.NumInstances, NumInstancesPerOp) / NumInstancesPerOp;
		for (int32 i = 0; i < LoopInstances; ++i)
		{
			Kernel::DoKernel(Dst.GetDest(), Arg0.Get(), Arg1.Get());
			Dst.Advance(); Arg0.Advance(); Arg1.Advance();
		}
	}
};

template<typename Kernel, typename DstHandler, typename Arg0Handler, typename Arg1Handler, typename Arg2Handler, int32 NumInstancesPerOp>
struct TTrinaryKernelHandler
{
	static void Exec(FVectorVMContext& Context)
	{
		Arg0Handler Arg0(Context);
		Arg1Handler Arg1(Context);
		Arg2Handler Arg2(Context);

		DstHandler Dst(Context);

		int32 LoopInstances = Align(Context.NumInstances, NumInstancesPerOp) / NumInstancesPerOp;
		for (int32 i = 0; i < LoopInstances; ++i)
		{
			Kernel::DoKernel(Dst.GetDest(), Arg0.Get(), Arg1.Get(), Arg2.Get());
			Dst.Advance(); Arg0.Advance(); Arg1.Advance(); Arg2.Advance();
		}
	}
};


template<typename Kernel, typename DstHandler, typename Arg0Handler, typename Arg1Handler, typename Arg2Handler, int32 NumInstancesPerOp>
struct TTrinaryOutputKernelHandler
{
	static void Exec(FVectorVMContext& Context)
	{
		Arg0Handler Arg0(Context);
		Arg1Handler Arg1(Context);
		Arg2Handler Arg2(Context);

		DstHandler Dst(Context, Arg0.Get());

		int32 LoopInstances = Align(Context.NumInstances, NumInstancesPerOp) / NumInstancesPerOp;
		for (int32 i = 0; i < LoopInstances; ++i)
		{
			Kernel::DoKernel(Dst.GetDest(), Arg0.Get(), Arg1.Get(), Arg2.Get());
			Dst.Advance(); Arg0.Advance(); Arg1.Advance(); Arg2.Advance();
		}
	}
};


/** Base class of vector kernels with a single operand. */
template <typename Kernel, typename DstHandler, typename ConstHandler, typename RegisterHandler, int32 NumInstancesPerOp>
struct TUnaryKernel
{
	static void Exec(FVectorVMContext& Context)
	{
		uint32 SrcOpTypes = DecodeSrcOperandTypes(Context);
		switch (SrcOpTypes)
		{
		case SRCOP_RRR: TUnaryKernelHandler<Kernel, DstHandler, RegisterHandler, NumInstancesPerOp>::Exec(Context); break;
		case SRCOP_RRC:	TUnaryKernelHandler<Kernel, DstHandler, ConstHandler, NumInstancesPerOp>::Exec(Context); break;
		default: check(0); break; 
		};
	}
};
template<typename Kernel>
struct TUnaryScalarKernel : public TUnaryKernel<Kernel, FRegisterHandler<float>, FConstantHandler<float>, FRegisterHandler<float>, 1> {};
template<typename Kernel>
struct TUnaryVectorKernel : public TUnaryKernel<Kernel, FRegisterDestHandler<VectorRegister>, FConstantHandler<VectorRegister>, FRegisterHandler<VectorRegister>, VECTOR_WIDTH_FLOATS> {};
template<typename Kernel>
struct TUnaryScalarIntKernel : public TUnaryKernel<Kernel, FRegisterHandler<int32>, FConstantHandler<int32>, FRegisterHandler<int32>, 1> {};
template<typename Kernel>
struct TUnaryVectorIntKernel : public TUnaryKernel<Kernel, FRegisterDestHandler<VectorRegisterInt>, FConstantHandler<VectorRegisterInt>, FRegisterHandler<VectorRegisterInt>, VECTOR_WIDTH_FLOATS> {};

/** Base class of Vector kernels with 2 operands. */
template <typename Kernel, typename DstHandler, typename ConstHandler, typename RegisterHandler, uint32 NumInstancesPerOp>
struct TBinaryKernel
{
	static void Exec(FVectorVMContext& Context)
	{
		uint32 SrcOpTypes = DecodeSrcOperandTypes(Context);
		switch (SrcOpTypes)
		{
		case SRCOP_RRR: TBinaryKernelHandler<Kernel, DstHandler, RegisterHandler, RegisterHandler, NumInstancesPerOp>::Exec(Context); break;
		case SRCOP_RRC:	TBinaryKernelHandler<Kernel, DstHandler, ConstHandler, RegisterHandler, NumInstancesPerOp>::Exec(Context); break;
		case SRCOP_RCR: TBinaryKernelHandler<Kernel, DstHandler, RegisterHandler, ConstHandler, NumInstancesPerOp>::Exec(Context); break;
		case SRCOP_RCC:	TBinaryKernelHandler<Kernel, DstHandler, ConstHandler, ConstHandler, NumInstancesPerOp>::Exec(Context); break;
		default: check(0); break;
		};
	}
};
template<typename Kernel>
struct TBinaryScalarKernel : public TBinaryKernel<Kernel, FRegisterHandler<float>, FConstantHandler<float>, FRegisterHandler<float>, 1> {};
template<typename Kernel>
struct TBinaryVectorKernel : public TBinaryKernel<Kernel, FRegisterDestHandler<VectorRegister>, FConstantHandler<VectorRegister>, FRegisterHandler<VectorRegister>, VECTOR_WIDTH_FLOATS> {};
template<typename Kernel>
struct TBinaryVectorIntKernel : public TBinaryKernel<Kernel, FRegisterDestHandler<VectorRegisterInt>, FConstantHandler<VectorRegisterInt>, FRegisterHandler<VectorRegisterInt>, VECTOR_WIDTH_FLOATS> {};

/** Base class of Vector kernels with 3 operands. */
template <typename Kernel, typename DstHandler, typename ConstHandler, typename RegisterHandler, uint32 NumInstancesPerOp>
struct TTrinaryKernel
{
	static void Exec(FVectorVMContext& Context)
	{
		uint32 SrcOpTypes = DecodeSrcOperandTypes(Context);
		switch (SrcOpTypes)
		{
		case SRCOP_RRR: TTrinaryKernelHandler<Kernel, DstHandler, RegisterHandler, RegisterHandler, RegisterHandler, NumInstancesPerOp>::Exec(Context); break;
		case SRCOP_RRC:	TTrinaryKernelHandler<Kernel, DstHandler, ConstHandler, RegisterHandler, RegisterHandler, NumInstancesPerOp>::Exec(Context); break;
		case SRCOP_RCR: TTrinaryKernelHandler<Kernel, DstHandler, RegisterHandler, ConstHandler, RegisterHandler, NumInstancesPerOp>::Exec(Context); break;
		case SRCOP_RCC:	TTrinaryKernelHandler<Kernel, DstHandler, ConstHandler, ConstHandler, RegisterHandler, NumInstancesPerOp>::Exec(Context); break;
		case SRCOP_CRR: TTrinaryKernelHandler<Kernel, DstHandler, RegisterHandler, RegisterHandler, ConstHandler, NumInstancesPerOp>::Exec(Context); break;
		case SRCOP_CRC:	TTrinaryKernelHandler<Kernel, DstHandler, ConstHandler, RegisterHandler, ConstHandler, NumInstancesPerOp>::Exec(Context); break;
		case SRCOP_CCR: TTrinaryKernelHandler<Kernel, DstHandler, RegisterHandler, ConstHandler, ConstHandler, NumInstancesPerOp>::Exec(Context); break;
		case SRCOP_CCC:	TTrinaryKernelHandler<Kernel, DstHandler, ConstHandler, ConstHandler, ConstHandler, NumInstancesPerOp>::Exec(Context); break;
		default: check(0); break;
		};
	}
};

template<typename Kernel>
struct TTrinaryScalarKernel : public TTrinaryKernel<Kernel, FRegisterHandler<float>, FConstantHandler<float>, FRegisterHandler<float>, 1> {};
template<typename Kernel>
struct TTrinaryVectorKernel : public TTrinaryKernel<Kernel, FRegisterDestHandler<VectorRegister>, FConstantHandler<VectorRegister>, FRegisterHandler<VectorRegister>, VECTOR_WIDTH_FLOATS> {};
template<typename Kernel>
struct TTrinaryVectorIntKernel : public TTrinaryKernel<Kernel, FRegisterDestHandler<VectorRegisterInt>, FConstantHandler<VectorRegisterInt>, FRegisterHandler<VectorRegisterInt>, VECTOR_WIDTH_FLOATS> {};


/*------------------------------------------------------------------------------
	Implementation of all kernel operations.
------------------------------------------------------------------------------*/

struct FVectorKernelAdd : public TBinaryVectorKernel<FVectorKernelAdd>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst,VectorRegister Src0,VectorRegister Src1)
	{
		*Dst = VectorAdd(Src0, Src1);
	}
};

struct FVectorKernelSub : public TBinaryVectorKernel<FVectorKernelSub>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst,VectorRegister Src0,VectorRegister Src1)
	{
		*Dst = VectorSubtract(Src0, Src1);
	}
};

struct FVectorKernelMul : public TBinaryVectorKernel<FVectorKernelMul>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst,VectorRegister Src0,VectorRegister Src1)
	{
		*Dst = VectorMultiply(Src0, Src1);
	}
};

struct FVectorKernelDiv : public TBinaryVectorKernel<FVectorKernelDiv>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0, VectorRegister Src1)
	{
		*Dst = VectorDivide(Src0, Src1);
	}
};


struct FVectorKernelMad : public TTrinaryVectorKernel<FVectorKernelMad>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst,VectorRegister Src0,VectorRegister Src1,VectorRegister Src2)
	{
		*Dst = VectorMultiplyAdd(Src0, Src1, Src2);
	}
};

struct FVectorKernelLerp : public TTrinaryVectorKernel<FVectorKernelLerp>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst,VectorRegister Src0,VectorRegister Src1,VectorRegister Src2)
	{
		const VectorRegister OneMinusAlpha = VectorSubtract(GlobalVectorConstants::FloatOne, Src2);
		const VectorRegister Tmp = VectorMultiply(Src0, OneMinusAlpha);
		*Dst = VectorMultiplyAdd(Src1, Src2, Tmp);
	}
};

struct FVectorKernelRcp : public TUnaryVectorKernel<FVectorKernelRcp>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst,VectorRegister Src0)
	{
		*Dst = VectorReciprocal(Src0);
	}
};

struct FVectorKernelRsq : public TUnaryVectorKernel<FVectorKernelRsq>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst,VectorRegister Src0)
	{
		*Dst = VectorReciprocalSqrt(Src0);
	}
};

struct FVectorKernelSqrt : public TUnaryVectorKernel<FVectorKernelSqrt>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst,VectorRegister Src0)
	{
		// TODO: Need a SIMD sqrt!
		*Dst = VectorReciprocal(VectorReciprocalSqrt(Src0));
	}
};

struct FVectorKernelNeg : public TUnaryVectorKernel<FVectorKernelNeg>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst,VectorRegister Src0)
	{
		*Dst = VectorNegate(Src0);
	}
};

struct FVectorKernelAbs : public TUnaryVectorKernel<FVectorKernelAbs>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst,VectorRegister Src0)
	{
		*Dst = VectorAbs(Src0);
	}
};

struct FVectorKernelExp : public TUnaryVectorKernel<FVectorKernelExp>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorExp(Src0);
	}
}; 

struct FVectorKernelExp2 : public TUnaryVectorKernel<FVectorKernelExp2>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorExp2(Src0);
	}
};

struct FVectorKernelLog : public TUnaryVectorKernel<FVectorKernelLog>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorLog(Src0);
	}
};

struct FVectorKernelLog2 : public TUnaryVectorKernel<FVectorKernelLog2>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorLog2(Src0);
	}
};

struct FVectorKernelClamp : public TTrinaryVectorKernel<FVectorKernelClamp>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst,VectorRegister Src0,VectorRegister Src1,VectorRegister Src2)
	{
		const VectorRegister Tmp = VectorMax(Src0, Src1);
		*Dst = VectorMin(Tmp, Src2);
	}
};

struct FVectorKernelSin : public TUnaryVectorKernel<FVectorKernelSin>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorSin(VectorMultiply(Src0, GlobalVectorConstants::TwoPi));
	}
};

struct FVectorKernelCos : public TUnaryVectorKernel<FVectorKernelCos>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
 	{
		*Dst = VectorCos(VectorMultiply(Src0, GlobalVectorConstants::TwoPi));
	}
};

struct FVectorKernelTan : public TUnaryVectorKernel<FVectorKernelTan>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorTan(VectorMultiply(Src0, GlobalVectorConstants::TwoPi));
	}
};

struct FVectorKernelASin : public TUnaryVectorKernel<FVectorKernelASin>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorMultiply(VectorASin(Src0), GlobalVectorConstants::OneOverTwoPi);
	}
};

struct FVectorKernelACos : public TUnaryVectorKernel<FVectorKernelACos>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorMultiply(VectorACos(Src0), GlobalVectorConstants::OneOverTwoPi);
	}
};

struct FVectorKernelATan : public TUnaryVectorKernel<FVectorKernelATan>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorMultiply(VectorATan(Src0), GlobalVectorConstants::OneOverTwoPi);
	}
};

struct FVectorKernelATan2 : public TBinaryVectorKernel<FVectorKernelATan2>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0, VectorRegister Src1)
	{
		*Dst = VectorMultiply(VectorATan2(Src0, Src1), GlobalVectorConstants::OneOverTwoPi);
	}
};

struct FVectorKernelCeil : public TUnaryVectorKernel<FVectorKernelCeil>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorCeil(Src0);
	}
};

struct FVectorKernelFloor : public TUnaryVectorKernel<FVectorKernelFloor>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorFloor(Src0);
	}
};

struct FVectorKernelRound : public TUnaryVectorKernel<FVectorKernelRound>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		//TODO: >SSE4 has direct ops for this.		
		VectorRegister Trunc = VectorTruncate(Src0);
		*Dst = VectorAdd(Trunc, VectorTruncate(VectorMultiply(VectorSubtract(Src0, Trunc), GlobalVectorConstants::FloatAlmostTwo)));
	}
};

struct FVectorKernelMod : public TBinaryVectorKernel<FVectorKernelMod>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0, VectorRegister Src1)
	{
		*Dst = VectorMod(Src0, Src1);
	}
};

struct FVectorKernelFrac : public TUnaryVectorKernel<FVectorKernelFrac>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorFractional(Src0);
	}
};

struct FVectorKernelTrunc : public TUnaryVectorKernel<FVectorKernelTrunc>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorTruncate(Src0);
	}
};

struct FVectorKernelCompareLT : public TBinaryVectorKernel<FVectorKernelCompareLT>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0, VectorRegister Src1)
	{
		*Dst = VectorCompareLT(Src0, Src1);
	}
};

struct FVectorKernelCompareLE : public TBinaryVectorKernel<FVectorKernelCompareLE>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0, VectorRegister Src1)
	{
		*Dst = VectorCompareLE(Src0, Src1);
	}
};

struct FVectorKernelCompareGT : public TBinaryVectorKernel<FVectorKernelCompareGT>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0, VectorRegister Src1)
	{
		*Dst = VectorCompareGT(Src0, Src1);
	}
};

struct FVectorKernelCompareGE : public TBinaryVectorKernel<FVectorKernelCompareGE>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0, VectorRegister Src1)
	{
		*Dst = VectorCompareGE(Src0, Src1);
	}
};

struct FVectorKernelCompareEQ : public TBinaryVectorKernel<FVectorKernelCompareEQ>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0, VectorRegister Src1)
	{
		*Dst = VectorCompareEQ(Src0, Src1);
	}
};

struct FVectorKernelCompareNEQ : public TBinaryVectorKernel<FVectorKernelCompareNEQ>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0, VectorRegister Src1)
	{		
		*Dst = VectorCompareNE(Src0, Src1);
	}
};

struct FVectorKernelSelect : public TTrinaryVectorKernel<FVectorKernelSelect>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Mask, VectorRegister A, VectorRegister B)
	{
		*Dst = VectorSelect(Mask, A, B);
	}
};

struct FVectorKernelExecutionIndex
{
	static void VM_FORCEINLINE Exec(FVectorVMContext& Context)
	{
		static_assert(VECTOR_WIDTH_FLOATS == 4, "Need to update this when upgrading the VM to support >SSE2");
		VectorRegisterInt VectorStride = MakeVectorRegisterInt(VECTOR_WIDTH_FLOATS, VECTOR_WIDTH_FLOATS, VECTOR_WIDTH_FLOATS, VECTOR_WIDTH_FLOATS);
		VectorRegisterInt Index = MakeVectorRegisterInt(Context.StartInstance, Context.StartInstance + 1, Context.StartInstance + 2, Context.StartInstance + 3);
		
		FRegisterDestHandler<VectorRegisterInt> Dest(Context);
		int32 Loops = Align(Context.NumInstances, VECTOR_WIDTH_FLOATS) / VECTOR_WIDTH_FLOATS;
		for (int32 i = 0; i < Loops; ++i)
		{
			*Dest.GetDest() = Index;
			Dest.Advance();
			Index = VectorIntAdd(Index, VectorStride);
		}
	}
};

struct FVectorKernelEnterStatScope
{
	static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
	{
#if STATS
		FConstantHandler<int32> ScopeIdx(Context);		
		int32 CounterIdx = Context.StatCounterStack.AddDefaulted(1);
		Context.StatCounterStack[CounterIdx].Start(Context.StatScopes[ScopeIdx.Get()]);
#endif
	}
};

struct FVectorKernelExitStatScope
{
	static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
	{
#if STATS
		Context.StatCounterStack.Last().Stop();
		Context.StatCounterStack.Pop();
#endif
	}
};

struct FVectorKernelRandom : public TUnaryVectorKernel<FVectorKernelRandom>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		const float rm = RAND_MAX;
		//EEK!. Improve this. Implement GPU style seeded rand instead of this.
		VectorRegister Result = MakeVectorRegister(static_cast<float>(FMath::Rand()) / rm,
			static_cast<float>(FMath::Rand()) / rm,
			static_cast<float>(FMath::Rand()) / rm,
			static_cast<float>(FMath::Rand()) / rm);
		*Dst = VectorMultiply(Result, Src0);
	}
};

/* gaussian distribution random number (not working yet) */
struct FVectorKernelRandomGauss : public TBinaryVectorKernel<FVectorKernelRandomGauss>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0, VectorRegister Src1)
	{
		const float rm = RAND_MAX;
		VectorRegister Result = MakeVectorRegister(static_cast<float>(FMath::Rand()) / rm,
			static_cast<float>(FMath::Rand()) / rm,
			static_cast<float>(FMath::Rand()) / rm,
			static_cast<float>(FMath::Rand()) / rm);

		Result = VectorSubtract(Result, MakeVectorRegister(0.5f, 0.5f, 0.5f, 0.5f));
		Result = VectorMultiply(MakeVectorRegister(3.0f, 3.0f, 3.0f, 3.0f), Result);

		// taylor series gaussian approximation
		const VectorRegister SPi2 = VectorReciprocal(VectorReciprocalSqrt(MakeVectorRegister(2 * PI, 2 * PI, 2 * PI, 2 * PI)));
		VectorRegister Gauss = VectorReciprocal(SPi2);
		VectorRegister Div = VectorMultiply(MakeVectorRegister(2.0f, 2.0f, 2.0f, 2.0f), SPi2);
		Gauss = VectorSubtract(Gauss, VectorDivide(VectorMultiply(Result, Result), Div));
		Div = VectorMultiply(MakeVectorRegister(8.0f, 8.0f, 8.0f, 8.0f), SPi2);
		Gauss = VectorAdd(Gauss, VectorDivide(VectorPow(MakeVectorRegister(4.0f, 4.0f, 4.0f, 4.0f), Result), Div));
		Div = VectorMultiply(MakeVectorRegister(48.0f, 48.0f, 48.0f, 48.0f), SPi2);
		Gauss = VectorSubtract(Gauss, VectorDivide(VectorPow(MakeVectorRegister(6.0f, 6.0f, 6.0f, 6.0f), Result), Div));

		Gauss = VectorDivide(Gauss, MakeVectorRegister(0.4f, 0.4f, 0.4f, 0.4f));
		Gauss = VectorMultiply(Gauss, Src0);
		*Dst = Gauss;
	}
};

struct FVectorKernelMin : public TBinaryVectorKernel<FVectorKernelMin>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst,VectorRegister Src0,VectorRegister Src1)
	{
		*Dst = VectorMin(Src0, Src1);
	}
};

struct FVectorKernelMax : public TBinaryVectorKernel<FVectorKernelMax>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst,VectorRegister Src0,VectorRegister Src1)
	{
		*Dst = VectorMax(Src0, Src1);
	}
};

struct FVectorKernelPow : public TBinaryVectorKernel<FVectorKernelPow>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst,VectorRegister Src0,VectorRegister Src1)
	{
		*Dst = VectorPow(Src0, Src1);
	}
};

struct FVectorKernelSign : public TUnaryVectorKernel<FVectorKernelSign>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorSign(Src0);
	}
};

struct FVectorKernelStep : public TUnaryVectorKernel<FVectorKernelStep>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorStep(Src0);
	}
};

namespace VectorVMNoise
{
	int32 P[512] =
	{
		151,160,137,91,90,15,
		131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
		190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
		88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
		77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
		102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
		135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
		5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
		223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
		129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
		251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
		49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
		138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180,
		151,160,137,91,90,15,
		131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
		190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
		88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
		77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
		102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
		135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
		5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
		223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
		129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
		251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
		49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
		138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
	};

	static FORCEINLINE float Lerp(float X, float A, float B)
	{
		return A + X * (B - A);
	}

	static FORCEINLINE float Fade(float X)
	{
		return X * X * X * (X * (X * 6 - 15) + 10);
	}
	
	static FORCEINLINE float Grad(int32 hash, float x, float y, float z)
	{
		 float u = (hash < 8) ? x : y;
		 float v = (hash < 4) ? y : ((hash == 12 || hash == 14) ? x : z);
		 return ((hash & 1) == 0 ? u : -u) + ((hash & 2) == 0 ? v : -v);
	}

	struct FScalarKernelNoise3D_iNoise : TTrinaryScalarKernel<FScalarKernelNoise3D_iNoise>
	{
		static void VM_FORCEINLINE DoKernel(float* RESTRICT Dst, float X, float Y, float Z)
		{
			float Xfl = FMath::FloorToFloat(X);
			float Yfl = FMath::FloorToFloat(Y);
			float Zfl = FMath::FloorToFloat(Z);
			int32 Xi = (int32)(Xfl) & 255;
			int32 Yi = (int32)(Yfl) & 255;
			int32 Zi = (int32)(Zfl) & 255;
			X -= Xfl;
			Y -= Yfl;
			Z -= Zfl;
			float Xm1 = X - 1.0f;
			float Ym1 = Y - 1.0f;
			float Zm1 = Z - 1.0f;

			int32 A = P[Xi] + Yi;
			int32 AA = P[A] + Zi;	int32 AB = P[A + 1] + Zi;

			int32 B = P[Xi + 1] + Yi;
			int32 BA = P[B] + Zi;	int32 BB = P[B + 1] + Zi;

			float U = Fade(X);
			float V = Fade(Y);
			float W = Fade(Z);

			*Dst =
				Lerp(W,
					Lerp(V,
						Lerp(U,
							Grad(P[AA], X, Y, Z),
							Grad(P[BA], Xm1, Y, Z)),
						Lerp(U,
							Grad(P[AB], X, Ym1, Z),
							Grad(P[BB], Xm1, Ym1, Z))),
					Lerp(V,
						Lerp(U,
							Grad(P[AA + 1], X, Y, Zm1),
							Grad(P[BA + 1], Xm1, Y, Zm1)),
						Lerp(U,
							Grad(P[AB + 1], X, Ym1, Zm1),
							Grad(P[BB + 1], Xm1, Ym1, Zm1))));
		}
	};

	struct FScalarKernelNoise2D_iNoise : TBinaryScalarKernel<FScalarKernelNoise2D_iNoise>
	{
		static void VM_FORCEINLINE DoKernel(float* RESTRICT Dst, float X, float Y)
		{
			*Dst = 0.0f;//TODO
		}
	};

	struct FScalarKernelNoise1D_iNoise : TUnaryScalarKernel<FScalarKernelNoise1D_iNoise>
	{
		static void VM_FORCEINLINE DoKernel(float* RESTRICT Dst, float X)
		{
			*Dst = 0.0f;//TODO;
		}
	};

	static void Noise1D(FVectorVMContext& Context) { FScalarKernelNoise1D_iNoise::Exec(Context); }
	static void Noise2D(FVectorVMContext& Context) { FScalarKernelNoise2D_iNoise::Exec(Context); }
	static void Noise3D(FVectorVMContext& Context)
	{
		//Basic scalar implementation of perlin's improved noise until I can spend some quality time exploring vectorized implementations of Marc O's noise from Random.usf.
		//http://mrl.nyu.edu/~perlin/noise/
		FScalarKernelNoise3D_iNoise::Exec(Context);
	}
};

//Olaf's orginal curl noise. Needs updating for the new scalar VM and possibly calling Curl Noise to avoid confusion with regular noise?
//Possibly needs to be a data interface as the VM can't output Vectors?
struct FVectorKernelNoise : public TUnaryVectorKernel<FVectorKernelNoise>
{
	static VectorRegister RandomTable[17][17][17];

	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		const VectorRegister One = MakeVectorRegister(1.0f, 1.0f, 1.0f, 1.0f);
		const VectorRegister VecSize = MakeVectorRegister(16.0f, 16.0f, 16.0f, 16.0f);

		*Dst = MakeVectorRegister(0.0f, 0.0f, 0.0f, 0.0f);
		
		for (uint32 i = 1; i < 2; i++)
		{
			float Di = 0.2f * (1.0f/(1<<i));
			VectorRegister Div = MakeVectorRegister(Di, Di, Di, Di);
			VectorRegister Coords = VectorMod( VectorAbs( VectorMultiply(Src0, Div) ), VecSize );
			const float *CoordPtr = reinterpret_cast<float const*>(&Coords);
			const int32 Cx = CoordPtr[0];
			const int32 Cy = CoordPtr[1];
			const int32 Cz = CoordPtr[2];

			VectorRegister Frac = VectorFractional(Coords);
			VectorRegister Alpha = VectorReplicate(Frac, 0);
			VectorRegister OneMinusAlpha = VectorSubtract(One, Alpha);
			
			VectorRegister XV1 = VectorMultiplyAdd(RandomTable[Cx][Cy][Cz], Alpha, VectorMultiply(RandomTable[Cx+1][Cy][Cz], OneMinusAlpha));
			VectorRegister XV2 = VectorMultiplyAdd(RandomTable[Cx][Cy+1][Cz], Alpha, VectorMultiply(RandomTable[Cx+1][Cy+1][Cz], OneMinusAlpha));
			VectorRegister XV3 = VectorMultiplyAdd(RandomTable[Cx][Cy][Cz+1], Alpha, VectorMultiply(RandomTable[Cx+1][Cy][Cz+1], OneMinusAlpha));
			VectorRegister XV4 = VectorMultiplyAdd(RandomTable[Cx][Cy+1][Cz+1], Alpha, VectorMultiply(RandomTable[Cx+1][Cy+1][Cz+1], OneMinusAlpha));

			Alpha = VectorReplicate(Frac, 1);
			OneMinusAlpha = VectorSubtract(One, Alpha);
			VectorRegister YV1 = VectorMultiplyAdd(XV1, Alpha, VectorMultiply(XV2, OneMinusAlpha));
			VectorRegister YV2 = VectorMultiplyAdd(XV3, Alpha, VectorMultiply(XV4, OneMinusAlpha));

			Alpha = VectorReplicate(Frac, 2);
			OneMinusAlpha = VectorSubtract(One, Alpha);
			VectorRegister ZV = VectorMultiplyAdd(YV1, Alpha, VectorMultiply(YV2, OneMinusAlpha));

			*Dst = VectorAdd(*Dst, ZV);
		}
	}
};

VectorRegister FVectorKernelNoise::RandomTable[17][17][17];

//////////////////////////////////////////////////////////////////////////
//Special Kernels.

/** Special kernel for reading from the main input dataset. */
template<typename T>
struct FVectorKernelReadInput
{
	static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
	{
		static const int32 InstancesPerVector = sizeof(VectorRegister) / sizeof(T);

		int32 DataSetIndex = DecodeU16(Context);
		int32 InputRegisterIdx = DecodeU16(Context);
		int32 DestRegisterIdx = DecodeU16(Context);
		int32 Loops = Align(Context.NumInstances, InstancesPerVector) / InstancesPerVector;

		VectorRegister* DestReg = (VectorRegister*)(Context.RegisterTable[DestRegisterIdx]);
		int32 DataSetOffset = Context.DataSetOffsetTable[DataSetIndex];  //TODO: we'll need a different way of doing this for a proper kernel; might need a specific offset handler
		VectorRegister* InputReg = (VectorRegister*)((T*)(Context.RegisterTable[InputRegisterIdx + DataSetOffset]) + Context.StartInstance);

		//TODO: We can actually do some scalar loads into the first and final vectors to get around alignment issues and then use the aligned load for all others.
		for (int32 i = 0; i < Loops; ++i)
		{
			*DestReg = VectorLoad(InputReg);
			++DestReg;
			++InputReg;
		}
	}
};



/** Special kernel for reading from an input dataset; non-advancing (reads same instance everytime). 
 *  this kernel splats the X component of the source register to all 4 dest components; it's meant to
 *	use scalar data sets as the source (e.g. events)
 */
template<typename T>
struct FVectorKernelReadInputNoAdvance
{
	static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
	{
		static const int32 InstancesPerVector = sizeof(VectorRegister) / sizeof(T);

		int32 DataSetIndex = DecodeU16(Context);
		int32 InputRegisterIdx = DecodeU16(Context);
		int32 DestRegisterIdx = DecodeU16(Context);
		int32 Loops = Align(Context.NumInstances, InstancesPerVector) / InstancesPerVector;

		VectorRegister* DestReg = (VectorRegister*)(Context.RegisterTable[DestRegisterIdx]);
		int32 DataSetOffset = Context.DataSetOffsetTable[DataSetIndex];  //TODO: we'll need a different way of doing this for a proper kernel; might need a specific offset handler
		VectorRegister* InputReg = (VectorRegister*)((T*)(Context.RegisterTable[InputRegisterIdx + DataSetOffset]) );

		//TODO: We can actually do some scalar loads into the first and final vectors to get around alignment issues and then use the aligned load for all others.
		for (int32 i = 0; i < Loops; ++i)
		{
			*DestReg = VectorSwizzle(VectorLoad(InputReg), 0,0,0,0);
			++DestReg;
		}
	}
};





//TODO - Should be straight forwards to follow the input with a mix of the outputs direct indexing
/** Special kernel for reading an specific location in an input register. */
// template<typename T>
// struct FScalarKernelReadInputIndexed
// {
// 	static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
// 	{
// 		int32* IndexReg = (int32*)(Context.RegisterTable[DecodeU16(Context)]);
// 		T* InputReg = (T*)(Context.RegisterTable[DecodeU16(Context)]);
// 		T* DestReg = (T*)(Context.RegisterTable[DecodeU16(Context)]);
// 
// 		//Has to be scalar as each instance can read from a different location in the input buffer.
// 		for (int32 i = 0; i < Context.NumInstances; ++i)
// 		{
// 			T* ReadPtr = (*InputReg) + (*IndexReg);
// 			*DestReg = (*ReadPtr);
// 			++IndexReg;
// 			++DestReg;
// 		}
// 	}
// };

//Needs it's own handler as the output registers are indexed absolutely rather than incrementing in advance().
template<typename T>
struct FOutputRegisterHandler : public FRegisterHandlerBase
{
	T* Register;
	FOutputRegisterHandler(FVectorVMContext& Context, uint32 DataSetOffset)
		: FRegisterHandlerBase(Context)
		, Register((T*)Context.RegisterTable[RegisterIndex+DataSetOffset])
	{}

	VM_FORCEINLINE void Advance() { }
	VM_FORCEINLINE T Get() { return *Register; }
	VM_FORCEINLINE T* GetDest() { return Register; }
};

/** Special kernel for writing to a specific output register. */
template<typename T>
struct FScalarKernelWriteOutputIndexed
{
	static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
	{
		uint32 SrcOpTypes = DecodeSrcOperandTypes(Context);		
		switch (SrcOpTypes)
		{
		case SRCOP_RRR: TTrinaryOutputKernelHandler<FScalarKernelWriteOutputIndexed, FOutputRegisterHandler<T>, FDataSetOffsetHandler, FRegisterHandler<int32>, FRegisterHandler<T>, 1>::Exec(Context); break;
		case SRCOP_RRC:	TTrinaryOutputKernelHandler<FScalarKernelWriteOutputIndexed, FOutputRegisterHandler<T>, FDataSetOffsetHandler, FRegisterHandler<int32>, FConstantHandler<T>, 1>::Exec(Context); break;
		default: check(0); break;
		};
	}

	static VM_FORCEINLINE void DoKernel(T* RESTRICT Dst, int32 SetOffset, int32 Index, T Data)
	{
		if (Index != INDEX_NONE)
		{
			Dst[Index] = Data;//TODO: On sse4 we can use _mm_stream_ss here.
		}
	}
};

struct FDataSetCounterHandler
{
	int32* Counter;
	FDataSetCounterHandler(FVectorVMContext& Context)
		: Counter(Context.DataSetIndexTable + DecodeU16(Context))
	{}

	VM_FORCEINLINE void Advance() { }
	VM_FORCEINLINE int32* Get() { return Counter; }
	//VM_FORCEINLINE const int32* GetDest() { return Counter; }Should never use as a dest. All kernels with read and write to this.
};

struct FScalarKernelAcquireCounterIndex
{
	static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
	{
		uint32 SrcOpTypes = DecodeSrcOperandTypes(Context);
		switch (SrcOpTypes)
		{
		case SRCOP_RRR: TBinaryKernelHandler<FScalarKernelAcquireCounterIndex, FRegisterDestHandler<int32>, FDataSetCounterHandler, FRegisterHandler<int32>, 1>::Exec(Context); break;
		case SRCOP_RRC:	TBinaryKernelHandler<FScalarKernelAcquireCounterIndex, FRegisterDestHandler<int32>, FDataSetCounterHandler, FConstantHandler<int32>, 1>::Exec(Context); break;
		default: check(0); break;
		};
	}

	static VM_FORCEINLINE void DoKernel(int32* RESTRICT Dst, int32* Index, int32 Valid)
	{
		if (*Index != INDEX_NONE && Valid != 0)
		{
			*Dst = (*Index)++;
		}
		else
		{	
			*Dst = INDEX_NONE;	// Subsequent DoKernal calls above will skip over INDEX_NONE register entries...
		}
	}
};

//TODO: REWORK TO FUNCITON LIKE THE ABOVE.
// /** Special kernel for decrementing a dataset counter. */
// struct FScalarKernelReleaseCounterIndex
// {
// 	static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
// 	{
// 		int32* CounterPtr = (int32*)(Context.ConstantTable[DecodeU16(Context)]);
// 		int32* DestReg = (int32*)(Context.RegisterTable[DecodeU16(Context)]);
// 
// 		for (int32 i = 0; i < Context.NumInstances; ++i)
// 		{
// 			int32 Counter = (*CounterPtr--);
// 			*DestReg = Counter >= 0 ? Counter : INDEX_NONE;
// 
// 			++DestReg;
// 		}
// 	}
// };

//////////////////////////////////////////////////////////////////////////
//external_func_call

struct FKernelExternalFunctionCall
{
	static void Exec(FVectorVMContext& Context)
	{
		uint32 ExternalFuncIdx = DecodeU8(Context);
		Context.ExternalFunctionTable[ExternalFuncIdx].Execute(Context);
	}
};

//////////////////////////////////////////////////////////////////////////
//Integer operations

//addi,
struct FVectorIntKernelAdd : TBinaryVectorIntKernel<FVectorIntKernelAdd>
{
	static void VM_FORCEINLINE DoKernel(VectorRegisterInt* Dst, VectorRegisterInt Src0, VectorRegisterInt Src1)
	{
		*Dst = VectorIntAdd(Src0, Src1);
	}
};

//subi,
struct FVectorIntKernelSubtract : TBinaryVectorIntKernel<FVectorIntKernelSubtract>
{
	static void VM_FORCEINLINE DoKernel(VectorRegisterInt* Dst, VectorRegisterInt Src0, VectorRegisterInt Src1)
	{
		*Dst = VectorIntSubtract(Src0, Src1);
	}
};

//muli,
struct FVectorIntKernelMultiply : TBinaryVectorIntKernel<FVectorIntKernelMultiply>
{
	static void VM_FORCEINLINE DoKernel(VectorRegisterInt* Dst, VectorRegisterInt Src0, VectorRegisterInt Src1)
	{
		*Dst = VectorIntMultiply(Src0, Src1);
	}
};

//clampi,
struct FVectorIntKernelClamp : TTrinaryVectorIntKernel<FVectorIntKernelClamp>
{
	static void VM_FORCEINLINE DoKernel(VectorRegisterInt* Dst, VectorRegisterInt Src0, VectorRegisterInt Src1, VectorRegisterInt Src2)
	{
		*Dst = VectorIntMin(VectorIntMax(Src0, Src1), Src2);
	}
};

//mini,
struct FVectorIntKernelMin : TBinaryVectorIntKernel<FVectorIntKernelMin>
{
	static void VM_FORCEINLINE DoKernel(VectorRegisterInt* Dst, VectorRegisterInt Src0, VectorRegisterInt Src1)
	{
		*Dst = VectorIntMin(Src0, Src1);
	}
};

//maxi,
struct FVectorIntKernelMax : TBinaryVectorIntKernel<FVectorIntKernelMax>
{
	static void VM_FORCEINLINE DoKernel(VectorRegisterInt* Dst, VectorRegisterInt Src0, VectorRegisterInt Src1)
	{
		*Dst = VectorIntMax(Src0, Src1);
	}
};

//absi,
struct FVectorIntKernelAbs : TUnaryVectorIntKernel<FVectorIntKernelAbs>
{
	static void VM_FORCEINLINE DoKernel(VectorRegisterInt* Dst, VectorRegisterInt Src0)
	{
		*Dst = VectorIntAbs(Src0);
	}
};

//negi,
struct FVectorIntKernelNegate : TUnaryVectorIntKernel<FVectorIntKernelNegate>
{
	static void VM_FORCEINLINE DoKernel(VectorRegisterInt* Dst, VectorRegisterInt Src0)
	{
		*Dst = VectorIntNegate(Src0);
	}
};

//signi,
struct FVectorIntKernelSign : TUnaryVectorIntKernel<FVectorIntKernelSign>
{
	static void VM_FORCEINLINE DoKernel(VectorRegisterInt* Dst, VectorRegisterInt Src0)
	{
		*Dst = VectorIntSign(Src0);
	}
};

//randomi,
//No good way to do this with SSE atm so just do it scalar.
struct FScalarIntKernelRandom : public TUnaryScalarIntKernel<FScalarIntKernelRandom>
{
	static void VM_FORCEINLINE DoKernel(int32* RESTRICT Dst, int32 Src0)
	{
		const float rm = RAND_MAX;
		//EEK!. Improve this. Implement GPU style seeded rand instead of this.
		*Dst = FMath::Rand() % (Src0 + 1);
	}
};

//cmplti,
struct FVectorIntKernelCompareLT : TBinaryVectorIntKernel<FVectorIntKernelCompareLT>
{
	static void VM_FORCEINLINE DoKernel(VectorRegisterInt* Dst, VectorRegisterInt Src0, VectorRegisterInt Src1)
	{
		*Dst = VectorIntCompareLT(Src0, Src1);
	}
};

//cmplei,
struct FVectorIntKernelCompareLE : TBinaryVectorIntKernel<FVectorIntKernelCompareLE>
{
	static void VM_FORCEINLINE DoKernel(VectorRegisterInt* Dst, VectorRegisterInt Src0, VectorRegisterInt Src1)
	{
		*Dst = VectorIntCompareLE(Src0, Src1);
	}
};

//cmpgti,
struct FVectorIntKernelCompareGT : TBinaryVectorIntKernel<FVectorIntKernelCompareGT>
{
	static void VM_FORCEINLINE DoKernel(VectorRegisterInt* Dst, VectorRegisterInt Src0, VectorRegisterInt Src1)
	{
		*Dst = VectorIntCompareGT(Src0, Src1);
	}
};

//cmpgei,
struct FVectorIntKernelCompareGE : TBinaryVectorIntKernel<FVectorIntKernelCompareGE>
{
	static void VM_FORCEINLINE DoKernel(VectorRegisterInt* Dst, VectorRegisterInt Src0, VectorRegisterInt Src1)
	{
		*Dst = VectorIntCompareGE(Src0, Src1);
	}
};

//cmpeqi,
struct FVectorIntKernelCompareEQ : TBinaryVectorIntKernel<FVectorIntKernelCompareEQ>
{
	static void VM_FORCEINLINE DoKernel(VectorRegisterInt* Dst, VectorRegisterInt Src0, VectorRegisterInt Src1)
	{
		*Dst = VectorIntCompareEQ(Src0, Src1);
	}
};

//cmpneqi,
struct FVectorIntKernelCompareNEQ : TBinaryVectorIntKernel<FVectorIntKernelCompareNEQ>
{
	static void VM_FORCEINLINE DoKernel(VectorRegisterInt* Dst, VectorRegisterInt Src0, VectorRegisterInt Src1)
	{
		*Dst = VectorIntCompareNEQ(Src0, Src1);
	}
};

//bit_and,
struct FVectorIntKernelBitAnd : TBinaryVectorIntKernel<FVectorIntKernelBitAnd>
{
	static void VM_FORCEINLINE DoKernel(VectorRegisterInt* Dst, VectorRegisterInt Src0, VectorRegisterInt Src1)
	{
		*Dst = VectorIntAnd(Src0, Src1);
	}
};

//bit_or,
struct FVectorIntKernelBitOr : TBinaryVectorIntKernel<FVectorIntKernelBitOr>
{
	static void VM_FORCEINLINE DoKernel(VectorRegisterInt* Dst, VectorRegisterInt Src0, VectorRegisterInt Src1)
	{
		*Dst = VectorIntOr(Src0, Src1);
	}
};

//bit_xor,
struct FVectorIntKernelBitXor : TBinaryVectorIntKernel<FVectorIntKernelBitXor>
{
	static void VM_FORCEINLINE DoKernel(VectorRegisterInt* Dst, VectorRegisterInt Src0, VectorRegisterInt Src1)
	{
		*Dst = VectorIntXor(Src0, Src1);
	}
};

//bit_not,
struct FVectorIntKernelBitNot : TUnaryVectorIntKernel<FVectorIntKernelBitNot>
{
	static void VM_FORCEINLINE DoKernel(VectorRegisterInt* Dst, VectorRegisterInt Src0)
	{
		*Dst = VectorIntNot(Src0);
	}
};

//"Boolean" ops. Currently handling bools as integers.
//logic_and,
struct FVectorIntKernelLogicAnd : TBinaryVectorIntKernel<FVectorIntKernelLogicAnd>
{
	static void VM_FORCEINLINE DoKernel(VectorRegisterInt* Dst, VectorRegisterInt Src0, VectorRegisterInt Src1)
	{
		//We need to assume a mask input and produce a mask output so just bitwise ops actually fine for these?
		*Dst = VectorIntAnd(Src0, Src1);
	}
};

//logic_or,
struct FVectorIntKernelLogicOr : TBinaryVectorIntKernel<FVectorIntKernelLogicOr>
{
	static void VM_FORCEINLINE DoKernel(VectorRegisterInt* Dst, VectorRegisterInt Src0, VectorRegisterInt Src1)
	{
		//We need to assume a mask input and produce a mask output so just bitwise ops actually fine for these?
		*Dst = VectorIntOr(Src0, Src1);
	}
};
//logic_xor,
struct FVectorIntKernelLogicXor : TBinaryVectorIntKernel<FVectorIntKernelLogicXor>
{
	static void VM_FORCEINLINE DoKernel(VectorRegisterInt* Dst, VectorRegisterInt Src0, VectorRegisterInt Src1)
	{
		//We need to assume a mask input and produce a mask output so just bitwise ops actually fine for these?
		*Dst = VectorIntXor(Src0, Src1);
	}
};

//logic_not,
struct FVectorIntKernelLogicNot : TUnaryVectorIntKernel<FVectorIntKernelLogicNot>
{
	static void VM_FORCEINLINE DoKernel(VectorRegisterInt* Dst, VectorRegisterInt Src0)
	{
		//We need to assume a mask input and produce a mask output so just bitwise ops actually fine for these?
		*Dst = VectorIntNot(Src0);
	}
};

//conversions
//f2i,
struct FVectorKernelFloatToInt : TUnaryKernel<FVectorKernelFloatToInt, FRegisterDestHandler<VectorRegisterInt>, FConstantHandler<VectorRegister>, FRegisterHandler<VectorRegister>, VECTOR_WIDTH_FLOATS>
{
	static void VM_FORCEINLINE DoKernel(VectorRegisterInt* Dst, VectorRegister Src0)
	{
		*Dst = VectorFloatToInt(Src0);
	}
};

//i2f,
struct FVectorKernelIntToFloat : TUnaryKernel<FVectorKernelIntToFloat, FRegisterDestHandler<VectorRegister>, FConstantHandler<VectorRegisterInt>, FRegisterHandler<VectorRegisterInt>, VECTOR_WIDTH_FLOATS>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* Dst, VectorRegisterInt Src0)
	{
		*Dst = VectorIntToFloat(Src0);
	}
};

//f2b,
struct FVectorKernelFloatToBool : TUnaryKernel<FVectorKernelFloatToBool, FRegisterDestHandler<VectorRegister>, FConstantHandler<VectorRegister>, FRegisterHandler<VectorRegister>, VECTOR_WIDTH_FLOATS>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* Dst, VectorRegister Src0)
	{		
		*Dst = VectorCompareGT(Src0, GlobalVectorConstants::FloatZero);
	}
};

//b2f,
struct FVectorKernelBoolToFloat : TUnaryKernel<FVectorKernelBoolToFloat, FRegisterDestHandler<VectorRegister>, FConstantHandler<VectorRegister>, FRegisterHandler<VectorRegister>, VECTOR_WIDTH_FLOATS>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* Dst, VectorRegister Src0)
	{
		*Dst = VectorSelect(Src0, GlobalVectorConstants::FloatOne, GlobalVectorConstants::FloatZero);
	}
};

//i2b,
struct FVectorKernelIntToBool : TUnaryKernel<FVectorKernelIntToBool, FRegisterDestHandler<VectorRegisterInt>, FConstantHandler<VectorRegisterInt>, FRegisterHandler<VectorRegisterInt>, VECTOR_WIDTH_FLOATS>
{
	static void VM_FORCEINLINE DoKernel(VectorRegisterInt* Dst, VectorRegisterInt Src0)
	{
		*Dst = VectorIntCompareGT(Src0, GlobalVectorConstants::IntZero);
	}
};

//b2i,
struct FVectorKernelBoolToInt : TUnaryKernel<FVectorKernelBoolToInt, FRegisterDestHandler<VectorRegisterInt>, FConstantHandler<VectorRegisterInt>, FRegisterHandler<VectorRegisterInt>, VECTOR_WIDTH_FLOATS>
{
	static void VM_FORCEINLINE DoKernel(VectorRegisterInt* Dst, VectorRegisterInt Src0)
	{
		*Dst = VectorIntSelect(Src0, GlobalVectorConstants::IntOne, GlobalVectorConstants::IntZero);
	}
};


void VectorVM::Init()
{
	static bool Inited = false;
	if (Inited == false)
	{
		// random noise
		float TempTable[17][17][17];
		for (int z = 0; z < 17; z++)
		{
			for (int y = 0; y < 17; y++)
			{
				for (int x = 0; x < 17; x++)
				{
					float f1 = (float)FMath::FRandRange(-1.0f, 1.0f);
					TempTable[x][y][z] = f1;
				}
			}
		}

		// pad
		for (int i = 0; i < 17; i++)
		{
			for (int j = 0; j < 17; j++)
			{
				TempTable[i][j][16] = TempTable[i][j][0];
				TempTable[i][16][j] = TempTable[i][0][j];
				TempTable[16][j][i] = TempTable[0][j][i];
			}
		}

		// compute gradients
		FVector TempTable2[17][17][17];
		for (int z = 0; z < 16; z++)
		{
			for (int y = 0; y < 16; y++)
			{
				for (int x = 0; x < 16; x++)
				{
					FVector XGrad = FVector(1.0f, 0.0f, TempTable[x][y][z] - TempTable[x+1][y][z]);
					FVector YGrad = FVector(0.0f, 1.0f, TempTable[x][y][z] - TempTable[x][y + 1][z]);
					FVector ZGrad = FVector(0.0f, 1.0f, TempTable[x][y][z] - TempTable[x][y][z+1]);

					FVector Grad = FVector(XGrad.Z, YGrad.Z, ZGrad.Z);
					TempTable2[x][y][z] = Grad;
				}
			}
		}

		// pad
		for (int i = 0; i < 17; i++)
		{
			for (int j = 0; j < 17; j++)
			{
				TempTable2[i][j][16] = TempTable2[i][j][0];
				TempTable2[i][16][j] = TempTable2[i][0][j];
				TempTable2[16][j][i] = TempTable2[0][j][i];
			}
		}


		// compute curl of gradient field
		for (int z = 0; z < 16; z++)
		{
			for (int y = 0; y < 16; y++)
			{
				for (int x = 0; x < 16; x++)
				{
					FVector Dy = TempTable2[x][y][z] - TempTable2[x][y + 1][z];
					FVector Sy = TempTable2[x][y][z] + TempTable2[x][y + 1][z];
					FVector Dx = TempTable2[x][y][z] - TempTable2[x + 1][y][z];
					FVector Sx = TempTable2[x][y][z] + TempTable2[x + 1][y][z];
					FVector Dz = TempTable2[x][y][z] - TempTable2[x][y][z + 1];
					FVector Sz = TempTable2[x][y][z] + TempTable2[x][y][z + 1];
					FVector Dir = FVector(Dy.Z - Sz.Y, Dz.X - Sx.Z, Dx.Y - Sy.X);

					FVectorKernelNoise::RandomTable[x][y][z] = MakeVectorRegister(Dir.X, Dir.Y, Dir.Z, 0.0f);
				}
			}
		}

		Inited = true;
	}
}

void VectorVM::Exec(
	uint8 const* Code,
	uint8** InputRegisters,
	int32 NumInputRegisters,
	uint8** OutputRegisters,
	int32 NumOutputRegisters,
	uint8 const* ConstantTable,
	TArray<FDataSetMeta> &DataSetMetaTable,
	FVMExternalFunction* ExternalFunctionTable,
	void** UserPtrTable,
	int32 NumInstances

#if STATS
	, const TArray<TStatId>& StatScopes
#endif
	)
{
	uint32 TempRegisterSize = Align((InstancesPerChunk) * MaxInstanceSizeBytes, VECTOR_WIDTH_BYTES) + VECTOR_WIDTH_BYTES;
	//TODO: Refactor this so VMs are a persistent object with growing buffers. Once spun up, there are no allocs.
	//Can be pooled and used for threading and branching.
	TArray<uint8,TAlignedHeapAllocator<VECTOR_WIDTH_BYTES>> TempRegTable;
	TempRegTable.SetNumUninitialized(TempRegisterSize * NumTempRegisters);
	uint8* RegisterTable[MaxRegisters] = {0};


	// Map temporary registers.
	for (int32 i = 0; i < NumTempRegisters; ++i)
	{
		RegisterTable[i] = TempRegTable.GetData() + TempRegisterSize * i;
	}

	// Map input and output registers.
	//Input and output registers are indexed absolutely directly in their kernels.
	//TODO: No need for these to be in the same table now.
	//TODO: Also no need for the i/o size table as the ops will deal with that now.
 	for (int32 i = 0; i < NumInputRegisters; ++i)
 	{
 		RegisterTable[NumTempRegisters + i] = InputRegisters[i];
 	}
 	for (int32 i = 0; i < NumOutputRegisters; ++i)
 	{
 		RegisterTable[NumTempRegisters + MaxInputRegisters + i] = OutputRegisters[i];
 	}

	// table of index counters, one for each data set
	TArray<int32> DataSetIndexTable;

	// map secondary data sets and fill in the offset table into the register table
	//
	TArray<int32> DataSetOffsetTable;
	for (int32 Idx = 0; Idx < DataSetMetaTable.Num(); Idx++)
	{
		uint32 DataSetOffset = DataSetMetaTable[Idx].NumVariables;
		DataSetOffsetTable.Add(DataSetOffset);
		DataSetIndexTable.Add(DataSetMetaTable[Idx].DataSetAccessIndex);	// prime counter index table with the data set offset; will be incremented with every write for each instance
	}



	// Process one chunk at a time.
	int32 InstancesLeft = NumInstances;
	int32 ChunkIdx = 0;
	while (InstancesLeft > 0)
	{
		// Setup execution context.
		FVectorVMContext Context(Code, RegisterTable, ConstantTable, DataSetIndexTable.GetData(), DataSetOffsetTable.GetData(), 
			ExternalFunctionTable, UserPtrTable, FMath::Min(InstancesLeft, (int32)InstancesPerChunk), InstancesPerChunk * ChunkIdx
#if STATS
			, StatScopes
#endif
		);
		Context.NumSecondaryDataSets = DataSetOffsetTable.Num();
		EVectorVMOp Op = EVectorVMOp::done;

		// Execute VM on all vectors in this chunk.
		do 
		{
			Op = DecodeOp(Context);
			switch (Op)
			{
			// Dispatch kernel ops.
			case EVectorVMOp::add: FVectorKernelAdd::Exec(Context); break;
			case EVectorVMOp::sub: FVectorKernelSub::Exec(Context); break;
			case EVectorVMOp::mul: FVectorKernelMul::Exec(Context); break;
			case EVectorVMOp::div: FVectorKernelDiv::Exec(Context); break;
			case EVectorVMOp::mad: FVectorKernelMad::Exec(Context); break;
			case EVectorVMOp::lerp: FVectorKernelLerp::Exec(Context); break;
			case EVectorVMOp::rcp: FVectorKernelRcp::Exec(Context); break;
			case EVectorVMOp::rsq: FVectorKernelRsq::Exec(Context); break;
			case EVectorVMOp::sqrt: FVectorKernelSqrt::Exec(Context); break;
			case EVectorVMOp::neg: FVectorKernelNeg::Exec(Context); break;
			case EVectorVMOp::abs: FVectorKernelAbs::Exec(Context); break;
			case EVectorVMOp::exp: FVectorKernelExp::Exec(Context); break;
			case EVectorVMOp::exp2: FVectorKernelExp2::Exec(Context); break;
			case EVectorVMOp::log: FVectorKernelLog::Exec(Context); break;
			case EVectorVMOp::log2: FVectorKernelLog2::Exec(Context); break;
			case EVectorVMOp::sin: FVectorKernelSin::Exec(Context); break;
			case EVectorVMOp::cos: FVectorKernelCos::Exec(Context); break;
			case EVectorVMOp::tan: FVectorKernelTan::Exec(Context); break;
			case EVectorVMOp::asin: FVectorKernelASin::Exec(Context); break;
			case EVectorVMOp::acos: FVectorKernelACos::Exec(Context); break;
			case EVectorVMOp::atan: FVectorKernelATan::Exec(Context); break;
			case EVectorVMOp::atan2: FVectorKernelATan2::Exec(Context); break;
			case EVectorVMOp::ceil: FVectorKernelCeil::Exec(Context); break;
			case EVectorVMOp::floor: FVectorKernelFloor::Exec(Context); break;
			case EVectorVMOp::round: FVectorKernelRound::Exec(Context); break;
			case EVectorVMOp::fmod: FVectorKernelMod::Exec(Context); break;
			case EVectorVMOp::frac: FVectorKernelFrac::Exec(Context); break;
			case EVectorVMOp::trunc: FVectorKernelTrunc::Exec(Context); break;
			case EVectorVMOp::clamp: FVectorKernelClamp::Exec(Context); break;
			case EVectorVMOp::min: FVectorKernelMin::Exec(Context); break;
			case EVectorVMOp::max: FVectorKernelMax::Exec(Context); break;
			case EVectorVMOp::pow: FVectorKernelPow::Exec(Context); break;
			case EVectorVMOp::sign: FVectorKernelSign::Exec(Context); break;
			case EVectorVMOp::step: FVectorKernelStep::Exec(Context); break;
			case EVectorVMOp::random: FVectorKernelRandom::Exec(Context); break;
			case EVectorVMOp::noise: VectorVMNoise::Noise1D(Context); break;
			case EVectorVMOp::noise2D: VectorVMNoise::Noise2D(Context); break;
			case EVectorVMOp::noise3D: VectorVMNoise::Noise3D(Context); break;

			case EVectorVMOp::cmplt: FVectorKernelCompareLT::Exec(Context); break;
			case EVectorVMOp::cmple: FVectorKernelCompareLE::Exec(Context); break;
			case EVectorVMOp::cmpgt: FVectorKernelCompareGT::Exec(Context); break;
			case EVectorVMOp::cmpge: FVectorKernelCompareGE::Exec(Context); break;
			case EVectorVMOp::cmpeq: FVectorKernelCompareEQ::Exec(Context); break;
			case EVectorVMOp::cmpneq: FVectorKernelCompareNEQ::Exec(Context); break;
			case EVectorVMOp::select: FVectorKernelSelect::Exec(Context); break;

			case EVectorVMOp::addi: FVectorIntKernelAdd::Exec(Context); break;
			case EVectorVMOp::subi: FVectorIntKernelSubtract::Exec(Context); break;
			case EVectorVMOp::muli: FVectorIntKernelMultiply::Exec(Context); break;
			case EVectorVMOp::clampi: FVectorIntKernelClamp::Exec(Context); break;
			case EVectorVMOp::mini: FVectorIntKernelMin::Exec(Context); break;
			case EVectorVMOp::maxi: FVectorIntKernelMax::Exec(Context); break;
			case EVectorVMOp::absi: FVectorIntKernelAbs::Exec(Context); break;
			case EVectorVMOp::negi: FVectorIntKernelNegate::Exec(Context); break;
			case EVectorVMOp::signi: FVectorIntKernelSign::Exec(Context); break;
			case EVectorVMOp::randomi: FScalarIntKernelRandom::Exec(Context); break;
			case EVectorVMOp::cmplti: FVectorIntKernelCompareLT::Exec(Context); break;
			case EVectorVMOp::cmplei: FVectorIntKernelCompareLE::Exec(Context); break;
			case EVectorVMOp::cmpgti: FVectorIntKernelCompareGT::Exec(Context); break;
			case EVectorVMOp::cmpgei: FVectorIntKernelCompareGE::Exec(Context); break;
			case EVectorVMOp::cmpeqi: FVectorIntKernelCompareEQ::Exec(Context); break;
			case EVectorVMOp::cmpneqi: FVectorIntKernelCompareNEQ::Exec(Context); break;
			case EVectorVMOp::bit_and: FVectorIntKernelBitAnd::Exec(Context); break;
			case EVectorVMOp::bit_or: FVectorIntKernelBitOr::Exec(Context); break;
			case EVectorVMOp::bit_xor: FVectorIntKernelBitXor::Exec(Context); break;
			case EVectorVMOp::bit_not: FVectorIntKernelBitNot::Exec(Context); break;
			case EVectorVMOp::logic_and: FVectorIntKernelLogicAnd::Exec(Context); break;
			case EVectorVMOp::logic_or: FVectorIntKernelLogicOr::Exec(Context); break;
			case EVectorVMOp::logic_xor: FVectorIntKernelLogicXor::Exec(Context); break;
			case EVectorVMOp::logic_not: FVectorIntKernelLogicNot::Exec(Context); break;
			case EVectorVMOp::f2i: FVectorKernelFloatToInt::Exec(Context); break;
			case EVectorVMOp::i2f: FVectorKernelIntToFloat::Exec(Context); break;
			case EVectorVMOp::f2b: FVectorKernelFloatToBool::Exec(Context); break;
			case EVectorVMOp::b2f: FVectorKernelBoolToFloat::Exec(Context); break;
			case EVectorVMOp::i2b: FVectorKernelIntToBool::Exec(Context); break;
			case EVectorVMOp::b2i: FVectorKernelBoolToInt::Exec(Context); break;

			case EVectorVMOp::outputdata_32bit:	FScalarKernelWriteOutputIndexed<int32>::Exec(Context);	break;
			case EVectorVMOp::inputdata_32bit: FVectorKernelReadInput<int32>::Exec(Context); break;
			//case EVectorVMOp::inputdata_32bit: FVectorKernelReadInput32::Exec(Context); break;
			case EVectorVMOp::inputdata_noadvance_32bit: FVectorKernelReadInputNoAdvance<int32>::Exec(Context); break;
			case EVectorVMOp::acquireindex:	FScalarKernelAcquireCounterIndex::Exec(Context); break;
			case EVectorVMOp::external_func_call: FKernelExternalFunctionCall::Exec(Context); break;

			case EVectorVMOp::exec_index: FVectorKernelExecutionIndex::Exec(Context); break;

			case EVectorVMOp::enter_stat_scope: FVectorKernelEnterStatScope::Exec(Context); break;
			case EVectorVMOp::exit_stat_scope: FVectorKernelExitStatScope::Exec(Context); break;

			// Execution always terminates with a "done" opcode.
			case EVectorVMOp::done:
				break;

			// Opcode not recognized / implemented.
			default:
				UE_LOG(LogVectorVM, Error, TEXT("Unknown op code 0x%02x"), (uint32)Op);
				return;//BAIL
			}
		} while (Op != EVectorVMOp::done);

		InstancesLeft -= InstancesPerChunk;
		++ChunkIdx;
	}

	// write back data set access indices, so we know how much was written to each data set
	for (int32 Idx = 0; Idx < DataSetMetaTable.Num(); Idx++)
	{
		DataSetMetaTable[Idx].DataSetAccessIndex = DataSetIndexTable[Idx];	
	}

}

uint8 VectorVM::GetNumOpCodes()
{
	return (uint8)EVectorVMOp::NumOpcodes;
}

#if WITH_EDITOR
FString VectorVM::GetOpName(EVectorVMOp Op)
{
	static UEnum* EnumStateObj = FindObject<UEnum>(ANY_PACKAGE, TEXT("EVectorVMOp"), true);	
	check(EnumStateObj);

	FString OpStr = EnumStateObj->GetNameByValue((uint8)Op).ToString();
	int32 LastIdx = 0;
	OpStr.FindLastChar(TEXT(':'),LastIdx);
	return OpStr.RightChop(LastIdx);
}

FString VectorVM::GetOperandLocationName(EVectorVMOperandLocation Location)
{
	static UEnum* EnumStateObj = FindObject<UEnum>(ANY_PACKAGE, TEXT("EVectorVMOperandLocation"), true);
	check(EnumStateObj);

	FString LocStr = EnumStateObj->GetNameByValue((uint8)Location).ToString();	
	int32 LastIdx = 0;
	LocStr.FindLastChar(TEXT(':'), LastIdx);
	return LocStr.RightChop(LastIdx);
}
#endif

#undef VM_FORCEINLINE
