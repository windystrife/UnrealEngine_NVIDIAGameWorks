// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Class.h"

class UEnum;
class UScriptStruct;
class UDelegateFunction;

// Forward declarations.
class UStruct;
class FClassMetaData;
class FUnrealSourceFile;
class FFileScope;
class FStructScope;

// Traits to achieve conditional types for const/non-const iterators.
template <bool Condition, class TypeIfTrue, class TypeIfFalse>
class TConditionalType
{
public:
	typedef TypeIfFalse Type;
};

template <class TypeIfTrue, class TypeIfFalse>
class TConditionalType<true, TypeIfTrue, TypeIfFalse>
{
public:
	typedef TypeIfTrue Type;
};

// Base class representing type scope.
class FScope : public TSharedFromThis<FScope>
{
public:
	// Default constructor i.e. Parent == nullptr
	FScope();

	// Constructor that's providing parent scope.
	FScope(FScope* Parent);

	// Virtual destructor.
	virtual ~FScope()
	{ };

	virtual FFileScope* AsFileScope() { return nullptr; }
	virtual FStructScope* AsStructScope() { return nullptr; }

	/**
	 * Adds type to the scope.
	 *
	 * @param Type Type to add.
	 */
	void AddType(UField* Type);

	/**
	 * Finds type by its name.
	 *
	 * @param Name Name to look for.
	 *
	 * @returns Found type or nullptr on failure.
	 */
	UField* FindTypeByName(FName Name);

	/**
	 * Finds type by its name.
	 *
	 * Const version.
	 *
	 * @param Name Name to look for.
	 *
	 * @returns Found type or nullptr on failure.
	 */
	const UField* FindTypeByName(FName Name) const;

	/**
	 * Checks if scope contains this type.
	 *
	 * @param Type Type to look for.
	 */
	bool ContainsType(UField* Type);

	/**
	 * Checks if scope contains type that satisfies a predicate.
	 *
	 * @param Predicate Predicate to satisfy.
	 */
	template <typename TPredicateType>
	bool Contains(TPredicateType Predicate)
	{
		for (const auto& NameTypePair : TypeMap)
		{
			if (Predicate.operator()(NameTypePair.Value))
			{
				return true;
			}
		}

		return false;
	}

	/**
	 * Gets struct's or class's scope.
	 *
	 * @param Type A struct or class for which to return the scope.
	 *
	 * @returns Struct's or class's scope.
	 */
	static TSharedRef<FScope> GetTypeScope(UStruct* Type);

	/**
	 * Adds struct's or class's scope.
	 *
	 * @param Type A struct or class for which to add the scope.
	 * @param ParentScope A scope in which this scope is contained.
	 *
	 * @returns Newly added scope.
	 */
	static TSharedRef<FScope> AddTypeScope(UStruct* Type, FScope* ParentScope);

	/**
	 * Gets structs, enums and delegate functions from this scope.
	 *
	 * @param Enums (Output parameter) List of enums from this scope.
	 * @param Structs (Output parameter) List of structs from this scope.
	 * @param DelegateFunctions (Output parameter) List of delegate properties from this scope.
	 */
	void SplitTypesIntoArrays(TArray<UEnum*>& Enums, TArray<UScriptStruct*>& Structs, TArray<UDelegateFunction*>& DelegateFunctions);

	/**
	 * Gets scope name.
	 */
	virtual FName GetName() const = 0;

	/**
	 * Gets scope types filtered by type.
	 *
	 * @param OutArray (Output parameter) Array to fill with types from this scope.
	 */
	template <class TTypeFilter>
	void GetTypes(TArray<TTypeFilter*> OutArray)
	{
		for (const auto& NameTypePair : TypeMap)
		{
			if (NameTypePair.Value->IsA<TTypeFilter>())
			{
				OutArray.Add((TTypeFilter*)NameTypePair.Value);
			}
		}
	}

	/**
	 * Gets scope's parent.
	 */
	const FScope* GetParent() const
	{
		return Parent;
	}

