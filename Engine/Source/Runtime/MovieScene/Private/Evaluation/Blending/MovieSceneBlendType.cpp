// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneBlendType.h"
#include "EnumClassFlags.h"

namespace MovieScene
{
	ENUM_CLASS_FLAGS(EMovieSceneBlendType)
}

FMovieSceneBlendTypeField::FMovieSceneBlendTypeField()
	: BlendTypeField((EMovieSceneBlendType)0)
{
}

FMovieSceneBlendTypeField FMovieSceneBlendTypeField::All()
{
	FMovieSceneBlendTypeField New;
	New.Add(EMovieSceneBlendType::Absolute, EMovieSceneBlendType::Additive, EMovieSceneBlendType::Relative);
	return New;
}

FMovieSceneBlendTypeField FMovieSceneBlendTypeField::None()
{
	FMovieSceneBlendTypeField New;
	return New;
}

void FMovieSceneBlendTypeField::Add(EMovieSceneBlendType Type)
{
	using namespace MovieScene;
	BlendTypeField |= Type;
}

void FMovieSceneBlendTypeField::Add(FMovieSceneBlendTypeField Field)
{
	using namespace MovieScene;
	BlendTypeField |= Field.BlendTypeField;
}

void FMovieSceneBlendTypeField::Remove(EMovieSceneBlendType Type)
{
	using namespace MovieScene;
	BlendTypeField |= Type;
}

void FMovieSceneBlendTypeField::Remove(FMovieSceneBlendTypeField Field)
{
	using namespace MovieScene;
	BlendTypeField &= ~Field.BlendTypeField;
}

FMovieSceneBlendTypeField FMovieSceneBlendTypeField::Invert() const
{
	using namespace MovieScene;
	return FMovieSceneBlendTypeField(~BlendTypeField);
}

bool FMovieSceneBlendTypeField::Contains(EMovieSceneBlendType InBlendType) const
{
	return EnumHasAnyFlags(BlendTypeField, InBlendType);
}

int32 FMovieSceneBlendTypeField::Num() const
{
	return
		(Contains(EMovieSceneBlendType::Absolute) ? 1 : 0) +
		(Contains(EMovieSceneBlendType::Relative) ? 1 : 0) +
		(Contains(EMovieSceneBlendType::Additive) ? 1 : 0);
}

void FMovieSceneBlendTypeFieldIterator::IterateToNext()
{
	do
	{
		++Offset;
	} while (*this && !Field.Contains(EMovieSceneBlendType(1 << Offset)));
}

EMovieSceneBlendType FMovieSceneBlendTypeFieldIterator::operator*()
{
	return EMovieSceneBlendType(1 << Offset);
}

FMovieSceneBlendTypeFieldIterator begin(const FMovieSceneBlendTypeField& InField)
{
	FMovieSceneBlendTypeFieldIterator It;
	It.Field = InField;
	It.Offset = -1;
	It.IterateToNext();
	return It;
}

FMovieSceneBlendTypeFieldIterator end(const FMovieSceneBlendTypeField& InField)
{
	FMovieSceneBlendTypeFieldIterator It;
	It.Field = InField;
	It.Offset = 3;
	return It;
}