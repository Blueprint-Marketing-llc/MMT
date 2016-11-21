# - Find Rocksdb (db.h, librocksdb.a, librocksdb.so)
# This module defines
#  Rocksdb_INCLUDE_DIRS, directory containing headers
#  Rocksdb_LIBRARIES, directory containing snappy libraries
#  Rocksdb_FOUND, whether snappy has been found

set(Rocksdb_SEARCH_HEADER_PATHS ${ROCKSDB_ROOT}/include $ENV{ROCKSDB_ROOT}/include )

set(Rocksdb_SEARCH_LIB_PATHS ${ROCKSDB_ROOT}/lib $ENV{ROCKSDB_ROOT}/lib )

find_path(Rocksdb_INCLUDE_DIRS rocksdb/db.h PATHS ${Rocksdb_SEARCH_HEADER_PATHS} )

find_library(Rocksdb_LIBRARIES NAMES rocksdb PATHS ${Rocksdb_SEARCH_LIB_PATHS})

if (Rocksdb_INCLUDE_DIRS AND Rocksdb_LIBRARIES)
    set(Rocksdb_FOUND TRUE)
else ()
    set(Rocksdb_FOUND FALSE)
endif ()

if (Rocksdb_FOUND)
    if (NOT Rocksdb_FIND_QUIETLY)
        message(STATUS "Found Rocksdb: ${Rocksdb_LIBRARIES}")
    endif ()
else ()
    if (NOT Rocksdb_FIND_QUIETLY)
        set(Rocksdb_ERR_MSG "Could not find Rocksdb. Set ROCKSDB_ROOT to the RocksDB root folder (current value is '${ROCKSDB_ROOT}')")
        if (Rocksdb_FIND_REQUIRED)
            message(FATAL_ERROR "${Rocksdb_ERR_MSG}")
        else (Rocksdb_FIND_REQUIRED)
            message(STATUS "${Rocksdb_ERR_MSG}")
        endif (Rocksdb_FIND_REQUIRED)
    endif ()
endif ()

mark_as_advanced(
        Rocksdb_INCLUDE_DIRS
        Rocksdb_LIBRARIES
        Rocksdb_FOUND
)
