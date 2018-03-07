// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Widgets/Views/SHeaderRow.h"
#include "ITreeItem.h"

namespace SceneOutliner
{
	/**
	 * Templated helper to alleviate performance problems with sorting based on complex predicates.
	 * Example Usage:
	 * 		FSortHelper<FString>().Primary([](const ITreeItem& Item){ return Item->GetString(); }).SortItems(Array);
	 *
	 * Or:
	 * 		FSortHelper<FString, FString>()
	 *			.Primary(FGetPrimaryStringVisitor())
	 *			.Secondary(FGetSecondaryStringVisitor())
	 *			.SortItems(Array);
	 */
	template<typename PrimaryKeyType, typename SecondaryKeyType = int32>
	struct FSortHelper
	{
		typedef TFunction<PrimaryKeyType(const ITreeItem&)> FPrimaryFunction;
		typedef TFunction<SecondaryKeyType(const ITreeItem&)> FSecondaryFunction;

		FSortHelper()
			: PrimarySortMode(EColumnSortMode::None), SecondarySortMode(EColumnSortMode::None)
			, PrimaryVisitor(nullptr), SecondaryVisitor(nullptr)
		{}

		/** Sort primarily by the specified function and mode. Beware the function is a reference, so must be valid for the lifetime of this instance. */
		FSortHelper& Primary(FPrimaryFunction&& Function, EColumnSortMode::Type SortMode)
		{
			PrimarySortMode = SortMode;
			PrimaryFunction = MoveTemp(Function);
			return *this;
		}
		/** Sort primarily using the specified 'getter' visitor and mode */
		FSortHelper& Primary(const TTreeItemGetter<PrimaryKeyType>& Visitor, EColumnSortMode::Type SortMode)
		{
			PrimarySortMode = SortMode;
			PrimaryVisitor = &Visitor;
			return *this;
		}

		/** Sort secondarily by the specified function and mode. Beware the function is a reference, so must be valid for the lifetime of this instance. */
		FSortHelper& Secondary(FSecondaryFunction&& Function, EColumnSortMode::Type SortMode)
		{
			SecondarySortMode = SortMode;
			SecondaryFunction = MoveTemp(Function);
			return *this;
		}
		/** Sort secondarily using the specified 'getter' visitor and mode */
		FSortHelper& Secondary(const TTreeItemGetter<SecondaryKeyType>& Visitor, EColumnSortMode::Type SortMode)
		{
			PrimarySortMode = SortMode;
			SecondaryVisitor = &Visitor;
			return *this;
		}

		/** Sort the specified array using the current sort settings */
		void Sort(TArray<FTreeItemPtr>& Array)
		{
			TArray<FSortPayload> SortData;
			const auto End = Array.Num();
			for (int32 Index = 0; Index < End; ++Index)
			{
				const auto& Element = Array[Index];

				PrimaryKeyType PrimaryKey;
				if (PrimaryVisitor)
				{
					Element->Visit(*PrimaryVisitor);
					PrimaryKey = MoveTemp(PrimaryVisitor->Data);
				}
				else if (PrimaryFunction)
				{
					PrimaryKey = PrimaryFunction.GetValue()(*Element);
				}

				SecondaryKeyType SecondaryKey;
				if (SecondarySortMode != EColumnSortMode::None)
				{
					if (SecondaryVisitor)
					{
						Element->Visit(*SecondaryVisitor);
						SecondaryKey = MoveTemp(SecondaryVisitor->Data);
					}
					else if (SecondaryFunction)
					{
						SecondaryKey = SecondaryFunction.GetValue()(*Element);
					}
				}

				SortData.Emplace(Index, MoveTemp(PrimaryKey), MoveTemp(SecondaryKey));
			}

			SortData.Sort([&](const FSortPayload& One, const FSortPayload& Two){
				if (PrimarySortMode == EColumnSortMode::Ascending && One.PrimaryKey != Two.PrimaryKey)
					return One.PrimaryKey < Two.PrimaryKey;
				else if (PrimarySortMode == EColumnSortMode::Descending && One.PrimaryKey != Two.PrimaryKey)
					return One.PrimaryKey > Two.PrimaryKey;

				if (SecondarySortMode == EColumnSortMode::Ascending)
					return One.SecondaryKey < Two.SecondaryKey;
				else if (SecondarySortMode == EColumnSortMode::Descending)
					return One.SecondaryKey > Two.SecondaryKey;

				return false;
			});

			TArray<FTreeItemPtr> NewArray;
			NewArray.Reserve(Array.Num());

			for (const auto& Element : SortData)
			{
				NewArray.Add(Array[Element.OriginalIndex]);
			}
			Array = MoveTemp(NewArray);
		}

	private:

