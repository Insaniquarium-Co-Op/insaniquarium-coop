// Insaniquarium Co-op - the guest's window onto the host's tank.

#ifndef __COOP_VIEW_H__
#define __COOP_VIEW_H__

#include <SexyAppFramework/Widget.h>

namespace Coop
{
	// Full-screen widget shown while playing as a guest: draws the host's
	// streamed frames and forwards every mouse/keyboard event to the host.
	// A passive view (the alien keeper watching the fish tank) only draws; the
	// lair panel on top of it takes the input.
	class RemoteView : public Sexy::Widget
	{
	public:
		explicit RemoteView(bool thePassive = false);

		virtual void	Update() override;
		virtual void	Draw(Sexy::Graphics* g) override;
		virtual bool	WantsFocus() override { return !mPassive; }

		virtual void	MouseMove(int x, int y) override;
		virtual void	MouseDrag(int x, int y) override;
		virtual void	MouseDown(int x, int y, int theClickCount) override;
		virtual void	MouseUp(int x, int y, int theClickCount) override;
		virtual void	MouseLeave() override;
		virtual void	MouseWheel(int theDelta) override;
		virtual void	KeyDown(Sexy::KeyCode theKey) override;
		virtual void	KeyUp(Sexy::KeyCode theKey) override;
		virtual void	KeyChar(char theChar) override;

		bool			mPassive = false;

	private:
		int				mLastX = 0;
		int				mLastY = 0;
	};
}

#endif
