#
# GDB Printers for the Unreal Engine 4
#
# How to install:
# If the file ~/.gdbinit doesn't exist 
#        touch ~/.gdbinit
#        open ~/.gdbinit
#
# and add the following lines:
#   python
#   import sys 
#   ...
#   sys.path.insert(0, '/Path/To/Epic/UE4/Engine/Extras/GDBPrinters')      <--
#   ...
#   from UE4Printers import register_ue4_printers                          <--
#   register_ue4_printers (None)                                           <--
#   ...
#   end


import gdb
import itertools
import re
import sys

# ------------------------------------------------------------------------------
# We make our own base of the iterator to prevent issues between Python 2/3.
#

if sys.version_info[0] == 3:
	Iterator = object
else:
	class Iterator(object):

		def next(self):
			return type(self).__next__(self)

def default_iterator(val):
	for field in val.type.fields():
		yield field.name, val[field.name]


# ------------------------------------------------------------------------------
#
#  Custom pretty printers.
#
#


# ------------------------------------------------------------------------------
# FBitReference
#
class FBitReferencePrinter:
	def __init__(self, typename, val):
		self.Value = val

	def to_string(self):
		self.Mask = self.Value['Mask']
		self.Data = self.Value['Data']
		return '\'%d\'' % (self.Data & self.Mask)

# ------------------------------------------------------------------------------
# TBitArray
#
class TBitArrayPrinter:
	"Print TBitArray"

	class _iterator(Iterator):
		def __init__(self, val):
			self.Value = val
			self.Counter = -1

			try:
				self.NumBits = self.Value['NumBits']
				if self.NumBits.is_optimized_out:
					self.NumBits = 0
				else:
					self.AllocatorInstance = self.Value['AllocatorInstance']
					self.InlineData = self.AllocatorInstance['InlineData']
					self.SecondaryData = self.AllocatorInstance['SecondaryData']
					self.SecondaryDataData = self.AllocatorInstance['SecondaryData']['Data']
					if self.SecondaryData != None:
						self.SecondaryDataData = self.SecondaryData['Data']
			except:
				raise

		def __iter__(self):
			return self

		def __next__(self):
			if self.NumBits == 0:
				raise StopIteration

			self.Counter = self.Counter + 1

			if self.Counter >= self.NumBits:
				raise StopIteration

			if self.SecondaryDataData > 0:
				data = self.SecondaryDataData.cast(gdb.lookup_type("uint32").pointer())
			else:
				data = self.InlineData.cast(gdb.lookup_type("uint32").pointer())

			return ('[%d]' % self.Counter, (data[self.Counter/32] >> self.Counter) & 1)

	def __init__(self, typename, val):
		self.Value = val
		self.NumBits = self.Value['NumBits']

	def to_string(self):
		if self.NumBits.is_optimized_out:
			pass
		if self.NumBits == 0:
			return 'empty'
		pass

	def children(self):
		return self._iterator(self.Value)

	def display_hint(self):
		return 'array'
# ------------------------------------------------------------------------------
# TIndirectArray
#


# ------------------------------------------------------------------------------
# TChunkedArray
#
class TChunkedArrayPrinter:
	"Print TChunkedArray"

	class _iterator(Iterator):
		def __init__(self, val, typename):
			self.Value = val
			self.Typename = typename
			self.Counter = -1
			self.ElementType = self.Value.type.template_argument(0)
			self.ElementTypeSize = self.ElementType.sizeof

			try:
				self.NumElements = self.Value['NumElements']
				if self.NumElements.is_optimized_out:
					self.NumElements = 0
				else:
					self.Chunks = self.Value['Chunks']
					self.Array = self.Chunks['Array']
					self.ArrayNum = self.Array['ArrayNum']
					self.AllocatorInstance = self.Array['AllocatorInstance']
					self.AllocatorData = self.AllocatorInstance['Data']
			except:
				raise

		def __iter__(self):
			return self

		def __next__(self):
			return self.next()

		def __next__(self):
			if self.NumElements == 0:
				raise StopIteration

			self.Counter = self.Counter + 1

			if self.Counter >= self.NumElements:
				raise StopIteration()

			Expr = '(unsigned)sizeof('+str(self.Typename)+'::FChunk)/'+str(self.ElementTypeSize)
			self.ChunkBytes = gdb.parse_and_eval(Expr)
			assert self.ChunkBytes != 0

			Expr = '*(*((('+str(self.ElementType.name)+'**)'+str(self.AllocatorData)+')+'+str(self.Counter / self.ChunkBytes)+')+'+str(self.Counter % self.ChunkBytes)+')'
			Val = gdb.parse_and_eval(Expr)
			return ('[%d]' % self.Counter, Val)

	def __init__(self, typename, val):
		self.Value = val
		self.Typename = typename
		self.NumElements = self.Value['NumElements']

	def to_string(self):
		if self.NumElements.is_optimized_out:
			pass
		if self.NumElements == 0:
			return 'empty'
		pass

	def children(self):
		return self._iterator(self.Value, self.Typename)

	def display_hint(self):
		return 'array'

