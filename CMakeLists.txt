cmake_minimum_required(VERSION 3.21)

project(
	Fallout4FavoritesMenuGrid
	VERSION 0.1.0
	LANGUAGES CXX
)

if (PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
	message(FATAL_ERROR "in-source builds are not allowed")
endif ()

# ---- Where the built plugin goes ----

macro(set_from_environment VARIABLE)
	if (NOT DEFINED ${VARIABLE} AND DEFINED ENV{${VARIABLE}})
		set(${VARIABLE} $ENV{${VARIABLE}})
	endif ()
endmacro()

set_from_environment(Fallout4Path)

# ---- Globals ----

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_STATIC_RUNTIME OFF)

# ---- Dependencies ----

if (NOT TARGET CommonLibF4)
	add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/extern/CommonLibF4/CommonLibF4" CommonLibF4)
endif ()

find_package(fmt REQUIRED CONFIG)
find_package(spdlog REQUIRED CONFIG)

# ---- The plugin ----

add_library(
	${PROJECT_NAME}
	SHARED
	src/grid.cpp
	src/grid.h
	src/main.cpp
	src/peek.cpp
	src/peek.h
	src/PCH.h
)

# The DLL keeps the mod's name rather than the repository's: it is what
# shows up in Data\F4SE\Plugins and what names the log file.
set_target_properties(${PROJECT_NAME} PROPERTIES OUTPUT_NAME "FavoritesMenuGrid")

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)

target_compile_definitions(
	${PROJECT_NAME}
	PRIVATE
		_UNICODE
		PLUGIN_NAME="Favorites Menu Grid"
		PLUGIN_LOG_NAME="FavoritesMenuGrid"
		PLUGIN_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
		PLUGIN_VERSION_MINOR=${PROJECT_VERSION_MINOR}
		PLUGIN_VERSION_PATCH=${PROJECT_VERSION_PATCH}
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(
	${PROJECT_NAME}
	PRIVATE
		CommonLibF4::CommonLibF4
		fmt::fmt
		spdlog::spdlog
		# REL::detail::sha512 checks the address library against the
		# executable; CommonLibF4 uses it but does not link it itself.
		bcrypt
		Version
)

target_precompile_headers(${PROJECT_NAME} PRIVATE src/PCH.h)

if (MSVC)
	target_compile_options(
		${PROJECT_NAME}
		PRIVATE
			/sdl
			/utf-8
			/Zi
			/permissive-
			/Zc:preprocessor
			/W4
			/WX
			"$<$<CONFIG:RELEASE>:/Zc:inline;/JMC-;/Ob3>"
	)

	target_link_options(
		${PROJECT_NAME}
		PRIVATE
			/WX
			"$<$<CONFIG:DEBUG>:/INCREMENTAL;/OPT:NOREF;/OPT:NOICF>"
			"$<$<CONFIG:RELEASE>:/INCREMENTAL:NO;/OPT:REF;/OPT:ICF;/DEBUG:FULL>"
	)
endif ()
