// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.



//=============================================================================
// Unreal base structures.

// Temporary mirrors of C++ structs, used mainly as forward declarations for the core object module
// to avoid including the full engine source. In most cases the full class definition is in another
// file and is noted as such. More complete documentation will generally be found in those files.

#pragma once

#if CPP

// Include the real definitions of the noexport classes below to allow the generated cpp file to compile.

#include "PixelFormat.h"

#include "Misc/Guid.h"
#include "Misc/DateTime.h"
#include "Misc/Timespan.h"
#include "UObject/SoftObjectPath.h"

#include "Math/InterpCurvePoint.h"
#include "Math/UnitConversion.h"
#include "Math/Vector.h"
#include "Math/Vector4.h"
#include "Math/Vector2D.h"
#include "Math/TwoVectors.h"
#include "Math/Plane.h"
#include "Math/Rotator.h"
#include "Math/Quat.h"
#include "Math/IntPoint.h"
#include "Math/IntVector.h"
#include "Math/Color.h"
#include "Math/Box.h"
#include "Math/Box2D.h"
#include "Math/BoxSphereBounds.h"
#include "Math/OrientedBox.h"
#include "Math/Matrix.h"
#include "Math/ScalarRegister.h"
#include "Math/RandomStream.h"
#include "Math/RangeBound.h"
#include "Math/Interval.h"

#include "GenericPlatform/ICursor.h"

#endif

#if !CPP      //noexport class

/** String search case used in UnrealString.h */
UENUM()
namespace ESearchCase
{
	enum Type
	{
		CaseSensitive,
		IgnoreCase,
	};
}

/** String search dir used in UnrealString.h */
UENUM()
namespace ESearchDir
{
	enum Type
	{
		FromStart,
		FromEnd,
	};
}

/**
 * Enum that defines how the log times are to be displayed (mirrored from OutputDevice.h).
 */
UENUM()
namespace ELogTimes
{
	enum Type
	{
		/** Do not display log timestamps. */
		None UMETA(DisplayName = "None"),

		/** Display log timestamps in UTC. */
		UTC UMETA(DisplayName = "UTC"),

		/** Display log timestamps in seconds elapsed since GStartTime. */
		SinceGStartTime UMETA(DisplayName = "Time since application start"),

		/** Display log timestamps in local time. */
		Local UMETA(DisplayName = "Local time"),
	};
}

/** Generic axis enum (mirrored for native use in Axis.h). */
UENUM()
namespace EAxis
{
	enum Type
	{
		None,
		X,
		Y,
		Z
	};
}

// Interpolation data types.
UENUM()
enum EInterpCurveMode
{
	CIM_Linear UMETA(DisplayName="Linear"),
	CIM_CurveAuto UMETA(DisplayName="Curve Auto"),
	CIM_Constant UMETA(DisplayName="Constant"),
	CIM_CurveUser UMETA(DisplayName="Curve User"),
	CIM_CurveBreak UMETA(DisplayName="Curve Break"),
	CIM_CurveAutoClamped UMETA(DisplayName="Curve Auto Clamped"),
};

// @warning:	When you update this, you must add an entry to GPixelFormats(see RenderUtils.cpp)
// @warning:	When you update this, you must add an entries to PixelFormat.h, usually just copy the generated section on the header into EPixelFormat
// @warning:	The *Tools DLLs will also need to be recompiled if the ordering is changed, but should not need code changes.

