// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "UObject/ScriptMacros.h"
#include "Leap_NoPI.h"
#include "LeapImage.generated.h"

/**
* The Image class represents a single image from one of the Leap Motion cameras.
* You can obtain the images from your frame object or from listening to raw image events in LeapEventInterface
*
* Leap API reference: https://developer.leapmotion.com/documentation/cpp/api/Leap.Image.html
*/
UCLASS(BlueprintType)
class LEAPMOTION_API ULeapImage : public UObject
{
	friend class FPrivateLeapImage;

	GENERATED_UCLASS_BODY()
public:
	~ULeapImage();

	/**
	* Returns a UTexture2D reference that contains the latest raw Leap Image data in 
	* UE format. This can be optionally gamma corrected.
	*
	* @return Image in converted UTexture2D format
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Texture", CompactNodeTitle = "", Keywords = "get texture"), Category = "Leap Image")
	class UTexture2D* Texture();
	
	/**
	* Faster raw distortion (R=U, G=V), requires channel conversion, 32bit float per 
	* channel texture will look odd if rendered raw.
	*
	* @return 128bit Distortion in raw UTexture2D format
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Distortion", CompactNodeTitle = "", Keywords = "distortion"), Category = "Leap Image")
	class UTexture2D* Distortion();

	/**
	* Visually correct distortion in UE format (R=U, G=1-V) at the cost of additional 
	* CPU time (roughly 1ms) in 8bit per channel format
	*
	* @return Distortion in converted UTexture2D format
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "Distortion UE", CompactNodeTitle = "", Keywords = "distortion ue"), Category = "Leap Image")
	class UTexture2D* DistortionUE();

	/**
	* The distortion map height.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Image")
	int32 DistortionHeight;

	/**
	* The stride of the distortion map.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Image")
	int32 DistortionWidth;

	/**
	* The image height.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Image")
	int32 Height;

	/**
	* The image ID. Images with ID of 0 are from the left camera;
	* those with an ID of 1 are from the right camera (with the device in its standard operating 
	* position with the green LED facing the operator).
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Image")
	int32 Id;

	/**
	* Reports whether this Image instance contains valid data.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Image")
	bool IsValid;

	/**
	* The horizontal ray offset.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Image")
	float RayOffsetX;

	/**
	* The vertical ray offset.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Image")
	float RayOffsetY;

	/**
	* The horizontal ray scale factor.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Image")
	float RayScaleX;

	/**
	* The vertical ray scale factor.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Image")
	float RayScaleY;

	/**
	* Provides the corrected camera ray intercepting the specified point on the image.
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "rectify", CompactNodeTitle = "", Keywords = "rectify"), Category = "Leap Image")
	FVector Rectify(FVector uv) const;

	/**
	* Whether this image should apply gamma correction when fetching the texture.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Image")
	bool UseGammaCorrection;

	/**
	* Provides the point in the image corresponding to a ray projecting from the camera.
	* Given a ray projected from the camera in the specified direction, warp() corrects 
	* for camera distortion and returns the corresponding pixel coordinates in the image.
	* The ray direction is specified in relationship to the camera. The first vector element
	* corresponds to the “horizontal” view angle; the second corresponds to the “vertical” 
	* view angle.
	*
	* @param	XY	A Vector containing the ray direction.
	* @return	A Vector containing the pixel coordinates [x, y, 0] (with z always zero).
	*/
	UFUNCTION(BlueprintCallable, meta = (DisplayName = "warp", CompactNodeTitle = "", Keywords = "warp"), Category = "Leap Image")
	FVector Warp(FVector XY) const;

	/**
	* The image width.
	*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Leap Image")
	int32 Width;


	void SetLeapImage(const class Leap::Image &LeapImage);

private:
	class FPrivateLeapImage* Private;

	UPROPERTY()
	UTexture2D* PImagePointer = nullptr;
	UPROPERTY()
	UTexture2D* PDistortionPointer = nullptr;
};