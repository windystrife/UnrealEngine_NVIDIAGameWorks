// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "ShaderCompilerCommon.h"
#include "glsl_parser_extras.h"
#include "ir.h"
#include "ir_visitor.h"
#include "ir_rvalue_visitor.h"
#include "OptValueNumbering.h"
#include "IRDump.h"
//@todo-rco: Remove STL!
#include <algorithm>
#include <stack>
#include <vector>
#include "ir_basic_block.h"

#define printf(...)

typedef std::list<struct SBasicBlock*> TBasicBlockList;

struct SBasicBlock
{
	const int id;

	ir_instruction* IRFirst;
	ir_instruction* IRLast;

	TBasicBlockList Predecessors;
	TBasicBlockList Successors;

	static int IDCounter;

	SBasicBlock(ir_instruction* InIRFirst, ir_instruction* InIRLast) : id(IDCounter++), IRFirst(InIRFirst), IRLast(InIRLast)
	{
	}
};

int SBasicBlock::IDCounter(1);

struct SCFG
{
	TBasicBlockList BasicBlocks;

	_mesa_glsl_parse_state* ParseState;
	bool bChanged;

	SCFG(_mesa_glsl_parse_state* InParseState) : ParseState(InParseState), bChanged(false) {}
	~SCFG()
	{
		for (auto it = BasicBlocks.begin(); it != BasicBlocks.end(); ++it)
		{
			delete *it;
		}
	}

	void CallPerBasicBlock( void(*Callback)(SBasicBlock*, SCFG*, void* Data), void* Data )
	{
		for (auto it = BasicBlocks.begin(); it != BasicBlocks.end(); ++it)
		{
			auto* BB = *it;
			Callback(BB, this, Data);
		}
	}
};

struct SNumber
{
	int Number;
	bool bPartialWrite;

	SNumber(int InNumber = -1) : Number(InNumber), bPartialWrite(false) {}

	bool operator==(const SNumber& N) { return Number == N.Number; }
	bool operator!=(const SNumber& N) { return !(*this == N); }

	static SNumber CreateNumber()
	{
		static int Counter = 0;
		++Counter;
		SNumber N(Counter);
		return N;
	}
};
static bool operator<(const SNumber& N1, const SNumber& N2) { return N1.Number < N2.Number; }
typedef TArray<SNumber> TNumberVector;

inline bool AreEqual(TNumberVector& A, TNumberVector& B)
{
	auto Size = A.Num();
	if (Size != B.Num())
	{
		return false;
	}
	
	for (int i = 0; i < Size; ++i)
	{
		if (A[i] != B[i])
		{
			return false;
		}
	}

	return true;
}

typedef std::map<ir_instruction*, SNumber> TLVN;

struct SLVNRedundancyInfo
{
	int NumRedundancies;
	ir_variable* NewVar;
	ir_assignment* NewAssign;

	SLVNRedundancyInfo() : NumRedundancies(0), NewVar(nullptr), NewAssign(nullptr) {}
};

struct SLVNOptimizeRedundant : public ir_rvalue_visitor
{
	_mesa_glsl_parse_state* ParseState;
	TLVN& LVN;
	std::map<SNumber, SLVNRedundancyInfo>& NumRedundancies;
	bool bChanged;

	SLVNOptimizeRedundant(_mesa_glsl_parse_state* InParseState, TLVN& InLVN, std::map<SNumber, SLVNRedundancyInfo>& InNumRedundancies) :
	ParseState(InParseState),
		LVN(InLVN),
		NumRedundancies(InNumRedundancies),
		bChanged(false)
	{
	}

