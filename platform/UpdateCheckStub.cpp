// Co-op build: the macOS port's GitHub self-updater does not apply here.
// The "Check Updates" flow simply reports that this build is current.
#include "UpdateCheck.h"

namespace Sexy {
void UpdateCheck::Start() {}
UpdateCheck::State UpdateCheck::Poll() { return OK; }
std::string UpdateCheck::LatestTag() { return std::string(); }
std::string UpdateCheck::Error() { return std::string(); }
void UpdateCheck::Cancel() {}
std::string UpdateCheck::InstalledTag() { return std::string(); }
bool UpdateCheck::IsUpdatableInstall() { return false; }
bool UpdateCheck::SpawnUpdater(pid_t) { return false; }
}
