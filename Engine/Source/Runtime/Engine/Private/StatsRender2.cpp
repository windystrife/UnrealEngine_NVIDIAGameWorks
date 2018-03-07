// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

/**
 *
 * This file contains the rendering functions used in the stats code
 */

#include "CoreMinimal.h"
#include "Misc/CoreMisc.h"
#include "Stats/Stats.h"
#include "EngineGlobals.h"
#include "Engine/Engine.h"
#include "CanvasItem.h"
#include "Engine/Canvas.h"
#include "Engine/Texture2D.h"

#if STATS

#include "Stats/StatsData.h"
#include "Performance/EnginePerformanceTargets.h"

TAutoConsoleVariable<int32> CVarNumStatsPerGroup(
	TEXT("stats.MaxPerGroup"),
	25,
	TEXT("The max number of lines of stats to show in a group")
);

/** Stats rendering constants. */
enum class EStatRenderConsts
{
	NUM_COLUMNS = 5,
};

/** Should we use a solid fill or a gradient? */
const bool bUseFlatBackgroundForStats = true;

/** Enumerates stat font types and maximum length of the stat names. */
enum class EStatFontTypes : int32
{
	/** Tiny font, used when ViewRectX < 1280. */
	Tiny = 0,
	/** Small font. */
	Small = 1,
	/** Number of fonts. */
	NumFonts,
};

/** Contains misc stat font properties. */
struct FStatFont
{
	/** Default constructor. */
	FStatFont( const int32 InMaxDisplayedChars, const int32 InFontHeight, const int32 InFontHeightOffsets ) :
		MaxDisplayedChars( InMaxDisplayedChars ),
		FontHeight( InFontHeight ),
		FontHeightOffset( InFontHeightOffsets )
	{}

	/** Maximum length of the displayed stat names. */
	const int32 MaxDisplayedChars;

	/** Font height, manually selected to fix more stats on screen. */
	const int32 FontHeight;

	/** Y offset of the background tile, manually selected to fix more stats on screen. */
	const int32 FontHeightOffset;
};

static FStatFont GStatFonts[(int32)EStatFontTypes::NumFonts] =
{
	/** Tiny. */
	FStatFont( 40, 10, 1 ),
	/** Small. */
	FStatFont( 72, 12, 2 ),
};

void FromString( EStatFontTypes& OutValue, const TCHAR* Buffer )
{
	OutValue = EStatFontTypes::Small;

	if( FCString::Stricmp( Buffer, TEXT( "Tiny" ) ) == 0 )
	{
		OutValue = EStatFontTypes::Tiny;
	}
}

/** Holds various parameters used for rendering stats. */
struct FStatRenderGlobals
{
	/** Rendering offset for first column from stat label. */
	int32 AfterNameColumnOffset;

	/** Rendering offsets for additional columns. */
	int32 InterColumnOffset;

	/** Color for rendering stats. */
	FLinearColor StatColor;

	/** Color to use when rendering headings. */
	FLinearColor HeadingColor;

	/** Color to use when rendering group names. */
	FLinearColor GroupColor;

	/** Color used as the background for every other stat item to make it easier to read across lines. */
	FLinearColor BackgroundColors[2];

	/** The font used for rendering stats. */
	UFont* StatFont;

	/** Current size of the viewport, used to detect resolution changes. */
	FIntPoint SizeXY;

	/** The X size that can be used to render the stats. */
	int32 SafeSizeX;

	/** Current stat font type. */
	EStatFontTypes StatFontType;

	/** Whether we are in the stereo mode. */
	bool bIsStereo;

	/** Whether we need update internals. */
	bool bNeedRefresh;

	/** Scale of the stat rendering */
	float StatScale;

	FStatRenderGlobals() :
		AfterNameColumnOffset(0),
		InterColumnOffset(0),
		StatColor(0.f,1.f,0.f),
		HeadingColor(1.f,0.2f,0.f),
		GroupColor(FLinearColor::White),
		StatFont(nullptr),
		StatFontType(EStatFontTypes::NumFonts),
		bNeedRefresh(true),
		StatScale(1.0f)
	{
		BackgroundColors[0] = FLinearColor(0.05f, 0.05f, 0.05f, 0.92f); // dark gray mostly occluding the background
		BackgroundColors[1] = FLinearColor(0.02f, 0.02f, 0.02f, 0.88f); // slightly different to help make long lines more readable
		SetNewFont(EStatFontTypes::Small);
	}