UENUM()
enum EPixelFormat
{
	PF_Unknown,
	PF_A32B32G32R32F,
	/** UNORM (0..1), corresponds to FColor.  Unpacks as rgba in the shader. */
	PF_B8G8R8A8,
	/** UNORM red (0..1) */
	PF_G8,
	PF_G16,
	PF_DXT1,
	PF_DXT3,
	PF_DXT5,
	PF_UYVY,
	/** Same as PF_FloatR11G11B10 */
	PF_FloatRGB,
	/** RGBA 16 bit signed FP format.  Use FFloat16Color on the CPU. */
	PF_FloatRGBA,
	/** A depth+stencil format with platform-specific implementation, for use with render targets. */
	PF_DepthStencil,
	/** A depth format with platform-specific implementation, for use with render targets. */
	PF_ShadowDepth,
	PF_R32_FLOAT,
	PF_G16R16,
	PF_G16R16F,
	PF_G16R16F_FILTER,
	PF_G32R32F,
	PF_A2B10G10R10,
	PF_A16B16G16R16,
	PF_D24,
	PF_R16F,
	PF_R16F_FILTER,
	PF_BC5,
	/** SNORM red, green (-1..1). Not supported on all RHI e.g. Metal */
	PF_V8U8,
	PF_A1,
	/** A low precision floating point format, unsigned.  Use FFloat3Packed on the CPU. */
	PF_FloatR11G11B10,
	PF_A8,
	PF_R32_UINT,
	PF_R32_SINT,
	PF_PVRTC2,
	PF_PVRTC4,
	PF_R16_UINT,
	PF_R16_SINT,
	PF_R16G16B16A16_UINT,
	PF_R16G16B16A16_SINT,
	PF_R5G6B5_UNORM,
	PF_R8G8B8A8,
	/** Only used for legacy loading; do NOT use! */
	PF_A8R8G8B8,
	/** High precision single channel block compressed, equivalent to a single channel BC5, 8 bytes per 4x4 block. */
	PF_BC4,
	/** UNORM red, green (0..1). */
	PF_R8G8,
	/** ATITC format. */
	PF_ATC_RGB,
	/** ATITC format. */
	PF_ATC_RGBA_E,
	/** ATITC format. */
	PF_ATC_RGBA_I,
	/** Used for creating SRVs to alias a DepthStencil buffer to read Stencil.  Don't use for creating textures. */
	PF_X24_G8,
	PF_ETC1,
	PF_ETC2_RGB,
	PF_ETC2_RGBA,
	PF_R32G32B32A32_UINT,
	PF_R16G16_UINT,
	/** 8.00 bpp */
	PF_ASTC_4x4,
	/** 3.56 bpp */
	PF_ASTC_6x6,
	/** 2.00 bpp */
	PF_ASTC_8x8,
	/** 1.28 bpp */
	PF_ASTC_10x10,
	/** 0.89 bpp */
	PF_ASTC_12x12,
	PF_BC6H,
	PF_BC7,
	PF_R8_UINT,
	PF_L8,
	PF_XGXR8,
	PF_R8G8B8A8_UINT,
	/** SNORM (-1..1), corresponds to FFixedRGBASigned8. */
	PF_R8G8B8A8_SNORM, 
	PF_MAX,
};

UENUM()
namespace EMouseCursor
{
	enum Type
	{
		/** Causes no mouse cursor to be visible. */
		None,

		/** Default cursor (arrow). */
		Default,

		/** Text edit beam. */
		TextEditBeam,

		/** Resize horizontal. */
		ResizeLeftRight,

		/** Resize vertical. */
		ResizeUpDown,

		/** Resize diagonal. */
		ResizeSouthEast,

		/** Resize other diagonal. */
		ResizeSouthWest,

		/** MoveItem. */
		CardinalCross,

		/** Target Cross. */
		Crosshairs,

		/** Hand cursor. */
		Hand,

		/** Grab Hand cursor. */
		GrabHand,

		/** Grab Hand cursor closed. */
		GrabHandClosed,

		/** a circle with a diagonal line through it. */
		SlashedCircle,

		/** Eye-dropper cursor for picking colors. */
		EyeDropper,
	};
}

/** A set of numerical unit types supported by the engine. Mirrored from UnitConversion.h */
UENUM(BlueprintType)
enum class EUnit : uint8
{
	/** Scalar distance/length unit. */
	Micrometers, Millimeters, Centimeters, Meters, Kilometers, Inches, Feet, Yards, Miles, Lightyears,
	
	/** Angular units */
	Degrees, Radians,
	
