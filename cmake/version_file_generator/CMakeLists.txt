cmake_minimum_required(VERSION 3.2)

project(version_file_generator)
message("Building version_file_generator.")

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
	message("Building with Clang.")
	set(CLANG ON)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
	message("Building with gcc.")
	set(GCC ON)
endif()

if(MSVC)
	if(CLANG)
		message("Building with Clang on Windows.")
	else()
		set(MSVC_SPECIFIC ON)
		message("Building with MSVC specifically.")
	endif()
endif()

if(MSVC_SPECIFIC)
	set(CMAKE_EXE_LINKER_FLAGS
		"${CMAKE_EXE_LINKER_FLAGS} /LTCG"
	)

	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /GL")
endif()

if (CLANG AND NOT MSVC)
	set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fuse-ld=lld ")
endif()

if(MSVC)
	set(CMAKE_CXX_FLAGS
		"${CMAKE_CXX_FLAGS} /Ob2 /Oi /O2 /permissive- /std:c++latest"
	)
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++1z")
endif()

if (CLANG AND NOT MSVC)
	set(USE_LIBCXX ON)
endif()

if(USE_LIBCXX)
	# Looks like stdlib=libc++ is duplicated for the linker as well,
	# so we only have to add it to CXX.
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++")
endif()

include_directories("../../src/")

if(UNIX)
    add_definitions(-DPLATFORM_UNIX=1)
elseif(MSVC OR CLANG_WINDOWS)
	add_definitions(-DPLATFORM_WINDOWS=1)

	add_definitions(
		-D_WINSOCK_DEPRECATED_NO_WARNINGS
		-D_CRT_SECURE_NO_WARNINGS
		-D_SCL_SECURE_NO_WARNINGS
		-DUNICODE
	  )
endif()

add_executable(version_file_generator 
	"version_file_generator.cpp"
	"../../src/augs/misc/time_utils.cpp"
)

if (MSVC)

elseif (CLANG)
	target_link_libraries(version_file_generator c++fs c++experimental)
elseif(GCC)
	target_link_libraries(version_file_generator stdc++fs)
endif()