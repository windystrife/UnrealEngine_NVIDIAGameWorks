// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScriptDisassembler.h: Disassembler for Kismet bytecode.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Script.h"

/**
 * Kismet bytecode disassembler; Can be used to create a human readable version
 * of Kismet bytecode for a specified structure or class.
 */
class FKismetBytecodeDisassembler
{
private:
	TArray<uint8> Script;
	FString Indents;
	FOutputDevice& Ar;
public:
	/**
	 * Construct a disassembler that will output to the specified archive.
	 *
	 * @param	InAr	The archive to emit disassembled bytecode to.
	 */
	UNREALED_API FKismetBytecodeDisassembler(FOutputDevice& InAr);

	/**
	 * Disassemble all of the script code in a single structure.
	 *
	 * @param [in,out]	Source	The structure to disassemble.
	 */
	UNREALED_API void DisassembleStructure(UFunction* Source);

	/**
	 * Disassemble all functions in any classes that have matching names.
	 *
	 * @param	InAr	The archive to emit disassembled bytecode to.
	 * @param	ClassnameSubstring	A class must contain this substring to be disassembled.
	 */
	UNREALED_API static void DisassembleAllFunctionsInClasses(FOutputDevice& Ar, const FString& ClassnameSubstring);
private:

	// Reading functions
	int32 ReadINT(int32& ScriptIndex);
	uint64 ReadQWORD(int32& ScriptIndex);
	uint8 ReadBYTE(int32& ScriptIndex);
	FString ReadName(int32& ScriptIndex);
	uint16 ReadWORD(int32& ScriptIndex);
	float ReadFLOAT(int32& ScriptIndex);
	CodeSkipSizeType ReadSkipCount(int32& ScriptIndex);
	FString ReadString(int32& ScriptIndex);
	FString ReadString8(int32& ScriptIndex);
	FString ReadString16(int32& ScriptIndex);

	EExprToken SerializeExpr(int32& ScriptIndex);
	void ProcessCastByte(int32 CastType, int32& ScriptIndex);
	void ProcessCommon(int32& ScriptIndex, EExprToken Opcode);

	void InitTables();

	template<typename T>
	void Skip(int32& ScriptIndex)
	{
		ScriptIndex += sizeof(T);
	}

	void AddIndent()
	{
		Indents += TEXT("  ");
	}

	void DropIndent()
	{
		// Blah, this is awful
		Indents = Indents.Left(Indents.Len() - 2);
	}

	template <typename T>
	T* ReadPointer(int32& ScriptIndex)
	{
		return (T*)ReadQWORD(ScriptIndex);
	}
};
