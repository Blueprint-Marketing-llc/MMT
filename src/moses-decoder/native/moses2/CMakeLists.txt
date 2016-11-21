cmake_minimum_required(VERSION 3.2)
project(moses2)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/cmake/")

set(CMAKE_MACOSX_RPATH 1)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_INSTALL_PREFIX}/bin)
set(LIBRARY_OUTPUT_PATH ${CMAKE_INSTALL_PREFIX}/lib)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

# Moses definitions
# -DHAVE_XMLRPC_C
add_definitions(-DMAX_NUM_FACTORS=1 -DKENLM_MAX_ORDER=6 -DWITH_THREADS)

include_directories(.)
include_directories(legacy)
#include_directories(../..)

## XMLRPC
#find_package(XMLRPC REQUIRED)
#include_directories(${XMLRPC_INCLUDE_DIRS})

## Boost
set(Boost_USE_STATIC_LIBS OFF)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)

find_package(Boost 1.55.0 REQUIRED COMPONENTS filesystem iostreams thread system program_options)
include_directories(${Boost_INCLUDE_DIRS})

## ZLIB
find_package(ZLIB REQUIRED)

if (USE_MMTILM)
    find_package(ROCKSDB REQUIRED)
    find_package(MMTCommons REQUIRED)
    find_package(MMTILM REQUIRED)
    add_definitions(-DLM_MMTILM=true)
endif (USE_MMTILM)

if (USE_SAPT)
    find_package(ROCKSDB REQUIRED)
    find_package(SAPT REQUIRED)
    find_package(MMTCommons REQUIRED)
    add_definitions(-DSAPT=true)
endif (USE_SAPT)

if (USE_MMTILM)
    include_directories(${ROCKSDB_INCLUDE_DIRS})
    include_directories(${MMTILM_INCLUDE_DIRS})
    include_directories(${MMTCommons_INCLUDE_DIR})
endif (USE_MMTILM)

if (USE_SAPT)
    include_directories(${ROCKSDB_INCLUDE_DIRS})
    include_directories(${SAPT_INCLUDE_DIRS})
    include_directories(${MMTCommons_INCLUDE_DIR})
endif (USE_SAPT)


