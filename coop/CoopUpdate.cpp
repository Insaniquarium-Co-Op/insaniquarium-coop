// Insaniquarium Co-op - "is there a newer version?" (check only; see CoopUpdate.h).

#include <SexyAppFramework/Font.h>
#include "CoopUpdate.h"
#include "CoopHeroes.h"
#include "CoopSession.h"
#include "MoneyDialog.h"
#include "WinFishApp.h"
#include "GameSelector.h"
#include "Res.h"
#include <SexyAppFramework/DialogButton.h>
#include <SDL.h>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <sstream>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#include <winhttp.h>
#endif

using namespace Sexy;

#ifndef COOP_UPDATE_URL
// The public repository's latest release (source only; see DECISIONS D27).
#define COOP_UPDATE_URL "https://api.github.com/repos/Insaniquarium-Co-Op/insaniquarium-coop/releases/latest"
#endif

namespace Coop
{
	static const int kDialogUpdate = 96;
	enum { CHECK_IDLE, CHECK_RUNNING, CHECK_DONE };
	static std::atomic<int> gState{ CHECK_IDLE };
	static std::mutex gLock;
	static std::string gLatest, gUrl, gError;	// under gLock
	static bool gShown = false;

	static WinFishApp* App() { return (WinFishApp*)gSexyAppBase; }

	static int gEnabled = -1;		// the saved setting, read once

	bool UpdateCheckEnabled()
	{
		if (gEnabled < 0 && App() != nullptr)
		{
			int anOn = 1;
			App()->RegistryReadInteger("CoopUpdateCheck", &anOn);
			gEnabled = anOn != 0 ? 1 : 0;
		}
		return gEnabled != 0;
	}

	void SetUpdateCheckEnabled(bool theOn)
	{
		gEnabled = theOn ? 1 : 0;
		if (App() != nullptr)
			App()->RegistryWriteInteger("CoopUpdateCheck", gEnabled);
	}

	static std::vector<int> VersionParts(const std::string& theVersion)
	{
		std::vector<int> aParts;
		size_t i = (!theVersion.empty() && (theVersion[0] == 'v' || theVersion[0] == 'V')) ? 1 : 0;
		while (i < theVersion.size() && isdigit((unsigned char)theVersion[i]))
		{
			int n = 0;
			while (i < theVersion.size() && isdigit((unsigned char)theVersion[i]))
				n = n * 10 + (theVersion[i++] - '0');
			aParts.push_back(n);
			if (i < theVersion.size() && theVersion[i] == '.')
				i++;
			else
				break;
		}
		return aParts;
	}

	int CompareVersions(const std::string& a, const std::string& b)
	{
		std::vector<int> x = VersionParts(a), y = VersionParts(b);
		for (size_t i = 0; i < std::max(x.size(), y.size()); i++)
		{
			int p = i < x.size() ? x[i] : 0, q = i < y.size() ? y[i] : 0;
			if (p != q)
				return p < q ? -1 : 1;
		}
		return 0;
	}

	// The string value of the first "key": "..." in the JSON (no escapes needed here).
	static std::string JsonString(const std::string& theJson, const std::string& theKey)
	{
		size_t k = theJson.find("\"" + theKey + "\"");
		if (k == std::string::npos)
			return "";
		size_t c = theJson.find(':', k);
		size_t q = c == std::string::npos ? c : theJson.find('"', c);
		if (q == std::string::npos)
			return "";
		size_t e = theJson.find('"', q + 1);
		return e == std::string::npos ? "" : theJson.substr(q + 1, e - q - 1);
	}

	bool ParseUpdateJson(const std::string& theJson, std::string& theVersion, std::string& theUrl)
	{
		// A manifest: {"version": "2.1.0", "url": "..."}; or GitHub's latest release
		// ("tag_name", and the release page is the first "html_url").
		theVersion = JsonString(theJson, "version");
		theUrl = JsonString(theJson, "url");
		if (theVersion.empty())
		{
			theVersion = JsonString(theJson, "tag_name");
			theUrl = JsonString(theJson, "html_url");
		}
		if (theVersion.empty() || VersionParts(theVersion).empty())
			return false;
		// Only ever open a web page.
		if (theUrl.rfind("https://", 0) != 0)
			theUrl.clear();
		return true;
	}

