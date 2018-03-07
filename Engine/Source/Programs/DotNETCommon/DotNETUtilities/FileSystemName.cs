using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon
{
	/// <summary>
	/// Represents a case-insensitive name in the filesystem
	/// </summary>
	public class FileSystemName
	{
		/// <summary>
		/// Name as it should be displayed
		/// </summary>
		public readonly string DisplayName;

		/// <summary>
		/// The canonical form of the name
		/// </summary>
		public readonly string CanonicalName;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="DisplayName">The display name</param>
		public FileSystemName(string DisplayName)
		{
			this.DisplayName = DisplayName;
			CanonicalName = DisplayName.ToLowerInvariant();
		}

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="Reference">Reference to a file or directory in the filesystem</param>
		public FileSystemName(FileSystemReference Reference)
		{
			int Idx = Reference.FullName.LastIndexOf(Path.DirectorySeparatorChar);
			if(Idx == Reference.FullName.Length - 1)
			{
				DisplayName = Reference.FullName;
				CanonicalName = Reference.CanonicalName;
			}
			else
			{
				DisplayName = Reference.FullName.Substring(Idx + 1);
				CanonicalName = Reference.CanonicalName.Substring(Idx + 1);
			}
		}

		/// <summary>
		/// Compares two filesystem object names for equality. Uses the canonical name representation, not the display name representation.
		/// </summary>
		/// <param name="A">First object to compare.</param>
		/// <param name="B">Second object to compare.</param>
		/// <returns>True if the names represent the same object, false otherwise</returns>
		public static bool operator ==(FileSystemName A, FileSystemName B)
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
		public static bool operator !=(FileSystemName A, FileSystemName B)
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
			return (Obj is FileSystemName) && ((FileSystemName)Obj) == this;
		}

        public bool HasExtension(string Extension)
        {
            if(Extension.Length == 0)
            {
                return CanonicalName.IndexOf('.') == -1;
            }
            else if(Extension[0] == '.')
            {
                return CanonicalName.EndsWith(Extension, StringComparison.InvariantCultureIgnoreCase);
            }
            else
            {
                return CanonicalName.Length > Extension.Length && CanonicalName[CanonicalName.Length - Extension.Length] == '.' && CanonicalName.EndsWith(Extension, StringComparison.InvariantCultureIgnoreCase);
            }
        }

        /// <summary>
        /// Returns a hash code for this object
        /// </summary>
        /// <returns>Hash code for this object</returns>
        public override int GetHashCode()
		{
			return CanonicalName.GetHashCode();
		}

		/// <summary>
		/// Returns the display name
		/// </summary>
		/// <returns>The display name</returns>
		public override string ToString()
		{
			return DisplayName;
		}
	}
}
