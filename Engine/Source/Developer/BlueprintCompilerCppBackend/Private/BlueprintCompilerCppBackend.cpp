// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "BlueprintCompilerCppBackend.h"
#include "UObject/UnrealType.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "UObject/Interface.h"
#include "Engine/LatentActionManager.h"
#include "Engine/UserDefinedEnum.h"
#include "K2Node.h"
#include "K2Node_Event.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ExecutionSequence.h"
#include "K2Node_FunctionEntry.h"
#include "KismetCompilerMisc.h"
#include "KismetCompiler.h"
#include "BlueprintCompilerCppBackendUtils.h"
#include "Kismet/KismetNodeHelperLibrary.h"
#include "Kismet/KismetArrayLibrary.h"
#include "UniquePtr.h"

// Generates single "if" scope. Its condition checks context of given term.
struct FSafeContextScopedEmmitter
{
private:
	FEmitterLocalContext& EmitterContext;
	bool bSafeContextUsed;
public:

	bool IsSafeContextUsed() const
	{
		return bSafeContextUsed;
	}

	static FString ValidationChain(FEmitterLocalContext& EmitterContext, const FBPTerminal* Term, FBlueprintCompilerCppBackend& CppBackend)
	{
		TArray<FString> SafetyConditions;
		for (; Term; Term = Term->Context)
		{
			if (!Term->IsStructContextType() && (Term->Type.PinSubCategory != TEXT("self")))
			{
				SafetyConditions.Add(CppBackend.TermToText(EmitterContext, Term, ENativizedTermUsage::Getter, false));
			}
		}

		FString Result;
		for (int32 Iter = SafetyConditions.Num() - 1; Iter >= 0; --Iter)
		{
			Result += FString(TEXT("IsValid("));
			Result += SafetyConditions[Iter];
			Result += FString(TEXT(")"));
			if (Iter)
			{
				Result += FString(TEXT(" && "));
			}
		}

		return Result;
	}

	FSafeContextScopedEmmitter(FEmitterLocalContext& InEmitterContext, const FBPTerminal* Term, FBlueprintCompilerCppBackend& CppBackend)
		: EmitterContext(InEmitterContext)
		, bSafeContextUsed(false)
	{
		const FString Conditions = ValidationChain(EmitterContext, Term, CppBackend);

		if (!Conditions.IsEmpty())
		{
			bSafeContextUsed = true;
			EmitterContext.AddLine(FString::Printf(TEXT("if(%s)"), *Conditions));
			EmitterContext.AddLine(TEXT("{"));
			EmitterContext.IncreaseIndent();
		}
	}

	~FSafeContextScopedEmmitter()
	{
		if (bSafeContextUsed)
		{
			EmitterContext.DecreaseIndent();
			EmitterContext.AddLine(TEXT("}"));
		}
	}

};

void FBlueprintCompilerCppBackend::EmitCallDelegateStatment(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	check(Statement.FunctionContext && Statement.FunctionContext->AssociatedVarProperty);
	FSafeContextScopedEmmitter SafeContextScope(EmitterContext, Statement.FunctionContext->Context, *this);
	EmitterContext.AddLine(FString::Printf(TEXT("%s.Broadcast(%s);"), *TermToText(EmitterContext, Statement.FunctionContext, ENativizedTermUsage::Getter, false), *EmitMethodInputParameterList(EmitterContext, Statement)));
}

void FBlueprintCompilerCppBackend::EmitCallStatment(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	const bool bCallOnDifferentObject = Statement.FunctionContext && (Statement.FunctionContext->Name != TEXT("self"));
	const bool bStaticCall = Statement.FunctionToCall->HasAnyFunctionFlags(FUNC_Static);
	const bool bUseSafeContext = bCallOnDifferentObject && !bStaticCall;

	FString CalledNamePostfix;
	if (Statement.TargetLabel && UberGraphContext && (UberGraphContext->Function == Statement.FunctionToCall) && UberGraphContext->UnsortedSeparateExecutionGroups.Num())
	{
		int32* ExecutionGroupIndexPtr = UberGraphStatementToExecutionGroup.Find(Statement.TargetLabel);
		if (ensure(ExecutionGroupIndexPtr))
		{
			CalledNamePostfix = FString::Printf(TEXT("_%d"), *ExecutionGroupIndexPtr);
		}
	}

	{
		FSafeContextScopedEmmitter SafeContextScope(EmitterContext, bUseSafeContext ? Statement.FunctionContext : nullptr, *this);
		FString Result = EmitCallStatmentInner(EmitterContext, Statement, false, CalledNamePostfix);
		EmitterContext.AddLine(Result);
	}
}

struct FSetterExpressionBuilder
{
	FString EndCustomSetExpression;
	FString DestinationExpression;

	FSetterExpressionBuilder(FBlueprintCompilerCppBackend& CppBackend, FEmitterLocalContext& EmitterContext, FBPTerminal* LHS)
	{
		DestinationExpression = CppBackend.TermToText(EmitterContext, LHS, ENativizedTermUsage::Setter, false, &EndCustomSetExpression);
	}

	FString BuildStart() const 
	{
		FString Result = DestinationExpression;
		const bool bCustomExpression = !EndCustomSetExpression.IsEmpty();
		if (!bCustomExpression)
		{
			//It is not use regular = operator
			Result += TEXT(" = ");
		}
		return Result;
	}

	FString BuildEnd(bool bAddSemicolon) const
	{
		FString Result = EndCustomSetExpression;
		if (bAddSemicolon)
		{
			Result += TEXT(";");
		}
		return Result;
	}

	FString BuildFull(const FString& SourceExpression) const
	{
		const FString Start = BuildStart();
		const FString End = BuildEnd(true);
		return FString::Printf(TEXT("%s%s%s"), *Start, *SourceExpression, *End);
	}
};

void FBlueprintCompilerCppBackend::EmitAssignmentStatment(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	check(Statement.LHS && Statement.RHS[0]);
	const FString SourceExpression = TermToText(EmitterContext, Statement.RHS[0], ENativizedTermUsage::Getter);
	FSetterExpressionBuilder SetterExpression(*this, EmitterContext, Statement.LHS);
	FSafeContextScopedEmmitter SafeContextScope(EmitterContext, Statement.LHS->Context, *this);

	FString BeginCast;
	FString EndCast;
	FEmitHelper::GenerateAutomaticCast(EmitterContext, Statement.LHS->Type, Statement.RHS[0]->Type, Statement.LHS->AssociatedVarProperty, Statement.RHS[0]->AssociatedVarProperty, BeginCast, EndCast);
	const FString RHS = FString::Printf(TEXT("%s%s%s"), *BeginCast, *SourceExpression, *EndCast);
	EmitterContext.AddLine(SetterExpression.BuildFull(RHS));
}

void FBlueprintCompilerCppBackend::EmitCastObjToInterfaceStatement(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	FString InterfaceClass = TermToText(EmitterContext, Statement.RHS[0], ENativizedTermUsage::Getter);
	FString ObjectValue = TermToText(EmitterContext, Statement.RHS[1], ENativizedTermUsage::Getter);
	FString InterfaceValue = TermToText(EmitterContext, Statement.LHS, ENativizedTermUsage::UnspecifiedOrReference);

	// Both here and in UObject::execObjectToInterface IsValid function should be used.

	if (ObjectValue.Equals(TEXT("this")))
	{
		//if ObjectValue is "this", we will be checking "this" against nullptr, which will not pass a strict compiler check (e.g. PS4)
		EmitterContext.AddLine(FString::Printf(TEXT("if ( %s->GetClass()->ImplementsInterface(%s) )"), *ObjectValue, *InterfaceClass));
	}
	else
	{
		EmitterContext.AddLine(FString::Printf(TEXT("if ( %s && %s->GetClass()->ImplementsInterface(%s) )"), *ObjectValue, *ObjectValue, *InterfaceClass));
	}
	EmitterContext.AddLine(FString::Printf(TEXT("{")));
	EmitterContext.AddLine(FString::Printf(TEXT("\t%s.SetObject(%s);"), *InterfaceValue, *ObjectValue));
	EmitterContext.AddLine(FString::Printf(TEXT("\tvoid* IAddress = %s->GetInterfaceAddress(%s);"), *ObjectValue, *InterfaceClass));
	EmitterContext.AddLine(FString::Printf(TEXT("\t%s.SetInterface(IAddress);"), *InterfaceValue));
	EmitterContext.AddLine(FString::Printf(TEXT("}")));
	EmitterContext.AddLine(FString::Printf(TEXT("else")));
	EmitterContext.AddLine(FString::Printf(TEXT("{")));
	EmitterContext.AddLine(FString::Printf(TEXT("\t%s.SetObject(nullptr);"), *InterfaceValue));
	EmitterContext.AddLine(FString::Printf(TEXT("}")));
}

void FBlueprintCompilerCppBackend::EmitCastBetweenInterfacesStatement(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	FString ClassToCastTo = TermToText(EmitterContext, Statement.RHS[0], ENativizedTermUsage::Getter);
	FString InputInterface = TermToText(EmitterContext, Statement.RHS[1], ENativizedTermUsage::Getter);
	FString ResultInterface = TermToText(EmitterContext, Statement.LHS, ENativizedTermUsage::UnspecifiedOrReference);

	FString InputObject = FString::Printf(TEXT("%s.GetObjectRef()"), *InputInterface);

	EmitterContext.AddLine(FString::Printf(TEXT("if ( %s && %s->GetClass()->IsChildOf(%s) )"), *InputObject, *InputObject, *ClassToCastTo));
	EmitterContext.AddLine(FString::Printf(TEXT("{")));
	EmitterContext.AddLine(FString::Printf(TEXT("\t%s.SetObject(%s);"), *ResultInterface, *InputObject));
	EmitterContext.AddLine(FString::Printf(TEXT("\tvoid* IAddress = %s->GetInterfaceAddress(%s);"), *InputObject, *ClassToCastTo));
	EmitterContext.AddLine(FString::Printf(TEXT("\t%s.SetInterface(IAddress);"), *ResultInterface));
	EmitterContext.AddLine(FString::Printf(TEXT("}")));
	EmitterContext.AddLine(FString::Printf(TEXT("else")));
	EmitterContext.AddLine(FString::Printf(TEXT("{")));
	EmitterContext.AddLine(FString::Printf(TEXT("\t%s.SetObject(nullptr);"), *ResultInterface));
	EmitterContext.AddLine(FString::Printf(TEXT("}")));
}

