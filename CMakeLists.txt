# This is a cmake script. Process it with the CMake gui or command line utility
# to produce makefiles / Visual Studio project files on Mac OS X and Windows.
#
# To configure the build options either use the CMake gui, or run the command
# line utility including the "-i" option.

set(CMAKE_LEGACY_CYGWIN_WIN32 0)

project(webit)

cmake_minimum_required(VERSION 2.8)
# Only for version 3 and up. cmake_policy(SET CMP0042 NEW)

set (VERSION 0.5.0)

if (WIN32)
	execute_process(COMMAND cmd /c echo %DATE% %TIME% OUTPUT_VARIABLE TIMESTAMP
			OUTPUT_STRIP_TRAILING_WHITESPACE)
else (WIN32)
	execute_process(COMMAND date "+%F %T%z" OUTPUT_VARIABLE TIMESTAMP
			OUTPUT_STRIP_TRAILING_WHITESPACE)
endif (WIN32)

add_definitions (-DCMAKE -DVERSION=\"${VERSION}\" -DTIMESTAMP=\"${TIMESTAMP}\")

if (WIN32)
	set (BINDIR .)
	set (SBINDIR .)
	set (SYSCONFDIR .)
	set (LIBDIR .)
	set (INCLUDEDIR include)
	set (DATAROOTDIR share)
	set (MANDIR man)
	set (SHAREDEST .)
	add_definitions("-D_CRT_SECURE_NO_WARNINGS")
	add_definitions("-D_CRT_NONSTDC_NO_DEPRECATE")
else (WIN32)
	set (BINDIR bin)
	set (SBINDIR sbin)
	if (${CMAKE_INSTALL_PREFIX} STREQUAL /usr)
		set (SYSCONFDIR /etc/webit)
	else (${CMAKE_INSTALL_PREFIX} STREQUAL /usr)
		set (SYSCONFDIR etc/webit)
	endif (${CMAKE_INSTALL_PREFIX} STREQUAL /usr)
	
	set (LIBDIR lib${LIB_SUFFIX})
	set (CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${LIBDIR}")
	set (INCLUDEDIR include)
	set (DATAROOTDIR share)
	set (MANDIR ${DATAROOTDIR}/man)
	set (SHAREDIR ${DATAROOTDIR}/webit)
endif (WIN32)

option(WITH_TLS
	"Include SSL/TLS support?" ON)

if (${WITH_TLS} STREQUAL ON)
	find_package(OpenSSL REQUIRED)
	add_definitions("-DWITH_TLS")
else (${WITH_TLS} STREQUAL ON)
	set (OPENSSL_INCLUDE_DIR "")
endif (${WITH_TLS} STREQUAL ON)

# ========================================
# Include projects
# ========================================

add_subdirectory(src)

# ========================================
# Install config file
# ========================================

install(FILES webit.conf DESTINATION ${SYSCONFDIR})
