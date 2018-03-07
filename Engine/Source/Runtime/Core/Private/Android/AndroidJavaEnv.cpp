// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "AndroidJavaEnv.h"
#include "CoreMinimal.h"
#include "HAL/ThreadSingleton.h"
#include "HAL/PlatformMisc.h"

//////////////////////////////////////////////////////////////////////////
// FJNIHelper
static JavaVM* CurrentJavaVM = nullptr;
static jint CurrentJavaVersion;
static jobject GlobalObjectRef;
static jobject ClassLoader;
static jmethodID FindClassMethod;


// Caches access to the environment, attached to the current thread
class FJNIHelper : public TThreadSingleton<FJNIHelper>
{
public:
	static JNIEnv* GetEnvironment()
	{
		return Get().CachedEnv;
	}

private:
	JNIEnv* CachedEnv = NULL;

private:
	friend class TThreadSingleton<FJNIHelper>;

	FJNIHelper()
		: CachedEnv(nullptr)
	{
		check(CurrentJavaVM);
		CurrentJavaVM->GetEnv((void **)&CachedEnv, CurrentJavaVersion);

		const jint AttachResult = CurrentJavaVM->AttachCurrentThread(&CachedEnv, nullptr);
		if (AttachResult == JNI_ERR)
		{
			FPlatformMisc::LowLevelOutputDebugString(TEXT("FJNIHelper failed to attach thread to Java VM!"));
			check(false);
		}
	}

	~FJNIHelper()
	{
		check(CurrentJavaVM);
		const jint DetachResult = CurrentJavaVM->DetachCurrentThread();
		if (DetachResult == JNI_ERR)
		{
			FPlatformMisc::LowLevelOutputDebugString(TEXT("FJNIHelper failed to detach thread from Java VM!"));
			check(false);
		}

		CachedEnv = nullptr;
	}
};

void AndroidJavaEnv::InitializeJavaEnv( JavaVM* VM, jint Version, jobject GlobalThis )
{
	if (CurrentJavaVM == nullptr)
	{
		CurrentJavaVM = VM;
		CurrentJavaVersion = Version;

		JNIEnv* Env = GetJavaEnv(false);
		jclass MainClass = Env->FindClass("com/epicgames/ue4/GameActivity");
		jclass classClass = Env->FindClass("java/lang/Class");
		jclass classLoaderClass = Env->FindClass("java/lang/ClassLoader");
		jmethodID getClassLoaderMethod = Env->GetMethodID(classClass, "getClassLoader", "()Ljava/lang/ClassLoader;");
		jobject classLoader = Env->CallObjectMethod(MainClass, getClassLoaderMethod);
		ClassLoader = Env->NewGlobalRef(classLoader);
		FindClassMethod = Env->GetMethodID(classLoaderClass, "findClass", "(Ljava/lang/String;)Ljava/lang/Class;");
	}
	GlobalObjectRef = GlobalThis;
}

jobject AndroidJavaEnv::GetGameActivityThis()
{
	return GlobalObjectRef;
}

jobject AndroidJavaEnv::GetClassLoader()
{
	return ClassLoader;
}

static void JavaEnvDestructor(void*)
{
	FPlatformMisc::LowLevelOutputDebugStringf(TEXT("*** JavaEnvDestructor: %d"), FPlatformTLS::GetCurrentThreadId());
	AndroidJavaEnv::DetachJavaEnv();
}

JNIEnv* AndroidJavaEnv::GetJavaEnv( bool bRequireGlobalThis /*= true*/ )
{
	//@TODO: ANDROID: Remove the other version if the helper works well
#if 0
	if (!bRequireGlobalThis || (GlobalObjectRef != nullptr))
	{
		return FJNIHelper::GetEnvironment();
	}
	else
	{
		return nullptr;
	}
#endif
#if 0
	// not reliable at the moment.. revisit later

	// Magic static - *should* be thread safe
	//Android & pthread specific, bind a destructor for thread exit
	static uint32 TlsSlot = 0;
	if (TlsSlot == 0)
	{
		pthread_key_create((pthread_key_t*)&TlsSlot, &JavaEnvDestructor);
	}
	JNIEnv* Env = (JNIEnv*)FPlatformTLS::GetTlsValue(TlsSlot);
	if (Env == nullptr)
	{
		CurrentJavaVM->GetEnv((void **)&Env, CurrentJavaVersion);

		jint AttachResult = CurrentJavaVM->AttachCurrentThread(&Env, NULL);
		if (AttachResult == JNI_ERR)
		{
			FPlatformMisc::LowLevelOutputDebugString(L"UNIT TEST -- Failed to get the JNI environment!");
			check(false);
			return nullptr;
		}
		FPlatformTLS::SetTlsValue(TlsSlot, (void*)Env);
	}

	return (!bRequireGlobalThis || (GlobalObjectRef != nullptr)) ? Env : nullptr;
#else
	// register a destructor to detach this thread
	static uint32 TlsSlot = 0;
	if (TlsSlot == 0)
	{
		pthread_key_create((pthread_key_t*)&TlsSlot, &JavaEnvDestructor);
	}

	JNIEnv* Env = nullptr;
	jint GetEnvResult = CurrentJavaVM->GetEnv((void **)&Env, CurrentJavaVersion);
	if (GetEnvResult == JNI_EDETACHED)
	{
		// attach to this thread
		jint AttachResult = CurrentJavaVM->AttachCurrentThread(&Env, NULL);
		if (AttachResult == JNI_ERR)
		{
			FPlatformMisc::LowLevelOutputDebugString(L"UNIT TEST -- Failed to attach thread to get the JNI environment!");
			check(false);
			return nullptr;
		}
		FPlatformTLS::SetTlsValue(TlsSlot, (void*)Env);
	}
	else if (GetEnvResult != JNI_OK)
	{
		FPlatformMisc::LowLevelOutputDebugStringf(L"UNIT TEST -- Failed to get the JNI environment! Result = %d", GetEnvResult);
		check(false);
		return nullptr;

	}
	return Env;
#endif
}

jclass AndroidJavaEnv::FindJavaClass( const char* name )
{
	JNIEnv* Env = GetJavaEnv();
	if (!Env)
	{
		return nullptr;
	}
	jstring ClassNameObj = Env->NewStringUTF(name);
	jclass FoundClass = static_cast<jclass>(Env->CallObjectMethod(ClassLoader, FindClassMethod, ClassNameObj));
	CheckJavaException();
	Env->DeleteLocalRef(ClassNameObj);
	return FoundClass;
}

void AndroidJavaEnv::DetachJavaEnv()
{
	CurrentJavaVM->DetachCurrentThread();
}

bool AndroidJavaEnv::CheckJavaException()
{
	JNIEnv* Env = GetJavaEnv();
	if (!Env)
	{
		return true;
	}
	if (Env->ExceptionCheck())
	{
		Env->ExceptionDescribe();
		Env->ExceptionClear();
		verify(false && "Java JNI call failed with an exception.");
		return true;
	}
	return false;
}