	/** Speed units */
	MetersPerSecond, KilometersPerHour, MilesPerHour,
	
	/** Temperature units */
	Celsius, Farenheit, Kelvin,
	
	/** Mass units */
	Micrograms, Milligrams, Grams, Kilograms, MetricTons, Ounces, Pounds, Stones,
	
	/** Force units */
	Newtons, PoundsForce, KilogramsForce,
	
	/** Frequency units */
	Hertz, Kilohertz, Megahertz, Gigahertz, RevolutionsPerMinute,
	
	/** Data Size units */	
	Bytes, Kilobytes, Megabytes, Gigabytes, Terabytes,
	
	/** Luminous flux units */	
	Lumens,
	
	/** Time units */	
	Milliseconds, Seconds, Minutes, Hours, Days, Months, Years,

	/** Arbitrary multiplier */	
	Multiplier,


	/** Percentage */
	
	Percentage,

	/** Symbolic entry, not specifiable on meta data. */
	
	Unspecified
};

/** A globally unique identifier. */
USTRUCT(immutable, noexport, BlueprintType)
struct FGuid
{
	UPROPERTY(EditAnywhere, SaveGame, Category=Guid)
	int32 A;

	UPROPERTY(EditAnywhere, SaveGame, Category=Guid)
	int32 B;

	UPROPERTY(EditAnywhere, SaveGame, Category=Guid)
	int32 C;

	UPROPERTY(EditAnywhere, SaveGame, Category=Guid)
	int32 D;
};

/**
 * A point or direction FVector in 3d space.
 * The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Vector.h
 */
USTRUCT(immutable, noexport, BlueprintType, meta=(HasNativeMake="Engine.KismetMathLibrary.MakeVector", HasNativeBreak="Engine.KismetMathLibrary.BreakVector"))
struct FVector
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Vector, SaveGame)
	float X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Vector, SaveGame)
	float Y;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Vector, SaveGame)
	float Z;
};

/**
* A 4-D homogeneous vector.
* The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Vector4.h
*/
USTRUCT(immutable, noexport, BlueprintType)
struct FVector4
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Vector4, SaveGame)
	float X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Vector4, SaveGame)
	float Y;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Vector4, SaveGame)
	float Z;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Vector4, SaveGame)
	float W;

};

/**
* A point or direction FVector in 2d space.
* The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Vector2D.h
*/
USTRUCT(immutable, noexport, BlueprintType, meta=(HasNativeMake="Engine.KismetMathLibrary.MakeVector2D", HasNativeBreak="Engine.KismetMathLibrary.BreakVector2D"))
struct FVector2D
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Vector2D, SaveGame)
	float X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Vector2D, SaveGame)
	float Y;

};



USTRUCT(immutable, BlueprintType, noexport)
struct FTwoVectors
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TwoVectors, SaveGame)
	FVector v1;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=TwoVectors, SaveGame)
	FVector v2;

};


/**
 * A plane definition in 3D space.
 * The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Plane.h
 */
USTRUCT(immutable, noexport, BlueprintType)
struct FPlane : public FVector
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Plane, SaveGame)
	float W;

};


/**
 * An orthogonal rotation in 3d space.
 * The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Rotator.h
 */
USTRUCT(immutable, noexport, BlueprintType, meta=(HasNativeMake="Engine.KismetMathLibrary.MakeRotator", HasNativeBreak="Engine.KismetMathLibrary.BreakRotator"))
struct FRotator
{
	/** Pitch (degrees) around Y axis */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Rotator, SaveGame, meta=(DisplayName="Y"))
	float Pitch;

	/** Yaw (degrees) around Z axis */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Rotator, SaveGame, meta=(DisplayName="Z"))
	float Yaw;

	/** Roll (degrees) around X axis */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Rotator, SaveGame, meta=(DisplayName="X"))
	float Roll;

};


/**
 * Quaternion.
 * The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Quat.h
 */
