// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AnimBody.h"

UAnimBody::UAnimBody(const class FObjectInitializer& Init)
	: Super(Init)
{
	Left = Init.CreateDefaultSubobject<UAnimHand>(this, TEXT("Left"));
	Right = Init.CreateDefaultSubobject<UAnimHand>(this, TEXT("Right"));

	Head = Init.CreateDefaultSubobject<UAnimBone>(this, TEXT("Head"));
}


bool UAnimBody::Enabled()
{
	return Alpha == 1.f;
}

void UAnimBody::SetEnabled(bool Enable)
{
	if (Enable)
	{
		Alpha = 1.f;
	}
	else
	{
		Alpha = 0.f;
	}

	//Forward to every finger
	Left->SetEnabled(Enable);
	Right->SetEnabled(Enable);

	Head->SetEnabled(Enable);
}

void UAnimBody::TranslateBody(FVector Shift)
{
	//Shift all hands
	Left->TranslateHand(Shift);
	Right->TranslateHand(Shift);

	//Shift head
	Head->TranslateBone(Shift);
}

void UAnimBody::ChangeBasis(FRotator PreBase, FRotator PostBase, bool AdjustVectors)
{
	//Change Basis for all hands
	Left->ChangeBasis(PreBase, PostBase, AdjustVectors);
	Right->ChangeBasis(PreBase, PostBase, AdjustVectors);

	//Careful with this tbh...
	Head->ChangeBasis(PreBase, PostBase, AdjustVectors);
}