// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Serialization/JsonTypes.h"

class FJsonObject;

/**
 * A Json Value is a structure that can be any of the Json Types.
 * It should never be used on its, only its derived types should be used.
 */
class JSON_API FJsonValue
{
public:

	/** Returns this value as a double, logging an error and returning zero if this is not an Json Number */
	double AsNumber() const;

	/** Returns this value as a string, logging an error and returning an empty string if not possible */
	FString AsString() const;

	/** Returns this value as a boolean, logging an error and returning false if not possible */
	bool AsBool() const;

	/** Returns this value as an array, logging an error and returning an empty array reference if not possible */
	const TArray< TSharedPtr<FJsonValue> >& AsArray() const;

	/** Returns this value as an object, throwing an error if this is not an Json Object */
	virtual const TSharedPtr<FJsonObject>& AsObject() const;

	/** Tries to convert this value to a number, returning false if not possible */
	virtual bool TryGetNumber(double& OutDouble) const { return false; }

	/** Tries to convert this value to an integer, returning false if not possible */
	bool TryGetNumber(int32& OutNumber) const;

	/** Tries to convert this value to an integer, returning false if not possible */
	bool TryGetNumber(uint32& OutNumber) const;

	/** Tries to convert this value to an integer, returning false if not possible */
	bool TryGetNumber(int64& OutNumber) const;

	/** Tries to convert this value to a string, returning false if not possible */
	virtual bool TryGetString(FString& OutString) const { return false; }

	/** Tries to convert this value to a bool, returning false if not possible */
	virtual bool TryGetBool(bool& OutBool) const { return false; }

	/** Tries to convert this value to an array, returning false if not possible */
	virtual bool TryGetArray(const TArray< TSharedPtr<FJsonValue> >*& OutArray) const { return false; }

	/** Tries to convert this value to an object, returning false if not possible */
	virtual bool TryGetObject(const TSharedPtr<FJsonObject>*& Object) const { return false; }

	/** Returns true if this value is a 'null' */
	bool IsNull() const { return Type == EJson::Null || Type == EJson::None; }

	/** Get a field of the same type as the argument */
	void AsArgumentType(double                          & Value) { Value = AsNumber(); }
	void AsArgumentType(FString                         & Value) { Value = AsString(); }
	void AsArgumentType(bool                            & Value) { Value = AsBool  (); }
	void AsArgumentType(TArray< TSharedPtr<FJsonValue> >& Value) { Value = AsArray (); }
	void AsArgumentType(TSharedPtr<FJsonObject>         & Value) { Value = AsObject(); }

	EJson Type;

	static bool CompareEqual(const FJsonValue& Lhs, const FJsonValue& Rhs);
	bool operator==(const FJsonValue& Rhs) const { return CompareEqual(*this, Rhs); }

protected:

	static const TArray< TSharedPtr<FJsonValue> > EMPTY_ARRAY;
	static const TSharedPtr<FJsonObject> EMPTY_OBJECT;

	FJsonValue() : Type(EJson::None) {}
	virtual ~FJsonValue() {}

	virtual FString GetType() const = 0;

	void ErrorMessage(const FString& InType) const;
};


/** A Json String Value. */
class JSON_API FJsonValueString : public FJsonValue
{
public:
	FJsonValueString(const FString& InString) : Value(InString) {Type = EJson::String;}

	virtual bool TryGetString(FString& OutString) const override	{ OutString = Value; return true; }
	virtual bool TryGetNumber(double& OutDouble) const override		{ if(Value.IsNumeric()){ OutDouble = FCString::Atod(*Value); return true; } else { return false; } }
	virtual bool TryGetBool(bool& OutBool) const override			{ OutBool = Value.ToBool(); return true; }

protected:
	FString Value;

	virtual FString GetType() const override {return TEXT("String");}
};


/** A Json Number Value. */
class JSON_API FJsonValueNumber : public FJsonValue
{
public:
	FJsonValueNumber(double InNumber) : Value(InNumber) {Type = EJson::Number;}
	virtual bool TryGetNumber(double& OutNumber) const override		{ OutNumber = Value; return true; }
	virtual bool TryGetBool(bool& OutBool) const override			{ OutBool = (Value != 0.0); return true; }
	virtual bool TryGetString(FString& OutString) const override	{ OutString = FString::SanitizeFloat(Value); return true; }
	
protected:
	double Value;

	virtual FString GetType() const override {return TEXT("Number");}
};


/** A Json Boolean Value. */
class JSON_API FJsonValueBoolean : public FJsonValue
{
public:
	FJsonValueBoolean(bool InBool) : Value(InBool) {Type = EJson::Boolean;}
	virtual bool TryGetNumber(double& OutNumber) const override		{ OutNumber = Value ? 1 : 0; return true; }
	virtual bool TryGetBool(bool& OutBool) const override			{ OutBool = Value; return true; }
	virtual bool TryGetString(FString& OutString) const override	{ OutString = Value ? TEXT("true") : TEXT("false"); return true; }
	
protected:
	bool Value;

	virtual FString GetType() const override {return TEXT("Boolean");}
};


/** A Json Array Value. */
class JSON_API FJsonValueArray : public FJsonValue
{
public:
	FJsonValueArray(const TArray< TSharedPtr<FJsonValue> >& InArray) : Value(InArray) {Type = EJson::Array;}
	virtual bool TryGetArray(const TArray< TSharedPtr<FJsonValue> >*& OutArray) const override	{ OutArray = &Value; return true; }
	
protected:
	TArray< TSharedPtr<FJsonValue> > Value;

	virtual FString GetType() const override {return TEXT("Array");}
};


/** A Json Object Value. */
class JSON_API FJsonValueObject : public FJsonValue
{
public:
	FJsonValueObject(TSharedPtr<FJsonObject> InObject) : Value(InObject) {Type = EJson::Object;}
	virtual bool TryGetObject(const TSharedPtr<FJsonObject>*& OutObject) const override			{ OutObject = &Value; return true; }
	
protected:
	TSharedPtr<FJsonObject> Value;

	virtual FString GetType() const override {return TEXT("Object");}
};


/** A Json Null Value. */
class JSON_API FJsonValueNull : public FJsonValue
{
public:
	FJsonValueNull() {Type = EJson::Null;}

protected:
	virtual FString GetType() const override {return TEXT("Null");}
};