	virtual void handle_rvalue(ir_rvalue** RValuePtr) override
	{
		if (RValuePtr && *RValuePtr)
		{
			ir_rvalue* RValue = *RValuePtr;
			auto Found = LVN.find(RValue);
			if (Found != LVN.end())
			{
				auto Num = Found->second;
				auto& RedundancyInfo = NumRedundancies[Num];
				if (RedundancyInfo.NumRedundancies > 1)
				{
					if (RedundancyInfo.NewVar == nullptr)
					{
						RedundancyInfo.NewVar = new(ParseState) ir_variable(RValue->type, nullptr, ir_var_temporary);
						RedundancyInfo.NewAssign = new(ParseState) ir_assignment(new(ParseState) ir_dereference_variable(RedundancyInfo.NewVar), RValue);
						base_ir->insert_before(RedundancyInfo.NewVar);
						base_ir->insert_before(RedundancyInfo.NewAssign);
						printf("--- Adding redundancy %d, NewVar %d, New Assign %d\n", Num.Number, RedundancyInfo.NewVar->id, RedundancyInfo.NewAssign->id);
					}

					*RValuePtr = new(ParseState) ir_dereference_variable(RedundancyInfo.NewVar);
					bChanged = true;
				}
			}
		}
	}

	virtual ir_visitor_status visit_leave(ir_call* IR) override
	{
		auto Found = LVN.find(IR);
		if (Found != LVN.end())
		{
			auto Num = Found->second;
			auto& RedundancyInfo = NumRedundancies[Num];
			if (RedundancyInfo.NumRedundancies > 1)
			{
				if (RedundancyInfo.NewVar == nullptr)
				{
					check(IR->return_deref);
					RedundancyInfo.NewVar = new(ParseState) ir_variable(IR->return_deref->type, nullptr, ir_var_temporary);
					RedundancyInfo.NewAssign = new(ParseState) ir_assignment(new(ParseState) ir_dereference_variable(IR->return_deref->var), new(ParseState) ir_dereference_variable(RedundancyInfo.NewVar));
					IR->return_deref = new(ParseState) ir_dereference_variable(RedundancyInfo.NewVar);
					base_ir->insert_before(RedundancyInfo.NewVar);
					base_ir->insert_after(RedundancyInfo.NewAssign);
				}
				else
				{
					// Convert call to assignment
					auto* NewAssign = new(ParseState) ir_assignment(new(ParseState) ir_dereference_variable(IR->return_deref->var), new(ParseState) ir_dereference_variable(RedundancyInfo.NewVar));
					base_ir->insert_after(NewAssign);
					base_ir->remove();
				}
			}
		}

		return visit_continue;
	}
};

typedef std::map<SBasicBlock*, struct SLVNVisitor*> TLVNVisitors;
struct SLVNVisitor : public ir_hierarchical_visitor
{
	TLVN LVN;
	bool bChanged;
	struct SArrayPair
	{
		SNumber Base;
		SNumber Index;

		SArrayPair() {}
		SArrayPair(SNumber B, SNumber I) : Base(B), Index(I) {}
		bool operator==(const SArrayPair& P) { return Base == P.Base && Index == P.Index; }
	};
	std::stack<SNumber> ExpressionNumberStack;
	std::map<ir_variable*, TNumberVector > Assignments;
	std::map<ir_expression*, TNumberVector > Expressions;
	struct SFunctionCall
	{
		TNumberVector Parameters;
		SNumber AssignmentNumber;
	};
	std::map<ir_call*, SFunctionCall > FunctionCalls;
	std::map<ir_texture*, TNumberVector > Textures;
	std::map<ir_dereference_array*, SArrayPair > Arrays;
	std::map<ir_swizzle*, SNumber > SwizzleVars;
	_mesa_glsl_parse_state* ParseState;
	bool bInLHS;

	SLVNVisitor(_mesa_glsl_parse_state* InParseState) : ParseState(InParseState), bChanged(false), bInLHS(false) {}

	virtual ir_visitor_status visit(ir_constant* IR) override
	{
		printf("\tconst @ %d\n", IR->id);
		for (auto it = LVN.begin(); it != LVN.end(); ++it)
		{
			auto* K = it->first->as_constant();
			if (K && K->has_value(IR))
			{
				LVN[IR] = it->second;
				ExpressionNumberStack.push(it->second);
				printf("\t\tRED %d\n", it->second.Number);
				return visit_continue;
			}
		}

		auto Number = SNumber::CreateNumber();
		printf("\t\tNEW %d\n", Number.Number, IR->id);
		LVN[IR] = Number;
		ExpressionNumberStack.push(Number);
		return visit_continue;
	}

