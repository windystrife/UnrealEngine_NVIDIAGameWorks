using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon
{
	public static class BinaryReaderExtensions
	{
		/// <summary>
		/// Read an array of objects from a binary reader
		/// </summary>
		/// <typeparam name="T">The element type for the array</typeparam>
		/// <param name="Reader">Reader to read data from</param>
		/// <param name="ReadElement">Delegate to call to serialize each element</param>
		/// <returns>Array of objects, as serialized. May be null.</returns>
		public static T[] ReadArray<T>(this BinaryReader Reader, Func<BinaryReader, T> ReadElement)
		{
			int NumItems = Reader.ReadInt32();
			if(NumItems < 0)
			{
				return null;
			}

			T[] Items = new T[NumItems];
			for(int Idx = 0; Idx < NumItems; Idx++)
			{
				Items[Idx] = ReadElement(Reader);
			}
			return Items;
		}

		/// <summary>
		/// Read a list of objects from a binary reader
		/// </summary>
		/// <typeparam name="T">The element type for the list</typeparam>
		/// <param name="Reader">Reader to read data from</param>
		/// <param name="ReadElement">Delegate to call to serialize each element</param>
		/// <returns>List of objects, as serialized. May be null.</returns>
		public static List<T> ReadList<T>(this BinaryReader Reader, Func<BinaryReader, T> ReadElement)
		{
			T[] Items = Reader.ReadArray<T>(ReadElement);
			return (Items == null)? null : new List<T>(Items);
		}

		/// <summary>
		/// Reads a value of a specific type from a binary reader
		/// </summary>
		/// <param name="Reader">Reader for input data</param>
		/// <param name="ObjectType">Type of value to read</param>
		/// <returns>The value read from the stream</returns>
		public static object ReadObject(this BinaryReader Reader, Type ObjectType)
		{
			if(ObjectType == typeof(string))
			{
				return Reader.ReadString();
			}
			else if(ObjectType == typeof(bool))
			{
				return Reader.ReadBoolean();
			}
			else if(ObjectType == typeof(int))
			{
				return Reader.ReadInt32();
			}
			else if(ObjectType == typeof(float))
			{
				return Reader.ReadSingle();
			}
			else if(ObjectType == typeof(double))
			{
				return Reader.ReadDouble();
			}
			else if(ObjectType == typeof(string[]))
			{
				return Reader.ReadArray(x => x.ReadString());
			}
			else if(ObjectType.IsEnum)
			{
				return Enum.ToObject(ObjectType, Reader.ReadInt32());
			}
			else
			{
				throw new Exception(String.Format("Reading binary objects of type '{0}' is not currently supported.", ObjectType.Name));
			}
		}
	}
}
