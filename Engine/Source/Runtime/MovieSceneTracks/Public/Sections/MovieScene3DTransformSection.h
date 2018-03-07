// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Components/SceneComponent.h"
#include "Curves/KeyHandle.h"
#include "Curves/RichCurve.h"
#include "MovieSceneSection.h"
#include "Sections/IKeyframeSection.h"
#include "MovieSceneKeyStruct.h"
#include "MovieScene3DTransformSection.generated.h"

class FStructOnScope;

namespace EKey3DTransformChannel
{
	enum Type
	{
		Translation = 0x00000001,
		Rotation = 0x00000002,
		Scale = 0x00000004,
		All = Translation | Rotation | Scale
	};
}

#if WITH_EDITORONLY_DATA
/** Visibility options for 3d trajectory. */
UENUM()
enum class EShow3DTrajectory : uint8
{
	EST_OnlyWhenSelected UMETA(DisplayName="Only When Selected"),
	EST_Always UMETA(DisplayName="Always"),
	EST_Never UMETA(DisplayName="Never"),
};
#endif

/**
* Stores information about a transform for the purpose of adding keys to a transform section
*/
struct FTransformData
{
	/** Translation component */
	FVector Translation;
	/** Rotation component */
	FRotator Rotation;
	/** Scale component */
	FVector Scale;
	/** Whether or not the data is valid (any values set) */
	bool bValid;

	bool IsValid() const { return bValid; }

	/**
	* Constructor.  Builds the data from a scene component
	* Uses relative transform only
	*
	* @param InComponent	The component to build from
	*/
	FTransformData( const USceneComponent* InComponent )
		: Translation( InComponent->RelativeLocation )
		, Rotation( InComponent->RelativeRotation )
		, Scale( InComponent->RelativeScale3D )
		, bValid( true )
	{}

	FTransformData()
		: bValid( false )
	{}
};


struct FTransformKey
{
	FTransformKey( EKey3DTransformChannel::Type InChannel, EAxis::Type InAxis, float InValue, bool InbUnwindRotation )
	{
		Channel = InChannel;
		Axis = InAxis;
		Value = InValue;
		bUnwindRotation = InbUnwindRotation;
	}
	EKey3DTransformChannel::Type Channel;
	EAxis::Type Axis;
	float Value;
	bool bUnwindRotation;
};


/**
 * Proxy structure for translation keys in 3D transform sections.
 */
USTRUCT()
struct FMovieScene3DLocationKeyStruct
	: public FMovieSceneKeyStruct
{
	GENERATED_BODY()

	/** The key's translation value. */
	UPROPERTY(EditAnywhere, Category=Key)
	FVector Location;

	/** The key's time. */
	UPROPERTY(EditAnywhere, Category=Key)
	float Time;

	FRichCurveKey* LocationKeys[3];

	virtual void PropagateChanges(const FPropertyChangedEvent& ChangeEvent) override;
};


/**
 * Proxy structure for translation keys in 3D transform sections.
 */
USTRUCT()
struct FMovieScene3DRotationKeyStruct
	: public FMovieSceneKeyStruct
{
	GENERATED_BODY()

	/** The key's rotation value. */
	UPROPERTY(EditAnywhere, Category=Key)
	FRotator Rotation;

	/** The key's time. */
	UPROPERTY(EditAnywhere, Category=Key)
	float Time;

	FRichCurveKey* RotationKeys[3];

	virtual void PropagateChanges(const FPropertyChangedEvent& ChangeEvent) override;
};


/**
 * Proxy structure for translation keys in 3D transform sections.
 */
USTRUCT()
struct FMovieScene3DScaleKeyStruct
	: public FMovieSceneKeyStruct
{
	GENERATED_BODY()

	/** The key's scale value. */
	UPROPERTY(EditAnywhere, Category=Key)
	FVector Scale;

	/** The key's time. */
	UPROPERTY(EditAnywhere, Category=Key)
	float Time;

	FRichCurveKey* ScaleKeys[3];

	virtual void PropagateChanges(const FPropertyChangedEvent& ChangeEvent) override;
};


/**
 * Proxy structure for 3D transform section key data.
 */