	virtual SNumber AddVariable(ir_variable* IR)
	{
		auto Number = SNumber::CreateNumber();
		Assignments[IR].Add(Number);
		return Number;
	}

	virtual ir_visitor_status visit(ir_variable* IR) override
	{
		AddVariable(IR);
		return visit_continue;
	}

	virtual ir_visitor_status visit(ir_dereference_variable* IR) override
	{
		printf("\tDeRefVar @ %d \n", IR->id);
		auto Found = Assignments.find(IR->var);
		if (Found == Assignments.end())
		{
			auto VarNumber = AddVariable(IR->var);
			printf("\t\tNEW %d\n", VarNumber.Number);
			ExpressionNumberStack.push(VarNumber.Number);
		}
		else
		{
			TNumberVector& NumberVector = Found->second;
			for (int32 Index = NumberVector.Num() - 1; Index >= 0; --Index)
			{
				const SNumber& VarNumber = NumberVector[Index];
				printf("\t\tRED %d\n", VarNumber.Number);
				ExpressionNumberStack.push(VarNumber);
				if (!VarNumber.bPartialWrite)
				{
					break;
				}
			}
		}

		return visit_continue;
	}

	virtual ir_visitor_status visit_enter(ir_dereference_array* IR) override
	{
		printf("\tArray @ %d\n", IR->id);
		check(!bInLHS);
		auto StackSize = ExpressionNumberStack.size();
		IR->array->accept(this);
		check(StackSize + 1 == ExpressionNumberStack.size());
		IR->array_index->accept(this);
		check(StackSize + 2 == ExpressionNumberStack.size());

		auto Index = ExpressionNumberStack.top();
		ExpressionNumberStack.pop();
		auto Base = ExpressionNumberStack.top();
		ExpressionNumberStack.pop();
		SArrayPair Pair(Base, Index);

		for (auto it = Arrays.begin(); it != Arrays.end(); ++it)
		{
			auto& ArrayPair = it->second;
			if (ArrayPair == Pair)
			{
				auto ArrayNum = LVN[it->first];
				printf("\t\tRED %d\n", ArrayNum.Number);
				ExpressionNumberStack.push(ArrayNum);
				return visit_continue_with_parent;
			}
		}

		auto Number = SNumber::CreateNumber();
		printf("\t\tNEW %d\n", Number.Number);
		LVN[IR] = Number;
		Arrays[IR] = Pair;
		ExpressionNumberStack.push(Number);

		return visit_continue_with_parent;
	}

	virtual ir_visitor_status visit_enter(ir_dereference_image *) override
	{
		check(0);
		return visit_continue_with_parent;
	}

