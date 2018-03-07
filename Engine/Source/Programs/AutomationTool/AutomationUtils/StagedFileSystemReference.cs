using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using UnrealBuildTool;

/// <summary>
/// Location of a staged file or directory. Stored in a normalized format with forward slashes as directory separators.
/// </summary>
public abstract class StagedFileSystemReference
{
	/// <summary>
	/// The name of this file system entity
	/// </summary>
	public readonly string Name;

	/// <summary>
	/// Name of this filesystem entity in a canonical format for easy comparison
	/// </summary>
	public readonly string CanonicalName;

	/// <summary>
	/// Constructor
	/// </summary>
	/// <param name="Name">The file system reference referred to. Either type of directory separator is permitted and will be normalized. Empty path fragments and leading/trailing slashes are not permitted.</param>
	public StagedFileSystemReference(string Name)
	{
		Name = Name.Replace('\\', '/');

		this.Name = Name;
		this.CanonicalName = Name.ToLowerInvariant();

		if (Name.Length > 0)
		{
			int LastIdx = -1;
			for (int Idx = 0; Idx <= Name.Length; Idx++)
			{
				if (Idx == Name.Length || Name[Idx] == '/')
				{
					if (Idx == LastIdx + 1)
					{
						throw new ArgumentException(String.Format("Empty path fragment in staged filesystem reference ({0})", Name));
					}
					if (Idx == LastIdx + 2 && Name[Idx - 1] == '.')
					{
						throw new ArgumentException(String.Format("Pseudo-directories are not permitted in filesystem references ({0})", Name));
					}
					if (Idx == LastIdx + 3 && Name[Idx - 1] == '.' && Name[Idx - 2] == '.')
					{
						throw new ArgumentException(String.Format("Pseudo-directories are not permitted in filesystem references ({0})", Name));
					}
					LastIdx = Idx;
				}
			}
		}
	}

	/// <summary>
	/// Protected constructor. Initializes to the given parameters without validation.
	/// </summary>
	/// <param name="Name">The name of this entity.</param>
	/// <param name="CanonicalName">Canonical name of this entity. Should be equal to Name.ToLowerInvariant().</param>
	protected StagedFileSystemReference(string Name, string CanonicalName)
	{
		this.Name = Name;
		this.CanonicalName = CanonicalName;
	}

	/// <summary>
	/// Checks if this path is equal to or under the given directory
	/// </summary>
	/// <param name="Directory">The directory to check against</param>
	/// <returns>True if the path is under the given directory</returns>
	public bool IsUnderDirectory(StagedDirectoryReference Directory)
	{
		return CanonicalName.StartsWith(Directory.CanonicalName) && (CanonicalName.Length == Directory.CanonicalName.Length || CanonicalName[Directory.CanonicalName.Length] == '/');
	}

	/// <summary>
	/// Create a full path by concatenating multiple strings
	/// </summary>
	/// <returns></returns>
	static protected string CombineStrings(string BaseDirectory, params string[] Fragments)
	{
		StringBuilder NewName = new StringBuilder(BaseDirectory);
		foreach (string Fragment in Fragments)
		{
			if (NewName.Length > 0 && (NewName[NewName.Length - 1] != '/' && NewName[NewName.Length - 1] != '\\'))
			{
				NewName.Append('/');
			}
			NewName.Append(Fragment);
		}
		return NewName.ToString();
	}

	/// <summary>
	/// Returns the name of this entity
	/// </summary>
	/// <returns>Path to the file or directory</returns>
	public override string ToString()
	{
		return Name;
	}
}
