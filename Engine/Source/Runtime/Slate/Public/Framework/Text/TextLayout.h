// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Misc/Attribute.h"
#include "Layout/Margin.h"
#include "Framework/Text/TextRange.h"
#include "Framework/Text/TextRunRenderer.h"
#include "Framework/Text/TextLineHighlight.h"
#include "Framework/Text/IRun.h"
#include "TextLayout.generated.h"

#define TEXT_LAYOUT_DEBUG 0

class IBreakIterator;
class ILayoutBlock;
class ILineHighlighter;
class IRunRenderer;
enum class ETextHitPoint : uint8;
enum class ETextShapingMethod : uint8;

UENUM( BlueprintType )
namespace ETextJustify
{
	enum Type
	{
		/**
		 * Justify the text logically to the left.
		 * When text is flowing left-to-right, this will align text visually to the left.
		 * When text is flowing right-to-left, this will align text visually to the right.
		 */
		Left,

		/**
		 * Justify the text in the center.
		 * Text flow direction has no impact on this justification mode.
		 */
		Center,

		/**
		 * Justify the text logically to the right.
		 * When text is flowing left-to-right, this will align text visually to the right.
		 * When text is flowing right-to-left, this will align text visually to the left.
		 */
		Right,
	};
}

/** 
 * The different methods that can be used if a word is too long to be broken by the default line-break iterator.
 */
UENUM( BlueprintType )
enum class ETextWrappingPolicy : uint8
{
	/** No fallback, just use the given line-break iterator */
	DefaultWrapping = 0,

	/** Fallback to per-character wrapping if a word is too long */
	AllowPerCharacterWrapping,
};

/** 
 * The different directions that text can flow within a paragraph of text.
 * @note If you change this enum, make sure and update CVarDefaultTextFlowDirection and GetDefaultTextFlowDirection.
 */
UENUM( BlueprintType )
enum class ETextFlowDirection : uint8
{
	/** Automatically detect the flow direction for each paragraph from its text */
	Auto = 0,

	/** Force text to be flowed left-to-right */
	LeftToRight,

	/** Force text to be flowed right-to-left */
	RightToLeft,
};

/** Get the default text flow direction (from the "Slate.DefaultTextFlowDirection" CVar) */
SLATE_API ETextFlowDirection GetDefaultTextFlowDirection();

/** Location within the text model. */
struct FTextLocation
{
public:
	FTextLocation( const int32 InLineIndex = 0, const int32 InOffset = 0 )
		: LineIndex( InLineIndex )
		, Offset( InOffset )
	{

	}

	FTextLocation( const FTextLocation& InLocation, const int32 InOffset )
		: LineIndex( InLocation.GetLineIndex() )
		, Offset(FMath::Max(InLocation.GetOffset() + InOffset, 0))
	{

	}

	bool operator==( const FTextLocation& Other ) const
	{
		return
			LineIndex == Other.LineIndex &&
			Offset == Other.Offset;
	}

	bool operator!=( const FTextLocation& Other ) const
	{
		return
			LineIndex != Other.LineIndex ||
			Offset != Other.Offset;
	}

	bool operator<( const FTextLocation& Other ) const
	{
		return this->LineIndex < Other.LineIndex || (this->LineIndex == Other.LineIndex && this->Offset < Other.Offset);
	}

	int32 GetLineIndex() const { return LineIndex; }
	int32 GetOffset() const { return Offset; }
	bool IsValid() const { return LineIndex != INDEX_NONE && Offset != INDEX_NONE; }

private:
	int32 LineIndex;
	int32 Offset;
};

class FTextSelection
{

public:

	FTextLocation LocationA;

	FTextLocation LocationB;

public:

	FTextSelection()
		: LocationA(INDEX_NONE)
		, LocationB(INDEX_NONE)
	{
	}

	FTextSelection(const FTextLocation& InLocationA, const FTextLocation& InLocationB)
		: LocationA(InLocationA)
		, LocationB(InLocationB)
	{
	}

	const FTextLocation& GetBeginning() const
	{
		if (LocationA.GetLineIndex() == LocationB.GetLineIndex())
		{
			if (LocationA.GetOffset() < LocationB.GetOffset())
			{
				return LocationA;
			}

			return LocationB;
		}
		else if (LocationA.GetLineIndex() < LocationB.GetLineIndex())
		{
			return LocationA;
		}

		return LocationB;
	}

