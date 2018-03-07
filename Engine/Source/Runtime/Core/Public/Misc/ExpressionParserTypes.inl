// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreFwd.h"
#include "Templates/RemoveReference.h"
#include "Templates/PointerIsConvertibleFromTo.h"

#define LOCTEXT_NAMESPACE "ExpressionParser"

class FExpressionNode;
class FExpressionToken;
struct FOperatorFunctionID;
template< class T > struct TRemoveConst;

namespace Impl
{
	/** Interface for a wrapper utility for any moveable/copyable data */
	struct IExpressionNodeStorage
	{
		virtual ~IExpressionNodeStorage() {}
		/** Move this type into another unallocated buffer (move-construct a new type from our wrapped value) */
		virtual void Reseat(uint8* Dst) = 0;
		/** Move this type to a buffer already allocated to the same type (uses type-defined move-assignment) */
		virtual void MoveAssign(uint8* Dst) = 0;
		/** Copy this data */
		virtual FExpressionNode Copy() const = 0;
	};

	/** Implementation of the wrapper utility for any moveable/copyable data, that allows us to virtually move/copy/destruct the data */
	/** Data is stored inline in this implementation, for efficiency */
 	template<typename T>
	struct FInlineDataStorage : IExpressionNodeStorage
	{
		/** The data itself, allocated on the stack */
		T Value;

		/** Constructor/destructor */
		FInlineDataStorage(T InValue) : Value(MoveTemp(InValue)) {}
		~FInlineDataStorage(){}
		
		const T* Access() const { return &Value; }

		virtual void Reseat(uint8* Dst) override 			{ new(Dst) FInlineDataStorage(MoveTemp(Value)); }
		virtual void MoveAssign(uint8* Dst) override 		{ reinterpret_cast<FInlineDataStorage*>(Dst)->Value = MoveTemp(Value); }
		virtual FExpressionNode Copy() const override		{ return Value; }
	};

	/** Data is stored on the heap in this implementation */
	template<typename T>
	struct FHeapDataStorage : IExpressionNodeStorage
	{
		/** The data itself, allocated on the heap */
		TUniquePtr<T> Value;
		
		/** Constructor/destructor */
		FHeapDataStorage(T InValue) : Value(new T(MoveTemp(InValue))) {}
		FHeapDataStorage(FHeapDataStorage&& In) : Value(MoveTemp(In.Value)) {}
		~FHeapDataStorage(){}
		
		const T* Access() const { return Value.Get(); }

		virtual void Reseat(uint8* Dst) override 			{ new(Dst) FHeapDataStorage(MoveTemp(*this)); }
		virtual void MoveAssign(uint8* Dst) override 		{ reinterpret_cast<FHeapDataStorage*>(Dst)->Value = MoveTemp(Value); }
		virtual FExpressionNode Copy() const override		{ return *Value; }
	};

	template<typename T, uint32 MaxStackAllocationSize, typename Enabled=void> struct TStorageTypeDeduction;
	template<typename T, uint32 MaxStackAllocationSize> struct TStorageTypeDeduction<T, MaxStackAllocationSize, typename TEnableIf<(sizeof(FInlineDataStorage<T>) <= MaxStackAllocationSize)>::Type>
		{ typedef FInlineDataStorage<T> Type; };
	template<typename T, uint32 MaxStackAllocationSize> struct TStorageTypeDeduction<T, MaxStackAllocationSize, typename TEnableIf<(sizeof(FInlineDataStorage<T>) >  MaxStackAllocationSize)>::Type>
		{ typedef FHeapDataStorage<T> 	Type; };


	/** Machinery required for operator mapping */
	template <typename> struct TCallableInfo;
	template <typename> struct TCallableInfoImpl;

	template <typename Ret_, typename T, typename Arg1_>
	struct TCallableInfoImpl<Ret_ (T::*)(Arg1_)> { typedef Ret_ Ret; typedef Arg1_ Arg1; enum { NumArgs = 1 }; };
	template <typename Ret_, typename T, typename Arg1_>
	struct TCallableInfoImpl<Ret_ (T::*)(Arg1_) const> { typedef Ret_ Ret; typedef Arg1_ Arg1; enum { NumArgs = 1 };  };

	template <typename Ret_, typename T, typename Arg1_, typename Arg2_>
	struct TCallableInfoImpl<Ret_ (T::*)(Arg1_, Arg2_)> { typedef Ret_ Ret; typedef Arg1_ Arg1; typedef Arg2_ Arg2; enum { NumArgs = 2 };  };
	template <typename Ret_, typename T, typename Arg1_, typename Arg2_>
	struct TCallableInfoImpl<Ret_ (T::*)(Arg1_, Arg2_) const> { typedef Ret_ Ret; typedef Arg1_ Arg1; typedef Arg2_ Arg2; enum { NumArgs = 2 };  };
	
