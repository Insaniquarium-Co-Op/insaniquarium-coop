// Insaniquarium Co-op - finds the player's own Insaniquarium Deluxe files.
//
// The mod ships no game content. We look, in order, at:
//   -resdir=<folder> on the command line
//   the folder remembered in InsaniquariumCoop.cfg (next to the exe on Windows,
//     in the save folder elsewhere, since a Mac app bundle must stay untouched)
//   the exe's own folder and its parents (mod unzipped into the game folder)
//   the folder scripts/get-game-files.sh downloads to (macOS / Linux)
//   every Steam library (libraryfolders.vdf under each Steam install)
//   common PopCap / retail install folders (Windows)
// and finally ask the player to point at the folder once.

#include "AssetLocator.h"

#include <SDL.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>
#include <system_error>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <stdio.h>
#endif

namespace fs = std::filesystem;

namespace Coop
{
	static const char* kGameFolderName = "Insaniquarium Deluxe";

	static fs::path U8Path(const std::string& s)
	{
#ifdef _WIN32
		int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, nullptr, 0);
		std::wstring w(n > 0 ? n - 1 : 0, L'\0');
		if (n > 1)
			MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &w[0], n);
		return fs::path(w);
#else
		return fs::path(s);
#endif
	}

	static std::string PathU8(const fs::path& p)
	{
#ifdef _WIN32
		std::wstring w = p.wstring();
		int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, nullptr, 0, nullptr, nullptr);
		std::string s(n > 0 ? n - 1 : 0, '\0');
		if (n > 1)
			WideCharToMultiByte(CP_UTF8, 0, w.c_str(), -1, &s[0], n, nullptr, nullptr);
		return s;
#else
		return p.string();
#endif
	}

	bool IsGameFolder(const std::string& theDir)
	{
		std::error_code ec;
		fs::path p = U8Path(theDir);
		return fs::exists(p / "properties" / "resources.xml", ec) && fs::is_directory(p / "images", ec) && fs::is_directory(p / "sounds", ec);
	}

	static std::string WithSlash(std::string s)
	{
		if (!s.empty() && s.back() != '/' && s.back() != '\\')
			s += '/';
		return s;
	}

	static std::string ExeDir()
	{
		char* aBase = SDL_GetBasePath();
		std::string s = aBase ? aBase : "./";
		if (aBase)
			SDL_free(aBase);
		return WithSlash(s);
	}

	static std::string ConfigPath()
	{
#ifdef _WIN32
		return ExeDir() + "InsaniquariumCoop.cfg";
#else
		char* aPref = SDL_GetPrefPath("PopCap", "InsaniquariumCoop");
		std::string s = aPref ? aPref : ExeDir();
		if (aPref)
			SDL_free(aPref);
		return WithSlash(s) + "InsaniquariumCoop.cfg";
#endif
	}

	// Where scripts/get-game-files.sh puts the files it downloads from Steam.
	static std::string DownloadedGameDir()
	{
		const char* aHome = getenv("HOME");
		if (aHome == nullptr)
			return std::string();
#if defined(__APPLE__)
		return std::string(aHome) + "/Library/Application Support/InsaniquariumCoop/Game";
#elif defined(_WIN32)
		return std::string();
#else
		const char* aData = getenv("XDG_DATA_HOME");
		return (aData && *aData ? std::string(aData) : std::string(aHome) + "/.local/share") + "/InsaniquariumCoop/Game";
#endif
	}

	// Steam libraries listed in <steam>/steamapps/libraryfolders.vdf, plus each root itself.
	static std::vector<std::string> LibrariesFromSteamRoots(const std::vector<std::string>& theRoots)
	{
		std::vector<std::string> aLibs;
		for (const std::string& aRoot : theRoots)
		{
			aLibs.push_back(aRoot);
			// libraryfolders.vdf: lines like   "path"		"D:\\SteamLibrary"
			std::ifstream f(U8Path(aRoot + "/steamapps/libraryfolders.vdf"));
			std::string aLine;
			while (std::getline(f, aLine))
			{
				size_t k = aLine.find("\"path\"");
				if (k == std::string::npos)
					continue;
				size_t q1 = aLine.find('"', k + 6);
				size_t q2 = q1 == std::string::npos ? q1 : aLine.find('"', q1 + 1);
				if (q1 == std::string::npos || q2 == std::string::npos)
					continue;
				std::string p = aLine.substr(q1 + 1, q2 - q1 - 1);
				std::string aClean;
				for (size_t i = 0; i < p.size(); i++)
				{
					if (p[i] == '\\' && i + 1 < p.size() && p[i + 1] == '\\')
						i++;
					aClean += p[i];
				}
				aLibs.push_back(aClean);
			}
		}
		return aLibs;
	}

	static std::string ReadConfigGameDir()
	{
		std::ifstream f(U8Path(ConfigPath()));
		std::string aLine;
		while (std::getline(f, aLine))
		{
			if (aLine.rfind("gamedir=", 0) == 0)
			{
				std::string v = aLine.substr(8);
				while (!v.empty() && (v.back() == '\r' || v.back() == '\n' || v.back() == ' '))
					v.pop_back();
				return v;
			}
		}
		return std::string();
	}

	static void WriteConfigGameDir(const std::string& theDir)
	{
		std::ofstream f(U8Path(ConfigPath()));
		if (f)
			f << "# Insaniquarium Co-op: where your Insaniquarium Deluxe game files are.\n" << "gamedir=" << theDir << "\n";
	}