	virtual ir_visitor_status visit_enter(ir_call* IR) override
	{
		printf("* Call @ %d\n", IR->id);

		// Traverse parameters and definitions
		printf("\tParams\n");
		TNumberVector Parameters;
		exec_node* Param = IR->actual_parameters.head;
		exec_node* ParamDefinition = IR->callee->parameters.head;
		check(Param && ParamDefinition);
		while (!Param->is_tail_sentinel() && !ParamDefinition->is_tail_sentinel())
		{
			ir_variable* Def = ((ir_instruction*)ParamDefinition)->as_variable();
			bool bIsOut = (Def->mode == ir_var_inout || Def->mode == ir_var_out);
			// TODO: Support out parameters
			check(!bIsOut);
			ir_rvalue* Parameter = (ir_rvalue*)Param;
			auto StackSize = ExpressionNumberStack.size();
			Parameter->accept(this);
			check(StackSize + 1 == ExpressionNumberStack.size());
			auto ParameterNum = ExpressionNumberStack.top();
			Parameters.Add(ParameterNum);
			ExpressionNumberStack.pop();

			Param = Param->get_next();
			ParamDefinition = ParamDefinition->get_next();
		}
		check(Param->is_tail_sentinel() && ParamDefinition->is_tail_sentinel());
		printf("\t%s(", IR->callee_name());
		for (auto it = Parameters.begin(); it != Parameters.end(); ++it)
		{
			if (it != Parameters.begin())
			{
				printf(", ");
			}
			printf("%d", *it);
		}
		printf(")\n");

		for (auto it = FunctionCalls.begin(); it != FunctionCalls.end(); ++it)
		{
			auto& FuncCall = it->second;
			if (AreEqual(FuncCall.Parameters, Parameters))
			{
				// TODO: Check for UAV's as their read/writes can't be optimized/reordered!
				if (IR->return_deref)
				{
					ir_variable* ReturnVar = IR->return_deref->variable_referenced();
					LVN[IR] = FuncCall.AssignmentNumber;
					Assignments[ReturnVar].Add(FuncCall.AssignmentNumber);
					printf("\tRED Var %d = FunctionCall %d\n", ReturnVar->id, FuncCall.AssignmentNumber);
				}
				else
				{
					check(0);
				}
				return visit_continue_with_parent;
			}
		}

		SFunctionCall Call;
		Call.AssignmentNumber = SNumber::CreateNumber();
		Call.Parameters = Parameters;

		// TODO: Check for UAV's as their read/writes can't be optimized/reordered!
		if (IR->return_deref)
		{
			ir_variable* ReturnVar = IR->return_deref->variable_referenced();
			Assignments[ReturnVar].Add(Call.AssignmentNumber);
			printf("\tVar %d = FunctionCall %d\n", ReturnVar->id, Call.AssignmentNumber);
		}
		else
		{
			printf("\tFunctionCall %d\n", Call.AssignmentNumber.Number);
		}

		FunctionCalls[IR] = Call;
		LVN[IR] = Call.AssignmentNumber;

		return visit_continue_with_parent;
	}

	virtual ir_visitor_status visit_enter(ir_dereference_record *) override
	{
		return visit_continue_with_parent;
	}

	virtual ir_visitor_status visit_enter(ir_return *) override
	{
		return visit_continue_with_parent;
	}

	virtual ir_visitor_status visit_enter(ir_discard *) override
	{
		return visit_continue_with_parent;
	}

	virtual ir_visitor_status visit_enter(ir_if* IR) override
	{
		IR->condition->accept(this);
		check(ExpressionNumberStack.size() == 1);
		ExpressionNumberStack.pop();
		// Skip Then/Else
		return visit_continue_with_parent;
	}

	virtual ir_visitor_status visit_enter(ir_atomic *) override
	{
		return visit_continue_with_parent;
	}

	virtual ir_visitor_status visit_enter(ir_loop *) override
	{
		return visit_continue_with_parent;
	}

	virtual ir_visitor_status visit_enter(ir_function_signature *) override
	{
		return visit_continue_with_parent;
	}

	virtual ir_visitor_status visit_enter(ir_function *) override
	{
		// Do not step inside, as this was handled as part of a Basic Block
		return visit_continue_with_parent;
	}

	virtual ir_visitor_status visit_leave(ir_swizzle* IR) override
	{
		printf("\tSwizzle @ %d\n", IR->id);
		check(ExpressionNumberStack.size() >= 1);
		auto Operand = ExpressionNumberStack.top();
		ExpressionNumberStack.pop();

		for (auto it = SwizzleVars.begin(); it != SwizzleVars.end(); ++it)
		{
			auto* Swizzle = it->first;
			auto SwizzleOperand = it->second;
			if (SwizzleOperand == Operand && !memcmp(&Swizzle->mask, &IR->mask, sizeof(IR->mask)))
			{
				auto SwizzleNumber = LVN[Swizzle];
				printf("\t\tRED %d\n", SwizzleNumber.Number);
				LVN[IR] = SwizzleNumber;
				ExpressionNumberStack.push(SwizzleNumber);
				return visit_continue;
			}
		}

		auto Number = SNumber::CreateNumber();
		printf("\t\tNEW %d\n", Number.Number);
		LVN[IR] = Number;
		SwizzleVars[IR] = Operand;
		ExpressionNumberStack.push(Number);
		return visit_continue;
	}