USTRUCT(immutable, noexport, BlueprintType)
struct FQuat
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Quat, SaveGame)
	float X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Quat, SaveGame)
	float Y;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Quat, SaveGame)
	float Z;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Quat, SaveGame)
	float W;

};


/**
 * A packed normal.
 * The full C++ class is located here: Engine\Source\Runtime\RenderCore\Public\PackedNormal.h
 */
USTRUCT(immutable, noexport)
struct FPackedNormal
{
	UPROPERTY(EditAnywhere, Category=PackedNormal, SaveGame)
	uint8 X;

	UPROPERTY(EditAnywhere, Category=PackedNormal, SaveGame)
	uint8 Y;

	UPROPERTY(EditAnywhere, Category=PackedNormal, SaveGame)
	uint8 Z;

	UPROPERTY(EditAnywhere, Category=PackedNormal, SaveGame)
	uint8 W;

};

/**
* A packed basis vector.
* The full C++ class is located here: Engine\Source\Runtime\RenderCore\Public\PackedNormal.h
*/
USTRUCT(immutable, noexport)
struct FPackedRGB10A2N
{
	UPROPERTY(EditAnywhere, Category = PackedBasis, SaveGame)
	int32 Packed;
};

/**
* A packed vector.
* The full C++ class is located here: Engine\Source\Runtime\RenderCore\Public\PackedNormal.h
*/
USTRUCT(immutable, noexport)
struct FPackedRGBA16N
{
	UPROPERTY(EditAnywhere, Category = PackedNormal, SaveGame)
	int32 XY;

	UPROPERTY(EditAnywhere, Category = PackedNormal, SaveGame)
	int32 ZW;
};

/**
 * Screen coordinates.
 * The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\IntPoint.h
 */
USTRUCT(immutable, noexport, BlueprintType)
struct FIntPoint
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=IntPoint, SaveGame)
	int32 X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=IntPoint, SaveGame)
	int32 Y;

};

/**
 * An integer vector in 3D space.
 * The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\IntVector.h
 */
USTRUCT(immutable, noexport, BlueprintType)
struct FIntVector
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=IntVector, SaveGame)
	int32 X;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=IntVector, SaveGame)
	int32 Y;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=IntVector, SaveGame)
	int32 Z;
};


/**
 * A Color (BGRA).
 * The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Color.h
 */
USTRUCT(immutable, noexport, BlueprintType)
struct FColor
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Color, SaveGame, meta=(ClampMin="0", ClampMax="255"))
	uint8 B;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Color, SaveGame, meta=(ClampMin="0", ClampMax="255"))
	uint8 G;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Color, SaveGame, meta=(ClampMin="0", ClampMax="255"))
	uint8 R;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Color, SaveGame, meta=(ClampMin="0", ClampMax="255"))
	uint8 A;

};


/**
 * A linear color.
 * The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Color.h
 */
USTRUCT(immutable, noexport, BlueprintType)
struct FLinearColor
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LinearColor, SaveGame)
	float R;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LinearColor, SaveGame)
	float G;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LinearColor, SaveGame)
	float B;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=LinearColor, SaveGame)
	float A;

};


/**
 * A bounding box.
 * The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Box.h
 */
USTRUCT(immutable, noexport, BlueprintType, meta=(HasNativeMake="Engine.KismetMathLibrary.MakeBox"))
struct FBox
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Box, SaveGame)
	FVector Min;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Box, SaveGame)
	FVector Max;

	UPROPERTY()
	uint8 IsValid;

};

/**
 * A rectangular 2D Box.
 * The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Box2D.h
 */
USTRUCT(immutable, noexport, BlueprintType, meta=(HasNativeMake="Engine.KismetMathLibrary.MakeBox2D"))
struct FBox2D
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Box2D, SaveGame)
	FVector2D Min;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Box2D, SaveGame)
	FVector2D Max;

	UPROPERTY()
	uint8 bIsValid;

};


/**
 * A bounding box and bounding sphere with the same origin.
 * The full C++ class is located here : Engine\Source\Runtime\Core\Public\Math\BoxSphereBounds.h
 */
