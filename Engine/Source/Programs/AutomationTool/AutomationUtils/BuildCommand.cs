// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace AutomationTool
{
	/// <summary>
	/// Base class for buildcommands.
	/// </summary>
	public abstract class BuildCommand : CommandUtils
	{
		/// <summary>
		/// Command line parameters for this command (empty by non-null by default)
		/// </summary>
		private string[] CommandLineParams = new string[0];
		public string[] Params
		{
			get { return CommandLineParams; }
			set { CommandLineParams = value; }
		}

		/// <summary>
		/// Parses the command's Params list for a parameter and returns whether it is defined or not.
		/// </summary>
		/// <param name="Param">Param to check for.</param>
		/// <returns>True if param was found, false otherwise.</returns>
		public bool ParseParam(string Param)
		{
			return ParseParam(Params, Param);
		}

		/// <summary>
		/// Parses the command's Params list for a parameter and reads its value. 
		/// Ex. ParseParamValue(Args, "map=")
		/// </summary>
		/// <param name="Param">Param to read its value.</param>
		/// <returns>Returns the value or Default if the parameter was not found.</returns>
		public string ParseParamValue(string Param, string Default = null)
		{
			return ParseParamValue(Params, Param, Default);
		}

		/// <summary>
		/// Parses the argument list for any number of parameters.
		/// </summary>
		/// <param name="ArgList">Argument list.</param>
		/// <param name="Param">Param to read its value.</param>
		/// <returns>Returns an array of values for this parameter (or an empty array if one was not found.</returns>
		public string[] ParseParamValues(string Param)
		{
			return ParseParamValues(Params, Param);
		}

		/// <summary>
		/// Parses the command's Params list for a parameter and reads its value. 
		/// Ex. ParseParamValue(Args, "map=")
		/// </summary>
		/// <param name="Param">Param to read its value.</param>
		/// <returns>Returns the value or Default if the parameter was not found.</returns>
		public int ParseParamInt(string Param, int Default = 0)
		{
			string num = ParseParamValue(Params, Param, Default.ToString());
			return int.Parse(num);
		}

		/// <summary>
		/// Parses the command's Params list for a parameter and reads its value. 
		/// Ex. ParseParamValue(Args, "map=")
		/// </summary>
		/// <param name="Param">Param to read its value.</param>
		/// <returns>Returns the value or Default if the parameter was not found.</returns>
		public int? ParseParamNullableInt(string Param)
		{
			string Value = ParseParamValue(Params, Param, null);
			if(Value == null)
			{
				return null;
			}
			else
			{
				return int.Parse(Value);
			}
		}

		/// <summary>
		/// Build command entry point.  Throws AutomationExceptions on failure.
		/// </summary>
		public virtual void ExecuteBuild()
		{
			throw new AutomationException("Either Execute() or ExecuteBuild() should be implemented for {0}", GetType().Name);
		}

		/// <summary>
		/// Command entry point.
		/// </summary>
		public virtual ExitCode Execute()
		{
			ExecuteBuild();
			return ExitCode.Success;
		}

		/// <summary>
		/// Executes a new command as a child of another command.
		/// </summary>
		/// <typeparam name="T"></typeparam>
		/// <param name="ParentCommand"></param>
		public static ExitCode Execute<T>(BuildCommand ParentCommand) where T : BuildCommand, new()
		{
			T Command = new T();
			if (ParentCommand != null)
			{
				Command.Params = ParentCommand.Params;
			}
			return Command.Execute();
		}
	}
}