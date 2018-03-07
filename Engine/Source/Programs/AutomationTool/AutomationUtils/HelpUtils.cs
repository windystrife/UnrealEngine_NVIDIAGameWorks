// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Runtime.CompilerServices;
using System.Runtime.Serialization;
using System.Runtime.Serialization.Json;
using System.Text;
using System.Threading;

using System.ComponentModel;
using System.Reflection;

namespace AutomationTool
{
	#region Help

	/// <summary>
	/// Help Attribute.
	/// </summary>
	[AttributeUsage(AttributeTargets.All, AllowMultiple = true)]
	public class HelpAttribute : DescriptionAttribute
	{
		private bool IsParamHelp;

		/// <summary>
		/// Basic constructor for class descriptions.
		/// </summary>
		/// <param name="Description">Class description</param>
		public HelpAttribute(string Description)
			: base(Description)
		{
			IsParamHelp = false;
		}

		/// <summary>
		/// Constructor for parameter descriptions.
		/// </summary>
		/// <param name="ParamName">Paramter name</param>
		/// <param name="Description">Paramter description</param>
		public HelpAttribute(string ParamName, string Description)
			: base(FormatDescription(ParamName, Description))
		{
			IsParamHelp = true;
		}

		/// <summary>
		/// Additional type to display help for.
		/// </summary>
		/// <param name="AdditionalType"></param>
		public HelpAttribute(Type AdditionalType)
		{
			IsParamHelp = false;
			AdditionalHelp = AdditionalType;
		}

		private static string FormatDescription(string Name, string Description)
		{
			return String.Format("-{0} {1}", Name, Description);
		}

		public bool IsParam
		{
			get { return IsParamHelp; }
		}

		public Type AdditionalHelp
		{
			get;
			private set;
		}
	}

	#endregion

	/// <summary>
	/// Base utility function for script commands.
	/// </summary>
	public partial class CommandUtils
	{
		#region Help

		/// <summary>
		/// Displays help for the specified command.
		/// <param name="Command">Command type.</param>
		/// </summary>
		public static void Help(Type Command)
		{
			string Description;
			List<string> Params;
			GetTypeHelp(Command, out Description, out Params);
			LogHelp(Command, Description, Params);
		}

		/// <summary>
		/// Displays help for the specified Type.
		/// </summary>
		/// <param name="Command">Type to display help for.</param>
		public static void LogHelp(Type Command)
		{
			string Description;
			List<string> Params;
			GetTypeHelp(Command, out Description, out Params);
			LogHelp(Command, Description, Params);
		}

		/// <summary>
		/// Displays a formatted help for the specified Command type.
		/// </summary>
		/// <param name="Command">Command Type</param>
		/// <param name="Description">Command Desscription</param>
		/// <param name="Params">Command Parameters</param>
		public static void LogHelp(Type Command, string Description, List<string> Params)
		{
			string HelpMessage = Environment.NewLine;

			bool bFirstLine = true;
			if (Command != null)
			{
				HelpMessage += String.Format("{0} Help:{1}", Command.Name, Environment.NewLine);
				bFirstLine = false;
			}

			if (String.IsNullOrEmpty(Description) == false)
			{
				if (!bFirstLine)
				{
					HelpMessage += Environment.NewLine;
				}
				HelpMessage += Description + Environment.NewLine;
				bFirstLine = false;
			}
			
			if (IsNullOrEmpty(Params) == false)
			{
				if (!bFirstLine)
				{
					HelpMessage += Environment.NewLine;
				}
				HelpMessage += "Parameters: " + Environment.NewLine;
				HelpMessage += Environment.NewLine;

				foreach(string Line in FormatParams(Params, 4, 24))
				{
					HelpMessage += Line + Environment.NewLine;
				}
			}

			Log(HelpMessage);
		}

		/// <summary>
		/// Formats the given parameters as so:
		///     -Param1     Param1 Description
		///
		///     -Param2      Param2 Description, this description is
		///                  longer and splits onto a separate line. 
		///
		///     -Param3      Param3 Description continues as before. 
		/// </summary>
		/// <param name="Params">List of parameters arranged as "-ParamName Param Description"</param>
		/// <param name="Indent">Indent from the left hand side</param>
		/// <param name="DefaultRightPadding">The minimum padding from the start of the param name to the start of the description (resizes with larger param names)</param>
		/// <returns></returns>
		public static IEnumerable<string> FormatParams(IEnumerable<string> Params, int Indent, int DefaultRightPadding)
		{
			Dictionary<string, string> ParamDict = new Dictionary<string, string>(StringComparer.InvariantCultureIgnoreCase);

			// Extract Params/Descriptions into Key/Value pairs
			foreach (var Param in Params)
			{
				// Find the first space (should be following the param name)
				if (!String.IsNullOrWhiteSpace(Param))
				{
					var ParamName = String.Empty;
					var ParamDesc = String.Empty;

					var SplitPoint = Param.IndexOf(' ');
					if (SplitPoint > 0)
					{
						// Extract the name and description seperately
						ParamName = Param.Substring(0, SplitPoint);
						ParamDesc = Param.Substring(SplitPoint + 1, Param.Length - (SplitPoint + 1));
					}
					else
					{
						ParamName = Param;
					}

					// build dictionary using Name and Desc as Key and Value
					if (!ParamDict.ContainsKey(ParamName))
					{
						ParamDict.Add(ParamName, ParamDesc);
					}
					else
					{
						LogWarning("Duplicated help parameter \"{0}\"", ParamName);
					}
				}
			}

			// string used to intent the param
			string IndentString = string.Empty.PadRight(Indent);

			// default the padding value
			int RightPadding = DefaultRightPadding;

			// If (Padding < longest param name) padding = longest param name + 1
			foreach (var ParamName in ParamDict.Keys)
			{
				if (ParamName.Length + 1 > RightPadding)
				{
					RightPadding = ParamName.Length + 1;
				}
			}

			// Get the window width, using a default value if there's no console attached to this process.
			int WindowWidth;
			try
			{
				WindowWidth = Console.WindowWidth;
			}
			catch
			{
				WindowWidth = 240;
			}

			// Build the formatted params
			foreach (var ParamName in ParamDict.Keys)
			{
				// build the param first, including intend and padding on the rights size
				string ParamString = IndentString + ParamName.PadRight(RightPadding);

				// Build the description line by line, adding the same amount of intending each time. 
				foreach (var DescriptionLine in WordWrap(ParamDict[ParamName], WindowWidth - ParamString.Length))
				{
					// Formatting as following:
					// <Indent>-param<Right Padding>Description<New line>
					yield return ParamString + DescriptionLine;

					// we replace the param string on subsequent lines with white space of the same length
					ParamString = string.Empty.PadRight(IndentString.Length + RightPadding);
				}
			}
		}