# ------------------------------------------------------------------------------
# TSparseArray
#
class TSparseArrayPrinter:
	"Print TSparseArray"

	class _iterator(Iterator):
		def __init__(self, val, typename):

			self.Value = val
			self.Counter = -1
			self.Typename = typename
			self.ElementType = self.Value.type.template_argument(0)
			self.ElementTypeSize = self.ElementType.sizeof
			assert self.ElementTypeSize != 0

			try:
				self.NumFreeIndices = self.Value['NumFreeIndices']
				self.Data = self.Value['Data']
				self.ArrayNum = self.Data['ArrayNum']
				if self.ArrayNum.is_optimized_out:
					self.ArrayNum = 0
				else:
					self.AllocatorInstance = self.Data['AllocatorInstance']
					self.AllocatorData = self.AllocatorInstance['Data']
					self.AllocationFlags = self.Value['AllocationFlags']
					self.AllocationFlagsInstance = self.AllocationFlags['AllocatorInstance']
					self.InlineData = self.AllocationFlagsInstance['InlineData']
					self.SecondaryData = self.AllocationFlagsInstance['SecondaryData']
					self.SecondaryDataData = self.SecondaryData['Data']
			except:
				raise

		def __iter__(self):
			return self

		def __next__(self):
			return self.next()

		def __next__(self):
			if self.ArrayNum == 0:
				raise StopIteration

			self.Counter = self.Counter + 1

			if self.Counter >= self.ArrayNum:
				raise StopIteration
			else:
				Data = None
				if self.SecondaryDataData > 0:
					Data = (self.SecondaryDataData.address.cast(gdb.lookup_type("int").pointer())[self.Counter/32] >> self.Counter) & 1
				else:
					Data = (self.InlineData.address.cast(gdb.lookup_type("int").pointer())[self.Counter/32] >> self.Counter) & 1

				Value = None
				if Data != 0:
					offset = self.Counter * self.ElementTypeSize
					Value = (self.AllocatorData + offset).cast(self.ElementType.pointer())
				else:
					Value = None

				return ('[%s]' % self.Counter, Value.dereference())

	def __init__(self, typename, val):
		self.Value = val
		self.Typename = typename
		self.ArrayNum = self.Value['Data']['ArrayNum']

	def to_string(self):
		if self.ArrayNum.is_optimized_out:
			pass
		if self.ArrayNum == 0:
			return 'empty'
		pass

	def children(self):
		return self._iterator(self.Value, self.Typename)

	def display_hint(self):
		return 'array'

