// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "ObjectMacros.h"
#include "UObject/Object.h"
#if WITH_EDITOR
#include "PropertyPath.h"
#endif
#include "ControlManipulator.generated.h"

UENUM(BlueprintType)
enum class EIKSpaceMode : uint8
{
	/** Switch To IK Mode */
	IKMode		UMETA(DisplayName = "IK Mode"),

	/** Switch to FK Mode */
	FKMode		UMETA(DisplayName = "FK Mode"),

	/** Use Weight */
	UseWeight	UMETA(Hidden),
};

class UStaticMeshComponent;
class FSceneView;
class FViewport;
class FPrimitiveDrawInterface;
class UMaterialInstanceDynamic;

/** The different parts of a transform that manipulators can support */
enum class ETransformComponent
{
	None,

	Rotation,

	Translation,

	Scale
};

/** Manipulator that represents an input. Add variables to your controller of this type and they will be picked up as a manipulator for the controller */
UCLASS(Abstract, editinlinenew)
class CONTROLRIG_API UControlManipulator : public UObject
{
	GENERATED_BODY()

public:
	/** The name of this manipulator */
	UPROPERTY(EditAnywhere, Category = "Manipulator")
	FText DisplayName;

	/** The name of this manipulator's node */
	UPROPERTY(EditAnywhere, Category = "Manipulator")
	FName Name;

	/** The name of the property that this manipulator controls (this can be a transform, vector, rotator etc.) */
	UPROPERTY(EditAnywhere, Category = "Manipulator")
	FName PropertyToManipulate;

	/** The property chain leading to the cached property */
	UPROPERTY(Transient)
	mutable TArray<UProperty*> CachedPropertyChain;

	/** The property that this manipulator controls (this can be a transform, vector, rotator etc.) */
	UPROPERTY(Transient)
	mutable UProperty* CachedProperty;

	/** The address for CachedProperty */
	mutable void* CachedPropertyAddress;

	/** Whether this manipulator works in Inverse or Forward kinematic space */
	UPROPERTY(EditAnywhere, Category = "Kinematics")
	EIKSpaceMode KinematicSpace;

	/** When enabled, this manipulator will affect translation */
	UPROPERTY(EditAnywhere, Category = "Transform Components")
	bool bUsesTranslation;

	/** When enabled, this manipulator will affect rotation */
	UPROPERTY(EditAnywhere, Category = "Transform Components")
	bool bUsesRotation;

	/** When enabled, this manipulator will affect scales */
	UPROPERTY(EditAnywhere, Category = "Transform Components")
	bool bUsesScale;

	/** When enabled, this manipulator will save/restore transform on local space. 
	 * However this space conversion all happens externally. This manipulator only knows its transform. 
	 * This is saved here, so that it can be used externally correctly. */
	UPROPERTY(EditAnywhere, Category = "Transform Components")
	bool bInLocalSpace;

#if WITH_EDITORONLY_DATA
	/** The proximity of the cursor, used for editor highlighting */
	float CurrentProximity;

	/** The desired proximity of the cursor, used for editor highlighting */
	float TargetProximity;

	/** Whether to notify listeners of any property changes */
	bool bNotifyListeners;

	/** Whether we are currently manipulating */
	bool bManipulating;

	/** Position of manipulator button in picker panel */
	UPROPERTY(EditAnywhere, Category = "Picker")
	FVector2D PickerPos;
	/** Size of manipulator button in picker panel */
	UPROPERTY(EditAnywhere, Category = "Picker")
	FVector2D PickerSize;

	/** Cached property path for our property, used for keying in Sequencer */
	mutable FPropertyPath CachedPropertyPath;
#endif

public:
	UControlManipulator()
		: CachedPropertyAddress(nullptr)
		, KinematicSpace(EIKSpaceMode::UseWeight)
		, bUsesTranslation(true)
		, bUsesRotation(true)
		, bUsesScale(false)
		, bInLocalSpace(false)
#if WITH_EDITOR
		, CurrentProximity(0.0f)
		, TargetProximity(1.0f)
		, bNotifyListeners(true)
		, bManipulating(false)
		, PickerPos(FVector2D::ZeroVector)
		, PickerSize(FVector2D(20,20))
#endif
	{}

	/** Set up any internal data on initial tick */
	virtual void Initialize(UObject* InContainer);

#if WITH_EDITOR
	/** Draw this manipulator */
	virtual void Draw(const FTransform& InTransform, const FSceneView* View, FPrimitiveDrawInterface* PDI, bool bIsSelected) {};
#endif

	/** 
	 * Sets the location of this manipulator 
	 * @param	InLocation		The location to set
	 * @param	InContainer		The container object to apply any property changes to
	 */
	virtual void SetLocation(const FVector& InLocation, UObject* InContainer);

	/**
	 * Gets the location of this manipulator
	 * @param	InContainer		The container object to get property changes from
	 * @return	the location of this manipulator
	 */
	virtual FVector GetLocation(const UObject* InContainer) const;

