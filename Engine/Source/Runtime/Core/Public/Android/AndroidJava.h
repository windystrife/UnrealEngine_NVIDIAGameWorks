// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include <jni.h>

/*
Wrappers for Java classes.
TODO: make function look up static? It doesn't matter for our case here (we only create one instance of the class ever)
*/
struct FJavaClassMethod
{
	FName		Name;
	FName		Signature;
	jmethodID	Method;
};

class FJavaClassObject
{
public:
	// !!  All Java objects returned by JNI functions are local references.
	FJavaClassObject(FName ClassName, const char* CtorSig, ...);

	~FJavaClassObject();

	FJavaClassMethod GetClassMethod(const char* MethodName, const char* FuncSig);

	// TODO: Define this for extra cases
	template<typename ReturnType>
	ReturnType CallMethod(FJavaClassMethod Method, ...);

	FORCEINLINE jobject GetJObject() const
	{
		return Object;
	}

	static jstring GetJString(const FString& String);

	void VerifyException();

protected:

	jobject			Object;
	jclass			Class;

private:
	FJavaClassObject(const FJavaClassObject& rhs);
	FJavaClassObject& operator = (const FJavaClassObject& rhs);
};

template<>
void FJavaClassObject::CallMethod<void>(FJavaClassMethod Method, ...);

template<>
bool FJavaClassObject::CallMethod<bool>(FJavaClassMethod Method, ...);

template<>
int FJavaClassObject::CallMethod<int>(FJavaClassMethod Method, ...);

template<>
jobject FJavaClassObject::CallMethod<jobject>(FJavaClassMethod Method, ...);

template<>
jobjectArray FJavaClassObject::CallMethod<jobjectArray>(FJavaClassMethod Method, ...);

template<>
int64 FJavaClassObject::CallMethod<int64>(FJavaClassMethod Method, ...);

template<>
FString FJavaClassObject::CallMethod<FString>(FJavaClassMethod Method, ...);
