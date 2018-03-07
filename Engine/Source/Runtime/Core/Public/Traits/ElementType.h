// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include <initializer_list>

/**
 * Traits class which gets the element type of a container.
 */
template <typename T>
struct TElementType
{
	using Type = typename T::ElementType;
};

template <typename T> struct TElementType<             T& > : TElementType<T> {};
template <typename T> struct TElementType<             T&&> : TElementType<T> {};
template <typename T> struct TElementType<const          T> : TElementType<T> {};
template <typename T> struct TElementType<      volatile T> : TElementType<T> {};
template <typename T> struct TElementType<const volatile T> : TElementType<T> {};

/**
 * Specialization for C arrays
 */
template <typename T, size_t N> struct TElementType<               T[N]> { using Type = T; };
template <typename T, size_t N> struct TElementType<const          T[N]> { using Type = T; };
template <typename T, size_t N> struct TElementType<      volatile T[N]> { using Type = T; };
template <typename T, size_t N> struct TElementType<const volatile T[N]> { using Type = T; };

/**
 * Specialization for initializer lists
 */
template <typename T>
struct TElementType<std::initializer_list<T>>
{
	using Type = typename TRemoveCV<T>::Type;
};