	const FTextLocation& GetEnd() const
	{
		if (LocationA.GetLineIndex() == LocationB.GetLineIndex())
		{
			if (LocationA.GetOffset() > LocationB.GetOffset())
			{
				return LocationA;
			}

			return LocationB;
		}
		else if (LocationA.GetLineIndex() > LocationB.GetLineIndex())
		{
			return LocationA;
		}

		return LocationB;
	}
};

class SLATE_API FTextLayout
	: public TSharedFromThis<FTextLayout>
{
public:

	struct FBlockDefinition
	{
		/** Range inclusive of trailing whitespace, as used to visually display and interact with the text */
		FTextRange ActualRange;
		/** The render to use with this run (if any) */
		TSharedPtr< IRunRenderer > Renderer;
	};

	struct FBreakCandidate
	{
		/** Range inclusive of trailing whitespace, as used to visually display and interact with the text */
		FTextRange ActualRange;
		/** Range exclusive of trailing whitespace, as used to perform wrapping on a word boundary */
		FTextRange TrimmedRange;
		/** Measured size inclusive of trailing whitespace, as used to visually display and interact with the text */
		FVector2D ActualSize;
		/** Measured size exclusive of trailing whitespace, as used to perform wrapping on a word boundary */
		FVector2D TrimmedSize;
		/** If this break candidate has trailing whitespace, this is the width of the first character of the trailing whitespace */
		float FirstTrailingWhitespaceCharWidth;

		int16 MaxAboveBaseline;
		int16 MaxBelowBaseline;

		uint8 Kerning;

#if TEXT_LAYOUT_DEBUG
		FString DebugSlice;
#endif
	};

	class SLATE_API FRunModel
	{
	public:

		FRunModel( const TSharedRef< IRun >& InRun);

	public:

		TSharedRef< IRun > GetRun() const;;

		void BeginLayout();
		void EndLayout();

		FTextRange GetTextRange() const;
		void SetTextRange( const FTextRange& Value );
		
		int16 GetBaseLine( float Scale ) const;
		int16 GetMaxHeight( float Scale ) const;

		FVector2D Measure( int32 BeginIndex, int32 EndIndex, float Scale, const FRunTextContext& InTextContext );

		uint8 GetKerning( int32 CurrentIndex, float Scale, const FRunTextContext& InTextContext );

		static int32 BinarySearchForBeginIndex( const TArray< FTextRange >& Ranges, int32 BeginIndex );

		static int32 BinarySearchForEndIndex( const TArray< FTextRange >& Ranges, int32 RangeBeginIndex, int32 EndIndex );

		TSharedRef< ILayoutBlock > CreateBlock( const FBlockDefinition& BlockDefine, float InScale, const FLayoutBlockTextContext& InTextContext ) const;

		void ClearCache();

		void AppendTextTo(FString& Text) const;
		void AppendTextTo(FString& Text, const FTextRange& Range) const;

	private:

		TSharedRef< IRun > Run;
		TArray< FTextRange > MeasuredRanges;
		TArray< FVector2D > MeasuredRangeSizes;
	};

	struct ELineModelDirtyState
	{
		typedef uint8 Flags;
		enum Enum
		{
			None = 0,
			WrappingInformation = 1<<0,
			TextBaseDirection = 1<<1, 
			ShapingCache = 1<<2,
			All = WrappingInformation | TextBaseDirection | ShapingCache,
		};
	};

	struct SLATE_API FLineModel
	{
	public:

		FLineModel( const TSharedRef< FString >& InText );

		TSharedRef< FString > Text;
		FShapedTextCacheRef ShapedTextCache;
		TextBiDi::ETextDirection TextBaseDirection;
		TArray< FRunModel > Runs;
		TArray< FBreakCandidate > BreakCandidates;
		TArray< FTextRunRenderer > RunRenderers;
		TArray< FTextLineHighlight > LineHighlights;
		ELineModelDirtyState::Flags DirtyFlags;
	};

	struct FLineViewHighlight
	{
		/** Offset in X for this highlight, relative to the FLineView::Offset that contains it */
		float OffsetX;
		/** Width for this highlight, the height will be either FLineView::Size.Y or FLineView::TextSize.Y depending on whether you want to highlight the entire line, or just the text within the line */
		float Width;
		/** Custom highlighter implementation used to do the painting */
		TSharedPtr< ILineHighlighter > Highlighter;
	};

	struct FLineView
	{
		TArray< TSharedRef< ILayoutBlock > > Blocks;
		TArray< FLineViewHighlight > UnderlayHighlights;
		TArray< FLineViewHighlight > OverlayHighlights;
		FVector2D Offset;
		FVector2D Size;
		FVector2D TextSize;
		FTextRange Range;
		TextBiDi::ETextDirection TextBaseDirection;
		int32 ModelIndex;
	};

	/** A mapping between the offsets into the text as a flat string (with line-breaks), and the internal lines used within a text layout */
	struct SLATE_API FTextOffsetLocations
	{
	friend class FTextLayout;

	public:
		int32 TextLocationToOffset(const FTextLocation& InLocation) const;
		FTextLocation OffsetToTextLocation(const int32 InOffset) const;
		int32 GetTextLength() const;

	private:
		struct FOffsetEntry
		{
			FOffsetEntry(const int32 InFlatStringIndex, const int32 InDocumentLineLength)
				: FlatStringIndex(InFlatStringIndex)
				, DocumentLineLength(InDocumentLineLength)
			{
			}

			/** Index in the flat string for this entry */
			int32 FlatStringIndex;

			/** The length of the line in the document (not including any trailing \n character) */
			int32 DocumentLineLength;
		};

		/** This array contains one entry for each line in the document; 
			the array index is the line number, and the entry contains the index in the flat string that marks the start of the line, 
			along with the length of the line (not including any trailing \n character) */
		TArray<FOffsetEntry> OffsetData;
	};

public:

	virtual ~FTextLayout();

	FORCEINLINE const TArray< FTextLayout::FLineView >& GetLineViews() const { return LineViews; }
	FORCEINLINE const TArray< FTextLayout::FLineModel >& GetLineModels() const { return LineModels; }

	FVector2D GetSize() const;
	FVector2D GetDrawSize() const;
	FVector2D GetWrappedSize() const;

	FORCEINLINE float GetWrappingWidth() const { return WrappingWidth; }
	void SetWrappingWidth( float Value );

	FORCEINLINE ETextWrappingPolicy GetWrappingPolicy() const { return WrappingPolicy; }
	void SetWrappingPolicy(ETextWrappingPolicy Value);

	FORCEINLINE float GetLineHeightPercentage() const { return LineHeightPercentage; }
	void SetLineHeightPercentage( float Value );

	FORCEINLINE ETextJustify::Type GetJustification() const { return Justification; }
	void SetJustification( ETextJustify::Type Value );

	FORCEINLINE float GetScale() const { return Scale; }
	void SetScale( float Value );

	FORCEINLINE ETextShapingMethod GetTextShapingMethod() const { return TextShapingMethod; }
	void SetTextShapingMethod( const ETextShapingMethod InTextShapingMethod );

	FORCEINLINE ETextFlowDirection GetTextFlowDirection() const { return TextFlowDirection; }
	void SetTextFlowDirection( const ETextFlowDirection InTextFlowDirection );

	FORCEINLINE FMargin GetMargin() const { return Margin; }
	void SetMargin( const FMargin& InMargin );

	void SetVisibleRegion( const FVector2D& InViewSize, const FVector2D& InScrollOffset );

	/** Set the iterator to use to detect appropriate soft-wrapping points for lines (or null to go back to using the default) */
	void SetLineBreakIterator( TSharedPtr<IBreakIterator> InLineBreakIterator );

	/** Set the information used to help identify who owns this text layout in the case of an error */
	void SetDebugSourceInfo(const TAttribute<FString>& InDebugSourceInfo);

	void ClearLines();

	struct FNewLineData
	{
		FNewLineData(TSharedRef<FString> InText, TArray<TSharedRef<IRun>> InRuns)
			: Text(MoveTemp(InText))
			, Runs(MoveTemp(InRuns))
		{
		}

		TSharedRef<FString> Text;
		TArray<TSharedRef<IRun>> Runs;
	};

	DEPRECATED(4.11, "Please use the version of AddLine that takes an FNewLineData parameter.")
	void AddLine( const TSharedRef< FString >& Text, const TArray< TSharedRef< IRun > >& Runs );

	void AddLine( const FNewLineData& NewLine );

	void AddLines( const TArray<FNewLineData>& NewLines );

	/**
	* Clears all run renderers
	*/
	void ClearRunRenderers();

	/**
	* Replaces the current set of run renderers with the provided renderers.
	*/
	void SetRunRenderers( const TArray< FTextRunRenderer >& Renderers );

	/**
	* Adds a single run renderer to the existing set of renderers.
	*/
	void AddRunRenderer( const FTextRunRenderer& Renderer );

	/**
	* Removes a single run renderer to the existing set of renderers.
	*/
	void RemoveRunRenderer( const FTextRunRenderer& Renderer );

	/**
	* Clears all line highlights
	*/
	void ClearLineHighlights();

	/**
	* Replaces the current set of line highlights with the provided highlights.
	*/
	void SetLineHighlights( const TArray< FTextLineHighlight >& Highlights );

	/**
	* Adds a single line highlight to the existing set of highlights.
	*/
	void AddLineHighlight( const FTextLineHighlight& Highlight );

	/**
	* Removes a single line highlight to the existing set of highlights.
	*/
	void RemoveLineHighlight( const FTextLineHighlight& Highlight );

	/**
	* Updates the TextLayout's if any changes have occurred since the last update.
	*/
	virtual void UpdateIfNeeded();

	virtual void UpdateLayout();

	virtual void UpdateHighlights();

	void DirtyRunLayout(const TSharedRef<const IRun>& Run);

	void DirtyLayout();

	bool IsLayoutDirty() const;

	int32 GetLineViewIndexForTextLocation(const TArray< FTextLayout::FLineView >& LineViews, const FTextLocation& Location, const bool bPerformInclusiveBoundsCheck) const;

	/**
	 * 
	 */
	FTextLocation GetTextLocationAt( const FVector2D& Relative, ETextHitPoint* const OutHitPoint = nullptr ) const;

	FTextLocation GetTextLocationAt( const FLineView& LineView, const FVector2D& Relative, ETextHitPoint* const OutHitPoint = nullptr ) const;

	FVector2D GetLocationAt( const FTextLocation& Location, const bool bPerformInclusiveBoundsCheck ) const;

	bool SplitLineAt(const FTextLocation& Location);

	bool JoinLineWithNextLine(int32 LineIndex);

	bool InsertAt(const FTextLocation& Location, TCHAR Character);

	bool InsertAt(const FTextLocation& Location, const FString& Text);

	bool InsertAt(const FTextLocation& Location, TSharedRef<IRun> InRun, const bool bAlwaysKeepRightRun = false);

	bool RemoveAt(const FTextLocation& Location, int32 Count = 1);

	bool RemoveLine(int32 LineIndex);

	bool IsEmpty() const;

	int32 GetLineCount() const;

	void GetAsText(FString& DisplayText, FTextOffsetLocations* const OutTextOffsetLocations = nullptr) const;

	void GetAsText(FText& DisplayText, FTextOffsetLocations* const OutTextOffsetLocations = nullptr) const;

	/** Constructs an array containing the mappings between the text that would be returned by GetAsText, and the internal FTextLocation points used within this text layout */
	void GetTextOffsetLocations(FTextOffsetLocations& OutTextOffsetLocations) const;

	void GetSelectionAsText(FString& DisplayText, const FTextSelection& Selection) const;

	FTextSelection GetWordAt(const FTextLocation& Location) const;

protected:

	FTextLayout();

	/**
	 * Calculates the text direction for each line based upon the current shaping method and document flow direction
	 * When changing the shaping method, or document flow direction, all the lines need to be dirtied (see DirtyAllLineModels(ELineModelDirtyState::TextBaseDirection))
	 */
	void CalculateTextDirection();

	/**
	 * Calculates the text direction for the given line based upon the current shaping method and document flow direction
	 */
	void CalculateLineTextDirection(FLineModel& LineModel) const;

	/**
	 * Calculates the visual justification for the given line view
	 */
	ETextJustify::Type CalculateLineViewVisualJustification(const FLineView& LineView) const;

	/**
	* Create the wrapping cache for the current text based upon the current scale
	* Each line keeps its own cached state, so needs to be cleared when changing the text within a line
	* When changing the scale, all the lines need to be cleared (see DirtyAllLineModels(ELineModelDirtyState::WrappingInformation))
	*/
	void CreateWrappingCache();

	/**
	* Create the wrapping cache for the given line based upon the current scale
	*/
	void CreateLineWrappingCache(FLineModel& LineModel);

	/**
	 * Flushes the text shaping cache for each line
	 */
	void FlushTextShapingCache();

	/**
	 * Flushes the text shaping cache for the given line
	 */
	void FlushLineTextShapingCache(FLineModel& LineModel);

	/**
	 * Set the given dirty flags on all line models in this layout
	 */
	void DirtyAllLineModels(const ELineModelDirtyState::Flags InDirtyFlags);

	/**
	* Clears the current layouts view information.
	*/
	void ClearView();

	/**
	* Notifies all Runs that we are beginning to generate a new layout.
	*/
	virtual void BeginLayout();

	/**
	* Notifies all Runs on the given line is beginning to have a new layout generated.
	*/
	void BeginLineLayout(FLineModel& LineModel);

	/**
	* Notifies all Runs that the layout has finished generating.
	*/
	virtual void EndLayout();

	/**
	* Notifies all Runs on the given line has finished having a new layout generated.
	*/
	void EndLineLayout(FLineModel& LineModel);

	/**
	 * Called to generate a new empty text run for this text layout
	 */
	virtual TSharedRef<IRun> CreateDefaultTextRun(const TSharedRef<FString>& NewText, const FTextRange& NewRange) const = 0;

private:

	float GetWrappingDrawWidth() const;

	void FlowLayout();
	void MarginLayout();

	void FlowLineLayout(const int32 LineModelIndex, const float WrappingDrawWidth, TArray<TSharedRef<ILayoutBlock>>& SoftLine);

	void FlowHighlights();

	void JustifyLayout();

	void CreateLineViewBlocks( int32 LineModelIndex, const int32 StopIndex, const float WrappedLineWidth, int32& OutRunIndex, int32& OutRendererIndex, int32& OutPreviousBlockEnd, TArray< TSharedRef< ILayoutBlock > >& OutSoftLine );

	FBreakCandidate CreateBreakCandidate( int32& OutRunIndex, FLineModel& Line, int32 PreviousBreak, int32 CurrentBreak );

	void GetAsTextAndOffsets(FString* const OutDisplayText, FTextOffsetLocations* const OutTextOffsetLocations) const;

protected:
	
	struct ETextLayoutDirtyState
	{
		typedef uint8 Flags;
		enum Enum
		{
			None = 0,
			Layout = 1<<0,
			Highlights = 1<<1, 
			All = Layout | Highlights,
		};
	};

	struct FTextLayoutSize
	{
		FTextLayoutSize()
			: DrawWidth(0.0f)
			, WrappedWidth(0.0f)
			, Height(0.0f)
		{
		}

		FVector2D GetDrawSize() const
		{
			return FVector2D(DrawWidth, Height);
		}

		FVector2D GetWrappedSize() const
		{
			return FVector2D(WrappedWidth, Height);
		}

		/** Width of the text layout, including any lines which extend beyond the wrapping boundaries (eg, lines with lots of trailing whitespace, or lines with no break candidates) */
		float DrawWidth;

		/** Width of the text layout after the text has been wrapped, and including the first piece of trailing whitespace for any given soft-wrapped line */
		float WrappedWidth;

		/** Height of the text layout */
		float Height;
	};

	/** The models for the lines of text. A LineModel represents a single string with no manual breaks. */
	TArray< FLineModel > LineModels;

	/** The views for the lines of text. A LineView represents a single visual line of text. Multiple LineViews can map to the same LineModel, if for example wrapping occurs. */
	TArray< FLineView > LineViews;

	/** The indices for all of the line views that require justification. */
	TSet< int32 > LineViewsToJustify;

	/** Whether parameters on the layout have changed which requires the view be updated. */
	ETextLayoutDirtyState::Flags DirtyFlags;

	/** The method used to shape the text within this layout */
	ETextShapingMethod TextShapingMethod;

	/** How should the text within this layout be flowed? */
	ETextFlowDirection TextFlowDirection;

	/** The scale to draw the text at */
	float Scale;

	/** The width that the text should be wrap at. If 0 or negative no wrapping occurs. */
	float WrappingWidth;

	/** The wrapping policy used by this text layout. */
	ETextWrappingPolicy WrappingPolicy;

	/** The size of the margins to put about the text. This is an unscaled value. */
	FMargin Margin;

	/** How the text should be aligned with the margin. */
	ETextJustify::Type Justification;

	/** The percentage to modify a line height by. */
	float LineHeightPercentage;

	/** The final size of the text layout on screen. */
	FTextLayoutSize TextLayoutSize;

	/** The size of the text layout that can actually be seen from the parent widget */
	FVector2D ViewSize;

	/** The scroll offset of the text layout from the parent widget */
	FVector2D ScrollOffset;

	/** The iterator to use to detect appropriate soft-wrapping points for lines */
	TSharedPtr<IBreakIterator> LineBreakIterator;

	/** The iterator to use to detect grapheme cluster boundaries */
	TSharedRef<IBreakIterator> GraphemeBreakIterator;

	/** The iterator to use to detect word boundaries */
	TSharedRef<IBreakIterator> WordBreakIterator;

	/** Unicode BiDi text detection */
	TUniquePtr<TextBiDi::ITextBiDi> TextBiDiDetection;

	/** Information given to use by our an external source (typically our owner widget) to help identify who owns this text layout in the case of an error */
	TAttribute<FString> DebugSourceInfo;
};
