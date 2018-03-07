// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ScriptMacros.h: Kismet VM execution engine.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "UObject/Script.h"
#include "UObject/ScriptInterface.h"
#include "UObject/UnrealType.h"
#include "UObject/Stack.h"

/*-----------------------------------------------------------------------------
	Macros.
-----------------------------------------------------------------------------*/

/**
 * This is the largest possible size that a single variable can be; a variables size is determined by multiplying the
 * size of the type by the variables ArrayDim (always 1 unless it's a static array).
 */
enum {MAX_VARIABLE_SIZE = 0x0FFF };

#define ZERO_INIT(Type,ParamName) FMemory::Memzero(&ParamName,sizeof(Type));

#define PARAM_PASSED_BY_VAL(ParamName, PropertyType, ParamType)									\
	ParamType ParamName;																		\
	Stack.StepCompiledIn<PropertyType>(&ParamName);

#define PARAM_PASSED_BY_VAL_ZEROED(ParamName, PropertyType, ParamType)							\
	ParamType ParamName = (ParamType)0;															\
	Stack.StepCompiledIn<PropertyType>(&ParamName);

#define PARAM_PASSED_BY_REF(ParamName, PropertyType, ParamType)									\
	ParamType ParamName##Temp;																	\
	ParamType& ParamName = Stack.StepCompiledInRef<PropertyType, ParamType>(&ParamName##Temp);

#define PARAM_PASSED_BY_REF_ZEROED(ParamName, PropertyType, ParamType)							\
	ParamType ParamName##Temp = (ParamType)0;													\
	ParamType& ParamName = Stack.StepCompiledInRef<PropertyType, ParamType>(&ParamName##Temp);

#define P_GET_PROPERTY(PropertyType, ParamName)													\
	PropertyType::TCppType ParamName = PropertyType::GetDefaultPropertyValue();					\
	Stack.StepCompiledIn<PropertyType>(&ParamName);

#define P_GET_PROPERTY_REF(PropertyType, ParamName)												\
	PropertyType::TCppType ParamName##Temp = PropertyType::GetDefaultPropertyValue();			\
	PropertyType::TCppType& ParamName = Stack.StepCompiledInRef<PropertyType, PropertyType::TCppType>(&ParamName##Temp);



#define P_GET_UBOOL(ParamName)						uint32 ParamName##32 = 0; bool ParamName=false;	Stack.StepCompiledIn<UBoolProperty>(&ParamName##32); ParamName = !!ParamName##32; // translate the bitfield into a bool type for non-intel platforms
#define P_GET_UBOOL8(ParamName)						uint32 ParamName##32 = 0; uint8 ParamName=0;    Stack.StepCompiledIn<UBoolProperty>(&ParamName##32); ParamName = ParamName##32 ? 1 : 0; // translate the bitfield into a bool type for non-intel platforms
#define P_GET_UBOOL16(ParamName)					uint32 ParamName##32 = 0; uint16 ParamName=0;   Stack.StepCompiledIn<UBoolProperty>(&ParamName##32); ParamName = ParamName##32 ? 1 : 0; // translate the bitfield into a bool type for non-intel platforms
#define P_GET_UBOOL32(ParamName)					uint32 ParamName=0;                             Stack.StepCompiledIn<UBoolProperty>(&ParamName); ParamName = ParamName ? 1 : 0; // translate the bitfield into a bool type for non-intel platforms
#define P_GET_UBOOL64(ParamName)					uint64 ParamName=0;                             Stack.StepCompiledIn<UBoolProperty>(&ParamName); ParamName = ParamName ? 1 : 0; // translate the bitfield into a bool type for non-intel platforms
#define P_GET_UBOOL_REF(ParamName)					PARAM_PASSED_BY_REF_ZEROED(ParamName, UBoolProperty, bool)

#define P_GET_STRUCT(StructType,ParamName)			PARAM_PASSED_BY_VAL(ParamName, UStructProperty, StructType)
#define P_GET_STRUCT_REF(StructType,ParamName)		PARAM_PASSED_BY_REF(ParamName, UStructProperty, StructType)

#define P_GET_OBJECT(ObjectType,ParamName)			PARAM_PASSED_BY_VAL_ZEROED(ParamName, UObjectPropertyBase, ObjectType*)
#define P_GET_OBJECT_REF(ObjectType,ParamName)		PARAM_PASSED_BY_REF_ZEROED(ParamName, UObjectPropertyBase, ObjectType*)

#define P_GET_OBJECT_NO_PTR(ObjectType,ParamName)			PARAM_PASSED_BY_VAL_ZEROED(ParamName, UObjectPropertyBase, ObjectType)
#define P_GET_OBJECT_REF_NO_PTR(ObjectType,ParamName)		PARAM_PASSED_BY_REF_ZEROED(ParamName, UObjectPropertyBase, ObjectType)

#define P_GET_TARRAY(ElementType,ParamName)			PARAM_PASSED_BY_VAL(ParamName, UArrayProperty, TArray<ElementType>)
#define P_GET_TARRAY_REF(ElementType,ParamName)		PARAM_PASSED_BY_REF(ParamName, UArrayProperty, TArray<ElementType>)

#define P_GET_TMAP(KeyType,ValueType,ParamName)			PARAM_PASSED_BY_VAL(ParamName, UMapProperty, PREPROCESSOR_COMMA_SEPARATED(TMap<KeyType, ValueType>))
#define P_GET_TMAP_REF(KeyType,ValueType,ParamName)		PARAM_PASSED_BY_REF(ParamName, UMapProperty, PREPROCESSOR_COMMA_SEPARATED(TMap<KeyType, ValueType>))

#define P_GET_TSET(ElementType,ParamName)			PARAM_PASSED_BY_VAL(ParamName, USetProperty, TSet<ElementType>)
#define P_GET_TSET_REF(ElementType,ParamName)		PARAM_PASSED_BY_REF(ParamName, USetProperty, TSet<ElementType>)

#define P_GET_TINTERFACE(ObjectType,ParamName)		PARAM_PASSED_BY_VAL(ParamName, UInterfaceProperty, TScriptInterface<ObjectType>)
#define P_GET_TINTERFACE_REF(ObjectType,ParamName)	PARAM_PASSED_BY_REF(ParamName, UInterfaceProperty, TScriptInterface<ObjectType>)

#define P_GET_SOFTOBJECT(ObjectType,ParamName)		PARAM_PASSED_BY_VAL(ParamName, USoftObjectProperty, ObjectType)
#define P_GET_SOFTOBJECT_REF(ObjectType,ParamName)	PARAM_PASSED_BY_REF(ParamName, USoftObjectProperty, ObjectType)

#define P_GET_ARRAY(ElementType,ParamName)			ElementType ParamName[(MAX_VARIABLE_SIZE/sizeof(ElementType))+1];		Stack.StepCompiledIn<UProperty>(ParamName);
#define P_GET_ARRAY_REF(ElementType,ParamName)		ElementType ParamName##Temp[(MAX_VARIABLE_SIZE/sizeof(ElementType))+1]; ElementType* ParamName = Stack.StepCompiledInRef<UProperty, ElementType*>(ParamName##Temp);

#define P_GET_ENUM(EnumType,ParamName)				EnumType ParamName = (EnumType)0; Stack.StepCompiledIn<UEnumProperty>(&ParamName);
#define P_GET_ENUM_REF(EnumType,ParamName)			PARAM_PASSED_BY_REF_ZEROED(ParamName, UEnumProperty, EnumType)

#define P_FINISH									Stack.Code += !!Stack.Code; /* increment the code ptr unless it is null */

#define P_NATIVE_BEGIN { SCOPED_SCRIPT_NATIVE_TIMER(ScopedNativeCallTimer);
#define P_NATIVE_END   }
