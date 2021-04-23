# library name
set(LIBNAME "cacaoKalAOBMC")
set(SRCNAME "KalAO_BMC")


set(SOURCEFILES
	${SRCNAME}.c)

set(INCLUDEFILES
	${SRCNAME}.h)


# DEFAULT SETTINGS
# Do not change unless needed
# =====================================================================

project(lib_${LIBNAME}_project)

# Library can be compiled from multiple source files
# Convention: the main souce file is named <libname>.c
#
add_library(${LIBNAME} SHARED ${SOURCEFILES})
target_link_libraries(${LIBNAME} PUBLIC CLIcore)

install(TARGETS ${LIBNAME} DESTINATION lib)
install(FILES ${INCLUDEFILES} DESTINATION include/${SRCNAME})

# NUVU SETTINGS
# =====================================================================
include_directories ("/opt/Boston\ Micromachines/include/")
target_link_libraries(${LIBNAME} PUBLIC /opt/Boston\ Micromachines/lib/libBMC.so)
