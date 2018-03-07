// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

// Only designed to be included directly by Delegate.h
#if !defined( __Delegate_h__ ) || !defined( FUNC_INCLUDING_INLINE_IMPL )
	#error "This inline header must only be included by Delegate.h"
#endif

#pragma once
#include "CoreTypes.h"
#include "Misc/AssertionMacros.h"
#include "Templates/TypeWrapper.h"
#include "Templates/RemoveReference.h"
#include "Templates/UnrealTemplate.h"
#include "Misc/Crc.h"
#include "UObject/NameTypes.h"
#include "UObject/WeakObjectPtrTemplates.h"
#include "Templates/SharedPointer.h"
#include "UObject/ScriptDelegates.h"

class FDelegateBase;
class FDelegateHandle;
class IDelegateInstance;
struct FWeakObjectPtr;
template <typename FuncType> struct IBaseDelegateInstance;
template<typename ObjectPtrType> class FMulticastDelegateBase;

/**
 * Unicast delegate base object.
 *
 * Use the various DECLARE_DELEGATE macros to create the actual delegate type, templated to
 * the function signature the delegate is compatible with. Then, you can create an instance
 * of that class when you want to bind a function to the delegate.
 */
template <typename WrappedRetValType, typename... ParamTypes>
class TBaseDelegate : public FDelegateBase
{
public:
	/** Type definition for return value type. */
	typedef typename TUnwrapType<WrappedRetValType>::Type RetValType;
	typedef RetValType TFuncType(ParamTypes...);

	/** Type definition for the shared interface of delegate instance types compatible with this delegate class. */
	typedef IBaseDelegateInstance<TFuncType> TDelegateInstanceInterface;