	virtual ir_visitor_status visit_enter(ir_texture* IR) override
	{
		printf("\tTex @ %d\n", IR->id);

		TNumberVector Operands;
		auto ProcessEntry = [&](ir_rvalue* x)
			{
				if (x)
				{
					auto n = ExpressionNumberStack.size();
					x->accept(this);
					check(n < ExpressionNumberStack.size());
					while (n < ExpressionNumberStack.size())
					{
						Operands.Add(ExpressionNumberStack.top());
						ExpressionNumberStack.pop();
					}
					check(n == ExpressionNumberStack.size());
				}
			};
		ProcessEntry(IR->sampler);
		ProcessEntry(IR->coordinate);
		ProcessEntry(IR->projector);
		ProcessEntry(IR->shadow_comparitor);
		ProcessEntry(IR->offset);
		ProcessEntry(IR->lod_info.grad.dPdy);
		ProcessEntry(IR->lod_info.grad.dPdx);
		ProcessEntry(IR->SamplerState);

		int NumOperands = Operands.Num();
		for (auto it = Textures.begin(); it != Textures.end(); ++it)
		{
			auto* Tex = it->first;
			if (Tex->op == IR->op && Tex->channel == IR->channel)
			{
				auto& TexOps = it->second;
				if (NumOperands == TexOps.Num())
				{
					if (AreEqual(TexOps, Operands))
					{
						auto TexNumber = LVN[Tex];
						LVN[IR] = TexNumber;
						Textures[IR] = Operands;
						ExpressionNumberStack.push(TexNumber);
						printf("\t\tRED %d\n", TexNumber.Number);
						return visit_continue_with_parent;
					}
				}
			}
		}

		auto Number = SNumber::CreateNumber();
		printf("\t\tNEW %d\n", Number.Number);
		LVN[IR] = Number;
		Exchange(Textures[IR], Operands);
		ExpressionNumberStack.push(Number);
		return visit_continue_with_parent;
	}

	virtual ir_visitor_status visit_leave(ir_expression* IR) override
	{
		printf("\tExpr @ %d\n", IR->id);
		int NumOperands = IR->get_num_operands();
		check(ExpressionNumberStack.size() >= NumOperands);

		TNumberVector Operands;
		for (int i = 0; i < NumOperands; ++i)
		{
			Operands.Add(ExpressionNumberStack.top());
			ExpressionNumberStack.pop();
		}
		std::reverse(Operands.begin(), Operands.end());

		printf("\t\top %s: ", IR->operator_string());
		for (int i = 0; i < NumOperands; ++i)
		{
			printf(" %d", Operands[i]);
		}
		printf("\n");

		for (auto& Pair : Expressions)
		{
			auto* Expr = Pair.first;
			if (Expr->operation == IR->operation)
			{
				auto& ExprOperands = Pair.second;
				check(ExprOperands.Num() == NumOperands);
				if (AreEqual(ExprOperands, Operands))
				{
					auto ExprNumber = LVN[Expr];
					LVN[IR] = ExprNumber;
					Expressions[IR] = Operands;
					ExpressionNumberStack.push(ExprNumber);
					printf("\t\tRED %d\n", ExprNumber.Number);
					return visit_continue;
				}
			}
		}

		auto Number = SNumber::CreateNumber();
		printf("\t\tNEW %d\n", Number.Number);
		LVN[IR] = Number;
		Expressions[IR] = Operands;
		ExpressionNumberStack.push(Number);

		return visit_continue;
	}

