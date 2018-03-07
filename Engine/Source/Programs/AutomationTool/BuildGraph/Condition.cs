// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace AutomationTool
{
	/// <summary>
	/// Exception class thrown due to type and syntax errors in condition expressions
	/// </summary>
	class ConditionException : Exception
	{
		/// <summary>
		/// Constructor; formats the exception message with the given String.Format() style parameters.
		/// </summary>
		/// <param name="Format">Formatting string, in String.Format syntax</param>
		/// <param name="Args">Optional arguments for the string</param>
		public ConditionException(string Format, params object[] Args) : base(String.Format(Format, Args))
		{
		}
	}

	/// <summary>
	/// Class to evaluate condition expressions in build scripts, following this grammar:
	/// 
	///		or-expression   ::= and-expression
	///		                  | or-expression "Or" and-expression;
	///		    
	///		and-expression  ::= comparison
	///		                  | and-expression "And" comparison;
	///		                  
	///		comparison      ::= scalar
	///		                  | scalar "==" scalar
	///		                  | scalar "!=" scalar
	///		                  | scalar "&lt;" scalar
	///		                  | scalar "&lt;=" scalar;
	///		                  | scalar "&gt;" scalar
	///		                  | scalar "&gt;=" scalar;
	///		                  
	///     scalar          ::= "(" or-expression ")"
	///                       | "!" scalar
	///                       | "Exists" "(" scalar ")"
	///                       | "HasTrailingSlash" "(" scalar ")"
	///                       | string
	///                       | identifier;
	///                       
	///     string          ::= any sequence of characters terminated by single quotes (') or double quotes ("). Not escaped.
	///     identifier      ::= any sequence of letters, digits, or underscore characters.
	///     
	/// The type of each subexpression is always a scalar, which are converted to expression-specific types (eg. booleans, integers) as required.
	/// Scalar values are case-insensitive strings. The identifier 'true' and the strings "true" and "True" are all identical scalars.
	/// </summary>
	static class Condition
	{
		/// <summary>
		/// Sentinel added to the end of a sequence of tokens.
		/// </summary>
		const string EndToken = "<EOF>";

		/// <summary>
		/// Evaluates the given string as a condition. Throws a ConditionException on a type or syntax error.
		/// </summary>
		/// <param name="Text">The condition text</param>
		/// <returns>The result of evaluating the condition</returns>
		public static bool Evaluate(string Text)
		{
			List<string> Tokens = new List<string>();
			Tokenize(Text, Tokens);

			bool bResult = true;
			if(Tokens.Count > 1)
			{
				int Idx = 0;
				string Result = EvaluateOr(Tokens, ref Idx);
				if(Tokens[Idx] != EndToken)
				{
					throw new ConditionException("Garbage after expression: {0}", String.Join("", Tokens.Skip(Idx)));
				}
				bResult = CoerceToBool(Result);
			}
			return bResult;
		}

		/// <summary>
		/// Evaluates an "or-expression" production.
		/// </summary>
		/// <param name="Tokens">List of tokens in the expression</param>
		/// <param name="Idx">Current position in the token stream. Will be incremented as tokens are consumed.</param>
		/// <returns>A scalar representing the result of evaluating the expression.</returns>
		static string EvaluateOr(List<string> Tokens, ref int Idx)
		{
			// <Condition> Or <Condition> Or...
			string Result = EvaluateAnd(Tokens, ref Idx);
			while(String.Compare(Tokens[Idx], "Or", true) == 0)
			{
				// Evaluate this condition. We use a binary OR here, because we want to parse everything rather than short-circuit it.
				Idx++;
				string Lhs = Result;
				string Rhs = EvaluateAnd(Tokens, ref Idx);
				Result = (CoerceToBool(Lhs) | CoerceToBool(Rhs))? "true" : "false";
			}
			return Result;
		}

		/// <summary>
		/// Evaluates an "and-expression" production.
		/// </summary>
		/// <param name="Tokens">List of tokens in the expression</param>
		/// <param name="Idx">Current position in the token stream. Will be incremented as tokens are consumed.</param>
		/// <returns>A scalar representing the result of evaluating the expression.</returns>
		static string EvaluateAnd(List<string> Tokens, ref int Idx)
		{
			// <Condition> And <Condition> And...
			string Result = EvaluateComparison(Tokens, ref Idx);
			while(String.Compare(Tokens[Idx], "And", true) == 0)
			{
				// Evaluate this condition. We use a binary AND here, because we want to parse everything rather than short-circuit it.
				Idx++;
				string Lhs = Result;
				string Rhs = EvaluateComparison(Tokens, ref Idx);
				Result = (CoerceToBool(Lhs) & CoerceToBool(Rhs))? "true" : "false";
			}
			return Result;
		}

		/// <summary>
		/// Evaluates a "comparison" production.
		/// </summary>
		/// <param name="Tokens">List of tokens in the expression</param>
		/// <param name="Idx">Current position in the token stream. Will be incremented as tokens are consumed.</param>
		/// <returns>The result of evaluating the expression</returns>
		static string EvaluateComparison(List<string> Tokens, ref int Idx)
		{
			// scalar
			// scalar == scalar
			// scalar != scalar
			// scalar < scalar
			// scalar <= scalar
			// scalar > scalar
			// scalar >= scalar

			string Result = EvaluateScalar(Tokens, ref Idx);
			if(Tokens[Idx] == "==")
			{
				// Compare two scalars for equality
				Idx++;
				string Lhs = Result;
				string Rhs = EvaluateScalar(Tokens, ref Idx);
				Result = (String.Compare(Lhs, Rhs, true) == 0)? "true" : "false";
			}
			else if(Tokens[Idx] == "!=")
			{
				// Compare two scalars for inequality
				Idx++;
				string Lhs = Result;
				string Rhs = EvaluateScalar(Tokens, ref Idx);
				Result = (String.Compare(Lhs, Rhs, true) != 0)? "true" : "false";
			}
			else if(Tokens[Idx] == "<")
			{
				// Compares whether the first integer is less than the second
				Idx++;
				int Lhs = CoerceToInteger(Result);
				int Rhs = CoerceToInteger(EvaluateScalar(Tokens, ref Idx));
				Result = (Lhs < Rhs)? "true" : "false";
			}
			else if(Tokens[Idx] == "<=")
			{
				// Compares whether the first integer is less than the second
				Idx++;
				int Lhs = CoerceToInteger(Result);
				int Rhs = CoerceToInteger(EvaluateScalar(Tokens, ref Idx));
				Result = (Lhs <= Rhs)? "true" : "false";
			}
			else if(Tokens[Idx] == ">")
			{
				// Compares whether the first integer is less than the second
				Idx++;
				int Lhs = CoerceToInteger(Result);
				int Rhs = CoerceToInteger(EvaluateScalar(Tokens, ref Idx));
				Result = (Lhs > Rhs)? "true" : "false";
			}
			else if(Tokens[Idx] == ">=")
			{
				// Compares whether the first integer is less than the second
				Idx++;
				int Lhs = CoerceToInteger(Result);
				int Rhs = CoerceToInteger(EvaluateScalar(Tokens, ref Idx));
				Result = (Lhs >= Rhs)? "true" : "false";
			}
			return Result;
		}

		/// <summary>
		/// Evaluates a "scalar" production.
		/// </summary>
		/// <param name="Tokens">List of tokens in the expression</param>
		/// <param name="Idx">Current position in the token stream. Will be incremented as tokens are consumed.</param>
		/// <returns>The result of evaluating the expression</returns>
		static string EvaluateScalar(List<string> Tokens, ref int Idx)
		{
			string Result;
			if(Tokens[Idx] == "(")
			{
				// Subexpression
				Idx++;
				Result = EvaluateOr(Tokens, ref Idx);
				if(Tokens[Idx] != ")")
				{
					throw new ConditionException("Expected ')'");
				}
				Idx++;
			}
			else if(Tokens[Idx] == "!")
			{
				// Logical not
				Idx++;
				string Rhs = EvaluateScalar(Tokens, ref Idx);
				Result = CoerceToBool(Rhs)? "false" : "true";
			}
			else if(String.Compare(Tokens[Idx], "Exists", true) == 0 && Tokens[Idx + 1] == "(")
			{
				// Check whether file or directory exists. Evaluate the argument as a subexpression.
				Idx++;
				string Argument = EvaluateScalar(Tokens, ref Idx);
				Result = (File.Exists(Argument) || Directory.Exists(Argument))? "true" : "false";
			}
			else if(String.Compare(Tokens[Idx], "HasTrailingSlash", true) == 0 && Tokens[Idx + 1] == "(")
			{
				// Check whether the given string ends with a slash
				Idx++;
				string Argument = EvaluateScalar(Tokens, ref Idx);
				Result = (Argument.Length > 0 && (Argument[Argument.Length - 1] == Path.DirectorySeparatorChar || Argument[Argument.Length - 1] == Path.AltDirectorySeparatorChar))? "true" : "false";
			}
			else
			{
				// Raw scalar. Remove quotes from strings, and allow literals and simple identifiers to pass through directly.
				string Token = Tokens[Idx];
				if(Token.Length >= 2 && (Token[0] == '\'' || Token[0] == '\"') && Token[Token.Length - 1] == Token[0])
				{
					Result = Token.Substring(1, Token.Length - 2);
					Idx++;
				}
				else if(Char.IsLetterOrDigit(Token[0]) || Token[0] == '_')
				{
					Result = Token;
					Idx++;
				}
				else
				{
					throw new ConditionException("Token '{0}' is not a valid scalar", Token);
				}
			}
			return Result;
		}

		/// <summary>
		/// Converts a scalar to a boolean value.
		/// </summary>
		/// <param name="Scalar">The scalar to convert</param>
		/// <returns>The scalar converted to a boolean value.</returns>
		static bool CoerceToBool(string Scalar)
		{
			bool Result;
			if(String.Compare(Scalar, "true", true) == 0)
			{
				Result = true;
			}
			else if(String.Compare(Scalar, "false", true) == 0)
			{
				Result = false;
			}
			else
			{
				throw new ConditionException("Token '{0}' cannot be coerced to a bool", Scalar);
			}
			return Result;
		}

		/// <summary>
		/// Converts a scalar to a boolean value.
		/// </summary>
		/// <param name="Scalar">The scalar to convert</param>
		/// <returns>The scalar converted to an integer value.</returns>
		static int CoerceToInteger(string Scalar)
		{
			int Value;
			if(!Int32.TryParse(Scalar, out Value))
			{
				throw new ConditionException("Token '{0}' cannot be coerced to an integer", Scalar);
			}
			return Value;
		}

		/// <summary>
		/// Splits an input string up into expression tokens.
		/// </summary>
		/// <param name="Text">Text to be converted into tokens</param>
		/// <param name="Tokens">List to receive a list of tokens</param>
		static void Tokenize(string Text, List<string> Tokens)
		{
			int Idx = 0;
			while(Idx < Text.Length)
			{
				int EndIdx = Idx + 1;
				if(!Char.IsWhiteSpace(Text[Idx]))
				{
					// Scan to the end of the current token
					if(Char.IsNumber(Text[Idx]))
					{
						// Number
						while(EndIdx < Text.Length && Char.IsNumber(Text[EndIdx]))
						{
							EndIdx++;
						}
					}
					else if(Char.IsLetter(Text[Idx]) || Text[Idx] == '_')
					{
						// Identifier
						while(EndIdx < Text.Length && (Char.IsLetterOrDigit(Text[EndIdx]) || Text[EndIdx] == '_'))
						{
							EndIdx++;
						}
					}
					else if(Text[Idx] == '!' || Text[Idx] == '<' || Text[Idx] == '>' || Text[Idx] == '=')
					{
						// Operator that can be followed by an equals character
						if(EndIdx < Text.Length && Text[EndIdx] == '=')
						{
							EndIdx++;
						}
					}
					else if(Text[Idx] == '\'' || Text[Idx] == '\"')
					{
						// String
						if(EndIdx < Text.Length)
						{
							EndIdx++;
							while(EndIdx < Text.Length && Text[EndIdx - 1] != Text[Idx]) EndIdx++;
						}
					}
					Tokens.Add(Text.Substring(Idx, EndIdx - Idx));
				}
				Idx = EndIdx;
			}
			Tokens.Add(EndToken);
		}

		/// <summary>
		/// Test cases for conditions.
		/// </summary>
		public static void TestConditions()
		{
			TestCondition("1 == 2", false);
			TestCondition("1 == 1", true);
			TestCondition("1 != 2", true);
			TestCondition("1 != 1", false);
			TestCondition("'hello' == 'hello'", true);
			TestCondition("'hello' == ('hello')", true);
			TestCondition("'hello' == 'world'", false);
			TestCondition("'hello' != ('world')", true);
			TestCondition("true == ('true')", true);
			TestCondition("true == ('True')", true);
			TestCondition("true == ('false')", false);
			TestCondition("true == !('False')", true);
			TestCondition("true == 'true' and 'false' == 'False'", true);
			TestCondition("true == 'true' and 'false' == 'true'", false);
			TestCondition("true == 'false' or 'false' == 'false'", true);
			TestCondition("true == 'false' or 'false' == 'true'", true);
		}

		/// <summary>
		/// Helper method to evaluate a condition and check it's the expected result
		/// </summary>
		/// <param name="Condition">Condition to evaluate</param>
		/// <param name="ExpectedResult">The expected result</param>
		static void TestCondition(string Condition, bool ExpectedResult)
		{
			bool Result = Evaluate(Condition);
			Console.WriteLine("{0}: {1} = {2}", (Result == ExpectedResult)? "PASS" : "FAIL", Condition, Result);
		}
	}
}
