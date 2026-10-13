#include "Public/Mappings.h"

void Mappings::GenerateMappings()
{	
	std::cout << "Generating mappings....\n";

	if (Dumper::EnumDataMap.size() + Dumper::ClassDataMap.size() + Dumper::StructDataMap.size() == 0) {
		std::cout << "Nothing to generate!\n";
		return;
	}

	for (auto [Name, EnumData] : Dumper::EnumDataMap)
	{
		GenerateEnum(Name, EnumData);
	}

	for (auto [Function, ClassData] : Dumper::ClassDataMap)
	{
		GenerateClassOrStructs(ClassData);
	}

	for (auto [Name, StructData] : Dumper::StructDataMap)
	{
		GenerateClassOrStructs(StructData);
	}

	std::stringstream DataStream;

	/* Write Name-count and names */
	WriteValue(DataStream, static_cast<uint32>(GNameMap.size()));
	DataStream << GNameStream.rdbuf();

	/* Write Enum-count and enums */
	WriteValue(DataStream, static_cast<uint32>(Dumper::EnumDataMap.size()));
	DataStream << GEnumStream.rdbuf();

	/* Write Struct-count and enums */
	WriteValue(DataStream, static_cast<uint32>(Dumper::ClassDataMap.size() + Dumper::StructDataMap.size()));
	DataStream << GStructStream.rdbuf();

	GenerateFileHeader(DataStream);
}

Mappings::EMappingsTypeFlags Mappings::GetMappingType(Dumper::CompactPropertyData Property)
{	
	switch ((Dumper::EPropertyGenFlags)((byte)Property.Flags & 0x3F)) //PropertyTypeMask
	{
	case Dumper::EPropertyGenFlags::Byte: return EMappingsTypeFlags::ByteProperty;
	case Dumper::EPropertyGenFlags::Int8: return EMappingsTypeFlags::Int8Property;
	case Dumper::EPropertyGenFlags::Int16: return EMappingsTypeFlags::Int16Property;
	case Dumper::EPropertyGenFlags::Int: return EMappingsTypeFlags::IntProperty;
	case Dumper::EPropertyGenFlags::Int64: return EMappingsTypeFlags::Int64Property;
	case Dumper::EPropertyGenFlags::UInt16: return EMappingsTypeFlags::UInt16Property;
	case Dumper::EPropertyGenFlags::UInt32: return EMappingsTypeFlags::UInt32Property;
	case Dumper::EPropertyGenFlags::UInt64: return EMappingsTypeFlags::UInt64Property;
	case Dumper::EPropertyGenFlags::Float: return EMappingsTypeFlags::FloatProperty;
	case Dumper::EPropertyGenFlags::Double: return EMappingsTypeFlags::DoubleProperty;
	case Dumper::EPropertyGenFlags::LargeWorldCoordinatesReal: return EMappingsTypeFlags::DoubleProperty;
	case Dumper::EPropertyGenFlags::Object: return EMappingsTypeFlags::ObjectProperty;
	case Dumper::EPropertyGenFlags::Class: return EMappingsTypeFlags::ObjectProperty; //shrugg
	case Dumper::EPropertyGenFlags::Name: return EMappingsTypeFlags::NameProperty;
	case Dumper::EPropertyGenFlags::Str: return EMappingsTypeFlags::StrProperty;
	case Dumper::EPropertyGenFlags::Text: return EMappingsTypeFlags::TextProperty;
	case Dumper::EPropertyGenFlags::Bool: return EMappingsTypeFlags::BoolProperty;
	case Dumper::EPropertyGenFlags::Struct: return EMappingsTypeFlags::StructProperty;
	case Dumper::EPropertyGenFlags::Array: return EMappingsTypeFlags::ArrayProperty;
	case Dumper::EPropertyGenFlags::WeakObject: return EMappingsTypeFlags::WeakObjectProperty;
	case Dumper::EPropertyGenFlags::LazyObject: return EMappingsTypeFlags::LazyObjectProperty;
	case Dumper::EPropertyGenFlags::SoftObject: return EMappingsTypeFlags::SoftObjectProperty;
	case Dumper::EPropertyGenFlags::SoftClass: return EMappingsTypeFlags::SoftObjectProperty; //shrugg
	case Dumper::EPropertyGenFlags::Map: return EMappingsTypeFlags::MapProperty;
	case Dumper::EPropertyGenFlags::Set: return EMappingsTypeFlags::SetProperty;
	case Dumper::EPropertyGenFlags::Enum: return EMappingsTypeFlags::EnumProperty;
	case Dumper::EPropertyGenFlags::Interface: return EMappingsTypeFlags::InterfaceProperty;
	case Dumper::EPropertyGenFlags::FieldPath: return EMappingsTypeFlags::FieldPathProperty;
	case Dumper::EPropertyGenFlags::Optional: return EMappingsTypeFlags::OptionalProperty;
	case Dumper::EPropertyGenFlags::SparseMulticastDelegate: return EMappingsTypeFlags::MulticastDelegateProperty;
	case Dumper::EPropertyGenFlags::Delegate: return EMappingsTypeFlags::DelegateProperty;
	case Dumper::EPropertyGenFlags::Utf8Str: return EMappingsTypeFlags::Utf8StrProperty;
	case Dumper::EPropertyGenFlags::AnsiStr: return EMappingsTypeFlags::AnsiStrProperty;
	case Dumper::EPropertyGenFlags::InlineMulticastDelegate: return EMappingsTypeFlags::MulticastInlineDelegateProperty;
	default:
		return EMappingsTypeFlags::Unknown;
		break;
	}
}

