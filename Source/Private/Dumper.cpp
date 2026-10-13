#include "Public/Dumper.h"

bool Dumper::SetupHooks(PE Module)
{
    if (!Module.IsValid()) return false;

    PE::HookFunction(Module.FindExportedFunction(RegisterCompiledInInfoName), RegisterCompiledInInfo, nullptr);
    PE::HookFunction(Module.FindExportedFunction(ConstructUClassName), ConstructUClass, nullptr);
    PE::HookFunction(Module.FindExportedFunction(ConstructUScriptStructName), ConstructUScriptStruct, nullptr);
    PE::HookFunction(Module.FindExportedFunction(ConstructUEnumName), ConstructUEnum, nullptr);

    PE::HookFunction(Module.FindExportedFunction(SetSparseClassDataStructName), SetSparseClassDataStruct, nullptr);
    return true;
}

bool Dumper::DumpModule(PE Module)
{
    if (!Module.IsValid()) return false;

    Module.CallEntryPoint();
    HandleMissingStructTypes();

    return true;
}

void Dumper::HandleMissingStructTypes()
{
    for (auto Property : MissingStructTypes) {
        if (((Dumper::FStructPropertyParams*)Property)->ScriptStructFunc) {
            ((Dumper::FStructPropertyParams*)Property)->ScriptStructFunc(0); //must be 0
        }
        else {
            std::cout << "ScriptStructFunc is still null for property: " << Property->NameUTF8 << "\n";
        }
    }
}

void Dumper::PrintInfo()
{
    std::cout << "Num of classes found: " << ClassDataMap.size()
        << ", Num of structs found: " << StructDataMap.size()
        << ", Num of enums found: " << EnumDataMap.size() << "\n";
}

void Dumper::RegisterCompiledInInfo(const TCHAR* PackageName, const FClassRegisterCompiledInInfo* ClassInfo, size_t NumClassInfo, const FStructRegisterCompiledInInfo* StructInfo, size_t NumStructInfo, const FEnumRegisterCompiledInInfo* EnumInfo, size_t NumEnumInfo, const FPartialRegisterCompiledInInfo* PartialsInfo, size_t NumPartialsInfo)
{   
    if (std::wstring(PackageName).contains(L"_Verse")) {
        std::cout << "Skipping packages in: " << WStringToString(PackageName) << "\n";
        return;
    }

    for (int i = 0; i < NumClassInfo; i++) {
        if (ClassDataMap.find(ClassInfo[i].Register) != ClassDataMap.end())
            continue;
        
        //set the classdata here since we can't get the classname in the Construct Hook
        ClassDataMap.emplace(ClassInfo[i].Register, CompactClassData { WStringToString(ClassInfo[i].Name) });

        ClassInfo[i].Register(1); //must be 1
    }

    for (int i = 0; i < NumStructInfo; i++) {
        if (StructDataMap.find(WStringToString(StructInfo[i].Name)) != StructDataMap.end())
            continue;

        StructInfo[i].OuterRegister(0); //must be 0
    }

    for (int i = 0; i < NumEnumInfo; i++) {
        if (EnumDataMap.find(WStringToString(EnumInfo[i].Name)) != EnumDataMap.end())
            continue;

        EnumInfo[i].OuterRegister(0); //must be 0
    }
}

void Dumper::ConstructUClass(const char** OutClass, const FClassParams* Params)
{   
    auto ClassDataMapResult = ClassDataMap.find(Params->ClassNoRegisterFunc);
    if (ClassDataMapResult == ClassDataMap.end()) {
        std::cout << "Failed to find function in ClassDataMap";
        return;
    }


    CompactClassData& ClassData = ClassDataMap.find(Params->ClassNoRegisterFunc)->second;
    ClassData.DependencySingletonFunction = Params->NumDependencySingletons == 1 ? nullptr : (*Params->DependencySingletonFuncArray);

    *OutClass = ClassData.ClassName.c_str();

    if (Params->NumProperties) {
        ClassData.Properties = ParseProperties(Params->PropertyArray, Params->NumProperties);
    }
}

void Dumper::ConstructUScriptStruct(const char** OutStruct, const FStructParams* Params)
{
    *OutStruct = Params->NameUTF8;

    if (StructDataMap.find(Params->NameUTF8) != StructDataMap.end()) {
        return;
    }

    CompactStructData StructData;
    StructData.StructName = Params->NameUTF8;
    StructData.SuperFunc = Params->SuperFunc;
    StructData.Properties = ParseProperties(Params->PropertyArray, Params->NumProperties);

    StructDataMap.emplace(Params->NameUTF8, StructData);
}

void Dumper::ConstructUEnum(const char** OutEnum, const FEnumParams* Params)
{
    *OutEnum = Params->NameUTF8;

    if (EnumDataMap.find(Params->NameUTF8) != EnumDataMap.end()) {
        return;
    }

    CompactEnumData EnumData;
    EnumData.CppForm = Params->CppForm;
    EnumData.NumProperties = Params->NumEnumerators;
    EnumData.PropertyArray = Params->EnumeratorParams;

    EnumDataMap.emplace(Params->NameUTF8, EnumData);
}

void Dumper::SetSparseClassDataStruct() {}

std::vector<Dumper::CompactPropertyData> Dumper::ParseProperties(const FPropertyParamsBase* const* PropertyParams, int32 NumProperties)
{   
    std::vector<CompactPropertyData> Properties;

    while (NumProperties) 
    {   
        CompactPropertyData PropertyData;
        ParseProperty(PropertyData, PropertyParams, NumProperties);

        Properties.insert(Properties.begin(), PropertyData);
    }

    return Properties;
}

//credits to Dumper-7: https://github.com/Encryqed/Dumper-7/blob/dd8fe34df8283378a1055386eee195005a100924/Dumper/Generator/Private/Generators/MappingGenerator.cpp 
void Dumper::ParseProperty(CompactPropertyData& PropertyData, const FPropertyParamsBase* const* PropertyParams, int32& NumProperties)
{
    const FPropertyParamsBase* Property = PropertyParams[--NumProperties];

    PropertyData.PropertyName = Property->NameUTF8;
    PropertyData.PropertyFlags = Property->PropertyFlags;
    PropertyData.Flags = Property->Flags;
    PropertyData.ArrayDim = Property->ArrayDim;
    PropertyData.RawProperty = Property;

    switch (Property->Flags)
    {
    case EPropertyGenFlags::Map: {
        CompactPropertyData KeyProperty;
        ParseProperty(KeyProperty, PropertyParams, NumProperties);

        CompactPropertyData ValueProperty;
        ParseProperty(ValueProperty, PropertyParams, NumProperties);

        PropertyData.PropertyLinks.push_back(KeyProperty);
        PropertyData.PropertyLinks.push_back(ValueProperty);
        break;
    }
    case EPropertyGenFlags::Array:
    case EPropertyGenFlags::Set:
    case EPropertyGenFlags::Enum:
    case EPropertyGenFlags::Optional:
    case EPropertyGenFlags::VerseString:
    case EPropertyGenFlags::VerseLegacyValue:
    {
        CompactPropertyData KeyProperty;
        ParseProperty(KeyProperty, PropertyParams, NumProperties);

        PropertyData.PropertyLinks.push_back(KeyProperty);
        break;
    }
    case EPropertyGenFlags::Struct: {
        if (((Dumper::FStructPropertyParams*)Property)->ScriptStructFunc)
        {
            ((Dumper::FStructPropertyParams*)Property)->ScriptStructFunc(0);
        }
        else {
            MissingStructTypes.push_back(Property);
        }
        break;
    }
    default:
        break;
    }
}