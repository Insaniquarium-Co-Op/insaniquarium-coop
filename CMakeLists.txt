cmake_minimum_required(VERSION 3.16...4.0)

# Insaniquarium Co-op: an online two-player co-op mod built on the WinFish
# decompilation + PvZ-Portable SDL2/OpenGL SexyAppFramework (via the
# insaniquarium-mac port). Targets Windows (x64, MinGW/llvm-mingw), macOS
# (Apple Silicon, an .app bundle) and Linux (development and automated tests).

project(insaniquarium_coop LANGUAGES C CXX)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
if(CMAKE_GENERATOR MATCHES "Ninja")
	set(CMAKE_CXX_SCAN_FOR_MODULES OFF)
endif()

set(PVZ_DIR ${CMAKE_CURRENT_SOURCE_DIR}/PvZ-Portable)
set(SAF ${PVZ_DIR}/src/SexyAppFramework)
set(WINFISH_DIR ${CMAKE_CURRENT_SOURCE_DIR}/WinFish/source/WinFish)
if(NOT EXISTS ${WINFISH_DIR}/WinFishApp.cpp OR NOT EXISTS ${SAF}/SexyAppBase.cpp)
	message(FATAL_ERROR "The game's source and framework aren't here yet. Run scripts/fetch-upstream.sh (Build.bat does it on Windows).")
endif()
set(COOP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/coop)
set(PLATFORM_DIR ${CMAKE_CURRENT_SOURCE_DIR}/platform)

file(READ ${CMAKE_CURRENT_SOURCE_DIR}/COOP_VERSION COOP_VERSION)
string(STRIP "${COOP_VERSION}" COOP_VERSION)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/COOP_VERSION)

