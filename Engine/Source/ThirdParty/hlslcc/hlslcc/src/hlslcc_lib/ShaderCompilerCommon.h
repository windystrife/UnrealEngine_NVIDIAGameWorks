// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

// Needed on platforms other than Windows
#ifndef WIN32
#include <stdio.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <limits.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#endif

#ifndef check
#include <assert.h>
#define check(x)	assert(x)
//#define check(x) do{if (!(x)) __debugbreak();}while(0)
#endif

// We can't use static_assert in .c files as this is a C++(11) feature
#if __cplusplus && !__clang__
#ifdef _MSC_VER
typedef unsigned long long uint64_t;
#endif // _MSC_VER
static_assert(sizeof(uint64_t) == 8, "Bad!");
#endif

#if __cplusplus
static inline bool isalpha(char c)
{
	return ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'));
}
#endif

//@todo-rco: Temp way to find out if we are compiling with or without UE
#ifndef CPP
// Wrap std::vector API's definitions
#include <vector>
template<typename InElementType>
class TArray
{
public:
	typedef InElementType ElementType;
	typedef std::vector<ElementType> TSTLVector;

	void Add(const ElementType& Element)
	{
		Vector.push_back(Element);
	}

	int Num() const
	{
		return (int)Vector.size();
	}

	ElementType& operator[](int Index)
	{
		return Vector[Index];
	}

	const ElementType& operator[](int Index) const
	{
		return Vector[Index];
	}

	typename TSTLVector::iterator begin()
	{
		return Vector.begin();
	}

	typename TSTLVector::iterator end()
	{
		return Vector.end();
	}

	typename TSTLVector::const_iterator begin() const
	{
		return Vector.begin();
	}

	typename TSTLVector::const_iterator end() const
	{
		return Vector.end();
	}

	void Reset(int NewSize)
	{
		Vector.clear();
		Vector.reserve(NewSize);
	}

	void AddZeroed(int Count)
	{
		Vector.resize(Vector.size() + Count, (ElementType)0);
	}

//private:
	TSTLVector Vector;
};

template<typename T>
void Exchange(TArray<T>& A, TArray<T>& B)
{
	A.Vector.swap(B.Vector);
}
#endif

#ifndef uint32
#include <stdint.h>
typedef uint32_t uint32;
typedef int32_t int32;
#endif
