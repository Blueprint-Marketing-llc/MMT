//
// Created by Davide  Caroselli on 03/12/15.
//

#include <map>
#include <thread>
#include <boost/thread/shared_mutex.hpp>

#include "TranslationTask.h"
#include "legacy/ThreadPool.h"
#include <decoder/MosesDecoder.h>
#include "Translator.h"
#include "legacy/Parameter.h"
#include "System.h"
#include "FF/FeatureFunction.h"
#include "FF/StatefulFeatureFunction.h"
#include "Weights.h"

using namespace mmt;
using namespace mmt::decoder;

namespace mmt {
    namespace decoder {
        class MosesDecoderImpl : public MosesDecoder {
            //MosesServer::JNITranslator m_translator;
            std::vector<feature_t> m_features;
            std::vector<IncrementalModel *> m_incrementalModels;
            Moses2::System m_system;
            Moses2::ThreadPool m_pool;

            std::map<uint64_t, std::map<std::string, float>> m_sessionContext;
            boost::shared_mutex m_sessionsMut;
            static constexpr size_t kGlobalSession = 0; // empty-context default session
            size_t m_isession = 1;

            std::map<std::string, std::vector<float>> m_featureWeights;
            Moses2::Weights m_weights;
            boost::shared_mutex m_featureWeightsMut;

        public:

            MosesDecoderImpl(Moses2::Parameter params, Aligner *aligner, Vocabulary *vocabulary);

            virtual std::vector<feature_t> getFeatures() override;

            virtual std::vector<float> getFeatureWeights(feature_t &feature) override;

            virtual void
            setDefaultFeatureWeights(const std::map<std::string, std::vector<float>> &featureWeights) override;

            virtual int64_t openSession(const std::map<std::string, float> &translationContext,
                                        const std::map<std::string, std::vector<float>> *featureWeights = NULL) override;

            virtual void closeSession(uint64_t session) override;

            virtual translation_t translate(const std::string &text, uint64_t session,
                                            const std::map<std::string, float> *translationContext,
                                            size_t nbestListSize) override;

            virtual void Add(const updateid_t &id, const domain_t domain, const std::vector<wid_t> &source,
                             const std::vector<wid_t> &target, const alignment_t &alignment) override;

            virtual std::unordered_map<stream_t, seqid_t> GetLatestUpdatesIdentifier() override;
        };
    }
}

MosesDecoder *MosesDecoder::createInstance(const char *inifile, Aligner *aligner, Vocabulary *vocabulary) {
    const char *argv[2] = {"-f", inifile};

    Moses2::Parameter params;

    if (!params.LoadParam(2, argv))
        return NULL;

    // initialize all "global" variables, which are stored in System
    // note: this also loads models such as the language model, etc.

    return new MosesDecoderImpl(params, aligner, vocabulary);
}

MosesDecoderImpl::MosesDecoderImpl(Moses2::Parameter params, Aligner *aligner, Vocabulary *vocabulary) :
    m_features(), m_system(params, aligner, vocabulary),
    m_pool(m_system.options.server.numThreads, m_system.cpuAffinityOffset, m_system.cpuAffinityOffsetIncr)
{
    m_system.options.nbest.nbest_size = 1; // must be non-zero so Scores collects individual scores, and we can get n-best lists
    const std::vector<const Moses2::FeatureFunction *> &ffs = m_system.featureFunctions.GetFeatureFunctions();
    for (size_t i = 0; i < ffs.size(); ++i) {
        const Moses2::FeatureFunction *feature = ffs[i];
        feature_t f;
        f.name = feature->GetName();
        f.stateless = dynamic_cast<const Moses2::StatefulFeatureFunction *>(feature) != nullptr;
        f.tunable = feature->IsTuneable();
        f.ptr = (void *) feature;

        m_features.push_back(f);
        mmt::IncrementalModel *model = feature->GetIncrementalModel();
        if (model)
            m_incrementalModels.push_back(model);
    }

    m_weights = m_system.GetWeights();
    m_sessionContext[kGlobalSession] = std::map<std::string, float>();
}

std::vector<feature_t> MosesDecoderImpl::getFeatures() {
    return m_features;
}