#ifdef _WIN32
	static std::string RegString(HKEY theRoot, const char* theKey, const char* theValue)
	{
		char aBuf[1024];
		DWORD aSize = sizeof(aBuf);
		DWORD aType = 0;
		HKEY k;
		if (RegOpenKeyExA(theRoot, theKey, 0, KEY_READ | KEY_WOW64_32KEY, &k) != ERROR_SUCCESS)
			return std::string();
		LONG r = RegQueryValueExA(k, theValue, nullptr, &aType, (LPBYTE)aBuf, &aSize);
		RegCloseKey(k);
		if (r != ERROR_SUCCESS || (aType != REG_SZ && aType != REG_EXPAND_SZ) || aSize == 0)
			return std::string();
		aBuf[std::min<DWORD>(aSize, sizeof(aBuf) - 1)] = 0;
		return aBuf;
	}

	static std::string KnownFolder(REFKNOWNFOLDERID theId)
	{
		PWSTR aPath = nullptr;
		std::string s;
		if (SUCCEEDED(SHGetKnownFolderPath(theId, 0, nullptr, &aPath)) && aPath)
			s = PathU8(fs::path(aPath));
		if (aPath)
			CoTaskMemFree(aPath);
		return s;
	}

	static std::vector<std::string> SteamLibraries()
	{
		std::vector<std::string> aLibs;
		std::vector<std::string> aSteamRoots;
		std::string a = RegString(HKEY_CURRENT_USER, "Software\\Valve\\Steam", "SteamPath");
		if (!a.empty()) aSteamRoots.push_back(a);
		a = RegString(HKEY_LOCAL_MACHINE, "SOFTWARE\\Valve\\Steam", "InstallPath");
		if (!a.empty()) aSteamRoots.push_back(a);
		a = RegString(HKEY_LOCAL_MACHINE, "SOFTWARE\\WOW6432Node\\Valve\\Steam", "InstallPath");
		if (!a.empty()) aSteamRoots.push_back(a);
		std::string aPF86 = KnownFolder(FOLDERID_ProgramFilesX86);
		if (!aPF86.empty()) aSteamRoots.push_back(aPF86 + "\\Steam");
		std::string aPF = KnownFolder(FOLDERID_ProgramFiles);
		if (!aPF.empty()) aSteamRoots.push_back(aPF + "\\Steam");

		aLibs = LibrariesFromSteamRoots(aSteamRoots);
		return aLibs;
	}

	static std::string PickFolderDialog()
	{
		HRESULT aCo = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
		BROWSEINFOW bi;
		memset(&bi, 0, sizeof(bi));
		bi.lpszTitle = L"Where is Insaniquarium Deluxe installed?\n(the folder that contains 'images', 'sounds' and 'properties')";
		bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
		PIDLIST_ABSOLUTE aList = SHBrowseForFolderW(&bi);
		std::string aResult;
		if (aList != nullptr)
		{
			wchar_t aPath[MAX_PATH * 2];
			if (SHGetPathFromIDListW(aList, aPath))
				aResult = PathU8(fs::path(aPath));
			CoTaskMemFree(aList);
		}
		if (SUCCEEDED(aCo))
			CoUninitialize();
		return aResult;
	}
#endif

