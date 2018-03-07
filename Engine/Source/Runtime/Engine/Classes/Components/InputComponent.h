// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "InputCoreTypes.h"
#include "Components/ActorComponent.h"
#include "Framework/Commands/InputChord.h"
#include "InputComponent.generated.h"

/** Utility delegate class to allow binding to either a C++ function or a blueprint script delegate */
template<class DelegateType, class DynamicDelegateType>
struct TInputUnifiedDelegate
{
	TInputUnifiedDelegate() {};
	TInputUnifiedDelegate(DelegateType const& D) : FuncDelegate(D) {};
	TInputUnifiedDelegate(DynamicDelegateType const& D) : FuncDynDelegate(D) {};

	/** Returns if either the native or dynamic delegate is bound */
	inline bool IsBound() const
	{
		return ( FuncDelegate.IsBound() || FuncDynDelegate.IsBound() );
	}

	/** Returns if either the native or dynamic delegate is bound to an object */
	inline bool IsBoundToObject(void const* Object) const
	{
		if (FuncDelegate.IsBound())
		{
			return FuncDelegate.IsBoundToObject(Object);
		}
		else if (FuncDynDelegate.IsBound())
		{
			return FuncDynDelegate.IsBoundToObject(Object);
		}

		return false;
	}

	/** Binds a native delegate and unbinds any bound dynamic delegate */
	template< class UserClass >
	inline void BindDelegate(UserClass* Object, typename DelegateType::template TUObjectMethodDelegate< UserClass >::FMethodPtr Func)
	{
		FuncDynDelegate.Unbind();
		FuncDelegate.BindUObject(Object, Func);
	}

	/** Binds a dynamic delegate and unbinds any bound native delegate */
	inline void BindDelegate(UObject* Object, const FName FuncName)
	{
		FuncDelegate.Unbind();
		FuncDynDelegate.BindUFunction(Object, FuncName);
	}

	/** Returns a reference to the native delegate and unbinds any bound dynamic delegate */
	DelegateType& GetDelegateForManualSet()
	{
		FuncDynDelegate.Unbind();
		return FuncDelegate;
	}

	/** Unbinds any bound delegates */
	inline void Unbind()
	{
		FuncDelegate.Unbind();
		FuncDynDelegate.Unbind();
	}

	/** Returns a const reference to the Function Delegate. */
	inline const DelegateType& GetDelegate() const
	{
		return FuncDelegate;
	}

	/** Returns a const reference to the Dynamic Function Delegate. */
	inline const DynamicDelegateType& GetDynamicDelegate() const
	{
		return FuncDynDelegate;
	}

protected:
	/** Holds the delegate to call. */
	DelegateType FuncDelegate;
	/** Holds the dynamic delegate to call. */
	DynamicDelegateType FuncDynDelegate;
};


/** Base class for the different binding types. */
struct FInputBinding
{
	/** Whether the binding should consume the input or allow it to pass to another component */
	uint32 bConsumeInput:1;

	/** Whether the binding should execute while paused */
	uint32 bExecuteWhenPaused:1;

	FInputBinding()
		: bConsumeInput(true)
		, bExecuteWhenPaused(false)
	{}
};

/** Delegate signature for action events. */
DECLARE_DELEGATE( FInputActionHandlerSignature );
DECLARE_DELEGATE_OneParam( FInputActionHandlerWithKeySignature, FKey );
DECLARE_DYNAMIC_DELEGATE_OneParam( FInputActionHandlerDynamicSignature, FKey, Key );

struct FInputActionUnifiedDelegate
{
	FInputActionUnifiedDelegate() {};
	FInputActionUnifiedDelegate(FInputActionHandlerSignature const& D) : FuncDelegate(D) {};
	FInputActionUnifiedDelegate(FInputActionHandlerWithKeySignature const& D) : FuncDelegateWithKey(D) {};
	FInputActionUnifiedDelegate(FInputActionHandlerDynamicSignature const& D) : FuncDynDelegate(D) {};

