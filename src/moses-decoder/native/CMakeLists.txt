cmake_minimum_required(VERSION 3.2)
project(mmtdecoder)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fPIC")

# Moses definitions
add_definitions(-DMAX_NUM_FACTORS=1 -DKENLM_MAX_ORDER=6 -DWITH_THREADS -DHAVE_ZLIB)

set(SOURCE_FILES
        javah/eu_modernmt_decoder_moses_MosesDecoder.h java/eu_modernmt_decoder_moses_MosesDecoder.cpp
        java/JMosesFeature.h java/JMosesFeature.cpp
        java/JTranslation.h java/JTranslation.cpp)

include_directories(${CMAKE_SOURCE_DIR})
include_directories(${CMAKE_SOURCE_DIR}/decoder/legacy)
include_directories(include)

# External Libraries

## Boost
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

find_package(Boost 1.55.0 REQUIRED COMPONENTS filesystem thread system program_options iostreams)
include_directories(${Boost_INCLUDE_DIRS})

## JNI
find_package(JNI REQUIRED)
include_directories(${JNI_INCLUDE_DIRS})

## ZLIB
find_package(ZLIB REQUIRED)

## ModernMT
find_package(ModernMT REQUIRED COMPONENTS common ilm sapt)
include_directories(${ModernMT_INCLUDE_DIRS})

## RocksDB
find_package(Rocksdb REQUIRED)
include_directories(${Rocksdb_INCLUDE_DIRS})

# Target definition

add_subdirectory(decoder)
add_subdirectory(mertdir)

add_library(${PROJECT_NAME} SHARED ${SOURCE_FILES} $<TARGET_OBJECTS:moses2> $<TARGET_OBJECTS:moses2_legacy>)

file(GLOB executables executables/*.cpp)
foreach (exefile ${executables})
    get_filename_component(exe ${exefile} NAME_WE)
    add_executable(${exe} ${exefile})
    target_link_libraries(${exe} ${PROJECT_NAME})
    install(TARGETS ${exe} RUNTIME DESTINATION bin)
endforeach ()

# Third-party libraries linking

target_link_libraries(${PROJECT_NAME} ${Boost_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${ZLIB_LIBRARIES})
target_link_libraries(${PROJECT_NAME} ${ModernMT_LIBS})
target_link_libraries(${PROJECT_NAME} ${Rocksdb_LIBRARIES})
target_link_libraries(${PROJECT_NAME} pthread)

# Install rules

install(FILES include/decoder/MosesDecoder.h DESTINATION include)

install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION lib)

foreach (exefile ${MERT_EXECUTABLES})
    get_filename_component(exe ${exefile} NAME_WE)
    add_executable(${exe} ${exefile} $<TARGET_OBJECTS:mert_objs> $<TARGET_OBJECTS:moses2_legacy>)
    target_link_libraries(${exe} ${Boost_LIBRARIES})
    target_link_libraries(${exe} ${ZLIB_LIBRARIES})
    target_link_libraries(${exe} pthread)
    install(TARGETS ${exe} RUNTIME DESTINATION bin)
endforeach ()
