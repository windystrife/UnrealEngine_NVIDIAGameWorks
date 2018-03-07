// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using Tools.DotNETCommon;
using UnrealBuildTool;

namespace AutomationTool
{
	/// <summary>
	/// Encapsulates a pattern containing the '?', '*', and '...' wildcards.
	/// </summary>
	public class FilePattern
	{
		/// <summary>
		/// Base directory for all matched files
		/// </summary>
		public readonly DirectoryReference BaseDirectory;
		
		/// <summary>
		/// List of tokens in the pattern. Every second token is a wildcard, other tokens are string fragments. Always has an odd number of elements. Path separators are normalized to the host platform format.
		/// </summary>
		public readonly List<string> Tokens = new List<string>();

		/// <summary>
		/// Constructs a file pattern which matches a single file
		/// </summary>
		/// <param name="File">Location of the file</param>
		public FilePattern(FileReference File)
		{
			BaseDirectory = File.Directory;
			Tokens.Add(File.GetFileName());
		}

		/// <summary>
		/// Constructs a file pattern from the given string, resolving relative paths to the given directory. 
		/// </summary>
		/// <param name="RootDirectory">If a relative path is specified by the pattern, the root directory used to turn it into an absolute path</param>
		/// <param name="Pattern">The pattern to match. If the pattern ends with a directory separator, an implicit '...' is appended.</param>
		public FilePattern(DirectoryReference RootDirectory, string Pattern)
		{
			// Normalize the path separators
			StringBuilder Text = new StringBuilder(Pattern);
			if(Path.DirectorySeparatorChar != '\\')
			{
				Text.Replace('\\', Path.DirectorySeparatorChar);
			}
			if(Path.DirectorySeparatorChar != '/')
			{
				Text.Replace('/', Path.DirectorySeparatorChar);
			}

			// Find the base directory, stopping when we hit a wildcard. The source directory must end with a path specification.
			int BaseDirectoryLen = 0;
			for(int Idx = 0; Idx < Text.Length; Idx++)
			{
				if(Text[Idx] == Path.DirectorySeparatorChar)
				{
					BaseDirectoryLen = Idx + 1;
				}
				else if(Text[Idx] == '?' || Text[Idx] == '*' || (Idx + 2 < Text.Length && Text[Idx] == '.' && Text[Idx + 1] == '.' && Text[Idx + 2] == '.'))
				{
					break;
				}
			}

			// Extract the base directory
			BaseDirectory = DirectoryReference.Combine(RootDirectory, Text.ToString(0, BaseDirectoryLen));

			// Convert any directory wildcards ("...") into complete directory wildcards ("\\...\\"). We internally treat use "...\\" as the wildcard 
			// token so we can correctly match zero directories. Patterns such as "foo...bar" should require at least one directory separator, so 
			// should be converted to "foo*\\...\\*bar".
			for(int Idx = BaseDirectoryLen; Idx < Text.Length; Idx++)
			{
				if(Text[Idx] == '.' && Text[Idx + 1] == '.' && Text[Idx + 2] == '.')
				{
					// Insert a directory separator before
					if(Idx > BaseDirectoryLen && Text[Idx - 1] != Path.DirectorySeparatorChar)
					{
						Text.Insert(Idx++, '*');
						Text.Insert(Idx++, Path.DirectorySeparatorChar);
					}

					// Skip past the ellipsis
					Idx += 3;

					// Insert a directory separator after
					if(Idx == Text.Length || Text[Idx] != Path.DirectorySeparatorChar)
					{
						Text.Insert(Idx++, Path.DirectorySeparatorChar);
						Text.Insert(Idx++, '*');
					}
				}
			}

			// Parse the tokens
			int LastIdx = BaseDirectoryLen;
			for(int Idx = BaseDirectoryLen; Idx < Text.Length; Idx++)
			{
				if(Text[Idx] == '?' || Text[Idx] == '*')
				{
					Tokens.Add(Text.ToString(LastIdx, Idx - LastIdx));
					Tokens.Add(Text.ToString(Idx, 1));
					LastIdx = Idx + 1;
				}
				else if(Idx - 3 >= BaseDirectoryLen && Text[Idx] == Path.DirectorySeparatorChar && Text[Idx - 1] == '.' && Text[Idx - 2] == '.' && Text[Idx - 3] == '.')
				{
					Tokens.Add(Text.ToString(LastIdx, Idx - 3 - LastIdx));
					Tokens.Add(Text.ToString(Idx - 3, 4));
					LastIdx = Idx + 1;
				}
			}
			Tokens.Add(Text.ToString(LastIdx, Text.Length - LastIdx));
		}