	/**
	 * Scope type iterator.
	 */
	template <class TType, bool TIsConst>
	class TScopeTypeIterator
	{
	public:
		// Conditional typedefs.
		typedef typename TConditionalType<TIsConst, TMap<FName, UField*>::TConstIterator, TMap<FName, UField*>::TIterator>::Type MapIteratorType;
		typedef typename TConditionalType<TIsConst, const FScope, FScope>::Type ScopeType;

		// Constructor
		TScopeTypeIterator(ScopeType* Scope)
			: TypeIterator(Scope->TypeMap)
		{
			bBeforeStart = true;
		}

		/**
		 * Indirection operator.
		 *
		 * @returns Type this iterator currently point to.
		 */
		TType* operator*()
		{
			return bBeforeStart ? nullptr : (TType*)(*TypeIterator).Value;
		}

		/**
		 * Moves this iterator to next type.
		 *
		 * @returns True on success. False otherwise.
		 */
		bool MoveNext()
		{
			if (!bBeforeStart)
			{
				++TypeIterator;
			}

			bBeforeStart = false;
			return TypeIterator.operator bool();
		}

	private:

		// Current type.
		MapIteratorType TypeIterator;

		// Tells if this iterator is in initial state.
		bool bBeforeStart;
	};

	/**
	 * Gets type iterator.
	 *
	 * @return Type iterator.
	 */
	template <class TType = UField>
	TScopeTypeIterator<TType, false> GetTypeIterator()
	{
		return TScopeTypeIterator<TType, false>(this);
	}

	/**
	 * Gets const type iterator.
	 *
	 * @return Const type iterator.
	 */
	template <class TType = UField>
	TScopeTypeIterator<TType, true> GetTypeIterator() const
	{
		return TScopeTypeIterator<TType, true>(this);
	}

	/**
	 * Tells if this scope is a file scope.
	 *
	 * @returns True if this is a file scope. False otherwise.
	 */
	bool IsFileScope() const;

	/**
	 * Tells if this scope contains any type.
	 *
	 * @returns True if this scope contains type. False otherwise.
	 */
	bool ContainsTypes() const;

	FFileScope* GetFileScope();
private:
	// This scopes parent.
	const FScope* Parent;

	// Map of types in this scope.
	TMap<FName, UField*> TypeMap;

	// Global map type <-> scope.
	static TMap<UStruct*, TSharedRef<FScope> > ScopeMap;

	friend struct FScopeArchiveProxy;
	friend struct FStructScopeArchiveProxy;
};

/**
 * Represents a scope associated with source file.
 */
class FFileScope : public FScope
{
public:
	FFileScope()
		: SourceFile(nullptr)
		, Name(NAME_None)
	{ }
	// Constructor.
	FFileScope(FName Name, FUnrealSourceFile* SourceFile);

	/**
	 * Includes other file scopes into this scope (#include).
	 *
	 * @param IncludedScope File scope to include.
	 */
	void IncludeScope(FFileScope* IncludedScope);

	virtual FFileScope* AsFileScope() override { return this; }

	/**
	 * Gets scope name.
	 */
	FName GetName() const override;

	/**
	 * Gets source file associated with this scope.
	 */
	FUnrealSourceFile* GetSourceFile() const;

	/**
	 * Appends included file scopes into given array.
	 *
	 * @param Out (Output parameter) Array to append scopes.
	 */
	void AppendIncludedFileScopes(TArray<FScope*>& Out)
	{
		if (!Out.Contains(this))
		{
			Out.Add(this);
		}

		for (FFileScope* IncludedScope : IncludedScopes)
		{
			if (!Out.Contains(IncludedScope))
			{
				Out.Add(IncludedScope);
				IncludedScope->AppendIncludedFileScopes(Out);
			}
		}
	}


	const TArray<FFileScope*>& GetIncludedScopes() const
	{
		return IncludedScopes;
	}

	void SetSourceFile(FUnrealSourceFile* InSourceFile)
	{
		SourceFile = InSourceFile;
	}
private:
	// Source file.
	FUnrealSourceFile* SourceFile;

	// Scope name.
	FName Name;