	template <typename Ret_, typename T, typename Arg1_, typename Arg2_, typename Arg3_>
	struct TCallableInfoImpl<Ret_ (T::*)(Arg1_, Arg2_, Arg3_)> { typedef Ret_ Ret; typedef Arg1_ Arg1; typedef Arg2_ Arg2; typedef Arg3_ Arg3; enum { NumArgs = 3 }; };
	template <typename Ret_, typename T, typename Arg1_, typename Arg2_, typename Arg3_>
	struct TCallableInfoImpl<Ret_ (T::*)(Arg1_, Arg2_, Arg3_) const> { typedef Ret_ Ret; typedef Arg1_ Arg1; typedef Arg2_ Arg2; typedef Arg3_ Arg3; enum { NumArgs = 3 }; };

	/** @todo: decltype(&T::operator()) can go directly in the specialization below if it weren't for VS2012 support */
	template<typename T>
	struct TGetOperatorCallPtr { typedef decltype(&T::operator()) Type; };

	template <typename T>
	struct TCallableInfo : TCallableInfoImpl<typename TGetOperatorCallPtr<T>::Type> {};

	/** Overloaded function that returns an FExpressionResult, regardless of what is passed in */
	template<typename T>
	static FExpressionResult ForwardReturnType(T&& Result) 					{ return MakeValue(MoveTemp(Result)); }
	static FExpressionResult ForwardReturnType(FExpressionResult&& Result) 	{ return MoveTemp(Result); }

	/** Wrapper function for supplied functions of the signature T(A) */
	template<typename OperandType, typename ContextType, typename FuncType>
	static typename TEnableIf<Impl::TCallableInfo<FuncType>::NumArgs == 1, typename TOperatorJumpTable<ContextType>::FUnaryFunction>::Type WrapUnaryFunction(FuncType In)
	{
		// Ignore the context
		return [=](const FExpressionNode& InOperand, const ContextType* Context) {
			return ForwardReturnType(In(*InOperand.Cast<OperandType>()));
		};
	}

	/** Wrapper function for supplied functions of the signature T(A, const ContextType* Context) */
	template<typename OperandType, typename ContextType, typename FuncType>
	static typename TEnableIf<Impl::TCallableInfo<FuncType>::NumArgs == 2, typename TOperatorJumpTable<ContextType>::FUnaryFunction>::Type WrapUnaryFunction(FuncType In)
	{
		// Ignore the context
		return [=](const FExpressionNode& InOperand, const ContextType* Context) {
			return ForwardReturnType(In(*InOperand.Cast<OperandType>(), Context));
		};
	}

	/** Wrapper function for supplied functions of the signature T(A, B) */
	template<typename LeftOperandType, typename RightOperandType, typename ContextType, typename FuncType>
	static typename TEnableIf<Impl::TCallableInfo<FuncType>::NumArgs == 2, typename TOperatorJumpTable<ContextType>::FBinaryFunction>::Type WrapBinaryFunction(FuncType In)
	{
		// Ignore the context
		return [=](const FExpressionNode& InLeftOperand, const FExpressionNode& InRightOperand, const ContextType* Context) {
			return ForwardReturnType(In(*InLeftOperand.Cast<LeftOperandType>(), *InRightOperand.Cast<RightOperandType>()));
		};
	}

	/** Wrapper function for supplied functions of the signature T(A, B, const ContextType* Context) */
	template<typename LeftOperandType, typename RightOperandType, typename ContextType, typename FuncType>
	static typename TEnableIf<Impl::TCallableInfo<FuncType>::NumArgs == 3, typename TOperatorJumpTable<ContextType>::FBinaryFunction>::Type WrapBinaryFunction(FuncType In)
	{
		// Ignore the context
		return [=](const FExpressionNode& InLeftOperand, const FExpressionNode& InRightOperand, const ContextType* Context) {
			return ForwardReturnType(In(*InLeftOperand.Cast<LeftOperandType>(), *InRightOperand.Cast<RightOperandType>(), Context));
		};
	}

}

template<typename T>
FExpressionNode::FExpressionNode(T In, typename TEnableIf<!TPointerIsConvertibleFromTo<T,FExpressionNode>::Value>::Type*)
	: TypeId(TGetExpressionNodeTypeId<T>::GetTypeId())
{
	// Choose the relevant allocation strategy based on the size of the type
	new(InlineBytes) typename Impl::TStorageTypeDeduction<T, MaxStackAllocationSize>::Type(MoveTemp(In));
}

template<typename T>
const T* FExpressionNode::Cast() const
{
	if (TypeId == TGetExpressionNodeTypeId<T>::GetTypeId())
	{
		return reinterpret_cast<const typename Impl::TStorageTypeDeduction<T, MaxStackAllocationSize>::Type*>(InlineBytes)->Access();
	}
	return nullptr;
}

// End FExpresionNode