		/// <summary>
		/// A pattern without wildcards may match either a single file or directory based on context. This pattern resolves to the later as necessary, producing a new pattern.
		/// </summary>
		/// <returns>Pattern which matches a directory</returns>
		public FilePattern AsDirectoryPattern()
		{
			if(ContainsWildcards())
			{
				return this;
			}
			else
			{
				StringBuilder Pattern = new StringBuilder();
				foreach(string Token in Tokens)
				{
					Pattern.Append(Token);
				}
				if(Pattern.Length > 0)
				{
					Pattern.Append(Path.DirectorySeparatorChar);
				}
				Pattern.Append("...");

				return new FilePattern(BaseDirectory, Pattern.ToString());
			}
		}

		/// <summary>
		/// For a pattern that does not contain wildcards, returns the single file location
		/// </summary>
		/// <returns>Location of the referenced file</returns>
		public FileReference GetSingleFile()
		{
			if(Tokens.Count == 1)
			{
				return FileReference.Combine(BaseDirectory, Tokens[0]);
			}
			else
			{
				throw new InvalidOperationException("File pattern does not reference a single file");
			}
		}

		/// <summary>
		/// Checks whether this pattern is explicitly a directory, ie. is terminated with a directory separator
		/// </summary>
		/// <returns>True if the pattern is a directory</returns>
		public bool EndsWithDirectorySeparator()
		{
			string LastToken = Tokens[Tokens.Count - 1];
			return LastToken.Length > 0 && LastToken[LastToken.Length - 1] == Path.DirectorySeparatorChar;
		}

		/// <summary>
		/// Determines whether the pattern contains wildcards
		/// </summary>
		/// <returns>True if the pattern contains wildcards, false otherwise.</returns>
		public bool ContainsWildcards()
		{
			return Tokens.Count > 1;
		}

		/// <summary>
		/// Tests whether a pattern is compatible with another pattern (that is, that the number and type of wildcards match)
		/// </summary>
		/// <param name="Other">Pattern to compare against</param>
		/// <returns>Whether the patterns are compatible.</returns>
		public bool IsCompatibleWith(FilePattern Other)
		{
			// Check there are the same number of tokens in each pattern
			if(Tokens.Count != Other.Tokens.Count)
			{
				return false;
			}

			// Check all the wildcard tokens match
			for(int Idx = 1; Idx < Tokens.Count; Idx += 2)
			{
				if(Tokens[Idx] != Other.Tokens[Idx])
				{
					return false;
				}
			}
			return true;
		}

		/// <summary>
		/// Converts this pattern to a C# regex format string, which matches paths relative to the base directory formatted with native directory separators
		/// </summary>
		/// <returns>The regex pattern</returns>
		public string GetRegexPattern()
		{
			StringBuilder Pattern = new StringBuilder("^");
			Pattern.Append(Regex.Escape(Tokens[0]));
			for(int Idx = 1; Idx < Tokens.Count; Idx += 2)
			{
				// Append the wildcard expression
				if(Tokens[Idx] == "?")
				{
					Pattern.Append("([^\\/])");
				}
				else if(Tokens[Idx] == "*")
				{
					Pattern.Append("([^\\/]*)");
				}
				else
				{
					Pattern.AppendFormat("((?:.+{0})?)", Regex.Escape(Path.DirectorySeparatorChar.ToString()));
				}

				// Append the next sequence of characters to match
				Pattern.Append(Regex.Escape(Tokens[Idx + 1]));
			}
			Pattern.Append("$");
			return Pattern.ToString();
		}

