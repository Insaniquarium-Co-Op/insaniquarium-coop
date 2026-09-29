#include "CoopUpnp.h"

#define MINIUPNP_STATICLIB
#include <miniupnpc/miniupnpc.h>
#include <miniupnpc/upnpcommands.h>
#include <miniupnpc/upnperrors.h>

#include <cstring>
#include <cstdio>

namespace Coop
{
	static bool IsPrivateIPv4(const std::string& s)
	{
		unsigned a = 0, b = 0, c = 0, d = 0;
		if (sscanf(s.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4)
			return true;
		if (a == 10 || a == 127 || a == 0)
			return true;
		if (a == 172 && b >= 16 && b <= 31)
			return true;
		if (a == 192 && b == 168)
			return true;
		if (a == 100 && b >= 64 && b <= 127)	// carrier-grade NAT
			return true;
		if (a == 169 && b == 254)
			return true;
		return false;
	}

	PortMapper::~PortMapper()
	{
		mCancel = true;
		Join();
	}

	void PortMapper::Join()
	{
		if (mThread.joinable())
			mThread.join();
	}

	void PortMapper::Start(uint16_t thePort)
	{
		Stop();
		{
			std::scoped_lock l(mMutex);
			mState = WORKING;
			mExternal.clear();
			mDetail = "Asking your router to open the port...";
		}
		mCancel = false;
		mThread = std::thread(&PortMapper::Run, this, thePort);
	}

	void PortMapper::Run(uint16_t thePort)
	{
		int anError = 0;
		UPNPDev* aDevs = upnpDiscover(2000, nullptr, nullptr, 0, 0, 2, &anError);
		if (mCancel)
		{
			freeUPNPDevlist(aDevs);
			return;
		}
		if (aDevs == nullptr)
		{
			std::scoped_lock l(mMutex);
			mState = FAILED;
			mDetail = "Your router didn't answer (UPnP is off or unsupported).";
			return;
		}

		UPNPUrls aUrls;
		IGDdatas aData;
		char aLan[64] = { 0 };
		char aWan[64] = { 0 };
		memset(&aUrls, 0, sizeof(aUrls));
		memset(&aData, 0, sizeof(aData));
		int anIgd = UPNP_GetValidIGD(aDevs, &aUrls, &aData, aLan, sizeof(aLan), aWan, sizeof(aWan));
		freeUPNPDevlist(aDevs);
		if (anIgd == 0 || mCancel)
		{
			if (anIgd != 0)
				FreeUPNPUrls(&aUrls);
			std::scoped_lock l(mMutex);
			mState = FAILED;
			mDetail = "No router that supports automatic port opening was found.";
			return;
		}

		char anExt[64] = { 0 };
		UPNP_GetExternalIPAddress(aUrls.controlURL, aData.first.servicetype, anExt);

		char aPort[16];
		snprintf(aPort, sizeof(aPort), "%u", (unsigned)thePort);
		int r = UPNP_AddPortMapping(aUrls.controlURL, aData.first.servicetype, aPort, aPort, aLan, "Insaniquarium Co-op", "TCP", nullptr, "14400");
		if (r == 725)	// OnlyPermanentLeasesSupported
			r = UPNP_AddPortMapping(aUrls.controlURL, aData.first.servicetype, aPort, aPort, aLan, "Insaniquarium Co-op", "TCP", nullptr, "0");

		std::scoped_lock l(mMutex);
		mExternal = anExt;
		if (r == UPNPCOMMAND_SUCCESS)
		{
			mState = MAPPED;
			mMapped = true;
			mControlUrl = aUrls.controlURL;
			mServiceType = aData.first.servicetype;
			mPortStr = aPort;
			mDetail = "Your router opened the port automatically.";
		}
		else
		{
			mState = FAILED;
			char aBuf[160];
			snprintf(aBuf, sizeof(aBuf), "Your router refused to open the port (%s).", strupnperror(r));
			mDetail = aBuf;
		}
		FreeUPNPUrls(&aUrls);
	}

	void PortMapper::Stop()
	{
		mCancel = true;
		Join();
		std::string aControl, aService, aPort;
		bool aMapped;
		{
			std::scoped_lock l(mMutex);
			aMapped = mMapped;
			aControl = mControlUrl;
			aService = mServiceType;
			aPort = mPortStr;
			mMapped = false;
			mState = IDLE;
			mExternal.clear();
			mDetail.clear();
		}
		if (aMapped)
			UPNP_DeletePortMapping(aControl.c_str(), aService.c_str(), aPort.c_str(), "TCP", nullptr);
	}

	PortMapper::State PortMapper::GetState()
	{
		std::scoped_lock l(mMutex);
		return mState;
	}

	std::string PortMapper::GetExternalAddress()
	{
		std::scoped_lock l(mMutex);
		return mExternal;
	}

	std::string PortMapper::GetDetail()
	{
		std::scoped_lock l(mMutex);
		return mDetail;
	}

	bool PortMapper::IsSharedAddress()
	{
		std::scoped_lock l(mMutex);
		return !mExternal.empty() && IsPrivateIPv4(mExternal);
	}
}
