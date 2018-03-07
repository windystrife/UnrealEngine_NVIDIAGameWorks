// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	SceneViewExtension.h: Allow changing the view parameters on the render thread
=============================================================================*/

#pragma once

#include "CoreMinimal.h"

/**
 *  SCENE VIEW EXTENSIONS
 *  -----------------------------------------------------------------------------------------------
 *
 *  This system lets you hook various aspects of UE4 rendering.
 *  To create a view extension, it is advisable to inherit
 *  from FSceneViewExtensionBase, which implements the 
 *  ISceneViewExtension interface.
 *
 *
 *  
 *  INHERITING, INSTANTIATING, LIFETIME
 *  -----------------------------------------------------------------------------------------------
 *
 *  In order to inherit from FSceneViewExtensionBase, do the following:
 *
 *      class FMyExtension : public FSceneViewExtensionBase
 *      {
 *          public:
 *          FMyExtension( const FAutoRegister& AutoRegister, FYourParam1 Param1, FYourParam2 Param2 )
 *          : FSceneViewExtensionBase( AutoRegister )
 *          {
 *          }
 *      };
 *  
 *  Notice that your first argument must be FAutoRegister, and you must pass it
 *  to FSceneViewExtensionBase constructor. To instantiate your extension and register
 *  it, do the following:
 *
 *      FSceneViewExtensions::NewExtension<FMyExtension>(Param1, Param2);
 *
 *  You should maintain a reference to the extension for as long as you want to
 *  keep it registered.
 *
 *      TSharedRef<FMyExtension,ESPMode::ThreadSafe> MyExtension;
 *      MyExtension = FSceneViewExtensions::NewExtension<FMyExtension>(Param1, Param2);
 *
 *  If you follow this pattern, the cleanup of the extension will be safe and automatic
 *  whenever the `MyExtension` reference goes out of scope. In most cases, the `MyExtension`
 *  variable should be a member of the class owning the extension instance.
 *  
 *  The engine will keep the extension alive for the duration of the current frame to allow
 *  the render thread to finish.
 *
 *
 *
 *  OPTING OUT of RUNNING
 *  -----------------------------------------------------------------------------------------------
 *
 *  Each frame, the engine will invoke ISceneVewExtension::IsActiveThisFrame() to determine
 *  if your extension wants to run this frame. Returning false will cause none of the methods
 *  to be called this frame. The IsActiveThisFrame() method will be invoked again next frame.
 *
 *  If you need fine grained control over individual methods, your IsActiveThisFrame should
 *  return `true` and gate each method as needed.
 *
 *
 *
 *  PRIORITY
 *  -----------------------------------------------------------------------------------------------
 *  Extensions are executed in priority order. Higher priority extensions run first.
 *  To determine the priority of your extension, override ISceneViewExtension::GetPriority();
 *
 */

class APlayerController;
class FRHICommandListImmediate;
class FSceneView;
class FSceneViewFamily;
struct FMinimalViewInfo;
struct FSceneViewProjectionData;

class ISceneViewExtension
{
public:
	virtual ~ISceneViewExtension() {}

	/**
     * Called on game thread when creating the view family.
     */
    virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) = 0;

	/**
	 * Called on game thread when creating the view.
	 */
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) = 0;

	/**
	* Called when creating the viewpoint, before culling, in case an external tracking device needs to modify the base location of the view
	*/
	virtual void SetupViewPoint(APlayerController* Player, FMinimalViewInfo& InViewInfo) {}

    /**
	 * Called when creating the view, in case non-stereo devices need to update projection matrix.
	 */
	virtual void SetupViewProjectionMatrix(FSceneViewProjectionData& InOutProjectionData) {}

    /**
     * Called on game thread when view family is about to be rendered.
     */
    virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) = 0;

    /**
     * Called on render thread at the start of rendering.
     */
    virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) = 0;

	/**
     * Called on render thread at the start of rendering, for each view, after PreRenderViewFamily_RenderThread call.
     */
    virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) = 0;

	/**
	 * Called on render thread from FSceneRenderer::Render implementation after init views has completed, but before rendering proper has started
	 */
	virtual void PostInitViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) {};

	/**
	 * Called on render thread, for each view, after the PostInitViewFamily_RenderThread call
	 */
	virtual void PostInitView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) {};

	/**
	 * Called right after MobileBasePass rendering finished
	 */
	virtual void PostRenderMobileBasePass_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) {};

	/**
	 * Allows to render content after the 3D content scene, useful for debugging
	 */
	virtual void PostRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) {}

	/**
	 * Allows to render content after the 3D content scene, useful for debugging
	 */
	virtual void PostRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) {}

	/**
     * Called to determine view extensions priority in relation to other view extensions, higher comes first
     */
	virtual int32 GetPriority() const { return 0; }

	/**
	 * If true, use PostInitViewFamily_RenderThread and PostInitView_RenderThread instead of PreRenderViewFamily_RenderThread and PreRenderView_RenderThread.
	 * Note: Frustum culling will have already happened in init views. This may require a small FOV buffer when culling to account for view changes/updates.
	 */
	virtual bool UsePostInitView() const { return false; }

	/**
	 * Returning false disables the extension for the current frame. This will be queried each frame to determine if the extension wants to run.
	 */
	virtual bool IsActiveThisFrame(class FViewport* InViewport) const { return true; }
};



/**
 * Used to ensure that all extensions are constructed
 * via FSceneViewExtensions::NewExtension<T>(Args).
 */
class FAutoRegister
{
	friend class FSceneViewExtensions;
	FAutoRegister(){}
};



/** Inherit from this class to make a view extension. */
class ENGINE_API FSceneViewExtensionBase : public ISceneViewExtension, public TSharedFromThis<FSceneViewExtensionBase, ESPMode::ThreadSafe>
{
public:
	FSceneViewExtensionBase(const FAutoRegister&) {}
	virtual ~FSceneViewExtensionBase();
};



/**
 * Repository of all registered scene view extensions.
 */
class ENGINE_API FSceneViewExtensions
{
	friend class FSceneViewExtensionBase;

public:
	/**
	 * Create a new extension of type ExtensionType.
	 */
	template<typename ExtensionType, typename... TArgs>
	static TSharedRef<ExtensionType, ESPMode::ThreadSafe> NewExtension( TArgs&&... Args )
	{
		TSharedRef<ExtensionType, ESPMode::ThreadSafe> NewExtension = MakeShareable(new ExtensionType( FAutoRegister(), Forward<TArgs>(Args)... ));
		RegisterExtension(NewExtension);
		return NewExtension;
	}

	/**
	 * Gathers all ViewExtensions that want to be active this frame (@see ISceneViewExtension::IsActiveThisFrame()).
	 * The list is sorted by priority (@see ISceneViewExtension::GetPriority())
	 */
	const TArray<TSharedRef<class ISceneViewExtension, ESPMode::ThreadSafe>> GatherActiveExtensions(class FViewport* InViewport = nullptr) const;

private:
	static void RegisterExtension(const TSharedRef<class ISceneViewExtension, ESPMode::ThreadSafe>& RegisterMe);
	TArray< TWeakPtr<class ISceneViewExtension, ESPMode::ThreadSafe> > KnownExtensions;
};

