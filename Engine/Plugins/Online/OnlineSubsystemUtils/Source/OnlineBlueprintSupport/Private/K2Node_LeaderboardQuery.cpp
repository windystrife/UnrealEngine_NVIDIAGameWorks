// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "K2Node_LeaderboardQuery.h"
#include "LeaderboardQueryCallbackProxy.h"

#define LOCTEXT_NAMESPACE "K2Node"

UK2Node_LeaderboardQuery::UK2Node_LeaderboardQuery(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ProxyFactoryFunctionName = GET_FUNCTION_NAME_CHECKED(ULeaderboardQueryCallbackProxy, CreateProxyObjectForIntQuery);
	ProxyFactoryClass = ULeaderboardQueryCallbackProxy::StaticClass();

	ProxyClass = ULeaderboardQueryCallbackProxy::StaticClass();
}

#undef LOCTEXT_NAMESPACE