# ------------------------------------------------------------------------------
# TSet
#
class TSetPrinter:
	"Print TSet"

	class _iterator(Iterator):
		def __init__(self, val, typename):
			self.Value = val
			self.Counter = -1
			self.typename = typename
			self.ElementType = self.Value.type.template_argument(0)

			try:
				self.Elements = self.Value["Elements"]
				self.ElementsData = self.Elements["Data"]
				self.ElementsArrayNum = self.ElementsData['ArrayNum']
				self.NumFreeIndices = self.Elements['NumFreeIndices']
			 
				self.AllocatorInstance = self.ElementsData['AllocatorInstance']
				self.AllocatorInstanceData = self.AllocatorInstance['Data']

				self.AllocationFlags = self.Elements['AllocationFlags']
				self.AllocationFlagsInstance = self.AllocationFlags['AllocatorInstance']
				self.InlineData = self.AllocationFlagsInstance['InlineData']
				self.SecondaryData = self.AllocationFlagsInstance['SecondaryData']
				self.SecondaryDataData = self.SecondaryData['Data']
			except:
				self.ElementsArrayNum = 0

			Expr = '(size_t)sizeof(FSetElementId) + sizeof(int32)'
			TSetElement = gdb.parse_and_eval(Expr)
			self.ElementTypeSize = self.ElementType.sizeof + TSetElement

		def __iter__(self):
			return self

		def __next__(self):
			self.Counter = self.Counter + 1

			if self.Counter >= self.ElementsArrayNum:
				raise StopIteration()
			else:
				Data = None
				if self.SecondaryDataData > 0:
					Data = (self.SecondaryDataData.address.cast(gdb.lookup_type("int").pointer())[self.Counter/32] >> self.Counter) & 1
				else:
					Data = (self.InlineData.address.cast(gdb.lookup_type("int").pointer())[self.Counter/32] >> self.Counter) & 1

				Value = None
				if Data != 0:
					offset = self.Counter * self.ElementTypeSize
					Value = (self.AllocatorInstanceData + offset).cast(self.ElementType.pointer())
				else:
					Value = None

			return ('[%s]' % self.Counter, Value.dereference())

	def __init__(self, typename, val):
		self.Value = val
		self.typename = typename
		self.ArrayNum = self.Value["Elements"]["Data"]['ArrayNum']

	def to_string(self):
		if self.ArrayNum.is_optimized_out:
			pass
		if self.ArrayNum == 0:
			return 'empty'
		pass

	def children(self):
		return self._iterator(self.Value, self.typename)

	def display_hint(self):
		return 'array'

# ------------------------------------------------------------------------------
# TSetElementPrinter
#

class TSetElementPrinter:
	"Print TSetElement"

	def __init__(self, typename, val):
		self.Value = val

	def to_string(self):
		if self.Value.is_optimized_out:
			return '<optimized out>'

		return self.Value["Value"]

	def display_hint(self):
		return "string"

# ------------------------------------------------------------------------------
# TMap
#
class TMapPrinter:
	"Print TMap"

	class _iterator(Iterator):
		def __init__(self, val):
			self.Value = val
			self.Counter = -1
			try:
				self.Pairs = self.Value['Pairs']
				if self.Pairs.is_optimized_out:
					self.ArrayNum = 0
				else:
					self.Elements = self.Pairs['Elements']
					self.ElementsData = self.Elements['Data']
					self.ArrayNum = self.ElementsData['ArrayNum']
			except:
				raise

		def __iter__(self):
			return self

		def __next__(self):
			if self.ArrayNum == 0:
				raise StopIteration

			self.Counter = self.Counter + 1

			if self.Counter > 0:
				raise StopIteration

			return ('Pairs', self.Pairs)

	def __init__(self, typename, val):
		self.Value = val
		self.ArrayNum = self.Value['Pairs']['Elements']['Data']['ArrayNum']

	def children(self):
		return self._iterator(self.Value)

	def to_string(self):
		if self.ArrayNum.is_optimized_out:
			pass
		if self.ArrayNum == 0:
			return 'empty'
		pass

	def display_hint(self):
		return 'map'

# ------------------------------------------------------------------------------
# TWeakObjectPtr
#
class TWeakObjectPtrPrinter:
	"Print TWeakObjectPtr"

	def __init__(self, typename, val):
		self.Value = val

	def to_string(self):
		self.ObjectSerialNumber = self.Value['ObjectSerialNumber']
		self.ObjectIndex = self.Value['ObjectIndex']
		self.ktype = self.Value.type.template_argument(0)

		if self.ObjectSerialNumber >= 1:
			Expr = 'GObjectArrayForDebugVisualizers->Objects['+str(self.ObjectIndex)+'].SerialNumber == '+str(self.ObjectSerialNumber)
			Value = gdb.parse_and_eval(Expr)
			if Value != 0:
				Expr = 'GObjectArrayForDebugVisualizers->Objects['+str(self.ObjectIndex)+'].Object'
				Value = gdb.parse_and_eval(Expr)
				return Value

			Expr = '(void*)0xDEADBEEF'
			Value = gdb.parse_and_eval(Expr)
			return Value

	def display_hint (self):
		return 'string'