USTRUCT()
struct FMovieScene3DTransformKeyStruct
	: public FMovieSceneKeyStruct
{
	GENERATED_BODY()

	/** The key's translation value. */
	UPROPERTY(EditAnywhere, Category=Key)
	FVector Location;

	/** The key's rotation value. */
	UPROPERTY(EditAnywhere, Category=Key)
	FRotator Rotation;

	/** The key's scale value. */
	UPROPERTY(EditAnywhere, Category=Key)
	FVector Scale;

	/** The key's time. */
	UPROPERTY(EditAnywhere, Category=Key)
	float Time;

	FRichCurveKey* LocationKeys[3];
	FRichCurveKey* RotationKeys[3];
	FRichCurveKey* ScaleKeys[3];

	virtual void PropagateChanges(const FPropertyChangedEvent& ChangeEvent) override;
};

enum class EMovieSceneTransformChannel : uint32
{
	None			= 0x000,

	TranslationX 	= 0x001,
	TranslationY 	= 0x002,
	TranslationZ 	= 0x004,
	Translation 	= TranslationX | TranslationY | TranslationZ,

	RotationX 		= 0x008,
	RotationY 		= 0x010,
	RotationZ 		= 0x020,
	Rotation 		= RotationX | RotationY | RotationZ,

	ScaleX 			= 0x040,
	ScaleY 			= 0x080,
	ScaleZ 			= 0x100,
	Scale 			= ScaleX | ScaleY | ScaleZ,

	AllTransform	= Translation | Rotation | Scale,

	Weight 			= 0x200,

	All				= Translation | Rotation | Scale | Weight,
};
ENUM_CLASS_FLAGS(EMovieSceneTransformChannel)

USTRUCT()
struct FMovieSceneTransformMask
{
	GENERATED_BODY()

	FMovieSceneTransformMask()
		: Mask(0)
	{}

	FMovieSceneTransformMask(EMovieSceneTransformChannel Channel)
		: Mask((__underlying_type(EMovieSceneTransformChannel))Channel)
	{}

	EMovieSceneTransformChannel GetChannels() const
	{
		return (EMovieSceneTransformChannel)Mask;
	}

	FVector GetTranslationFactor() const
	{
		EMovieSceneTransformChannel Channels = GetChannels();
		return FVector(
			EnumHasAllFlags(Channels, EMovieSceneTransformChannel::TranslationX) ? 1.f : 0.f,
			EnumHasAllFlags(Channels, EMovieSceneTransformChannel::TranslationY) ? 1.f : 0.f,
			EnumHasAllFlags(Channels, EMovieSceneTransformChannel::TranslationZ) ? 1.f : 0.f);
	}

	FVector GetRotationFactor() const
	{
		EMovieSceneTransformChannel Channels = GetChannels();
		return FVector(
			EnumHasAllFlags(Channels, EMovieSceneTransformChannel::RotationX) ? 1.f : 0.f,
			EnumHasAllFlags(Channels, EMovieSceneTransformChannel::RotationY) ? 1.f : 0.f,
			EnumHasAllFlags(Channels, EMovieSceneTransformChannel::RotationZ) ? 1.f : 0.f);
	}

	FVector GetScaleFactor() const
	{
		EMovieSceneTransformChannel Channels = GetChannels();
		return FVector(
			EnumHasAllFlags(Channels, EMovieSceneTransformChannel::ScaleX) ? 1.f : 0.f,
			EnumHasAllFlags(Channels, EMovieSceneTransformChannel::ScaleY) ? 1.f : 0.f,
			EnumHasAllFlags(Channels, EMovieSceneTransformChannel::ScaleZ) ? 1.f : 0.f);
	}

private:

	UPROPERTY()
	uint32 Mask;
};

/**
 * A 3D transform section
 */
