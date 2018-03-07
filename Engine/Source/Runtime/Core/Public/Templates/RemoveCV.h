// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"

/**
 * TRemoveCV<type> will remove any const/volatile qualifiers from a type.
 * (based on std::remove_cv<>
 * note: won't remove the const from "const int*", as the pointer is not const
 */
template <typename T> struct TRemoveCV                   { typedef T Type; };
template <typename T> struct TRemoveCV<const T>          { typedef T Type; };
template <typename T> struct TRemoveCV<volatile T>       { typedef T Type; };
template <typename T> struct TRemoveCV<const volatile T> { typedef T Type; };