	/**
	 * Initializes stat render globals.
	 */
	void Initialize( const int32 InSizeX, const int32 InSizeY, const int32 InSafeSizeX, const bool bInIsStereo, const float InStatScale )
	{
		StatScale = InStatScale;
		FIntPoint NewSizeXY( InSizeX, InSizeY );
		if (NewSizeXY != SizeXY)
		{
			SizeXY = NewSizeXY;
			bNeedRefresh = true;
		}

		if( SizeXY.X < 1280 )
		{
			SetNewFont( EStatFontTypes::Tiny );
		}

		SafeSizeX = InSafeSizeX;
		bIsStereo = bInIsStereo;

		if (bIsStereo)
		{
			SetNewFont( EStatFontTypes::Tiny );
		}

		if( bNeedRefresh )
		{
			// This is the number of W characters to leave spacing for in the stat name column.
			const FString STATNAME_COLUMN_WIDTH = FString::ChrN( GetNumCharsForStatName(), 'S' );

			// This is the number of W characters to leave spacing for in the other stat columns.
			const FString STATDATA_COLUMN_WIDTH = FString::ChrN( StatFontType == EStatFontTypes::Small ? 8 : 7, 'W' );

			// Get the size of the spaces, since we can't render the width calculation strings.
			int32 StatColumnSpaceSizeY = 0;
			int32 TimeColumnSpaceSizeY = 0;

			// #TODO: Compute on the stats thread to get the exact measurement
			// Determine where the first column goes.
			StringSize(StatFont, AfterNameColumnOffset, StatColumnSpaceSizeY, *STATNAME_COLUMN_WIDTH);

			// Determine the width of subsequent columns.
			StringSize(StatFont, InterColumnOffset, TimeColumnSpaceSizeY, *STATDATA_COLUMN_WIDTH);

			bNeedRefresh = false;
		}
	}
	
	/**
	 * @return number of characters use to render the stat name
	 */
	int32 GetNumCharsForStatName() const
	{
		const int32 MaxDisplayedChars = GStatFonts[(int32)StatFontType].MaxDisplayedChars;
		return !bIsStereo ? MaxDisplayedChars : MaxDisplayedChars / 2;
	}
	
	/**
	 * @return number of characters use to render the stat name
	 */
	int32 GetFontHeight() const
	{
		return GStatFonts[( int32 )StatFontType].FontHeight * StatScale;
	}

	/**
	 * @return Y offset of the background tile, so this will align with the text
	 */
	int32 GetYOffset() const
	{
		return GStatFonts[(int32)StatFontType].FontHeightOffset;
	}

	/** Sets a new font. */
	void SetNewFont( EStatFontTypes NewFontType )
	{
		if( StatFontType != NewFontType )
		{
			StatFontType = NewFontType;
			if( StatFontType == EStatFontTypes::Tiny )
			{
				StatFont = GEngine->GetTinyFont();
			}
			else if( StatFontType == EStatFontTypes::Small )
			{
				StatFont = GEngine->GetSmallFont();
			}
			bNeedRefresh = true;
		}
	}

	/** Returns the background texture for stat rows */
	FTexture* GetBackgroundTexture() const
	{
		if (bUseFlatBackgroundForStats)
		{
			return GWhiteTexture;
		}
		else
		{
			UTexture2D* BackgroundTexture = UCanvas::StaticClass()->GetDefaultObject<UCanvas>()->GradientTexture0;
			return (BackgroundTexture != nullptr) ? BackgroundTexture->Resource : nullptr;
		}
	}
};

FStatRenderGlobals& GetStatRenderGlobals()
{
	// Sanity checks.
	check( IsInGameThread() );
	check( GEngine );
	static FStatRenderGlobals Singleton;
	return Singleton;
}

/** Shorten a name for stats display. */
static FString ShortenName( TCHAR const* LongName )
{
	FString Result( LongName );
	const int32 Limit = GetStatRenderGlobals().GetNumCharsForStatName();
	if( Result.Len() > Limit )
	{
		Result = FString( TEXT( "..." ) ) + Result.Right( Limit );
	}
	return Result;
}

/** Exec used to execute engine stats command on the game thread. */
static class FStatCmdEngine : private FSelfRegisteringExec
{
public:
	/** Console commands. */
	virtual bool Exec( UWorld*, const TCHAR* Cmd, FOutputDevice& Ar ) override
	{
		if( FParse::Command( &Cmd, TEXT( "stat" ) ) )
		{
			if( FParse::Command( &Cmd, TEXT( "display" ) ) )
			{
				TParsedValueWithDefault<EStatFontTypes> Font( Cmd, TEXT( "font=" ), EStatFontTypes::Small );
				GetStatRenderGlobals().SetNewFont( Font.Get() );
				return true;
			}
		}
		return false;
	}
}
StatCmdEngineExec;

static void RightJustify( FCanvas* Canvas, const int32 X, const int32 Y, TCHAR const* Text, FLinearColor const& Color )
{
	const FStatRenderGlobals& Globals = GetStatRenderGlobals();

	int32 StatColumnSpaceSizeX, StatColumnSpaceSizeY;
	StringSize(Globals.StatFont, StatColumnSpaceSizeX, StatColumnSpaceSizeY, Text);
	StatColumnSpaceSizeX *= Globals.StatScale;
	StatColumnSpaceSizeY *= Globals.StatScale;
	Canvas->DrawShadowedString(X + Globals.InterColumnOffset - StatColumnSpaceSizeX, Y, Text, Globals.StatFont, Color, Globals.StatScale);
}


