// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreTypes.h"
#include "Misc/Exec.h"
#include "Containers/Array.h"
#include "Containers/UnrealString.h"
#include "Containers/Map.h"
#include "Math/Color.h"

class CORE_API FColorList
	: public FExec
{
public:
	typedef TMap< FString, const FColor* > TColorsMap;
	typedef TArray< const FColor* > TColorsLookup;

	// Common colors.	
	static const FColor White;
	static const FColor Red;
	static const FColor Green;
	static const FColor Blue;
	static const FColor Magenta;
	static const FColor Cyan;
	static const FColor Yellow;
	static const FColor Black;
	static const FColor Aquamarine;
	static const FColor BakerChocolate;
	static const FColor BlueViolet;
	static const FColor Brass;
	static const FColor BrightGold;
	static const FColor Brown;
	static const FColor Bronze;
	static const FColor BronzeII;
	static const FColor CadetBlue;
	static const FColor CoolCopper;
	static const FColor Copper;
	static const FColor Coral;
	static const FColor CornFlowerBlue;
	static const FColor DarkBrown;
	static const FColor DarkGreen;
	static const FColor DarkGreenCopper;
	static const FColor DarkOliveGreen;
	static const FColor DarkOrchid;
	static const FColor DarkPurple;
	static const FColor DarkSlateBlue;
	static const FColor DarkSlateGrey;
	static const FColor DarkTan;
	static const FColor DarkTurquoise;
	static const FColor DarkWood;
	static const FColor DimGrey;
	static const FColor DustyRose;
	static const FColor Feldspar;
	static const FColor Firebrick;
	static const FColor ForestGreen;
	static const FColor Gold;
	static const FColor Goldenrod;
	static const FColor Grey;
	static const FColor GreenCopper;
	static const FColor GreenYellow;
	static const FColor HunterGreen;
	static const FColor IndianRed;
	static const FColor Khaki;
	static const FColor LightBlue;
	static const FColor LightGrey;
	static const FColor LightSteelBlue;
	static const FColor LightWood;
	static const FColor LimeGreen;
	static const FColor MandarianOrange;
	static const FColor Maroon;
	static const FColor MediumAquamarine;
	static const FColor MediumBlue;
	static const FColor MediumForestGreen;
	static const FColor MediumGoldenrod;
	static const FColor MediumOrchid;
	static const FColor MediumSeaGreen;
	static const FColor MediumSlateBlue;
	static const FColor MediumSpringGreen;
	static const FColor MediumTurquoise;
	static const FColor MediumVioletRed;
	static const FColor MediumWood;
	static const FColor MidnightBlue;
	static const FColor NavyBlue;
	static const FColor NeonBlue;
	static const FColor NeonPink;
	static const FColor NewMidnightBlue;
	static const FColor NewTan;
	static const FColor OldGold;
	static const FColor Orange;
	static const FColor OrangeRed;
	static const FColor Orchid;
	static const FColor PaleGreen;
	static const FColor Pink;
	static const FColor Plum;
	static const FColor Quartz;
	static const FColor RichBlue;
	static const FColor Salmon;
	static const FColor Scarlet;
	static const FColor SeaGreen;
	static const FColor SemiSweetChocolate;
	static const FColor Sienna;
	static const FColor Silver;
	static const FColor SkyBlue;
	static const FColor SlateBlue;
	static const FColor SpicyPink;
	static const FColor SpringGreen;
	static const FColor SteelBlue;
	static const FColor SummerSky;
	static const FColor Tan;
	static const FColor Thistle;
	static const FColor Turquoise;
	static const FColor VeryDarkBrown;
	static const FColor VeryLightGrey;
	static const FColor Violet;
	static const FColor VioletRed;
	static const FColor Wheat;
	static const FColor YellowGreen;

	/** Initializes list of common colors. */
	void CreateColorMap();

	/** Returns a color based on ColorName if not found returs White. */
	const FColor& GetFColorByName( const TCHAR* ColorName ) const;

	/** Returns a linear color based on ColorName if not found returs White. */
	const FLinearColor GetFLinearColorByName( const TCHAR* ColorName ) const;

	/** Returns true if color is valid common colors, returns false otherwise. */
	bool IsValidColorName( const TCHAR* ColorName ) const;

	/** Returns index of color. */
	int32 GetColorIndex( const TCHAR* ColorName ) const;

	/** Returns a color based on index. If index is invalid, returns White. */
	const FColor& GetFColorByIndex( int32 ColorIndex ) const;

	/** Resturn color's name based on index. If index is invalid, returns BadIndex. */
	const FString& GetColorNameByIndex( int32 ColorIndex ) const;

	/** Returns the number of colors. */
	int32 GetColorsNum() const
	{
		return ColorsMap.Num();
	}

	/** Prints to log all colors information. */
	void LogColors();

protected:
	void InitializeColor( const TCHAR* ColorName, const FColor* ColorPtr, int32& CurrentIndex );

	/** List of common colors. */
	TColorsMap ColorsMap;

	/** Array of colors for fast lookup when using index. */
	TColorsLookup ColorsLookup;
};


extern CORE_API FColorList GColorList;