	virtual ir_visitor_status visit_enter(ir_assignment* IR) override
	{
		printf("* Assignment @ %d (stack size %d)\n", IR->id, ExpressionNumberStack.size());

		// Handle LHS
		printf("\tLHS\n");
		check(!bInLHS);
		bInLHS = true;
		ir_variable* LHSVar = IR->lhs->variable_referenced();
		check(LHSVar);
		bInLHS = false;

		printf("\tRHS\n");
		// Handle RHS
		auto StatusRHS = IR->rhs->accept(this);
		check(StatusRHS == visit_continue);
		check(!ExpressionNumberStack.empty());
		auto RHSNumber = ExpressionNumberStack.top();
		ExpressionNumberStack.pop();
		check(ExpressionNumberStack.empty());

		if (!IR->whole_variable_written())
		{
			// Currently we only support full masked writes
			RHSNumber.bPartialWrite = true;
		}
		Assignments[LHSVar].Add(RHSNumber);
		printf("\tVar %d, Expr %d %s\n", LHSVar->id, RHSNumber.Number, RHSNumber.bPartialWrite ? "SWIZZLE" : "");
		LVN[IR->rhs] = RHSNumber;

		return visit_continue_with_parent;
	}

	bool OptimizeRedundantExpressions(SBasicBlock* BasicBlock, TLVNVisitors& Visitors)
	{
		std::map<SNumber, SLVNRedundancyInfo> NumRedundancies;
		for (auto it = LVN.begin(); it != LVN.end(); ++it)
		{
			auto Num = it->second;
			// No need to increase redundancies on Vars or Constants
			auto* RValue = it->first;
			if (!RValue->as_constant() && !RValue->as_variable() && !RValue->as_swizzle())
			{
				if (!Num.bPartialWrite)
				{
					if (RValue->as_texture())
					{
						NumRedundancies[Num].NumRedundancies++;
						printf("+++ @ RValueIR @%d: Redundancy %d: %d\n", RValue->id, Num.Number, NumRedundancies[Num].NumRedundancies);
					}
				}
			}
		}

		printf("****************** LVN for BB %d (%d - %d)\n", BasicBlock->id, BasicBlock->IRFirst->id, BasicBlock->IRLast->id);
		SLVNOptimizeRedundant Visitor(ParseState, LVN, NumRedundancies);
		VisitRange(&Visitor, BasicBlock->IRFirst, BasicBlock->IRLast, true);
		return Visitor.bChanged;
	}

	static void RunPerBasicBlock(SBasicBlock* BB, SCFG* CFG, void* Data)
	{
		printf("----------- BB %d\n", BB->id);
		auto* LVNVisitorList = (TLVNVisitors*)Data;
		// Deleted looping through map later on
		auto* Visitor = new SLVNVisitor(CFG->ParseState);
		VisitRange(Visitor, BB->IRFirst, BB->IRLast);
		(*LVNVisitorList)[BB] = Visitor;
	}
};

struct SCFGCreator
{
	SCFG& CFG;
	TBasicBlockList& BasicBlocks;
	SCFGCreator(SCFG& InCFG) : CFG(InCFG), BasicBlocks(InCFG.BasicBlocks) {}

	static void CreateBasicBlocks(ir_instruction* IRFirst, ir_instruction* IRLast, void* Data)
	{
		SCFGCreator* CFGCreator = (SCFGCreator*)Data;
		// Deleted in ~SCFG()
		auto* BasicBlock = new SBasicBlock(IRFirst, IRLast);
		CFGCreator->/*CFG->*/BasicBlocks.push_back(BasicBlock);
	}

	void LinkBBs(SBasicBlock* Prev, SBasicBlock* Next)
	{
		Next->Predecessors.push_back(Prev);
		Prev->Successors.push_back(Next);
	}

	void LinkBBs(SBasicBlock* Prev, TBasicBlockList::iterator& Next)
	{
		if (Next != BasicBlocks.end())
		{
			LinkBBs(Prev, *Next);
		}
	}

	void LinkBBs(TBasicBlockList::iterator& Prev, TBasicBlockList::iterator& Next)
	{
		if (Prev != BasicBlocks.end() && Next != BasicBlocks.end())
		{
			LinkBBs(*Prev, *Next);
		}
	}

