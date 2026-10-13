#pragma once
#include <sstream>
#include <fstream>
#include <filesystem>
#include "Dumper.h"
#include "zstd.h"

//credits to Dumper-7: https://github.com/Encryqed/Dumper-7/blob/dd8fe34df8283378a1055386eee195005a100924/Dumper/Generator/Private/Generators/MappingGenerator.cpp 
class Mappings
{
public:
	Mappings(std::string OutPutFileName)
		: FileName(OutPutFileName) {};

	void GenerateMappings();

private:
	static constexpr uint16 UsmapFileMagic = 0x30C4;

	enum class EMappingsTypeFlags : uint8
	{
		ByteProperty,
		BoolProperty,
		IntProperty,
		FloatProperty,
		ObjectProperty,
		NameProperty,
		DelegateProperty,
		DoubleProperty,
		ArrayProperty,
		StructProperty,
		StrProperty,
		TextProperty,
		InterfaceProperty,
		MulticastDelegateProperty,
		WeakObjectProperty, //
		LazyObjectProperty, // When deserialized, these 3 properties will be SoftObjects
		AssetObjectProperty, //
		SoftObjectProperty,
		UInt64Property,
		UInt32Property,
		UInt16Property,
		Int64Property,
		Int16Property,
		Int8Property,
		MapProperty,
		SetProperty,
		EnumProperty,
		FieldPathProperty,
		OptionalProperty, // Last property for which support was added
		Utf8StrProperty,
		AnsiStrProperty,

		ClassProperty,
		MulticastInlineDelegateProperty,
		SoftClassProperty,
		VerseStringProperty,
		VerseDynamicProperty,
		VerseFunctionProperty,

		Unknown = 0xFF
	};

	enum class EUsmapVersion : uint8
	{
		/* Initial format. */
		Initial,

		/* Adds package versioning to aid with compatibility */
		PackageVersioning,

		/* Adds support for 16-bit wide name-lengths (ushort/uint16) */
		LongFName,

		/* Adds support for enums with more than 255 values */
		LargeEnums,

		/* Adds support for explicit enum values */
		ExplicitEnumValues,

		Latest,
		LatestPlusOne,
	};

	enum class EUsmapCompressionMethod : uint8
	{
		None,
		Oodle,
		Brotli,
		ZStandard,
		Unknown = 0xFF
	};

	static const EUsmapCompressionMethod CompressionMethod = EUsmapCompressionMethod::ZStandard;
	
	EMappingsTypeFlags GetMappingType(Dumper::CompactPropertyData Property);

private:
	int32 GetOrAddNameIndex(std::string Name);

	template <typename T, typename U>
	void WriteValue(T& Stream, U value)
	{
		Stream.write((char*)&value, sizeof(value));
	}

	void GenerateFileHeader(std::stringstream& DataStream);
	bool GenerateCompressedBuffer(const std::stringstream& DataStream, void*& Buffer, uint32& CompressedSize);

	void GenerateClassOrStructs(Dumper::ClassStructWrapper Wrapper);
	void GenerateEnum(std::string EnumName, Dumper::CompactEnumData EnumData);

	void GeneratePropertyType(Dumper::CompactPropertyData Property);

	std::string StripEnumParam(Dumper::ECppForm CppForm, std::string EnumParam) {
		if (CppForm == Dumper::ECppForm::Regular)
			return EnumParam;

		auto Result = EnumParam.find("::");
		if (Result == std::string::npos) {
			std::cout << "Failed to strip enum prefix for: " << EnumParam << "\n";
			return EnumParam;
		}

		return EnumParam.substr(Result + 2);
	}

	std::unordered_map<std::string, int32> GNameMap;
	std::stringstream GNameStream;
	std::stringstream GStructStream;
	std::stringstream GEnumStream;
	std::string FileName;
};
