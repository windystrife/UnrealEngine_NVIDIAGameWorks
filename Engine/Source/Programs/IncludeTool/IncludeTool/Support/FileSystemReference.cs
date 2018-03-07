// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace IncludeTool.Support
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
				return CanonicalName.EndsWith(Extension.ToLowerInvariant());
			}
		}

		/// <summary>
		/// Determines if the given object is at or under the given directory
		/// </summary>
		/// <param name="Directory"></param>
		/// <returns></returns>
		public bool IsUnderDirectory(DirectoryReference Other)
		{
			return CanonicalName.StartsWith(Other.CanonicalName) && (CanonicalName.Length == Other.CanonicalName.Length || CanonicalName[Other.CanonicalName.Length] == Path.DirectorySeparatorChar);
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

	/// <summary>
	/// Representation of an absolute directory path. Allows fast hashing and comparisons.
	/// </summary>
	[Serializable]
	public class DirectoryReference : FileSystemReference, IEquatable<DirectoryReference>
	{
		/// <summary>
		/// Default constructor.
		/// </summary>
		/// <param name="InPath">Path to this directory.</param>
		public DirectoryReference(string InPath)
			: base(FixTrailingPathSeparator(Path.GetFullPath(InPath)))
		{
		}

		/// <summary>
		/// Construct a DirectoryReference from a DirectoryInfo object.
		/// </summary>
		/// <param name="InInfo">Path to this file</param>
		public DirectoryReference(DirectoryInfo InInfo)
			: base(FixTrailingPathSeparator(InInfo.FullName))
		{
		}

		/// <summary>
		/// Constructor for creating a directory object directly from two strings.
		/// </summary>
		/// <param name="InFullName"></param>
		/// <param name="InCanonicalName"></param>
		protected DirectoryReference(string InFullName, string InCanonicalName)
			: base(InFullName, InCanonicalName)
		{
		}

		/// <summary>
		/// Ensures that the correct trailing path separator is appended. On Windows, the root directory (eg. C:\) always has a trailing path separator, but no other
		/// path does.
		/// </summary>
		/// <param name="DirName">Absolute path to the directory</param>
		/// <returns>Path to the directory, with the correct trailing path separator</returns>
		private static string FixTrailingPathSeparator(string DirName)
		{
			if(DirName.Length == 2 && DirName[2] == ':')
			{
				return DirName + Path.DirectorySeparatorChar;
			}
			else if(DirName.Length == 3 && DirName[1] == ':' && DirName[2] == Path.DirectorySeparatorChar)
			{
				return DirName;
			}
			else if(DirName.Length > 1 && DirName[DirName.Length - 1] == Path.DirectorySeparatorChar)
			{
				return DirName.TrimEnd(Path.DirectorySeparatorChar);
			}
			else
			{
				return DirName;
			}
		}

		/// <summary>
		/// Gets the top level directory name
		/// </summary>
		/// <returns>The name of the directory</returns>
		public string GetDirectoryName()
		{
			return Path.GetFileName(FullName);
		}

		/// <summary>
		/// Gets the directory containing this object
		/// </summary>
		/// <returns>A new directory object representing the directory containing this object</returns>
		public DirectoryReference ParentDirectory
		{
			get
			{
				if (IsRootDirectory())
				{
					return null;
				}

				int ParentLength = CanonicalName.LastIndexOf(Path.DirectorySeparatorChar);
				if (ParentLength == 2 && CanonicalName[1] == ':')
				{
					ParentLength++;
				}

				return new DirectoryReference(FullName.Substring(0, ParentLength), CanonicalName.Substring(0, ParentLength));
			}
		}

		/// <summary>
		/// Gets the parent directory for a file
		/// </summary>
		/// <param name="File">The file to get directory for</param>
		/// <returns>The full directory name containing the given file</returns>
		public static DirectoryReference GetParentDirectory(FileReference File)
		{
			int ParentLength = File.CanonicalName.LastIndexOf(Path.DirectorySeparatorChar);
			return new DirectoryReference(File.FullName.Substring(0, ParentLength), File.CanonicalName.Substring(0, ParentLength));
		}

		/// <summary>
		/// Creates the directory
		/// </summary>
		public void CreateDirectory()
		{
			Directory.CreateDirectory(FullName);
		}

		/// <summary>
		/// Checks whether the directory exists
		/// </summary>
		/// <returns>True if this directory exists</returns>
		public bool Exists()
		{
			return Directory.Exists(FullName);
		}

		/// <summary>
		/// Enumerate files from a given directory
		/// </summary>
		/// <returns>Sequence of file references</returns>
		public IEnumerable<FileReference> EnumerateFileReferences()
		{
			foreach (string FileName in Directory.EnumerateFiles(FullName))
			{
				yield return FileReference.MakeFromNormalizedFullPath(FileName);
			}
		}

		/// <summary>
		/// Enumerate files from a given directory
		/// </summary>
		/// <returns>Sequence of file references</returns>
		public IEnumerable<FileReference> EnumerateFileReferences(string Pattern)
		{
			foreach (string FileName in Directory.EnumerateFiles(FullName, Pattern))
			{
				yield return FileReference.MakeFromNormalizedFullPath(FileName);
			}
		}

		/// <summary>
		/// Enumerate files from a given directory
		/// </summary>
		/// <returns>Sequence of file references</returns>
		public IEnumerable<FileReference> EnumerateFileReferences(string Pattern, SearchOption Option)
		{
			foreach (string FileName in Directory.EnumerateFiles(FullName, Pattern, Option))
			{
				yield return FileReference.MakeFromNormalizedFullPath(FileName);
			}
		}

		/// <summary>
		/// Enumerate subdirectories in a given directory
		/// </summary>
		/// <returns>Sequence of directory references</returns>
		public IEnumerable<DirectoryReference> EnumerateDirectoryReferences()
		{
			foreach (string DirectoryName in Directory.EnumerateDirectories(FullName))
			{
				yield return DirectoryReference.MakeFromNormalizedFullPath(DirectoryName);
			}
		}

		/// <summary>
		/// Enumerate subdirectories in a given directory
		/// </summary>
		/// <returns>Sequence of directory references</returns>
		public IEnumerable<DirectoryReference> EnumerateDirectoryReferences(string Pattern)
		{
			foreach (string DirectoryName in Directory.EnumerateDirectories(FullName, Pattern))
			{
				yield return DirectoryReference.MakeFromNormalizedFullPath(DirectoryName);
			}
		}

		/// <summary>
		/// Enumerate subdirectories in a given directory
		/// </summary>
		/// <returns>Sequence of directory references</returns>
		public IEnumerable<DirectoryReference> EnumerateDirectoryReferences(string Pattern, SearchOption Option)
		{
			foreach (string DirectoryName in Directory.EnumerateDirectories(FullName, Pattern, Option))
			{
				yield return DirectoryReference.MakeFromNormalizedFullPath(DirectoryName);
			}
		}

		/// <summary>
		/// Determines whether this path represents a root directory in the filesystem
		/// </summary>
		/// <returns>True if this path is a root directory, false otherwise</returns>
		public bool IsRootDirectory()
		{
			return CanonicalName[CanonicalName.Length - 1] == Path.DirectorySeparatorChar;
		}

		/// <summary>
		/// Combine several fragments with a base directory, to form a new directory name
		/// </summary>
		/// <param name="BaseDirectory">The base directory</param>
		/// <param name="Fragments">Fragments to combine with the base directory</param>
		/// <returns>The new directory name</returns>
		public static DirectoryReference Combine(DirectoryReference BaseDirectory, params string[] Fragments)
		{
			string FullName = FileSystemReference.CombineStrings(BaseDirectory, Fragments);
			return new DirectoryReference(FullName, FullName.ToLowerInvariant());
		}

		/// <summary>
		/// Compares two filesystem object names for equality. Uses the canonical name representation, not the display name representation.
		/// </summary>
		/// <param name="A">First object to compare.</param>
		/// <param name="B">Second object to compare.</param>
		/// <returns>True if the names represent the same object, false otherwise</returns>
		public static bool operator ==(DirectoryReference A, DirectoryReference B)
		{
			if ((object)A == null)
			{
				return (object)B == null;
			}
			else
			{
				return (object)B != null && A.CanonicalName == B.CanonicalName;
			}
		}

		/// <summary>
		/// Compares two filesystem object names for inequality. Uses the canonical name representation, not the display name representation.
		/// </summary>
		/// <param name="A">First object to compare.</param>
		/// <param name="B">Second object to compare.</param>
		/// <returns>False if the names represent the same object, true otherwise</returns>
		public static bool operator !=(DirectoryReference A, DirectoryReference B)
		{
			return !(A == B);
		}

		/// <summary>
		/// Compares against another object for equality.
		/// </summary>
		/// <param name="Obj">other instance to compare.</param>
		/// <returns>True if the names represent the same object, false otherwise</returns>
		public override bool Equals(object Obj)
		{
			return (Obj is DirectoryReference) && ((DirectoryReference)Obj) == this;
		}

		/// <summary>
		/// Compares against another object for equality.
		/// </summary>
		/// <param name="Obj">other instance to compare.</param>
		/// <returns>True if the names represent the same object, false otherwise</returns>
		public bool Equals(DirectoryReference Obj)
		{
			return Obj == this;
		}

		/// <summary>
		/// Returns a hash code for this object
		/// </summary>
		/// <returns></returns>
		public override int GetHashCode()
		{
			return CanonicalName.GetHashCode();
		}

		/// <summary>
		/// Helper function to create a remote directory reference. Unlike normal DirectoryReference objects, these aren't converted to a full path in the local filesystem.
		/// </summary>
		/// <param name="AbsolutePath">The absolute path in the remote file system</param>
		/// <returns>New directory reference</returns>
		public static DirectoryReference MakeRemote(string AbsolutePath)
		{
			return new DirectoryReference(AbsolutePath, AbsolutePath.ToLowerInvariant());
		}

		/// <summary>
		/// Helper function to create a directory reference from a raw platform path. The path provided *MUST* be exactly the same as that returned by Path.GetFullPath(). 
		/// </summary>
		/// <param name="AbsolutePath">The absolute path in the file system</param>
		/// <returns>New file reference</returns>
		public static DirectoryReference MakeFromNormalizedFullPath(string AbsolutePath)
		{
			return new DirectoryReference(AbsolutePath, AbsolutePath.ToLowerInvariant());
		}

		/// <summary>
		/// Gets the parent directory for a file, or returns null if it's null.
		/// </summary>
		/// <param name="File">The file to create a directory reference for</param>
		/// <returns>The directory containing the file  </returns>
		public static DirectoryReference FromFile(FileReference File)
		{
			return (File == null)? null : File.Directory;
		}
	}

	/// <summary>
	/// Representation of an absolute file path. Allows fast hashing and comparisons.
	/// </summary>
	[Serializable]
	public class FileReference : FileSystemReference, IEquatable<FileReference>
	{
		/// <summary>
		/// Default constructor.
		/// </summary>
		/// <param name="InPath">Path to this file</param>
		public FileReference(string InPath)
			: base(Path.GetFullPath(InPath))
		{
			if(FullName.EndsWith("\\") || FullName.EndsWith("/"))
			{
				throw new ArgumentException("File names may not be terminated by a path separator character");
			}
		}

		/// <summary>
		/// Construct a FileReference from a FileInfo object.
		/// </summary>
		/// <param name="InInfo">Path to this file</param>
		public FileReference(FileInfo InInfo)
			: base(InInfo.FullName)
		{
		}

		/// <summary>
		/// Default constructor.
		/// </summary>
		/// <param name="InPath">Path to this file</param>
		protected FileReference(string InFullName, string InCanonicalName)
			: base(InFullName, InCanonicalName)
		{
		}

		/// <summary>
		/// Gets the file name without path information
		/// </summary>
		/// <returns>A string containing the file name</returns>
		public string GetFileName()
		{
			return Path.GetFileName(FullName);
		}

		/// <summary>
		/// Gets the file name without path information or an extension
		/// </summary>
		/// <returns>A string containing the file name without an extension</returns>
		public string GetFileNameWithoutExtension()
		{
			return Path.GetFileNameWithoutExtension(FullName);
		}

		/// <summary>
		/// Gets the file name without path or any extensions
		/// </summary>
		/// <returns>A string containing the file name without an extension</returns>
		public string GetFileNameWithoutAnyExtensions()
		{
			int StartIdx = FullName.LastIndexOf(Path.DirectorySeparatorChar) + 1;

			int EndIdx = FullName.IndexOf('.', StartIdx);
			if (EndIdx < StartIdx)
			{
				return FullName.Substring(StartIdx);
			}
			else
			{
				return FullName.Substring(StartIdx, EndIdx - StartIdx);
			}
		}

		/// <summary>
		/// Gets the extension for this filename
		/// </summary>
		/// <returns>A string containing the extension of this filename</returns>
		public string GetExtension()
		{
			return Path.GetExtension(FullName);
		}

		/// <summary>
		/// Change the file's extension to something else
		/// </summary>
		/// <param name="Extension">The new extension</param>
		/// <returns>A FileReference with the same path and name, but with the new extension</returns>
		public FileReference ChangeExtension(string Extension)
		{
			string NewFullName = Path.ChangeExtension(FullName, Extension);
			return new FileReference(NewFullName, NewFullName.ToLowerInvariant());
		}

		/// <summary>
		/// Gets the directory containing this file
		/// </summary>
		/// <returns>A new directory object representing the directory containing this object</returns>
		public DirectoryReference Directory
		{
			get { return DirectoryReference.GetParentDirectory(this); }
		}

		/// <summary>
		/// Determines whether the given filename exists
		/// </summary>
		/// <returns>True if it exists, false otherwise</returns>
		public bool Exists()
		{
			return File.Exists(FullName);
		}

		/// <summary>
		/// Deletes this file
		/// </summary>
		public void Delete()
		{
			File.Delete(FullName);
		}

		/// <summary>
		/// Combine several fragments with a base directory, to form a new filename
		/// </summary>
		/// <param name="BaseDirectory">The base directory</param>
		/// <param name="Fragments">Fragments to combine with the base directory</param>
		/// <returns>The new file name</returns>
		public static FileReference Combine(DirectoryReference BaseDirectory, params string[] Fragments)
		{
			string FullName = FileSystemReference.CombineStrings(BaseDirectory, Fragments);
			return new FileReference(FullName, FullName.ToLowerInvariant());
		}

		/// <summary>
		/// Append a string to the end of a filename
		/// </summary>
		/// <param name="A">The base file reference</param>
		/// <param name="B">Suffix to be appended</param>
		/// <returns>The new file reference</returns>
		public static FileReference operator +(FileReference A, string B)
		{
			return new FileReference(A.FullName + B, A.CanonicalName + B.ToLowerInvariant());
		}

		/// <summary>
		/// Compares two filesystem object names for equality. Uses the canonical name representation, not the display name representation.
		/// </summary>
		/// <param name="A">First object to compare.</param>
		/// <param name="B">Second object to compare.</param>
		/// <returns>True if the names represent the same object, false otherwise</returns>
		public static bool operator ==(FileReference A, FileReference B)
		{
			if ((object)A == null)
			{
				return (object)B == null;
			}
			else
			{
				return (object)B != null && A.CanonicalName == B.CanonicalName;
			}
		}

		/// <summary>
		/// Compares two filesystem object names for inequality. Uses the canonical name representation, not the display name representation.
		/// </summary>
		/// <param name="A">First object to compare.</param>
		/// <param name="B">Second object to compare.</param>
		/// <returns>False if the names represent the same object, true otherwise</returns>
		public static bool operator !=(FileReference A, FileReference B)
		{
			return !(A == B);
		}

		/// <summary>
		/// Compares against another object for equality.
		/// </summary>
		/// <param name="Obj">other instance to compare.</param>
		/// <returns>True if the names represent the same object, false otherwise</returns>
		public override bool Equals(object Obj)
		{
			return (Obj is FileReference) && ((FileReference)Obj) == this;
		}

		/// <summary>
		/// Compares against another object for equality.
		/// </summary>
		/// <param name="Obj">other instance to compare.</param>
		/// <returns>True if the names represent the same object, false otherwise</returns>
		public bool Equals(FileReference Obj)
		{
			return Obj == this;
		}

		/// <summary>
		/// Returns a hash code for this object
		/// </summary>
		/// <returns></returns>
		public override int GetHashCode()
		{
			return CanonicalName.GetHashCode();
		}

		/// <summary>
		/// Helper function to create a remote file reference. Unlike normal FileReference objects, these aren't converted to a full path in the local filesystem, but are
		/// left as they are passed in.
		/// </summary>
		/// <param name="AbsolutePath">The absolute path in the remote file system</param>
		/// <returns>New file reference</returns>
		public static FileReference MakeRemote(string AbsolutePath)
		{
			return new FileReference(AbsolutePath, AbsolutePath.ToLowerInvariant());
		}

		/// <summary>
		/// Helper function to create a file reference from a raw platform path. The path provided *MUST* be exactly the same as that returned by Path.GetFullPath(). 
		/// </summary>
		/// <param name="AbsolutePath">The absolute path in the file system</param>
		/// <returns>New file reference</returns>
		public static FileReference MakeFromNormalizedFullPath(string AbsolutePath)
		{
			return new FileReference(AbsolutePath, AbsolutePath.ToLowerInvariant());
		}
	}

	static class FileReferenceExtensionMethods
	{
		/// <summary>
		/// Manually serialize a file reference to a binary stream.
		/// </summary>
		/// <param name="Writer">Binary writer to write to</param>
		public static void Write(this BinaryWriter Writer, FileReference File)
		{
			Writer.Write((File == null) ? String.Empty : File.FullName);
		}

		/// <summary>
		/// Serializes a file reference, using a lookup table to avoid serializing the same name more than once.
		/// </summary>
		/// <param name="Writer">The writer to save this reference to</param>
		/// <param name="File">A file reference to output; may be null</param>
		/// <param name="FileToUniqueId">A lookup table that caches previous files that have been output, and maps them to unique id's.</param>
		public static void Write(this BinaryWriter Writer, FileReference File, Dictionary<FileReference, int> FileToUniqueId)
		{
			int UniqueId;
			if (File == null)
			{
				Writer.Write(-1);
			}
			else if (FileToUniqueId.TryGetValue(File, out UniqueId))
			{
				Writer.Write(UniqueId);
			}
			else
			{
				Writer.Write(FileToUniqueId.Count);
				Writer.Write(File);
				FileToUniqueId.Add(File, FileToUniqueId.Count);
			}
		}

		/// <summary>
		/// Manually deserialize a file reference from a binary stream.
		/// </summary>
		/// <param name="Reader">Binary reader to read from</param>
		/// <returns>New FileReference object</returns>
		public static FileReference ReadFileReference(this BinaryReader Reader)
		{
			string FullName = Reader.ReadString();
			return (FullName.Length == 0) ? null : FileReference.MakeFromNormalizedFullPath(FullName);
		}

		/// <summary>
		/// Deserializes a file reference, using a lookup table to avoid writing the same name more than once.
		/// </summary>
		/// <param name="Reader">The source to read from</param>
		/// <param name="UniqueFiles">List of previously read file references. The index into this array is used in place of subsequent ocurrences of the file.</param>
		/// <returns>The file reference that was read</returns>
		public static FileReference ReadFileReference(this BinaryReader Reader, List<FileReference> UniqueFiles)
		{
			int UniqueId = Reader.ReadInt32();
			if (UniqueId == -1)
			{
				return null;
			}
			else if (UniqueId < UniqueFiles.Count)
			{
				return UniqueFiles[UniqueId];
			}
			else
			{
				FileReference Result = Reader.ReadFileReference();
				UniqueFiles.Add(Result);
				return Result;
			}
		}
	}
}