# ------------------------------------------------------------------------------
# FString
#
class FStringPrinter:
	"Print FString"

	def __init__(self, typename, val):
		self.Value = val

	def to_string(self):
		if self.Value.is_optimized_out:
			return '<optimized out>'

		ArrayNum = self.Value['Data']['ArrayNum']
		if ArrayNum == 0:
			return 'empty'
		elif ArrayNum < 0:
			return "nullptr"
		else:
			ActualData = self.Value['Data']['AllocatorInstance']['Data']
			data = ActualData.cast(gdb.lookup_type("wchar_t").pointer())
			return '%s' % (data.string())

	def display_hint (self):
		return 'string'


# ------------------------------------------------------------------------------
# FNameEntry
#

class FNameEntryPrinter:
	"Print FNameEntry"

	def __init__(self, typename, val):
		self.Value = val

	def to_string(self):
		self.AnsiName = self.Value['AnsiName']
		return self.AnsiName


# ------------------------------------------------------------------------------
# FName
#
class FNamePrinter:
	"Print FName"

	def __init__(self, typename, val):
		self.Value = val

	def to_string(self):
		if self.Value.is_optimized_out:
			return '<optimized out>'

		DisplayIndex = self.Value['DisplayIndex']

		if not DisplayIndex:
			DisplayIndex = self.Value['ComparisonIndex']

		if DisplayIndex >= 1048576:
			return 'invalid'
		else:
			Expr = '(char*)(((FNameEntry***)FName::GetNameTableForDebuggerVisualizers_MT())['+str(DisplayIndex)+' / 16384]['+str(DisplayIndex)+' % 16384])->AnsiName'
			Value = gdb.parse_and_eval(Expr)
			return '%s' % (Value.string()) 

	def display_hint(self):
		return 'string'


# ------------------------------------------------------------------------------
# FMinimalName
#
class FMinimalNamePrinter:
	"Print FMinimalName"

	def __init__(self, typename, val):
		self.Value = val

	def to_string(self):
		Index = self.Value['Index']

		if Index >= 1048576:
			return 'invalid'
		else:
			Expr = '(char*)(((FNameEntry***)FName::GetNameTableForDebuggerVisualizers_MT())['+str(Index)+' / 16384]['+str(Index)+' % 16384])->AnsiName'
			Value = gdb.parse_and_eval(Expr)
			return '%s' % (Value) 

	def display_hint (self):
		return 'string'


# ------------------------------------------------------------------------------
# TTuple
#
class TTuplePrinter:
	"Print TTuple"

	def __init__(self, typename, val):
		self.Value = val

		try:
			self.TKey = self.Value["Key"];
			self.TValue = self.Value["Value"];
		except:
			pass

	def to_string(self):
		return '(%s, %s)' % (self.TKey, self.TValue)