int32 Mappings::GetOrAddNameIndex(std::string Name)
{	
	auto result = GNameMap.find(Name);
	if (result != GNameMap.end()) {
		return result->second;
	}


	GNameMap.insert({ Name, (int32)GNameMap.size() });

	WriteValue(GNameStream, (int16)Name.size());
	GNameStream.write(Name.c_str(), Name.size());

	return (int32)GNameMap.size() - 1;
}

void Mappings::GenerateFileHeader(std::stringstream& DataStream)
{
	std::ofstream FileOutPut(FileName, std::ios::binary);

	/* Write 2bytes unsigned */
	WriteValue(FileOutPut, UsmapFileMagic);

	/* Version: ExplicitEnumValues, adds support for enums with explicit values to fix mismatches */
	WriteValue(FileOutPut, EUsmapVersion::ExplicitEnumValues);

	/* We're on 'ExplicitEnumValues' version, we need to write 'bool' (aka int32) bHasVersioning. (NoVersioning = false) -> no [int32 UE4Version, int32 UE5Version] and no [uint32 NetCL] */
	WriteValue(FileOutPut, static_cast<int32>(false));

	/* Write 'CompressionMethod' to the compression byte */
	WriteValue(FileOutPut, EUsmapCompressionMethod::None);

	/* Write compressed size */
	WriteValue(FileOutPut, static_cast<uint32>(DataStream.str().length()));

	/* Write uncompressed size */
	WriteValue(FileOutPut, static_cast<uint32>(DataStream.str().length()));

	/* Header is done, now write the payload to the file */
	FileOutPut.write(DataStream.str().c_str(), DataStream.str().length());
	FileOutPut.close();

	std::cout << "Wrote mappings file to: " << (std::filesystem::current_path() / FileName).string() << "\n";
}

void Mappings::GenerateClassOrStructs(Dumper::ClassStructWrapper Wrapper)
{
	WriteValue(GStructStream, GetOrAddNameIndex(Wrapper.GetStrippedName()));

	Dumper::ClassStructWrapper Super = Wrapper.GetSuper();
	if (Super.GetIsValid()) 
	{
		WriteValue(GStructStream, GetOrAddNameIndex(Super.GetStrippedName()));
	}
	else
	{
		WriteValue(GStructStream, (int32)-1);
	}

	std::vector<Dumper::CompactPropertyData> Properties = Wrapper.GetLocalProperties();

	uint16 PropertyCount = 0x0;
	uint16 SerializablePropertyCount = (uint16)Properties.size();

	for (int i = 0; i < Properties.size(); i++) {
		PropertyCount += Properties[i].ArrayDim;
	}

	WriteValue(GStructStream, PropertyCount);
	WriteValue(GStructStream, SerializablePropertyCount);

	uint16 Index = 0;

	for (int i = 0; i < Properties.size(); i++)
	{
		WriteValue(GStructStream, Index);
		WriteValue(GStructStream, (uint8)Properties[i].ArrayDim);

		WriteValue(GStructStream, GetOrAddNameIndex(Properties[i].PropertyName));

		GeneratePropertyType(Properties[i]);

		Index += Properties[i].ArrayDim;
	}
}