	/** Returns if either the native or dynamic delegate is bound */
	inline bool IsBound() const
	{
		return ( FuncDelegate.IsBound() || FuncDelegateWithKey.IsBound() || FuncDynDelegate.IsBound() );
	}

	/** Returns if either the native or dynamic delegate is bound to an object */
	inline bool IsBoundToObject(void const* Object) const
	{
		if (FuncDelegate.IsBound())
		{
			return FuncDelegate.IsBoundToObject(Object);
		}
		else if (FuncDelegateWithKey.IsBound())
		{
			return FuncDelegateWithKey.IsBoundToObject(Object);
		}
		else if (FuncDynDelegate.IsBound())
		{
			return FuncDynDelegate.IsBoundToObject(Object);
		}

		return false;
	}

	/** Binds a native delegate and unbinds any bound dynamic delegate */
	template< class UserClass >
	inline void BindDelegate(UserClass* Object, typename FInputActionHandlerSignature::template TUObjectMethodDelegate< UserClass >::FMethodPtr Func)
	{
		FuncDynDelegate.Unbind();
		FuncDelegateWithKey.Unbind();
		FuncDelegate.BindUObject(Object, Func);
	}

	template< class UserClass >
	inline void BindDelegate(UserClass* Object, typename FInputActionHandlerWithKeySignature::template TUObjectMethodDelegate< UserClass >::FMethodPtr Func)
	{
		FuncDynDelegate.Unbind();
		FuncDelegate.Unbind();
		FuncDelegateWithKey.BindUObject(Object, Func);
	}

	template< class DelegateType, class UserClass, typename... VarTypes >
	inline void BindDelegate(UserClass* Object, typename DelegateType::template TUObjectMethodDelegate< UserClass >::FMethodPtr Func, VarTypes... Vars)
	{
		FuncDynDelegate.Unbind();
		FuncDelegateWithKey.Unbind();
		FuncDelegate.BindUObject(Object, Func, Vars...);
	}

	/** Binds a dynamic delegate and unbinds any bound native delegate */
	inline void BindDelegate(UObject* Object, const FName FuncName)
	{
		FuncDelegate.Unbind();
		FuncDelegateWithKey.Unbind();
		FuncDynDelegate.BindUFunction(Object, FuncName);
	}

	/** Returns a reference to the native delegate and unbinds any bound dynamic delegate */
	FInputActionHandlerSignature& GetDelegateForManualSet()
	{
		FuncDynDelegate.Unbind();
		FuncDelegateWithKey.Unbind();
		return FuncDelegate;
	}

	/** Returns a reference to the native delegate and unbinds any bound dynamic delegate */
	FInputActionHandlerWithKeySignature& GetDelegateWithKeyForManualSet()
	{
		FuncDynDelegate.Unbind();
		FuncDelegate.Unbind();
		return FuncDelegateWithKey;
	}

	/** Unbinds any bound delegates */
	inline void Unbind()
	{
		FuncDelegate.Unbind();
		FuncDelegateWithKey.Unbind();
		FuncDynDelegate.Unbind();
	}

	/** Execute function for the action unified delegate. */
	inline void Execute(const FKey Key) const
	{
		if (FuncDelegate.IsBound())
		{
			FuncDelegate.Execute();
		}
		else if (FuncDelegateWithKey.IsBound())
		{
			FuncDelegateWithKey.Execute(Key);
		}
		else if (FuncDynDelegate.IsBound())
		{
			FuncDynDelegate.Execute(Key);
		}
	}
protected:
	/** Holds the delegate to call. */
	FInputActionHandlerSignature FuncDelegate;
	/** Holds the delegate that wants to know the key to call. */
	FInputActionHandlerWithKeySignature FuncDelegateWithKey;
	/** Holds the dynamic delegate to call. */
	FInputActionHandlerDynamicSignature FuncDynDelegate;
};

/** Binds a delegate to an action. */
struct FInputActionBinding : public FInputBinding
{
	/** Friendly name of action, e.g "jump" */
	FName ActionName;

	/** Key event to bind it to, e.g. pressed, released, double click */
	TEnumAsByte<EInputEvent> KeyEvent;

