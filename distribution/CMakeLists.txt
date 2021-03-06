# Setup global packaging parameters ...
SET(CPACK_PACKAGE_DESCRIPTION "XTrackCad is a CAD program for designing model railroad layouts.")
SET(CPACK_PACKAGE_EXECUTABLES "xtrkcad;XTrkCAD ${XTRKCAD_VERSION}")
SET(CPACK_PACKAGE_FILE_NAME "xtrkcad-setup-${XTRKCAD_VERSION}")
SET(CPACK_PACKAGE_INSTALL_DIRECTORY "XTrkCAD ${XTRKCAD_VERSION}")
SET(CPACK_PACKAGE_INSTALL_REGISTRY_KEY "XTrkCAD ${XTRKCAD_VERSION}")
SET(CPACK_PACKAGE_NAME "xtrkcad")
SET(CPACK_PACKAGE_VENDOR "XTrackCAD Project")
SET(CPACK_PACKAGE_VERSION ${XTRKCAD_VERSION})
SET(CPACK_PACKAGE_VERSION_MAJOR ${XTRKCAD_MAJOR_VERSION})
SET(CPACK_PACKAGE_VERSION_MINOR ${XTRKCAD_MINOR_VERSION})
SET(CPACK_PACKAGE_VERSION_PATCH ${XTRKCAD_RELEASE_VERSION})
SET(CPACK_RESOURCE_FILE_LICENSE "${XTrkCAD_SOURCE_DIR}/app/COPYING")
SET(CPACK_SOURCE_GENERATOR "ZIP;TGZ")
SET(CPACK_SOURCE_PACKAGE_FILE_NAME "xtrkcad-source-${XTRKCAD_VERSION}")

CONFIGURE_FILE(${CMAKE_CURRENT_SOURCE_DIR}/downloadpage.wiki.in ${CMAKE_BINARY_DIR}/downloadpage.wiki)

# Setup platform-specific packaging ...
IF(APPLE)
	ADD_SUBDIRECTORY(osx)
ENDIF(APPLE)

IF(UNIX AND NOT APPLE)
	ADD_SUBDIRECTORY(posix)
ENDIF(UNIX AND NOT APPLE)

IF(WIN32)
	ADD_SUBDIRECTORY(win32)
ENDIF(WIN32)
