// Insaniquarium Co-op - "is there a newer version?" (check only).
//
// Once per launch, on reaching the main menu, a background thread fetches a small
// JSON document (the GitHub "latest release" API by default, or a manifest with
// "version" and "url") and compares its version with COOP_VERSION. If it's newer,
// a dialog offers to open the release page in the browser. Nothing is downloaded
// or installed by the game. Players can turn it off in the Co-op window.
// DECISIONS.md D26.

#ifndef __COOP_UPDATE_H__
#define __COOP_UPDATE_H__

#include <string>

namespace Coop
{
	// Per frame (from Session::PreUpdateFrames): starts the check on the main menu,
	// shows the dialog when a newer version is found.
	void	UpdateCheckFrame();

	bool	UpdateCheckEnabled();
	void	SetUpdateCheckEnabled(bool theOn);
	// The newer version found this launch ("" if none or not checked yet).
	std::string	UpdateAvailableVersion();

	// The version this copy reports to its partner (COOP_VERSION unless a test fakes it).
	std::string	LocalVersion();
	void	SetFakeVersion(const std::string& theVersion);

	// -1, 0 or 1 comparing "2.0.1" style versions (a leading "v" is ignored).
	int		CompareVersions(const std::string& a, const std::string& b);
	// Pulls "version"/"tag_name" and "url"/"html_url" out of the JSON (tests use this).
	bool	ParseUpdateJson(const std::string& theJson, std::string& theVersion, std::string& theUrl);
}

#endif