set(SOURCE_FILES
#    defer/CubePruningBitmapStack/Misc.cpp
#    defer/CubePruningBitmapStack/Misc.h
#    defer/CubePruningBitmapStack/Search.cpp
#    defer/CubePruningBitmapStack/Search.h
#    defer/CubePruningBitmapStack/Stack.cpp
#    defer/CubePruningBitmapStack/Stack.h
#    defer/CubePruningCardinalStack/Misc.cpp
#    defer/CubePruningCardinalStack/Misc.h
#    defer/CubePruningCardinalStack/Search.cpp
#    defer/CubePruningCardinalStack/Search.h
#    defer/CubePruningCardinalStack/Stack.cpp
#    defer/CubePruningCardinalStack/Stack.h
#    defer/CubePruningPerBitmap/Misc.cpp
#    defer/CubePruningPerBitmap/Misc.h
#    defer/CubePruningPerBitmap/Search.cpp
#    defer/CubePruningPerBitmap/Search.h
#    defer/CubePruningPerBitmap/Stacks.cpp
#    defer/CubePruningPerBitmap/Stacks.h
#    defer/CubePruningPerMiniStack/Misc.cpp
#    defer/CubePruningPerMiniStack/Misc.h
#    defer/CubePruningPerMiniStack/Search.cpp
#    defer/CubePruningPerMiniStack/Search.h
#    defer/CubePruningPerMiniStack/Stacks.cpp
#    defer/CubePruningPerMiniStack/Stacks.h
    FF/LexicalReordering/BidirectionalReorderingState.cpp
    FF/LexicalReordering/BidirectionalReorderingState.h
    FF/LexicalReordering/HReorderingBackwardState.cpp
    FF/LexicalReordering/HReorderingBackwardState.h
    FF/LexicalReordering/HReorderingForwardState.cpp
    FF/LexicalReordering/HReorderingForwardState.h
    FF/LexicalReordering/LexicalReordering.cpp
    FF/LexicalReordering/LexicalReordering.h
    FF/LexicalReordering/LRModel.cpp
    FF/LexicalReordering/LRModel.h
    FF/LexicalReordering/LRState.cpp
    FF/LexicalReordering/LRState.h
    FF/LexicalReordering/PhraseBasedReorderingState.cpp
    FF/LexicalReordering/PhraseBasedReorderingState.h
    FF/LexicalReordering/ReorderingStack.cpp
    FF/LexicalReordering/ReorderingStack.h
    FF/OSM/KenOSM.cpp
    FF/OSM/KenOSM.h
    FF/OSM/OpSequenceModel.cpp
    FF/OSM/OpSequenceModel.h
    FF/OSM/osmHyp.cpp
    FF/OSM/osmHyp.h
    FF/Distortion.cpp
    FF/Distortion.h
    FF/FeatureFunction.cpp
    FF/FeatureFunction.h
    FF/FeatureFunctions.cpp
    FF/FeatureFunctions.h
    FF/FeatureRegistry.cpp
    FF/FeatureRegistry.h
    FF/FFState.cpp
    FF/FFState.h
    FF/PhrasePenalty.cpp
    FF/PhrasePenalty.h
    FF/PointerState.cpp
    FF/PointerState.h
    FF/SkeletonStatefulFF.cpp
    FF/SkeletonStatefulFF.h
    FF/SkeletonStatelessFF.cpp
    FF/SkeletonStatelessFF.h
    FF/StatefulFeatureFunction.cpp
    FF/StatefulFeatureFunction.h
    FF/StatelessFeatureFunction.cpp
    FF/StatelessFeatureFunction.h
    FF/WordPenalty.cpp
    FF/WordPenalty.h
    legacy/Bitmap.cpp
    legacy/Bitmap.h
    legacy/Bitmaps.cpp
    legacy/Bitmaps.h
    legacy/lm/bhiksha.cc
    legacy/lm/binary_format.cc
    legacy/lm/config.cc
    legacy/lm/lm_exception.cc
    legacy/lm/model.cc
    legacy/lm/quantize.cc
    legacy/lm/read_arpa.cc
    legacy/lm/search_hashed.cc
    legacy/lm/search_trie.cc
    legacy/lm/sizes.cc
    legacy/lm/trie.cc
    legacy/lm/trie_sort.cc
    legacy/lm/value_build.cc
    legacy/lm/virtual_interface.cc
    legacy/lm/vocab.cc
    legacy/util/bit_packing.cc
    legacy/util/ersatz_progress.cc
    legacy/util/exception.cc
    legacy/util/file.cc
    legacy/util/file_piece.cc
    legacy/util/float_to_string.cc
    legacy/util/integer_to_string.cc
    legacy/util/mmap.cc
    legacy/util/murmur_hash.cc
    legacy/util/parallel_read.cc
    legacy/util/pool.cc
    legacy/util/random.cc
    legacy/util/read_compressed.cc
    legacy/util/scoped.cc
    legacy/util/string_piece.cc
    legacy/util/usage.cc
    legacy/util/double-conversion/bignum.cc
    legacy/util/double-conversion/bignum-dtoa.cc
    legacy/util/double-conversion/cached-powers.cc
    legacy/util/double-conversion/diy-fp.cc
    legacy/util/double-conversion/double-conversion.cc
    legacy/util/double-conversion/fast-dtoa.cc
    legacy/util/double-conversion/fixed-dtoa.cc
    legacy/util/double-conversion/strtod.cc
    legacy/Factor.cpp
    legacy/Factor.h
    legacy/FactorCollection.cpp
    legacy/FactorCollection.h
    legacy/gzfilebuf.h
    legacy/InputFileStream.cpp
    legacy/InputFileStream.h
    legacy/Matrix.cpp
    legacy/Matrix.h
    legacy/OutputCollector.h
    legacy/OutputFileStream.cpp
    legacy/OutputFileStream.h
    legacy/Parameter.cpp
    legacy/Parameter.h
    legacy/Range.cpp
    legacy/Range.h
    legacy/ThreadPool.cpp
    legacy/ThreadPool.h
    legacy/Timer.cpp
    legacy/Timer.h
    legacy/Util2.cpp
    legacy/Util2.h
    LM/GPULM.cpp
    LM/GPULM.h
    LM/KENLM.cpp
    LM/KENLM.h
    LM/KENLMBatch.cpp
    LM/KENLMBatch.h
    LM/LanguageModel.cpp
    LM/LanguageModel.h
#    LM/LanguageModelDALM.cpp
#    LM/LanguageModelDALM.h
    LM/MMTInterpolatedLM.cpp
    LM/MMTInterpolatedLM.h
    mmt/MosesDecoder.cpp
    mmt/MosesDecoder.h
    MorphoTrie/MorphTrie.h
    MorphoTrie/Node.h
    MorphoTrie/utils.h
    parameters/AllOptions.cpp
    parameters/AllOptions.h
    parameters/BeamSearchOptions.h
    parameters/BookkeepingOptions.cpp
    parameters/BookkeepingOptions.h
    parameters/ContextParameters.cpp
    parameters/ContextParameters.h
    parameters/CubePruningOptions.cpp
    parameters/CubePruningOptions.h
    parameters/InputOptions.cpp
    parameters/InputOptions.h
    parameters/LMBR_Options.cpp
    parameters/LMBR_Options.h
    parameters/LookupOptions.h
    parameters/MBR_Options.cpp
    parameters/MBR_Options.h
    parameters/NBestOptions.cpp
    parameters/NBestOptions.h
    parameters/OOVHandlingOptions.cpp
    parameters/OOVHandlingOptions.h
    parameters/OptionsBaseClass.cpp
    parameters/OptionsBaseClass.h
    parameters/ReorderingOptions.cpp
    parameters/ReorderingOptions.h
    parameters/ReportingOptions.cpp
    parameters/ReportingOptions.h
    parameters/SearchOptions.cpp
    parameters/SearchOptions.h
    parameters/ServerOptions.cpp
    parameters/ServerOptions.h
    parameters/SyntaxOptions.cpp
    parameters/SyntaxOptions.h
    PhraseBased/Batch/Search.cpp
    PhraseBased/Batch/Search.h
    PhraseBased/Batch/Stack.cpp
    PhraseBased/Batch/Stack.h
    PhraseBased/Batch/Stacks.cpp
    PhraseBased/Batch/Stacks.h
    PhraseBased/CubePruningMiniStack/Misc.cpp
    PhraseBased/CubePruningMiniStack/Misc.h
    PhraseBased/CubePruningMiniStack/Search.cpp
    PhraseBased/CubePruningMiniStack/Search.h
    PhraseBased/CubePruningMiniStack/Stack.cpp
    PhraseBased/CubePruningMiniStack/Stack.h
    PhraseBased/Normal/Search.cpp
    PhraseBased/Normal/Search.h
    PhraseBased/Normal/Stack.cpp
    PhraseBased/Normal/Stack.h
    PhraseBased/Normal/Stacks.cpp
    PhraseBased/Normal/Stacks.h
    PhraseBased/Hypothesis.cpp
    PhraseBased/Hypothesis.h
    PhraseBased/InputPath.cpp
    PhraseBased/InputPath.h
    PhraseBased/InputPaths.cpp
    PhraseBased/InputPaths.h
    PhraseBased/Manager.cpp
    PhraseBased/Manager.h
    PhraseBased/PhraseImpl.cpp
    PhraseBased/PhraseImpl.h
    PhraseBased/ReorderingConstraint.cpp
    PhraseBased/ReorderingConstraint.h
    PhraseBased/Search.cpp
    PhraseBased/Search.h
    PhraseBased/Sentence.cpp
    PhraseBased/Sentence.h
    PhraseBased/TargetPhraseImpl.cpp
    PhraseBased/TargetPhraseImpl.h
    PhraseBased/TargetPhrases.cpp
    PhraseBased/TargetPhrases.h
    PhraseBased/TrellisPath.cpp
    PhraseBased/TrellisPath.h
    SCFG/nbest/KBestExtractor.cpp
    SCFG/nbest/KBestExtractor.h
    SCFG/nbest/NBest.cpp
    SCFG/nbest/NBest.h
    SCFG/nbest/NBestColl.cpp
    SCFG/nbest/NBestColl.h
    SCFG/nbest/NBests.cpp
    SCFG/nbest/NBests.h
    SCFG/ActiveChart.cpp
    SCFG/ActiveChart.h
    SCFG/Hypothesis.cpp
    SCFG/Hypothesis.h
    SCFG/InputPath.cpp
    SCFG/InputPath.h
    SCFG/InputPaths.cpp
    SCFG/InputPaths.h
    SCFG/Manager.cpp
    SCFG/Manager.h
    SCFG/Misc.cpp
    SCFG/Misc.h
    SCFG/PhraseImpl.cpp
    SCFG/PhraseImpl.h
    SCFG/Sentence.cpp
    SCFG/Sentence.h
    SCFG/Stack.cpp
    SCFG/Stack.h
    SCFG/Stacks.cpp
    SCFG/Stacks.h
    SCFG/TargetPhraseImpl.cpp
    SCFG/TargetPhraseImpl.h
    SCFG/TargetPhrases.cpp
    SCFG/TargetPhrases.h
    SCFG/Word.cpp
    SCFG/Word.h
#    server/Server.cpp
#    server/Server.h
#    server/TranslationRequest.cpp
#    server/TranslationRequest.h
#    server/Translator.cpp
#    server/Translator.h
    TranslationModel/CompactPT/BlockHashIndex.cpp
    TranslationModel/CompactPT/BlockHashIndex.h
    TranslationModel/CompactPT/CanonicalHuffman.h
    TranslationModel/CompactPT/CmphStringVectorAdapter.cpp
    TranslationModel/CompactPT/CmphStringVectorAdapter.h
    TranslationModel/CompactPT/LexicalReorderingTableCompact.cpp
    TranslationModel/CompactPT/LexicalReorderingTableCompact.h
    TranslationModel/CompactPT/ListCoders.h
    TranslationModel/CompactPT/MmapAllocator.h
    TranslationModel/CompactPT/MonotonicVector.h
    TranslationModel/CompactPT/MurmurHash3.cpp
    TranslationModel/CompactPT/MurmurHash3.h
    TranslationModel/CompactPT/PackedArray.h
    TranslationModel/CompactPT/PhraseDecoder.cpp
    TranslationModel/CompactPT/PhraseDecoder.h
    TranslationModel/CompactPT/PhraseTableCompact.cpp
    TranslationModel/CompactPT/PhraseTableCompact.h
    TranslationModel/CompactPT/StringVector.h
    TranslationModel/CompactPT/TargetPhraseCollectionCache.cpp
    TranslationModel/CompactPT/TargetPhraseCollectionCache.h
    TranslationModel/CompactPT/ThrowingFwrite.cpp
    TranslationModel/CompactPT/ThrowingFwrite.h
    TranslationModel/Memory/Node.h
    TranslationModel/Memory/PhraseTableMemory.cpp
    TranslationModel/Memory/PhraseTableMemory.h
    TranslationModel/ProbingPT/hash.cpp
    TranslationModel/ProbingPT/hash.hh
    TranslationModel/ProbingPT/line_splitter.cpp
    TranslationModel/ProbingPT/line_splitter.hh
    TranslationModel/ProbingPT/probing_hash_utils.cpp
    TranslationModel/ProbingPT/probing_hash_utils.hh
    TranslationModel/ProbingPT/ProbingPT.cpp
    TranslationModel/ProbingPT/ProbingPT.h
    TranslationModel/ProbingPT/querying.cpp
    TranslationModel/ProbingPT/querying.hh
    TranslationModel/ProbingPT/StoreTarget.cpp
    TranslationModel/ProbingPT/StoreTarget.h
    TranslationModel/ProbingPT/StoreVocab.cpp
    TranslationModel/ProbingPT/StoreVocab.h
    TranslationModel/ProbingPT/storing.cpp
    TranslationModel/ProbingPT/storing.hh
    TranslationModel/ProbingPT/vocabid.cpp
    TranslationModel/ProbingPT/vocabid.hh
    TranslationModel/PhraseTable.cpp
    TranslationModel/PhraseTable.h
    TranslationModel/PhraseTableSADB.cpp
    TranslationModel/PhraseTableSADB.h
    TranslationModel/Transliteration.cpp
    TranslationModel/Transliteration.h
    TranslationModel/UnknownWordPenalty.cpp
    TranslationModel/UnknownWordPenalty.h
    AlignmentInfo.cpp
    AlignmentInfo.h
    AlignmentInfoCollection.cpp
    AlignmentInfoCollection.h
    ArcLists.cpp
    ArcLists.h
    Array.h
    EstimatedScores.cpp
    EstimatedScores.h
    HypothesisBase.cpp
    HypothesisBase.h
    HypothesisColl.cpp
    HypothesisColl.h
    InputPathBase.cpp
    InputPathBase.h
    InputPathsBase.cpp
    InputPathsBase.h
    InputType.cpp
    InputType.h
    Main.h
    ManagerBase.cpp
    ManagerBase.h
    MemPool.cpp
    MemPool.h
    Phrase.cpp
    Phrase.h
    PhraseImplTemplate.h
    pugiconfig.hpp
    pugixml.cpp
    pugixml.hpp
    Recycler.cpp
    Recycler.h
    Scores.cpp
    Scores.h
    SubPhrase.cpp
    SubPhrase.h
    System.cpp
    System.h
    TargetPhrase.cpp
    TargetPhrase.h
    TranslationTask.cpp
    TranslationTask.h
    TrellisPaths.cpp
    TrellisPaths.h
    TypeDef.cpp
    TypeDef.h
    Vector.cpp
    Vector.h
    Weights.cpp
    Weights.h
    Word.cpp
    Word.h mmt/Translator.h)