static FString GenerateCastRHS(FEmitterLocalContext& EmitterContext, UClass* ClassPtr, const FString& ObjectValue)
{
	check(ClassPtr != nullptr);

	auto BPGC = Cast<UBlueprintGeneratedClass>(ClassPtr);
	if (BPGC && !EmitterContext.Dependencies.WillClassBeConverted(BPGC))
	{
		const FString NativeClass = FEmitHelper::GetCppName(EmitterContext.GetFirstNativeOrConvertedClass(ClassPtr));
		const FString TargetClass = EmitterContext.FindGloballyMappedObject(ClassPtr, UClass::StaticClass(), true);
		return FString::Printf(TEXT("NoNativeCast<%s>(%s, %s)"), *NativeClass, *TargetClass, *ObjectValue);
	}
	else
	{
		const FString TargetClass = FEmitHelper::GetCppName(ClassPtr);
		return FString::Printf(TEXT("Cast<%s>(%s)"), *TargetClass, *ObjectValue);
	}
}

void FBlueprintCompilerCppBackend::EmitCastInterfaceToObjStatement(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	const FString InputInterface = TermToText(EmitterContext, Statement.RHS[1], ENativizedTermUsage::Getter);
	FSetterExpressionBuilder SetterExpression(*this, EmitterContext, Statement.LHS);
	FSafeContextScopedEmmitter SafeContextScope(EmitterContext, Statement.LHS->Context, *this);

	const FString CastRHS = GenerateCastRHS(EmitterContext, CastChecked<UClass>(Statement.RHS[0]->ObjectLiteral), FString::Printf(TEXT("%s.GetObjectRef()"), *InputInterface));
	EmitterContext.AddLine(SetterExpression.BuildFull(CastRHS));
}

void FBlueprintCompilerCppBackend::EmitDynamicCastStatement(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	const FString ObjectValue = TermToText(EmitterContext, Statement.RHS[1], ENativizedTermUsage::Getter);
	FSetterExpressionBuilder SetterExpression(*this, EmitterContext, Statement.LHS);
	FSafeContextScopedEmmitter SafeContextScope(EmitterContext, Statement.LHS->Context, *this);
	
	const FString CastRHS = GenerateCastRHS(EmitterContext, CastChecked<UClass>(Statement.RHS[0]->ObjectLiteral), ObjectValue);
	EmitterContext.AddLine(SetterExpression.BuildFull(CastRHS));
}

void FBlueprintCompilerCppBackend::EmitMetaCastStatement(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	const FString DesiredClass = TermToText(EmitterContext, Statement.RHS[0], ENativizedTermUsage::Getter);
	const FString SourceClass = TermToText(EmitterContext, Statement.RHS[1], ENativizedTermUsage::Getter);
	FSetterExpressionBuilder SetterExpression(*this, EmitterContext, Statement.LHS);
	FSafeContextScopedEmmitter SafeContextScope(EmitterContext, Statement.LHS->Context, *this);

	const FString CastRHS = FString::Printf(TEXT("DynamicMetaCast(%s, %s);"), *DesiredClass, *SourceClass);
	EmitterContext.AddLine(SetterExpression.BuildFull(CastRHS));
}

void FBlueprintCompilerCppBackend::EmitObjectToBoolStatement(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	const FString ObjectTarget = TermToText(EmitterContext, Statement.RHS[0], ENativizedTermUsage::Getter);
	FSetterExpressionBuilder SetterExpression(*this, EmitterContext, Statement.LHS);
	FSafeContextScopedEmmitter SafeContextScope(EmitterContext, Statement.LHS->Context, *this);

	const FString RHS = FString::Printf(TEXT("(%s != nullptr);"), *ObjectTarget);
	EmitterContext.AddLine(SetterExpression.BuildFull(RHS));
}

void FBlueprintCompilerCppBackend::EmitAddMulticastDelegateStatement(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	check(Statement.LHS && Statement.LHS->AssociatedVarProperty);
	FSafeContextScopedEmmitter SafeContextScope(EmitterContext, Statement.LHS->Context, *this);

	const FString Delegate = TermToText(EmitterContext, Statement.LHS, ENativizedTermUsage::UnspecifiedOrReference, false);
	const FString DelegateToAdd = TermToText(EmitterContext, Statement.RHS[0], ENativizedTermUsage::Getter);

	EmitterContext.AddLine(FString::Printf(TEXT("%s.AddUnique(%s);"), *Delegate, *DelegateToAdd));
}

void FBlueprintCompilerCppBackend::EmitRemoveMulticastDelegateStatement(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	check(Statement.LHS && Statement.LHS->AssociatedVarProperty);
	FSafeContextScopedEmmitter SafeContextScope(EmitterContext, Statement.LHS->Context, *this);

	const FString Delegate = TermToText(EmitterContext, Statement.LHS, ENativizedTermUsage::UnspecifiedOrReference, false);
	const FString DelegateToAdd = TermToText(EmitterContext, Statement.RHS[0], ENativizedTermUsage::Getter);

	EmitterContext.AddLine(FString::Printf(TEXT("%s.Remove(%s);"), *Delegate, *DelegateToAdd));
}

void FBlueprintCompilerCppBackend::EmitBindDelegateStatement(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	check(2 == Statement.RHS.Num());
	check(Statement.LHS);
	FSafeContextScopedEmmitter SafeContextScope(EmitterContext, Statement.LHS->Context, *this);

	const FString Delegate = TermToText(EmitterContext, Statement.LHS, ENativizedTermUsage::UnspecifiedOrReference, false);
	const FString NameTerm = TermToText(EmitterContext, Statement.RHS[0], ENativizedTermUsage::Getter);
	const FString ObjectTerm = TermToText(EmitterContext, Statement.RHS[1], ENativizedTermUsage::Getter);

	EmitterContext.AddLine(FString::Printf(TEXT("%s.BindUFunction(%s,%s);"), *Delegate, *ObjectTerm, *NameTerm));
}

void FBlueprintCompilerCppBackend::EmitClearMulticastDelegateStatement(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	check(Statement.LHS);
	FSafeContextScopedEmmitter SafeContextScope(EmitterContext, Statement.LHS->Context, *this);

	const FString Delegate = TermToText(EmitterContext, Statement.LHS, ENativizedTermUsage::UnspecifiedOrReference, false);

	EmitterContext.AddLine(FString::Printf(TEXT("%s.Clear();"), *Delegate));
}

void FBlueprintCompilerCppBackend::EmitCreateArrayStatement(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	FBPTerminal* ArrayTerm = Statement.LHS;
	const FString Array = TermToText(EmitterContext, ArrayTerm, ENativizedTermUsage::UnspecifiedOrReference);

	EmitterContext.AddLine(FString::Printf(TEXT("%s.SetNum(%d, true);"), *Array, Statement.RHS.Num()));

	for (int32 i = 0; i < Statement.RHS.Num(); ++i)
	{
		FBPTerminal* CurrentTerminal = Statement.RHS[i];
		EmitterContext.AddLine(FString::Printf(TEXT("%s[%d] = %s;"), *Array, i, *TermToText(EmitterContext, CurrentTerminal, ENativizedTermUsage::Getter)));
	}
}

void FBlueprintCompilerCppBackend::EmitCreateSetStatement(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	FBPTerminal* SetTerm = Statement.LHS;
	const FString Set = TermToText(EmitterContext, SetTerm, ENativizedTermUsage::UnspecifiedOrReference);

	EmitterContext.AddLine(FString::Printf(TEXT("%s.Reserve(%d);"), *Set, Statement.RHS.Num()));

	for (int32 i = 0; i < Statement.RHS.Num(); ++i)
	{
		FBPTerminal* CurrentTerminal = Statement.RHS[i];
		EmitterContext.AddLine(FString::Printf(TEXT("%s.Add( %s );"), *Set, *TermToText(EmitterContext, CurrentTerminal, ENativizedTermUsage::Getter)));
	}
}

void FBlueprintCompilerCppBackend::EmitCreateMapStatement(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	FBPTerminal* MapTerm = Statement.LHS;
	const FString Map = TermToText(EmitterContext, MapTerm, ENativizedTermUsage::UnspecifiedOrReference);

	check((Statement.RHS.Num() % 2) == 0);
	EmitterContext.AddLine(FString::Printf(TEXT("%s.Reserve(%d);"), *Map, Statement.RHS.Num() / 2));

	for (int32 i = 0; i < Statement.RHS.Num(); i+=2)
	{
		FBPTerminal* KeyTerminal = Statement.RHS[i];
		FBPTerminal* ValueTerminal = Statement.RHS[i+1];
		EmitterContext.AddLine(FString::Printf(TEXT("%s.Add( %s, %s );"), *Map, *TermToText(EmitterContext, KeyTerminal, ENativizedTermUsage::Getter), *TermToText(EmitterContext, ValueTerminal, ENativizedTermUsage::Getter)));
	}
}


