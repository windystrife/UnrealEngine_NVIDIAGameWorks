// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "MovieSceneAnimTypeID.h"

struct FMovieSceneBlendingActuatorID : FMovieSceneAnimTypeID
{
	explicit FMovieSceneBlendingActuatorID(FMovieSceneAnimTypeID InTypeID) : FMovieSceneAnimTypeID(InTypeID) {}

	friend bool operator==(FMovieSceneBlendingActuatorID A, FMovieSceneBlendingActuatorID B)
	{
		return A.ID == B.ID;
	}

	friend bool operator!=(FMovieSceneBlendingActuatorID A, FMovieSceneBlendingActuatorID B)
	{
		return A.ID != B.ID;
	}

	friend uint32 GetTypeHash(FMovieSceneBlendingActuatorID In)
	{
		return GetTypeHash(In.ID);
	}
};
