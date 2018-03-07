// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "Components/InputComponent.h"
#include "GameFramework/PlayerController.h"


/* UInputComponent interface
 *****************************************************************************/

UInputComponent::UInputComponent( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
	bBlockInput = false;
}


float UInputComponent::GetAxisValue( const FName AxisName ) const
{
	float AxisValue = 0.f;
	if (AxisName.IsNone())
	{
		return AxisValue;
	}

	bool bFound = false;

	for (const FInputAxisBinding& AxisBinding : AxisBindings)
	{
		if (AxisBinding.AxisName == AxisName)
		{
			bFound = true;
			AxisValue = AxisBinding.AxisValue;
			break;
		}
	}

	if (!bFound)
	{
		UE_LOG(LogPlayerController, Warning, TEXT("Request for value of axis '%s' returning 0 as it is not bound on this input component."), *AxisName.ToString());
	}

	return AxisValue;
}


float UInputComponent::GetAxisKeyValue( const FKey AxisKey ) const
{
	float AxisValue = 0.f;
	bool bFound = false;

	for (const FInputAxisKeyBinding& AxisBinding : AxisKeyBindings)
	{
		if (AxisBinding.AxisKey == AxisKey)
		{
			bFound = true;
			AxisValue = AxisBinding.AxisValue;
			break;
		}
	}

	if (!bFound)
	{
		UE_LOG(LogPlayerController, Warning, TEXT("Request for value of axis key '%s' returning 0 as it is not bound on this input component."), *AxisKey.ToString());
	}

	return AxisValue;
}


FVector UInputComponent::GetVectorAxisValue( const FKey AxisKey ) const
{
	FVector AxisValue;
	bool bFound = false;

	for (const FInputVectorAxisBinding& AxisBinding : VectorAxisBindings)
	{
		if (AxisBinding.AxisKey == AxisKey)
		{
			AxisValue = AxisBinding.AxisValue;
			bFound = true;
			break;
		}
	}

	if (!bFound)
	{
		UE_LOG(LogPlayerController, Warning, TEXT("Request for value of vector axis key '%s' returning 0 as it is not bound on this input component."), *AxisKey.ToString());
	}

	return AxisValue;
}


bool UInputComponent::HasBindings( ) const
{
	return ((ActionBindings.Num() > 0) ||
			(AxisBindings.Num() > 0) ||
			(AxisKeyBindings.Num() > 0) ||
			(KeyBindings.Num() > 0) ||
			(TouchBindings.Num() > 0) ||
			(GestureBindings.Num() > 0) ||
			(VectorAxisBindings.Num() > 0));
}


FInputActionBinding& UInputComponent::AddActionBinding( const FInputActionBinding& Binding )
{
	ActionBindings.Add(Binding);
	if (Binding.KeyEvent == IE_Pressed || Binding.KeyEvent == IE_Released)
	{
		const EInputEvent PairedEvent = (Binding.KeyEvent == IE_Pressed ? IE_Released : IE_Pressed);
		for (int32 BindingIndex = ActionBindings.Num() - 2; BindingIndex >= 0; --BindingIndex )
		{
			FInputActionBinding& ActionBinding = ActionBindings[BindingIndex];
			if (ActionBinding.ActionName == Binding.ActionName)
			{
				// If we find a matching event that is already paired we know this is paired so mark it off and we're done
				if (ActionBinding.bPaired)
				{
					ActionBindings.Last().bPaired = true;
					break;
				}
				// Otherwise if this is a pair to the new one mark them both as paired
				// Don't break as there could be two bound paired events
				else if (ActionBinding.KeyEvent == PairedEvent)
				{
					ActionBinding.bPaired = true;
					ActionBindings.Last().bPaired = true;
				}
			}
		}
	}

	return ActionBindings.Last();
}


void UInputComponent::ClearActionBindings( )
{
	ActionBindings.Reset();
}

void UInputComponent::RemoveActionBinding( const int32 BindingIndex )
{
	if (BindingIndex >= 0 && BindingIndex < ActionBindings.Num())
	{
		const FInputActionBinding& BindingToRemove = ActionBindings[BindingIndex];

		// Potentially need to clear some pairings
		if (BindingToRemove.bPaired)
		{
			TArray<int32> IndicesToClear;
			const EInputEvent PairedEvent = (BindingToRemove.KeyEvent == IE_Pressed ? IE_Released : IE_Pressed);

			for (int32 ActionIndex = 0; ActionIndex < ActionBindings.Num(); ++ActionIndex)
			{
				if (ActionIndex != BindingIndex)
				{
					const FInputActionBinding& ActionBinding = ActionBindings[ActionIndex];
					if (ActionBinding.ActionName == BindingToRemove.ActionName)
					{
						// If we find another of the same key event then the pairing is intact so we're done
						if (ActionBinding.KeyEvent == BindingToRemove.KeyEvent)
						{
							IndicesToClear.Empty();
							break;
						}
						// Otherwise we may need to clear the pairing so track the index
						else if (ActionBinding.KeyEvent == PairedEvent)
						{
							IndicesToClear.Add(ActionIndex);
						}
					}
				}
			}

			for (int32 ClearIndex = 0; ClearIndex < IndicesToClear.Num(); ++ClearIndex)
			{
				ActionBindings[IndicesToClear[ClearIndex]].bPaired = false;
			}
		}

		ActionBindings.RemoveAt(BindingIndex);
	}
}

void UInputComponent::ClearBindingValues()
{
	for (FInputAxisBinding& AxisBinding : AxisBindings)
	{
		AxisBinding.AxisValue = 0.f;
	}
	for (FInputAxisKeyBinding& AxisKeyBinding : AxisKeyBindings)
	{
		AxisKeyBinding.AxisValue = 0.f;
	}
	for (FInputVectorAxisBinding& VectorAxisBinding : VectorAxisBindings)
	{
		VectorAxisBinding.AxisValue = FVector::ZeroVector;
	}
	for (FInputGestureBinding& GestureBinding : GestureBindings)
	{
		GestureBinding.GestureValue = 0.f;
	}
}

/* Deprecated functions (needed for Blueprints)
 *****************************************************************************/

bool UInputComponent::IsControllerKeyDown(FKey Key) const { return false; }
bool UInputComponent::WasControllerKeyJustPressed(FKey Key) const { return false; }
bool UInputComponent::WasControllerKeyJustReleased(FKey Key) const { return false; }
float UInputComponent::GetControllerAnalogKeyState(FKey Key) const { return 0.f; }
FVector UInputComponent::GetControllerVectorKeyState(FKey Key) const { return FVector(); }
void UInputComponent::GetTouchState(int32 FingerIndex, float& LocationX, float& LocationY, bool& bIsCurrentlyPressed) const { }
float UInputComponent::GetControllerKeyTimeDown(FKey Key) const { return 0.f; }
void UInputComponent::GetControllerMouseDelta(float& DeltaX, float& DeltaY) const { }
void UInputComponent::GetControllerAnalogStickState(EControllerAnalogStick::Type WhichStick, float& StickX, float& StickY) const { }