template<typename ContextType>
FExpressionResult TOperatorJumpTable<ContextType>::ExecBinary(const FExpressionToken& Operator, const FExpressionToken& L, const FExpressionToken& R, const ContextType* Context) const
{
	FOperatorFunctionID ID = { Operator.Node.GetTypeId(), L.Node.GetTypeId(), R.Node.GetTypeId() };
	if (const auto* Func = BinaryOps.Find(ID))
	{
		return (*Func)(L.Node, R.Node, Context);
	}

	FFormatOrderedArguments Args;
	Args.Add(FText::FromString(Operator.Context.GetString()));
	Args.Add(FText::FromString(L.Context.GetString()));
	Args.Add(FText::FromString(R.Context.GetString()));
	return MakeError(FText::Format(LOCTEXT("BinaryExecutionError", "Binary operator {0} cannot operate on {1} and {2}"), Args));
}

template<typename ContextType>
FExpressionResult TOperatorJumpTable<ContextType>::ExecPreUnary(const FExpressionToken& Operator, const FExpressionToken& R, const ContextType* Context) const
{
	FOperatorFunctionID ID = { Operator.Node.GetTypeId(), FGuid(), R.Node.GetTypeId() };
	if (const auto* Func = PreUnaryOps.Find(ID))
	{
		return (*Func)(R.Node, Context);
	}

	FFormatOrderedArguments Args;
	Args.Add(FText::FromString(Operator.Context.GetString()));
	Args.Add(FText::FromString(R.Context.GetString()));
	return MakeError(FText::Format(LOCTEXT("PreUnaryExecutionError", "Pre-unary operator {0} cannot operate on {1}"), Args));
}

template<typename ContextType>
FExpressionResult TOperatorJumpTable<ContextType>::ExecPostUnary(const FExpressionToken& Operator, const FExpressionToken& L, const ContextType* Context) const
{
	FOperatorFunctionID ID = { Operator.Node.GetTypeId(), L.Node.GetTypeId(), FGuid() };
	if (const auto* Func = PostUnaryOps.Find(ID))
	{
		return (*Func)(L.Node, Context);
	}

	FFormatOrderedArguments Args;
	Args.Add(FText::FromString(Operator.Context.GetString()));
	Args.Add(FText::FromString(L.Context.GetString()));
	return MakeError(FText::Format(LOCTEXT("PostUnaryExecutionError", "Post-unary operator {0} cannot operate on {1}"), Args));
}

template<typename ContextType>
template<typename OperatorType, typename FuncType>
void TOperatorJumpTable<ContextType>::MapPreUnary(FuncType InFunc)
{
	typedef typename TRemoveConst<typename TRemoveReference<typename Impl::TCallableInfo<FuncType>::Arg1>::Type>::Type OperandType;

	FOperatorFunctionID ID = {
		TGetExpressionNodeTypeId<OperatorType>::GetTypeId(),
		FGuid(),
		TGetExpressionNodeTypeId<OperandType>::GetTypeId()
	};

	PreUnaryOps.Add(ID, Impl::WrapUnaryFunction<OperandType, ContextType>(InFunc));
}

template<typename ContextType>
template<typename OperatorType, typename FuncType>
void TOperatorJumpTable<ContextType>::MapPostUnary(FuncType InFunc)
{
	typedef typename TRemoveConst<typename TRemoveReference<typename Impl::TCallableInfo<FuncType>::Arg1>::Type>::Type OperandType;

	FOperatorFunctionID ID = {
		TGetExpressionNodeTypeId<OperatorType>::GetTypeId(),
		TGetExpressionNodeTypeId<OperandType>::GetTypeId(),
		FGuid()
	};

	PostUnaryOps.Add(ID, Impl::WrapUnaryFunction<OperandType, ContextType>(InFunc));
}

template<typename ContextType>
template<typename OperatorType, typename FuncType>
void TOperatorJumpTable<ContextType>::MapBinary(FuncType InFunc)
{
	typedef typename TRemoveConst<typename TRemoveReference<typename Impl::TCallableInfo<FuncType>::Arg1>::Type>::Type LeftOperandType;
	typedef typename TRemoveConst<typename TRemoveReference<typename Impl::TCallableInfo<FuncType>::Arg2>::Type>::Type RightOperandType;

	FOperatorFunctionID ID = {
		TGetExpressionNodeTypeId<OperatorType>::GetTypeId(),
		TGetExpressionNodeTypeId<LeftOperandType>::GetTypeId(),
		TGetExpressionNodeTypeId<RightOperandType>::GetTypeId()
	};

	BinaryOps.Add(ID, Impl::WrapBinaryFunction<LeftOperandType, RightOperandType, ContextType>(InFunc));
}

typedef TOperatorEvaluationEnvironment<> FOperatorEvaluationEnvironment;

#undef LOCTEXT_NAMESPACE
