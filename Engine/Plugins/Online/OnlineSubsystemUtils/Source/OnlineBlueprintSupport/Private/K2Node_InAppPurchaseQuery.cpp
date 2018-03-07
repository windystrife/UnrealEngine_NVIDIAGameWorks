// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "K2Node_InAppPurchaseQuery.h"
#include "InAppPurchaseQueryCallbackProxy.h"

#define LOCTEXT_NAMESPACE "K2Node"

UK2Node_InAppPurchaseQuery::UK2Node_InAppPurchaseQuery(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ProxyFactoryFunctionName = GET_FUNCTION_NAME_CHECKED(UInAppPurchaseQueryCallbackProxy, CreateProxyObjectForInAppPurchaseQuery);
	ProxyFactoryClass = UInAppPurchaseQueryCallbackProxy::StaticClass();

	ProxyClass = UInAppPurchaseQueryCallbackProxy::StaticClass();
}

#undef LOCTEXT_NAMESPACE