/**
 *
 * @param Item the stat to render
 * @param Canvas the render interface to draw with
 * @param X the X location to start drawing at
 * @param Y the Y location to start drawing at
 * @param Indent Indentation of this cycles, used when rendering hierarchy
 * @param bStackStat If false, this is a non-stack cycle counter, don't render the call count column
 */
static int32 RenderCycle( const FComplexStatMessage& Item, class FCanvas* Canvas, const int32 X, const int32 Y, const int32 Indent, const bool bStackStat, const float Budget, const bool bIsBudgetIgnored )
{
	const bool bBudget = Budget >= 0.f;
	const FStatRenderGlobals& Globals = GetStatRenderGlobals();
	FColor Color = Globals.StatColor.ToFColor(true);

	check(Item.NameAndInfo.GetFlag(EStatMetaFlags::IsCycle));

	const bool bIsInitialized = Item.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64;

	const int32 IndentWidth = (Indent + (bIsBudgetIgnored ? 1 : 0))*8;

	if( bIsInitialized )
	{
		const float InMs = FPlatformTime::ToMilliseconds(Item.GetValue_Duration(EComplexStatField::IncAve));
		// Color will be determined by the average value of history
		// If show inclusive and and show exclusive is on, then it will choose color based on inclusive average
		// #Stats: 2015-06-09 This is slow, fix this
		FString CounterName = Item.GetShortName().ToString();
		CounterName.RemoveFromStart(TEXT("STAT_"), ESearchCase::CaseSensitive);
		GEngine->GetStatValueColoration(CounterName, InMs, Color);

		// the time of a "full bar" in ms
		const float MaxMeter = bBudget ? Budget : FEnginePerformanceTargets::GetTargetFrameTimeThresholdMS();

		const int32 MeterWidth = Globals.AfterNameColumnOffset;

		int32 BarWidth = int32((InMs / MaxMeter) * MeterWidth);
		if (BarWidth > 2)
		{
			if (BarWidth > MeterWidth ) 
			{
				BarWidth = MeterWidth;
			}

			FCanvasBoxItem BoxItem(FVector2D(X + MeterWidth - BarWidth, Y + .4f * Globals.GetFontHeight()), FVector2D(BarWidth, 0.2f * Globals.GetFontHeight()));
			BoxItem.SetColor( FLinearColor::Red );
			BoxItem.Draw( Canvas );		
		}
	}

	// #Stats: Move to the stats thread to avoid expensive computation on the game thread.
	const FString StatDesc = Item.GetDescription();
	const FString StatDisplay = StatDesc.Len() == 0 ? Item.GetShortName().GetPlainNameString() : StatDesc;

	Canvas->DrawShadowedString(X + IndentWidth, Y, *ShortenName(*StatDisplay), Globals.StatFont, Color, Globals.StatScale);

	int32 CurrX = X + Globals.AfterNameColumnOffset;
	// Now append the call count
	if( bStackStat )
	{
		if (Item.NameAndInfo.GetFlag(EStatMetaFlags::IsPackedCCAndDuration) && bIsInitialized)
		{
			RightJustify(Canvas,CurrX,Y,*FString::Printf(TEXT("%u"), Item.GetValue_CallCount(EComplexStatField::IncAve)),Color);
		}
		CurrX += Globals.InterColumnOffset;
	}

	// Add the two inclusive columns if asked
	if( bIsInitialized )
	{
		RightJustify(Canvas,CurrX,Y,*FString::Printf(TEXT("%1.2f ms"),FPlatformTime::ToMilliseconds(Item.GetValue_Duration(EComplexStatField::IncAve))),Color); 
	}
	CurrX += Globals.InterColumnOffset;

	if( bIsInitialized )
	{
		RightJustify(Canvas,CurrX,Y,*FString::Printf(TEXT("%1.2f ms"),FPlatformTime::ToMilliseconds(Item.GetValue_Duration(EComplexStatField::IncMax))),Color);
		
	}
	CurrX += Globals.InterColumnOffset;

	if( bStackStat && !bBudget)
	{
		// And the exclusive if asked
		if( bIsInitialized )
		{
			RightJustify(Canvas,CurrX,Y,*FString::Printf(TEXT("%1.2f ms"),FPlatformTime::ToMilliseconds(Item.GetValue_Duration(EComplexStatField::ExcAve))),Color);
		}
		CurrX += Globals.InterColumnOffset;

		if( bIsInitialized )
		{
			RightJustify(Canvas,CurrX,Y,*FString::Printf(TEXT("%1.2f ms"),FPlatformTime::ToMilliseconds(Item.GetValue_Duration(EComplexStatField::ExcMax))),Color);
		}
		CurrX += Globals.InterColumnOffset;
	}
	return Globals.GetFontHeight();
}