#ifdef __APPLE__
	// Runs an AppleScript via osascript and returns what it printed (trimmed);
	// empty when the user cancels.
	static std::string RunAppleScript(const std::string& theScript)
	{
		std::string aQuoted = "'";
		for (char c : theScript)
		{
			if (c == '\'')
				aQuoted += "'\\''";
			else
				aQuoted += c;
		}
		aQuoted += "'";
		FILE* p = popen(("/usr/bin/osascript -e " + aQuoted + " 2>/dev/null").c_str(), "r");
		if (p == nullptr)
			return std::string();
		std::string anOut;
		char aBuf[512];
		while (fgets(aBuf, sizeof(aBuf), p) != nullptr)
			anOut += aBuf;
		pclose(p);
		while (!anOut.empty() && (anOut.back() == '\n' || anOut.back() == '\r'))
			anOut.pop_back();
		return anOut;
	}
#endif

	bool LocateGameAssets(int argc, char** argv, std::string& theOutDir)
	{
		std::vector<std::string> aCandidates;
		for (int i = 1; i < argc; i++)
		{
			std::string a = argv[i];
			if (a.rfind("-resdir=", 0) == 0)
				aCandidates.push_back(a.substr(8));
		}
		std::string aCfg = ReadConfigGameDir();
		if (!aCfg.empty())
			aCandidates.push_back(aCfg);

		std::string anExe = ExeDir();
		fs::path p = U8Path(anExe);
		aCandidates.push_back(anExe);
		for (int i = 0; i < 3 && p.has_parent_path() && p.parent_path() != p; i++)
		{
			p = p.parent_path();
			if (p.filename().empty())
				p = p.parent_path();
			aCandidates.push_back(PathU8(p));
			aCandidates.push_back(PathU8(p / kGameFolderName));
		}

#ifdef _WIN32
		for (const std::string& aLib : SteamLibraries())
			aCandidates.push_back(aLib + "/steamapps/common/" + kGameFolderName);
		std::string aPF86 = KnownFolder(FOLDERID_ProgramFilesX86);
		std::string aPF = KnownFolder(FOLDERID_ProgramFiles);
		for (const std::string& aBase : { aPF86, aPF })
		{
			if (aBase.empty())
				continue;
			aCandidates.push_back(aBase + "/PopCap Games/Insaniquarium Deluxe");
			aCandidates.push_back(aBase + "/PopCap Games/Insaniquarium");
			aCandidates.push_back(aBase + "/Insaniquarium Deluxe");
		}
#else
		aCandidates.push_back(DownloadedGameDir());
		aCandidates.push_back(anExe + "game");	// a copy placed inside the app bundle's Resources
		const char* aHome = getenv("HOME");
		if (aHome)
		{
			std::string h = aHome;
			std::vector<std::string> aRoots = {
#ifdef __APPLE__
				h + "/Library/Application Support/Steam",
#else
				h + "/.steam/steam",
				h + "/.local/share/Steam",
				h + "/.var/app/com.valvesoftware.Steam/.local/share/Steam",
#endif
			};
			for (const std::string& aLib : LibrariesFromSteamRoots(aRoots))
				aCandidates.push_back(aLib + "/steamapps/common/" + kGameFolderName);
		}
#endif

		for (const std::string& c : aCandidates)
		{
			if (!c.empty() && IsGameFolder(c))
			{
				theOutDir = WithSlash(c);
				return true;
			}
		}

#ifdef _WIN32
		MessageBoxA(nullptr,
			"Insaniquarium Co-op needs your own copy of Insaniquarium Deluxe (Steam or PopCap).\n\n"
			"It couldn't find it automatically. In the next window, pick the game's folder: the one "
			"that contains the 'images', 'sounds' and 'properties' folders.\n\n"
			"On Steam: right-click Insaniquarium Deluxe > Manage > Browse local files.",
			"Insaniquarium Co-op", MB_OK | MB_ICONINFORMATION);
		for (int aTry = 0; aTry < 3; aTry++)
		{
			std::string aPicked = PickFolderDialog();
			if (aPicked.empty())
				return false;
			if (IsGameFolder(aPicked))
			{
				WriteConfigGameDir(aPicked);
				theOutDir = WithSlash(aPicked);
				return true;
			}
			if (IsGameFolder(aPicked + "/" + kGameFolderName))
			{
				aPicked += std::string("/") + kGameFolderName;
				WriteConfigGameDir(aPicked);
				theOutDir = WithSlash(aPicked);
				return true;
			}
			MessageBoxA(nullptr, "That folder doesn't look like Insaniquarium Deluxe (no 'images' / 'properties' folders inside). Please try again.", "Insaniquarium Co-op", MB_OK | MB_ICONWARNING);
		}
		return false;
#elif defined(__APPLE__)
		// No Cocoa code needed: AppleScript shows the dialogs.
		std::string aChoice = RunAppleScript(
			"display dialog \"Insaniquarium Co-op needs your own copy of Insaniquarium Deluxe.\\n\\n"
			"To download it from Steam, double-click 'Get Game Files' (next to the app in the download), "
			"or run scripts/get-game-files.sh from the source folder. Then open the game again.\\n\\n"
			"Already have the game files (e.g. copied from a Windows PC)? Choose their folder: the one "
			"with 'images', 'sounds' and 'properties' inside.\" "
			"with title \"Insaniquarium Co-op\" buttons {\"Quit\", \"Choose Folder...\"} default button 2");
		if (aChoice.find("Choose") == std::string::npos)
			return false;
		for (int aTry = 0; aTry < 3; aTry++)
		{
			std::string aPicked = RunAppleScript("POSIX path of (choose folder with prompt \"Pick the Insaniquarium Deluxe folder (it contains 'images', 'sounds' and 'properties')\")");
			if (aPicked.empty())
				return false;
			for (const std::string& c : { aPicked, WithSlash(aPicked) + kGameFolderName })
			{
				if (IsGameFolder(c))
				{
					WriteConfigGameDir(c);
					theOutDir = WithSlash(c);
					return true;
				}
			}
			RunAppleScript("display dialog \"That folder doesn't look like Insaniquarium Deluxe (no 'images' or 'properties' folders inside). Please try again.\" "
				"with title \"Insaniquarium Co-op\" buttons {\"OK\"} default button 1");
		}
		return false;
#else
		fprintf(stderr, "Insaniquarium Co-op: game files not found. Run scripts/get-game-files.sh, or start with -resdir=<Insaniquarium Deluxe folder> or INSANIQ_RESDIR.\n");
		return false;
#endif
	}

	// Brings over progress from an original Windows install the first time the
	// mod runs, so nobody has to replay Tank 1. Never overwrites existing saves.
	void ImportOriginalSaves(const std::string& theSaveDir, const std::string& theGameDir)
	{
		std::error_code ec;
		fs::path aDest = U8Path(theSaveDir) / "userdata";
		if (fs::exists(aDest / "users.dat", ec))
			return;

		std::vector<std::string> aSources;
		aSources.push_back(theGameDir + "userdata");
#ifdef _WIN32
		std::string aPD = KnownFolder(FOLDERID_ProgramData);
		if (!aPD.empty())
		{
			aSources.push_back(aPD + "/Steam/Insaniquarium/userdata");
			aSources.push_back(aPD + "/Steam/Insaniquarium Deluxe/userdata");
			aSources.push_back(aPD + "/PopCap Games/Insaniquarium/userdata");
			aSources.push_back(aPD + "/PopCap Games/Insaniquarium Deluxe/userdata");
		}
		std::string aLocal = KnownFolder(FOLDERID_LocalAppData);
		if (!aLocal.empty())
		{
			aSources.push_back(aLocal + "/VirtualStore/Program Files (x86)/Steam/steamapps/common/Insaniquarium Deluxe/userdata");
			aSources.push_back(aLocal + "/VirtualStore/ProgramData/Steam/Insaniquarium/userdata");
		}
#elif defined(__APPLE__)
		// The insaniquarium-mac port (github.com/Jake-Vellora/insaniquarium-mac).
		const char* aHome = getenv("HOME");
		if (aHome != nullptr)
			aSources.push_back(std::string(aHome) + "/Library/Application Support/PopCap/Insaniquarium/userdata");
#endif
		for (const std::string& s : aSources)
		{
			fs::path aSrc = U8Path(s);
			if (!fs::exists(aSrc / "users.dat", ec))
				continue;
			fs::create_directories(aDest, ec);
			int aCopied = 0;
			for (const auto& e : fs::directory_iterator(aSrc, ec))
			{
				if (!e.is_regular_file(ec))
					continue;
				if (e.path().extension() != ".dat")
					continue;
				fs::copy_file(e.path(), aDest / e.path().filename(), fs::copy_options::skip_existing, ec);
				aCopied++;
			}
			fprintf(stderr, "Insaniquarium Co-op: imported %d save files from %s\n", aCopied, s.c_str());
			return;
		}
	}
}
