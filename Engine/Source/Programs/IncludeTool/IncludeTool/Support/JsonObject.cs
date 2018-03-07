// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;

namespace IncludeTool.Support
{
	public class JsonParseException : Exception
	{
		public JsonParseException(string Format, params object[] Args)
			: base(String.Format(Format, Args))
		{
		}
	}

	public class JsonObject
	{
		Dictionary<string, object> RawObject;

		public JsonObject(Dictionary<string, object> InRawObject)
		{
			RawObject = new Dictionary<string, object>(InRawObject, StringComparer.InvariantCultureIgnoreCase);
		}

		public static JsonObject Read(string FileName)
		{
			string Text = File.ReadAllText(FileName);
			Dictionary<string, object> CaseSensitiveRawObject = (Dictionary<string, object>)fastJSON.JSON.Instance.Parse(Text);
			return new JsonObject(CaseSensitiveRawObject);
		}

		public static bool TryRead(string FileName, out JsonObject Result)
		{
			if (!File.Exists(FileName))
			{
				Result = null;
				return false;
			}

			try
			{
				Result = Read(FileName);
				return true;
			}
			catch (Exception)
			{
				Result = null;
				return false;
			}
		}

		public IEnumerable<string> KeyNames
		{
			get { return RawObject.Keys; }
		}

		public string GetStringField(string FieldName)
		{
			string StringValue;
			if (!TryGetStringField(FieldName, out StringValue))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", FieldName);
			}
			return StringValue;
		}

		public bool TryGetStringField(string FieldName, out string Result)
		{
			object RawValue;
			if (RawObject.TryGetValue(FieldName, out RawValue) && (RawValue is string))
			{
				Result = (string)RawValue;
				return true;
			}
			else
			{
				Result = null;
				return false;
			}
		}

		public string[] GetStringArrayField(string FieldName)
		{
			string[] StringValues;
			if (!TryGetStringArrayField(FieldName, out StringValues))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", FieldName);
			}
			return StringValues;
		}

		public bool TryGetStringArrayField(string FieldName, out string[] Result)
		{
			object RawValue;
			if (RawObject.TryGetValue(FieldName, out RawValue) && (RawValue is IEnumerable<object>) && ((IEnumerable<object>)RawValue).All(x => x is string))
			{
				Result = ((IEnumerable<object>)RawValue).Select(x => (string)x).ToArray();
				return true;
			}
			else
			{
				Result = null;
				return false;
			}
		}

		public bool GetBoolField(string FieldName)
		{
			bool BoolValue;
			if (!TryGetBoolField(FieldName, out BoolValue))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", FieldName);
			}
			return BoolValue;
		}

		public bool TryGetBoolField(string FieldName, out bool Result)
		{
			object RawValue;
			if (RawObject.TryGetValue(FieldName, out RawValue) && (RawValue is Boolean))
			{
				Result = (bool)RawValue;
				return true;
			}
			else
			{
				Result = false;
				return false;
			}
		}

		public int GetIntegerField(string FieldName)
		{
			int IntegerValue;
			if (!TryGetIntegerField(FieldName, out IntegerValue))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", FieldName);
			}
			return IntegerValue;
		}

		public bool TryGetIntegerField(string FieldName, out int Result)
		{
			object RawValue;
			if (!RawObject.TryGetValue(FieldName, out RawValue) || !int.TryParse(RawValue.ToString(), out Result))
			{
				Result = 0;
				return false;
			}
			return true;
		}

		public long[] GetIntegerArrayField(string FieldName)
		{
			long[] IntegerValues;
			if (!TryGetIntegerArrayField(FieldName, out IntegerValues))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", FieldName);
			}
			return IntegerValues;
		}

		public bool TryGetIntegerArrayField(string FieldName, out long[] Result)
		{
			object RawValue;
			if (RawObject.TryGetValue(FieldName, out RawValue) && (RawValue is IEnumerable<object>) && ((IEnumerable<object>)RawValue).All(x => x is long))
			{
				Result = ((IEnumerable<object>)RawValue).Select(x => (long)x).ToArray();
				return true;
			}
			else
			{
				Result = null;
				return false;
			}
		}

		public bool TryGetUnsignedIntegerField(string FieldName, out uint Result)
		{
			object RawValue;
			if (!RawObject.TryGetValue(FieldName, out RawValue) || !uint.TryParse(RawValue.ToString(), out Result))
			{
				Result = 0;
				return false;
			}
			return true;
		}

		public double GetDoubleField(string FieldName)
		{
			double DoubleValue;
			if (!TryGetDoubleField(FieldName, out DoubleValue))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", FieldName);
			}
			return DoubleValue;
		}

		public bool TryGetDoubleField(string FieldName, out double Result)
		{
			object RawValue;
			if (!RawObject.TryGetValue(FieldName, out RawValue) || !double.TryParse(RawValue.ToString(), out Result))
			{
				Result = 0.0;
				return false;
			}
			return true;
		}

		public T GetEnumField<T>(string FieldName) where T : struct
		{
			T EnumValue;
			if (!TryGetEnumField(FieldName, out EnumValue))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", FieldName);
			}
			return EnumValue;
		}

		public bool TryGetEnumField<T>(string FieldName, out T Result) where T : struct
		{
			string StringValue;
			if (!TryGetStringField(FieldName, out StringValue) || !Enum.TryParse<T>(StringValue, true, out Result))
			{
				Result = default(T);
				return false;
			}
			return true;
		}

		public bool TryGetEnumArrayField<T>(string FieldName, out T[] Result) where T : struct
		{
			string[] StringValues;
			if (!TryGetStringArrayField(FieldName, out StringValues))
			{
				Result = null;
				return false;
			}

			T[] EnumValues = new T[StringValues.Length];
			for (int Idx = 0; Idx < StringValues.Length; Idx++)
			{
				if (!Enum.TryParse<T>(StringValues[Idx], true, out EnumValues[Idx]))
				{
					Result = null;
					return false;
				}
			}

			Result = EnumValues;
			return true;
		}

		public JsonObject GetObjectField(string FieldName)
		{
			JsonObject Result;
			if (!TryGetObjectField(FieldName, out Result))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", FieldName);
			}
			return Result;
		}

		public bool TryGetObjectField(string FieldName, out JsonObject Result)
		{
			object RawValue;
			if (RawObject.TryGetValue(FieldName, out RawValue) && (RawValue is Dictionary<string, object>))
			{
				Result = new JsonObject((Dictionary<string, object>)RawValue);
				return true;
			}
			else
			{
				Result = null;
				return false;
			}
		}

		public JsonObject[] GetObjectArrayField(string FieldName)
		{
			JsonObject[] ObjectValues;
			if (!TryGetObjectArrayField(FieldName, out ObjectValues))
			{
				throw new JsonParseException("Missing or invalid '{0}' field", FieldName);
			}
			return ObjectValues;
		}

		public bool TryGetObjectArrayField(string FieldName, out JsonObject[] Result)
		{
			object RawValue;
			if (RawObject.TryGetValue(FieldName, out RawValue) && (RawValue is IEnumerable<object>) && ((IEnumerable<object>)RawValue).All(x => x is Dictionary<string, object>))
			{
				Result = ((IEnumerable<object>)RawValue).Select(x => new JsonObject((Dictionary<string, object>)x)).ToArray();
				return true;
			}
			else
			{
				Result = null;
				return false;
			}
		}
	}

}
