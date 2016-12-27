# - Find ModernMT components
# This module defines
#  ModernMT_INCLUDE_DIRS, directory containing headers
#  ModernMT_LIBS, components libraries
#  ModernMT_FOUND, whether all requested components has been found

set(__MMT_INCLUDE_common mmt/sentence.h)
set(__MMT_INCLUDE_fastalign fastalign/FastAligner.h)
set(__MMT_INCLUDE_ilm ilm/InterpolatedLM.h)
set(__MMT_INCLUDE_decoder decoder/MosesDecoder.h)
set(__MMT_INCLUDE_sapt sapt/PhraseTable.h)

set(__MMT_LIB_common mmtc)
set(__MMT_LIB_fastalign fastalign)
set(__MMT_LIB_ilm ilm)
set(__MMT_LIB_decoder mmtdecoder)
set(__MMT_LIB_sapt sapt)

set(ModernMT_BUILD_DIR "${CMAKE_SOURCE_DIR}/../../../build")
set(ModernMT_SEARCH_HEADER_PATHS ${ModernMT_BUILD_DIR}/include)
set(ModernMT_SEARCH_LIB_PATHS ${ModernMT_BUILD_DIR}/lib)

set(ModernMT_FOUND TRUE)

foreach (COMPONENT ${ModernMT_FIND_COMPONENTS})
    set(__ModernMT_INCLUDE_DIR "__ModernMT_INCLUDE_DIR-NOTFOUND")
    set(__ModernMT_LIB "__ModernMT_LIB-NOTFOUND")

    find_path(__ModernMT_INCLUDE_DIR "${__MMT_INCLUDE_${COMPONENT}}" HINTS ${ModernMT_SEARCH_HEADER_PATHS} NO_DEFAULT_PATH)
    find_library(__ModernMT_LIB NAMES "${__MMT_LIB_${COMPONENT}}" HINTS ${ModernMT_SEARCH_LIB_PATHS} NO_DEFAULT_PATH)

    if (NOT (__ModernMT_INCLUDE_DIR AND __ModernMT_LIB))
        list(APPEND ModernMT_NOT_FOUND_COMPONENTS ${COMPONENT})
    else ()
        list(APPEND ModernMT_INCLUDE_DIRS ${__ModernMT_INCLUDE_DIR})
        list(APPEND ModernMT_LIBS ${__ModernMT_LIB})
    endif ()
endforeach (COMPONENT)

if (ModernMT_NOT_FOUND_COMPONENTS)
    set(ModernMT_ERR_MSG "Could not find the following ModernMT components:")
    foreach (COMPONENT ${ModernMT_NOT_FOUND_COMPONENTS})
        set(ModernMT_ERR_MSG "${ModernMT_ERR_MSG} ${COMPONENT}")
    endforeach (COMPONENT)

    if (ModernMT_FIND_QUIETLY)
        message(WARNING "${ModernMT_ERR_MSG}")
    else ()
        message(FATAL_ERROR "${ModernMT_ERR_MSG}")
    endif ()

    set(ModernMT_FOUND FALSE)
else (ModernMT_NOT_FOUND_COMPONENTS)
    set(ModernMT_FOUND TRUE)
endif (ModernMT_NOT_FOUND_COMPONENTS)

mark_as_advanced(
        ModernMT_INCLUDE_DIRS
        ModernMT_LIBS
        ModernMT_FOUND
)
