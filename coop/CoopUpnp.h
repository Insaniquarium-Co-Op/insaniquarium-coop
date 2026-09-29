// Insaniquarium Co-op - automatic router port opening (UPnP IGD).
//
// When hosting, we ask the home router (in the background) to forward the
// co-op port to this computer and to tell us the internet-facing address, so a
// friend in another house can join by typing that address. Many routers allow
// this; when one doesn't, the Co-op window says so and points at the README.

#ifndef __COOP_UPNP_H__
#define __COOP_UPNP_H__

#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <atomic>

namespace Coop
{
	class PortMapper
	{
	public:
		enum State { IDLE, WORKING, MAPPED, FAILED };

		~PortMapper();
		void			Start(uint16_t thePort);
		// Removes the mapping (waits at most a couple of seconds).
		void			Stop();

		State			GetState();
		std::string		GetExternalAddress();
		std::string		GetDetail();
		bool			IsSharedAddress();	// external address is itself private / CGNAT

	private:
		void			Run(uint16_t thePort);
		void			Join();

		std::mutex		mMutex;
		std::thread		mThread;
		std::atomic<bool> mCancel{ false };
		State			mState = IDLE;
		std::string		mExternal;
		std::string		mDetail;
		// what we mapped, for removal
		std::string		mControlUrl;
		std::string		mServiceType;
		std::string		mPortStr;
		bool			mMapped = false;
	};
}

#endif
