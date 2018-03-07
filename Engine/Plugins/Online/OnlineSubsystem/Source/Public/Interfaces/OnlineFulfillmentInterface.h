// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlinePurchaseInterface.h"

/**
 * IOnlineFulfillment - Interface for redeeming codes.
 */
class IOnlineFulfillment
{
public:
	virtual ~IOnlineFulfillment() {}

	/**
	 * Initiate the checkout process for obtaining offers via code redemption
	 *
	 * @param UserId user initiating the request
	 * @param RedeemCodeRequest info needed for the redeem request
	 * @param Delegate completion callback (guaranteed to be called)
	 */
	virtual void RedeemCode(const FUniqueNetId& UserId, const FRedeemCodeRequest& RedeemCodeRequest, const FOnPurchaseRedeemCodeComplete& Delegate) = 0;
};