	/** Whether the binding is part of a paired (both pressed and released events bound) action */
	uint32 bPaired:1;

	/** The delegate bound to the action */
	FInputActionUnifiedDelegate ActionDelegate;

	FInputActionBinding()
		: FInputBinding()
		, ActionName(NAME_None)
		, KeyEvent(EInputEvent::IE_Pressed)
		, bPaired(false)
	{ }

	FInputActionBinding(const FName InActionName, const enum EInputEvent InKeyEvent)
		: FInputBinding()
		, ActionName(InActionName)
		, KeyEvent(InKeyEvent)
		, bPaired(false)
	{ }
};

/** Binds a delegate to a key chord. */
struct FInputKeyBinding : public FInputBinding
{
	/** Input Chord to bind to */
	FInputChord Chord;

	/** Key event to bind it to (e.g. pressed, released, double click) */
	TEnumAsByte<EInputEvent> KeyEvent;

	/** The delegate bound to the key chord */
	FInputActionUnifiedDelegate KeyDelegate;

	FInputKeyBinding()
		: FInputBinding()
		, KeyEvent(EInputEvent::IE_Pressed)
	{ }

	FInputKeyBinding(const FInputChord InChord, const enum EInputEvent InKeyEvent)
		: FInputBinding()
		, Chord(InChord)
		, KeyEvent(InKeyEvent)
	{ }
};


/** 
 * Delegate signature for touch handlers. 
 * @FingerIndex: Which finger touched
 * @Location: The 2D screen location that was touched
 */
DECLARE_DELEGATE_TwoParams( FInputTouchHandlerSignature, ETouchIndex::Type, FVector );
DECLARE_DYNAMIC_DELEGATE_TwoParams( FInputTouchHandlerDynamicSignature, ETouchIndex::Type, FingerIndex, FVector, Location );

/** Unified delegate specialization for Touch events. */
struct FInputTouchUnifiedDelegate : public TInputUnifiedDelegate<FInputTouchHandlerSignature, FInputTouchHandlerDynamicSignature>
{
	/** Execute function for the touch unified delegate. */
	inline void Execute(const ETouchIndex::Type FingerIndex, const FVector Location) const
	{
		if (FuncDelegate.IsBound())
		{
			FuncDelegate.Execute(FingerIndex, Location);
		}
		else if (FuncDynDelegate.IsBound())
		{
			FuncDynDelegate.Execute(FingerIndex, Location);
		}
	}
};

/** Binds a delegate to touch input. */
struct FInputTouchBinding : public FInputBinding
{
	/** Key event to bind it to (e.g. pressed, released, double click) */
	TEnumAsByte<EInputEvent> KeyEvent;

	/** The delegate bound to the touch events */
	FInputTouchUnifiedDelegate TouchDelegate;

	FInputTouchBinding()
		: FInputBinding()
		, KeyEvent(EInputEvent::IE_Pressed)
	{ }

	FInputTouchBinding(const enum EInputEvent InKeyEvent)
		: FInputBinding()
		, KeyEvent(InKeyEvent)
	{ }
};


/** 
 * Delegate signature for axis handlers. 
 * @AxisValue: "Value" to pass to the axis.  This value will be the device-dependent, so a mouse will report absolute change since the last update, 
 *		a joystick will report total displacement from the center, etc.  It is up to the handler to interpret this data as it sees fit, i.e. treating 
 *		joystick values as a rate of change would require scaling by frametime to get an absolute delta.
 */
DECLARE_DELEGATE_OneParam( FInputAxisHandlerSignature, float );
DECLARE_DYNAMIC_DELEGATE_OneParam( FInputAxisHandlerDynamicSignature, float, AxisValue );

/** Unified delegate specialization for float axis events. */
struct FInputAxisUnifiedDelegate : public TInputUnifiedDelegate<FInputAxisHandlerSignature, FInputAxisHandlerDynamicSignature>
{
	/** Execute function for the axis unified delegate. */
	inline void Execute(const float AxisValue) const
	{
		if (FuncDelegate.IsBound())
		{
			FuncDelegate.Execute(AxisValue);
		}
		else if (FuncDynDelegate.IsBound())
		{
			FuncDynDelegate.Execute(AxisValue);
		}
	}
};