		/// <summary>
		/// Takes a given sentence and wraps it on a word by word basis so that no line exceeds the
		/// set maximum line length. Words longer than a line are broken up. Returns the sentence as
		/// a list of individual lines.
		/// </summary>
		/// <param name="sentence">The sentence to be wrapped</param>
		/// <param name="MaximumLineLength">The maximum (non negative) length of the returned sentences</param>
		/// <returns>The provided sentence in a list of individual lines no longer than MaximumLineLength</returns>
		static List<string> WordWrap(string Sentence, int MaximumLineLength)
		{
			// Early out
			if (Sentence.Length == 0)
			{
				return new List<string>();
			}

			string[] words = Sentence.Split(' ');
			List<string> WrappedWords = new List<string>();

			string CurrentSentence = string.Empty;
			foreach (var word in words)
			{
				// if this is a very large word, split it
				if (word.Length > MaximumLineLength)
				{
					// Top up the current line
					WrappedWords.Add(CurrentSentence + word.Substring(0, MaximumLineLength - CurrentSentence.Length));

					int length = MaximumLineLength - CurrentSentence.Length;
					while (length + MaximumLineLength < word.Length)
					{
						// Place the starting lengths into their own lines
						WrappedWords.Add(word.Substring(length, Math.Min(MaximumLineLength, word.Length - length)));
						length += MaximumLineLength;
					}

					// then the trailing end into the next line
					CurrentSentence += word.Substring(length, Math.Min(MaximumLineLength, word.Length - length)) + " ";
				}
				else
				{
					if (CurrentSentence.Length + word.Length > MaximumLineLength)
					{
						// next line and reset sentence
						WrappedWords.Add(CurrentSentence);
						CurrentSentence = string.Empty;
					}

					// Add the word to the current sentence.
					CurrentSentence += word + " ";
				}
			}

			if (CurrentSentence.Length > 0)
			{
				WrappedWords.Add(CurrentSentence);
			}

			return WrappedWords;
		}

		/// <summary>
		/// Gets the description and a list of parameters for the specified type.
		/// </summary>
		/// <param name="ObjType">Type to get help for.</param>
		/// <param name="ObjectDescription">Description</param>
		/// <param name="Params">List of paramters</param>
		public static void GetTypeHelp(Type ObjType, out string ObjectDescription, out List<string> Params)
		{
			ObjectDescription = String.Empty;
			Params = new List<string>();

			var AllAttributes = ObjType.GetCustomAttributes(false);			
			foreach (var CustomAttribute in AllAttributes)
			{
				var HelpAttribute = CustomAttribute as HelpAttribute;
				if (HelpAttribute != null)
				{
					if (HelpAttribute.IsParam)
					{
						Params.Add(HelpAttribute.Description);
					}
					else if (HelpAttribute.AdditionalHelp != null)
					{
						string DummyDescription;
						List<string> AdditionalParams;
						GetTypeHelp(HelpAttribute.AdditionalHelp, out DummyDescription, out AdditionalParams);
						Params.AddRange(AdditionalParams);
					}
					else
					{
						ObjectDescription = HelpAttribute.Description;
					}
				}
			}

			var AllMembers = GetAllMemebers(ObjType);
			foreach (var Member in AllMembers)
			{
				var AllMemeberAtributes = Member.GetCustomAttributes(false);
				foreach (var CustomAttribute in AllMemeberAtributes)
				{
					var HelpAttribute = CustomAttribute as HelpAttribute;
					if (HelpAttribute != null && HelpAttribute.IsParam)
					{
						Params.Add(HelpAttribute.Description);
					}
				}
			}
		}

		private static List<MemberInfo> GetAllMemebers(Type ObjType)
		{
			var Members = new List<MemberInfo>();
			for (; ObjType != null; ObjType = ObjType.BaseType)
			{
				var ObjMembers = ObjType.GetMembers(BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static);
				Members.AddRange(ObjMembers);
			}
			return Members;
		}

		#endregion
	}
}