# WinFish includes <SexyAppFramework/Foo.h> flat-style. Build that flat view as
# real copies inside the build tree (no symlinks, so any host can build it).
set(FLAT_INC ${CMAKE_BINARY_DIR}/flatinc)
file(GLOB_RECURSE SAF_HEADERS RELATIVE ${SAF} ${SAF}/*.h ${SAF}/*.hpp ${SAF}/*.inc)
foreach(h ${SAF_HEADERS})
	# SDL-Mixer-X's timidity/common.h would overwrite Common.h on a case-insensitive disk (macOS).
	if(NOT h MATCHES "^platform/" AND NOT h MATCHES "^sound/SDL-Mixer-X/")
		get_filename_component(hn ${h} NAME)
		configure_file(${SAF}/${h} ${FLAT_INC}/SexyAppFramework/${hn} COPYONLY)
	endif()
endforeach()
# The decompilation was written on a case-insensitive filesystem.
configure_file(${SAF}/imagelib/ImageLib.h ${FLAT_INC}/ImageLib/ImageLib.h COPYONLY)

# ---------------- Dependencies ----------------
find_package(Threads REQUIRED)
find_package(ZLIB REQUIRED)
find_package(JPEG REQUIRED)
find_package(PNG REQUIRED)
find_package(SDL2 REQUIRED)

find_library(OGG_LIBRARY NAMES ogg libogg REQUIRED)
find_library(VORBIS_LIBRARY NAMES vorbis libvorbis REQUIRED)
find_library(VORBISFILE_LIBRARY NAMES vorbisfile libvorbisfile REQUIRED)
find_library(OPENMPT_LIBRARY NAMES openmpt libopenmpt REQUIRED)
find_library(MINIUPNPC_LIBRARY NAMES miniupnpc libminiupnpc REQUIRED)
find_path(MINIUPNPC_INCLUDE_DIR miniupnpc/miniupnpc.h REQUIRED)

set(SDL_MIXER_X_STATIC ON CACHE BOOL "Build static library of SDL Mixer X")
set(SDL_MIXER_X_SHARED OFF CACHE BOOL "Build shared library of SDL Mixer X")
set(USE_MIDI OFF CACHE BOOL "SDL Mixer X midi")
set(USE_MODPLUG OFF CACHE BOOL "SDL Mixer X modplug")
set(USE_XMP OFF CACHE BOOL "SDL Mixer X xmp")
set(USE_OPUS OFF CACHE BOOL "SDL Mixer X opus")
set(USE_DRFLAC OFF CACHE BOOL "SDL Mixer X flac")
set(USE_PXTONE OFF CACHE BOOL "SDL Mixer X pxtone")
set(USE_GME OFF CACHE BOOL "SDL Mixer X gme")
set(USE_WAVPACK OFF CACHE BOOL "SDL Mixer X wavpack")
set(USE_MP3_MPG123 OFF CACHE BOOL "SDL Mixer X mpg123")
set(USE_MIDI_NATIVE OFF CACHE BOOL "")
set(USE_MIDI_NATIVE_ALT OFF CACHE BOOL "")
set(USE_CMD OFF CACHE BOOL "")
add_subdirectory(${SAF}/sound/SDL-Mixer-X)

# ---------------- Sources ----------------
file(GLOB FRAMEWORK_SOURCES
	${SAF}/*.cpp
	${SAF}/graphics/*.cpp
	${SAF}/imagelib/*.cpp
	${SAF}/misc/*.cpp
	${SAF}/paklib/*.cpp
	${SAF}/sound/*.cpp
	${SAF}/widget/*.cpp
	${SAF}/fcaseopen/*.c
	${SAF}/ogg/*.c
)
list(APPEND FRAMEWORK_SOURCES
	${SAF}/platform/default/Window.cpp
	${SAF}/platform/default/Input.cpp
)

file(GLOB GAME_SOURCES ${WINFISH_DIR}/*.cpp)
# macOS-only pieces of the port are replaced by stubs in platform/.
list(REMOVE_ITEM GAME_SOURCES
	${WINFISH_DIR}/Window.cpp
	${WINFISH_DIR}/SteamPresence.cpp
	${WINFISH_DIR}/SaverBridge.cpp
)
file(GLOB COOP_SOURCES CONFIGURE_DEPENDS ${COOP_DIR}/*.cpp ${COOP_DIR}/heroes/*.cpp)
list(APPEND COOP_SOURCES
	${PLATFORM_DIR}/Main.cpp
	${PLATFORM_DIR}/AssetLocator.cpp
	${PLATFORM_DIR}/UpdateCheckStub.cpp
	${PLATFORM_DIR}/SteamPresenceStub.cpp
	${PLATFORM_DIR}/SaverBridgeStub.cpp
)

set(COMMON_INCLUDES
	${SAF}/platform/default
	${FLAT_INC}
	${PVZ_DIR}/src
	${SAF}
	${SAF}/sound/SDL-Mixer-X/include
	${COOP_DIR}
	${COOP_DIR}/heroes
	${PLATFORM_DIR}
	${MINIUPNPC_INCLUDE_DIR}
	${SDL2_INCLUDE_DIRS}
)

set(COMMON_C_FLAGS
	-fsigned-char
	-fno-strict-aliasing
	-Wall
	-Wreturn-type
	-Wno-unused-variable
	-Wno-unused-but-set-variable
	-Wno-parentheses
	-Wno-switch
	-Wno-deprecated-declarations
	-Wno-invalid-source-encoding
	-Wno-writable-strings
	-Wno-overloaded-virtual
	-Wno-reorder-ctor
	-Wno-delete-non-abstract-non-virtual-dtor
	-Wno-unused-private-field
	-Wno-missing-braces
	-Wno-unused-function
	-Wno-sign-compare
	-Wno-char-subscripts
	-Wno-unused-value
	-Wno-dangling-else
	-Wno-tautological-constant-out-of-range-compare
	-Wno-unknown-pragmas
	-Wno-c++11-narrowing
	-Wno-format
	-Wno-pointer-bool-conversion
	-Wno-tautological-undefined-compare
	-Wno-undefined-bool-conversion
	-Wno-microsoft-cast
	-Wno-unused-local-typedef
	-Wno-bitwise-instead-of-logical
	-Wno-deprecated-non-prototype
	-Wno-inconsistent-missing-override
	-Wno-unused-lambda-capture
	-Wno-sometimes-uninitialized
	-Wno-uninitialized
	-Wno-mismatched-tags
)

set(COMMON_DEFS IMG_DOWNSCALE=1 "COOP_VERSION=\"${COOP_VERSION}\"" "WINFISH_PORT_VERSION=\"coop-${COOP_VERSION}\"")
if(WIN32)
	list(APPEND COMMON_DEFS WIN32_LEAN_AND_MEAN NOMINMAX _CRT_SECURE_NO_WARNINGS)
endif()

add_library(sexyapp OBJECT ${FRAMEWORK_SOURCES})
target_compile_definitions(sexyapp PRIVATE ${COMMON_DEFS})
target_include_directories(sexyapp PRIVATE ${COMMON_INCLUDES})
target_compile_features(sexyapp PRIVATE cxx_std_20)
set_target_properties(sexyapp PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(sexyapp PRIVATE ${COMMON_C_FLAGS})
target_link_libraries(sexyapp PRIVATE SDL2_mixer_ext_Static PNG::PNG JPEG::JPEG ZLIB::ZLIB SDL2::SDL2 Threads::Threads)

set(COMMON_LIBS
	${MINIUPNPC_LIBRARY}
	SDL2_mixer_ext_Static
	${OPENMPT_LIBRARY}
	${VORBISFILE_LIBRARY}
	${VORBIS_LIBRARY}
	${OGG_LIBRARY}
	PNG::PNG
	JPEG::JPEG
	ZLIB::ZLIB
	SDL2::SDL2main
	SDL2::SDL2
	Threads::Threads
)
if(WIN32)
	list(APPEND COMMON_LIBS ws2_32 iphlpapi winhttp winmm imm32 version setupapi opengl32 shell32 ole32 uuid)
elseif(NOT APPLE)
	list(APPEND COMMON_LIBS dl)
endif()

set(RC_SOURCES)
if(WIN32)
	set(RC_SOURCES ${PLATFORM_DIR}/windows/InsaniquariumCoop.rc)
endif()

add_executable(InsaniquariumCoop ${GAME_SOURCES} ${COOP_SOURCES} ${RC_SOURCES} $<TARGET_OBJECTS:sexyapp>)
target_compile_definitions(InsaniquariumCoop PRIVATE ${COMMON_DEFS})
target_include_directories(InsaniquariumCoop PRIVATE ${COMMON_INCLUDES} ${WINFISH_DIR})
target_compile_features(InsaniquariumCoop PRIVATE cxx_std_20)
set_target_properties(InsaniquariumCoop PROPERTIES CXX_EXTENSIONS OFF)
target_compile_options(InsaniquariumCoop PRIVATE ${COMMON_C_FLAGS})
target_link_libraries(InsaniquariumCoop PRIVATE ${COMMON_LIBS})
if(WIN32)
	set_target_properties(InsaniquariumCoop PROPERTIES WIN32_EXECUTABLE ON)
	target_link_options(InsaniquariumCoop PRIVATE -static -static-libgcc -static-libstdc++)
elseif(APPLE)
	# InsaniquariumCoop.app. scripts/build-macos.sh copies the libraries it uses
	# into Contents/Frameworks (found through the rpath below), signs it and zips it.
	set(COOP_MACOS_DIR ${PLATFORM_DIR}/macos)
	if(CMAKE_OSX_DEPLOYMENT_TARGET)
		set(COOP_MACOS_MIN ${CMAKE_OSX_DEPLOYMENT_TARGET})
	else()
		set(COOP_MACOS_MIN 12.0)
	endif()
	set(COOP_ICON ${COOP_MACOS_DIR}/InsaniquariumCoop.icns)
	set_source_files_properties(${COOP_ICON} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
	target_sources(InsaniquariumCoop PRIVATE ${COOP_ICON})
	set_target_properties(InsaniquariumCoop PROPERTIES
		MACOSX_BUNDLE TRUE
		MACOSX_BUNDLE_INFO_PLIST ${COOP_MACOS_DIR}/Info.plist.in
		MACOSX_BUNDLE_BUNDLE_VERSION ${COOP_VERSION}
		MACOSX_BUNDLE_SHORT_VERSION_STRING ${COOP_VERSION}
		BUILD_WITH_INSTALL_RPATH ON
		INSTALL_RPATH "@executable_path/../Frameworks"
	)
endif()
