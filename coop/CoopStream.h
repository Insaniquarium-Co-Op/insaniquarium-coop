// Insaniquarium Co-op - display-list streaming.
//
// FrameEncoder (host) implements the framework's DrawRecorder: while a frame is
// being captured, every primitive drawn to the screen is serialized. Source
// images are identified by a 64-bit content hash; images the guest already owns
// (it hashes its own loaded assets and tells us in HELLO) are referenced, all
// others (runtime-generated ones) are sent as raw pixels once and cached on the
// guest under a small id until evicted. The whole stream is one continuous
// deflate stream, so a frame that looks like the previous one costs very little.
//
// FrameDecoder (guest) inflates, maintains the id -> Image table, and replays
// the most recent complete frame onto its screen with identical primitives.

#ifndef __COOP_STREAM_H__
#define __COOP_STREAM_H__

#include "CoopProtocol.h"
#include <SexyAppFramework/CoopHooks.h>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <zlib.h>

namespace Sexy
{
	class Image;
	class MemoryImage;
	class GLImage;
}

namespace Coop
{
	uint64_t HashImage(Sexy::MemoryImage* theImage);

	class FrameEncoder : public Sexy::DrawRecorder
	{
	public:
		FrameEncoder();
		~FrameEncoder();

		void			Reset(const std::vector<uint64_t>& theGuestHashes);
		void			BeginFrame(Sexy::Image* theScreen, uint32_t theSeq, int theHostX, int theHostY, int theGuestCursor, uint8_t theFlags);
		// Finishes the frame and returns the compressed MSG_FRAME payload.
		void			EndFrame(std::vector<uint8_t>& theOut);
		void			AbortFrame();
		bool			IsRecording() const { return mRecording; }
		void			GuestCannotResolve(uint32_t theId);
		void			OnImageDestroyed(Sexy::Image* theImage);

		// stats
		uint32_t		mLastRawBytes = 0;
		uint32_t		mLastOps = 0;
		uint32_t		mPixelDefsSent = 0;
		uint64_t		mPixelBytesLive = 0;

		// DrawRecorder
		virtual void	RecFillRect(const Sexy::Rect& theRect, const Sexy::Color& theColor, int theDrawMode) override;
		virtual void	RecLine(double x1, double y1, double x2, double y2, const Sexy::Color& theColor, int theDrawMode, bool aa) override;
		virtual void	RecPoly(const Sexy::Point* theVertices, int theNum, const Sexy::Rect* theClip, const Sexy::Color& theColor, int theDrawMode, int tx, int ty) override;
		virtual void	RecBlt(Sexy::Image* theImage, int x, int y, const Sexy::Rect& theSrc, const Sexy::Color& theColor, int theDrawMode, bool linear, bool mirror) override;
		virtual void	RecBltF(Sexy::Image* theImage, float x, float y, const Sexy::Rect& theSrc, const Sexy::Rect& theClip, const Sexy::Color& theColor, int theDrawMode) override;
		virtual void	RecBltRotated(Sexy::Image* theImage, float x, float y, const Sexy::Rect& theSrc, const Sexy::Rect& theClip, const Sexy::Color& theColor, int theDrawMode, double theRot, float cx, float cy) override;
		virtual void	RecStretch(Sexy::Image* theImage, const Sexy::Rect& theDest, const Sexy::Rect& theSrc, const Sexy::Rect& theClip, const Sexy::Color& theColor, int theDrawMode, bool fast, bool mirror) override;
		virtual void	RecMatrix(Sexy::Image* theImage, float x, float y, const Sexy::SexyMatrix3& theMatrix, const Sexy::Rect& theClip, const Sexy::Color& theColor, int theDrawMode, const Sexy::Rect& theSrc, bool blend) override;
		virtual void	RecTris(Sexy::Image* theImage, const Sexy::TriVertex theVertices[][3], int theNum, const Sexy::Rect& theClip, const Sexy::Color& theColor, int theDrawMode, float tx, float ty, bool blend) override;

	private:
		struct Entry
		{
			uint64_t	mHash = 0;
			uint32_t	mBytes = 0;
			uint32_t	mLastFrame = 0;
			bool		mPixels = false;
		};
		struct SerialInfo
		{
			uint32_t	mChangeCount = 0;
			uint32_t	mId = 0;
			int			mWidth = 0;
			int			mHeight = 0;
		};

		uint32_t		ImageRef(Sexy::Image* theImage);
		void			Evict();
		void			Rect16(const Sexy::Rect& r);
		void			ColorU32(const Sexy::Color& c);

		bool			mRecording = false;
		uint32_t		mFrameNum = 0;
		ByteWriter		mDefs;
		ByteWriter		mOps;
		std::vector<uint32_t> mPendingFrees;
		std::unordered_map<uint32_t, Entry> mEntries;
		std::unordered_map<uint64_t, uint32_t> mHashToId;
		std::unordered_map<uint32_t, SerialInfo> mSerials;
		std::unordered_set<uint64_t> mGuestHas;
		uint32_t		mNextId = 1;
		z_stream		mZ;
		bool			mZInit = false;
	};

	class FrameDecoder
	{
	public:
		FrameDecoder();
		~FrameDecoder();

		void			Reset();
		// Collects hashes of every image this process currently has loaded.
		static void		CollectLocalHashes(std::vector<uint64_t>& theOut);
		// Decodes one MSG_FRAME payload. Returns false on a corrupt stream.
		bool			Decode(const uint8_t* theData, size_t theSize);
		// Draws the newest decoded frame onto theScreen.
		void			Replay(Sexy::Image* theScreen);
		bool			HasFrame() const { return mHaveFrame; }
		void			OnImageDestroyed(Sexy::Image* theImage);

		uint32_t		mSeq = 0;
		int				mHostX = -1;
		int				mHostY = -1;
		int				mGuestCursor = 0;
		uint8_t			mFlags = 0;
		std::vector<uint32_t> mUnresolved;	// ids to report via MSG_NEED_PIXELS
		uint32_t		mFramesDecoded = 0;
		std::string		mError;

	private:
		struct Slot
		{
			Sexy::Image*	mImage = nullptr;
			Sexy::MemoryImage* mOwned = nullptr;
		};
		void			FreeSlot(uint32_t theId);
		bool			ParseCommands(const uint8_t* p, size_t n);
		Sexy::Image*	Lookup(uint32_t theId);
		void			EnsureLocalIndex();

		z_stream		mZ;
		bool			mZInit = false;
		std::vector<uint8_t> mRaw;
		std::vector<uint8_t> mFrameOps;	// ops of the newest frame
		bool			mHaveFrame = false;
		std::unordered_map<uint32_t, Slot> mSlots;
		std::unordered_map<uint64_t, Sexy::MemoryImage*> mLocal;	// hash -> local image
		std::unordered_map<Sexy::Image*, uint64_t> mLocalRev;
		bool			mLocalIndexed = false;
	};
}

#endif
