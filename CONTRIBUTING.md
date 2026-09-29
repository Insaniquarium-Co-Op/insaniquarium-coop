# Contributing

Thanks for helping! A few ground rules keep this project fair to the game's owners:

- **Never commit or attach game files**: no images, sounds, music, fonts, `.dat`/`.xml`
  data or executables from Insaniquarium Deluxe, and no extracted or converted copies,
  in code, issues, pull requests or screenshots of extracted data. Screenshots of the
  game running are fine.
- **Don't add the decompiled source or the framework to this repository.** Changes to
  them go in `patches/`: edit the `WinFish/` or `PvZ-Portable/` folder the build
  fetched, then run `scripts/make-patches.sh` to regenerate the patches.
- Keep it free and non-commercial.

Build as described in the README. Both players need the same version, so if you change
a network message, bump `kProtocolVersion` in `coop/CoopProtocol.h`.
