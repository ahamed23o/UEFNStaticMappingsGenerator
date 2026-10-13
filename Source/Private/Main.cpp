#include "Public/Mappings.h"

static const wchar_t* EngineModuleName = L"UnrealEditorFortnite-Engine-Win64-Shipping.dll";
static const wchar_t* CommonModuleName = L"UnrealEditorFortnite-Common-Win64-Shipping.dll";

int main(int argc, char** argv) 
{	
	std::string GameFolderPath = "";

#if _DEBUG
	GameFolderPath = "C:\\Program Files\\Epic Games\\Fortnite\\FortniteGame\\Binaries\\Win64\\";
#else
	if (argc >= 2) {
		if (std::filesystem::is_directory(argv[1])) {
			GameFolderPath = argv[1];
		}
		else {
			std::cout << "Provided argument \"" << argv[1] << "\" does not exists or is not a directory!\n falling back to exe directory!\n";
		}
	}

	if (GameFolderPath.empty()) {
		GameFolderPath = std::filesystem::current_path().string().c_str();
	}

	std::cout << "Using \"" << GameFolderPath << "\" as the GameFolderPath!\n";
#endif

	//save the currentPath For later
	const std::string CurrentExePath = std::filesystem::current_path().string();

	//needs to be set to be able to load imports
	std::filesystem::current_path(GameFolderPath);

	PE EngineModule(EngineModuleName);
	PE CommonModule(CommonModuleName);

	Dumper::SetupHooks(EngineModule);

	Dumper::DumpModule(EngineModule);
	Dumper::DumpModule(CommonModule);

	Dumper::PrintInfo();

	//Go Back to the exe Directory
	std::filesystem::current_path(CurrentExePath);

	Mappings Mapping("Mappings.usmap");
	Mapping.GenerateMappings();

	std::cout << "Press enter to close.";
	std::cin.get();
	return 0;
}