#
# Try to find the FreeImage library and include path.
# Once done this will define
#
# FREEIMAGE_FOUND
# FREEIMAGE_INCLUDE_PATH
# FREEIMAGE_LIBRARY
# FREEIMAGE_SHAREDLIB (Win32 only)
#

if (WIN32)
	find_path( FREEIMAGE_INCLUDE_PATH FreeImage.h
		${FREEIMAGE_ROOT_DIR}/include
		${FREEIMAGE_ROOT_DIR}
		DOC "The directory where FreeImage.h resides")
	find_library( FREEIMAGE_LIBRARY
		NAMES FreeImage freeimage
		PATHS
		${FREEIMAGE_ROOT_DIR}/lib
		${FREEIMAGE_ROOT_DIR}
		DOC "The FreeImage library")
	find_file( FREEIMAGE_SHAREDLIB
		NAMES freeimage.DLL
		PATHS
		${FREEIMAGE_ROOT_DIR}/dll
		${FREEIMAGE_ROOT_DIR}
	)
else (WIN32)
	find_path( FREEIMAGE_INCLUDE_PATH FreeImage.h
		/usr/include
		/usr/local/include
		/sw/include
		/opt/local/include
		DOC "The directory where FreeImage.h resides")
	find_library( FREEIMAGE_LIBRARY
		NAMES FreeImage freeimage
		PATHS
		/usr/lib64
		/usr/lib
		/usr/local/lib64
		/usr/local/lib
		/sw/lib
		/opt/local/lib
		DOC "The FreeImage library")
endif (WIN32)

find_package_handle_standard_args( FreeImage
		DEFAULT_MSG
		FREEIMAGE_LIBRARY
		FREEIMAGE_INCLUDE_PATH
)

mark_as_advanced(
	FREEIMAGE_FOUND
	FREEIMAGE_LIBRARY
	FREEIMAGE_INCLUDE_PATH
  FREEIMAGE_SHAREDLIB)
