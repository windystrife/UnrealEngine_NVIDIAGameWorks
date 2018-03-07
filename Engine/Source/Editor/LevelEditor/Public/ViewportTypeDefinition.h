// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

class FUICommandInfo;
class IViewportLayoutEntity;
struct FViewportConstructionArgs;

/** Definition of a custom viewport */
struct FViewportTypeDefinition
{
	typedef TFunction<TSharedRef<IViewportLayoutEntity>(const FViewportConstructionArgs&)> FFactoryFunctionType;

	template<typename T>
	static FViewportTypeDefinition FromType(const TSharedPtr<FUICommandInfo>& ActivationCommand)
	{
		return FViewportTypeDefinition([](const FViewportConstructionArgs& Args) -> TSharedRef<IViewportLayoutEntity> {
			return MakeShareable(new T(Args));
		}, ActivationCommand);
	}

	FViewportTypeDefinition(const FFactoryFunctionType& InFactoryFunction, const TSharedPtr<FUICommandInfo>& InActivationCommand)
		: ActivationCommand(InActivationCommand)
		, FactoryFunction(InFactoryFunction)
	{}

	/** A UI command for toggling activation this viewport */
	TSharedPtr<FUICommandInfo> ActivationCommand;

	/** Function used to create a new instance of the viewport */
	FFactoryFunctionType FactoryFunction;
};