void Mappings::GenerateEnum(std::string EnumName, Dumper::CompactEnumData EnumData)
{
	WriteValue(GEnumStream, GetOrAddNameIndex(EnumName));
	WriteValue(GEnumStream, (uint16)EnumData.NumProperties);

	for (int i = 0; i < EnumData.NumProperties; i++)
	{	
		auto Property = EnumData.PropertyArray[i];

		WriteValue(GEnumStream, Property.Value);
		WriteValue(GEnumStream, GetOrAddNameIndex(StripEnumParam(EnumData.CppForm, Property.NameUTF8)));
	}
}

void Mappings::GeneratePropertyType(Dumper::CompactPropertyData Property)
{
	EMappingsTypeFlags MappingType = GetMappingType(Property);

	/* Serialize ByteProperty as an EnumProperty with 'UnderlayingType == uint8' if the inner enum is valid */
	const char* ByteEnumName = nullptr;

	if (MappingType == EMappingsTypeFlags::ByteProperty && !((byte)Property.Flags & (byte)Dumper::EPropertyGenFlags::NativeBool))
	{
		auto ByteEnumFunc = ((Dumper::FBytePropertyParams*)Property.RawProperty)->EnumFunc;
		if (ByteEnumFunc)
			ByteEnumName = (const char*)ByteEnumFunc(0); // must be 0 too like EnumProperty
	}
	const bool bIsFakeEnumProperty = ByteEnumName && Dumper::EnumDataMap.find(ByteEnumName) != Dumper::EnumDataMap.end();

	WriteValue(GStructStream, static_cast<uint8>(!bIsFakeEnumProperty ? MappingType : EMappingsTypeFlags::EnumProperty));

	/* Write ByteProperty as the fake EnumProperty's underlaying type */
	if (bIsFakeEnumProperty)
		WriteValue(GStructStream, static_cast<uint8>(EMappingsTypeFlags::ByteProperty));

	if (MappingType == EMappingsTypeFlags::EnumProperty)
	{	
		GeneratePropertyType(Property.PropertyLinks[0]);

		auto EnumFunction = ((Dumper::FEnumPropertyParams*)Property.RawProperty)->EnumFunc;
		if (!EnumFunction) {
			std::cout << "EnumFunction is null for property: " << Property.PropertyName << "\n";
			WriteValue(GStructStream, (int32)-1);
			return;
		}

		const char* EnumTypeName = (char*)EnumFunction(0); //must be 0
		if (Dumper::EnumDataMap.find(EnumTypeName) == Dumper::EnumDataMap.end()) {
			std::cout << "Failed to find EnumData for property: " << Property.PropertyName << " for enum: " << EnumTypeName << "\n"; //just incase
			WriteValue(GStructStream, (int32)-1);
			return;
		}

		WriteValue(GStructStream, GetOrAddNameIndex(EnumTypeName));
	}
	else if (bIsFakeEnumProperty)
	{
		WriteValue(GStructStream, GetOrAddNameIndex(ByteEnumName));
	}
	else if (MappingType == EMappingsTypeFlags::StructProperty)
	{	
		auto StructFunction = ((Dumper::FStructPropertyParams*)Property.RawProperty)->ScriptStructFunc;

		if (!StructFunction) {
			std::cout << "StructFunction is null for property: " << Property.PropertyName << "\n";
			WriteValue(GStructStream, (int32)-1);
			return;
		}

		const char* StructTypeName = (char*)StructFunction(0); //must be 0
		if (Dumper::StructDataMap.find(StructTypeName) == Dumper::StructDataMap.end()) {
			std::cout << "Failed to find StructData for property: " << Property.PropertyName << " for struct: " << StructTypeName << "\n"; //just incase
			WriteValue(GStructStream, (int32)-1);
			return;
		}

		WriteValue(GStructStream, GetOrAddNameIndex(StructTypeName));
		
	}
	else if (MappingType == EMappingsTypeFlags::SetProperty)
	{
		GeneratePropertyType(Property.PropertyLinks[0]);
	}
	else if (MappingType == EMappingsTypeFlags::ArrayProperty)
	{
		GeneratePropertyType(Property.PropertyLinks[0]);
	}
	else if (MappingType == EMappingsTypeFlags::OptionalProperty)
	{
		GeneratePropertyType(Property.PropertyLinks[0]);
	}
	else if (MappingType == EMappingsTypeFlags::MapProperty)
	{
		GeneratePropertyType(Property.PropertyLinks[0]);
		GeneratePropertyType(Property.PropertyLinks[1]);
	}
}