std::vector<float> MosesDecoderImpl::getFeatureWeights(feature_t &_feature) {
    boost::shared_lock<boost::shared_mutex> lock(m_featureWeightsMut);

    Moses2::FeatureFunction *feature = (Moses2::FeatureFunction *) _feature.ptr;
    std::vector<Moses2::SCORE> weights;

    if (feature->IsTuneable()) {
        weights = m_weights.GetWeights(*feature);

        for (size_t i = 0; i < feature->GetNumScores(); ++i) {
            if (!feature->IsTuneable()) {
                weights[i] = UNTUNEABLE_COMPONENT;
            }
        }
    }

    return weights;
}

void MosesDecoderImpl::setDefaultFeatureWeights(const std::map<std::string, std::vector<float>> &featureWeights) {
    boost::unique_lock<boost::shared_mutex> lock(m_featureWeightsMut);
    m_featureWeights = featureWeights;
    m_weights.SetWeights(m_system.featureFunctions, featureWeights);
}

int64_t MosesDecoderImpl::openSession(const std::map<std::string, float> &translationContext,
                                      const std::map<std::string, std::vector<float>> *featureWeights) {

    // note: feature weights are now ignored. use setDefaultFeatureWeights() instead.
    // TODO: add docs, assert featureWeights==nullptr or remove it entirely

    boost::unique_lock<boost::shared_mutex> lock(m_sessionsMut);
    uint64_t session = m_isession++;
    m_sessionContext[session] = translationContext;
    return session;
}

void MosesDecoderImpl::closeSession(uint64_t session) {
    boost::unique_lock<boost::shared_mutex> lock(m_sessionsMut);
    if(session != kGlobalSession)
        m_sessionContext.erase(session);
}

translation_t MosesDecoderImpl::translate(const std::string &text, uint64_t session,
                                          const std::map<std::string, float> *translationContext,
                                          size_t nbestListSize) {

    std::map<std::string, float> sessionContext;
    if(!translationContext) {
        boost::shared_lock<boost::shared_mutex> lock(m_sessionsMut);
        sessionContext = m_sessionContext[session];
        translationContext = &sessionContext;
    }

    Moses2::Weights weights;
    {
        boost::shared_lock<boost::shared_mutex> lock(m_featureWeightsMut);
        weights = m_weights;
    }

    Moses2::TranslationResponse response;
    boost::shared_ptr<Moses2::TranslationTask> task(new Moses2::TranslationTask(m_system, text, 0));
    if(translationContext)
        task->SetContextWeights(*translationContext);
    task->SetWeights(weights);
    task->SetNBestListSize(nbestListSize);
    task->SetResultCallback([&response](Moses2::TranslationResponse result) {
        // called from different thread
        response = result;
    });
    m_pool.Submit(task);
    task->Join(); // synchronization point for result callback

    translation_t translation;

    translation.text = response.text;
    for (auto h: response.hypotheses)
        translation.hypotheses.push_back(hypothesis_t{h.text, h.score, h.fvals});
    translation.session = response.session;
    translation.alignment = response.alignment;

    return translation;
}

void MosesDecoderImpl::Add(const updateid_t &id, const domain_t domain, const std::vector<wid_t> &source,
                           const std::vector<wid_t> &target, const alignment_t &alignment) {
    for (size_t i = 0; i < m_incrementalModels.size(); ++i) {
        m_incrementalModels[i]->Add(id, domain, source, target, alignment);
    }
}

unordered_map<stream_t, seqid_t> MosesDecoderImpl::GetLatestUpdatesIdentifier() {
    vector<unordered_map<stream_t, seqid_t>> maps;

    stream_t maxStreamId = -1;
    for (auto it = m_incrementalModels.begin(); it != m_incrementalModels.end(); ++it) {
        unordered_map<stream_t, seqid_t> map = (*it)->GetLatestUpdatesIdentifier();
        maps.push_back(map);

        for (auto entry = map.begin(); entry != map.end(); ++entry)
            maxStreamId = max(maxStreamId, entry->first);
    }

    unordered_map<stream_t, seqid_t> result;

    for (auto map = maps.begin(); map != maps.end(); ++map) {
        for (stream_t streamId = 0; streamId <= maxStreamId; ++streamId) {
            auto entry = map->find(streamId);

            if (entry == map->end()) {
                result[streamId] = -1;
            } else {
                auto e = result.emplace(streamId, entry->second);
                if (!e.second)
                    e.first->second = min(e.first->second, entry->second);
            }
        }
    }

    return result;
}

constexpr size_t MosesDecoderImpl::kGlobalSession;
