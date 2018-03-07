// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "K2Node_InAppPurchaseRestore.h"
#include "InAppPurchaseRestoreCallbackProxy.h"

#define LOCTEXT_NAMESPACE "K2Node"

UK2Node_InAppPurchaseRestore::UK2Node_InAppPurchaseRestore(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ProxyFactoryFunctionName = GET_FUNCTION_NAME_CHECKED(UInAppPurchaseRestoreCallbackProxy, CreateProxyObjectForInAppPurchaseRestore);
	ProxyFactoryClass = UInAppPurchaseRestoreCallbackProxy::StaticClass();

	ProxyClass = UInAppPurchaseRestoreCallbackProxy::StaticClass();
}

#undef LOCTEXT_NAMESPACE