UCLASS(MinimalAPI)
class UMovieScene3DTransformSection
	: public UMovieSceneSection
	, public IKeyframeSection<FTransformKey>
{
	GENERATED_UCLASS_BODY()

public:

	/**
	 * Evaluates the translation component of the transform
	 *
	 * @param Time				The position in time within the movie scene
	 * @param OutTranslation	The evaluated translation.  Note: will remain unchanged if there were no keys to evaluate
	 */
	MOVIESCENETRACKS_API void EvalTranslation( float Time, FVector& InOutTranslation ) const;

	/**
	 * Evaluates the rotation component of the transform
	 *
	 * @param Time				The position in time within the movie scene
	 * @param OutRotation		The evaluated rotation.  Note: will remain unchanged if there were no keys to evaluate
	 */
	MOVIESCENETRACKS_API void EvalRotation( float Time, FRotator& InOutRotation ) const;

	/**
	 * Evaluates the scale component of the transform
	 *
	 * @param Time				The position in time within the movie scene
	 * @param OutScale			The evaluated scale.  Note: will remain unchanged if there were no keys to evaluate
	 */
	MOVIESCENETRACKS_API void EvalScale( float Time, FVector& InOutScale ) const;

	/** 
	 * Returns the translation curve for a specific axis
	 *
	 * @param Axis	The axis of the translation curve to get
	 * @return The curve on the axis
	 */
	MOVIESCENETRACKS_API FRichCurve& GetTranslationCurve( EAxis::Type Axis );
	MOVIESCENETRACKS_API const FRichCurve& GetTranslationCurve( EAxis::Type Axis ) const;

	/** 
	 * Returns the rotation curve for a specific axis
	 *
	 * @param Axis	The axis of the rotation curve to get
	 * @return The curve on the axis
	 */
	MOVIESCENETRACKS_API FRichCurve& GetRotationCurve( EAxis::Type Axis );
	MOVIESCENETRACKS_API const FRichCurve& GetRotationCurve( EAxis::Type Axis ) const;

	/** 
	 * Returns the scale curve for a specific axis
	 *
	 * @param Axis	The axis of the scale curve to get
	 * @return The curve on the axis
	 */
	MOVIESCENETRACKS_API FRichCurve& GetScaleCurve( EAxis::Type Axis );
	MOVIESCENETRACKS_API const FRichCurve& GetScaleCurve( EAxis::Type Axis ) const;

	/**
	 * Returns the manual weight curve for this section
	 * @return The manual weight curve
	 */
	MOVIESCENETRACKS_API FRichCurve& GetManualWeightCurve();
	MOVIESCENETRACKS_API const FRichCurve& GetManualWeightCurve() const;

	FMovieSceneTransformMask GetMask() const
	{
		return TransformMask;
	}

	void SetMask(FMovieSceneTransformMask NewMask)
	{
		TransformMask = NewMask;
	}

	/**
	 * Return the trajectory visibility
	 */
#if WITH_EDITORONLY_DATA
	MOVIESCENETRACKS_API EShow3DTrajectory GetShow3DTrajectory() { return Show3DTrajectory; }
#endif

public:

	// UMovieSceneSection interface

	virtual void MoveSection( float DeltaPosition, TSet<FKeyHandle>& KeyHandles ) override;
	virtual void DilateSection( float DilationFactor, float Origin, TSet<FKeyHandle>& KeyHandles ) override;
	virtual void GetKeyHandles(TSet<FKeyHandle>& OutKeyHandles, TRange<float> TimeRange) const override;
	virtual TSharedPtr<FStructOnScope> GetKeyStruct(const TArray<FKeyHandle>& KeyHandles) override;
	virtual TOptional<float> GetKeyTime( FKeyHandle KeyHandle ) const override;
	virtual void SetKeyTime( FKeyHandle KeyHandle, float Time ) override;

public:

	// IKeyframeSection interface.

	virtual bool NewKeyIsNewData( float Time, const FTransformKey& KeyData ) const override;
	virtual bool HasKeys( const FTransformKey& KeyData ) const override;
	virtual void AddKey( float Time, const FTransformKey& KeyData, EMovieSceneKeyInterpolation KeyInterpolation ) override;
	virtual void SetDefault( const FTransformKey& KeyData ) override;
	virtual void ClearDefaults() override;
	virtual FMovieSceneEvalTemplatePtr GenerateTemplate() const override;
	
private:

	UPROPERTY()
	FMovieSceneTransformMask TransformMask;

	/** Translation curves */
	UPROPERTY()
	FRichCurve Translation[3];
	
	/** Rotation curves */
	UPROPERTY()
	FRichCurve Rotation[3];

	/** Scale curves */
	UPROPERTY()
	FRichCurve Scale[3];

	/** Manual weight curve */
	UPROPERTY()
	FRichCurve ManualWeight;

#if WITH_EDITORONLY_DATA
	/** Whether to show the 3d trajectory */
	UPROPERTY(EditAnywhere, DisplayName = "Show 3D Trajectory", Category = "Transform")
	EShow3DTrajectory Show3DTrajectory;
#endif
};
