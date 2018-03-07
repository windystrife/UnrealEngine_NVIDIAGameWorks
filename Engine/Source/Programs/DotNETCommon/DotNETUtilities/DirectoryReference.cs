using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon
{
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
			if(DirName.Length == 2 && DirName[1] == ':')
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

		#region System.IO.Directory Wrapper Methods

		/// <summary>
		/// Finds the current directory
		/// </summary>
		/// <returns>The current directory</returns>
		public static DirectoryReference GetCurrentDirectory()
		{
			return new DirectoryReference(Directory.GetCurrentDirectory());
		}

		/// <summary>
		/// Creates a directory
		/// </summary>
		/// <param name="Location">Location of the directory</param>
		public static void CreateDirectory(DirectoryReference Location)
		{
			Directory.CreateDirectory(Location.FullName);
		}

        /// <summary>
        /// Deletes a directory
        /// </summary>
		/// <param name="Location">Location of the directory</param>
        public static void Delete(DirectoryReference Location)
        {
            Directory.Delete(Location.FullName);
        }

        /// <summary>
        /// Deletes a directory
        /// </summary>
		/// <param name="Location">Location of the directory</param>
		/// <param name="bRecursive">Whether to remove directories recursively</param>
        public static void Delete(DirectoryReference Location, bool bRecursive)
        {
            Directory.Delete(Location.FullName, bRecursive);
        }

        /// <summary>
        /// Checks whether the directory exists
        /// </summary>
		/// <param name="Location">Location of the directory</param>
        /// <returns>True if this directory exists</returns>
        public static bool Exists(DirectoryReference Location)
		{
			return Directory.Exists(Location.FullName);
		}

		/// <summary>
		/// Enumerate files from a given directory
		/// </summary>
		/// <param name="BaseDir">Base directory to search in</param>
		/// <returns>Sequence of file references</returns>
		public static IEnumerable<FileReference> EnumerateFiles(DirectoryReference BaseDir)
		{
			foreach (string FileName in Directory.EnumerateFiles(BaseDir.FullName))
			{
				yield return FileReference.MakeFromNormalizedFullPath(FileName);
			}
		}

		/// <summary>
		/// Enumerate files from a given directory
		/// </summary>
		/// <param name="BaseDir">Base directory to search in</param>
		/// <param name="Pattern">Pattern for matching files</param>
		/// <returns>Sequence of file references</returns>
		public static IEnumerable<FileReference> EnumerateFiles(DirectoryReference BaseDir, string Pattern)
		{
			foreach (string FileName in Directory.EnumerateFiles(BaseDir.FullName, Pattern))
			{
				yield return FileReference.MakeFromNormalizedFullPath(FileName);
			}
		}

		/// <summary>
		/// Enumerate files from a given directory
		/// </summary>
		/// <param name="BaseDir">Base directory to search in</param>
		/// <param name="Pattern">Pattern for matching files</param>
		/// <param name="Option">Options for the search</param>
		/// <returns>Sequence of file references</returns>
		public static IEnumerable<FileReference> EnumerateFiles(DirectoryReference BaseDir, string Pattern, SearchOption Option)
		{
			foreach (string FileName in Directory.EnumerateFiles(BaseDir.FullName, Pattern, Option))
			{
				yield return FileReference.MakeFromNormalizedFullPath(FileName);
			}
		}

		/// <summary>
		/// Enumerate subdirectories in a given directory
		/// </summary>
		/// <param name="BaseDir">Base directory to search in</param>
		/// <returns>Sequence of directory references</returns>
		public static IEnumerable<DirectoryReference> EnumerateDirectories(DirectoryReference BaseDir)
		{
			foreach (string DirectoryName in Directory.EnumerateDirectories(BaseDir.FullName))
			{
				yield return DirectoryReference.MakeFromNormalizedFullPath(DirectoryName);
			}
		}

		/// <summary>
		/// Enumerate subdirectories in a given directory
		/// </summary>
		/// <param name="BaseDir">Base directory to search in</param>
		/// <param name="Pattern">Pattern for matching directories</param>
		/// <returns>Sequence of directory references</returns>
		public static IEnumerable<DirectoryReference> EnumerateDirectories(DirectoryReference BaseDir, string Pattern)
		{
			foreach (string DirectoryName in Directory.EnumerateDirectories(BaseDir.FullName, Pattern))
			{
				yield return DirectoryReference.MakeFromNormalizedFullPath(DirectoryName);
			}
		}

		/// <summary>
		/// Enumerate subdirectories in a given directory
		/// </summary>
		/// <param name="BaseDir">Base directory to search in</param>
		/// <param name="Pattern">Pattern for matching files</param>
		/// <param name="Option">Options for the search</param>
		/// <returns>Sequence of directory references</returns>
		public static IEnumerable<DirectoryReference> EnumerateDirectories(DirectoryReference BaseDir, string Pattern, SearchOption Option)
		{
			foreach (string DirectoryName in Directory.EnumerateDirectories(BaseDir.FullName, Pattern, Option))
			{
				yield return DirectoryReference.MakeFromNormalizedFullPath(DirectoryName);
			}
		}

		/// <summary>
		/// Sets the current directory
		/// </summary>
		/// <param name="Location">Location of the new current directory</param>
		public static void SetCurrentDirectory(DirectoryReference Location)
		{
			Directory.SetCurrentDirectory(Location.FullName);
		}

		#endregion
	}

	/// <summary>
	/// Extension methods for passing DirectoryReference arguments
	/// </summary>
	public static class DirectoryReferenceExtensionMethods
	{
		/// <summary>
		/// Manually serialize a file reference to a binary stream.
		/// </summary>
		/// <param name="Writer">Binary writer to write to</param>
		/// <param name="Directory">The directory reference to write</param>
		public static void Write(this BinaryWriter Writer, DirectoryReference Directory)
		{
			Writer.Write((Directory == null) ? String.Empty : Directory.FullName);
		}

		/// <summary>
		/// Manually deserialize a directory reference from a binary stream.
		/// </summary>
		/// <param name="Reader">Binary reader to read from</param>
		/// <returns>New DirectoryReference object</returns>
		public static DirectoryReference ReadDirectoryReference(this BinaryReader Reader)
		{
			string FullName = Reader.ReadString();
			return (FullName.Length == 0) ? null : DirectoryReference.MakeFromNormalizedFullPath(FullName);
		}
	}
}