		/// <summary>
		/// Creates a regex replacement pattern
		/// </summary>
		/// <returns>String representing the regex replacement pattern</returns>
		public string GetRegexReplacementPattern()
		{
			StringBuilder Pattern = new StringBuilder();
			for(int Idx = 0;;Idx += 2)
			{
				// Append the escaped replacement character
				Pattern.Append(Tokens[Idx].Replace("$", "$$"));

				// Check if we've reached the end of the string
				if(Idx == Tokens.Count - 1)
				{
					break;
				}

				// Insert the capture
				Pattern.AppendFormat("${0}", (Idx / 2) + 1);
			}
			return Pattern.ToString();
		}

		/// <summary>
		/// Creates a file mapping between a set of source patterns and a target pattern. All patterns should have a matching order and number of wildcards.
		/// </summary>
		/// <param name="Files">Files to use for the mapping</param>
		/// <param name="SourcePatterns">List of source patterns</param>
		/// <param name="TargetPattern">Matching output pattern</param>
		/// <param name="Filter">Filter to apply to source files</param>
		/// <param name="TargetFileToSourceFile">Dictionary to receive a mapping from target file to source file. An exception is thrown if multiple source files map to one target file, or a source file is also used as a target file.</param>
		public static Dictionary<FileReference, FileReference> CreateMapping(HashSet<FileReference> Files, FilePattern SourcePattern, FilePattern TargetPattern)
		{
			// If the source pattern ends in a directory separator, or a set of input files are specified and it doesn't contain wildcards, treat it as a full directory match
			if(SourcePattern.EndsWithDirectorySeparator())
			{
				SourcePattern = new FilePattern(SourcePattern.BaseDirectory, String.Join("", SourcePattern.Tokens) + "...");
			}
			else if(Files != null)
			{
				SourcePattern = SourcePattern.AsDirectoryPattern();
			}

			// If we have multiple potential source files, but no wildcards in the output pattern, assume it's a directory and append the pattern from the source.
			if(SourcePattern.ContainsWildcards() && !TargetPattern.ContainsWildcards())
			{
				StringBuilder NewPattern = new StringBuilder();
				foreach(string Token in TargetPattern.Tokens)
				{
					NewPattern.Append(Token);
				}
				if(NewPattern.Length > 0 && NewPattern[NewPattern.Length - 1] != Path.DirectorySeparatorChar)
				{
					NewPattern.Append(Path.DirectorySeparatorChar);
				}
				foreach(string Token in SourcePattern.Tokens)
				{
					NewPattern.Append(Token);
				}
				TargetPattern = new FilePattern(TargetPattern.BaseDirectory, NewPattern.ToString());
			}

			// If the target pattern ends with a directory separator, treat it as a full directory match if it has wildcards, or a copy of the source pattern if not
			if(TargetPattern.EndsWithDirectorySeparator())
			{
				TargetPattern = new FilePattern(TargetPattern.BaseDirectory, String.Join("", TargetPattern.Tokens) + "...");
			}

			// Handle the case where source and target pattern are both individual files
			Dictionary<FileReference, FileReference> TargetFileToSourceFile = new Dictionary<FileReference, FileReference>();
			if(SourcePattern.ContainsWildcards() || TargetPattern.ContainsWildcards())
			{
				// Check the two patterns are compatible
				if(!SourcePattern.IsCompatibleWith(TargetPattern))
				{
					throw new AutomationException("File patterns '{0}' and '{1}' do not have matching wildcards", SourcePattern, TargetPattern);
				}

				// Create a filter to match the source files
				FileFilter Filter = new FileFilter(FileFilterType.Exclude);
				Filter.Include(String.Join("", SourcePattern.Tokens));

				// Apply it to the source directory
				List<FileReference> SourceFiles;
				if(Files == null)
				{
					SourceFiles = Filter.ApplyToDirectory(SourcePattern.BaseDirectory, true);
				}
				else
				{
					SourceFiles = CheckInputFiles(Files, SourcePattern.BaseDirectory);
				}

				// Map them onto output files
				FileReference[] TargetFiles = new FileReference[SourceFiles.Count];

				// Get the source and target regexes
				string SourceRegex = SourcePattern.GetRegexPattern();
				string TargetRegex = TargetPattern.GetRegexReplacementPattern();
				for(int Idx = 0; Idx < SourceFiles.Count; Idx++)
				{
					string SourceRelativePath = SourceFiles[Idx].MakeRelativeTo(SourcePattern.BaseDirectory);
					string TargetRelativePath = Regex.Replace(SourceRelativePath, SourceRegex, TargetRegex);
					TargetFiles[Idx] = FileReference.Combine(TargetPattern.BaseDirectory, TargetRelativePath);
				}

				// Add them to the output map
				for(int Idx = 0; Idx < TargetFiles.Length; Idx++)
				{
					FileReference ExistingSourceFile;
					if(TargetFileToSourceFile.TryGetValue(TargetFiles[Idx], out ExistingSourceFile) && ExistingSourceFile != SourceFiles[Idx])
					{
						throw new AutomationException("Output file '{0}' is mapped from '{1}' and '{2}'", TargetFiles[Idx], ExistingSourceFile, SourceFiles[Idx]);
					}
					TargetFileToSourceFile[TargetFiles[Idx]] = SourceFiles[Idx];
				}
			}
			else
			{
				// Just copy a single file
				FileReference SourceFile = SourcePattern.GetSingleFile();
				if(FileReference.Exists(SourceFile))
				{
					FileReference TargetFile = TargetPattern.GetSingleFile();
					TargetFileToSourceFile[TargetFile] = SourceFile;
				}
				else
				{
					throw new AutomationException("Source file '{0}' does not exist", SourceFile);
				}
			}

			// Check that no source file is also destination file
			foreach(FileReference SourceFile in TargetFileToSourceFile.Values)
			{
				if(TargetFileToSourceFile.ContainsKey(SourceFile))
				{
					throw new AutomationException("'{0}' is listed as a source and target file", SourceFile);
				}
			}

			// Return the map
			return TargetFileToSourceFile;
		}

		/// <summary>
		/// Checks that the given input files all exist and are under the given base directory
		/// </summary>
		/// <param name="InputFiles">Input files to check</param>
		/// <param name="BaseDirectory">Base directory for files</param>
		/// <returns>List of valid files</returns>
		public static List<FileReference> CheckInputFiles(IEnumerable<FileReference> InputFiles, DirectoryReference BaseDirectory)
		{
			List<FileReference> Files = new List<FileReference>();
			foreach(FileReference InputFile in InputFiles)
			{
				if(!InputFile.IsUnderDirectory(BaseDirectory))
				{
					throw new AutomationException("Source file '{0}' is not under '{1}'", InputFile, BaseDirectory);
				}
				else if(!FileReference.Exists(InputFile))
				{
					throw new AutomationException("Source file '{0}' does not exist", InputFile);
				}
				else
				{
					Files.Add(InputFile);
				}
			}
			return Files;
		}

		/// <summary>
		/// Formats the pattern as a string
		/// </summary>
		/// <returns>The original representation of this pattern</returns>
		public override string ToString()
		{
			return BaseDirectory.ToString() + Path.DirectorySeparatorChar + String.Join("", Tokens);
		}
	}
}