		EColumnSortMode::Type 	PrimarySortMode;
		EColumnSortMode::Type 	SecondarySortMode;

		TOptional<FPrimaryFunction>		PrimaryFunction;
		TOptional<FSecondaryFunction>	SecondaryFunction;

		const TTreeItemGetter<PrimaryKeyType>*		PrimaryVisitor;
		const TTreeItemGetter<SecondaryKeyType>*	SecondaryVisitor;

		/** Aggregated data from the sort methods. We extract the sort data from all elements first, then sort based on the extracted data. */
		struct FSortPayload
		{
			int32 OriginalIndex;
			
			PrimaryKeyType PrimaryKey;
			SecondaryKeyType SecondaryKey;

			FSortPayload(int32 InOriginalIndex, PrimaryKeyType&& InPrimaryKey, SecondaryKeyType&& InSecondaryKey)
				: OriginalIndex(InOriginalIndex), PrimaryKey(MoveTemp(InPrimaryKey)), SecondaryKey(MoveTemp(InSecondaryKey))
			{}

			FSortPayload(FSortPayload&& Other)
			{
				(*this) = MoveTemp(Other);
			}
			FSortPayload& operator=(FSortPayload&& rhs)
			{
				OriginalIndex = rhs.OriginalIndex;
				PrimaryKey = MoveTemp(rhs.PrimaryKey);
				SecondaryKey = MoveTemp(rhs.SecondaryKey);
				return *this;
			}

		private:
			FSortPayload(const FSortPayload&);
			FSortPayload& operator=(const FSortPayload&);
		};
	};

	/** Wrapper type that will sort FString's using a more natural comparison (correctly sorts numbers and ignores underscores) */
	struct FNumericStringWrapper
	{
		FString String;

		FNumericStringWrapper()
		{}
		FNumericStringWrapper(FString&& InString)
			: String(InString)
		{}
		FNumericStringWrapper(FNumericStringWrapper&& Other)
			: String(MoveTemp(Other.String))
		{}
		FNumericStringWrapper& operator=(FNumericStringWrapper&& Other)
		{
			String = MoveTemp(Other.String);
			return *this;
		}
		FORCEINLINE bool operator<(const FNumericStringWrapper& Other) const
		{
			return CompareNumeric(String, Other.String) < 0;
		}
		FORCEINLINE bool operator>(const FNumericStringWrapper& Other) const
		{
			return CompareNumeric(String, Other.String) > 0;
		}
		FORCEINLINE bool operator==(const FNumericStringWrapper& Other) const
		{
			return String == Other.String;
		}

		/** Compare the 2 specified strings using the specified default character comparison function */
		typedef int32 (*StrncmpMethod)(const TCHAR*, const TCHAR*, SIZE_T Count);
		static int32 CompareNumeric(const FString& A, const FString& B, StrncmpMethod Strncmp = FCString::Strnicmp)
		{
			const TCHAR* CharA = *A, *CharB = *B;
			const TCHAR* EndA = CharA + A.Len(), *EndB = CharB + B.Len();

			while(CharA < EndA && CharB < EndB)
			{
				// Ignore underscores
				if (*CharA == '_')
				{
					++CharA;
					continue;
				}
				if (*CharB == '_')
				{
					++CharB;
					continue;
				}
				
				TCHAR *IntAEnd = nullptr;
				const uint64 IntA = FCString::Strtoui64(CharA, &IntAEnd, 10);

				TCHAR *IntBEnd = nullptr;
				const uint64 IntB = FCString::Strtoui64(CharB, &IntBEnd, 10);

				const bool AIsNum = (IntAEnd != CharA), BIsNum = (IntBEnd != CharB);
				if (AIsNum != BIsNum)
				{
					// At the current CharA/CharB position, either (but not both) of the strings is an integer
					// Numbers are considered less than characters
					return AIsNum ? -1 : 1;
				}
				else if (AIsNum)
				{
					CharA = IntAEnd;
					CharB = IntBEnd;

					if (IntA != IntB)
					{
						return IntA < IntB ? -1 : 1;
					}
				}
				else if (int32 Cmp = (*Strncmp)(CharA, CharB, 1))
				{
					return Cmp;
				}
				else
				{
					++CharA;
					++CharB;
				}
			}

			if (CharA == EndA && CharB == EndB)
			{
				// Strings compared equal, return shortest first
				if (const int32 LengthDifference = A.Len() - B.Len())
				{
					return LengthDifference / FMath::Abs(LengthDifference);
				}
				else
				{
					return 0;
				}
			}

			// Strings are different comparative lengths, return shortest
			return CharA == EndA ? -1 : 1;
		}

	private:
		FNumericStringWrapper(const FNumericStringWrapper&);
		FNumericStringWrapper& operator=(const FNumericStringWrapper&);
	};
}