	TBasicBlockList::iterator FindBasicBlock(ir_instruction* IR)
	{
		if (IR)
		{
			for (auto it = BasicBlocks.begin(); it != BasicBlocks.end(); ++it)
			{
				auto* Block = *it;
				if (Block->IRFirst == IR)
				{
					return it;
				}
			}
		}

		return BasicBlocks.end();
	}

	TBasicBlockList::iterator FindBasicBlock(exec_list& List)
	{
		auto* IR = (ir_instruction*)List.get_head();
		return FindBasicBlock(IR);
	}

	void LinkBasicBlocks(TBasicBlockList::iterator& It)
	{
		check(It != BasicBlocks.end());
		auto* BasicBlock = *It;
		auto* IRLast = (*It)->IRLast;
		auto* IRIf = IRLast->as_if();
		if (IRIf)
		{
			auto ThenIter = FindBasicBlock(IRIf->then_instructions);
			auto ElseIter = FindBasicBlock(IRIf->else_instructions);

			auto* IRAfterElse = (ir_instruction*)IRIf->get_next();
			auto IterAfterElse = FindBasicBlock(IRAfterElse);

			if (ThenIter != BasicBlocks.end() && ElseIter != BasicBlocks.end())
			{
				// Only need to link If to then & else
				LinkBBs(BasicBlock, ThenIter);
				LinkBBs(BasicBlock, ElseIter);
				LinkBBs(ThenIter, IterAfterElse);
				LinkBBs(ElseIter, IterAfterElse);
			}
			else
			{
				if (ThenIter != BasicBlocks.end())
				{
					LinkBBs(BasicBlock, ThenIter);
					LinkBBs(ThenIter, IterAfterElse);
				}
				else if (ElseIter != BasicBlocks.end())
				{
					LinkBBs(BasicBlock, ElseIter);
					LinkBBs(ElseIter, IterAfterElse);
				}

				LinkBBs(BasicBlock, IterAfterElse);
			}
		}
		else if (IRLast->as_loop())
		{
			check(0);
		}
		else if (IRLast->as_call())
		{
			++It;
			if (It != BasicBlocks.end())
			{
				auto* NextBB = *It;
				LinkBBs(BasicBlock, NextBB);
				LinkBasicBlocks(It);
			}
		}
		else if (IRLast->as_function() || IRLast->as_return() || IRLast->as_loop() || IRLast->as_loop())
		{
			// TODO
			check(0);
		}
		else
		{
			check(IRLast->as_assignment());
		}
	}

	void TrimOrphanBBs()
	{
		TBasicBlockList NewList;
		for (auto it = BasicBlocks.begin(); it != BasicBlocks.end(); ++it)
		{
			auto* BB = *it;
			if (!BB->Predecessors.empty() || !BB->Successors.empty())
			{
				NewList.push_back(BB);
			}
		}

		BasicBlocks.swap(NewList);
	}

	void Dump()
	{
		printf("------------\n");
		for (auto it = BasicBlocks.begin(); it != BasicBlocks.end(); ++it)
		{
			auto* BB = *it;
			printf("*** Basic Block %d @ %d - %d\n", BB->id, BB->IRFirst->id, BB->IRLast->id);
			printf("\tPRED:");
			for (auto j = BB->Predecessors.begin(); j != BB->Predecessors.end(); ++j)
			{
				auto* Pred = *j;
				printf(" %d", Pred->id);
			}
			printf("\n\tSUCC:");
			for (auto j = BB->Successors.begin(); j != BB->Successors.end(); ++j)
			{
				auto* Pred = *j;
				printf(" %d", Pred->id);
			}
			printf("\n");
		}
	}