static FString FormatStatValueFloat(const float Value)
{
	const float Frac = FMath::Frac(Value);
	// #TODO: Move to stats thread, add support for int64 type, int32 may not be sufficient all the time.
	const int32 Integer = FMath::FloorToInt(Value);
	const FString IntString = FString::FormatAsNumber(Integer);
	const FString FracString = FString::Printf(TEXT("%0.2f"), Frac);
	const FString Result = FString::Printf(TEXT("%s.%s"), *IntString, *FracString.Mid(2));
	return Result;
}

static FString FormatStatValueInt64(const int64 Value)
{
	const FString IntString = FString::FormatAsNumber((int32)Value);
	return IntString;
}


/**
 * Renders the headings for grouped rendering
 *
 * @param RI the render interface to draw with
 * @param X the X location to start rendering at
 * @param Y the Y location to start rendering at
 *
 * @return the height of headings rendered
 */
static int32 RenderGroupedHeadings( class FCanvas* Canvas, const int X, const int32 Y, const bool bIsHierarchy, const bool bBudget )
{
	// The heading looks like:
	// Stat [32chars]	CallCount [8chars]	IncAvg [8chars]	IncMax [8chars]	ExcAvg [8chars]	ExcMax [8chars]
	// If we are in budget mode ignore ExcAvg and ExcMax

	static const TCHAR* CaptionFlat = TEXT("Cycle counters (flat)");
	static const TCHAR* CaptionHier = TEXT("Cycle counters (hierarchy)");
	const FStatRenderGlobals& Globals = GetStatRenderGlobals();

	if(!bBudget)
	{
		Canvas->DrawShadowedString(X, Y, bIsHierarchy ? CaptionHier : CaptionFlat, Globals.StatFont, Globals.HeadingColor, Globals.StatScale);
	}

	int32 CurrX = X + Globals.AfterNameColumnOffset;
	RightJustify(Canvas, CurrX, Y, TEXT("CallCount"), Globals.HeadingColor);
	CurrX += Globals.InterColumnOffset;

	RightJustify(Canvas, CurrX, Y, TEXT("InclusiveAvg"), Globals.HeadingColor);
	CurrX += Globals.InterColumnOffset;
	RightJustify(Canvas, CurrX, Y, TEXT("InclusiveMax"), Globals.HeadingColor);
	CurrX += Globals.InterColumnOffset;

	if(!bBudget)
	{
		RightJustify(Canvas, CurrX, Y, TEXT("ExclusiveAvg"), Globals.HeadingColor);
		CurrX += Globals.InterColumnOffset;
		RightJustify(Canvas, CurrX, Y, TEXT("ExclusiveMax"), Globals.HeadingColor);
		CurrX += Globals.InterColumnOffset;
	}
	

	return Globals.GetFontHeight() + (Globals.GetFontHeight() / 3);
}

/**
 * Renders the counter headings for grouped rendering
 *
 * @param RI the render interface to draw with
 * @param X the X location to start rendering at
 * @param Y the Y location to start rendering at
 *
 * @return the height of headings rendered
 */
static int32 RenderCounterHeadings( class FCanvas* Canvas, const int32 X, const int32 Y )
{
	// The heading looks like:
	// Stat [32chars]	Value [8chars]	Average [8chars]
	const FStatRenderGlobals& Globals = GetStatRenderGlobals();

	Canvas->DrawShadowedString(X, Y, TEXT("Counters"), Globals.StatFont, Globals.HeadingColor, Globals.StatScale);

	// Determine where the first column goes
	int32 CurrX = X + Globals.AfterNameColumnOffset;

	// Draw the average column label.
	RightJustify(Canvas, CurrX, Y, TEXT("Average"), Globals.HeadingColor);
	CurrX += Globals.InterColumnOffset * Globals.StatScale;

	// Draw the max column label.
	RightJustify(Canvas, CurrX, Y, TEXT("Max"), Globals.HeadingColor);
	return Globals.GetFontHeight() + (Globals.GetFontHeight() / 3);
}

/**
 * Renders the memory headings for grouped rendering
 *
 * @param RI the render interface to draw with
 * @param X the X location to start rendering at
 * @param Y the Y location to start rendering at
 *
 * @return the height of headings rendered
 */
static int32 RenderMemoryHeadings( class FCanvas* Canvas, const int32 X, const  int32 Y )
{
	// The heading looks like:
	// Stat [32chars]	MemUsed [8chars]	PhysMem [8chars]
	const FStatRenderGlobals& Globals = GetStatRenderGlobals();

	Canvas->DrawShadowedString(X, Y, TEXT("Memory Counters"), Globals.StatFont, Globals.HeadingColor, Globals.StatScale);

	// Determine where the first column goes
	int32 CurrX = X + Globals.AfterNameColumnOffset;
	RightJustify(Canvas, CurrX, Y, TEXT("UsedMax"), Globals.HeadingColor);

	CurrX += Globals.InterColumnOffset * Globals.StatScale;
	RightJustify(Canvas, CurrX, Y, TEXT("Mem%"), Globals.HeadingColor);

	CurrX += Globals.InterColumnOffset * Globals.StatScale;
	RightJustify(Canvas, CurrX, Y, TEXT("MemPool"), Globals.HeadingColor);

	CurrX += Globals.InterColumnOffset * Globals.StatScale;
	RightJustify(Canvas, CurrX, Y, TEXT("Pool Capacity"), Globals.HeadingColor);

	return Globals.GetFontHeight() + (Globals.GetFontHeight() / 3);
}

