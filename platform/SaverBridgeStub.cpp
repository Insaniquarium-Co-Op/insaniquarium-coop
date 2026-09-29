// Co-op build: there is no macOS screensaver container to sync with.
#include "SaverBridge.h"
namespace Sexy {
bool PushSavesToSaverContainer(const std::string&, bool) { return true; }
void ImportSaverTank(const std::string&) {}
void ImportScreenSaverEarnings(const std::string&) {}
bool SaverBridgePushFailed() { return false; }
void OpenFullDiskAccessSettings() {}
}
