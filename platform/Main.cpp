// Insaniquarium Co-op - entry point.
#include "WinFishApp.h"
#include "CoopSession.h"
#include "AssetLocator.h"
#include <SDL2/SDL_main.h>
#include <SexyAppFramework/SexyAppBase.h>
#include <cstdlib>
#include <string>
#include <vector>

using namespace Sexy;

int main(int argc, char* argv[])
{
	WinFishApp* aTheApp = new WinFishApp();

	// The framework rejects parameters it doesn't know, so hand it only its own.
	std::vector<char*> aFrameworkArgs;
	for (int i = 0; i < argc; i++)
	{
		std::string a = argv[i];
		if (i > 0 && (a == "-host" || a == "--host" || a == "-windowed" || a == "--windowed" || a.compare(0, 6, "-join=") == 0))
			continue;
		// Older macOS passes a process serial number to apps opened from Finder.
		if (i > 0 && a.compare(0, 5, "-psn_") == 0)
			continue;
		if (i > 0 && a == "-join")
		{
			i++;
			continue;
		}
		aFrameworkArgs.push_back(argv[i]);
	}
	aTheApp->SetArgs((int)aFrameworkArgs.size(), aFrameworkArgs.data());

	// Find the player's own Insaniquarium Deluxe files (Steam install, next to
	// the exe, or a folder they picked once). No game assets ship with the mod.
	std::string aResDir;
	const char* anEnvResDir = std::getenv("INSANIQ_RESDIR");
	if (anEnvResDir != nullptr)
		aResDir = anEnvResDir;
	else if (!Coop::LocateGameAssets(argc, argv, aResDir))
		return 1;
	aTheApp->mResourceDir = aResDir;
	Sexy::ChDir(aTheApp->mResourceDir);

	char* aPrefPath = SDL_GetPrefPath("PopCap", "InsaniquariumCoop");
	if (aPrefPath != nullptr)
	{
		aTheApp->mCustomSaveDir = aPrefPath;
		SDL_free(aPrefPath);
	}

	std::string aJoin;
	bool aHost = false, aWindowed = false;
	for (int i = 1; i < argc; i++)
	{
		const std::string anArg = argv[i];
		if (anArg.compare(0, 9, "-savedir=") == 0)
			aTheApp->mCustomSaveDir = anArg.substr(9);
		else if (anArg == "-host" || anArg == "--host")
			aHost = true;
		else if (anArg.compare(0, 6, "-join=") == 0)
			aJoin = anArg.substr(6);
		else if (anArg == "-join" && i + 1 < argc)
			aJoin = argv[++i];
		else if (anArg == "-windowed" || anArg == "--windowed")
			aWindowed = true;
	}

	Coop::ImportOriginalSaves(aTheApp->mCustomSaveDir, aResDir);

	extern bool gCoopForceWindowed;
	gCoopForceWindowed = aWindowed;
	aTheApp->Init();
	Coop::Session::Get().Init(aTheApp);
	if (aHost)
		Coop::Session::Get().QueueAutoHost();
	if (!aJoin.empty())
		Coop::Session::Get().QueueAutoJoin(aJoin);

	aTheApp->Start();
	Coop::Session::Get().Shutdown();
	aTheApp->Shutdown();
	delete aTheApp;
	return 0;
}
