// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Sections/VectorPropertySection.h"
#include "FloatCurveKeyArea.h"
#include "ISectionLayoutBuilder.h"
#include "Sections/MovieSceneVectorSection.h"


void FVectorPropertySection::GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const
{
	UMovieSceneVectorSection* VectorSection = Cast<UMovieSceneVectorSection>(&SectionObject);
	ChannelsUsed = VectorSection->GetChannelsUsed();
	check(ChannelsUsed >= 2 && ChannelsUsed <= 4);

	TAttribute<TOptional<float>> XExternalValue = TAttribute<TOptional<float>>::Create(
		TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &FVectorPropertySection::GetVectorXValue));
	TSharedRef<FFloatCurveKeyArea> XKeyArea = MakeShareable(new FFloatCurveKeyArea(&VectorSection->GetCurve(0), XExternalValue, VectorSection));
	LayoutBuilder.AddKeyArea("Vector.X", NSLOCTEXT("FVectorPropertySection", "XArea", "X"), XKeyArea);

	TAttribute<TOptional<float>> YExternalValue = TAttribute<TOptional<float>>::Create(
		TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &FVectorPropertySection::GetVectorYValue));
	TSharedRef<FFloatCurveKeyArea> YKeyArea = MakeShareable(new FFloatCurveKeyArea(&VectorSection->GetCurve(1), YExternalValue, VectorSection));
	LayoutBuilder.AddKeyArea("Vector.Y", NSLOCTEXT("FVectorPropertySection", "YArea", "Y"), YKeyArea);

	if (ChannelsUsed >= 3)
	{
		TAttribute<TOptional<float>> ZExternalValue = TAttribute<TOptional<float>>::Create(
			TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &FVectorPropertySection::GetVectorZValue));
		TSharedRef<FFloatCurveKeyArea> ZKeyArea = MakeShareable(new FFloatCurveKeyArea(&VectorSection->GetCurve(2), ZExternalValue, VectorSection));
		LayoutBuilder.AddKeyArea("Vector.Z", NSLOCTEXT("FVectorPropertySection", "ZArea", "Z"), ZKeyArea);
	}

	if (ChannelsUsed >= 4)
	{
		TAttribute<TOptional<float>> WExternalValue = TAttribute<TOptional<float>>::Create(
			TAttribute<TOptional<float>>::FGetter::CreateRaw(this, &FVectorPropertySection::GetVectorWValue));
		TSharedRef<FFloatCurveKeyArea> WKeyArea = MakeShareable(new FFloatCurveKeyArea(&VectorSection->GetCurve(3), WExternalValue, VectorSection));
		LayoutBuilder.AddKeyArea("Vector.W", NSLOCTEXT("FVectorPropertySection", "WArea", "W"), WKeyArea);
	}
}


TOptional<FVector4> FVectorPropertySection::GetPropertyValueAsVector4() const
{
	if (ChannelsUsed == 2)
	{
		TOptional<FVector2D> Vector = GetPropertyValue<FVector2D>();
		return Vector.IsSet() ? TOptional<FVector4>(FVector4(Vector.GetValue().X, Vector.GetValue().Y, 0, 0)) : TOptional<FVector4>();
	}
	else if (ChannelsUsed == 3)
	{
		TOptional<FVector> Vector = GetPropertyValue<FVector>();
		return Vector.IsSet() ? TOptional<FVector4>(FVector4(Vector.GetValue().X, Vector.GetValue().Y, Vector.GetValue().Z, 0)) : TOptional<FVector4>();
	}
	else // ChannelsUsed == 4
	{
		return GetPropertyValue<FVector4>();
	}
}


TOptional<float> FVectorPropertySection::GetVectorXValue() const
{
	TOptional<FVector4> Vector = GetPropertyValueAsVector4();
	return Vector.IsSet() ? TOptional<float>(Vector.GetValue().X) : TOptional<float>();
}

TOptional<float> FVectorPropertySection::GetVectorYValue() const
{
	TOptional<FVector4> Vector = GetPropertyValueAsVector4();
	return Vector.IsSet() ? TOptional<float>(Vector.GetValue().Y) : TOptional<float>();
}

TOptional<float> FVectorPropertySection::GetVectorZValue() const
{
	TOptional<FVector4> Vector = GetPropertyValueAsVector4();
	return Vector.IsSet() ? TOptional<float>(Vector.GetValue().Z) : TOptional<float>();
}

TOptional<float> FVectorPropertySection::GetVectorWValue() const
{
	TOptional<FVector4> Vector = GetPropertyValueAsVector4();
	return Vector.IsSet() ? TOptional<float>(Vector.GetValue().W) : TOptional<float>();
}