// @param bAutoType true: automatically choose GB/MB/KB/... false: always use MB for easier comparisons
static FString GetMemoryString( const double Value, const bool bAutoType = true )
{
	if (bAutoType)
	{
		if (Value > 1024.0 * 1024.0 * 1024.0)
		{
			return FString::Printf( TEXT( "%.2f GB" ), float( Value / (1024.0 * 1024.0 * 1024.0) ) );
		}
		if (Value > 1024.0 * 1024.0)
		{
			return FString::Printf( TEXT( "%.2f MB" ), float( Value / (1024.0 * 1024.0) ) );
		}
		if (Value > 1024.0)
		{
			return FString::Printf( TEXT( "%.2f KB" ), float( Value / (1024.0) ) );
		}
		return FString::Printf( TEXT( "%.2f B" ), float( Value ) );
	}

	return FString::Printf( TEXT( "%.2f MB" ), float( Value / (1024.0 * 1024.0) ) );
}

static int32 RenderMemoryCounter( const FGameThreadStatsData& ViewData, const FComplexStatMessage& All, class FCanvas* Canvas, const int32 X, const int32 Y, const float Budget, const bool bIsBudgetIgnored )
{
	FPlatformMemory::EMemoryCounterRegion Region = FPlatformMemory::EMemoryCounterRegion(All.NameAndInfo.GetField<EMemoryRegion>());
	// At this moment we only have memory stats that are marked as non frame stats, so can't be cleared every frame.
	//const bool bDisplayAll = All.NameAndInfo.GetFlag(EStatMetaFlags::ShouldClearEveryFrame);
	const FStatRenderGlobals& Globals = GetStatRenderGlobals();
	const float MaxMemUsed = All.GetValue_double(EComplexStatField::IncMax);

	// Draw the label
	Canvas->DrawShadowedString(X, Y, *ShortenName(*All.GetDescription()), Globals.StatFont, Globals.StatColor, Globals.StatScale);
	int32 CurrX = X + Globals.AfterNameColumnOffset;

	// always use MB for easier comparisons
	const bool bAutoType = false;

	// Now append the max value of the stat
	RightJustify(Canvas, CurrX, Y, *GetMemoryString(MaxMemUsed, bAutoType), Globals.StatColor);
	CurrX += Globals.InterColumnOffset * Globals.StatScale;
	if (ViewData.PoolCapacity.Contains(Region))
	{
		RightJustify(Canvas, CurrX, Y, *FString::Printf(TEXT("%.0f%%"), float(100.0 * MaxMemUsed / double(ViewData.PoolCapacity[Region]))), Globals.StatColor);
	}
	CurrX += Globals.InterColumnOffset * Globals.StatScale;
	if (ViewData.PoolAbbreviation.Contains(Region))
	{
		RightJustify(Canvas, CurrX, Y, *ViewData.PoolAbbreviation[Region], Globals.StatColor);
	}
	CurrX += Globals.InterColumnOffset * Globals.StatScale;
	if (ViewData.PoolCapacity.Contains(Region))
	{
		RightJustify(Canvas, CurrX, Y, *GetMemoryString(double(ViewData.PoolCapacity[Region]), bAutoType), Globals.StatColor);
	}
	CurrX += Globals.InterColumnOffset * Globals.StatScale;

	return Globals.GetFontHeight();
}