/** Binds a delegate to an axis mapping. */
struct FInputAxisBinding : public FInputBinding
{
	/** The axis mapping being bound to. */
	FName AxisName;

	/** 
	 * The delegate bound to the axis. 
	 * It will be called each frame that the input component is in the input stack 
	 * regardless of whether the value is non-zero or has changed.
	 */
	FInputAxisUnifiedDelegate AxisDelegate;

	/** 
	 * The value of the axis as calculated during the most recent UPlayerInput::ProcessInputStack
	 * if the InputComponent was in the stack, otherwise all values should be 0.
	 */
	float AxisValue;

	FInputAxisBinding()
		: FInputBinding()
		, AxisName(NAME_None)
		, AxisValue(0.f)
	{ }

	FInputAxisBinding(const FName InAxisName)
		: FInputBinding()
		, AxisName(InAxisName)
		, AxisValue(0.f)
	{ }
};


/** Binds a delegate to a raw float axis mapping. */
struct FInputAxisKeyBinding : public FInputBinding
{
	/** The axis being bound to. */
	FKey AxisKey;

	/** 
	 * The delegate bound to the axis. 
	 * It will be called each frame that the input component is in the input stack 
	 * regardless of whether the value is non-zero or has changed.
	 */
	FInputAxisUnifiedDelegate AxisDelegate;

	/** 
	 * The value of the axis as calculated during the most recent UPlayerInput::ProcessInputStack
	 * if the InputComponent containing the binding was in the stack, otherwise the value will be 0.
	 */
	float AxisValue;

	FInputAxisKeyBinding()
		: FInputBinding()
		, AxisValue(0.f)
	{ }

	FInputAxisKeyBinding(const FKey InAxisKey)
		: FInputBinding()
		, AxisKey(InAxisKey)
		, AxisValue(0.f)
	{
		ensure(AxisKey.IsFloatAxis());
	}
};


/**
* Delegate signature for vector axis handlers.
* @AxisValue: "Value" to pass to the axis.
*/
DECLARE_DELEGATE_OneParam(FInputVectorAxisHandlerSignature, FVector);
DECLARE_DYNAMIC_DELEGATE_OneParam(FInputVectorAxisHandlerDynamicSignature, FVector, AxisValue);

/** Unified delegate specialization for vector axis events. */
struct FInputVectorAxisUnifiedDelegate : public TInputUnifiedDelegate<FInputVectorAxisHandlerSignature, FInputVectorAxisHandlerDynamicSignature>
{
	/** Execute function for the axis unified delegate. */
	inline void Execute(const FVector AxisValue) const
	{
		if (FuncDelegate.IsBound())
		{
			FuncDelegate.Execute(AxisValue);
		}
		else if (FuncDynDelegate.IsBound())
		{
			FuncDynDelegate.Execute(AxisValue);
		}
	}
};

/** Binds a delegate to a raw vector axis mapping. */
struct FInputVectorAxisBinding : public FInputBinding
{
	/** The axis being bound to. */
	FKey AxisKey;

	/** 
	 * The delegate bound to the axis. 
	 * It will be called each frame that the input component is in the input stack 
	 * regardless of whether the value is non-zero or has changed.
	 */
	FInputVectorAxisUnifiedDelegate AxisDelegate;

	/** 
	 * The value of the axis as calculated during the most recent UPlayerInput::ProcessInputStack
	 * if the InputComponent containing the binding was in the stack, otherwise the value will be (0,0,0).
	 */
	FVector AxisValue;

	FInputVectorAxisBinding()
		: FInputBinding()
	{ }

	FInputVectorAxisBinding(const FKey InAxisKey)
		: FInputBinding()
		, AxisKey(InAxisKey)
	{
		ensure(AxisKey.IsVectorAxis());
	}
};


