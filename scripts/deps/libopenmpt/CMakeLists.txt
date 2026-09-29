# libopenmpt (plays the game's .mo3 music) as a static library, built with CMake so the
# Windows build needs no GNU make or shell. Mirrors the upstream makefile with
# NO_ZLIB/NO_MPG123/NO_OGG/NO_VORBIS: the bundled miniz, minimp3 and stb_vorbis.
#   cmake -S scripts/deps/libopenmpt -DOPENMPT_SRC=<openmpt source> ...
cmake_minimum_required(VERSION 3.16)
project(libopenmpt CXX C)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(S ${OPENMPT_SRC})
file(GLOB CXX_SOURCES
	${S}/src/openmpt/all/*.cpp ${S}/src/openmpt/base/*.cpp ${S}/src/openmpt/logging/*.cpp
	${S}/src/openmpt/random/*.cpp ${S}/src/openmpt/soundbase/*.cpp ${S}/common/*.cpp
	${S}/soundlib/*.cpp ${S}/soundlib/plugins/*.cpp ${S}/soundlib/plugins/dmo/*.cpp
	${S}/sounddsp/*.cpp ${S}/libopenmpt/*.cpp)
add_library(openmpt STATIC ${CXX_SOURCES}
	${S}/include/miniz/miniz.c ${S}/include/minimp3/minimp3.c ${S}/include/stb_vorbis/stb_vorbis.c)
target_include_directories(openmpt PRIVATE ${S}/src ${S}/common ${S} ${S}/include)
target_compile_definitions(openmpt PRIVATE LIBOPENMPT_BUILD MPT_WITH_MINIZ MPT_WITH_MINIMP3
	MPT_WITH_STBVORBIS STB_VORBIS_NO_PULLDATA_API STB_VORBIS_NO_STDIO)
install(TARGETS openmpt ARCHIVE DESTINATION lib)
file(GLOB HEADERS ${S}/libopenmpt/libopenmpt*.h ${S}/libopenmpt/libopenmpt*.hpp)
install(FILES ${HEADERS} DESTINATION include/libopenmpt)
