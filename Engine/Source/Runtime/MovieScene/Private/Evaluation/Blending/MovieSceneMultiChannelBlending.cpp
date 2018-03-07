// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneMultiChannelBlending.h"

template<> FMovieSceneAnimTypeID GetBlendingDataType<int32>()		{ static FMovieSceneAnimTypeID TypeID = FMovieSceneAnimTypeID::Unique(); return TypeID; }
template<> FMovieSceneAnimTypeID GetBlendingDataType<float>()		{ static FMovieSceneAnimTypeID TypeID = FMovieSceneAnimTypeID::Unique(); return TypeID; }
template<> FMovieSceneAnimTypeID GetBlendingDataType<FVector2D>()	{ static FMovieSceneAnimTypeID TypeID = FMovieSceneAnimTypeID::Unique(); return TypeID; }
template<> FMovieSceneAnimTypeID GetBlendingDataType<FVector>()		{ static FMovieSceneAnimTypeID TypeID = FMovieSceneAnimTypeID::Unique(); return TypeID; }
template<> FMovieSceneAnimTypeID GetBlendingDataType<FVector4>()	{ static FMovieSceneAnimTypeID TypeID = FMovieSceneAnimTypeID::Unique(); return TypeID; }
template<> FMovieSceneAnimTypeID GetBlendingDataType<FTransform>()	{ static FMovieSceneAnimTypeID TypeID = FMovieSceneAnimTypeID::Unique(); return TypeID; }