	// Included scopes list.
	TArray<FFileScope*> IncludedScopes;

	friend struct FFileScopeArchiveProxy;
};


/**
 * Data structure representing scope of a struct or class.
 */
class FStructScope : public FScope
{
public:
	// Constructor.
	FStructScope(UStruct* Struct, FScope* Parent);

	virtual FStructScope* AsStructScope() override { return this; }
	/**
	 * Gets struct associated with this scope.
	 */
	UStruct* GetStruct() const;

	/**
	 * Gets scope name.
	 */
	FName GetName() const override;

private:
	// Struct associated with this scope.
	UStruct* Struct;

	friend struct FStructScopeArchiveProxy;
};

/**
 * Deep scope type iterator. It looks for type in the whole scope hierarchy.
 * First going up inheritance from inner structs to outer. Then through all
 * included scopes.
 */
template <class TType, bool TIsConst>
class TDeepScopeTypeIterator
{
public:
	// Conditional typedefs.
	typedef typename TConditionalType<TIsConst, TMap<FName, UField*>::TConstIterator, TMap<FName, UField*>::TIterator>::Type MapIteratorType;
	typedef typename TConditionalType<TIsConst, const FScope, FScope>::Type ScopeType;
	typedef typename TConditionalType<TIsConst, const FFileScope, FFileScope>::Type FileScopeType;
	typedef typename TConditionalType<TIsConst, typename TArray<ScopeType*>::TConstIterator, typename TArray<ScopeType*>::TIterator>::Type ScopeArrayIteratorType;

	// Constructor.
	TDeepScopeTypeIterator(ScopeType* Scope)
	{
		const ScopeType* CurrentScope = Scope;

		while (!CurrentScope->IsFileScope())
		{
			ScopesToTraverse.Add(Scope);

			UStruct* Struct = ((FStructScope*)CurrentScope)->GetStruct();

			if (Struct->IsA<UClass>())
			{
				UClass* Class = ((UClass*)Struct)->GetSuperClass();

				while (Class && !(Class->ClassFlags & EClassFlags::CLASS_Intrinsic))
				{
					ScopesToTraverse.Add(&FScope::GetTypeScope(Class).Get());
					Class = Class->GetSuperClass();
				}
			}

			CurrentScope = CurrentScope->GetParent();
		}

		((FileScopeType*)CurrentScope)->AppendIncludedFileScopes(ScopesToTraverse);
	}

	/**
	 * Iterator increment.
	 *
	 * @returns True if moved iterator to another position. False otherwise.
	 */
	bool MoveNext()
	{
		if (!ScopeIterator.IsValid() && !MoveToNextScope())
		{
			return false;
		}

		if (ScopeIterator->MoveNext())
		{
			return true;
		}
		else
		{
			do
			{
				ScopeIterator = nullptr;
				if (!MoveToNextScope())
				{
					return false;
				}
			}
			while(!ScopeIterator->MoveNext());

			return true;
		}
	}

	/**
	 * Current type getter.
	 *
	 * @returns Current type.
	 */
	TType* operator*()
	{
		return ScopeIterator->operator*();
	}

private:
	/**
	 * Moves the iterator to the next scope.
	 *
	 * @returns True if succeeded. False otherwise.
	 */
	bool MoveToNextScope()
	{
		if (!ScopesIterator.IsValid())
		{
			ScopesIterator = MakeShareable(new ScopeArrayIteratorType(ScopesToTraverse));
		}
		else
		{
			ScopesIterator->operator++();
		}

		if (!ScopesIterator->operator bool())
		{
			return false;
		}

		ScopeIterator = MakeShareable(new FScope::TScopeTypeIterator<TType, TIsConst>(ScopesIterator->operator*()));
		return true;
	}

	// List of scopes to traverse.
	TArray<ScopeType*> ScopesToTraverse;

	// Current scope iterator.
	TSharedPtr<FScope::TScopeTypeIterator<TType, TIsConst> > ScopeIterator;

	// Scopes list iterator.
	TSharedPtr<ScopeArrayIteratorType> ScopesIterator;
};