void FBlueprintCompilerCppBackend::EmitGotoStatement(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	if (Statement.Type == KCST_ComputedGoto)
	{
		if (bUseGotoState)
		{
			FString NextStateExpression;
			NextStateExpression = TermToText(EmitterContext, Statement.LHS, ENativizedTermUsage::Getter);

			EmitterContext.AddLine(FString::Printf(TEXT("__CurrentState = %s;"), *NextStateExpression));
			EmitterContext.AddLine(FString::Printf(TEXT("break;\n")));
		}
	}
	else if ((Statement.Type == KCST_GotoIfNot) || (Statement.Type == KCST_EndOfThreadIfNot) || (Statement.Type == KCST_GotoReturnIfNot))
	{
		FString ConditionExpression;
		ConditionExpression = TermToText(EmitterContext, Statement.LHS, ENativizedTermUsage::Getter);

		EmitterContext.AddLine(FString::Printf(TEXT("if (!%s)"), *ConditionExpression));
		EmitterContext.AddLine(FString::Printf(TEXT("{")));
		EmitterContext.IncreaseIndent();
		if (Statement.Type == KCST_EndOfThreadIfNot)
		{
			if (bUseFlowStack)
			{
				EmitterContext.AddLine(TEXT("__CurrentState = (__StateStack.Num() > 0) ? __StateStack.Pop(/*bAllowShrinking=*/ false) : -1;"));
			}
			else if (bUseGotoState)
			{
				EmitterContext.AddLine(TEXT("__CurrentState = -1;"));
			}
			else
			{
				// is it needed?
				EmitterContext.AddLine(TEXT("return; //KCST_EndOfThreadIfNot"));
			}
		}
		else if (Statement.Type == KCST_GotoReturnIfNot)
		{
			if (bUseGotoState)
			{
				EmitterContext.AddLine(TEXT("__CurrentState = -1;"));
			}
			else
			{
				// is it needed?
				EmitterContext.AddLine(TEXT("return; //KCST_GotoReturnIfNot"));
			}
		}
		else
		{
			ensureMsgf(bUseGotoState, TEXT("KCST_GotoIfNot requires bUseGotoState == true class: %s"), *GetPathNameSafe(EmitterContext.GetCurrentlyGeneratedClass()));
			EmitterContext.AddLine(FString::Printf(TEXT("__CurrentState = %d;"), StatementToStateIndex(FunctionContext, Statement.TargetLabel)));
		}

		if (bUseGotoState)
		{
			EmitterContext.AddLine(FString::Printf(TEXT("break;")));
		}
		EmitterContext.DecreaseIndent();
		EmitterContext.AddLine(FString::Printf(TEXT("}")));
	}
	else if (Statement.Type == KCST_GotoReturn)
	{
		if (bUseGotoState)
		{
			EmitterContext.AddLine(TEXT("__CurrentState = -1;"));
			EmitterContext.AddLine(FString::Printf(TEXT("break;")));
		}
		else
		{
			EmitterContext.AddLine(TEXT("return; // KCST_GotoReturn"));
		}
	}
	else if (Statement.Type == KCST_UnconditionalGoto)
	{
		if (bUseGotoState)
		{
			EmitterContext.AddLine(FString::Printf(TEXT("__CurrentState = %d;"), StatementToStateIndex(FunctionContext, Statement.TargetLabel)));
			EmitterContext.AddLine(FString::Printf(TEXT("break;")));
		}
		else
		{
			EmitterContext.AddLine(FString::Printf(TEXT("// optimized KCST_UnconditionalGoto")));
		}
	}
	else
	{
		check(false);
	}
}

void FBlueprintCompilerCppBackend::EmitPushStateStatement(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext, FBlueprintCompiledStatement& Statement)
{
	ensure(bUseFlowStack);
	EmitterContext.AddLine(FString::Printf(TEXT("__StateStack.Push(%d);"), StatementToStateIndex(FunctionContext, Statement.TargetLabel)));
}

void FBlueprintCompilerCppBackend::EmitEndOfThreadStatement(FEmitterLocalContext& EmitterContext, FKismetFunctionContext& FunctionContext)
{
	if (bUseFlowStack)
	{
		EmitterContext.AddLine(TEXT("__CurrentState = (__StateStack.Num() > 0) ? __StateStack.Pop(/*bAllowShrinking=*/ false) : -1;"));
		EmitterContext.AddLine(TEXT("break;"));
	}
	else if (bUseGotoState)
	{
		EmitterContext.AddLine(TEXT("__CurrentState = -1;"));
		EmitterContext.AddLine(TEXT("break;"));
	}
	else
	{
		// is it needed?
		EmitterContext.AddLine(TEXT("return; //KCST_EndOfThread"));
	}
}

FString FBlueprintCompilerCppBackend::EmitSwitchValueStatmentInner(FEmitterLocalContext& EmitterContext, FBlueprintCompiledStatement& Statement)
{
	check(Statement.RHS.Num() >= 2);
	const int32 TermsBeforeCases = 1;
	const int32 TermsPerCase = 2;
	const int32 NumCases = ((Statement.RHS.Num() - 2) / TermsPerCase);
	auto IndexTerm = Statement.RHS[0];
	auto DefaultValueTerm = Statement.RHS.Last();

	const uint32 CppTemplateTypeFlags = EPropertyExportCPPFlags::CPPF_CustomTypeName
		| EPropertyExportCPPFlags::CPPF_NoConst | EPropertyExportCPPFlags::CPPF_NoRef
		| EPropertyExportCPPFlags::CPPF_BlueprintCppBackend;

	check(IndexTerm && IndexTerm->AssociatedVarProperty);
	const FString IndexDeclaration = EmitterContext.ExportCppDeclaration(IndexTerm->AssociatedVarProperty, EExportedDeclaration::Local, CppTemplateTypeFlags, FEmitterLocalContext::EPropertyNameInDeclaration::Skip);

	check(DefaultValueTerm && DefaultValueTerm->AssociatedVarProperty);
	const FString ValueDeclaration = EmitterContext.ExportCppDeclaration(DefaultValueTerm->AssociatedVarProperty, EExportedDeclaration::Local, CppTemplateTypeFlags, FEmitterLocalContext::EPropertyNameInDeclaration::Skip);

	FString Result = FString::Printf(TEXT("TSwitchValue<%s, %s>(%s, %s, %d")
		, *IndexDeclaration
		, *ValueDeclaration
		, *TermToText(EmitterContext, IndexTerm, ENativizedTermUsage::UnspecifiedOrReference) //index
		, *TermToText(EmitterContext, DefaultValueTerm, ENativizedTermUsage::UnspecifiedOrReference) // default
		, NumCases);

	for (int32 TermIndex = TermsBeforeCases; TermIndex < (NumCases * TermsPerCase); TermIndex += TermsPerCase)
	{
		auto TermToRef = [&](const FBPTerminal* Term) -> FString
		{
			const UEdGraphSchema_K2* Schema = GetDefault<UEdGraphSchema_K2>();
			check(Schema);

			FString BeginCast;
			FString EndCast;
			FEdGraphPinType LType;
			if (Schema->ConvertPropertyToPinType(DefaultValueTerm->AssociatedVarProperty, LType))
			{
				FEmitHelper::GenerateAutomaticCast(EmitterContext, LType, Term->Type, DefaultValueTerm->AssociatedVarProperty, Term->AssociatedVarProperty, BeginCast, EndCast, true);
			}

			const FString TermEvaluation = TermToText(EmitterContext, Term, ENativizedTermUsage::UnspecifiedOrReference); //should bGetter be false ?
			const FString CastedTerm = FString::Printf(TEXT("%s%s%s"), *BeginCast, *TermEvaluation, *EndCast);
			if (Term->bIsLiteral) //TODO it should be done for every term, that cannot be handled as reference.
			{
				const FString LocalVarName = EmitterContext.GenerateUniqueLocalName();
				EmitterContext.AddLine(FString::Printf(TEXT("%s %s = %s;"), *ValueDeclaration, *LocalVarName, *CastedTerm));
				return LocalVarName;
			}
			return CastedTerm;
		};

		const FString Term0_Index = TermToText(EmitterContext, Statement.RHS[TermIndex], ENativizedTermUsage::UnspecifiedOrReference);
		const FString Term1_Ref = TermToRef(Statement.RHS[TermIndex + 1]);

		Result += FString::Printf(TEXT(", TSwitchPair<%s, %s>(%s, %s)")
			, *IndexDeclaration
			, *ValueDeclaration
			, *Term0_Index
			, *Term1_Ref);
	}

	Result += TEXT(")");

	return Result;
}

struct FCastWildCard
{
	TArray<FString> TypeDependentPinNames;
	int32 ArrayParamIndex = -1;
	const FBlueprintCompiledStatement& Statement;

