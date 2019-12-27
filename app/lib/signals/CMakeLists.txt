FILE(GLOB SIGNAL_FILES *.xtps)
INSTALL(
	FILES ${SIGNAL_FILES}
	DESTINATION ${XTRKCAD_SHARE_INSTALL_DIR}/signals
	)
add_subdirectory(HeadTypes)
add_subdirectory(Posts)
add_subdirectory(Signals)