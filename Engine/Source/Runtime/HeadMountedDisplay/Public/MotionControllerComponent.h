// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/PrimitiveComponent.h"
#include "SceneViewExtension.h"
#include "IMotionController.h"
#include "LateUpdateManager.h"
#include "MotionControllerComponent.generated.h"

class FPrimitiveSceneInfo;
class FRHICommandListImmediate;
class FSceneView;
class FSceneViewFamily;

UCLASS(Blueprintable, meta = (BlueprintSpawnableComponent), ClassGroup = MotionController)
class HEADMOUNTEDDISPLAY_API UMotionControllerComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()

	~UMotionControllerComponent();

	/** Which player index this motion controller should automatically follow */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionController")
	int32 PlayerIndex;

	/** Which hand this component should automatically follow */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionController")
	EControllerHand Hand;

	/** If false, render transforms within the motion controller hierarchy will be updated a second time immediately before rendering. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MotionController")
	uint32 bDisableLowLatencyUpdate:1;

	/** The tracking status for the device (e.g. full tracking, inertial tracking only, no tracking) */
	UPROPERTY(BlueprintReadOnly, Category = "MotionController")
	ETrackingStatus CurrentTrackingStatus;

	void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction) override;

	/** Whether or not this component had a valid tracked device this frame */
	UFUNCTION(BlueprintPure, Category = "MotionController")
	bool IsTracked() const
	{
		return bTracked;
	}

protected:
	//~ Begin UActorComponent Interface.
	virtual void SendRenderTransform_Concurrent() override;
	//~ End UActorComponent Interface.

private:
	/** Whether or not this component had a valid tracked controller associated with it this frame*/
	bool bTracked;

	/** Whether or not this component has authority within the frame*/
	bool bHasAuthority;

	/** If true, the Position and Orientation args will contain the most recent controller state */
	bool PollControllerState(FVector& Position, FRotator& Orientation, float WorldToMetersScale);

	FTransform RenderThreadRelativeTransform;
	FVector RenderThreadComponentScale;

	/** View extension object that can persist on the render thread without the motion controller component */
	class FViewExtension : public FSceneViewExtensionBase
	{
	public:
		FViewExtension(const FAutoRegister& AutoRegister, UMotionControllerComponent* InMotionControllerComponent)
			: FSceneViewExtensionBase(AutoRegister)
			, MotionControllerComponent(InMotionControllerComponent)
		{}
		virtual ~FViewExtension() {}

		/** ISceneViewExtension interface */
		virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
		virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
		virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override;
		virtual void PreRenderView_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneView& InView) override {}
		virtual void PreRenderViewFamily_RenderThread(FRHICommandListImmediate& RHICmdList, FSceneViewFamily& InViewFamily) override;
		virtual int32 GetPriority() const override { return -10; }
		virtual bool IsActiveThisFrame(class FViewport* InViewport) const;

	private:
		friend class UMotionControllerComponent;

		/** Motion controller component associated with this view extension */
		UMotionControllerComponent* MotionControllerComponent;
		FLateUpdateManager LateUpdate;
	};
	TSharedPtr< FViewExtension, ESPMode::ThreadSafe > ViewExtension;
};
