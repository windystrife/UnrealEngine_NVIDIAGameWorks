// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "BuildPatchServicesPrivate.h"

#if WITH_DEV_AUTOMATION_TESTS

#define _TEST_EQUAL(text, expression, expected) \
	TestEqual(text, expression, expected)

#define _TEST_NOT_EQUAL(text, expression, expected) \
	TestNotEqual(text, expression, expected)

#define _TEST_NULL(text, expression) \
	TestNull(text, expression)

#define _TEST_NOT_NULL(text, expression) \
	TestNotNull(text, expression)

#define TEST_EQUAL(expression, expected) \
	_TEST_EQUAL(TEXT(#expression), expression, expected)

#define TEST_NOT_EQUAL(expression, expected) \
	_TEST_NOT_EQUAL(TEXT(#expression), expression, expected)

#define TEST_TRUE(expression) \
	TEST_EQUAL(expression, true)

#define TEST_FALSE(expression) \
	TEST_EQUAL(expression, false)

#define TEST_NULL(expression) \
	_TEST_NULL(TEXT(#expression), expression)

#define TEST_NOT_NULL(expression) \
	_TEST_NOT_NULL(TEXT(#expression), expression)

#define MOCK_FUNC_NOT_IMPLEMENTED(funcname) \
	UE_LOG(LogBuildPatchServices, Error, TEXT(funcname) TEXT(": Called but there is no implementation."))

#define ARRAY(Type, ...) TArray<Type>({ __VA_ARGS__ })
#define ARRAYU64(...) ARRAY(uint64, __VA_ARGS__)

template<typename ElementType>
bool operator==(const TSet<ElementType>& LHS, const TSet<ElementType>& RHS)
{
	return (LHS.Num() == RHS.Num())
	    && (LHS.Difference(RHS).Num() == 0)
	    && (RHS.Difference(LHS).Num() == 0);
}

template<typename ElementType>
bool operator!=(const TSet<ElementType>& LHS, const TSet<ElementType>& RHS)
{
	return !(LHS == RHS);
}

#endif //WITH_DEV_AUTOMATION_TESTS