static int32 RenderCounter( const FGameThreadStatsData& ViewData, const FComplexStatMessage& All, class FCanvas* Canvas, const int32 X, const int32 Y, const float Budget, const bool bIsBudgetIgnored )
{
	const FStatRenderGlobals& Globals = GetStatRenderGlobals();

	// If this is a cycle, render it as a cycle. This is a special case for manually set cycle counters.
	const bool bIsCycle = All.NameAndInfo.GetFlag(EStatMetaFlags::IsCycle);
	if( bIsCycle )
	{
		return RenderCycle( All, Canvas, X, Y, 0, false, Budget, bIsBudgetIgnored );
	}

	const bool bDisplayAll = All.NameAndInfo.GetFlag(EStatMetaFlags::ShouldClearEveryFrame);

	// Draw the label
	Canvas->DrawShadowedString(X, Y, *ShortenName(*All.GetDescription()), Globals.StatFont, Globals.StatColor, Globals.StatScale);
	int32 CurrX = X + Globals.AfterNameColumnOffset;

	if( bDisplayAll )
	{
		// Append the average.
		if (All.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_double)
		{
			const FString ValueFormatted = FormatStatValueFloat( All.GetValue_double( EComplexStatField::IncAve ) );
			RightJustify(Canvas, CurrX, Y, *ValueFormatted, Globals.StatColor);
		}
		else if (All.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64)
		{
			const FString ValueFormatted = FormatStatValueInt64( All.GetValue_int64( EComplexStatField::IncAve ) );
			RightJustify(Canvas, CurrX, Y, *ValueFormatted, Globals.StatColor);
		}
	}
	CurrX += Globals.InterColumnOffset * Globals.StatScale;

	// Append the maximum.
	if (All.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_double)
	{
		const FString ValueFormatted = FormatStatValueFloat( All.GetValue_double( EComplexStatField::IncMax ) );
		RightJustify(Canvas, CurrX, Y, *ValueFormatted, Globals.StatColor);
	}
	else if (All.NameAndInfo.GetField<EStatDataType>() == EStatDataType::ST_int64)
	{
		const FString ValueFormatted = FormatStatValueInt64( All.GetValue_int64( EComplexStatField::IncMax ) );
		RightJustify(Canvas, CurrX, Y, *ValueFormatted, Globals.StatColor);
	}
	return Globals.GetFontHeight();
}

void RenderHierCycles( FCanvas* Canvas, const int32 X, int32& Y, const FActiveStatGroupInfo& HudGroup )
{
	const FStatRenderGlobals& Globals = GetStatRenderGlobals();
	const FTexture* BackgroundTexture = Globals.GetBackgroundTexture();

	// Render all cycle counters.
	for( int32 RowIndex = 0; RowIndex < HudGroup.HierAggregate.Num(); ++RowIndex )
	{
		const FComplexStatMessage& ComplexStat = HudGroup.HierAggregate[RowIndex];
		const int32 Indent = HudGroup.Indentation[RowIndex];

		if (BackgroundTexture != nullptr)
		{
			Canvas->DrawTile(X, Y + Globals.GetYOffset(), Globals.AfterNameColumnOffset + Globals.InterColumnOffset * (int32)EStatRenderConsts::NUM_COLUMNS, Globals.GetFontHeight(),
				0, 0, 1, 1,
				Globals.BackgroundColors[RowIndex & 1], BackgroundTexture, true);
		}

		Y += RenderCycle( ComplexStat, Canvas, X, Y, Indent, true, -1.f, false );
	}
}


int32 RenderGroupBudget( FCanvas* Canvas, const  int32 X, const int32 Y, const uint64 AvgTotalTime, const  uint64 MaxTotalTime, const float GroupBudget )
{
	// The budget looks like:
	// Stat [32chars]	Value [8chars]	Average [8chars]
	const FStatRenderGlobals& Globals = GetStatRenderGlobals();

	const float AvgTotalMs = FPlatformTime::ToMilliseconds(AvgTotalTime);
	const float MaxTotalMs = FPlatformTime::ToMilliseconds(MaxTotalTime);

	FString BudgetString = FString::Printf(TEXT("Total (of %1.2f ms)"), GroupBudget);

	Canvas->DrawShadowedString(X, Y, *BudgetString, Globals.StatFont, FLinearColor::Green, Globals.StatScale);

	int32 CurrX = X + Globals.AfterNameColumnOffset;
	CurrX += Globals.InterColumnOffset;
   
	RightJustify(Canvas, CurrX, Y, *FString::Printf(TEXT("%1.2f ms"), AvgTotalMs) , AvgTotalMs > GroupBudget ? FLinearColor::Red : FLinearColor::Green);
	
	CurrX += Globals.InterColumnOffset;
	RightJustify(Canvas, CurrX, Y, *FString::Printf(TEXT("%1.2f ms"), MaxTotalMs) , MaxTotalMs > GroupBudget ? FLinearColor::Red : FLinearColor::Green);
	
	return Globals.GetFontHeight();
}

int32 RenderMoreStatsLine(FCanvas* Canvas, const  int32 X, const int32 Y, const int32 NumMoreStats)
{
	const FStatRenderGlobals& Globals = GetStatRenderGlobals();

	FString MoreString = FString::Printf(TEXT("[%d more stats. Use the stats.MaxPerGroup CVar to increase the limit]"), NumMoreStats);
	Canvas->DrawShadowedString(X, Y, *MoreString, Globals.StatFont, FLinearColor::Yellow, Globals.StatScale);

	return Globals.GetFontHeight();
}

