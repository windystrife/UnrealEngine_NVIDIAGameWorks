using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Text;
using System.Threading.Tasks;
using Tools.DotNETCommon;
using UnrealBuildTool;

namespace UnrealBuildTool
{
	/// <summary>
	/// Stores parsed values from XML config files which can be applied to a configurable type. Can be serialized to disk in binary form as a cache.
	/// </summary>
	class XmlConfigData
	{
		/// <summary>
		/// The current cache serialization version
		/// </summary>
		const int SerializationVersion = 1;

		/// <summary>
		/// List of input files. Stored to allow checking cache validity.
		/// </summary>
		public FileReference[] InputFiles;

		/// <summary>
		/// Stores a mapping from type -> field -> value, with all the config values for configurable fields.
		/// </summary>
		public Dictionary<Type, KeyValuePair<FieldInfo, object>[]> TypeToValues;

		/// <summary>
		/// Constructor
		/// </summary>
		/// <param name="InputFiles"></param>
		/// <param name="TypeToValues"></param>
		public XmlConfigData(FileReference[] InputFiles, Dictionary<Type, KeyValuePair<FieldInfo, object>[]> TypeToValues)
		{
			this.InputFiles = InputFiles;
			this.TypeToValues = TypeToValues;
		}

		/// <summary>
		/// Attempts to read a previous block of config values from disk
		/// </summary>
		/// <param name="Location">The file to read from</param>
		/// <param name="Types">Array of valid types. Used to resolve serialized type names to concrete types.</param>
		/// <param name="Data">On success, receives the parsed data</param>
		/// <returns>True if the data was read and is valid</returns>
		public static bool TryRead(FileReference Location, IEnumerable<Type> Types, out XmlConfigData Data)
		{
			// Check the file exists first
			if(!FileReference.Exists(Location))
			{
				Data = null;
				return false;
			}

			// Read the cache from disk
			using (BinaryReader Reader = new BinaryReader(File.Open(Location.FullName, FileMode.Open, FileAccess.Read, FileShare.Read)))
			{
				// Check the serialization version matches
				if(Reader.ReadInt32() != SerializationVersion)
				{
					Data = null;
					return false;
				}

				// Read the input files
				FileReference[] InputFiles = Reader.ReadArray(x => x.ReadFileReference());

				// Read the types
				int NumTypes = Reader.ReadInt32();
				Dictionary<Type, KeyValuePair<FieldInfo, object>[]> TypeToValues = new Dictionary<Type, KeyValuePair<FieldInfo, object>[]>(NumTypes);
				for(int TypeIdx = 0; TypeIdx < NumTypes; TypeIdx++)
				{
					// Read the type name
					string TypeName = Reader.ReadString();

					// Try to find it in the list of configurable types
					Type Type = Types.FirstOrDefault(x => x.Name == TypeName);
					if(Type == null)
					{
						Data = null;
						return false;
					}

					// Read all the values
					KeyValuePair<FieldInfo, object>[] Values = new KeyValuePair<FieldInfo, object>[Reader.ReadInt32()];
					for(int ValueIdx = 0; ValueIdx < Values.Length; ValueIdx++)
					{
						string FieldName = Reader.ReadString();

						// Find the matching field on the output type
						FieldInfo Field = Type.GetField(FieldName);
						if(Field == null || Field.GetCustomAttribute<XmlConfigFileAttribute>() == null)
						{
							Data = null;
							return false;
						}

						// Try to parse the value and add it to the output array
						object Value = Reader.ReadObject(Field.FieldType);
						Values[ValueIdx] = new KeyValuePair<FieldInfo, object>(Field, Value);
					}

					// Add it to the type map
					TypeToValues.Add(Type, Values);
				}

				// Return the parsed data
				Data = new XmlConfigData(InputFiles.ToArray(), TypeToValues);
				return true;
			}
		}

		/// <summary>
		/// Writes the coalesced config hierarchy to disk
		/// </summary>
		/// <param name="Location">File to write to</param>
		public void Write(FileReference Location)
		{
			DirectoryReference.CreateDirectory(Location.Directory);
			using (BinaryWriter Writer = new BinaryWriter(File.Open(Location.FullName, FileMode.Create, FileAccess.Write, FileShare.Read)))
			{
				Writer.Write(SerializationVersion);

				// Save all the input files. The cache will not be valid if these change.
				Writer.Write(InputFiles, (x, y) => x.Write(y));

				// Write all the categories
				Writer.Write(TypeToValues.Count);
				foreach(KeyValuePair<Type, KeyValuePair<FieldInfo, object>[]> TypePair in TypeToValues)
				{
					Writer.Write(TypePair.Key.Name);
					Writer.Write(TypePair.Value.Length);
					foreach(KeyValuePair<FieldInfo, object> FieldPair in TypePair.Value)
					{
						Writer.Write(FieldPair.Key.Name);
						Writer.Write(FieldPair.Key.FieldType, FieldPair.Value);
					}
				}
			}
		}
	}
}
