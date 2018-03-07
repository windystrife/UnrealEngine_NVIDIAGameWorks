// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	HlslParser.h - Interface for parsing hlsl.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "Developer/ShaderCompilerCommon/Private/HlslUtils.h"

class Error;

namespace CrossCompiler
{
	struct FLinearAllocator;
	namespace AST
	{
		class FNode;
	}

	enum class EParseResult
	{
		Matched,
		NotMatched,
		Error,
	};

	namespace Parser
	{
		typedef void TCallback(void* CallbackData, CrossCompiler::FLinearAllocator* Allocator, CrossCompiler::TLinearArray<CrossCompiler::AST::FNode*>& ASTNodes);

		// Returns true if successfully parsed
		bool Parse(const FString& Input, const FString& Filename, FCompilerMessages& OutCompilerMessages, TCallback* Callback, void* CallbackData = nullptr);

		// Returns true if successfully parsed
		bool Parse(const FString& Input, const FString& Filename, FCompilerMessages& OutCompilerMessages, TFunction< void(CrossCompiler::FLinearAllocator* Allocator, CrossCompiler::TLinearArray<CrossCompiler::AST::FNode*>& ASTNodes)> Function);

		// Sample callback to write out all nodes into a string; pass a valid pointer FString* as OutFStringPointer
		void WriteNodesToString(void* OutFStringPointer, CrossCompiler::FLinearAllocator* Allocator, CrossCompiler::TLinearArray<CrossCompiler::AST::FNode*>& ASTNodes);
	}
}