USTRUCT(noexport, BlueprintType)
struct FBoxSphereBounds
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=BoxSphereBounds, SaveGame)
	FVector Origin;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=BoxSphereBounds, SaveGame)
	FVector BoxExtent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=BoxSphereBounds, SaveGame)
	float SphereRadius;

};

/**
 * Structure for arbitrarily oriented boxes (i.e. not necessarily axis-aligned).
 * The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\OrientedBox.h
 */
USTRUCT(immutable, noexport)
struct FOrientedBox
{
	UPROPERTY(EditAnywhere, Category=OrientedBox, SaveGame)
	FVector Center;

	UPROPERTY(EditAnywhere, Category=OrientedBox, SaveGame)
	FVector AxisX;
	
	UPROPERTY(EditAnywhere, Category=OrientedBox, SaveGame)
	FVector AxisY;
	
	UPROPERTY(EditAnywhere, Category=OrientedBox, SaveGame)
	FVector AxisZ;

	UPROPERTY(EditAnywhere, Category=OrientedBox, SaveGame)
	float ExtentX;
	
	UPROPERTY(EditAnywhere, Category=OrientedBox, SaveGame)
	float ExtentY;

	UPROPERTY(EditAnywhere, Category=OrientedBox, SaveGame)
	float ExtentZ;
};

/*
 * A 4x4 matrix.
 * The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Matrix.h
 */
USTRUCT(immutable, noexport, BlueprintType)
struct FMatrix
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Matrix, SaveGame)
	FPlane XPlane;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Matrix, SaveGame)
	FPlane YPlane;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Matrix, SaveGame)
	FPlane ZPlane;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Matrix, SaveGame)
	FPlane WPlane;

};



USTRUCT(noexport, BlueprintType)
struct FInterpCurvePointFloat
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointFloat)
	float InVal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointFloat)
	float OutVal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointFloat)
	float ArriveTangent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointFloat)
	float LeaveTangent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointFloat)
	TEnumAsByte<enum EInterpCurveMode> InterpMode;

};



USTRUCT(noexport, BlueprintType)
struct FInterpCurveFloat
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveFloat)
	TArray<FInterpCurvePointFloat> Points;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveFloat)
	bool bIsLooped;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveFloat)
	float LoopKeyOffset;
};



USTRUCT(noexport, BlueprintType)
struct FInterpCurvePointVector2D
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointVector2D)
	float InVal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointVector2D)
	FVector2D OutVal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointVector2D)
	FVector2D ArriveTangent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointVector2D)
	FVector2D LeaveTangent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointVector2D)
	TEnumAsByte<enum EInterpCurveMode> InterpMode;

};



USTRUCT(noexport, BlueprintType)
struct FInterpCurveVector2D
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveVector2D)
	TArray<FInterpCurvePointVector2D> Points;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveVector2D)
	bool bIsLooped;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveVector2D)
	float LoopKeyOffset;
};



USTRUCT(noexport, BlueprintType)
struct FInterpCurvePointVector
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointVector)
	float InVal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointVector)
	FVector OutVal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointVector)
	FVector ArriveTangent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointVector)
	FVector LeaveTangent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointVector)
	TEnumAsByte<enum EInterpCurveMode> InterpMode;

};



USTRUCT(noexport, BlueprintType)
struct FInterpCurveVector
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveVector)
	TArray<FInterpCurvePointVector> Points;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveVector)
	bool bIsLooped;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveVector)
	float LoopKeyOffset;
};



USTRUCT(noexport, BlueprintType)
struct FInterpCurvePointQuat
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointQuat)
	float InVal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointQuat)
	FQuat OutVal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointQuat)
	FQuat ArriveTangent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointQuat)
	FQuat LeaveTangent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointQuat)
	TEnumAsByte<enum EInterpCurveMode> InterpMode;

};