#add_executable(moses2 ${SOURCE_FILES} Main.cpp)
add_library(moses2 SHARED ${SOURCE_FILES})
target_link_libraries(moses2 pthread)
#target_link_libraries(moses2 ${XMLRPC_LIBRARIES})
target_link_libraries(moses2 ${Boost_LIBRARIES})
target_link_libraries(moses2 ${ZLIB_LIBRARIES})

if (USE_MMTILM)
    target_link_libraries(moses2 ${ROCKSDB_LIBRARIES})
    target_link_libraries(moses2 ${MMTILM_LIBRARIES})
endif (USE_MMTILM)

if (USE_SAPT)
    target_link_libraries(moses2 ${SAPT_LIBRARIES})
endif (USE_SAPT)

# currently just a dummy so  $ make install  runs:
install(FILES mmt/MosesDecoder.h DESTINATION include)



add_executable(test-moses2 ${SOURCE_FILES} mmt/test_mosesdecoder.cpp)
target_link_libraries(test-moses2 pthread)
#target_link_libraries(test-moses2 ${XMLRPC_LIBRARIES})
target_link_libraries(test-moses2 ${Boost_LIBRARIES})
target_link_libraries(test-moses2 ${ZLIB_LIBRARIES})

target_link_libraries(test-moses2 ${ROCKSDB_LIBRARIES})
target_link_libraries(test-moses2 ${MMTILM_LIBRARIES})

target_link_libraries(test-moses2 ${SAPT_LIBRARIES})
