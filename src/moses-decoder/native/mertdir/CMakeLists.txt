set(MERT_SOURCE_FILES
        BleuDocScorer.cpp
        BleuScorer.cpp
        CderScorer.cpp
        Data.cpp
        FeatureArray.cpp
        FeatureData.cpp
        FeatureDataIterator.cpp
        FeatureStats.cpp
        FileStream.cpp
        ForestRescore.cpp
        GzFileBuf.cpp
        HopeFearDecoder.cpp
        HwcmScorer.cpp
        Hypergraph.cpp
        HypPackEnumerator.cpp
        InternalTree.cpp
        InterpolatedScorer.cpp
        MeteorScorer.cpp
        MiraFeatureVector.cpp
        MiraWeightVector.cpp
        Optimizer.cpp
        OptimizerFactory.cpp
        Permutation.cpp
        PermutationScorer.cpp
        PerScorer.cpp
        Point.cpp
        PreProcessFilter.cpp
        ScoreArray.cpp
        ScoreData.cpp
        ScoreDataIterator.cpp
        Scorer.cpp
        ScorerFactory.cpp
        ScoreStats.cpp
        SemposOverlapping.cpp
        SemposScorer.cpp
        SentenceLevelScorer.cpp
        StatisticsBasedScorer.cpp
        Timer.cpp
        Util.cpp
        Vocabulary.cpp)

# we don't currently build these. note: need boost_test linked.
set(MERT_TEST_FILES
        BleuScorerTest.cpp
        DataTest.cpp
        FeatureDataTest.cpp
        ForestRescoreTest.cpp
        HypergraphTest.cpp
        MiraFeatureVectorTest.cpp
        NgramTest.cpp
        OptimizerFactoryTest.cpp
        PointTest.cpp
        ReferenceTest.cpp
        SingletonTest.cpp
        TimerTest.cpp
        UtilTest.cpp
        VocabularyTest.cpp
        )

#  $<TARGET_OBJECTS:double-conversion>  # doesn't work
add_library(mert_objs OBJECT ${MERT_SOURCE_FILES})

set(MERT_EXECUTABLES
        ${CMAKE_CURRENT_SOURCE_DIR}/extractor.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/mert.cpp
        PARENT_SCOPE)