	void MergeSingleBasicBlocks()
	{
		bool bChangesMade = false;
		do
		{
			bChangesMade = false;
			TBasicBlockList NewList;
			for (auto it = BasicBlocks.begin(); it != BasicBlocks.end(); ++it)
			{
				auto* BB = *it;
				bool bDoAdd = true;
				if (BB->Predecessors.size() == 1)
				{
					SBasicBlock* Pred = BB->Predecessors.front();
					if (Pred->Successors.size() == 1)
					{
						printf("\tMerging BB %d and BB %d\n", Pred->id, BB->id);

						// Modify Pred to include this instructions and remove this link
						Pred->IRLast = BB->IRLast;
						Pred->Successors.remove(BB);

						// Move all successors to be Pred's successors and update their Predecessors
						for (auto j = BB->Successors.begin(); j != BB->Successors.end(); ++j)
						{
							auto* Succ = *j;
							Succ->Predecessors.remove(BB);
							LinkBBs(Pred, Succ);
							bChangesMade = true;
						}
						bDoAdd = false;
					}
				}

				if (bDoAdd)
				{
					NewList.push_back(BB);
				}
			}

			BasicBlocks = NewList;
		}
		while (bChangesMade);
	}

	void Construct(exec_list* Instructions)
	{
		call_for_basic_blocks(Instructions, &CreateBasicBlocks, this);
		TBasicBlockList::iterator It = BasicBlocks.begin();
		LinkBasicBlocks(It);
//Dump();
		MergeSingleBasicBlocks();
//Dump();
//		TrimOrphanBBs();
//		Dump();
	}
};

bool LocalValueNumbering(exec_list* Instructions, _mesa_glsl_parse_state* ParseState)
{
//IRDump(Instructions);
	// Create initial CFG & Basic Blocks
	SCFG CFG(ParseState);
	SCFGCreator CFGCreator(CFG);
	CFGCreator.Construct(Instructions);

	// Perform Local Value Numbering per Basic Block
	TLVNVisitors LVNVisitors;
	CFG.CallPerBasicBlock(&SLVNVisitor::RunPerBasicBlock, &LVNVisitors);

	for (auto it = LVNVisitors.begin(); it != LVNVisitors.end(); ++it)
	{
		auto* BB = it->first;
		auto* Visitor = it->second;
		CFG.bChanged |= Visitor->OptimizeRedundantExpressions(BB, LVNVisitors);
	}

	{
		for (auto& Pair : LVNVisitors)
		{
			delete Pair.second;
		}
	}

//IRDump(Instructions);
	return CFG.bChanged;
}

struct SExpandSubexpressionsVisitor : public ir_rvalue_visitor
{
	_mesa_glsl_parse_state* ParseState;
	int ExpressionDepth;
	SExpandSubexpressionsVisitor(_mesa_glsl_parse_state* InParseState) : ParseState(InParseState), ExpressionDepth(0) {}

	virtual void handle_rvalue(ir_rvalue** RValuePtr) override
	{
		if (RValuePtr && *RValuePtr)
		{
			auto* Expression = (*RValuePtr)->as_expression();
			if (Expression && Expression->get_num_operands() > 1)
			{
				auto* NewVar = new(ParseState) ir_variable(Expression->type, nullptr, ir_var_temporary);
				auto* NewAssignment = new(ParseState) ir_assignment(new(ParseState) ir_dereference_variable(NewVar), Expression);
				base_ir->insert_before(NewVar);
				base_ir->insert_before(NewAssignment);
				*RValuePtr = new(ParseState) ir_dereference_variable(NewVar);
			}
		}
	}

	virtual ir_visitor_status visit_enter(ir_expression* IR) override
	{
		++ExpressionDepth;
		return ir_rvalue_visitor::visit_enter(IR);
	}

	virtual ir_visitor_status visit_leave(ir_expression* IR) override
	{
		--ExpressionDepth;
		return ir_rvalue_visitor::visit_leave(IR);
	}
};

bool ExpandSubexpressions( exec_list* Instructions, _mesa_glsl_parse_state* ParseState )
{
//IRDump(Instructions);
	SExpandSubexpressionsVisitor Visitor(ParseState);
	Visitor.run(Instructions);
//IRDump(Instructions);
	return true;
}
