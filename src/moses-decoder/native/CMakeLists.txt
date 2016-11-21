cmake_minimum_required(VERSION 3.2)
project(mmt-decoder)

# where to find cmake_modules modules
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake_modules")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

# Moses definitions
add_definitions(-DMAX_NUM_FACTORS=1 -DKENLM_MAX_ORDER=6 -DWITH_THREADS)

set(SOURCE_FILES
        javah/eu_modernmt_decoder_moses_MosesDecoder.h java/eu_modernmt_decoder_moses_MosesDecoder.cpp
        java/JMosesFeature.h java/JMosesFeature.cpp
        java/JTranslation.h java/JTranslation.cpp)

include_directories(${CMAKE_SOURCE_DIR})
#include_directories(${MMT_INCLUDE_DIR})
include_directories(include)

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES})
install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)

file(GLOB executables executables/*.cpp)
foreach(exefile ${executables})
    get_filename_component(exe ${exefile} NAME_WE)
    add_executable(${exe} ${exefile})
    target_link_libraries(${exe} ${PROJECT_NAME})
    target_link_libraries(${exe} moses2)  # TODO(david): replace with MOSES2_SOURCE_FILES / object bundle.
endforeach()

# External Libraries

## Boost
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

find_package(Boost 1.55.0 REQUIRED COMPONENTS filesystem thread system program_options iostreams)
include_directories(${Boost_INCLUDE_DIRS})
target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES})

## JNI
find_package(JNI REQUIRED)
include_directories(${JNI_INCLUDE_DIRS})

## ZLIB
find_package(ZLIB REQUIRED)

## MMT interpolated LM
if (USE_MMTILM)
    find_package(Rocksdb REQUIRED)
    find_package(MMTCommons REQUIRED)
    find_package(MMTILM REQUIRED)
    add_definitions(-DLM_MMTILM=true)

    include_directories(${Rocksdb_INCLUDE_DIRS})
    include_directories(${MMTILM_INCLUDE_DIRS})
    include_directories(${MMTCommons_INCLUDE_DIR})
endif (USE_MMTILM)

## MMT sampling phrasetable
if (USE_SAPT)
    find_package(Rocksdb REQUIRED)
    find_package(SAPT REQUIRED)
    find_package(MMTCommons REQUIRED)
    add_definitions(-DSAPT=true)

    include_directories(${Rocksdb_INCLUDE_DIRS})
    include_directories(${SAPT_INCLUDE_DIRS})
    include_directories(${MMTCommons_INCLUDE_DIR})
endif (USE_SAPT)

install(FILES include/decoder/MosesDecoder.h DESTINATION include)

add_subdirectory(decoder)
target_link_libraries(${PROJECT_NAME} moses2)  # TODO(david): replace with MOSES2_SOURCE_FILES / object bundle.

