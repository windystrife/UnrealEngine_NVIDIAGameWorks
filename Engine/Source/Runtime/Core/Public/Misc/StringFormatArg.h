// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Containers/ContainersFwd.h"

/** An argument supplied to FString::Format */
struct CORE_API FStringFormatArg
{
	enum EType { Int, UInt, Double, String, StringLiteral };

	/** The type of this arg */
	EType Type;

	/* todo: convert this to a TVariant */
	union
	{
		/** Value as integer */
		int64 IntValue;
		/** Value as uint */
		uint64 UIntValue;
		/** Value as double */
		double DoubleValue;
		/** Value as a string literal */
		const TCHAR* StringLiteralValue;
	};

	/** Value as an FString */
	FString StringValue;

	FStringFormatArg( const int32 Value );
	FStringFormatArg( const uint32 Value );
	FStringFormatArg( const int64 Value );
	FStringFormatArg( const uint64 Value );
	FStringFormatArg( const float Value );
	FStringFormatArg( const double Value );
	FStringFormatArg( FString Value );
	FStringFormatArg( const TCHAR* Value );

	/** Copyable */
	FStringFormatArg( const FStringFormatArg& RHS );
	
private:

	/** Not default constructible */
	FStringFormatArg();
};