	/** Declare the user's "fast" shared pointer-based delegate instance types. */
	template <typename UserClass                                                                            > struct TSPMethodDelegate                 : TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::Fast, TFuncType                                        > { typedef TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::Fast, TFuncType                                        > Super; TSPMethodDelegate                (const TSharedRef< UserClass, ESPMode::Fast >& InUserObject, typename Super::FMethodPtr InMethodPtr                                                            ) : Super(InUserObject, InMethodPtr                        ) {} };
	template <typename UserClass                                                                            > struct TSPMethodDelegate_Const           : TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::Fast, TFuncType                                        > { typedef TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::Fast, TFuncType                                        > Super; TSPMethodDelegate_Const          (const TSharedRef< UserClass, ESPMode::Fast >& InUserObject, typename Super::FMethodPtr InMethodPtr                                                            ) : Super(InUserObject, InMethodPtr                        ) {} };
	template <typename UserClass, typename Var1Type                                                         > struct TSPMethodDelegate_OneVar          : TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::Fast, TFuncType, Var1Type                              > { typedef TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::Fast, TFuncType, Var1Type                              > Super; TSPMethodDelegate_OneVar         (const TSharedRef< UserClass, ESPMode::Fast >& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1                                             ) : Super(InUserObject, InMethodPtr, Var1                  ) {} };
	template <typename UserClass, typename Var1Type                                                         > struct TSPMethodDelegate_OneVar_Const    : TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::Fast, TFuncType, Var1Type                              > { typedef TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::Fast, TFuncType, Var1Type                              > Super; TSPMethodDelegate_OneVar_Const   (const TSharedRef< UserClass, ESPMode::Fast >& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1                                             ) : Super(InUserObject, InMethodPtr, Var1                  ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type                                      > struct TSPMethodDelegate_TwoVars         : TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::Fast, TFuncType, Var1Type, Var2Type                    > { typedef TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::Fast, TFuncType, Var1Type, Var2Type                    > Super; TSPMethodDelegate_TwoVars        (const TSharedRef< UserClass, ESPMode::Fast >& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2                              ) : Super(InUserObject, InMethodPtr, Var1, Var2            ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type                                      > struct TSPMethodDelegate_TwoVars_Const   : TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::Fast, TFuncType, Var1Type, Var2Type                    > { typedef TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::Fast, TFuncType, Var1Type, Var2Type                    > Super; TSPMethodDelegate_TwoVars_Const  (const TSharedRef< UserClass, ESPMode::Fast >& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2                              ) : Super(InUserObject, InMethodPtr, Var1, Var2            ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type                   > struct TSPMethodDelegate_ThreeVars       : TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::Fast, TFuncType, Var1Type, Var2Type, Var3Type          > { typedef TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::Fast, TFuncType, Var1Type, Var2Type, Var3Type          > Super; TSPMethodDelegate_ThreeVars      (const TSharedRef< UserClass, ESPMode::Fast >& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3               ) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3      ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type                   > struct TSPMethodDelegate_ThreeVars_Const : TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::Fast, TFuncType, Var1Type, Var2Type, Var3Type          > { typedef TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::Fast, TFuncType, Var1Type, Var2Type, Var3Type          > Super; TSPMethodDelegate_ThreeVars_Const(const TSharedRef< UserClass, ESPMode::Fast >& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3               ) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3      ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type> struct TSPMethodDelegate_FourVars        : TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::Fast, TFuncType, Var1Type, Var2Type, Var3Type, Var4Type> { typedef TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::Fast, TFuncType, Var1Type, Var2Type, Var3Type, Var4Type> Super; TSPMethodDelegate_FourVars       (const TSharedRef< UserClass, ESPMode::Fast >& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3, Var4) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type> struct TSPMethodDelegate_FourVars_Const  : TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::Fast, TFuncType, Var1Type, Var2Type, Var3Type, Var4Type> { typedef TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::Fast, TFuncType, Var1Type, Var2Type, Var3Type, Var4Type> Super; TSPMethodDelegate_FourVars_Const (const TSharedRef< UserClass, ESPMode::Fast >& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3, Var4) {} };

	/** Declare the user's "thread-safe" shared pointer-based delegate instance types. */
	template <typename UserClass                                                                            > struct TThreadSafeSPMethodDelegate                 : TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, TFuncType                                        > { typedef TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, TFuncType                                        > Super; TThreadSafeSPMethodDelegate                (const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObject, typename Super::FMethodPtr InMethodPtr                                                            ) : Super(InUserObject, InMethodPtr                        ) {} };
	template <typename UserClass                                                                            > struct TThreadSafeSPMethodDelegate_Const           : TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, TFuncType                                        > { typedef TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, TFuncType                                        > Super; TThreadSafeSPMethodDelegate_Const          (const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObject, typename Super::FMethodPtr InMethodPtr                                                            ) : Super(InUserObject, InMethodPtr                        ) {} };
	template <typename UserClass, typename Var1Type                                                         > struct TThreadSafeSPMethodDelegate_OneVar          : TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, TFuncType, Var1Type                              > { typedef TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, TFuncType, Var1Type                              > Super; TThreadSafeSPMethodDelegate_OneVar         (const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1                                             ) : Super(InUserObject, InMethodPtr, Var1                  ) {} };
	template <typename UserClass, typename Var1Type                                                         > struct TThreadSafeSPMethodDelegate_OneVar_Const    : TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, TFuncType, Var1Type                              > { typedef TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, TFuncType, Var1Type                              > Super; TThreadSafeSPMethodDelegate_OneVar_Const   (const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1                                             ) : Super(InUserObject, InMethodPtr, Var1                  ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type                                      > struct TThreadSafeSPMethodDelegate_TwoVars         : TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, TFuncType, Var1Type, Var2Type                    > { typedef TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, TFuncType, Var1Type, Var2Type                    > Super; TThreadSafeSPMethodDelegate_TwoVars        (const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2                              ) : Super(InUserObject, InMethodPtr, Var1, Var2            ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type                                      > struct TThreadSafeSPMethodDelegate_TwoVars_Const   : TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, TFuncType, Var1Type, Var2Type                    > { typedef TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, TFuncType, Var1Type, Var2Type                    > Super; TThreadSafeSPMethodDelegate_TwoVars_Const  (const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2                              ) : Super(InUserObject, InMethodPtr, Var1, Var2            ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type                   > struct TThreadSafeSPMethodDelegate_ThreeVars       : TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, TFuncType, Var1Type, Var2Type, Var3Type          > { typedef TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, TFuncType, Var1Type, Var2Type, Var3Type          > Super; TThreadSafeSPMethodDelegate_ThreeVars      (const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3               ) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3      ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type                   > struct TThreadSafeSPMethodDelegate_ThreeVars_Const : TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, TFuncType, Var1Type, Var2Type, Var3Type          > { typedef TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, TFuncType, Var1Type, Var2Type, Var3Type          > Super; TThreadSafeSPMethodDelegate_ThreeVars_Const(const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3               ) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3      ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type> struct TThreadSafeSPMethodDelegate_FourVars        : TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, TFuncType, Var1Type, Var2Type, Var3Type, Var4Type> { typedef TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, TFuncType, Var1Type, Var2Type, Var3Type, Var4Type> Super; TThreadSafeSPMethodDelegate_FourVars       (const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3, Var4) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type> struct TThreadSafeSPMethodDelegate_FourVars_Const  : TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, TFuncType, Var1Type, Var2Type, Var3Type, Var4Type> { typedef TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, TFuncType, Var1Type, Var2Type, Var3Type, Var4Type> Super; TThreadSafeSPMethodDelegate_FourVars_Const (const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3, Var4) {} };

	/** Declare the user's C++ pointer-based delegate instance types. */
	template <typename UserClass                                                                            > struct TRawMethodDelegate                 : TBaseRawMethodDelegateInstance<false, UserClass, TFuncType                                        > { typedef TBaseRawMethodDelegateInstance<false, UserClass, TFuncType                                        > Super; TRawMethodDelegate                (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr                                                            ) : Super(InUserObject, InMethodPtr                        ) {} };
	template <typename UserClass                                                                            > struct TRawMethodDelegate_Const           : TBaseRawMethodDelegateInstance<true , UserClass, TFuncType                                        > { typedef TBaseRawMethodDelegateInstance<true , UserClass, TFuncType                                        > Super; TRawMethodDelegate_Const          (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr                                                            ) : Super(InUserObject, InMethodPtr                        ) {} };
	template <typename UserClass, typename Var1Type                                                         > struct TRawMethodDelegate_OneVar          : TBaseRawMethodDelegateInstance<false, UserClass, TFuncType, Var1Type                              > { typedef TBaseRawMethodDelegateInstance<false, UserClass, TFuncType, Var1Type                              > Super; TRawMethodDelegate_OneVar         (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1                                             ) : Super(InUserObject, InMethodPtr, Var1                  ) {} };
	template <typename UserClass, typename Var1Type                                                         > struct TRawMethodDelegate_OneVar_Const    : TBaseRawMethodDelegateInstance<true , UserClass, TFuncType, Var1Type                              > { typedef TBaseRawMethodDelegateInstance<true , UserClass, TFuncType, Var1Type                              > Super; TRawMethodDelegate_OneVar_Const   (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1                                             ) : Super(InUserObject, InMethodPtr, Var1                  ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type                                      > struct TRawMethodDelegate_TwoVars         : TBaseRawMethodDelegateInstance<false, UserClass, TFuncType, Var1Type, Var2Type                    > { typedef TBaseRawMethodDelegateInstance<false, UserClass, TFuncType, Var1Type, Var2Type                    > Super; TRawMethodDelegate_TwoVars        (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2                              ) : Super(InUserObject, InMethodPtr, Var1, Var2            ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type                                      > struct TRawMethodDelegate_TwoVars_Const   : TBaseRawMethodDelegateInstance<true , UserClass, TFuncType, Var1Type, Var2Type                    > { typedef TBaseRawMethodDelegateInstance<true , UserClass, TFuncType, Var1Type, Var2Type                    > Super; TRawMethodDelegate_TwoVars_Const  (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2                              ) : Super(InUserObject, InMethodPtr, Var1, Var2            ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type                   > struct TRawMethodDelegate_ThreeVars       : TBaseRawMethodDelegateInstance<false, UserClass, TFuncType, Var1Type, Var2Type, Var3Type          > { typedef TBaseRawMethodDelegateInstance<false, UserClass, TFuncType, Var1Type, Var2Type, Var3Type          > Super; TRawMethodDelegate_ThreeVars      (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3               ) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3      ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type                   > struct TRawMethodDelegate_ThreeVars_Const : TBaseRawMethodDelegateInstance<true , UserClass, TFuncType, Var1Type, Var2Type, Var3Type          > { typedef TBaseRawMethodDelegateInstance<true , UserClass, TFuncType, Var1Type, Var2Type, Var3Type          > Super; TRawMethodDelegate_ThreeVars_Const(UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3               ) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3      ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type> struct TRawMethodDelegate_FourVars        : TBaseRawMethodDelegateInstance<false, UserClass, TFuncType, Var1Type, Var2Type, Var3Type, Var4Type> { typedef TBaseRawMethodDelegateInstance<false, UserClass, TFuncType, Var1Type, Var2Type, Var3Type, Var4Type> Super; TRawMethodDelegate_FourVars       (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3, Var4) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type> struct TRawMethodDelegate_FourVars_Const  : TBaseRawMethodDelegateInstance<true , UserClass, TFuncType, Var1Type, Var2Type, Var3Type, Var4Type> { typedef TBaseRawMethodDelegateInstance<true , UserClass, TFuncType, Var1Type, Var2Type, Var3Type, Var4Type> Super; TRawMethodDelegate_FourVars_Const (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3, Var4) {} };
	
	/** Declare the user's UFunction-based delegate instance types. */
	template <typename UObjectTemplate                                                                            > struct TUFunctionDelegateBinding           : TBaseUFunctionDelegateInstance<UObjectTemplate, TFuncType                                        > { typedef TBaseUFunctionDelegateInstance<UObjectTemplate, TFuncType                                        > Super; TUFunctionDelegateBinding          (UObjectTemplate* InUserObject, const FName& InFunctionName                                                            ) : Super(InUserObject, InFunctionName                        ) {} };
	template <typename UObjectTemplate, typename Var1Type                                                         > struct TUFunctionDelegateBinding_OneVar    : TBaseUFunctionDelegateInstance<UObjectTemplate, TFuncType, Var1Type                              > { typedef TBaseUFunctionDelegateInstance<UObjectTemplate, TFuncType, Var1Type                              > Super; TUFunctionDelegateBinding_OneVar   (UObjectTemplate* InUserObject, const FName& InFunctionName, Var1Type Var1                                             ) : Super(InUserObject, InFunctionName, Var1                  ) {} };
	template <typename UObjectTemplate, typename Var1Type, typename Var2Type                                      > struct TUFunctionDelegateBinding_TwoVars   : TBaseUFunctionDelegateInstance<UObjectTemplate, TFuncType, Var1Type, Var2Type                    > { typedef TBaseUFunctionDelegateInstance<UObjectTemplate, TFuncType, Var1Type, Var2Type                    > Super; TUFunctionDelegateBinding_TwoVars  (UObjectTemplate* InUserObject, const FName& InFunctionName, Var1Type Var1, Var2Type Var2                              ) : Super(InUserObject, InFunctionName, Var1, Var2            ) {} };
	template <typename UObjectTemplate, typename Var1Type, typename Var2Type, typename Var3Type                   > struct TUFunctionDelegateBinding_ThreeVars : TBaseUFunctionDelegateInstance<UObjectTemplate, TFuncType, Var1Type, Var2Type, Var3Type          > { typedef TBaseUFunctionDelegateInstance<UObjectTemplate, TFuncType, Var1Type, Var2Type, Var3Type          > Super; TUFunctionDelegateBinding_ThreeVars(UObjectTemplate* InUserObject, const FName& InFunctionName, Var1Type Var1, Var2Type Var2, Var3Type Var3               ) : Super(InUserObject, InFunctionName, Var1, Var2, Var3      ) {} };
	template <typename UObjectTemplate, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type> struct TUFunctionDelegateBinding_FourVars  : TBaseUFunctionDelegateInstance<UObjectTemplate, TFuncType, Var1Type, Var2Type, Var3Type, Var4Type> { typedef TBaseUFunctionDelegateInstance<UObjectTemplate, TFuncType, Var1Type, Var2Type, Var3Type, Var4Type> Super; TUFunctionDelegateBinding_FourVars (UObjectTemplate* InUserObject, const FName& InFunctionName, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4) : Super(InUserObject, InFunctionName, Var1, Var2, Var3, Var4) {} };

	/** Declare the user's UObject-based delegate instance types. */
	template <typename UserClass                                                                            > struct TUObjectMethodDelegate                 : TBaseUObjectMethodDelegateInstance<false, UserClass, TFuncType                                        > { typedef TBaseUObjectMethodDelegateInstance<false, UserClass, TFuncType                                        > Super; TUObjectMethodDelegate                (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr                                                            ) : Super(InUserObject, InMethodPtr                        ) {} };
	template <typename UserClass                                                                            > struct TUObjectMethodDelegate_Const           : TBaseUObjectMethodDelegateInstance<true , UserClass, TFuncType                                        > { typedef TBaseUObjectMethodDelegateInstance<true , UserClass, TFuncType                                        > Super; TUObjectMethodDelegate_Const          (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr                                                            ) : Super(InUserObject, InMethodPtr                        ) {} };
	template <typename UserClass, typename Var1Type                                                         > struct TUObjectMethodDelegate_OneVar          : TBaseUObjectMethodDelegateInstance<false, UserClass, TFuncType, Var1Type                              > { typedef TBaseUObjectMethodDelegateInstance<false, UserClass, TFuncType, Var1Type                              > Super; TUObjectMethodDelegate_OneVar         (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1                                             ) : Super(InUserObject, InMethodPtr, Var1                  ) {} };
	template <typename UserClass, typename Var1Type                                                         > struct TUObjectMethodDelegate_OneVar_Const    : TBaseUObjectMethodDelegateInstance<true , UserClass, TFuncType, Var1Type                              > { typedef TBaseUObjectMethodDelegateInstance<true , UserClass, TFuncType, Var1Type                              > Super; TUObjectMethodDelegate_OneVar_Const   (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1                                             ) : Super(InUserObject, InMethodPtr, Var1                  ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type                                      > struct TUObjectMethodDelegate_TwoVars         : TBaseUObjectMethodDelegateInstance<false, UserClass, TFuncType, Var1Type, Var2Type                    > { typedef TBaseUObjectMethodDelegateInstance<false, UserClass, TFuncType, Var1Type, Var2Type                    > Super; TUObjectMethodDelegate_TwoVars        (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2                              ) : Super(InUserObject, InMethodPtr, Var1, Var2            ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type                                      > struct TUObjectMethodDelegate_TwoVars_Const   : TBaseUObjectMethodDelegateInstance<true , UserClass, TFuncType, Var1Type, Var2Type                    > { typedef TBaseUObjectMethodDelegateInstance<true , UserClass, TFuncType, Var1Type, Var2Type                    > Super; TUObjectMethodDelegate_TwoVars_Const  (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2                              ) : Super(InUserObject, InMethodPtr, Var1, Var2            ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type                   > struct TUObjectMethodDelegate_ThreeVars       : TBaseUObjectMethodDelegateInstance<false, UserClass, TFuncType, Var1Type, Var2Type, Var3Type          > { typedef TBaseUObjectMethodDelegateInstance<false, UserClass, TFuncType, Var1Type, Var2Type, Var3Type          > Super; TUObjectMethodDelegate_ThreeVars      (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3               ) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3      ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type                   > struct TUObjectMethodDelegate_ThreeVars_Const : TBaseUObjectMethodDelegateInstance<true , UserClass, TFuncType, Var1Type, Var2Type, Var3Type          > { typedef TBaseUObjectMethodDelegateInstance<true , UserClass, TFuncType, Var1Type, Var2Type, Var3Type          > Super; TUObjectMethodDelegate_ThreeVars_Const(UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3               ) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3      ) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type> struct TUObjectMethodDelegate_FourVars        : TBaseUObjectMethodDelegateInstance<false, UserClass, TFuncType, Var1Type, Var2Type, Var3Type, Var4Type> { typedef TBaseUObjectMethodDelegateInstance<false, UserClass, TFuncType, Var1Type, Var2Type, Var3Type, Var4Type> Super; TUObjectMethodDelegate_FourVars       (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3, Var4) {} };
	template <typename UserClass, typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type> struct TUObjectMethodDelegate_FourVars_Const  : TBaseUObjectMethodDelegateInstance<true , UserClass, TFuncType, Var1Type, Var2Type, Var3Type, Var4Type> { typedef TBaseUObjectMethodDelegateInstance<true , UserClass, TFuncType, Var1Type, Var2Type, Var3Type, Var4Type> Super; TUObjectMethodDelegate_FourVars_Const (UserClass* InUserObject, typename Super::FMethodPtr InMethodPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4) : Super(InUserObject, InMethodPtr, Var1, Var2, Var3, Var4) {} };
	
	/** Declare the user's static function pointer delegate instance types. */
	                                                                                      struct FStaticDelegate           : TBaseStaticDelegateInstance<TFuncType                                        > { typedef TBaseStaticDelegateInstance<TFuncType                                        > Super; FStaticDelegate          ( typename Super::FFuncPtr InFuncPtr                                                            ) : Super(InFuncPtr                        ) {} };
	template <typename Var1Type                                                         > struct TStaticDelegate_OneVar    : TBaseStaticDelegateInstance<TFuncType, Var1Type                              > { typedef TBaseStaticDelegateInstance<TFuncType, Var1Type                              > Super; TStaticDelegate_OneVar   ( typename Super::FFuncPtr InFuncPtr, Var1Type Var1                                             ) : Super(InFuncPtr, Var1                  ) {} };
	template <typename Var1Type, typename Var2Type                                      > struct TStaticDelegate_TwoVars   : TBaseStaticDelegateInstance<TFuncType, Var1Type, Var2Type                    > { typedef TBaseStaticDelegateInstance<TFuncType, Var1Type, Var2Type                    > Super; TStaticDelegate_TwoVars  ( typename Super::FFuncPtr InFuncPtr, Var1Type Var1, Var2Type Var2                              ) : Super(InFuncPtr, Var1, Var2            ) {} };
	template <typename Var1Type, typename Var2Type, typename Var3Type                   > struct TStaticDelegate_ThreeVars : TBaseStaticDelegateInstance<TFuncType, Var1Type, Var2Type, Var3Type          > { typedef TBaseStaticDelegateInstance<TFuncType, Var1Type, Var2Type, Var3Type          > Super; TStaticDelegate_ThreeVars( typename Super::FFuncPtr InFuncPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3               ) : Super(InFuncPtr, Var1, Var2, Var3      ) {} };
	template <typename Var1Type, typename Var2Type, typename Var3Type, typename Var4Type> struct TStaticDelegate_FourVars  : TBaseStaticDelegateInstance<TFuncType, Var1Type, Var2Type, Var3Type, Var4Type> { typedef TBaseStaticDelegateInstance<TFuncType, Var1Type, Var2Type, Var3Type, Var4Type> Super; TStaticDelegate_FourVars ( typename Super::FFuncPtr InFuncPtr, Var1Type Var1, Var2Type Var2, Var3Type Var3, Var4Type Var4) : Super(InFuncPtr, Var1, Var2, Var3, Var4) {} };

public:

	/**
	 * Static: Creates a raw C++ pointer global function delegate
	 */
	template <typename... VarTypes>
	FUNCTION_CHECK_RETURN_START
	inline static TBaseDelegate<RetValType, ParamTypes...> CreateStatic(typename TIdentity<RetValType (*)(ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	FUNCTION_CHECK_RETURN_END
	{
		TBaseDelegate<RetValType, ParamTypes...> Result;
		TBaseStaticDelegateInstance<TFuncType, VarTypes...>::Create(Result, InFunc, Vars...);
		return Result;
	}

	/**
	 * Static: Creates a C++ lambda delegate
	 * technically this works for any functor types, but lambdas are the primary use case
	 */
	template<typename FunctorType, typename... VarTypes>
	FUNCTION_CHECK_RETURN_START
	inline static TBaseDelegate<RetValType, ParamTypes...> CreateLambda(FunctorType&& InFunctor, VarTypes... Vars)
	FUNCTION_CHECK_RETURN_END
	{
		TBaseDelegate<RetValType, ParamTypes...> Result;
		TBaseFunctorDelegateInstance<TFuncType, typename TRemoveReference<FunctorType>::Type, VarTypes...>::Create(Result, Forward<FunctorType>(InFunctor), Vars...);
		return Result;
	}

	/**
	 * Static: Creates a raw C++ pointer member function delegate.
	 *
	 * Raw pointer doesn't use any sort of reference, so may be unsafe to call if the object was
	 * deleted out from underneath your delegate. Be careful when calling Execute()!
	 */
	template <typename UserClass, typename... VarTypes>
	FUNCTION_CHECK_RETURN_START
	inline static TBaseDelegate<RetValType, ParamTypes...> CreateRaw(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	FUNCTION_CHECK_RETURN_END
	{
		TBaseDelegate<RetValType, ParamTypes...> Result;
		TBaseRawMethodDelegateInstance<false, UserClass, TFuncType, VarTypes...>::Create(Result, InUserObject, InFunc, Vars...);
		return Result;
	}
	template <typename UserClass, typename... VarTypes>
	FUNCTION_CHECK_RETURN_START
	inline static TBaseDelegate<RetValType, ParamTypes...> CreateRaw(UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	FUNCTION_CHECK_RETURN_END
	{
		TBaseDelegate<RetValType, ParamTypes...> Result;
		TBaseRawMethodDelegateInstance<true , UserClass, TFuncType, VarTypes...>::Create(Result, InUserObject, InFunc, Vars...);
		return Result;
	}
	
	/**
	 * Static: Creates a shared pointer-based (fast, not thread-safe) member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template <typename UserClass, typename... VarTypes>
	FUNCTION_CHECK_RETURN_START
	inline static TBaseDelegate<RetValType, ParamTypes...> CreateSP(const TSharedRef<UserClass, ESPMode::Fast>& InUserObjectRef, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	FUNCTION_CHECK_RETURN_END
	{
		TBaseDelegate<RetValType, ParamTypes...> Result;
		TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::Fast, TFuncType, VarTypes...>::Create(Result, InUserObjectRef, InFunc, Vars...);
		return Result;
	}
	template <typename UserClass, typename... VarTypes>
	FUNCTION_CHECK_RETURN_START
	inline static TBaseDelegate<RetValType, ParamTypes...> CreateSP(const TSharedRef<UserClass, ESPMode::Fast>& InUserObjectRef, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	FUNCTION_CHECK_RETURN_END
	{
		TBaseDelegate<RetValType, ParamTypes...> Result;
		TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::Fast, TFuncType, VarTypes...>::Create(Result, InUserObjectRef, InFunc, Vars...);
		return Result;
	}

	/**
	 * Static: Creates a shared pointer-based (fast, not thread-safe) member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template <typename UserClass, typename... VarTypes>
	FUNCTION_CHECK_RETURN_START
	inline static TBaseDelegate<RetValType, ParamTypes...> CreateSP(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	FUNCTION_CHECK_RETURN_END
	{
		return CreateSP(StaticCastSharedRef<UserClass>(InUserObject->AsShared()), InFunc, Vars...);
	}
	template <typename UserClass, typename... VarTypes>
	FUNCTION_CHECK_RETURN_START
	inline static TBaseDelegate<RetValType, ParamTypes...> CreateSP(UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	FUNCTION_CHECK_RETURN_END
	{
		return CreateSP(StaticCastSharedRef<UserClass>(InUserObject->AsShared()), InFunc, Vars...);
	}
	
	/**
	 * Static: Creates a shared pointer-based (slower, conditionally thread-safe) member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template <typename UserClass, typename... VarTypes>
	FUNCTION_CHECK_RETURN_START
	inline static TBaseDelegate<RetValType, ParamTypes...> CreateThreadSafeSP(const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObjectRef, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	FUNCTION_CHECK_RETURN_END
	{
		TBaseDelegate<RetValType, ParamTypes...> Result;
		TBaseSPMethodDelegateInstance<false, UserClass, ESPMode::ThreadSafe, TFuncType, VarTypes...>::Create(Result, InUserObjectRef, InFunc, Vars...);
		return Result;
	}
	template <typename UserClass, typename... VarTypes>
	FUNCTION_CHECK_RETURN_START
	inline static TBaseDelegate<RetValType, ParamTypes...> CreateThreadSafeSP(const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObjectRef, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	FUNCTION_CHECK_RETURN_END
	{
		TBaseDelegate<RetValType, ParamTypes...> Result;
		TBaseSPMethodDelegateInstance<true , UserClass, ESPMode::ThreadSafe, TFuncType, VarTypes...>::Create(Result, InUserObjectRef, InFunc, Vars...);
		return Result;
	}

	/**
	 * Static: Creates a shared pointer-based (slower, conditionally thread-safe) member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template <typename UserClass, typename... VarTypes>
	FUNCTION_CHECK_RETURN_START
	inline static TBaseDelegate<RetValType, ParamTypes...> CreateThreadSafeSP(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	FUNCTION_CHECK_RETURN_END
	{
		return CreateThreadSafeSP(StaticCastSharedRef<UserClass>(InUserObject->AsShared()), InFunc, Vars...);
	}
	template <typename UserClass, typename... VarTypes>
	FUNCTION_CHECK_RETURN_START
	inline static TBaseDelegate<RetValType, ParamTypes...> CreateThreadSafeSP(UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	FUNCTION_CHECK_RETURN_END
	{
		return CreateThreadSafeSP(StaticCastSharedRef<UserClass>(InUserObject->AsShared()), InFunc, Vars...);
	}

	/**
	 * Static: Creates a UFunction-based member function delegate.
	 *
	 * UFunction delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template <typename UObjectTemplate, typename... VarTypes>
	FUNCTION_CHECK_RETURN_START
	inline static TBaseDelegate<RetValType, ParamTypes...> CreateUFunction(UObjectTemplate* InUserObject, const FName& InFunctionName, VarTypes... Vars)
	FUNCTION_CHECK_RETURN_END
	{
		TBaseDelegate<RetValType, ParamTypes...> Result;
		TBaseUFunctionDelegateInstance<UObjectTemplate, TFuncType, VarTypes...>::Create(Result, InUserObject, InFunctionName, Vars...);
		return Result;
	}

	/**
	 * Static: Creates a UObject-based member function delegate.
	 *
	 * UObject delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template <typename UserClass, typename... VarTypes>
	FUNCTION_CHECK_RETURN_START
	inline static TBaseDelegate<RetValType, ParamTypes...> CreateUObject(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	FUNCTION_CHECK_RETURN_END
	{
		TBaseDelegate<RetValType, ParamTypes...> Result;
		TBaseUObjectMethodDelegateInstance<false, UserClass, TFuncType, VarTypes...>::Create(Result, InUserObject, InFunc, Vars...);
		return Result;
	}
	template <typename UserClass, typename... VarTypes>
	FUNCTION_CHECK_RETURN_START
	inline static TBaseDelegate<RetValType, ParamTypes...> CreateUObject(UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	FUNCTION_CHECK_RETURN_END
	{
		TBaseDelegate<RetValType, ParamTypes...> Result;
		TBaseUObjectMethodDelegateInstance<true , UserClass, TFuncType, VarTypes...>::Create(Result, InUserObject, InFunc, Vars...);
		return Result;
	}

public:

	/**
	 * Default constructor
	 */
	inline TBaseDelegate()
	{
	}

	/**
	 * 'Null' constructor
	 */
	inline TBaseDelegate(TYPE_OF_NULLPTR)
	{
	}

	/**
	 * Destructor.
	 */
	inline ~TBaseDelegate()
	{
		Unbind();
	}

	/**
	 * Move constructor.
	 *
	 * @param Other The delegate object to move from.
	 */
	inline TBaseDelegate(TBaseDelegate&& Other)
	{
		*this = MoveTemp(Other);
	}

	/**
	 * Creates and initializes a new instance from an existing delegate object.
	 *
	 * @param Other The delegate object to copy from.
	 */
	inline TBaseDelegate(const TBaseDelegate& Other)
	{
		*this = Other;
	}

	/**
	 * Move assignment operator.
	 *
	 * @param	OtherDelegate	Delegate object to copy from
	 */
	inline TBaseDelegate& operator=(TBaseDelegate&& Other)
	{
		if (&Other != this)
		{
			// this down-cast is OK! allows for managing invocation list in the base class without requiring virtual functions
			TDelegateInstanceInterface* OtherInstance = Other.GetDelegateInstanceProtected();

			if (OtherInstance != nullptr)
			{
				OtherInstance->CreateCopy(*this);
			}
			else
			{
				Unbind();
			}
		}

		return *this;
	}

	/**
	 * Assignment operator.
	 *
	 * @param	OtherDelegate	Delegate object to copy from
	 */
	inline TBaseDelegate& operator=(const TBaseDelegate& Other)
	{
		if (&Other != this)
		{
			// this down-cast is OK! allows for managing invocation list in the base class without requiring virtual functions
			TDelegateInstanceInterface* OtherInstance = Other.GetDelegateInstanceProtected();

			if (OtherInstance != nullptr)
			{
				OtherInstance->CreateCopy(*this);
			}
			else
			{
				Unbind();
			}
		}

		return *this;
	}

public:

	/**
	 * Binds a raw C++ pointer global function delegate
	 */
	template <typename... VarTypes>
	inline void BindStatic(typename TBaseStaticDelegateInstance<TFuncType, VarTypes...>::FFuncPtr InFunc, VarTypes... Vars)
	{
		*this = CreateStatic(InFunc, Vars...);
	}
	
	/**
	 * Static: Binds a C++ lambda delegate
	 * technically this works for any functor types, but lambdas are the primary use case
	 */
	template<typename FunctorType, typename... VarTypes>
	inline void BindLambda(FunctorType&& InFunctor, VarTypes... Vars)
	{
		*this = CreateLambda(Forward<FunctorType>(InFunctor), Vars...);
	}

	/**
	 * Binds a raw C++ pointer delegate.
	 *
	 * Raw pointer doesn't use any sort of reference, so may be unsafe to call if the object was
	 * deleted out from underneath your delegate. Be careful when calling Execute()!
	 */
	template <typename UserClass, typename... VarTypes>
	inline void BindRaw(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		*this = CreateRaw(InUserObject, InFunc, Vars...);
	}
	template <typename UserClass, typename... VarTypes>
	inline void BindRaw(UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		*this = CreateRaw(InUserObject, InFunc, Vars...);
	}

	/**
	 * Binds a shared pointer-based (fast, not thread-safe) member function delegate.  Shared pointer delegates keep a weak reference to your object.  You can use ExecuteIfBound() to call them.
	 */
	template <typename UserClass, typename... VarTypes>
	inline void BindSP(const TSharedRef<UserClass, ESPMode::Fast>& InUserObjectRef, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		*this = CreateSP(InUserObjectRef, InFunc, Vars...);
	}
	template <typename UserClass, typename... VarTypes>
	inline void BindSP(const TSharedRef<UserClass, ESPMode::Fast>& InUserObjectRef, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		*this = CreateSP(InUserObjectRef, InFunc, Vars...);
	}

	/**
	 * Binds a shared pointer-based (fast, not thread-safe) member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template <typename UserClass, typename... VarTypes>
	inline void BindSP(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		*this = CreateSP(InUserObject, InFunc, Vars...);
	}
	template <typename UserClass, typename... VarTypes>
	inline void BindSP(UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		*this = CreateSP(InUserObject, InFunc, Vars...);
	}

	/**
	 * Binds a shared pointer-based (slower, conditionally thread-safe) member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template <typename UserClass, typename... VarTypes>
	inline void BindThreadSafeSP(const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObjectRef, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		*this = CreateThreadSafeSP(InUserObjectRef, InFunc, Vars...);
	}
	template <typename UserClass, typename... VarTypes>
	inline void BindThreadSafeSP(const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObjectRef, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		*this = CreateThreadSafeSP(InUserObjectRef, InFunc, Vars...);
	}

	/**
	 * Binds a shared pointer-based (slower, conditionally thread-safe) member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template <typename UserClass, typename... VarTypes>
	inline void BindThreadSafeSP(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		*this = CreateThreadSafeSP(InUserObject, InFunc, Vars...);
	}
	template <typename UserClass, typename... VarTypes>
	inline void BindThreadSafeSP(UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		*this = CreateThreadSafeSP(InUserObject, InFunc, Vars...);
	}

	/**
	 * Binds a UFunction-based member function delegate.
	 *
	 * UFunction delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template <typename UObjectTemplate, typename... VarTypes>
	inline void BindUFunction(UObjectTemplate* InUserObject, const FName& InFunctionName, VarTypes... Vars)
	{
		*this = CreateUFunction(InUserObject, InFunctionName, Vars...);
	}

	/**
	 * Binds a UObject-based member function delegate.
	 *
	 * UObject delegates keep a weak reference to your object.
	 * You can use ExecuteIfBound() to call them.
	 */
	template <typename UserClass, typename... VarTypes>
	inline void BindUObject(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		*this = CreateUObject(InUserObject, InFunc, Vars...);
	}
	template <typename UserClass, typename... VarTypes>
	inline void BindUObject(UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, RetValType (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		*this = CreateUObject(InUserObject, InFunc, Vars...);
	}

public:
	/**
	 * Execute the delegate.
	 *
	 * If the function pointer is not valid, an error will occur. Check IsBound() before
	 * calling this method or use ExecuteIfBound() instead.
	 *
	 * @see ExecuteIfBound
	 */
	FORCEINLINE RetValType Execute(ParamTypes... Params) const
	{
		TDelegateInstanceInterface* LocalDelegateInstance = GetDelegateInstanceProtected();

		// If this assert goes off, Execute() was called before a function was bound to the delegate.
		// Consider using ExecuteIfSafe() instead.
		checkSlow(LocalDelegateInstance != nullptr);

		return LocalDelegateInstance->Execute(Params...);
	}

	/**
	 * Returns a pointer to the correctly-typed delegate instance.
	 */
	FORCEINLINE TDelegateInstanceInterface* GetDelegateInstanceProtected() const
	{
		return (TDelegateInstanceInterface*)FDelegateBase::GetDelegateInstanceProtected();
	}
};

template <typename... ParamTypes>
class TBaseDelegate<void, ParamTypes...> : public TBaseDelegate<TTypeWrapper<void>, ParamTypes...>
{
	typedef TBaseDelegate<TTypeWrapper<void>, ParamTypes...> Super;

public:
	typedef typename Super::TDelegateInstanceInterface TDelegateInstanceInterface;

	/**
	 * Default constructor
	 */
	TBaseDelegate()
	{
	}

	/**
	 * 'Null' constructor
	 */
	FORCEINLINE TBaseDelegate(TYPE_OF_NULLPTR)
		: Super(nullptr)
	{
	}

	/**
	 * Move constructor.
	 *
	 * @param Other The delegate object to move from.
	 */
	FORCEINLINE TBaseDelegate(TBaseDelegate&& Other)
		: Super(MoveTemp(Other))
	{
	}

	/**
	 * Creates and initializes a new instance from an existing delegate object.
	 *
	 * @param Other The delegate object to copy from.
	 */
	FORCEINLINE TBaseDelegate(const TBaseDelegate& Other)
		: Super(Other)
	{
	}

	/**
	 * Move assignment operator.
	 *
	 * @param	OtherDelegate	Delegate object to copy from
	 */
	inline TBaseDelegate& operator=(TBaseDelegate&& Other)
	{
		(Super&)*this = MoveTemp((Super&)Other);
		return *this;
	}

	/**
	 * Assignment operator.
	 *
	 * @param	OtherDelegate	Delegate object to copy from
	 */
	inline TBaseDelegate& operator=(const TBaseDelegate& Other)
	{
		(Super&)*this = (Super&)Other;
		return *this;
	}

	/**
	 * Execute the delegate, but only if the function pointer is still valid
	 *
	 * @return  Returns true if the function was executed
	 */
	// NOTE: Currently only delegates with no return value support ExecuteIfBound() 
	inline bool ExecuteIfBound(ParamTypes... Params) const
	{
		if (Super::IsBound())
		{
			return Super::GetDelegateInstanceProtected()->ExecuteIfSafe(Params...);
		}

		return false;
	}
};


/**
 * Multicast delegate base class.
 *
 * This class implements the functionality of multicast delegates. It is templated to the function signature
 * that it is compatible with. Use the various DECLARE_MULTICAST_DELEGATE and DECLARE_EVENT macros to create
 * actual delegate types.
 *
 * Multicast delegates offer no guarantees for the calling order of bound functions. As bindings get added
 * and removed over time, the calling order may change. Only bindings without return values are supported.
 */
template <typename RetValType, typename... ParamTypes>
class TBaseMulticastDelegate;

template <typename... ParamTypes>
class TBaseMulticastDelegate<void, ParamTypes...> : public FMulticastDelegateBase<FWeakObjectPtr>
{
	typedef FMulticastDelegateBase<FWeakObjectPtr> Super;

public:
	/** DEPRECATED: Type definition for unicast delegate classes whose delegate instances are compatible with this delegate. */
	typedef TBaseDelegate< void, ParamTypes... > FDelegate;

	/** Type definition for the shared interface of delegate instance types compatible with this delegate class. */
	typedef IBaseDelegateInstance<void (ParamTypes...)> TDelegateInstanceInterface;

public:

	/**
	 * DEPRECATED: Adds a delegate instance to this multicast delegate's invocation list.
	 *
	 * This method is retained for backwards compatibility.
	 *
	 * @param Delegate The delegate to add.
	 */
	FDelegateHandle Add(FDelegate&& InNewDelegate)
	{
		FDelegateHandle Result;
		if (Super::GetDelegateInstanceProtectedHelper(InNewDelegate))
		{
			Result = AddDelegateInstance(MoveTemp(InNewDelegate));
		}

		return Result;
	}

	/**
	 * DEPRECATED: Adds a unicast delegate to this multi-cast delegate's invocation list.
	 *
	 * This method is retained for backwards compatibility.
	 *
	 * @param Delegate The delegate to add.
	 */
	FDelegateHandle Add(const FDelegate& InNewDelegate)
	{
		FDelegateHandle Result;
		if (Super::GetDelegateInstanceProtectedHelper(InNewDelegate))
		{
			Result = AddDelegateInstance(FDelegate(InNewDelegate));
		}

		return Result;
	}

	/**
	 * Adds a raw C++ pointer global function delegate
	 *
	 * @param	InFunc	Function pointer
	 */
	template <typename... VarTypes>
	inline FDelegateHandle AddStatic(typename TBaseStaticDelegateInstance<void (ParamTypes...), VarTypes...>::FFuncPtr InFunc, VarTypes... Vars)
	{
		return Add(FDelegate::CreateStatic(InFunc, Vars...));
	}

	/**
	 * Adds a C++ lambda delegate
	 * technically this works for any functor types, but lambdas are the primary use case
	 *
	 * @param	InFunctor	Functor (e.g. Lambda)
	 */
	template<typename FunctorType, typename... VarTypes>
	inline FDelegateHandle AddLambda(FunctorType&& InFunctor, VarTypes... Vars)
	{
		return Add(FDelegate::CreateLambda(Forward<FunctorType>(InFunctor), Vars...));
	}

	/**
	 * Adds a raw C++ pointer delegate.
	 *
	 * Raw pointer doesn't use any sort of reference, so may be unsafe to call if the object was
	 * deleted out from underneath your delegate. Be careful when calling Execute()!
	 *
	 * @param	InUserObject	User object to bind to
	 * @param	InFunc			Class method function address
	 */
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddRaw(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, void (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		return Add(FDelegate::CreateRaw(InUserObject, InFunc, Vars...));
	}
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddRaw(UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, void (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		return Add(FDelegate::CreateRaw(InUserObject, InFunc, Vars...));
	}

	/**
	 * Adds a shared pointer-based (fast, not thread-safe) member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 *
	 * @param	InUserObjectRef	User object to bind to
	 * @param	InFunc			Class method function address
	 */
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddSP(const TSharedRef<UserClass, ESPMode::Fast>& InUserObjectRef, typename TMemFunPtrType<false, UserClass, void (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		return Add(FDelegate::CreateSP(InUserObjectRef, InFunc, Vars...));
	}
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddSP(const TSharedRef<UserClass, ESPMode::Fast>& InUserObjectRef, typename TMemFunPtrType<true, UserClass, void (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		return Add(FDelegate::CreateSP(InUserObjectRef, InFunc, Vars...));
	}

	/**
	 * Adds a shared pointer-based (fast, not thread-safe) member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 *
	 * @param	InUserObject	User object to bind to
	 * @param	InFunc			Class method function address
	 */
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddSP(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, void (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		return Add(FDelegate::CreateSP(InUserObject, InFunc, Vars...));
	}
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddSP(UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, void (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		return Add(FDelegate::CreateSP(InUserObject, InFunc, Vars...));
	}

	/**
	 * Adds a shared pointer-based (slower, conditionally thread-safe) member function delegate.  Shared pointer delegates keep a weak reference to your object.
	 *
	 * @param	InUserObjectRef	User object to bind to
	 * @param	InFunc			Class method function address
	 */
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddThreadSafeSP(const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObjectRef, typename TMemFunPtrType<false, UserClass, void (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		return Add(FDelegate::CreateThreadSafeSP(InUserObjectRef, InFunc, Vars...));
	}
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddThreadSafeSP(const TSharedRef<UserClass, ESPMode::ThreadSafe>& InUserObjectRef, typename TMemFunPtrType<true, UserClass, void (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		return Add(FDelegate::CreateThreadSafeSP(InUserObjectRef, InFunc, Vars...));
	}

	/**
	 * Adds a shared pointer-based (slower, conditionally thread-safe) member function delegate.
	 *
	 * Shared pointer delegates keep a weak reference to your object.
	 *
	 * @param	InUserObject	User object to bind to
	 * @param	InFunc			Class method function address
	 */
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddThreadSafeSP(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, void (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		return Add(FDelegate::CreateThreadSafeSP(InUserObject, InFunc, Vars...));
	}
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddThreadSafeSP(UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, void (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		return Add(FDelegate::CreateThreadSafeSP(InUserObject, InFunc, Vars...));
	}

	/**
	 * Adds a UFunction-based member function delegate.
	 *
	 * UFunction delegates keep a weak reference to your object.
	 *
	 * @param	InUserObject	User object to bind to
	 * @param	InFunctionName			Class method function address
	 */
	template <typename UObjectTemplate, typename... VarTypes>
	inline FDelegateHandle AddUFunction(UObjectTemplate* InUserObject, const FName& InFunctionName, VarTypes... Vars)
	{
		return Add(FDelegate::CreateUFunction(InUserObject, InFunctionName, Vars...));
	}

	/**
	 * Adds a UObject-based member function delegate.
	 *
	 * UObject delegates keep a weak reference to your object.
	 *
	 * @param	InUserObject	User object to bind to
	 * @param	InFunc			Class method function address
	 */
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddUObject(UserClass* InUserObject, typename TMemFunPtrType<false, UserClass, void (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		return Add(FDelegate::CreateUObject(InUserObject, InFunc, Vars...));
	}
	template <typename UserClass, typename... VarTypes>
	inline FDelegateHandle AddUObject(UserClass* InUserObject, typename TMemFunPtrType<true, UserClass, void (ParamTypes..., VarTypes...)>::Type InFunc, VarTypes... Vars)
	{
		return Add(FDelegate::CreateUObject(InUserObject, InFunc, Vars...));
	}

public:

	/**
	 * Removes a delegate instance from this multi-cast delegate's invocation list (performance is O(N)).
	 *
	 * Note that the order of the delegate instances may not be preserved!
	 *
	 * @param Handle The handle of the delegate instance to remove.
	 */
	void Remove( FDelegateHandle Handle )
	{
		RemoveDelegateInstance(Handle);
	}

protected:

	/** 
	 * Hidden default constructor.
	 */
	inline TBaseMulticastDelegate( ) { }

	/**
	 * Hidden copy constructor (for proper deep copies).
	 *
	 * @param Other The multicast delegate to copy from.
	 */
	TBaseMulticastDelegate( const TBaseMulticastDelegate& Other )
	{
		*this = Other;
	}

	/**
	 * Hidden assignment operator (for proper deep copies).
	 *
	 * @param Other The delegate to assign from.
	 * @return This instance.
	 */
	TBaseMulticastDelegate& operator=( const TBaseMulticastDelegate& Other )
	{
		if (&Other != this)
		{
			Super::Clear();

			for (const FDelegateBase& OtherDelegateRef : Other.GetInvocationList())
			{
				if (IDelegateInstance* OtherInstance = Super::GetDelegateInstanceProtectedHelper(OtherDelegateRef))
				{
					FDelegate TempDelegate;
					((TDelegateInstanceInterface*)OtherInstance)->CreateCopy(TempDelegate);
					Super::AddInternal(MoveTemp(TempDelegate));
				}
			}
		}

		return *this;
	}

protected:

	/**
	 * Adds a function delegate to this multi-cast delegate's invocation list.
	 *
	 * This method will assert if the same function has already been bound.
	 *
	 * @param	InDelegate	Delegate to add
	 */
	FDelegateHandle AddDelegateInstance(FDelegate&& InNewDelegate)
	{
		return Super::AddInternal(MoveTemp(InNewDelegate));
	}

public:
	/**
	 * Broadcasts this delegate to all bound objects, except to those that may have expired.
	 *
	 * The constness of this method is a lie, but it allows for broadcasting from const functions.
	 */
	void Broadcast(ParamTypes... Params) const
	{
		bool NeedsCompaction = false;

		Super::LockInvocationList();
		{
			const TInvocationList& LocalInvocationList = Super::GetInvocationList();

			// call bound functions in reverse order, so we ignore any instances that may be added by callees
			for (int32 InvocationListIndex = LocalInvocationList.Num() - 1; InvocationListIndex >= 0; --InvocationListIndex)
			{
				// this down-cast is OK! allows for managing invocation list in the base class without requiring virtual functions
				const FDelegate& DelegateBase = (const FDelegate&)LocalInvocationList[InvocationListIndex];

				IDelegateInstance* DelegateInstanceInterface = Super::GetDelegateInstanceProtectedHelper(DelegateBase);
				if (DelegateInstanceInterface == nullptr || !((TDelegateInstanceInterface*)DelegateInstanceInterface)->ExecuteIfSafe(Params...))
				{
					NeedsCompaction = true;
				}
			}
		}
		Super::UnlockInvocationList();

		if (NeedsCompaction)
		{
			const_cast<TBaseMulticastDelegate*>(this)->CompactInvocationList();
		}
	}

protected:
	/**
	 * Removes a function from this multi-cast delegate's invocation list (performance is O(N)).
	 *
	 * The function is not actually removed, but deleted and marked as removed.
	 * It will be removed next time the invocation list is compacted within Broadcast().
	 *
	 * @param Handle The handle of the delegate instance to remove.
	 */
	void RemoveDelegateInstance( FDelegateHandle Handle )
	{
		const TInvocationList& LocalInvocationList = Super::GetInvocationList();

		for (int32 InvocationListIndex = 0; InvocationListIndex < LocalInvocationList.Num(); ++InvocationListIndex)
		{
			// InvocationList is const, so we const_cast to be able to unbind the entry
			// TODO: This is horrible, can we get the base class to do it?
			FDelegateBase& DelegateBase = const_cast<FDelegateBase&>(LocalInvocationList[InvocationListIndex]);

			IDelegateInstance* DelegateInstanceInterface = Super::GetDelegateInstanceProtectedHelper(DelegateBase);
			if ((DelegateInstanceInterface != nullptr) && DelegateInstanceInterface->GetHandle() == Handle)
			{
				DelegateBase.Unbind();

				break; // each delegate binding has a unique handle, so once we find it, we can stop
			}
		}

		Super::CompactInvocationList();
	}
};


/**
 * Implements a multicast delegate.
 *
 * This class should not be instantiated directly. Use the DECLARE_MULTICAST_DELEGATE macros instead.
 */
template <typename RetValType, typename... ParamTypes>
class TMulticastDelegate;

template <typename... ParamTypes>
class TMulticastDelegate<void, ParamTypes...> : public TBaseMulticastDelegate<void, ParamTypes... >
{
private:
	// Prevents erroneous use by other classes.
	typedef TBaseMulticastDelegate< void, ParamTypes... > Super;
};


/**
 * Dynamic delegate base object (UObject-based, serializable).  You'll use the various DECLARE_DYNAMIC_DELEGATE
 * macros to create the actual delegate type, templated to the function signature the delegate is compatible with.
 * Then, you can create an instance of that class when you want to assign functions to the delegate.
 */
template <typename TWeakPtr, typename RetValType, typename... ParamTypes>
class TBaseDynamicDelegate : public TScriptDelegate<TWeakPtr>
{
public:
	/**
	 * Default constructor
	 */
	TBaseDynamicDelegate() { }

	/**
	 * Construction from an FScriptDelegate must be explicit.  This is really only used by UObject system internals.
	 *
	 * @param	InScriptDelegate	The delegate to construct from by copying
	 */
	explicit TBaseDynamicDelegate( const TScriptDelegate<TWeakPtr>& InScriptDelegate )
		: TScriptDelegate<TWeakPtr>( InScriptDelegate )
	{ }

	/**
	 * Templated helper class to define a typedef for user's method pointer, then used below
	 */
	template< class UserClass >
	class TMethodPtrResolver
	{
	public:
		typedef RetValType (UserClass::*FMethodPtr)(ParamTypes... Params);
	};

	/**
	 * Binds a UObject instance and a UObject method address to this delegate.
	 *
	 * @param	InUserObject		UObject instance
	 * @param	InMethodPtr			Member function address pointer
	 * @param	InFunctionName		Name of member function, without class name
	 *
	 * NOTE:  Do not call this function directly.  Instead, call BindDynamic() which is a macro proxy function that
	 *        automatically sets the function name string for the caller.
	 */
	template< class UserClass >
	void __Internal_BindDynamic( UserClass* InUserObject, typename TMethodPtrResolver< UserClass >::FMethodPtr InMethodPtr, FName InFunctionName )
	{
		check( InUserObject != nullptr && InMethodPtr != nullptr );

		// NOTE: We're not actually storing the incoming method pointer or calling it.  We simply require it for type-safety reasons.

		// NOTE: If you hit a compile error on the following line, it means you're trying to use a non-UObject type
		//       with this delegate, which is not supported
		this->Object = InUserObject;

		// Store the function name.  The incoming function name was generated by a macro and includes the method's class name.
		this->FunctionName = InFunctionName;

		ensureMsgf(this->IsBound(), TEXT("Unable to bind delegate to '%s' (function might not be marked as a UFUNCTION or object may be pending kill)"), *InFunctionName.ToString());
	}

	friend uint32 GetTypeHash(const TBaseDynamicDelegate& Key)
	{
		return FCrc::MemCrc_DEPRECATED(&Key,sizeof(Key));
	}

	// NOTE:  Execute() method must be defined in derived classes

	// NOTE:  ExecuteIfBound() method must be defined in derived classes
};


/**
 * Dynamic multi-cast delegate base object (UObject-based, serializable).  You'll use the various
 * DECLARE_DYNAMIC_MULTICAST_DELEGATE macros to create the actual delegate type, templated to the function
 * signature the delegate is compatible with.   Then, you can create an instance of that class when you
 * want to assign functions to the delegate.
 */
template <typename TWeakPtr, typename RetValType, typename... ParamTypes>
class TBaseDynamicMulticastDelegate : public TMulticastScriptDelegate<TWeakPtr>
{
public:
	/** The actual single-cast delegate class for this multi-cast delegate */
	typedef TBaseDynamicDelegate<FWeakObjectPtr, RetValType, ParamTypes...> FDelegate;

	/**
	 * Default constructor
	 */
	TBaseDynamicMulticastDelegate() { }

	/**
	 * Construction from an FMulticastScriptDelegate must be explicit.  This is really only used by UObject system internals.
	 *
	 * @param	InScriptDelegate	The delegate to construct from by copying
	 */
	explicit TBaseDynamicMulticastDelegate( const TMulticastScriptDelegate<TWeakPtr>& InMulticastScriptDelegate )
		: TMulticastScriptDelegate<TWeakPtr>( InMulticastScriptDelegate )
	{ }

	/**
	 * Tests if a UObject instance and a UObject method address pair are already bound to this multi-cast delegate.
	 *
	 * @param	InUserObject		UObject instance
	 * @param	InMethodPtr			Member function address pointer
	 * @param	InFunctionName		Name of member function, without class name
	 * @return	True if the instance/method is already bound.
	 *
	 * NOTE:  Do not call this function directly.  Instead, call IsAlreadyBound() which is a macro proxy function that
	 *        automatically sets the function name string for the caller.
	 */
	template< class UserClass >
	bool __Internal_IsAlreadyBound( UserClass* InUserObject, typename FDelegate::template TMethodPtrResolver< UserClass >::FMethodPtr InMethodPtr, FName InFunctionName ) const
	{
		check( InUserObject != nullptr && InMethodPtr != nullptr );

		// NOTE: We're not actually using the incoming method pointer or calling it.  We simply require it for type-safety reasons.

		return this->Contains( InUserObject, InFunctionName );
	}

	/**
	 * Binds a UObject instance and a UObject method address to this multi-cast delegate.
	 *
	 * @param	InUserObject		UObject instance
	 * @param	InMethodPtr			Member function address pointer
	 * @param	InFunctionName		Name of member function, without class name
	 *
	 * NOTE:  Do not call this function directly.  Instead, call AddDynamic() which is a macro proxy function that
	 *        automatically sets the function name string for the caller.
	 */
	template< class UserClass >
	void __Internal_AddDynamic( UserClass* InUserObject, typename FDelegate::template TMethodPtrResolver< UserClass >::FMethodPtr InMethodPtr, FName InFunctionName )
	{
		check( InUserObject != nullptr && InMethodPtr != nullptr );

		// NOTE: We're not actually storing the incoming method pointer or calling it.  We simply require it for type-safety reasons.

		FDelegate NewDelegate;
		NewDelegate.__Internal_BindDynamic( InUserObject, InMethodPtr, InFunctionName );

		this->Add( NewDelegate );
	}

	/**
	 * Binds a UObject instance and a UObject method address to this multi-cast delegate, but only if it hasn't been bound before.
	 *
	 * @param	InUserObject		UObject instance
	 * @param	InMethodPtr			Member function address pointer
	 * @param	InFunctionName		Name of member function, without class name
	 *
	 * NOTE:  Do not call this function directly.  Instead, call AddUniqueDynamic() which is a macro proxy function that
	 *        automatically sets the function name string for the caller.
	 */
	template< class UserClass >
	void __Internal_AddUniqueDynamic( UserClass* InUserObject, typename FDelegate::template TMethodPtrResolver< UserClass >::FMethodPtr InMethodPtr, FName InFunctionName )
	{
		check( InUserObject != nullptr && InMethodPtr != nullptr );

		// NOTE: We're not actually storing the incoming method pointer or calling it.  We simply require it for type-safety reasons.

		FDelegate NewDelegate;
		NewDelegate.__Internal_BindDynamic( InUserObject, InMethodPtr, InFunctionName );

		this->AddUnique( NewDelegate );
	}

	/**
	 * Unbinds a UObject instance and a UObject method address from this multi-cast delegate.
	 *
	 * @param	InUserObject		UObject instance
	 * @param	InMethodPtr			Member function address pointer
	 * @param	InFunctionName		Name of member function, without class name
	 *
	 * NOTE:  Do not call this function directly.  Instead, call RemoveDynamic() which is a macro proxy function that
	 *        automatically sets the function name string for the caller.
	 */
	template< class UserClass >
	void __Internal_RemoveDynamic( UserClass* InUserObject, typename FDelegate::template TMethodPtrResolver< UserClass >::FMethodPtr InMethodPtr, FName InFunctionName )
	{
		check( InUserObject != nullptr && InMethodPtr != nullptr );

		// NOTE: We're not actually storing the incoming method pointer or calling it.  We simply require it for type-safety reasons.

		this->Remove( InUserObject, InFunctionName );
	}

	// NOTE:  Broadcast() method must be defined in derived classes
};
