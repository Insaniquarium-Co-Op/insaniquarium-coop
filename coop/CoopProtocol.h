// Insaniquarium Co-op - wire protocol.
//
// Transport: one TCP connection, host <-> guest. Every message is
//   [u32 length][u8 type][payload...]   (length counts the type byte)
// All integers little-endian.
//
// The host runs the only simulation. It streams what it draws (a display list
// of framework primitives, with images referenced by content hash) plus audio
// events; the guest replays them with its own copy of the game assets and sends
// its mouse/keyboard back, which the host injects as player 2.

#ifndef __COOP_PROTOCOL_H__
#define __COOP_PROTOCOL_H__

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace Coop
{
	static const char		kMagic[8] = { 'I','N','S','Q','C','O','O','P' };
	static const uint16_t	kProtocolVersion = 10;	// 4: Tank Race messages, 5: poison raid attack, 6: Alien Keeper, 7: keeper clock and juveniles, 8: Pet Heroes, 9: Pet Heroes food quality and the coin pet, 10: Pet Heroes 3.0 (the Trench, talents, new heroes)
	static const uint16_t	kDefaultPort = 24050;
	static const uint16_t	kDiscoveryPort = 24051;
	static const uint32_t	kMaxMessageSize = 64u * 1024u * 1024u;

	enum MsgType : uint8_t
	{
		// guest -> host
		MSG_HELLO			= 1,	// magic, proto, version str, player name, base image hashes
		MSG_INPUT			= 2,	// one or more input events
		MSG_FRAME_ACK		= 3,	// u32 frame seq
		MSG_NEED_PIXELS		= 7,	// u32 image id the guest could not resolve
		MSG_PING_MARKER		= 8,	// i16 x, i16 y: "look here!"

		// both directions
		MSG_PING			= 10,	// u32 token, u32 sender clock ms
		MSG_PONG			= 11,	// echo of PING
		MSG_BYE				= 12,	// str reason

		// host -> guest
		MSG_WELCOME			= 20,	// proto, host name, settings
		MSG_REJECT			= 21,	// str reason
		MSG_FRAME			= 22,	// chunk of the shared deflate stream (one frame)
		MSG_AUDIO			= 23,	// audio events
		MSG_STATUS			= 24,	// co-op HUD / settings snapshot

		// Tank Race: each computer runs its own tank; these keep the two in step.
		MSG_RACE_SETUP		= 30,	// host -> guest: u32 race id, u8 tank, u8 level, u8 catch-up
		MSG_RACE_READY		= 31,	// both: u32 race id (my tank is built and waiting)
		MSG_RACE_GO			= 32,	// host -> guest: u32 race id (start the countdown)
		MSG_RACE_STATE		= 33,	// both: money, eggs, counts, effects, fish dots
		MSG_RACE_ATTACK		= 34,	// both: u8 attack, i32 price paid
		MSG_RACE_FINISH		= 35,	// guest -> host: u8 reason (egg done / tank lost / gave up)
		MSG_RACE_RESULT		= 36,	// host -> guest: u8 winner, u8 reason, u8 finisher, u32 ms, u8 wins[2]
		MSG_RACE_EVENT		= 37,	// both: u8 event, i32 value
		MSG_RACE_CANCEL		= 38,	// both: str reason

		// Alien Keeper: the fish keeper's computer runs the tank and streams it to the
		// alien keeper, whichever of them hosts. Rounds use the Tank Race messages above.
		MSG_STREAM_START	= 39,	// viewer -> sender: u32 count, u64 image hashes (start streaming to me)
		MSG_KEEPER_LAUNCH	= 40,	// alien keeper -> fish keeper: u8 alien kind, i16 x, i16 y
		MSG_KEEPER_POWER	= 41,	// alien keeper -> fish keeper: u8 power
		MSG_KEEPER_CURSOR	= 42,	// alien keeper -> fish keeper: i16 x, i16 y, u8 hunting
		MSG_KEEPER_LAIR		= 43,	// alien keeper -> fish keeper: i32 dark energy, u8 eggs, u8 ready
		MSG_KEEPER_EVENT	= 44,	// fish keeper -> alien keeper: u8 event, i32 value
		MSG_LAST_ROUND_MSG	= MSG_KEEPER_EVENT,

		// Pet Heroes: each computer runs its own side; MSG_HEROES_DATA carries the
		// sides' own messages (coop/heroes/HeroesNet.h).
		MSG_HEROES_SETUP	= 45,	// host -> guest: u32 match id (open the draft)
		MSG_HEROES_PICK		= 46,	// both: u32 match id, u8 hero (locked in)
		MSG_HEROES_GO		= 47,	// host -> guest: u32 match id, u64 seed, u8 host hero, u8 guest hero
		MSG_HEROES_DATA		= 48,	// both: u32 match id, u8 sides' message type, payload
		MSG_HEROES_CANCEL	= 49,	// both: u32 match id, str reason (left the draft)
		MSG_HEROES_FIRST	= MSG_HEROES_SETUP,
		MSG_HEROES_LAST		= MSG_HEROES_CANCEL,
	};

	// Frame stream commands (inside the deflated MSG_FRAME payload).
	enum FrameCmd : uint8_t
	{
		FC_FILLRECT = 1,
		FC_LINE,
		FC_POLY,
		FC_BLT,
		FC_BLTMIRROR,
		FC_BLTF,
		FC_BLTROT,
		FC_STRETCH,
		FC_STRETCHMIRROR,
		FC_MATRIX,
		FC_TRIS,

		FC_DEF_REF = 100,			// u32 id, u64 hash
		FC_DEF_PIX,					// u32 id, u16 w, u16 h, u8 flags, w*h u32 pixels
		FC_FREE,					// u32 id
		FC_HEADER,					// u32 seq, i16 hostX, i16 hostY, u8 guestCursor, u8 flags
		FC_END,
	};

	enum InputKind : uint8_t
	{
		IN_MOVE = 1,	// i16 x, i16 y
		IN_DOWN,		// i16 x, i16 y, i8 button code (framework click-count encoding)
		IN_UP,			// i16 x, i16 y, i8 button code
		IN_WHEEL,		// i16 delta
		IN_KEYDOWN,		// u16 keycode
		IN_KEYUP,		// u16 keycode
		IN_CHAR,		// u8 char
		IN_LEAVE,		// pointer left the guest's window
	};

	enum AudioEvt : uint8_t
	{
		AE_PLAY = 1,	// u32 serial, i16 sfx, f32 vol, i16 pan, f32 pitch, u8 loop
		AE_STOP,		// u32 serial
		AE_VOLUME,		// u32 serial, f32 vol
		AE_PAN,			// u32 serial, i16 pan
		AE_PITCH,		// u32 serial, f32 pitch
		AE_MUSIC,		// u8 op, i16 song, i32 offset, f32 value, u8 flag
	};

	class ByteWriter
	{
	public:
		std::vector<uint8_t> mData;

		void Clear() { mData.clear(); }
		size_t Size() const { return mData.size(); }
		void Bytes(const void* p, size_t n) { const uint8_t* b = (const uint8_t*)p; mData.insert(mData.end(), b, b + n); }
		void U8(uint8_t v) { mData.push_back(v); }
		void I8(int8_t v) { mData.push_back((uint8_t)v); }
		void U16(uint16_t v) { U8(v & 0xFF); U8(v >> 8); }
		void I16(int v) { if (v < -32768) v = -32768; else if (v > 32767) v = 32767; U16((uint16_t)(int16_t)v); }
		void U32(uint32_t v) { U16(v & 0xFFFF); U16(v >> 16); }
		void I32(int32_t v) { U32((uint32_t)v); }
		void U64(uint64_t v) { U32((uint32_t)v); U32((uint32_t)(v >> 32)); }
		void F32(float f) { uint32_t u; memcpy(&u, &f, 4); U32(u); }
		void Str(const std::string& s) { uint16_t n = (uint16_t)(s.size() > 0xFFFF ? 0xFFFF : s.size()); U16(n); Bytes(s.data(), n); }
	};

	class ByteReader
	{
	public:
		const uint8_t*	mData;
		size_t			mSize;
		size_t			mPos = 0;
		bool			mError = false;

		ByteReader(const uint8_t* d, size_t n) : mData(d), mSize(n) {}
		bool AtEnd() const { return mPos >= mSize; }
		size_t Remaining() const { return mPos < mSize ? mSize - mPos : 0; }
		bool Need(size_t n) { if (mError || mSize - mPos < n || mPos > mSize) { mError = true; return false; } return true; }
		uint8_t U8() { if (!Need(1)) return 0; return mData[mPos++]; }
		int8_t I8() { return (int8_t)U8(); }
		uint16_t U16() { if (!Need(2)) return 0; uint16_t v = mData[mPos] | (mData[mPos + 1] << 8); mPos += 2; return v; }
		int I16() { return (int16_t)U16(); }
		uint32_t U32() { if (!Need(4)) return 0; uint32_t v = (uint32_t)mData[mPos] | ((uint32_t)mData[mPos + 1] << 8) | ((uint32_t)mData[mPos + 2] << 16) | ((uint32_t)mData[mPos + 3] << 24); mPos += 4; return v; }
		int32_t I32() { return (int32_t)U32(); }
		uint64_t U64() { uint64_t lo = U32(); uint64_t hi = U32(); return lo | (hi << 32); }
		float F32() { uint32_t u = U32(); float f; memcpy(&f, &u, 4); return f; }
		std::string Str() { uint16_t n = U16(); if (!Need(n)) return std::string(); std::string s((const char*)mData + mPos, n); mPos += n; return s; }
		const uint8_t* Bytes(size_t n) { if (!Need(n)) return nullptr; const uint8_t* p = mData + mPos; mPos += n; return p; }
	};
}

#endif
