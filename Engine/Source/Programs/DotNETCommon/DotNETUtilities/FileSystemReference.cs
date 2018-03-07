// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon
{
	/// <summary>
	/// Base class for file system objects (files or directories).
	/// </summary>
	[Serializable]
	public abstract class FileSystemReference
	{
		/// <summary>
		/// The path to this object. Stored as an absolute path, with O/S preferred separator characters, and no trailing slash for directories.
		/// </summary>
		public readonly string FullName;

		/// <summary>
		/// The canonical full name for this object.
		/// </summary>
		public readonly string CanonicalName;

		/// <summary>
		/// Direct constructor for a path
		/// </summary>
		protected FileSystemReference(string InFullName)
		{
			FullName = InFullName;
			CanonicalName = InFullName.ToLowerInvariant();
		}

		/// <summary>
		/// Direct constructor for a path
		/// </summary>
		protected FileSystemReference(string InFullName, string InCanonicalName)
		{
			FullName = InFullName;
			CanonicalName = InCanonicalName;
		}

		/// <summary>
		/// Create a full path by concatenating multiple strings
		/// </summary>
		/// <returns></returns>
		static protected string CombineStrings(DirectoryReference BaseDirectory, params string[] Fragments)
		{
			// Get the initial string to append to, and strip any root directory suffix from it
			StringBuilder NewFullName = new StringBuilder(BaseDirectory.FullName);
			if (NewFullName.Length > 0 && NewFullName[NewFullName.Length - 1] == Path.DirectorySeparatorChar)
			{
				NewFullName.Remove(NewFullName.Length - 1, 1);
			}

			// Scan through the fragments to append, appending them to a string and updating the base length as we go
			foreach (string Fragment in Fragments)
			{
				// Check if this fragment is an absolute path
				if ((Fragment.Length >= 2 && Fragment[1] == ':') || (Fragment.Length >= 1 && (Fragment[0] == '\\' || Fragment[0] == '/')))
				{
					// It is. Reset the new name to the full version of this path.
					NewFullName.Clear();
					NewFullName.Append(Path.GetFullPath(Fragment).TrimEnd(Path.DirectorySeparatorChar));
				}
				else
				{
					// Append all the parts of this fragment to the end of the existing path.
					int StartIdx = 0;
					while (StartIdx < Fragment.Length)
					{
						// Find the end of this fragment. We may have been passed multiple paths in the same string.
						int EndIdx = StartIdx;
						while (EndIdx < Fragment.Length && Fragment[EndIdx] != '\\' && Fragment[EndIdx] != '/')
						{
							EndIdx++;
						}

						// Ignore any empty sections, like leading or trailing slashes, and '.' directory references.
						int Length = EndIdx - StartIdx;
						if (Length == 0)
						{
							// Multiple directory separators in a row; illegal.
							throw new ArgumentException(String.Format("Path fragment '{0}' contains invalid directory separators.", Fragment));
						}
						else if (Length == 2 && Fragment[StartIdx] == '.' && Fragment[StartIdx + 1] == '.')
						{
							// Remove the last directory name
							for (int SeparatorIdx = NewFullName.Length - 1; SeparatorIdx >= 0; SeparatorIdx--)
							{
								if (NewFullName[SeparatorIdx] == Path.DirectorySeparatorChar)
								{
									NewFullName.Remove(SeparatorIdx, NewFullName.Length - SeparatorIdx);
									break;
								}
							}
						}
						else if (Length != 1 || Fragment[StartIdx] != '.')
						{
							// Append this fragment
							NewFullName.Append(Path.DirectorySeparatorChar);
							NewFullName.Append(Fragment, StartIdx, Length);
						}

						// Move to the next part
						StartIdx = EndIdx + 1;
					}
				}
			}

			// Append the directory separator
			if (NewFullName.Length == 0 || (NewFullName.Length == 2 && NewFullName[1] == ':'))
			{
				NewFullName.Append(Path.DirectorySeparatorChar);
			}

			// Set the new path variables
			return NewFullName.ToString();
		}

		/// <summary>
		/// Checks whether this name has the given extension.
		/// </summary>
		/// <param name="Extension">The extension to check</param>
		/// <returns>True if this name has the given extension, false otherwise</returns>
		public bool HasExtension(string Extension)
		{
			if (Extension.Length > 0 && Extension[0] != '.')
			{
				return HasExtension("." + Extension);
			}
			else
			{
				return CanonicalName.EndsWith(Extension, StringComparison.InvariantCultureIgnoreCase);
			}
		}

		/// <summary>
		/// Determines if the given object is at or under the given directory
		/// </summary>
		/// <param name="Other">Directory to check against</param>
		/// <returns>True if this path is under the given directory</returns>
		public bool IsUnderDirectory(DirectoryReference Other)
		{
			return CanonicalName.StartsWith(Other.CanonicalName) && (CanonicalName.Length == Other.CanonicalName.Length || CanonicalName[Other.CanonicalName.Length] == Path.DirectorySeparatorChar || Other.IsRootDirectory());
		}

		/// <summary>
		/// Searches the path fragments for the given name. Only complete fragments are considered a match.
		/// </summary>
		/// <param name="Name">Name to check for</param>
		/// <param name="Offset">Offset within the string to start the search</param>
		/// <returns>True if the given name is found within the path</returns>
		public bool ContainsName(FileSystemName Name, int Offset)
		{
			return ContainsName(Name, Offset, FullName.Length - Offset);
		}

		/// <summary>
		/// Searches the path fragments for the given name. Only complete fragments are considered a match.
		/// </summary>
		/// <param name="Name">Name to check for</param>
		/// <param name="Offset">Offset within the string to start the search</param>
		/// <param name="Length">Length of the substring to search</param>
		/// <returns>True if the given name is found within the path</returns>
		public bool ContainsName(FileSystemName Name, int Offset, int Length)
		{
			// Check the substring to search is at least long enough to contain a match
			if(Length < Name.CanonicalName.Length)
			{
				return false;
			}

			// Find each occurence of the name within the remaining string, then test whether it's surrounded by directory separators
			int MatchIdx = Offset;
			for(;;)
			{
				// Find the next occurrence
				MatchIdx = CanonicalName.IndexOf(Name.CanonicalName, MatchIdx, Offset + Length - MatchIdx);
				if(MatchIdx == -1)
				{
					return false;
				}

				// Check if the substring is a directory
				int MatchEndIdx = MatchIdx + Name.CanonicalName.Length;
				if(CanonicalName[MatchIdx - 1] == Path.DirectorySeparatorChar && (MatchEndIdx == CanonicalName.Length || CanonicalName[MatchEndIdx] == Path.DirectorySeparatorChar))
				{
					return true;
				}

				// Move past the string that didn't match
				MatchIdx += Name.CanonicalName.Length;
			}
		}

		/// <summary>
		/// Determines if the given object is under the given directory, within a subfolder of the given name. Useful for masking out directories by name.
		/// </summary>
		/// <param name="Name">Name of a subfolder to also check for</param>
		/// <param name="BaseDir">Base directory to check against</param>
		/// <returns>True if the path is under the given directory</returns>
		public bool ContainsName(FileSystemName Name, DirectoryReference BaseDir)
		{
			// Check that this is under the base directory
			if(!IsUnderDirectory(BaseDir))
			{
				return false;
			}
			else
			{
				return ContainsName(Name, BaseDir.FullName.Length);
			}
		}

		/// <summary>
		/// Determines if the given object is under the given directory, within a subfolder of the given name. Useful for masking out directories by name.
		/// </summary>
		/// <param name="Names">Names of subfolders to also check for</param>
		/// <param name="BaseDir">Base directory to check against</param>
		/// <returns>True if the path is under the given directory</returns>
		public bool ContainsAnyNames(FileSystemName[] Names, DirectoryReference BaseDir)
		{
			// Check that this is under the base directory
			if(!IsUnderDirectory(BaseDir))
			{
				return false;
			}
			else
			{
				return Names.Any(x => ContainsName(x, BaseDir.FullName.Length));
			}
		}
		
		/// <summary>
		/// Creates a relative path from the given base directory
		/// </summary>
		/// <param name="Directory">The directory to create a relative path from</param>
		/// <returns>A relative path from the given directory</returns>
		public string MakeRelativeTo(DirectoryReference Directory)
		{
			// Find how much of the path is common between the two paths. This length does not include a trailing directory separator character.
			int CommonDirectoryLength = -1;
			for (int Idx = 0; ; Idx++)
			{
				if (Idx == CanonicalName.Length)
				{
					// The two paths are identical. Just return the "." character.
					if (Idx == Directory.CanonicalName.Length)
					{
						return ".";
					}

					// Check if we're finishing on a complete directory name
					if (Directory.CanonicalName[Idx] == Path.DirectorySeparatorChar)
					{
						CommonDirectoryLength = Idx;
					}
					break;
				}
				else if (Idx == Directory.CanonicalName.Length)
				{
					// Check whether the end of the directory name coincides with a boundary for the current name.
					if (CanonicalName[Idx] == Path.DirectorySeparatorChar)
					{
						CommonDirectoryLength = Idx;
					}
					break;
				}
				else
				{
					// Check the two paths match, and bail if they don't. Increase the common directory length if we've reached a separator.
					if (CanonicalName[Idx] != Directory.CanonicalName[Idx])
					{
						break;
					}
					if (CanonicalName[Idx] == Path.DirectorySeparatorChar)
					{
						CommonDirectoryLength = Idx;
					}
				}
			}

			// If there's no relative path, just return the absolute path
			if (CommonDirectoryLength == -1)
			{
				return FullName;
			}

			// Append all the '..' separators to get back to the common directory, then the rest of the string to reach the target item
			StringBuilder Result = new StringBuilder();
			for (int Idx = CommonDirectoryLength + 1; Idx < Directory.CanonicalName.Length; Idx++)
			{
				// Move up a directory
				Result.Append("..");
				Result.Append(Path.DirectorySeparatorChar);

				// Scan to the next directory separator
				while (Idx < Directory.CanonicalName.Length && Directory.CanonicalName[Idx] != Path.DirectorySeparatorChar)
				{
					Idx++;
				}
			}
			if (CommonDirectoryLength + 1 < FullName.Length)
			{
				Result.Append(FullName, CommonDirectoryLength + 1, FullName.Length - CommonDirectoryLength - 1);
			}
			return Result.ToString();
		}

		/// <summary>
		/// Returns a string representation of this filesystem object
		/// </summary>
		/// <returns>Full path to the object</returns>
		public override string ToString()
		{
			return FullName;
		}
	}
}
