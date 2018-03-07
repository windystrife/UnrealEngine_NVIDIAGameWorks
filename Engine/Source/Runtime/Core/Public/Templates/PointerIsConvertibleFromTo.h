// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

/**
 * Tests if a From* is convertible to a To*
 **/
template <typename From, typename To>
struct TPointerIsConvertibleFromTo
{
private:
	static uint8  Test(...);
	static uint16 Test(To*);

public:
	enum { Value  = sizeof(Test((From*)nullptr)) - 1 };
};


class TPointerIsConvertibleFromTo_TestBase
{
};

class TPointerIsConvertibleFromTo_TestDerived : public TPointerIsConvertibleFromTo_TestBase
{
};

class TPointerIsConvertibleFromTo_Unrelated
{
};

static_assert(TPointerIsConvertibleFromTo<bool, bool>::Value, "Platform TPointerIsConvertibleFromTo test failed.");
static_assert(TPointerIsConvertibleFromTo<void, void>::Value, "Platform TPointerIsConvertibleFromTo test failed.");
static_assert(TPointerIsConvertibleFromTo<bool, void>::Value, "Platform TPointerIsConvertibleFromTo test failed.");
static_assert(TPointerIsConvertibleFromTo<const bool, const void>::Value, "Platform TPointerIsConvertibleFromTo test failed.");
static_assert(TPointerIsConvertibleFromTo<TPointerIsConvertibleFromTo_TestDerived, TPointerIsConvertibleFromTo_TestBase>::Value, "Platform TPointerIsConvertibleFromTo test failed.");
static_assert(TPointerIsConvertibleFromTo<TPointerIsConvertibleFromTo_TestDerived, const TPointerIsConvertibleFromTo_TestBase>::Value, "Platform TPointerIsConvertibleFromTo test failed.");
static_assert(TPointerIsConvertibleFromTo<const TPointerIsConvertibleFromTo_TestDerived, const TPointerIsConvertibleFromTo_TestBase>::Value, "Platform TPointerIsConvertibleFromTo test failed.");
static_assert(TPointerIsConvertibleFromTo<TPointerIsConvertibleFromTo_TestBase, TPointerIsConvertibleFromTo_TestBase>::Value, "Platform TPointerIsConvertibleFromTo test failed.");
static_assert(TPointerIsConvertibleFromTo<TPointerIsConvertibleFromTo_TestBase, void>::Value, "Platform TPointerIsConvertibleFromTo test failed.");

static_assert(!TPointerIsConvertibleFromTo<TPointerIsConvertibleFromTo_TestBase, TPointerIsConvertibleFromTo_TestDerived>::Value, "Platform TPointerIsConvertibleFromTo test failed.");
static_assert(!TPointerIsConvertibleFromTo<TPointerIsConvertibleFromTo_Unrelated, TPointerIsConvertibleFromTo_TestBase>::Value, "Platform TPointerIsConvertibleFromTo test failed.");
static_assert(!TPointerIsConvertibleFromTo<bool, TPointerIsConvertibleFromTo_TestBase>::Value, "Platform TPointerIsConvertibleFromTo test failed.");
static_assert(!TPointerIsConvertibleFromTo<void, TPointerIsConvertibleFromTo_TestBase>::Value, "Platform TPointerIsConvertibleFromTo test failed.");
static_assert(!TPointerIsConvertibleFromTo<TPointerIsConvertibleFromTo_TestBase, bool>::Value, "Platform TPointerIsConvertibleFromTo test failed.");
static_assert(!TPointerIsConvertibleFromTo<void, bool>::Value, "Platform TPointerIsConvertibleFromTo test failed.");