USTRUCT(noexport, BlueprintType)
struct FInterpCurveQuat
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveQuat)
	TArray<FInterpCurvePointQuat> Points;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveQuat)
	bool bIsLooped;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveQuat)
	float LoopKeyOffset;
};



USTRUCT(noexport, BlueprintType)
struct FInterpCurvePointTwoVectors
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointTwoVectors)
	float InVal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointTwoVectors)
	FTwoVectors OutVal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointTwoVectors)
	FTwoVectors ArriveTangent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointTwoVectors)
	FTwoVectors LeaveTangent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointTwoVectors)
	TEnumAsByte<enum EInterpCurveMode> InterpMode;

};



USTRUCT(noexport, BlueprintType)
struct FInterpCurveTwoVectors
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveTwoVectors)
	TArray<FInterpCurvePointTwoVectors> Points;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveTwoVectors)
	bool bIsLooped;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveTwoVectors)
	float LoopKeyOffset;
};



USTRUCT(noexport, BlueprintType)
struct FInterpCurvePointLinearColor
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointLinearColor)
	float InVal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointLinearColor)
	FLinearColor OutVal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointLinearColor)
	FLinearColor ArriveTangent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointLinearColor)
	FLinearColor LeaveTangent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurvePointLinearColor)
	TEnumAsByte<enum EInterpCurveMode> InterpMode;

};



USTRUCT(noexport, BlueprintType)
struct FInterpCurveLinearColor
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveLinearColor)
	TArray<FInterpCurvePointLinearColor> Points;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveLinearColor)
	bool bIsLooped;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=InterpCurveLinearColor)
	float LoopKeyOffset;
};


/**
 * Transform composed of Quat/Translation/Scale.
 * The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\Transform.h
 */
USTRUCT(noexport, BlueprintType, meta=(HasNativeMake="Engine.KismetMathLibrary.MakeTransform", HasNativeBreak="Engine.KismetMathLibrary.BreakTransform"))
struct FTransform
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Transform, SaveGame)
	FQuat Rotation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Transform, SaveGame)
	FVector Translation;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Transform, SaveGame, meta=(MakeStructureDefaultValue = "1,1,1"))
	FVector Scale3D;

};


/**
 * Thread-safe RNG.
 * The full C++ class is located here: Engine\Source\Runtime\Core\Public\Math\RandomStream.h
 */
USTRUCT(noexport, BlueprintType, meta = (HasNativeMake = "Engine.KismetMathLibrary.MakeRandomStream", HasNativeBreak = "Engine.KismetMathLibrary.BreakRandomStream"))
struct FRandomStream
{
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=RandomStream, SaveGame)
	int32 InitialSeed;
	
	UPROPERTY()
	int32 Seed;
};


// A date/time value.

USTRUCT(immutable, noexport, BlueprintType, meta=(HasNativeMake="Engine.KismetMathLibrary.MakeDateTime", HasNativeBreak="Engine.KismetMathLibrary.BreakDateTime"))
struct FDateTime
{
	int64 Ticks;
};


// A time span value.

USTRUCT(immutable, noexport, BlueprintType, meta=(HasNativeMake="Engine.KismetMathLibrary.MakeTimespan", HasNativeBreak="Engine.KismetMathLibrary.BreakTimespan"))
struct FTimespan
{
	int64 Ticks;
};


/** An object path, this is saved as a name/string pair */
USTRUCT(noexport, BlueprintType, meta=(HasNativeMake="Engine.KismetSystemLibrary.MakeSoftObjectPath", HasNativeBreak="Engine.KismetSystemLibrary.BreakSoftObjectPath"))
struct FSoftObjectPath
{
	/** Asset path, patch to a top level object in a package */
	UPROPERTY()
	FName AssetPathName;

	/** Optional FString for subobject within an asset */
	UPROPERTY()
	FString SubPathString;
};

/** Subclass of FSoftObjectPath that is only valid to use with UClass* */
USTRUCT(noexport)
struct FSoftClassPath : public FSoftObjectPath
{
};