/** 
 * Delegate signature for gesture handlers. 
 * @Value: "Value" to pass to the axis.  Note that by convention this is assumed to be a framerate-independent "delta" value, i.e. absolute change for this frame
 *				so the handler need not scale by frametime.
 */
DECLARE_DELEGATE_OneParam( FInputGestureHandlerSignature, float );
DECLARE_DYNAMIC_DELEGATE_OneParam( FInputGestureHandlerDynamicSignature, float, Value );

/** Unified delegate specialization for gestureevents. */
struct FInputGestureUnifiedDelegate : public TInputUnifiedDelegate<FInputGestureHandlerSignature, FInputGestureHandlerDynamicSignature>
{
	/** Execute function for the gesture unified delegate. */
	inline void Execute(const float Value) const
	{
		if (FuncDelegate.IsBound())
		{
			FuncDelegate.Execute(Value);
		}
		else if (FuncDynDelegate.IsBound())
		{
			FuncDynDelegate.Execute(Value);
		}
	}
};


/** Binds a gesture to a function. */
struct FInputGestureBinding : public FInputBinding
{
	/** The gesture being bound to. */
	FKey GestureKey;

	/** The delegate bound to the gesture events */
	FInputGestureUnifiedDelegate GestureDelegate;

	/** Value parameter, meaning is dependent on the gesture. */
	float GestureValue;

	FInputGestureBinding()
		: FInputBinding()
		, GestureValue(0.f)
	{ }

	FInputGestureBinding(const FKey InGestureKey)
		: FInputBinding()
		, GestureKey(InGestureKey)
		, GestureValue(0.f)
	{ }
};



UENUM()
namespace EControllerAnalogStick
{
	enum Type
	{
		CAS_LeftStick,
		CAS_RightStick,
		CAS_MAX
	};
}


/**
 * Implement an Actor component for input bindings.
 *
 * An Input Component is a transient component that enables an Actor to bind various forms of input events to delegate functions.  
 * Input components are processed from a stack managed by the PlayerController and processed by the PlayerInput.
 * Each binding can consume the input event preventing other components on the input stack from processing the input.
 *
 * @see https://docs.unrealengine.com/latest/INT/Gameplay/Input/index.html
 */