# ------------------------------------------------------------------------------
# TArray
#
class TArrayPrinter:
	"Print TArray"

	class _iterator(Iterator):
		def __init__(self, val):
			self.Value = val
			self.Counter = -1
			self.TType = self.Value.type.template_argument(0)

			try:
				self.ArrayNum = self.Value['ArrayNum']
				if self.ArrayNum.is_optimized_out:
					self.ArrayNum = 0
				
				if self.ArrayNum > 0:
					self.AllocatorInstance = self.Value['AllocatorInstance']
					self.AllocatorInstanceData = self.AllocatorInstance['Data']
					try:
						self.InlineData = self.AllocatorInstance['InlineData']
						self.SecondaryData = self.AllocatorInstance['SecondaryData']
						if self.SecondaryData != None:
							self.SecondaryDataData = self.SecondaryData['Data']
						else:
							self.SecondaryDataData = None
					except:
						pass
			except:
				raise

		def __iter__(self):
			return self

		def __next__(self):
			if self.ArrayNum == 0:
				raise StopIteration

			self.Counter = self.Counter + 1

			if self.Counter >= self.ArrayNum:
				raise StopIteration

			try:
				if self.AllocatorInstanceData != None:
					data = self.AllocatorInstanceData.cast(self.TType.pointer())
				elif self.SecondaryDataDataVal > 0:
					data = self.SecondaryDataData.cast(self.TType.pointer())
				else:
					data = self.InlineData.cast(self.TType.pointer())
			except:
				return ('[%d]' % self.Counter, "optmized")

			return ('[%d]' % self.Counter, data[self.Counter])

	def __init__(self, typename, val):
		self.Value = val;
		self.ArrayNum = self.Value['ArrayNum']

	def to_string(self):
		if self.ArrayNum.is_optimized_out:
			pass
		if self.ArrayNum == 0:
			return 'empty'
		pass

	def children(self):
		return self._iterator(self.Value)

	def display_hint(self):
		return 'array'

#
# Register our lookup function. If no objfile is passed use all globally.
def register_ue4_printers(objfile):
	if objfile == None:
		objfile = gdb

	objfile.pretty_printers.append(lookup_function)


#
# We need this part which is a definition how pretty printers work.
#
def lookup_function (val):
	"Look-up and return a pretty-printer that can print val."

	# Get the type and check if it points to a reference. We check for both Object and Pointer type.
	type = val.type;
	if (type.code == gdb.TYPE_CODE_REF) or (type.code == gdb.TYPE_CODE_PTR):
		type = type.target ()

	# Get the unqualified type, mean remove const nor volatile and strippe of typedefs. 
	type = type.unqualified ().strip_typedefs ()

	# Get the tag name. The tag name is the name after struct, union, or enum in C and C++
	tag = type.tag
	if tag == None:
		return None

	for function in pretty_printers_dict:
		if function.search (tag):
			return pretty_printers_dict[function] (tag, val)

	# Cannot find a pretty printer.  Return None.
	return None

def build_dictionary ():
	pretty_printers_dict[re.compile('^FString$')] = lambda typename, val: FStringPrinter(typename, val)
	pretty_printers_dict[re.compile('^FNameEntry$')] = lambda typename, val: FNameEntryPrinter(typename, val)
	pretty_printers_dict[re.compile('^FName$')] = lambda typename, val: FNamePrinter(typename, val)
	pretty_printers_dict[re.compile('^FMinimalName$')] = lambda typename, val: FMinimalNamePrinter(typename, val)
	pretty_printers_dict[re.compile('^TArray<.+,.+>$')] = lambda typename, val: TArrayPrinter(typename, val)
	pretty_printers_dict[re.compile('^TBitArray<.+>$')] = lambda typename, val: TBitArrayPrinter(typename, val)
	pretty_printers_dict[re.compile('^TChunkedArray<.+>$')] = lambda typename, val: TChunkedArrayPrinter(typename, val)
	pretty_printers_dict[re.compile('^TSparseArray<.+>$')] = lambda typename, val: TSparseArrayPrinter(typename, val)
	pretty_printers_dict[re.compile('^TSetElement<.+>$')] = lambda typename, val: TSetElementPrinter(typename, val)
	pretty_printers_dict[re.compile('^TSet<.+>$')] = lambda typename, val: TSetPrinter(typename, val)
	pretty_printers_dict[re.compile('^FBitReference$')] = lambda typename, val: FBitReferencePrinter(typename, val)
	pretty_printers_dict[re.compile('^TMap<.+,.+,.+>$')] = lambda typename, val: TMapPrinter(typename, val)
	pretty_printers_dict[re.compile('^TPair<.+,.+>$')] = lambda typename, val: TTuplePrinter(typename, val)
	pretty_printers_dict[re.compile('^TTuple<.+,.+>$')] = lambda typename, val: TTuplePrinter(typename, val)
#	pretty_printers_dict[re.compile('^TWeakObjectPtr<.+>$')] = lambda typename, val: TWeakObjectPtrPrinter(typename, val)


pretty_printers_dict = {}
build_dictionary ()
