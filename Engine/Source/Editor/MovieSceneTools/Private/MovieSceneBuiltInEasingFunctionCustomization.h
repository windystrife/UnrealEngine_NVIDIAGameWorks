// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDetailCustomization.h"

enum class EMovieSceneBuiltInEasing : uint8;
class FReply;
class IPropertyHandle;

class FMovieSceneBuiltInEasingFunctionCustomization : public IDetailCustomization
{
public:

	virtual void CustomizeDetails(IDetailLayoutBuilder& DetailBuilder) override;

	FReply SetType(EMovieSceneBuiltInEasing NewType);

private:

	TSharedPtr<IPropertyHandle> TypeProperty;
};