	FCastWildCard(const FBlueprintCompiledStatement& InStatement) : ArrayParamIndex(-1), Statement(InStatement)
	{
		const FString& DependentPinMetaData = Statement.FunctionToCall->GetMetaData(FBlueprintMetadata::MD_ArrayDependentParam);
		DependentPinMetaData.ParseIntoArray(TypeDependentPinNames, TEXT(","), true);

		const FString& ArrayPointerMetaData = Statement.FunctionToCall->GetMetaData(FBlueprintMetadata::MD_ArrayParam);
		TArray<FString> ArrayPinComboNames;
		ArrayPointerMetaData.ParseIntoArray(ArrayPinComboNames, TEXT(","), true);

		int32 LocNumParams = 0;
		if (ArrayPinComboNames.Num() == 1)
		{
			for (TFieldIterator<UProperty> PropIt(Statement.FunctionToCall); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
			{
				if (!PropIt->HasAnyPropertyFlags(CPF_ReturnParm))
				{
					if (PropIt->GetName() == ArrayPinComboNames[0])
					{
						ArrayParamIndex = LocNumParams;
						break;
					}
					LocNumParams++;
				}
			}
		}
	}

	bool FillWildcardType(const UProperty* FuncParamProperty, FEdGraphPinType& LType)
	{
		if ((FuncParamProperty->HasAnyPropertyFlags(CPF_ConstParm) || !FuncParamProperty->HasAnyPropertyFlags(CPF_OutParm)) // it's pointless(?) and unsafe(?) to cast Output parameter
			&& (ArrayParamIndex >= 0)
			&& ((LType.PinCategory == UEdGraphSchema_K2::PC_Wildcard) || (LType.PinCategory == UEdGraphSchema_K2::PC_Int))
			&& TypeDependentPinNames.Contains(FuncParamProperty->GetName()))
		{
			FBPTerminal* ArrayTerm = Statement.RHS[ArrayParamIndex];
			check(ArrayTerm);
			LType.PinCategory = ArrayTerm->Type.PinCategory;
			LType.PinSubCategory = ArrayTerm->Type.PinSubCategory;
			LType.PinSubCategoryObject = ArrayTerm->Type.PinSubCategoryObject;
			LType.PinSubCategoryMemberReference = ArrayTerm->Type.PinSubCategoryMemberReference;
			return true;
		}
		return false;
	}
};

FString FBlueprintCompilerCppBackend::EmitMethodInputParameterList(FEmitterLocalContext& EmitterContext, FBlueprintCompiledStatement& Statement)
{
	FCastWildCard CastWildCard(Statement);

	FString Result;
	int32 NumParams = 0;

	for (TFieldIterator<UProperty> PropIt(Statement.FunctionToCall); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
	{
		UProperty* FuncParamProperty = *PropIt;

		if (!FuncParamProperty->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			if (NumParams > 0)
			{
				Result += TEXT(", ");
			}

			FString VarName;

			FBPTerminal* Term = Statement.RHS[NumParams];
			check(Term != nullptr);

			if ((Statement.TargetLabel != nullptr) && (Statement.UbergraphCallIndex == NumParams))
			{
				// The target label will only ever be set on a call function when calling into the Ubergraph or
				// on a latent function that will later call into the ubergraph, either of which requires a patchup
				UStructProperty* StructProp = Cast<UStructProperty>(FuncParamProperty);
				if (StructProp && (StructProp->Struct == FLatentActionInfo::StaticStruct()))
				{
					// Latent function info case
					VarName = LatentFunctionInfoTermToText(EmitterContext, Term, Statement.TargetLabel);
				}
				else
				{
					// Ubergraph entry point case
					VarName = FString::FromInt(StateMapPerFunction[0].StatementToStateIndex(Statement.TargetLabel));
				}
			}
			else
			{
				// Emit a normal parameter term
				FString BeginCast;
				FString CloseCast;
				FEdGraphPinType LType;
				auto Schema = GetDefault<UEdGraphSchema_K2>();
				check(Schema);
				ENativizedTermUsage TermUsage = ENativizedTermUsage::UnspecifiedOrReference;
				if (Schema->ConvertPropertyToPinType(FuncParamProperty, LType))
				{
					CastWildCard.FillWildcardType(FuncParamProperty, LType);

					FEmitHelper::GenerateAutomaticCast(EmitterContext, LType, Term->Type, FuncParamProperty, Term->AssociatedVarProperty, BeginCast, CloseCast);
					TermUsage = LType.bIsReference ? ENativizedTermUsage::UnspecifiedOrReference : ENativizedTermUsage::Getter;
				}
				VarName += BeginCast;
				VarName += TermToText(EmitterContext, Term, TermUsage);
				VarName += CloseCast;
			}

			if (FuncParamProperty->HasAnyPropertyFlags(CPF_OutParm) && !FuncParamProperty->HasAnyPropertyFlags(CPF_ConstParm))
			{
				Result += TEXT("/*out*/ ");
			}
			Result += *VarName;

			NumParams++;
		}
	}

	return Result;
}

static FString CustomThunkFunctionPostfix(FBlueprintCompiledStatement& Statement)
{
	/* Some native structures have no operator==. There are special versions of array functions for them (see GeneratedCodeHelpers.h). */

	check(Statement.FunctionToCall);

	int32 NumParams = 0;
	FBPTerminal* ArrayTerm = nullptr;
	for (TFieldIterator<UProperty> PropIt(Statement.FunctionToCall); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
	{
		UProperty* FuncParamProperty = *PropIt;
		if (!FuncParamProperty->HasAnyPropertyFlags(CPF_ReturnParm))
		{
			if (UArrayProperty* ArrayProperty = Cast<UArrayProperty>(*PropIt))
			{
				ArrayTerm = Statement.RHS[NumParams];
				ensure(ArrayTerm && ArrayTerm->Type.IsArray());
				break;
			}
			NumParams++;
		}
	}

	const FName FunctionName = Statement.FunctionToCall->GetFName();
	if (ArrayTerm 
		&& ((FunctionName == GET_FUNCTION_NAME_CHECKED(UKismetArrayLibrary, Array_Find))
			|| (FunctionName == GET_FUNCTION_NAME_CHECKED(UKismetArrayLibrary, Array_Contains))
			|| (FunctionName == GET_FUNCTION_NAME_CHECKED(UKismetArrayLibrary, Array_RemoveItem))
			|| (FunctionName == GET_FUNCTION_NAME_CHECKED(UKismetArrayLibrary, Array_AddUnique))
			))
	{
		if (UEdGraphSchema_K2::PC_Text == ArrayTerm->Type.PinCategory)
		{
			return FString(TEXT("_FText"));
		}

		if (UEdGraphSchema_K2::PC_Struct == ArrayTerm->Type.PinCategory)
		{
			UScriptStruct* Struct = Cast<UScriptStruct>(ArrayTerm->Type.PinSubCategoryObject.Get());
			if (Struct && Struct->IsNative() && (0 == (Struct->StructFlags & STRUCT_NoExport)))
			{
				return FString(TEXT("_Struct"));
			}
		}
	}
	return FString();
}

FString FBlueprintCompilerCppBackend::EmitCallStatmentInner(FEmitterLocalContext& EmitterContext, FBlueprintCompiledStatement& Statement, bool bInline, FString PostFix)
{
	check(Statement.FunctionToCall != nullptr);

	const bool bCallOnDifferentObject = Statement.FunctionContext && (Statement.FunctionContext->Name != TEXT("self"));
	const bool bStaticCall = Statement.FunctionToCall->HasAnyFunctionFlags(FUNC_Static);
	const bool bUseSafeContext = bCallOnDifferentObject && !bStaticCall;
	const bool bAnyInterfaceCall = bCallOnDifferentObject && Statement.FunctionContext && (Statement.bIsInterfaceContext || UEdGraphSchema_K2::PC_Interface == Statement.FunctionContext->Type.PinCategory);
	const bool bInterfaceCallExecute = bAnyInterfaceCall && Statement.FunctionToCall->HasAnyFunctionFlags(FUNC_Event | FUNC_BlueprintEvent);
	const bool bNativeEvent = FEmitHelper::ShouldHandleAsNativeEvent(Statement.FunctionToCall, false);
	const bool bNetRPC = !bAnyInterfaceCall && Statement.FunctionToCall->HasAllFunctionFlags(FUNC_Net) && !Statement.FunctionToCall->HasAnyFunctionFlags(FUNC_NetResponse);

	const UClass* CurrentClass = EmitterContext.GetCurrentlyGeneratedClass();
	const UClass* SuperClass = CurrentClass ? CurrentClass->GetSuperClass() : nullptr;
	const UClass* OriginalSuperClass = SuperClass ? EmitterContext.Dependencies.FindOriginalClass(SuperClass) : nullptr;
	const UFunction* ActualParentFunction = (Statement.bIsParentContext && OriginalSuperClass) ? OriginalSuperClass->FindFunctionByName(Statement.FunctionToCall->GetFName(), EIncludeSuperFlag::IncludeSuper) : nullptr;
	// if(Statement.bIsParentContext && bNativeEvent) then name is constructed from original function with "_Implementation postfix
	const FString FunctionToCallOriginalName = FEmitHelper::GetCppName((ActualParentFunction && !bNativeEvent && !bNetRPC) ? ActualParentFunction : FEmitHelper::GetOriginalFunction(Statement.FunctionToCall)) + PostFix;
	const bool bIsFunctionValidToCallFromBP = !ActualParentFunction || ActualParentFunction->HasAnyFunctionFlags(FUNC_Native) || (ActualParentFunction->Script.Num() > 0);

	if (!bIsFunctionValidToCallFromBP)
	{
		return TEXT("/*This function cannot be called from BP. See bIsValidFunction in UObject::CallFunction*/");
	}

	if (Statement.FunctionToCall->HasAllFunctionFlags(FUNC_Native))
	{
		// Cloned logic from: FScriptBytecodeWriter::EmitFunctionCall
		// Array output parameters are cleared, in case the native function doesn't clear them before filling.
		int32 NumParams = 0;
		for (TFieldIterator<UProperty> PropIt(Statement.FunctionToCall); PropIt && (PropIt->PropertyFlags & CPF_Parm); ++PropIt)
		{
			UProperty* Param = *PropIt;
			if (ensure(Param) && !Param->HasAnyPropertyFlags(CPF_ReturnParm))
			{
				const bool bShouldParameterBeCleared = Param->IsA<UArrayProperty>()
					&& Param->HasAllPropertyFlags(CPF_Parm | CPF_OutParm)
					&& !Param->HasAnyPropertyFlags(CPF_ReferenceParm | CPF_ConstParm | CPF_ReturnParm);
				if (bShouldParameterBeCleared)
				{
					FBPTerminal* Term = Statement.RHS[NumParams];
					const FString TermStr = ensure(Term != nullptr) ? TermToText(EmitterContext, Term, ENativizedTermUsage::UnspecifiedOrReference) : FString();
					EmitterContext.AddLine(FString::Printf(TEXT("(%s).Reset();"), *TermStr));
				}
				NumParams++;
			}
		}
	}

	FString Result;
	FString CloseCast;
	TUniquePtr<FSetterExpressionBuilder> SetterExpression;
	if (!bInline)
	{
		// Handle the return value of the function being called
		UProperty* FuncToCallReturnProperty = Statement.FunctionToCall->GetReturnProperty();
		if (FuncToCallReturnProperty && ensure(Statement.LHS))
		{
			SetterExpression = MakeUnique<FSetterExpressionBuilder>(*this, EmitterContext, Statement.LHS);
			Result += SetterExpression->BuildStart();

			FString BeginCast;
			FEdGraphPinType RType;
			auto Schema = GetDefault<UEdGraphSchema_K2>();
			check(Schema);
			if (Schema->ConvertPropertyToPinType(FuncToCallReturnProperty, RType))
			{
				FEmitHelper::GenerateAutomaticCast(EmitterContext, Statement.LHS->Type, RType, Statement.LHS->AssociatedVarProperty, FuncToCallReturnProperty, BeginCast, CloseCast);
			}
			Result += BeginCast;
		}
	}

	FNativizationSummaryHelper::FunctionUsed(CurrentClass, Statement.FunctionToCall);

	UClass* FunctionOwner = Statement.FunctionToCall->GetOwnerClass();
	// Emit object to call the method on
	if (bInterfaceCallExecute)
	{
		UClass* ContextInterfaceClass = CastChecked<UClass>(Statement.FunctionContext->Type.PinSubCategoryObject.Get());
		const bool bInputIsInterface = ContextInterfaceClass->IsChildOf<UInterface>();

		FString ExecuteFormat = TEXT("%s::Execute_%s(%s ");
		if (bInputIsInterface)
		{
			ExecuteFormat.InsertAt(ExecuteFormat.Len()-1, TEXT(".GetObject()"));
		}
		else
		{
			ContextInterfaceClass = FunctionOwner;
			ensure(ContextInterfaceClass->IsChildOf<UInterface>());
		}		
		
		Result += FString::Printf(*ExecuteFormat
			, *FEmitHelper::GetCppName(ContextInterfaceClass)
			, *FunctionToCallOriginalName
			, *TermToText(EmitterContext, Statement.FunctionContext, ENativizedTermUsage::Getter, false));
	}
	else
	{
		auto OwnerBPGC = Cast<UBlueprintGeneratedClass>(FunctionOwner);
		const bool bUnconvertedClass = OwnerBPGC && !EmitterContext.Dependencies.WillClassBeConverted(OwnerBPGC);
		const bool bIsCustomThunk = bStaticCall && ( Statement.FunctionToCall->GetBoolMetaData(TEXT("CustomThunk"))
			|| Statement.FunctionToCall->HasMetaData(TEXT("CustomStructureParam"))
			|| Statement.FunctionToCall->HasMetaData(TEXT("ArrayParm")) );
		if (bUnconvertedClass)
		{
			ensure(!Statement.bIsParentContext); //unsupported yet
			ensure(bCallOnDifferentObject); //unexpected
			const FString WrapperName = FString::Printf(TEXT("FUnconvertedWrapper__%s"), *FEmitHelper::GetCppName(OwnerBPGC));
			EmitterContext.MarkUnconvertedClassAsNecessary(OwnerBPGC);
			const FString CalledObject = bCallOnDifferentObject ? TermToText(EmitterContext, Statement.FunctionContext, ENativizedTermUsage::UnspecifiedOrReference, false) : TEXT("this");
			Result += FString::Printf(TEXT("%s(%s)."), *WrapperName, *CalledObject);
		}
		else if (bStaticCall)
		{
			auto OwnerClass = Statement.FunctionToCall->GetOuterUClass();
			Result += bIsCustomThunk ? TEXT("FCustomThunkTemplates::") : FString::Printf(TEXT("%s::"), *FEmitHelper::GetCppName(OwnerClass));
		}
		else if (bCallOnDifferentObject) //@TODO: Badness, could be a self reference wired to another instance!
		{
			Result += FString::Printf(TEXT("%s->"), *TermToText(EmitterContext, Statement.FunctionContext, ENativizedTermUsage::Getter, false));
		}

		if (Statement.bIsParentContext)
		{
			Result += TEXT("Super::");
		}
		else if (!bUnconvertedClass && !bStaticCall && FunctionOwner && !OwnerBPGC && Statement.FunctionToCall->HasAnyFunctionFlags(FUNC_Final))
		{
			Result += FString::Printf(TEXT("%s::"), *FEmitHelper::GetCppName(FunctionOwner));
		}
		Result += FunctionToCallOriginalName;

		if (bIsCustomThunk)
		{
			Result += CustomThunkFunctionPostfix(Statement);
		}

		if ((Statement.bIsParentContext || Statement.bIsInterfaceContext) && (bNativeEvent || bNetRPC))
		{
			ensure(!bCallOnDifferentObject);
			Result += TEXT("_Implementation");
		}

		// Emit method parameter list
		Result += TEXT("(");
	}
	const FString ParameterList = EmitMethodInputParameterList(EmitterContext, Statement);
	if (bInterfaceCallExecute && !ParameterList.IsEmpty())
	{
		Result += TEXT(", ");
	}
	Result += ParameterList;
	Result += TEXT(")");

	Result += CloseCast;
	if (SetterExpression)
	{
		Result += SetterExpression->BuildEnd(false);
	}
	if (!bInline)
	{
		Result += TEXT(";");
	}

	return Result;
}

FString FBlueprintCompilerCppBackend::EmitArrayGetByRef(FEmitterLocalContext& EmitterContext, FBlueprintCompiledStatement& Statement)
{
	check(Statement.RHS.Num() == 2);

	FString Result;
	Result += TermToText(EmitterContext, Statement.RHS[0], ENativizedTermUsage::UnspecifiedOrReference);
	Result += TEXT("[");
	Result += TermToText(EmitterContext, Statement.RHS[1], ENativizedTermUsage::Getter);
	Result += TEXT("]");
	return Result;
}

FString FBlueprintCompilerCppBackend::TermToText(FEmitterLocalContext& EmitterContext, const FBPTerminal* Term, const ENativizedTermUsage TermUsage, const bool bUseSafeContext, FString* EndCustomSetExpression)
{
	ensure((TermUsage != ENativizedTermUsage::Setter) || !bUseSafeContext);
	ensure((TermUsage == ENativizedTermUsage::Setter) == (EndCustomSetExpression != nullptr));
	if (EndCustomSetExpression)
	{
		EndCustomSetExpression->Reset();
	}

	const bool bGetter = (TermUsage == ENativizedTermUsage::Getter);
	const FString PSC_Self(TEXT("self"));
	if (Term->bIsLiteral)
	{
		return FEmitHelper::LiteralTerm(EmitterContext, Term->Type, Term->Name, Term->ObjectLiteral, &Term->TextLiteral);
	}
	else if (Term->InlineGeneratedParameter)
	{
		if (KCST_SwitchValue == Term->InlineGeneratedParameter->Type)
		{
			return EmitSwitchValueStatmentInner(EmitterContext, *Term->InlineGeneratedParameter);
		}
		else if (KCST_CallFunction == Term->InlineGeneratedParameter->Type)
		{
			return EmitCallStatmentInner(EmitterContext, *Term->InlineGeneratedParameter, true, FString());
		}
		else if (KCST_ArrayGetByRef == Term->InlineGeneratedParameter->Type)
		{
			return EmitArrayGetByRef(EmitterContext, *Term->InlineGeneratedParameter);
		}
		else
		{
			ensureMsgf(false, TEXT("KCST %d is not accepted as inline statement."), Term->InlineGeneratedParameter->Type);
			return FString();
		}
	}
	else
	{
		auto GenerateDefaultLocalVariable = [&](const FBPTerminal* InTerm) -> FString
		{
			const FString DefaultValueVariable = EmitterContext.GenerateUniqueLocalName();
			const uint32 PropertyExportFlags = EPropertyExportCPPFlags::CPPF_CustomTypeName | EPropertyExportCPPFlags::CPPF_BlueprintCppBackend | EPropertyExportCPPFlags::CPPF_NoConst;
			const FString CppType = Term->AssociatedVarProperty
				? EmitterContext.ExportCppDeclaration(Term->AssociatedVarProperty, EExportedDeclaration::Local, PropertyExportFlags, FEmitterLocalContext::EPropertyNameInDeclaration::Skip)
				: FEmitHelper::PinTypeToNativeType(Term->Type);

			const FString DefaultValueConstructor = (!Term->Type.IsContainer())
				? FEmitHelper::LiteralTerm(EmitterContext, Term->Type, FString(), nullptr, &FText::GetEmpty())
				: FString::Printf(TEXT("%s{}"), *CppType);

			EmitterContext.AddLine(*FString::Printf(TEXT("%s %s = %s;"), *CppType, *DefaultValueVariable, *DefaultValueConstructor));

			return DefaultValueVariable;
		};

		if (Term->AssociatedVarProperty && Term->AssociatedVarProperty->HasAnyPropertyFlags(CPF_EditorOnly))
		{
			UE_LOG(LogK2Compiler, Warning, TEXT("C++ backend cannot cannot use EditorOnly property: %s"), *GetPathNameSafe(Term->AssociatedVarProperty));
			EmitterContext.AddLine(*FString::Printf(TEXT("// EDITOR-ONLY Variable: %s"), *FEmitHelper::GetCppName(Term->AssociatedVarProperty)));
			const FString DefaultValueVariable = GenerateDefaultLocalVariable(Term);
			return DefaultValueVariable;
		}

		FString ContextStr;
		if ((Term->Context != nullptr) && (Term->Context->Name != PSC_Self))
		{
			ensure(Term->AssociatedVarProperty);
			const bool bFromDefaultValue = Term->Context->IsClassContextType();
			if (bFromDefaultValue)
			{
				UClass* MinimalClass = Term->AssociatedVarProperty 
					? Term->AssociatedVarProperty->GetOwnerClass()
					: Cast<UClass>(Term->Context->Type.PinSubCategoryObject.Get());
				if (MinimalClass)
				{
					MinimalClass = EmitterContext.GetFirstNativeOrConvertedClass(MinimalClass);
					ContextStr += FString::Printf(TEXT("GetDefaultValueSafe<%s>(")
						, *FEmitHelper::GetCppName(MinimalClass));
				}
				else
				{
					UE_LOG(LogK2Compiler, Error, TEXT("C++ backend cannot find specific class"));
				}
			}

			ContextStr += TermToText(EmitterContext, Term->Context, ENativizedTermUsage::UnspecifiedOrReference, false); // Should we just pass TermUsage?
			ContextStr += bFromDefaultValue ? TEXT(")") : TEXT("");
		}

		FString ResultPath;
		const bool bNativeConst = Term->AssociatedVarProperty && Term->AssociatedVarProperty->HasMetaData(FName(TEXT("NativeConst")));
		bool bIsAccessible = bGetter || !bNativeConst;
		if (Term->Context && Term->Context->IsStructContextType())
		{
			check(Term->AssociatedVarProperty);
			bIsAccessible &= !Term->AssociatedVarProperty->HasAnyPropertyFlags(CPF_NativeAccessSpecifierPrivate | CPF_NativeAccessSpecifierProtected);
			if (!bIsAccessible)
			{
				ResultPath = FEmitHelper::AccessInaccessibleProperty(EmitterContext, Term->AssociatedVarProperty, FString(), ContextStr, TEXT("&"), 0, TermUsage, EndCustomSetExpression);
			}
			else
			{
				ResultPath = ContextStr + TEXT(".") + FEmitHelper::GetCppName(Term->AssociatedVarProperty);
			}
		}
		else if (Term->AssociatedVarProperty)
		{
			FNativizationSummaryHelper::PropertyUsed(EmitterContext.GetCurrentlyGeneratedClass(), Term->AssociatedVarProperty);

			const bool bSelfContext = (!Term->Context) || (Term->Context->Name == PSC_Self);
			const bool bPropertyOfParent = EmitterContext.Dependencies.GetActualStruct()->IsChildOf(Term->AssociatedVarProperty->GetOwnerStruct());
			bIsAccessible &= !Term->AssociatedVarProperty->HasAnyPropertyFlags(CPF_NativeAccessSpecifierPrivate)
				&& ((bPropertyOfParent && bSelfContext) || !Term->AssociatedVarProperty->HasAnyPropertyFlags(CPF_NativeAccessSpecifierProtected));

			auto MinimalClass = Term->AssociatedVarProperty->GetOwnerClass();
			auto MinimalBPGC = Cast<UBlueprintGeneratedClass>(MinimalClass);
			if (MinimalBPGC && !EmitterContext.Dependencies.WillClassBeConverted(MinimalBPGC))
			{
				if (bSelfContext)
				{
					ensure(ContextStr.IsEmpty());
					ContextStr = TEXT("this");
				}

				ResultPath = FString::Printf(TEXT("FUnconvertedWrapper__%s(%s).GetRef__%s()")
					, *FEmitHelper::GetCppName(MinimalBPGC)
					, *ContextStr
					, *UnicodeToCPPIdentifier(Term->AssociatedVarProperty->GetName(), false, nullptr));
				EmitterContext.MarkUnconvertedClassAsNecessary(MinimalBPGC);
			}
			else if (!bIsAccessible)
			{
				if (bSelfContext)
				{
					ensure(ContextStr.IsEmpty());
					ContextStr = TEXT("this");
				}
				ResultPath = FEmitHelper::AccessInaccessibleProperty(EmitterContext, Term->AssociatedVarProperty, FString(), ContextStr, FString(), 0, TermUsage, EndCustomSetExpression);
			}
			else
			{
				if (!bSelfContext)
				{
					ResultPath = ContextStr + TEXT("->");
				}

				ResultPath += FEmitHelper::GetCppName(Term->AssociatedVarProperty);

				// convert bitfield to bool...
				UBoolProperty* BoolProperty = Cast<UBoolProperty>(Term->AssociatedVarProperty);
				if (bGetter && BoolProperty && !BoolProperty->IsNativeBool())
				{
					//TODO: the result still cannot be used as reference
					ResultPath = FString::Printf(TEXT("(%s != 0)"), *ResultPath);
				}
			}
		}
		else
		{
			ensure(ContextStr.IsEmpty());
			ResultPath = Term->Name;
		}

		const bool bUseWeakPtrGetter = Term->Type.bIsWeakPointer && bGetter;
		const TCHAR* WeakPtrGetter = TEXT(".Get()");
		if (bUseWeakPtrGetter)
		{
			ResultPath += WeakPtrGetter;
		}

		const bool bNativeConstTemplateArg = Term->AssociatedVarProperty && Term->AssociatedVarProperty->HasMetaData(FName(TEXT("NativeConstTemplateArg")));
		if (Term->Type.IsArray() && bNativeConstTemplateArg && bIsAccessible && bGetter)
		{
			FEdGraphPinType InnerType = Term->Type;
			InnerType.ContainerType = EPinContainerType::None;
			InnerType.bIsConst = false;
			const FString CppType = FEmitHelper::PinTypeToNativeType(InnerType);
			ResultPath = FString::Printf(TEXT("TArrayCaster<const %s>(%s).Get<%s>()"), *CppType, *ResultPath, *CppType);
		}
		else if ((bNativeConst || bNativeConstTemplateArg) && bIsAccessible && bGetter) //Why only getters?
		{
			const FString CppType = FEmitHelper::PinTypeToNativeType(Term->Type);
			ResultPath = FString::Printf(TEXT("const_cast<%s>(%s)"), *CppType, *ResultPath);
		}

		const FString Conditions = bUseSafeContext ? FSafeContextScopedEmmitter::ValidationChain(EmitterContext, Term->Context, *this) : FString();
		if (!Conditions.IsEmpty())
		{
			const FString DefaultValueVariable = GenerateDefaultLocalVariable(Term);
			const FString DefaultExpression = bUseWeakPtrGetter ? (DefaultValueVariable + WeakPtrGetter) : DefaultValueVariable;
			return FString::Printf(TEXT("((%s) ? (%s) : (%s))"), *Conditions, *ResultPath, *DefaultExpression);
		}
		return ResultPath; 
	}
}

FString FBlueprintCompilerCppBackend::LatentFunctionInfoTermToText(FEmitterLocalContext& EmitterContext, FBPTerminal* Term, FBlueprintCompiledStatement* TargetLabel)
{
	auto LatentInfoStruct = FLatentActionInfo::StaticStruct();

	// Find the term name we need to fixup
	FString FixupTermName;
	for (UProperty* Prop = LatentInfoStruct->PropertyLink; Prop; Prop = Prop->PropertyLinkNext)
	{
		static const FName NeedsLatentFixup(TEXT("NeedsLatentFixup"));
		if (Prop->GetBoolMetaData(NeedsLatentFixup))
		{
			FixupTermName = Prop->GetName();
			break;
		}
	}

	check(!FixupTermName.IsEmpty());

	FString StructValues = Term->Name;

	// Index 0 is always the ubergraph
	const int32 TargetStateIndex = StateMapPerFunction[0].StatementToStateIndex(TargetLabel);
	const int32 LinkageTermStartIdx = StructValues.Find(FixupTermName);
	check(LinkageTermStartIdx != INDEX_NONE);
	StructValues = StructValues.Replace(TEXT("-1"), *FString::FromInt(TargetStateIndex));

	int32* ExecutionGroupPtr = UberGraphStatementToExecutionGroup.Find(TargetLabel);
	if (ExecutionGroupPtr && UberGraphContext)
	{
		const FString OldExecutionFunctionName = UEdGraphSchema_K2::FN_ExecuteUbergraphBase.ToString() + TEXT("_") + UberGraphContext->Blueprint->GetName();
		const FString NewExecutionFunctionName = OldExecutionFunctionName + FString::Printf(TEXT("_%d"), *ExecutionGroupPtr);
		StructValues = StructValues.Replace(*OldExecutionFunctionName, *NewExecutionFunctionName);
	}

	return FEmitHelper::LiteralTerm(EmitterContext, Term->Type, StructValues, nullptr);
}

bool FBlueprintCompilerCppBackend::InnerFunctionImplementation(FKismetFunctionContext& FunctionContext, FEmitterLocalContext& EmitterContext, int32 ExecutionGroup)
{
	EmitterContext.ResetPropertiesForInaccessibleStructs();

	bUseExecutionGroup = ExecutionGroup >= 0;
	ensure(FunctionContext.bIsUbergraph || !bUseExecutionGroup); // currently we split only ubergraphs

	auto DoesUseFlowStack = [&]() -> bool
	{
		for (UEdGraphNode* Node : FunctionContext.UnsortedSeparateExecutionGroups[ExecutionGroup])
		{
			TArray<FBlueprintCompiledStatement*>* StatementList = FunctionContext.StatementsPerNode.Find(Node);
			const bool bFlowStackIsRequired = StatementList && StatementList->ContainsByPredicate([](const FBlueprintCompiledStatement* Statement)->bool
			{
				return Statement && (Statement->Type == KCST_PushState);
			});
			if (bFlowStackIsRequired)
			{
				return true;
			}
		}
		return false;
	};
	bUseFlowStack =  bUseExecutionGroup ? DoesUseFlowStack() : FunctionContext.bUseFlowStack;

	UEdGraphNode* TheOnlyEntryPoint = nullptr;
	TArray<UEdGraphNode*> LocalLinearExecutionList;
	//TODO: unify ubergraph and function handling
	if (bUseExecutionGroup)
	{
		const bool bCanUseWithoutGotoState = PrepareToUseExecutionGroupWithoutGoto(FunctionContext, ExecutionGroup, TheOnlyEntryPoint);
		const bool bSortedWithoutCycles = bCanUseWithoutGotoState && SortNodesInUberGraphExecutionGroup(FunctionContext, TheOnlyEntryPoint, ExecutionGroup, LocalLinearExecutionList);
		bUseGotoState = !bSortedWithoutCycles;
	}
	else
	{
		bUseGotoState = FunctionContext.MustUseSwitchState(nullptr) || FunctionContext.bIsUbergraph;
	}
	ensureMsgf(!bUseFlowStack || bUseGotoState, TEXT("FBlueprintCompilerCppBackend::InnerFunctionImplementation - %s"), *GetPathNameSafe(FunctionContext.Function));
	TArray<UEdGraphNode*>* ActualLinearExecutionList = &FunctionContext.LinearExecutionList;
	if (bUseGotoState)
	{
		if (bUseFlowStack)
		{
			EmitterContext.AddLine(TEXT("TArray< int32, TInlineAllocator<8> > __StateStack;\n"));
		}
		if (FunctionContext.bIsUbergraph)
		{
			EmitterContext.AddLine(TEXT("int32 __CurrentState = bpp__EntryPoint__pf;"));
		}
		else
		{
			FBlueprintCompiledStatement* FirstStatement = nullptr;
			for (int32 NodeIndex = 0; (NodeIndex < FunctionContext.LinearExecutionList.Num()) && (!FirstStatement); ++NodeIndex)
			{
				UEdGraphNode* ItNode = FunctionContext.LinearExecutionList[NodeIndex];
				TArray<FBlueprintCompiledStatement*>* FirstStatementList = ItNode ? FunctionContext.StatementsPerNode.Find(ItNode) : nullptr;
				FirstStatement = (FirstStatementList && FirstStatementList->Num()) ? (*FirstStatementList)[0] : nullptr;
			}
			const int32 FirstIndex = FirstStatement ? StatementToStateIndex(FunctionContext, FirstStatement) : 0;
			EmitterContext.AddLine(FString::Printf(TEXT("int32 __CurrentState = %d;"), FirstIndex));
		}
		EmitterContext.AddLine(TEXT("do"));
		EmitterContext.AddLine(TEXT("{"));
		EmitterContext.IncreaseIndent();
		EmitterContext.AddLine(TEXT("switch( __CurrentState )"));
		EmitterContext.AddLine(TEXT("{"));
	}
	else if (FunctionContext.bIsUbergraph)
	{
		if (ensure(TheOnlyEntryPoint))
		{
			TArray<FBlueprintCompiledStatement*>* FirstStatementList = FunctionContext.StatementsPerNode.Find(TheOnlyEntryPoint);
			FBlueprintCompiledStatement* FirstStatement = (FirstStatementList && FirstStatementList->Num()) ? (*FirstStatementList)[0] : nullptr;
			const int32 UberGraphOnlyEntryPoint = ensure(FirstStatement) ? StatementToStateIndex(FunctionContext, FirstStatement) : -1;
			EmitterContext.AddLine(FString::Printf(TEXT("check(bpp__EntryPoint__pf == %d);"), UberGraphOnlyEntryPoint));
			ActualLinearExecutionList = &LocalLinearExecutionList;
		}
	}

	const bool bIsNotReducible = EmitAllStatements(FunctionContext, ExecutionGroup, EmitterContext, *ActualLinearExecutionList);

	if (bUseGotoState)
	{
		EmitterContext.DecreaseIndent();
		EmitterContext.AddLine(TEXT("}"));
		EmitterContext.DecreaseIndent();
		EmitterContext.AddLine(TEXT("default:"));
		EmitterContext.IncreaseIndent();
		if (bUseFlowStack)
		{
			EmitterContext.AddLine(TEXT("check(false); // Invalid state"));
		}
		EmitterContext.AddLine(TEXT("break;"));
		EmitterContext.DecreaseIndent();
		EmitterContext.AddLine(TEXT("}"));
		EmitterContext.DecreaseIndent();
		EmitterContext.AddLine(TEXT("} while( __CurrentState != -1 );"));
	}

	return bIsNotReducible;
}

bool FBlueprintCompilerCppBackend::SortNodesInUberGraphExecutionGroup(FKismetFunctionContext &FunctionContext, UEdGraphNode* TheOnlyEntryPoint, int32 ExecutionGroup, TArray<UEdGraphNode*> &LocalLinearExecutionList)
{	
	ensure(FunctionContext.LinearExecutionList.Contains(TheOnlyEntryPoint));

	TArray<int32> ExecutionIndiceQueue;
	int32 EntryIndiceIndex = INDEX_NONE;
	for (int32 NodeIndex = 0; NodeIndex < FunctionContext.LinearExecutionList.Num(); ++NodeIndex)
	{
		UEdGraphNode* Node = FunctionContext.LinearExecutionList[NodeIndex];
		if (FunctionContext.UnsortedSeparateExecutionGroups[ExecutionGroup].Contains(Node))
		{
			if (Node == TheOnlyEntryPoint)
			{
				EntryIndiceIndex = ExecutionIndiceQueue.Num();
			}
			ExecutionIndiceQueue.Add(NodeIndex);
		}
	}

	bool bFoundComputedGoto = false;
	bool bDetectedCyclicalLogic = false;
	for (int32 IndiceIndex = EntryIndiceIndex; IndiceIndex >= 0 && IndiceIndex < ExecutionIndiceQueue.Num() && !bDetectedCyclicalLogic; )
	{
		int32 NodeIndex = ExecutionIndiceQueue[IndiceIndex];
		// pop this from the execution queue (so we can detect if a separate statement requires a loop, jumping back to this one)
		ExecutionIndiceQueue.RemoveAt(IndiceIndex, /*Count =*/1, /*bAllowShrinking =*/false);

		UEdGraphNode* CurrentNode = FunctionContext.LinearExecutionList[NodeIndex];
		// here we're defining the (possibly new) execution order
		LocalLinearExecutionList.Push(CurrentNode);

		int32 NextIndiceIndex = INDEX_NONE;
		bool  bReturnExpected = false;

		TArray<FBlueprintCompiledStatement*>* StatementList = FunctionContext.StatementsPerNode.Find(CurrentNode);
		if (StatementList)
		{
			for (int32 StatementIndex = 0; StatementIndex < StatementList->Num() && !bDetectedCyclicalLogic; ++StatementIndex)
			{
				FBlueprintCompiledStatement& Statement = *((*StatementList)[StatementIndex]);
				switch (Statement.Type)
				{
				case KCST_ComputedGoto:
					{
						// sanity checking, that is all
						ensure(!bFoundComputedGoto);
						bFoundComputedGoto = true;
						ensure(CurrentNode == TheOnlyEntryPoint);
					}
					break;

				case KCST_UnconditionalGoto:
					{
						ensure(StatementIndex == (StatementList->Num() - 1)); // it should be the last statement generated from the node
						ensure(Statement.TargetLabel);

						int32 TargetIndiceIndex = 0;
						for ( ; TargetIndiceIndex < ExecutionIndiceQueue.Num(); ++TargetIndiceIndex)
						{
							int32 TargetNodeIndex = ExecutionIndiceQueue[TargetIndiceIndex];
							UEdGraphNode* RemainingNode = FunctionContext.LinearExecutionList[TargetNodeIndex];
							
							TArray<FBlueprintCompiledStatement*>* TargetStatementList = FunctionContext.StatementsPerNode.Find(RemainingNode);
							// check and see if the statement we're supposed to jump to is contained within this node
							if (TargetStatementList && TargetStatementList->Contains(Statement.TargetLabel))
							{
								break;
							}
						}
						NextIndiceIndex = TargetIndiceIndex;

						// if we couldn't find the target node (it was likely already processed - implying cyclical logic)
						if (TargetIndiceIndex >= ExecutionIndiceQueue.Num())
						{
							bDetectedCyclicalLogic = true;
						}
					}
					break;

				case KCST_GotoReturn:
				case KCST_EndOfThread:
					{
						ensure(StatementIndex == (StatementList->Num() - 1)); // it should be the last statement generated from the node
						bReturnExpected = true;
					}
					break;

				default:
					break;
				};
			}
		}

		// if there was no goto statement, then we expect the statement to fall through to the next
		if (NextIndiceIndex == INDEX_NONE)
		{
			// the index remains the same, because we popped the current one out of the queue
			NextIndiceIndex = IndiceIndex;
			if (NextIndiceIndex >= ExecutionIndiceQueue.Num())
			{
				NextIndiceIndex = 0;
			}

			if (ExecutionIndiceQueue.Num() == 0 && !bReturnExpected)
			{
				// we've popped a node out of the queue that we were supposed to 
				// fall through to (implying cyclical logic)
				bDetectedCyclicalLogic = true;
			}
			// since we're falling through to the next node, we expect that node 
			// to be what was directly next in the source LinearExecutionList, 
			// if not, we can assume something pulled it out of order (implying cyclical logic)
			else if (ExecutionIndiceQueue.Num() > 0 && ExecutionIndiceQueue[NextIndiceIndex] != NodeIndex+1)
			{
				bDetectedCyclicalLogic = true;
			}
		}
		IndiceIndex = NextIndiceIndex;
	}

	// we didn't get through the entire execution queue, meaning we likely found 
	// a cycle that we couldn't resolve (an UnconditionalGoto that looped back
	// on a node we already processed)
	if (ExecutionIndiceQueue.Num() > 0)
	{
		bDetectedCyclicalLogic = true;
	}
	// if we detected cyclical logic, then we cannot compose a sorted/linear execution list
	return !bDetectedCyclicalLogic;
}

void FBlueprintCompilerCppBackend::EmitStatement(FBlueprintCompiledStatement &Statement, FEmitterLocalContext &EmitterContext, FKismetFunctionContext& FunctionContext)
{
	switch (Statement.Type)
	{
	case KCST_Nop:
		EmitterContext.AddLine(TEXT("//No operation."));
		break;
	case KCST_CallFunction:
		EmitCallStatment(EmitterContext, FunctionContext, Statement);
		break;
	case KCST_Assignment:
		EmitAssignmentStatment(EmitterContext, FunctionContext, Statement);
		break;
	case KCST_CompileError:
		UE_LOG(LogK2Compiler, Error, TEXT("C++ backend encountered KCST_CompileError"));
		EmitterContext.AddLine(TEXT("static_assert(false); // KCST_CompileError"));
		break;
	case KCST_PushState:
		EmitPushStateStatement(EmitterContext, FunctionContext, Statement);
		break;
	case KCST_Return:
		UE_LOG(LogK2Compiler, Error, TEXT("C++ backend encountered KCST_Return"));
		EmitterContext.AddLine(TEXT("// Return statement."));
		break;
	case KCST_EndOfThread:
		EmitEndOfThreadStatement(EmitterContext, FunctionContext);
		break;
	case KCST_Comment:
		EmitterContext.AddLine(FString::Printf(TEXT("// %s"), *Statement.Comment.Replace(TEXT("\n"), TEXT(" "))));
		break;
	case KCST_DebugSite:
		break;
	case KCST_CastObjToInterface:
		EmitCastObjToInterfaceStatement(EmitterContext, FunctionContext, Statement);
		break;
	case KCST_DynamicCast:
		EmitDynamicCastStatement(EmitterContext, FunctionContext, Statement);
		break;
	case KCST_ObjectToBool:
		EmitObjectToBoolStatement(EmitterContext, FunctionContext, Statement);
		break;
	case KCST_AddMulticastDelegate:
		EmitAddMulticastDelegateStatement(EmitterContext, FunctionContext, Statement);
		break;
	case KCST_ClearMulticastDelegate:
		EmitClearMulticastDelegateStatement(EmitterContext, FunctionContext, Statement);
		break;
	case KCST_WireTraceSite:
		break;
	case KCST_BindDelegate:
		EmitBindDelegateStatement(EmitterContext, FunctionContext, Statement);
		break;
	case KCST_RemoveMulticastDelegate:
		EmitRemoveMulticastDelegateStatement(EmitterContext, FunctionContext, Statement);
		break;
	case KCST_CallDelegate:
		EmitCallDelegateStatment(EmitterContext, FunctionContext, Statement);
		break;
	case KCST_CreateArray:
		EmitCreateArrayStatement(EmitterContext, FunctionContext, Statement);
		break;
	case KCST_CrossInterfaceCast:
		EmitCastBetweenInterfacesStatement(EmitterContext, FunctionContext, Statement);
		break;
	case KCST_MetaCast:
		EmitMetaCastStatement(EmitterContext, FunctionContext, Statement);
		break;
	case KCST_CastInterfaceToObj:
		EmitCastInterfaceToObjStatement(EmitterContext, FunctionContext, Statement);
		break;
	case KCST_ComputedGoto:
	case KCST_UnconditionalGoto:
	case KCST_GotoIfNot:
	case KCST_EndOfThreadIfNot:
	case KCST_GotoReturn:
	case KCST_GotoReturnIfNot:
		EmitGotoStatement(EmitterContext, FunctionContext, Statement);
		break;
	case KCST_CreateSet:
		EmitCreateSetStatement(EmitterContext, FunctionContext, Statement);
		break;
	case KCST_CreateMap:
		EmitCreateMapStatement(EmitterContext, FunctionContext, Statement);
		break;
	case KCST_SwitchValue:
		// Switch Value should be always an "inline" statement, so there is no point to handle it here
		// case: KCST_AssignmentOnPersistentFrame
	default:
		EmitterContext.AddLine(TEXT("// Warning: Ignoring unsupported statement\n"));
		UE_LOG(LogK2Compiler, Error, TEXT("C++ backend encountered unsupported statement type %d"), (int32)Statement.Type);
		break;
	};
}

bool FBlueprintCompilerCppBackend::EmitAllStatements(FKismetFunctionContext &FunctionContext, int32 ExecutionGroup, FEmitterLocalContext &EmitterContext, const TArray<UEdGraphNode*>& LinearExecutionList)
{
	ensure(!bUseExecutionGroup || FunctionContext.UnsortedSeparateExecutionGroups.IsValidIndex(ExecutionGroup));
	bool bFirsCase = true;

	bool bAnyNonReducableStatement = false;
	// Emit code in the order specified by the linear execution list (the first node is always the entry point for the function)
	for (int32 NodeIndex = 0; NodeIndex < LinearExecutionList.Num(); ++NodeIndex)
	{
		UEdGraphNode* StatementNode = LinearExecutionList[NodeIndex];
		TArray<FBlueprintCompiledStatement*>* StatementList = FunctionContext.StatementsPerNode.Find(StatementNode);
		ensureMsgf(StatementNode && StatementNode->IsA<UK2Node>() && !CastChecked<UK2Node>(StatementNode)->IsNodePure()
			, TEXT("Wrong Statement node %s in function %s")
			, *GetPathNameSafe(StatementNode)
			, *GetPathNameSafe(FunctionContext.Function));

		const bool bIsCurrentExecutionGroup = !bUseExecutionGroup || FunctionContext.UnsortedSeparateExecutionGroups[ExecutionGroup].Contains(StatementNode);
		if (StatementList && bIsCurrentExecutionGroup)
		{
			for (int32 StatementIndex = 0; StatementIndex < StatementList->Num(); ++StatementIndex)
			{
				FBlueprintCompiledStatement& Statement = *((*StatementList)[StatementIndex]);
				if ((Statement.bIsJumpTarget || bFirsCase) && bUseGotoState)
				{
					const int32 StateNum = StatementToStateIndex(FunctionContext, &Statement);
					if (bFirsCase)
					{
						bFirsCase = false;
					}
					else
					{
						EmitterContext.DecreaseIndent();
						EmitterContext.AddLine(TEXT("}"));
						EmitterContext.DecreaseIndent();
					}
					EmitterContext.AddLine(FString::Printf(TEXT("case %d:"), StateNum));
					EmitterContext.IncreaseIndent();
					EmitterContext.AddLine(TEXT("{"));
					EmitterContext.IncreaseIndent();
				}
				EmitStatement(Statement, EmitterContext, FunctionContext);
				bAnyNonReducableStatement |= !FKismetCompilerUtilities::IsStatementReducible(Statement.Type);
			}
		}
	}
	return bAnyNonReducableStatement;
}

bool FBlueprintCompilerCppBackend::PrepareToUseExecutionGroupWithoutGoto(FKismetFunctionContext &FunctionContext, int32 ExecutionGroup, UEdGraphNode* &TheOnlyEntryPoint)
{
	ensure(FunctionContext.bIsUbergraph && bUseExecutionGroup);
	for (UEdGraphNode* Node : FunctionContext.UnsortedSeparateExecutionGroups[ExecutionGroup])
	{
		if (Node && Node->IsA<UK2Node_ExecutionSequence>())
		{
			return false;
		}

		auto RequiresGoto = [](const FBlueprintCompiledStatement* Statement)->bool
		{
			// has no KCST_GotoIfNot state. Other states can be handled without switch
			return Statement && (Statement->Type == KCST_PushState || Statement->Type == KCST_GotoIfNot);
			//Statement->Type == KCST_UnconditionalGoto ||
			//Statement->Type == KCST_ComputedGoto ||
			//Statement->Type == KCST_EndOfThread ||
			//Statement->Type == KCST_EndOfThreadIfNot ||
			//Statement->Type == KCST_GotoReturn ||
			//Statement->Type == KCST_GotoReturnIfNot
		};
		TArray<FBlueprintCompiledStatement*>* StatementList = FunctionContext.StatementsPerNode.Find(Node);
		if (StatementList && StatementList->ContainsByPredicate(RequiresGoto))
		{
			return false;
		}

		// we assume, that only the entry point generates Computed Goto
		if (Node && Node->IsA<UK2Node_FunctionEntry>())
		{
			return false;
		}
		UK2Node_Event* AsEvent = Cast<UK2Node_Event>(Node);
		if (TheOnlyEntryPoint && AsEvent)
		{
			return false;
		}
		if (AsEvent)
		{
			TheOnlyEntryPoint = AsEvent;
		}
	}

	// 2. find latent action calling this group
	for (UEdGraphNode* Node : FunctionContext.LinearExecutionList)
	{
		UK2Node_CallFunction* CallFunctionNode = Cast<UK2Node_CallFunction>(Node);
		if (CallFunctionNode && CallFunctionNode->IsLatentFunction() && CallFunctionNode->GetThenPin())
		{
			for (UEdGraphPin* Link : CallFunctionNode->GetThenPin()->LinkedTo)
			{
				UEdGraphNode* OwnerNode = Link ? Link->GetOwningNodeUnchecked() : nullptr;
				if (OwnerNode && FunctionContext.UnsortedSeparateExecutionGroups[ExecutionGroup].Contains(OwnerNode))
				{
					if (!TheOnlyEntryPoint)
					{
						TheOnlyEntryPoint = OwnerNode;

						TArray<FBlueprintCompiledStatement*>* OwnerStatementList = FunctionContext.StatementsPerNode.Find(OwnerNode);
						FBlueprintCompiledStatement* FirstStatementToCall = (OwnerStatementList && OwnerStatementList->Num()) ? (*OwnerStatementList)[0] : nullptr;
						TArray<FBlueprintCompiledStatement*>* LatentCallStatementList = FunctionContext.StatementsPerNode.Find(CallFunctionNode);
						check(LatentCallStatementList && FirstStatementToCall);
						bool bMatch = false;
						for (FBlueprintCompiledStatement* LatentCallStatement : *LatentCallStatementList)
						{
							if (LatentCallStatement && (KCST_CallFunction == LatentCallStatement->Type))
							{
								if (ensure(LatentCallStatement->TargetLabel == FirstStatementToCall))
								{
									bMatch = true;
								}
							}
						}
						ensure(bMatch);
					}
					else if (TheOnlyEntryPoint && (OwnerNode != TheOnlyEntryPoint))
					{
						return false;
					}
				}
			}

		}
	}
	return true;
}
