using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace Tools.DotNETCommon
{
	public static class BinaryWriterExtensions
	{
		/// <summary>
		/// Writes an array to a binary writer.
		/// </summary>
		/// <typeparam name="T">The array element type</typeparam>
		/// <param name="Writer">Writer to serialize to</param>
		/// <param name="Items">Array of items</param>
		/// <param name="WriteElement">Delegate to call to serialize each element</param>
		public static void Write<T>(this BinaryWriter Writer, T[] Items, Action<BinaryWriter, T> WriteElement)
		{
			if(Items == null)
			{
				Writer.Write(-1);
			}
			else
			{
				Writer.Write(Items.Length);
				for(int Idx = 0; Idx < Items.Length; Idx++)
				{
					WriteElement(Writer, Items[Idx]);
				}
			}
		}

		/// <summary>
		/// Writes a list to a binary writer.
		/// </summary>
		/// <typeparam name="T">The list element type</typeparam>
		/// <param name="Writer">Writer to serialize to</param>
		/// <param name="Items">List of items</param>
		/// <param name="WriteElement">Delegate to call to serialize each element</param>
		public static void Write<T>(this BinaryWriter Writer, List<T> Items, Action<BinaryWriter, T> WriteElement)
		{
			if (Items == null)
			{
				Writer.Write(-1);
			}
			else
			{
				Writer.Write(Items.Count);
				for (int Idx = 0; Idx < Items.Count; Idx++)
				{
					WriteElement(Writer, Items[Idx]);
				}
			}
		}

		/// <summary>
		/// Writes a value of a specific type to a binary writer
		/// </summary>
		/// <param name="Writer">Writer for output data</param>
		/// <param name="FieldType">Type of value to write</param>
		/// <param name="Value">The value to output</param>
		public static void Write(this BinaryWriter Writer, Type FieldType, object Value)
		{
			if(FieldType == typeof(string))
			{
				Writer.Write((string)Value);
			}
			else if(FieldType == typeof(bool))
			{
				Writer.Write((bool)Value);
			}
			else if(FieldType == typeof(int))
			{
				Writer.Write((int)Value);
			}
			else if(FieldType == typeof(float))
			{
				Writer.Write((float)Value);
			}
			else if(FieldType == typeof(double))
			{
				Writer.Write((double)Value);
			}
			else if(FieldType.IsEnum)
			{
				Writer.Write((int)Value);
			}
			else if(FieldType == typeof(string[]))
			{
				Writer.Write((string[])Value, (x, y) => x.Write(y));
			}
			else
			{
				throw new Exception(String.Format("Unsupported type '{0}' for binary serialization", FieldType.Name));
			}
		}
	}
}
