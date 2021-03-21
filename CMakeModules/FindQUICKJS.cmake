# ################################################
# QUICKJS library finder
# 20210317 - Added to the edbrowser project
# Modified from Geoff's FindREADLINE.cmake.
# Defines
# QUICKJS_FOUND
# QUICKJS_LIBRARY
# QUICKJS_INCLUDE_DIR
# ################################################
set(_LIB_NAME quickjs)

FIND_PATH(QUICKJS_INCLUDE_DIR quickjs/quickjs-libc.h
    PATH_SUFFIXES include/quickjs quickjs
  )
if (MSVC)
    FIND_LIBRARY(QUICKJS_LIB_DBG NAMES ${_LIB_NAME}d)
    FIND_LIBRARY(QUICKJS_LIB_REL NAMES ${_LIB_NAME})
    if (QUICKJS_LIB_DBG AND QUICKJS_LIB_REL)
        set(QUICKJS_LIBRARY
            debug ${QUICKJS_LIB_DBG}
            optimized ${QUICKJS_LIB_REL}
            )
    else ()
        if (QUICKJS_LIB_REL)
            set(QUICKJS_LIBRARY ${QUICKJS_LIB_REL})
        endif ()
    endif ()
else ()
    FIND_LIBRARY(QUICKJS_LIBRARY
        NAMES libquickjs.a ${_LIB_NAME}
        HINTS ENV QUICKJS_ROOT
        )
endif ()

message(STATUS "*** Res: inc '${QUICKJS_INCLUDE_DIR}', lib '${QUICKJS_LIBRARY}'")

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(QUICKJS DEFAULT_MSG QUICKJS_INCLUDE_DIR QUICKJS_LIBRARY )

# eof
