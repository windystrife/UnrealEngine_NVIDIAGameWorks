// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnrealNetwork.h: Unreal networking.
=============================================================================*/

/*-----------------------------------------------------------------------------
	Forward declarations.
-----------------------------------------------------------------------------*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "UObject/CoreNet.h"
#include "EngineLogs.h"
#include "UObject/UnrealType.h"

class AActor;

/*class	UNetDriver;*/
class	UNetConnection;
class	UPendingNetGame;

/*-----------------------------------------------------------------------------
	Types.
-----------------------------------------------------------------------------*/

// Return the value of Max/2 <= Value-Reference+some_integer*Max < Max/2.
inline int32 BestSignedDifference( int32 Value, int32 Reference, int32 Max )
{
	return ((Value-Reference+Max/2) & (Max-1)) - Max/2;
}
inline int32 MakeRelative( int32 Value, int32 Reference, int32 Max )
{
	return Reference + BestSignedDifference(Value,Reference,Max);
}

DECLARE_MULTICAST_DELEGATE_OneParam(FPreActorDestroyReplayScrub, AActor*);

DECLARE_MULTICAST_DELEGATE_OneParam(FPreReplayScrub, UWorld*);

struct ENGINE_API FNetworkReplayDelegates
{
	/** global delegate called one time prior to scrubbing */
	static FPreReplayScrub OnPreScrub;
};

/*-----------------------------------------------------------------------------
	Replication.
-----------------------------------------------------------------------------*/

/** wrapper to find replicated properties that also makes sure they're valid */
static UProperty* GetReplicatedProperty(UClass* CallingClass, UClass* PropClass, const FName& PropName)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!CallingClass->IsChildOf(PropClass))
	{
		UE_LOG(LogNet, Fatal,TEXT("Attempt to replicate property '%s.%s' in C++ but class '%s' is not a child of '%s'"), *PropClass->GetName(), *PropName.ToString(), *CallingClass->GetName(), *PropClass->GetName());
	}
#endif
	UProperty* TheProperty = FindFieldChecked<UProperty>(PropClass, PropName);
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	if (!(TheProperty->PropertyFlags & CPF_Net))
	{
		UE_LOG(LogNet, Fatal,TEXT("Attempt to replicate property '%s' that was not tagged to replicate! Please use 'Replicated' or 'ReplicatedUsing' keyword in the UPROPERTY() declaration."), *TheProperty->GetFullName());
	}
#endif
	return TheProperty;
}

#define DOREPLIFETIME(c,v) \
{ \
	static UProperty* sp##v = GetReplicatedProperty(StaticClass(), c::StaticClass(),GET_MEMBER_NAME_CHECKED(c,v)); \
	for ( int32 i = 0; i < sp##v->ArrayDim; i++ )							\
	{																		\
		OutLifetimeProps.AddUnique( FLifetimeProperty( sp##v->RepIndex + i ) );	\
	}																		\
}

/** This macro is used by nativized code (DynamicClasses), so the Property may be recreated. */
#define DOREPLIFETIME_DIFFNAMES(c,v, n) \
{ \
	static TWeakObjectPtr<UProperty> __swp##v{};							\
	const UProperty* sp##v = __swp##v.Get();								\
	if (nullptr == sp##v)													\
	{																		\
		sp##v = GetReplicatedProperty(StaticClass(), c::StaticClass(), n);	\
		__swp##v = sp##v;													\
	}																		\
	for ( int32 i = 0; i < sp##v->ArrayDim; i++ )							\
	{																		\
		OutLifetimeProps.AddUnique( FLifetimeProperty( sp##v->RepIndex + i ) );	\
	}																		\
}

#define DOREPLIFETIME_CONDITION(c,v,cond) \
{ \
	static UProperty* sp##v = GetReplicatedProperty(StaticClass(), c::StaticClass(),GET_MEMBER_NAME_CHECKED(c,v)); \
	for ( int32 i = 0; i < sp##v->ArrayDim; i++ )										\
	{																					\
		OutLifetimeProps.AddUnique( FLifetimeProperty( sp##v->RepIndex + i, cond ) );	\
	}																					\
}

/** Allows gamecode to specify RepNotify condition: REPNOTIFY_OnChanged (default) or REPNOTIFY_Always for when repnotify function is called  */
#define DOREPLIFETIME_CONDITION_NOTIFY(c,v,cond, rncond) \
{ \
	static UProperty* sp##v = GetReplicatedProperty(StaticClass(), c::StaticClass(),GET_MEMBER_NAME_CHECKED(c,v)); \
	for ( int32 i = 0; i < sp##v->ArrayDim; i++ )										\
	{																					\
		OutLifetimeProps.AddUnique( FLifetimeProperty( sp##v->RepIndex + i, cond, rncond) );	\
	}																					\
}

#define DOREPLIFETIME_ACTIVE_OVERRIDE(c,v,active)	\
{													\
	static UProperty* sp##v = GetReplicatedProperty(StaticClass(), c::StaticClass(),GET_MEMBER_NAME_CHECKED(c,v)); \
	for ( int32 i = 0; i < sp##v->ArrayDim; i++ )											\
	{																						\
		ChangedPropertyTracker.SetCustomIsActiveOverride( sp##v->RepIndex + i, active );	\
	}																						\
}

#define DOREPLIFETIME_CHANGE_CONDITION(c,v,cond) \
{ \
	static UProperty* sp##v = GetReplicatedProperty(StaticClass(), c::StaticClass(),GET_MEMBER_NAME_CHECKED(c,v));	\
	bool bFound = false;																							\
	for ( int32 i = 0; i < OutLifetimeProps.Num(); i++ )															\
	{																												\
		if ( OutLifetimeProps[i].RepIndex == sp##v->RepIndex )														\
		{																											\
			for ( int32 j = 0; j < sp##v->ArrayDim; j++ )															\
			{																										\
				OutLifetimeProps[i + j].Condition = cond;															\
			}																										\
			bFound = true;																							\
			break;																									\
		}																											\
	}																												\
	check( bFound );																								\
}

/*-----------------------------------------------------------------------------
	RPC Parameter Validation Helpers
-----------------------------------------------------------------------------*/

// This macro is for RPC parameter validation.
// It handles the details of what should happen if a validation expression fails
#define RPC_VALIDATE( expression )						\
	if ( !( expression ) )								\
	{													\
		UE_LOG( LogNet, Warning,						\
		TEXT("RPC_VALIDATE Failed: ")					\
		TEXT( PREPROCESSOR_TO_STRING( expression ) )	\
		TEXT(" File: ")									\
		TEXT( PREPROCESSOR_TO_STRING( __FILE__ ) )		\
		TEXT(" Line: ")									\
		TEXT( PREPROCESSOR_TO_STRING( __LINE__ ) ) );	\
		return false;									\
	}