	static bool SafeUrl(const std::string& theUrl)
	{
		if (theUrl.rfind("https://", 0) != 0 && theUrl.rfind("http://", 0) != 0 && theUrl.rfind("file://", 0) != 0)
			return false;
		for (char c : theUrl)
			if (!(isalnum((unsigned char)c) || strchr(":/._-~?=&%+", c) != nullptr))
				return false;
		return true;
	}

#ifdef _WIN32
	static bool Fetch(const std::string& theUrl, std::string& theBody, std::string& theError)
	{
		std::wstring aUrl(theUrl.begin(), theUrl.end());
		URL_COMPONENTS aParts = {};
		aParts.dwStructSize = sizeof(aParts);
		wchar_t aHost[256] = {}, aPath[1024] = {};
		aParts.lpszHostName = aHost; aParts.dwHostNameLength = 255;
		aParts.lpszUrlPath = aPath; aParts.dwUrlPathLength = 1023;
		if (!WinHttpCrackUrl(aUrl.c_str(), 0, 0, &aParts))
		{
			theError = "bad address";
			return false;
		}
		std::string anAgent = std::string("InsaniquariumCoop/") + COOP_VERSION;
		std::wstring aAgent(anAgent.begin(), anAgent.end());
		HINTERNET aSession = WinHttpOpen(aAgent.c_str(), WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
		HINTERNET aConn = aSession ? WinHttpConnect(aSession, aHost, aParts.nPort, 0) : nullptr;
		HINTERNET aReq = aConn ? WinHttpOpenRequest(aConn, L"GET", aPath, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
			aParts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0) : nullptr;
		bool ok = false;
		if (aReq)
		{
			WinHttpSetTimeouts(aReq, 5000, 5000, 8000, 8000);
			ok = WinHttpSendRequest(aReq, L"Accept: application/vnd.github+json\r\n", (DWORD)-1L, WINHTTP_NO_REQUEST_DATA, 0, 0, 0)
				&& WinHttpReceiveResponse(aReq, nullptr);
			DWORD aStatus = 0, aSize = sizeof(aStatus);
			if (ok && WinHttpQueryHeaders(aReq, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &aStatus, &aSize, WINHTTP_NO_HEADER_INDEX))
			{
				if (aStatus != 200)
				{
					ok = false;
					theError = "HTTP " + std::to_string(aStatus);
				}
			}
			while (ok && theBody.size() < 1 << 20)
			{
				char aBuf[4096];
				DWORD aRead = 0;
				if (!WinHttpReadData(aReq, aBuf, sizeof(aBuf), &aRead) || aRead == 0)
					break;
				theBody.append(aBuf, aRead);
			}
		}
		if (!ok && theError.empty())
			theError = "no connection";
		if (aReq) WinHttpCloseHandle(aReq);
		if (aConn) WinHttpCloseHandle(aConn);
		if (aSession) WinHttpCloseHandle(aSession);
		return ok;
	}
#else
	static bool Fetch(const std::string& theUrl, std::string& theBody, std::string& theError)
	{
		// curl ships with macOS; the URL was checked by SafeUrl (no quotes or spaces).
		std::string aCmd = std::string("curl -fsSL --max-time 8 -A 'InsaniquariumCoop/") + COOP_VERSION +
			"' -H 'Accept: application/vnd.github+json' '" + theUrl + "' 2>/dev/null";
		FILE* p = popen(aCmd.c_str(), "r");
		if (p == nullptr)
		{
			theError = "no curl";
			return false;
		}
		char aBuf[4096];
		size_t n;
		while ((n = fread(aBuf, 1, sizeof(aBuf), p)) > 0 && theBody.size() < 1 << 20)
			theBody.append(aBuf, n);
		int aCode = pclose(p);
		if (aCode != 0)
		{
			theError = "no answer (curl " + std::to_string(aCode) + ")";
			return false;
		}
		return true;
	}
#endif

	static void StartCheck()
	{
		const char* anEnv = getenv("INSANIQ_UPDATE_URL");
		std::string aUrl = anEnv != nullptr ? anEnv : COOP_UPDATE_URL;
		if (!SafeUrl(aUrl))
		{
			S().Log("Update check: skipped (bad address)");
			gState = CHECK_DONE;
			return;
		}
		gState = CHECK_RUNNING;
		std::thread([aUrl]() {
			std::string aBody, anError, aVersion, aPage;
			bool ok = Fetch(aUrl, aBody, anError) && ParseUpdateJson(aBody, aVersion, aPage);
			std::lock_guard<std::mutex> aGuard(gLock);
			if (ok)
			{
				gLatest = aVersion;
				gUrl = aPage;
			}
			else
				gError = anError.empty() ? "unreadable answer" : anError;
			gState = CHECK_DONE;
		}).detach();
	}

	std::string UpdateAvailableVersion()
	{
		if (gState != CHECK_DONE)
			return "";
		std::lock_guard<std::mutex> aGuard(gLock);
		return !gLatest.empty() && CompareVersions(gLatest, COOP_VERSION) > 0 ? gLatest : "";
	}

	class UpdateDialog : public MoneyDialog
	{
	public:
		std::string mVersion, mUrl;

		UpdateDialog(WinFishApp* theApp, const std::string& theVersion, const std::string& theUrl)
			: MoneyDialog(theApp, IMAGE_DIALOG, IMAGE_DIALOGBUTTON, kDialogUpdate, true, "UPDATE AVAILABLE",
				"Insaniquarium Co-op " + theVersion + " is out (you have " + COOP_VERSION + ").\n\n"
				"Both players need the same version to play together." +
				(theUrl.empty() ? std::string("") : std::string(" Open the download page in your browser?")),
				theUrl.empty() ? "OK" : "", theUrl.empty() ? BUTTONS_FOOTER : BUTTONS_YES_NO),
			  mVersion(theVersion), mUrl(theUrl)
		{
			SetHeaderFont(FONT_JUNGLEFEVER15OUTLINE);
			SetColor(COLOR_HEADER, Color(0xff, 200, 0));
			if (mYesButton != nullptr)
				mYesButton->mLabel = "Open Page";
			if (mNoButton != nullptr)
				mNoButton->mLabel = "Not Now";
		}

		virtual void ButtonDepress(int theId) override
		{
			if (mEnableButtonsTimer >= mUpdateCnt)
				return;
			if (theId == ID_YES && !mUrl.empty())
			{
				S().Log("Update check: opening %s", mUrl.c_str());
				SDL_OpenURL(mUrl.c_str());
			}
			else if (theId == ID_NO)
				mApp->RegistryWriteString("CoopUpdateSkipped", mVersion);	// don't ask again for this one
			mApp->KillDialog(kDialogUpdate);
		}
	};

	void UpdateCheckFrame()
	{
		WinFishApp* anApp = App();
		if (anApp == nullptr || gShown)
			return;
		bool onMenu = anApp->mGameSelector != nullptr && anApp->mGameSelector->mVisible && anApp->mBoard == nullptr &&
			!HeroesBusy() && anApp->mDialogMap.empty();
		if (gState == CHECK_IDLE)
		{
			// Test runs stay offline unless a script points the check somewhere.
			bool aTest = getenv("INSANIQ_TESTSCRIPT") != nullptr && getenv("INSANIQ_UPDATE_URL") == nullptr;
			if (anApp->mGameSelector != nullptr && !aTest && UpdateCheckEnabled())
				StartCheck();
			else if (aTest || !UpdateCheckEnabled())
				gState = CHECK_DONE, gShown = true;
			return;
		}
		if (gState != CHECK_DONE || !onMenu)
			return;
		gShown = true;
		std::string aVersion, aUrl, anError;
		{
			std::lock_guard<std::mutex> aGuard(gLock);
			aVersion = gLatest, aUrl = gUrl, anError = gError;
		}
		if (!anError.empty())
		{
			S().Log("Update check: %s", anError.c_str());
			return;
		}
		S().Log("Update check: latest %s, this is %s", aVersion.c_str(), COOP_VERSION);
		std::string aSkipped;
		anApp->RegistryReadString("CoopUpdateSkipped", &aSkipped);
		if (CompareVersions(aVersion, COOP_VERSION) <= 0 || aSkipped == aVersion)
			return;
		UpdateDialog* aDialog = new UpdateDialog(anApp, aVersion, aUrl);
		int aWidth = 380;
		int aHeight = aDialog->GetPreferredHeight(aWidth);
		aDialog->Resize(320 - aWidth / 2, 240 - aHeight / 2, aWidth, aHeight);
		anApp->AddDialog(kDialogUpdate, aDialog);
	}
}