template< typename T >
void RenderArrayOfStats( FCanvas* Canvas, const int32 X, int32& Y, const TArray<FComplexStatMessage>& Aggregates, const FGameThreadStatsData& ViewData, const TSet<FName>& IgnoreBudgetStats, const float TotalGroupBudget, const T& FunctionToCall )
{
	const FStatRenderGlobals& Globals = GetStatRenderGlobals();
	const FTexture* BackgroundTexture = Globals.GetBackgroundTexture();

	const bool bBudget = TotalGroupBudget >= 0.f;
	const int32 NumColumns = (int32)EStatRenderConsts::NUM_COLUMNS - (bBudget ? 2 : 0);
	uint64 AvgTotalTime = 0;
	uint64 MaxTotalTime = 0;

	// Render all counters.
	int32 MaxStatsPerGroup = CVarNumStatsPerGroup.GetValueOnGameThread();
	int32 RowIndex;
	for( RowIndex = 0; RowIndex < Aggregates.Num() && RowIndex < MaxStatsPerGroup; ++RowIndex )
	{
		const FComplexStatMessage& ComplexStat = Aggregates[RowIndex];
		const bool bIsBudgetIgnored = IgnoreBudgetStats.Contains(ComplexStat.NameAndInfo.GetShortName());
		if(bBudget && !bIsBudgetIgnored && ComplexStat.NameAndInfo.GetFlag(EStatMetaFlags::IsPackedCCAndDuration))
		{
			AvgTotalTime += ComplexStat.GetValue_Duration(EComplexStatField::IncAve);
			MaxTotalTime += ComplexStat.GetValue_Duration(EComplexStatField::IncMax);
		}

		if (BackgroundTexture != nullptr)
		{
			Canvas->DrawTile(X, Y + Globals.GetYOffset(), Globals.AfterNameColumnOffset + (Globals.InterColumnOffset * Globals.StatScale) * NumColumns, Globals.GetFontHeight(),
				0, 0, 1, 1,
				Globals.BackgroundColors[RowIndex & 1], BackgroundTexture, true);
		}

		Y += FunctionToCall( ViewData, ComplexStat, Canvas, X, Y, TotalGroupBudget, bIsBudgetIgnored );
	}

	if (MaxStatsPerGroup < Aggregates.Num())
	{
		Canvas->DrawTile(X, Y + Globals.GetYOffset(), Globals.AfterNameColumnOffset + Globals.InterColumnOffset * NumColumns, Globals.GetFontHeight(),
			0, 0, 1, 1,
			Globals.BackgroundColors[(RowIndex++) & 1], BackgroundTexture, true);
		Y += RenderMoreStatsLine(Canvas, X, Y, Aggregates.Num() - MaxStatsPerGroup);
	}

	if(bBudget)
	{
		Canvas->DrawTile(X, Y + Globals.GetYOffset(), Globals.AfterNameColumnOffset + Globals.InterColumnOffset * NumColumns, Globals.GetFontHeight(),
			0, 0, 1, 1,
			Globals.BackgroundColors[RowIndex & 1], BackgroundTexture, true);
		Y += RenderGroupBudget(Canvas, X, Y, AvgTotalTime, MaxTotalTime, TotalGroupBudget);
	}
}

static int32 RenderFlatCycle( const FGameThreadStatsData& ViewData, const FComplexStatMessage& Item, class FCanvas* Canvas, const int32 X, const int32 Y, const  float Budget, const bool bIsBudgetIgnored )
{
	return RenderCycle( Item, Canvas, X, Y, 0, true, Budget, bIsBudgetIgnored);
}

/**
 * Renders stats using groups
 *
 * @param ViewData data from the stats thread
 * @param RI the render interface to draw with
 * @param X the X location to start rendering at
 * @param Y the Y location to start rendering at
 */