/** A type of primary asset, this is a wrapper around FName and can be cast back and forth */
USTRUCT(noexport, BlueprintType)
struct FPrimaryAssetType
{
	/** The Type of this object, by default it's base class's name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PrimaryAssetType)
	FName Name;
};

/** This identifies an object as a "primary" asset that can be searched for by the AssetManager and used in various tools */
USTRUCT(noexport, BlueprintType)
struct FPrimaryAssetId
{
	/** The Type of this object, by default it's base class's name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PrimaryAssetId)
	FPrimaryAssetType PrimaryAssetType;

	/** The Name of this object, by default it's short name */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = PrimaryAssetId)
	FName PrimaryAssetName;
};

// A struct used as stub for deleted ones.

USTRUCT(noexport)
struct FFallbackStruct  
{
};

UENUM(BlueprintType)
namespace ERangeBoundTypes
{
	/**
	* Enumerates the valid types of range bounds.
	*/
	enum Type
	{
		/**
		* The range excludes the bound.
		*/
		Exclusive,

		/**
		* The range includes the bound.
		*/
		Inclusive,

		/**
		* The bound is open.
		*/
		Open
	};
}

// A float range bound

USTRUCT(noexport, BlueprintType)
struct FFloatRangeBound
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Range)
	TEnumAsByte<ERangeBoundTypes::Type> Type;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Range)
	float Value;
};

// A float range

USTRUCT(noexport, BlueprintType)
struct FFloatRange
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Range)
	FFloatRangeBound LowerBound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category=Range)
	FFloatRangeBound UpperBound;
};

// An int32 range bound

USTRUCT(noexport, BlueprintType)
struct FInt32RangeBound
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Range)
	TEnumAsByte<ERangeBoundTypes::Type> Type;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Range)
	int32 Value;
};

// An int32 range

USTRUCT(noexport, BlueprintType)
struct FInt32Range
{
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Range)
	FInt32RangeBound LowerBound;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Range)
	FInt32RangeBound UpperBound;
};

// A float interval

USTRUCT(noexport)
struct FFloatInterval
{
	UPROPERTY(EditAnywhere, Category=Interval)
	float Min;

	UPROPERTY(EditAnywhere, Category=Interval)
	float Max;
};

// An int32 interval

USTRUCT(noexport)
struct FInt32Interval
{
	UPROPERTY(EditAnywhere, Category=Interval)
	int32 Min;

	UPROPERTY(EditAnywhere, Category=Interval)
	int32 Max;
};

// Automation

UENUM()
enum class EAutomationEventType : uint8
{
	Info,
	Warning,
	Error
};

USTRUCT(noexport)
struct FAutomationEvent
{
	UPROPERTY()
	EAutomationEventType Type;

	UPROPERTY()
	FString Message;

	UPROPERTY()
	FString Context;

	UPROPERTY()
	FString Filename;

	UPROPERTY()
	int32 LineNumber;

	UPROPERTY()
	FDateTime Timestamp;
};

//=============================================================================
/**
 * Object: The base class all objects.
 * This is a built-in Unreal class and it shouldn't be modified by mod authors.
 * The full C++ class is located here: Engine\Source\Runtime\CoreUObject\Public\UObject\UObject.h
 */
//=============================================================================

UCLASS(abstract, noexport)
class UObject
{
	GENERATED_BODY()
public:

	/**
	 * Default UObject constructor.
	 */
	UObject(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
	/** DO NOT USE. This constructor is for internal usage only for hot-reload purposes. */
	UObject(FVTableHelper& Helper);

	//=============================================================================
	// K2 support functions.
	
	/**
	 * Executes some portion of the ubergraph.
	 *
	 * @param	EntryPoint	The entry point to start code execution at.
	 */
	UFUNCTION(BlueprintImplementableEvent, meta=(BlueprintInternalUseOnly = "true"))
	void ExecuteUbergraph(int32 EntryPoint);
};

#endif