UCLASS(transient, config=Input, hidecategories=(Activation, "Components|Activation"))
class ENGINE_API UInputComponent
	: public UActorComponent
{
	GENERATED_UCLASS_BODY()

	/** The collection of key bindings. */
	TArray<struct FInputKeyBinding> KeyBindings;

	/** The collection of touch bindings. */
	TArray<struct FInputTouchBinding> TouchBindings;

	/** The collection of axis bindings. */
	TArray<struct FInputAxisBinding> AxisBindings;

	/** The collection of axis key bindings. */
	TArray<struct FInputAxisKeyBinding> AxisKeyBindings;

	/** The collection of vector axis bindings. */
	TArray<struct FInputVectorAxisBinding> VectorAxisBindings;

	/** The collection of gesture bindings. */
	TArray<struct FInputGestureBinding> GestureBindings;

	/** The priority of this input component when pushed in to the stack. */
	int32 Priority;

	/** Whether any components lower on the input stack should be allowed to receive input. */
	uint32 bBlockInput:1;

public:

	/**
	 * Gets the current value of the axis with the specified name.
	 *
	 * @param AxisName The name of the axis.
	 * @return Axis value.
	 * @see GetAxisKeyValue, GetVectorAxisValue
	 */
	float GetAxisValue( const FName AxisName ) const;

	/**
	 * Gets the current value of the axis with the specified key.
	 *
	 * @param AxisKey The key of the axis.
	 * @return Axis value.
	 * @see GetAxisKeyValue, GetVectorAxisValue
	 */
	float GetAxisKeyValue( const FKey AxisKey ) const;

	/**
	 * Gets the current vector value of the axis with the specified key.
	 *
	 * @param AxisKey The key of the axis.
	 * @return Axis value.
	 * @see GetAxisValue, GetAxisKeyValue
	 */
	FVector GetVectorAxisValue( const FKey AxisKey ) const;

	/**
	 * Checks whether this component has any input bindings.
	 *
	 * @return true if any bindings are set, false otherwise.
	 */
	bool HasBindings( ) const;

	/**
	 * Adds the specified action binding.
	 *
	 * @param Binding The binding to add.
	 * @return The last binding in the list.
	 * @see ClearActionBindings, GetActionBinding, GetNumActionBindings, RemoveActionBinding
	 */
	FInputActionBinding& AddActionBinding( const FInputActionBinding& Binding );

	/**
	 * Removes all action bindings.
	 *
	 * @see AddActionBinding, GetActionBinding, GetNumActionBindings, RemoveActionBinding
	 */
	void ClearActionBindings( );

	/**
	 * Gets the action binding with the specified index.
	 *
	 * @param BindingIndex The index of the binding to get.
	 * @see AddActionBinding, ClearActionBindings, GetNumActionBindings, RemoveActionBinding
	 */
	FInputActionBinding& GetActionBinding(const int32 BindingIndex) { return ActionBindings[BindingIndex]; }

	/**
	 * Gets the number of action bindings.
	 *
	 * @return Number of bindings.
	 * @see AddActionBinding, ClearActionBindings, GetActionBinding, RemoveActionBinding
	 */
	int32 GetNumActionBindings() const { return ActionBindings.Num(); }

	/**
	 * Removes the action binding at the specified index.
	 *
	 * @param BindingIndex The index of the binding to remove.
	 * @see AddActionBinding, ClearActionBindings, GetActionBinding, GetNumActionBindings
	 */
	void RemoveActionBinding( const int32 BindingIndex );

	/** Clears all cached binding values. */
	void ClearBindingValues();

public:

	/**
	 * Binds a delegate function to an Action defined in the project settings.
	 * Returned reference is only guaranteed to be valid until another action is bound.
	 */
	template<class UserClass>
	FInputActionBinding& BindAction( const FName ActionName, const EInputEvent KeyEvent, UserClass* Object, typename FInputActionHandlerSignature::TUObjectMethodDelegate< UserClass >::FMethodPtr Func )
	{
		FInputActionBinding AB( ActionName, KeyEvent );
		AB.ActionDelegate.BindDelegate(Object, Func);
		return AddActionBinding(AB);
	}

	/**
	 * Binds a delegate function to an Action defined in the project settings.
	 * Returned reference is only guaranteed to be valid until another action is bound.
	 */
	template<class UserClass>
	FInputActionBinding& BindAction( const FName ActionName, const EInputEvent KeyEvent, UserClass* Object, typename FInputActionHandlerWithKeySignature::TUObjectMethodDelegate< UserClass >::FMethodPtr Func )
	{
		FInputActionBinding AB( ActionName, KeyEvent );
		AB.ActionDelegate.BindDelegate(Object, Func);
		return AddActionBinding(AB);
	}

	/**
	* Binds a delegate function to an Action defined in the project settings.
	* Returned reference is only guaranteed to be valid until another action is bound.
	*/
	template< class DelegateType, class UserClass, typename... VarTypes >
	FInputActionBinding& BindAction( const FName ActionName, const EInputEvent KeyEvent, UserClass* Object, typename DelegateType::template TUObjectMethodDelegate< UserClass >::FMethodPtr Func, VarTypes... Vars )
	{
		FInputActionBinding AB( ActionName, KeyEvent );
		AB.ActionDelegate.BindDelegate<DelegateType>(Object, Func, Vars...);
		return AddActionBinding(AB);
	}

	/**
	 * Binds a delegate function an Axis defined in the project settings.
	 * Returned reference is only guaranteed to be valid until another axis is bound.
	 */
	template<class UserClass>
	FInputAxisBinding& BindAxis( const FName AxisName, UserClass* Object, typename FInputAxisHandlerSignature::TUObjectMethodDelegate< UserClass >::FMethodPtr Func )
	{
		FInputAxisBinding AB( AxisName );
		AB.AxisDelegate.BindDelegate(Object, Func);
		AxisBindings.Add(AB);
		return AxisBindings.Last();
	}

	/**
	 * Indicates that the InputComponent is interested in knowing the Axis value
	 * (via GetAxisValue) but does not want a delegate function called each frame.
	 * Returned reference is only guaranteed to be valid until another axis is bound.
	 */
	FInputAxisBinding& BindAxis( const FName AxisName )
	{
		FInputAxisBinding AB( AxisName );
		AxisBindings.Add(AB);
		return AxisBindings.Last();
	}

	/**
	 * Binds a delegate function for an axis key (e.g. Mouse X).
	 * Returned reference is only guaranteed to be valid until another axis key is bound.
	 */
	template<class UserClass>
	FInputAxisKeyBinding& BindAxisKey( const FKey AxisKey, UserClass* Object, typename FInputAxisHandlerSignature::TUObjectMethodDelegate< UserClass >::FMethodPtr Func )
	{
		FInputAxisKeyBinding AB(AxisKey);
		AB.AxisDelegate.BindDelegate(Object, Func);
		AxisKeyBindings.Add(AB);
		return AxisKeyBindings.Last();
	}

	/**
	 * Indicates that the InputComponent is interested in knowing/consuming an axis key's
	 * value (via GetAxisKeyValue) but does not want a delegate function called each frame.
	 * Returned reference is only guaranteed to be valid until another axis key is bound.
	 */
	FInputAxisKeyBinding& BindAxisKey( const FKey AxisKey )
	{
		FInputAxisKeyBinding AB(AxisKey);
		AxisKeyBindings.Add(AB);
		return AxisKeyBindings.Last();
	}

	/**
	 * Binds a delegate function to a vector axis key (e.g. Tilt)
	 * Returned reference is only guaranteed to be valid until another vector axis key is bound.
	 */
	template<class UserClass>
	FInputVectorAxisBinding& BindVectorAxis( const FKey AxisKey, UserClass* Object, typename FInputVectorAxisHandlerSignature::TUObjectMethodDelegate< UserClass >::FMethodPtr Func )
	{
		FInputVectorAxisBinding AB(AxisKey);
		AB.AxisDelegate.BindDelegate(Object, Func);
		VectorAxisBindings.Add(AB);
		return VectorAxisBindings.Last();
	}

	/**
	 * Indicates that the InputComponent is interested in knowing/consuming a vector axis key's
	 * value (via GetVectorAxisKeyValue) but does not want a delegate function called each frame.
	 * Returned reference is only guaranteed to be valid until another vector axis key is bound.
	 */
	FInputVectorAxisBinding& BindVectorAxis( const FKey AxisKey )
	{
		FInputVectorAxisBinding AB(AxisKey);
		VectorAxisBindings.Add(AB);
		return VectorAxisBindings.Last();
	}

	/**
	 * Binds a chord event to a delegate function.
	 * Returned reference is only guaranteed to be valid until another input key is bound.
	 */
	template<class UserClass>
	FInputKeyBinding& BindKey( const FInputChord Chord, const EInputEvent KeyEvent, UserClass* Object, typename FInputActionHandlerSignature::TUObjectMethodDelegate< UserClass >::FMethodPtr Func )
	{
		FInputKeyBinding KB(Chord, KeyEvent);
		KB.KeyDelegate.BindDelegate(Object, Func);
		KeyBindings.Add(KB);
		return KeyBindings.Last();
	}

	/**
	 * Binds a key event to a delegate function.
	 * Returned reference is only guaranteed to be valid until another input key is bound.
	 */
	template<class UserClass>
	FInputKeyBinding& BindKey( const FKey Key, const EInputEvent KeyEvent, UserClass* Object, typename FInputActionHandlerSignature::TUObjectMethodDelegate< UserClass >::FMethodPtr Func )
	{
		return BindKey(FInputChord(Key, false, false, false, false), KeyEvent, Object, Func);
	}

	/**
	 * Binds this input component to touch events.
	 * Returned reference is only guaranteed to be valid until another touch event is bound.
	 */
	template<class UserClass>
	FInputTouchBinding& BindTouch( const EInputEvent KeyEvent, UserClass* Object, typename FInputTouchHandlerSignature::TUObjectMethodDelegate< UserClass >::FMethodPtr Func )
	{
		FInputTouchBinding TB(KeyEvent);
		TB.TouchDelegate.BindDelegate(Object, Func);
		TouchBindings.Add(TB);
		return TouchBindings.Last();
	}

	/**
	 * Binds a gesture event to a delegate function.
	 * Returned reference is only guaranteed to be valid until another gesture event is bound.
	 */
	template<class UserClass>
	FInputGestureBinding& BindGesture( const FKey GestureKey, UserClass* Object, typename FInputGestureHandlerSignature::TUObjectMethodDelegate< UserClass >::FMethodPtr Func )
	{
		FInputGestureBinding GB(GestureKey);
		GB.GestureDelegate.BindDelegate(Object, Func);
		GestureBindings.Add(GB);
		return GestureBindings.Last();
	}

private:

	/** Returns true if the given key/button is pressed on the input of the controller (if present) */
	UFUNCTION(BlueprintCallable, meta=(DeprecatedFunction, DeprecationMessage="Use PlayerController.IsInputKeyDown instead."))
	bool IsControllerKeyDown(FKey Key) const;

	/** Returns true if the given key/button was up last frame and down this frame. */
	UFUNCTION(BlueprintCallable, meta=(DeprecatedFunction, DeprecationMessage="Use PlayerController.WasInputKeyJustPressed instead."))
	bool WasControllerKeyJustPressed(FKey Key) const;

	/** Returns true if the given key/button was down last frame and up this frame. */
	UFUNCTION(BlueprintCallable, meta=(DeprecatedFunction, DeprecationMessage="Use PlayerController.WasInputKeyJustReleased instead."))
	bool WasControllerKeyJustReleased(FKey Key) const;

	/** Returns the analog value for the given key/button.  If analog isn't supported, returns 1 for down and 0 for up. */
	UFUNCTION(BlueprintCallable, meta=(DeprecatedFunction, DeprecationMessage="Use PlayerController.GetInputAnalogKeyState instead."))
	float GetControllerAnalogKeyState(FKey Key) const;

	/** Returns the vector value for the given key/button. */
	UFUNCTION(BlueprintCallable, meta=(DeprecatedFunction, DeprecationMessage="Use PlayerController.GetInputVectorKeyState instead."))
	FVector GetControllerVectorKeyState(FKey Key) const;

	/** Returns the location of a touch, and if it's held down */
	UFUNCTION(BlueprintCallable, meta=(DeprecatedFunction, DeprecationMessage="Use PlayerController.GetInputTouchState instead."))
	void GetTouchState(int32 FingerIndex, float& LocationX, float& LocationY, bool& bIsCurrentlyPressed) const;

	/** Returns how long the given key/button has been down.  Returns 0 if it's up or it just went down this frame. */
	UFUNCTION(BlueprintCallable, meta=(DeprecatedFunction, DeprecationMessage="Use PlayerController.GetInputKeyTimeDown instead."))
	float GetControllerKeyTimeDown(FKey Key) const;

	/** Retrieves how far the mouse moved this frame. */
	UFUNCTION(BlueprintCallable, meta=(DeprecatedFunction, DeprecationMessage="Use PlayerController.GetInputMouseDelta instead."))
	void GetControllerMouseDelta(float& DeltaX, float& DeltaY) const;

	/** Retrieves the X and Y displacement of the given analog stick.  For WhickStick, 0 = left, 1 = right. */
	UFUNCTION(BlueprintCallable, meta=(DeprecatedFunction, DeprecationMessage="Use PlayerController.GetInputAnalogStickState instead."))
	void GetControllerAnalogStickState(EControllerAnalogStick::Type WhichStick, float& StickX, float& StickY) const;

private:

	/** Holds the collection of action bindings. */
	TArray<struct FInputActionBinding> ActionBindings;
};