static void RenderGroupedWithHierarchy(const FGameThreadStatsData& ViewData, FViewport* Viewport, class FCanvas* Canvas, const int32 X, int32& Y)
{
	// Grab texture for rendering text background.
	UTexture2D* BackgroundTexture = UCanvas::StaticClass()->GetDefaultObject<UCanvas>()->DefaultTexture;

	const FStatRenderGlobals& Globals = GetStatRenderGlobals();

	// Render all groups.
	for( int32 GroupIndex = 0; GroupIndex < ViewData.ActiveStatGroups.Num(); ++GroupIndex )
	{
		const FActiveStatGroupInfo& StatGroup = ViewData.ActiveStatGroups[GroupIndex];
		const bool bBudget = StatGroup.ThreadBudgetMap.Num() > 0;
		const int32 NumThreadsBreakdown = bBudget ? StatGroup.FlatAggregateThreadBreakdown.Num() : 1;
		TArray<FName> ThreadNames;
		StatGroup.FlatAggregateThreadBreakdown.GetKeys(ThreadNames);

		for(int32 ThreadBreakdownIdx = 0; ThreadBreakdownIdx < NumThreadsBreakdown; ++ThreadBreakdownIdx)
		{
			// If the stat isn't enabled for this particular viewport, skip
			FString StatGroupName = ViewData.GroupNames[GroupIndex].ToString();
			StatGroupName.RemoveFromStart(TEXT("STATGROUP_"), ESearchCase::CaseSensitive);
			if (!Viewport->GetClient() || !Viewport->GetClient()->IsStatEnabled(StatGroupName))
			{
				continue;
			}

			// Render header.
			const FName& GroupName = ViewData.GroupNames[GroupIndex];
			const FString& GroupDesc = ViewData.GroupDescriptions[GroupIndex];
			FString GroupLongName = FString::Printf(TEXT("%s [%s]"), *GroupDesc, *GroupName.GetPlainNameString());
			
			FName ThreadName;
			FName ShortThreadName;
			if(bBudget)
			{
				ThreadName = ThreadNames[ThreadBreakdownIdx];
				ShortThreadName = FStatNameAndInfo::GetShortNameFrom(ThreadName);
				GroupLongName += FString::Printf(TEXT(" - %s"), *ShortThreadName.ToString());
			}

			if(!ViewData.RootFilter.IsEmpty())
			{
				GroupLongName += FString::Printf(TEXT(" ROOT=%s"), *ViewData.RootFilter);
			}
			Canvas->DrawShadowedString(X, Y, *GroupLongName, Globals.StatFont, Globals.GroupColor, Globals.StatScale);
			Y += Globals.GetFontHeight();

			
			const bool bHasHierarchy = !!StatGroup.HierAggregate.Num();
			const bool bHasFlat = !!StatGroup.FlatAggregate.Num();

			if (bHasHierarchy || bHasFlat)
			{
				// Render grouped headings.
				Y += RenderGroupedHeadings(Canvas, X, Y, bHasHierarchy, bBudget);
			}

			// Render hierarchy.
			if (bHasHierarchy)
			{
				RenderHierCycles(Canvas, X, Y, StatGroup);
				Y += Globals.GetFontHeight();
			}

			const float* BudgetPtr = ShortThreadName != NAME_None ? StatGroup.ThreadBudgetMap.Find(ShortThreadName) : nullptr;
			const float Budget = BudgetPtr ? *BudgetPtr : -1.f;
			// Render flat.
			if (bHasFlat)
			{
				RenderArrayOfStats(Canvas, X, Y, bBudget ? StatGroup.FlatAggregateThreadBreakdown[ThreadName] : StatGroup.FlatAggregate, ViewData, StatGroup.BudgetIgnoreStats, Budget, RenderFlatCycle);
				Y += Globals.GetFontHeight();
			}
		}
		

			// Render memory counters.
			if (StatGroup.MemoryAggregate.Num())
			{
				Y += RenderMemoryHeadings(Canvas, X, Y);
			RenderArrayOfStats(Canvas, X, Y, StatGroup.MemoryAggregate, ViewData, StatGroup.BudgetIgnoreStats, -1.f, RenderMemoryCounter);
				Y += Globals.GetFontHeight();
			}

			// Render remaining counters.
			if (StatGroup.CountersAggregate.Num())
			{
				Y += RenderCounterHeadings(Canvas, X, Y);
			RenderArrayOfStats(Canvas, X, Y, StatGroup.CountersAggregate, ViewData, StatGroup.BudgetIgnoreStats, -1.f, RenderCounter);
				Y += Globals.GetFontHeight();
			}
		}
}

/**
 * Renders the stats data
 *
 * @param Viewport	The viewport to render to
 * @param Canvas	Canvas object to use for rendering
 * @param X			the X location to start rendering at
 * @param Y			the Y location to start rendering at
 * @param SafeSizeX	the X size that can be used to render the stats
 */
void RenderStats(FViewport* Viewport, class FCanvas* Canvas, int32 X, int32 Y, int32 SafeSizeX, const float TextScale)
{
	DECLARE_SCOPE_CYCLE_COUNTER( TEXT( "RenderStats" ), STAT_RenderStats, STATGROUP_StatSystem );

	FGameThreadStatsData* ViewData = FLatestGameThreadStatsData::Get().Latest;
	if (!ViewData || !ViewData->bRenderStats)
	{
		return;
	}
	
	FStatRenderGlobals& Globals = GetStatRenderGlobals();
	// SizeX is used to clip/arrange the rendered stats to avoid overlay in stereo mode.
	const bool bIsStereo = Canvas->IsStereoRendering();
	Globals.Initialize( Viewport->GetSizeXY().X, Viewport->GetSizeXY().Y, SafeSizeX, bIsStereo, TextScale );

	if( !ViewData->bDrawOnlyRawStats )
	{
		RenderGroupedWithHierarchy(*ViewData, Viewport, Canvas, X, Y);
	}
	else
	{
		// Render all counters.
		for( int32 RowIndex = 0; RowIndex < ViewData->GroupDescriptions.Num(); ++RowIndex )
		{
			Canvas->DrawShadowedString(X, Y, *ViewData->GroupDescriptions[RowIndex], Globals.StatFont, Globals.StatColor, TextScale);
			Y += Globals.GetFontHeight();
		}
	}
}

#endif
