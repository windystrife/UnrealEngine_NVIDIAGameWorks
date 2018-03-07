// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

template <typename T>
struct TIsEnum
{
	enum { Value = __is_enum(T) };
};
