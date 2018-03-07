// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericApplication.h"
#include "AndroidWindow.h"
#include "AndroidJavaEnv.h"

namespace FAndroidAppEntry
{
	void PlatformInit();

	// if the native window handle has changed then the new handle is required.
	void ReInitWindow(void* NewNativeWindowHandle = nullptr);

	void DestroyWindow();
	void ReleaseEGL();
}

struct FPlatformOpenGLContext;
namespace FAndroidEGL
{
	// Back door into more intimate Android OpenGL variables (a.k.a. a hack)
	FPlatformOpenGLContext*	GetRenderingContext();
	FPlatformOpenGLContext*	CreateContext();
	void					MakeCurrent(FPlatformOpenGLContext*);
	void					ReleaseContext(FPlatformOpenGLContext*);
	void					SwapBuffers(FPlatformOpenGLContext*);
	void					SetFlipsEnabled(bool Enabled);
	void					BindDisplayToContext(FPlatformOpenGLContext*);
}

//disable warnings from overriding the deprecated forcefeedback.  
//calls to the deprecated function will still generate warnings.
PRAGMA_DISABLE_DEPRECATION_WARNINGS

class FAndroidApplication : public GenericApplication
{
public:

	static FAndroidApplication* CreateAndroidApplication();

	// Returns the java environment
	static FORCEINLINE void InitializeJavaEnv(JavaVM* VM, jint Version, jobject GlobalThis)
	{
		AndroidJavaEnv::InitializeJavaEnv(VM, Version, GlobalThis);
    }
	static FORCEINLINE jobject GetGameActivityThis()
	{
		return AndroidJavaEnv::GetGameActivityThis();
	} 
	static FORCEINLINE jobject GetClassLoader()
	{
		return AndroidJavaEnv::GetClassLoader();
	} 
	static FORCEINLINE JNIEnv* GetJavaEnv(bool bRequireGlobalThis = true)
	{
		return AndroidJavaEnv::GetJavaEnv(bRequireGlobalThis);
	}
	static FORCEINLINE jclass FindJavaClass(const char* name)
	{
		return AndroidJavaEnv::FindJavaClass(name);
	}
	static FORCEINLINE void DetachJavaEnv()
	{
		AndroidJavaEnv::DetachJavaEnv();
	}
	static FORCEINLINE bool CheckJavaException()
	{
		return AndroidJavaEnv::CheckJavaException();
	}

	static FAndroidApplication* Get() { return _application; }

public:	
	
	virtual ~FAndroidApplication() {}

	void SetMessageHandler( const TSharedRef< FGenericApplicationMessageHandler >& InMessageHandler );

	virtual void PollGameDeviceState( const float TimeDelta ) override;

	virtual FPlatformRect GetWorkArea( const FPlatformRect& CurrentWindow ) const override;

	virtual IInputInterface* GetInputInterface() override;

	virtual TSharedRef< FGenericWindow > MakeWindow() override;

	virtual void AddExternalInputDevice(TSharedPtr<class IInputDevice> InputDevice);

	void InitializeWindow( const TSharedRef< FGenericWindow >& InWindow, const TSharedRef< FGenericWindowDefinition >& InDefinition, const TSharedPtr< FGenericWindow >& InParent, const bool bShowImmediately );

	static void OnWindowSizeChanged();

	virtual bool IsGamepadAttached() const override;

private:

	FAndroidApplication();


private:

	TSharedPtr< class FAndroidInputInterface > InputInterface;
	bool bHasLoadedInputPlugins;

	TArray< TSharedRef< FAndroidWindow > > Windows;

	static bool bWindowSizeChanged;

	static FAndroidApplication* _application;
};

PRAGMA_ENABLE_DEPRECATION_WARNINGS
