// Insaniquarium Co-op - in-game screens: the Co-op dialog (host / join /
// difficulty / status) and the guest's pause menu.

#ifndef __COOP_UI_H__
#define __COOP_UI_H__

#include <SexyAppFramework/Font.h>
#include "MoneyDialog.h"
#include <SexyAppFramework/EditListener.h>
#include <SexyAppFramework/KeyCodes.h>
#include <string>
#include <vector>

namespace Sexy
{
	class WinFishApp;
	class DialogButton;
	class EditWidget;
}

namespace Coop
{
	static const int kDialogCoop = 92;
	static const int kDialogGuestMenu = 93;
	static const int kDialogVersus = 94;

	void OpenCoopDialog(Sexy::WinFishApp* theApp);
	void OpenGuestMenu();
	// Called once per frame by the game selector / options so labels stay live.
	std::string CoopButtonLabel();

	class CoopDialog : public Sexy::MoneyDialog, public Sexy::EditListener
	{
	public:
		enum Page { PAGE_MAIN, PAGE_HOST, PAGE_JOIN };

		CoopDialog(Sexy::WinFishApp* theApp);
		virtual ~CoopDialog();

		virtual void	AddedToManager(Sexy::WidgetManager* theWidgetManager) override;
		virtual void	RemovedFromManager(Sexy::WidgetManager* theWidgetManager) override;
		virtual void	Draw(Sexy::Graphics* g) override;
		virtual void	Update() override;
		virtual void	Resize(int theX, int theY, int theWidth, int theHeight) override;
		virtual int		GetPreferredHeight(int theWidth) override;
		virtual void	ButtonPress(int theId) override;
		virtual void	ButtonDepress(int theId) override;
		virtual void	EditWidgetText(int theId, const std::string& theString) override;
		bool			AllowChar(int theId, char theChar);

	private:
		void			SetPage(Page thePage);
		void			Layout();
		void			DoConnect(const std::string& theAddress);

		Page			mPage = PAGE_MAIN;
		Sexy::DialogButton* mHostButton;
		Sexy::DialogButton* mJoinButton;
		Sexy::DialogButton* mDiffButton;
		Sexy::DialogButton* mStopButton;
		Sexy::DialogButton* mKickButton;
		Sexy::DialogButton* mCopyButton;
		Sexy::DialogButton* mConnectButton;
		Sexy::DialogButton* mBackButton;
		Sexy::DialogButton* mPracticeButton;
		Sexy::DialogButton* mUpdateButton;
		Sexy::DialogButton* mLanButtons[3];
		Sexy::EditWidget*	mAddressEdit;
		std::vector<std::string> mLanAddresses;
		std::vector<std::string> mLocalIps;
		std::string		mMessage;
		bool			mMessageIsError = false;
		bool			mDiscoveryStarted = false;
		int				mCopiedTimer = 0;
		std::string		BestAddress();
	};

	class GuestMenu : public Sexy::MoneyDialog
	{
	public:
		GuestMenu(Sexy::WinFishApp* theApp);
		virtual ~GuestMenu();
		virtual void	AddedToManager(Sexy::WidgetManager* theWidgetManager) override;
		virtual void	RemovedFromManager(Sexy::WidgetManager* theWidgetManager) override;
		virtual void	Draw(Sexy::Graphics* g) override;
		virtual void	Resize(int theX, int theY, int theWidth, int theHeight) override;
		virtual int		GetPreferredHeight(int theWidth) override;
		virtual void	ButtonPress(int theId) override;
		virtual void	ButtonDepress(int theId) override;
		virtual void	KeyDown(Sexy::KeyCode theKey) override;

	private:
		Sexy::DialogButton* mLeaveButton;
		Sexy::DialogButton* mBackButton;
		Sexy::DialogButton* mOptionsButton;
	};
}

#endif
