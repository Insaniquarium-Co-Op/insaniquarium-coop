#ifndef __COOP_ASSETLOCATOR_H__
#define __COOP_ASSETLOCATOR_H__
#include <string>
namespace Coop
{
	bool IsGameFolder(const std::string& theDir);
	bool LocateGameAssets(int argc, char** argv, std::string& theOutDir);
	void ImportOriginalSaves(const std::string& theSaveDir, const std::string& theGameDir);
}
#endif