	/** 
	 * Sets the rotation of this manipulator 
	 * @param	InRotation		The rotation to set
	 * @param	InContainer		The container object to apply any property changes to
	 */
	virtual void SetRotation(const FRotator& InRotation, UObject* InContainer);

	/**
	 * Gets the rotation of this manipulator
	 * @param	InContainer		The container object to get property changes from
	 * @return	the rotation of this manipulator
	 */
	virtual FRotator GetRotation(const UObject* InContainer) const;

	/** 
	 * Sets the rotation of this manipulator as a quaternion
	 * @param	InQuat			The rotation to set
	 * @param	InContainer		The container object to apply any property changes to
	 */
	virtual void SetQuat(const FQuat& InQuat, UObject* InContainer);

	/**
	 * Gets the rotation of this manipulator
	 * @param	InContainer		The container object to get property changes from
	 * @return	the rotation of this manipulator as a quaternion
	 */
	virtual FQuat GetQuat(const UObject* InContainer) const;

	/** 
	 * Sets the scale of this manipulator 
	 * @param	InScale		The scale to set
	 * @param	InContainer		The container object to apply any property changes to
	 */
	virtual void SetScale(const FVector& InScale, UObject* InContainer);

	/**
	 * Gets the scale of this manipulator
	 * @param	InContainer		The container object to get property changes from
	 * @return	the scale of this manipulator
	 */
	virtual FVector GetScale(const UObject* InContainer) const;

	/** 
	 * Sets the transform of this manipulator 
	 * @param	InTransfrom		The transform to set
	 * @param	InContainer		The container object to apply any property changes to
	 */
	virtual void SetTransform(const FTransform& InTransform, UObject* InContainer);

	/**
	 * Gets the transform of this manipulator
	 * @param	InContainer		The container object to get property changes from
	 * @return	the transform of this manipulator
	 */
	virtual FTransform GetTransform(const UObject* InContainer) const;

#if WITH_EDITOR
	/** 
	 * Check whether a transform component is supported by this manipulator
	 * @param	InComponent		The component to check
	 */
	virtual bool SupportsTransformComponent(ETransformComponent InComponent) const;

	/** Get the local bounds of this manipulator */
	virtual FBox GetLocalBoundingBox() const { return FBox(FVector(-0.5f), FVector(0.5f)); }

	/** Get the local bounds of this manipulator */
	virtual FSphere GetLocalBoundingSphere() const { return FSphere(FVector::ZeroVector, 1.0f); }
#endif

	/** Let the target object know we have changed one of its properties */
	void NotifyPostEditChangeProperty(UObject* InContainer);

protected:
	/** Cache the property this manipulator references */
	void CacheProperty(const UObject* InContainer) const;

	/** Let the target object know we are about to change one of its properties */
	void NotifyPreEditChangeProperty(UObject* InContainer);
};

UCLASS(Abstract, editinlinenew)
class CONTROLRIG_API UColoredManipulator : public UControlManipulator
{
	GENERATED_BODY()

public:
	UColoredManipulator();

	// The color of this manipulator */
	UPROPERTY(EditAnywhere, Category = "Color")
	FLinearColor Color;

	// The selected color of this manipulator */
	UPROPERTY(EditAnywhere, Category = "Color")
	FLinearColor SelectedColor;

#if WITH_EDITOR
	virtual void Draw(const FTransform& InTransform, const FSceneView* View, FPrimitiveDrawInterface* PDI, bool bIsSelected) override;
#endif

protected:
#if WITH_EDITORONLY_DATA
	/** Material we use for rendering with a single color */
	UPROPERTY(Transient)
	TWeakObjectPtr<UMaterialInstanceDynamic> ColorMaterial;
#endif
};

UCLASS(editinlinenew)
class CONTROLRIG_API USphereManipulator : public UColoredManipulator
{
	GENERATED_BODY()

public:
	USphereManipulator();

	/** The name of this manipulator */
	UPROPERTY(EditAnywhere, Category = "Sphere")
	float Radius;

public:
#if WITH_EDITOR
	virtual void Draw(const FTransform& InTransform, const FSceneView* View, FPrimitiveDrawInterface* PDI, bool bIsSelected) override;
	virtual FBox GetLocalBoundingBox() const override { return FBox(FVector(-Radius), FVector(Radius)); }
	virtual FSphere GetLocalBoundingSphere() const override { return FSphere(FVector::ZeroVector, Radius); }
#endif
};

UCLASS(editinlinenew)
class CONTROLRIG_API UBoxManipulator : public UColoredManipulator
{
	GENERATED_BODY()

public:
	UBoxManipulator();

	/** The name of this manipulator */
	UPROPERTY(EditAnywhere, Category = "Size")
	FVector BoxExtent;

public:
#if WITH_EDITOR
	virtual void Draw(const FTransform& InTransform, const FSceneView* View, FPrimitiveDrawInterface* PDI, bool bIsSelected) override;
	virtual FBox GetLocalBoundingBox() const override { return FBox(-BoxExtent, BoxExtent); }
	virtual FSphere GetLocalBoundingSphere() const override { return FSphere(FVector::ZeroVector, BoxExtent.GetMax()); }
#endif
};

