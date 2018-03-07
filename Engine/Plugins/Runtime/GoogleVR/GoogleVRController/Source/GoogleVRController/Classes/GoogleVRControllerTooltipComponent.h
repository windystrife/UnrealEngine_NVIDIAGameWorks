// Copyright 2017 Google Inc.

#pragma once

#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Materials/MaterialParameterCollection.h"
#include "GoogleVRControllerTooltipComponent.generated.h"

class UMotionControllerComponent;

UENUM(BlueprintType)
enum class EGoogleVRControllerTooltipLocation : uint8
{
	TouchPadOutside,
	TouchPadInside,
	AppButtonOutside,
	AppButtonInside,
	None
};

UCLASS(ClassGroup=(GoogleVRController), meta=(BlueprintSpawnableComponent))
class GOOGLEVRCONTROLLER_API UGoogleVRControllerTooltipComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UGoogleVRControllerTooltipComponent(const FObjectInitializer& ObjectInitializer);

	/** Determines the location of this tooltip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Location")
	EGoogleVRControllerTooltipLocation TooltipLocation;

	/** Text to display for the tooltip. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Text")
	UTextRenderComponent* TextRenderComponent;

	/** Parameter collection used to set the alpha of the tooltip.
	 *  Must include property named "GoogleVRControllerTooltipAlpha".
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Materials")
	UMaterialParameterCollection* ParameterCollection;

	/** Called when the tooltip changes sides.
	 *  This happens when the handedness of the user changes.
	 *  Override to update the visual based on which side it is on.
	 */
	virtual void OnSideChanged(bool IsLocationOnLeft);

	/** Blueprint implementable event for when the tooltip changes sides.
	 *  This happens when the handedness of the user changes.
	 *  Override to update the visual based on which side it is on.
	 */
	UFUNCTION(BlueprintImplementableEvent, meta=(DisplayName = "On Side Changed"))
	void ReceiveOnSideChanged(bool IsLocationOnLeft);

	virtual void BeginPlay() override;

	virtual void TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction ) override;

private:

	void RefreshTooltipLocation();
	FVector GetRelativeLocation() const;
	bool IsTooltipInside() const;
	bool IsTooltipOnLeft() const;
	float GetWorldToMetersScale() const;

	UMotionControllerComponent* MotionController;
	bool IsOnLeft;
};
