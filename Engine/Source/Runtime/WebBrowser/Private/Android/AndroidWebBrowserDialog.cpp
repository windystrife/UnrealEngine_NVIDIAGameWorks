// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AndroidWebBrowserDialog.h"
#include "AndroidApplication.h"
#include "AndroidJava.h"

#include <jni.h>

namespace
{
	FText GetFText(jstring InString)
	{
		if (InString == nullptr)
		{
			return FText::GetEmpty();
		}

		JNIEnv*	JEnv = FAndroidApplication::GetJavaEnv();
		const char* Chars = JEnv->GetStringUTFChars(InString, 0);
		FText Retval = FText::FromString(UTF8_TO_TCHAR(Chars));
		JEnv->ReleaseStringUTFChars(InString, Chars);
		return Retval;
	}
}

FAndroidWebBrowserDialog::FAndroidWebBrowserDialog(jstring InMessageText, jstring InDefaultPrompt, jobject InCallback)
	: Type(EWebBrowserDialogType::Prompt)
	, MessageText(GetFText(InMessageText))
	, DefaultPrompt(GetFText(InDefaultPrompt))
	, Callback(InCallback)
{
}

FAndroidWebBrowserDialog::FAndroidWebBrowserDialog(EWebBrowserDialogType InDialogType, jstring InMessageText, jobject InCallback)
	: Type(InDialogType)
	, MessageText(GetFText(InMessageText))
	, DefaultPrompt()
	, Callback(InCallback)
{
}

void FAndroidWebBrowserDialog::Continue(bool Success, const FText& UserResponse)
{
	check(Callback != nullptr);
	JNIEnv*	JEnv = FAndroidApplication::GetJavaEnv();
	const char* MethodName = Success?"confirm":"cancel";
	const char* MethodSignature = (Success && Type==EWebBrowserDialogType::Prompt)?"(Ljava/lang/String;)V":"()V";
	jclass Class = JEnv->GetObjectClass(Callback);
	jmethodID MethodId = JEnv->GetMethodID(Class, MethodName, MethodSignature);

	jstring JUserResponse = nullptr;
	if (Success && Type==EWebBrowserDialogType::Prompt)
	{
		JUserResponse = FJavaClassObject::GetJString(UserResponse.ToString());
	}
	JEnv->CallVoidMethod(Callback, MethodId, JUserResponse);
}
