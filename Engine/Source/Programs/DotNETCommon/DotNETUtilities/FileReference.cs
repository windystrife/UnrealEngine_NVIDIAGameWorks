using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon
{
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

		/// <summary>
		/// Makes a file location writeable; 
		/// </summary>
		/// <param name="Location">Location of the file</param>
		public static void MakeWriteable(FileReference Location)
		{
			if(Exists(Location))
			{
				FileAttributes Attributes = GetAttributes(Location);
				if((Attributes & FileAttributes.ReadOnly) != 0)
				{
					SetAttributes(Location, Attributes & ~FileAttributes.ReadOnly);
				}
			}
		}

		/// <summary>
		/// Constructs a FileInfo object from this reference
		/// </summary>
		/// <returns>New FileInfo object</returns>
		public FileInfo ToFileInfo()
		{
			return new FileInfo(FullName);
		}

		#region System.IO.File methods

		/// <summary>
		/// Copies a file from one location to another
		/// </summary>
		/// <param name="SourceLocation">Location of the source file</param>
		/// <param name="TargetLocation">Location of the target file</param>
		public static void Copy(FileReference SourceLocation, FileReference TargetLocation)
		{
			File.Copy(SourceLocation.FullName, TargetLocation.FullName);
		}

		/// <summary>
		/// Copies a file from one location to another
		/// </summary>
		/// <param name="SourceLocation">Location of the source file</param>
		/// <param name="TargetLocation">Location of the target file</param>
		/// <param name="bOverwrite">Whether to overwrite the file in the target location</param>
		public static void Copy(FileReference SourceLocation, FileReference TargetLocation, bool bOverwrite)
		{
			File.Copy(SourceLocation.FullName, TargetLocation.FullName, bOverwrite);
		}

		/// <summary>
		/// Deletes this file
		/// </summary>
		public static void Delete(FileReference Location)
		{
			File.Delete(Location.FullName);
		}

		/// <summary>
		/// Determines whether the given filename exists
		/// </summary>
		/// <returns>True if it exists, false otherwise</returns>
		public static bool Exists(FileReference Location)
		{
			return File.Exists(Location.FullName);
		}

		/// <summary>
		/// Gets the attributes for a file
		/// </summary>
		/// <param name="Location">Location of the file</param>
		/// <returns>Attributes for the file</returns>
		public static FileAttributes GetAttributes(FileReference Location)
		{
			return File.GetAttributes(Location.FullName);
		}

		/// <summary>
		/// Gets the time that the file was last written to
		/// </summary>
		/// <param name="Location">Location of the file</param>
		/// <returns>Last write time, in local time</returns>
		public static DateTime GetLastWriteTime(FileReference Location)
		{
			return File.GetLastWriteTime(Location.FullName);
		}

		/// <summary>
		/// Gets the time that the file was last written to
		/// </summary>
		/// <param name="Location">Location of the file</param>
		/// <returns>Last write time, in UTC time</returns>
		public static DateTime GetLastWriteTimeUtc(FileReference Location)
		{
			return File.GetLastWriteTimeUtc(Location.FullName);
		}

		/// <summary>
		/// Moves a file from one location to another
		/// </summary>
		/// <param name="SourceLocation">Location of the source file</param>
		/// <param name="TargetLocation">Location of the target file</param>
		public static void Move(FileReference SourceLocation, FileReference TargetLocation)
		{
			File.Move(SourceLocation.FullName, TargetLocation.FullName);
		}

		/// <summary>
		/// Opens a FileStream on the specified path with read/write access
		/// </summary>
		/// <param name="Location">Location of the file</param>
		/// <param name="Mode">Mode to use when opening the file</param>
		/// <returns>New filestream for the given file</returns>
		public static FileStream Open(FileReference Location, FileMode Mode)
		{
			return File.Open(Location.FullName, Mode);
		}

		/// <summary>
		/// Opens a FileStream on the specified path
		/// </summary>
		/// <param name="Location">Location of the file</param>
		/// <param name="Mode">Mode to use when opening the file</param>
		/// <param name="Access">Sharing mode for the new file</param>
		/// <returns>New filestream for the given file</returns>
		public static FileStream Open(FileReference Location, FileMode Mode, FileAccess Access)
		{
			return File.Open(Location.FullName, Mode, Access);
		}

		/// <summary>
		/// Opens a FileStream on the specified path
		/// </summary>
		/// <param name="Location">Location of the file</param>
		/// <param name="Mode">Mode to use when opening the file</param>
		/// <param name="Access">Access mode for the new file</param>
		/// <param name="Share">Sharing mode for the open file</param>
		/// <returns>New filestream for the given file</returns>
		public static FileStream Open(FileReference Location, FileMode Mode, FileAccess Access, FileShare Share)
		{
			return File.Open(Location.FullName, Mode, Access, Share);
		}

		/// <summary>
		/// Reads the contents of a file
		/// </summary>
		/// <param name="Location">Location of the file</param>
		/// <returns>Byte array containing the contents of the file</returns>
		public static byte[] ReadAllBytes(FileReference Location)
		{
			return File.ReadAllBytes(Location.FullName);
		}

		/// <summary>
		/// Reads the contents of a file
		/// </summary>
		/// <param name="Location">Location of the file</param>
		/// <returns>Contents of the file as a single string</returns>
		public static string ReadAllText(FileReference Location)
		{
			return File.ReadAllText(Location.FullName);
		}

		/// <summary>
		/// Reads the contents of a file
		/// </summary>
		/// <param name="Location">Location of the file</param>
		/// <param name="Encoding">Encoding of the file</param>
		/// <returns>Contents of the file as a single string</returns>
		public static string ReadAllText(FileReference Location, Encoding Encoding)
		{
			return File.ReadAllText(Location.FullName, Encoding);
		}

		/// <summary>
		/// Reads the contents of a file
		/// </summary>
		/// <param name="Location">Location of the file</param>
		/// <returns>String array containing the contents of the file</returns>
		public static string[] ReadAllLines(FileReference Location)
		{
			return File.ReadAllLines(Location.FullName);
		}

		/// <summary>
		/// Reads the contents of a file
		/// </summary>
		/// <param name="Location">Location of the file</param>
		/// <param name="Encoding">The encoding to use when parsing the file</param>
		/// <returns>String array containing the contents of the file</returns>
		public static string[] ReadAllLines(FileReference Location, Encoding Encoding)
		{
			return File.ReadAllLines(Location.FullName, Encoding);
		}

		/// <summary>
		/// Sets the attributes for a file
		/// </summary>
		/// <param name="Location">Location of the file</param>
		/// <param name="Attributes">New attributes for the file</param>
		public static void SetAttributes(FileReference Location, FileAttributes Attributes)
		{
			File.SetAttributes(Location.FullName, Attributes);
		}

		/// <summary>
		/// Sets the time that the file was last written to
		/// </summary>
		/// <param name="Location">Location of the file</param>
		/// <param name="LastWriteTime">Last write time, in local time</param>
		public static void SetLastWriteTime(FileReference Location, DateTime LastWriteTime)
		{
			File.SetLastWriteTime(Location.FullName, LastWriteTime);
		}

		/// <summary>
		/// Sets the time that the file was last written to
		/// </summary>
		/// <param name="Location">Location of the file</param>
		/// <param name="LastWriteTimeUtc">Last write time, in UTC time</param>
		public static void SetLastWriteTimeUtc(FileReference Location, DateTime LastWriteTimeUtc)
		{
			File.SetLastWriteTimeUtc(Location.FullName, LastWriteTimeUtc);
		}

		/// <summary>
		/// Writes the contents of a file
		/// </summary>
		/// <param name="Location">Location of the file</param>
		/// <param name="Contents">Contents of the file</param>
		public static void WriteAllBytes(FileReference Location, byte[] Contents)
		{
			File.WriteAllBytes(Location.FullName, Contents);
		}

		/// <summary>
		/// Writes the contents of a file
		/// </summary>
		/// <param name="Location">Location of the file</param>
		/// <param name="Contents">Contents of the file</param>
		public static void WriteAllLines(FileReference Location, IEnumerable<string> Contents)
		{
			File.WriteAllLines(Location.FullName, Contents);
		}

		/// <summary>
		/// Writes the contents of a file
		/// </summary>
		/// <param name="Location">Location of the file</param>
		/// <param name="Contents">Contents of the file</param>
		/// <param name="Encoding">The encoding to use when parsing the file</param>
		public static void WriteAllLines(FileReference Location, IEnumerable<string> Contents, Encoding Encoding)
		{
			File.WriteAllLines(Location.FullName, Contents, Encoding);
		}

		/// <summary>
		/// Writes the contents of a file
		/// </summary>
		/// <param name="Location">Location of the file</param>
		/// <param name="Contents">Contents of the file</param>
		public static void WriteAllLines(FileReference Location, string[] Contents)
		{
			File.WriteAllLines(Location.FullName, Contents);
		}

		/// <summary>
		/// Writes the contents of a file
		/// </summary>
		/// <param name="Location">Location of the file</param>
		/// <param name="Contents">Contents of the file</param>
		/// <param name="Encoding">The encoding to use when parsing the file</param>
		public static void WriteAllLines(FileReference Location, string[] Contents, Encoding Encoding)
		{
			File.WriteAllLines(Location.FullName, Contents, Encoding);
		}

		/// <summary>
		/// Writes the contents of a file
		/// </summary>
		/// <param name="Location">Location of the file</param>
		/// <param name="Contents">Contents of the file</param>
		public static void WriteAllText(FileReference Location, string Contents)
		{
			File.WriteAllText(Location.FullName, Contents);
		}

		/// <summary>
		/// Writes the contents of a file
		/// </summary>
		/// <param name="Location">Location of the file</param>
		/// <param name="Contents">Contents of the file</param>
		/// <param name="Encoding">The encoding to use when parsing the file</param>
		public static void WriteAllText(FileReference Location, string Contents, Encoding Encoding)
		{
			File.WriteAllText(Location.FullName, Contents, Encoding);
		}

		#endregion
	}

	/// <summary>
	/// Extension methods for FileReference functionality
	/// </summary>
	public static class FileReferenceExtensionMethods
	{
		/// <summary>
		/// Manually serialize a file reference to a binary stream.
		/// </summary>
		/// <param name="Writer">Binary writer to write to</param>
		/// <param name="File">The file reference to write</param>
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
