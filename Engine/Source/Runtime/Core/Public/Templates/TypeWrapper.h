// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

/**
 * Wraps a type.
 *
 * The intended use of this template is to allow types to be passed around where the unwrapped type would give different behavior.
 * An example of this is when you want a template specialization to refer to the primary template, but doing that would cause
 * infinite recursion through the specialization, e.g.:
 *
 * // Before
 * template <typename T>
 * struct TThing
 * {
 *     void f(T t)
 *     {
 *          DoSomething(t);
 *     }
 * };
 *
 * template <>
 * struct TThing<int>
 * {
 *     void f(int t)
 *     {
 *         DoSomethingElseFirst(t);
 *         TThing<int>::f(t); // Infinite recursion
 *     }
 * };
 *
 *
 * // After
 * template <typename T>
 * struct TThing
 * {
 *     typedef typename TUnwrapType<T>::Type RealT;
 *
 *     void f(RealT t)
 *     {
 *          DoSomething(t);
 *     }
 * };
 *
 * template <>
 * struct TThing<int>
 * {
 *     void f(int t)
 *     {
 *         DoSomethingElseFirst(t);
 *         TThing<TTypeWrapper<int>>::f(t); // works
 *     }
 * };
 */
template <typename T>
struct TTypeWrapper;

template <typename T>
struct TUnwrapType
{
	typedef T Type;
};

template <typename T>
struct TUnwrapType<TTypeWrapper<T>>
{
	typedef T Type;
};
