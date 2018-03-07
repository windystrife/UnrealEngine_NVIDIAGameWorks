// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "K2Node_PlayMontage.h"
#include "PlayMontageCallbackProxy.h"

#define LOCTEXT_NAMESPACE "K2Node"

UK2Node_PlayMontage::UK2Node_PlayMontage(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	ProxyFactoryFunctionName = GET_FUNCTION_NAME_CHECKED(UPlayMontageCallbackProxy, CreateProxyObjectForPlayMontage);
	ProxyFactoryClass = UPlayMontageCallbackProxy::StaticClass();
	ProxyClass = UPlayMontageCallbackProxy::StaticClass();
}

FText UK2Node_PlayMontage::GetTooltipText() const
{
	return LOCTEXT("K2Node_PlayMontage_Tooltip", "Plays a Montage on a SkeletalMeshComponent");
}

FText UK2Node_PlayMontage::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return LOCTEXT("PlayMontage", "Play Montage");
}

FText UK2Node_PlayMontage::GetMenuCategory() const
{
	return LOCTEXT("PlayMontageCategory", "Animation|Montage");
}

void UK2Node_PlayMontage::GetPinHoverText(const UEdGraphPin& Pin, FString& HoverTextOut) const
{
	Super::GetPinHoverText(Pin, HoverTextOut);

	static FName NAME_OnNotifyBegin = FName(TEXT("OnNotifyBegin"));
	static FName NAME_OnNotifyEnd = FName(TEXT("OnNotifyEnd"));

	const FName PinName = FName(*Pin.PinName);
	if (PinName == NAME_OnNotifyBegin)
	{
		FText ToolTipText = LOCTEXT("K2Node_PlayMontage_OnNotifyBegin_Tooltip", "Event called when using a PlayMontageNotify or PlayMontageNotifyWindow Notify in a Montage.");
		HoverTextOut = FString::Printf(TEXT("%s\n%s"), *ToolTipText.ToString(), *HoverTextOut);
	}
	else if (PinName == NAME_OnNotifyEnd)
	{
		FText ToolTipText = LOCTEXT("K2Node_PlayMontage_OnNotifyEnd_Tooltip", "Event called when using a PlayMontageNotifyWindow Notify in a Montage.");
		HoverTextOut = FString::Printf(TEXT("%s\n%s"), *ToolTipText.ToString(), *HoverTextOut);
	}
}

#undef LOCTEXT_NAMESPACE
