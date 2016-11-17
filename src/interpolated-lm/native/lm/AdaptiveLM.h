//
// Created by Nicola Bertoldi on 29/07/16.
//

#ifndef ILM_ADAPTIVELM_H
#define ILM_ADAPTIVELM_H

#include <string>
#include <db/NGramStorage.h>
#include <mmt/IncrementalModel.h>
#include "LM.h"
#include "AdaptiveLMCache.h"
#include "BufferedUpdateManager.h"

using namespace std;

namespace mmt {
    namespace ilm {

        static const float kNaturalLogZeroProbability = -20.f;

        class AdaptiveLM : public LM, public IncrementalModel {
        public:

            AdaptiveLM(const string &modelPath, uint8_t order, size_t updateBufferSize, size_t hugePageSize, double updateMaxDelay);

            /* LM */

            inline virtual float ComputeProbability(const wid_t word, const HistoryKey *historyKey,
                                                    const context_t *context,
                                                    HistoryKey **outHistoryKey) const override {
                return ComputeProbability(word, historyKey, context, outHistoryKey, NULL);
            }

            float ComputeProbability(const wid_t word, const HistoryKey *historyKey,
                                     const context_t *context, HistoryKey **outHistoryKey,
                                     AdaptiveLMCache *cache) const;

            virtual HistoryKey *MakeHistoryKey(const Phrase &phrase, HistoryKey *memory = nullptr) const override;

            virtual HistoryKey *MakeEmptyHistoryKey(HistoryKey *memory = nullptr) const override;

            virtual size_t GetHistoryKeySize() const override;

            virtual bool IsOOV(const context_t *context, const wid_t word) const override;

            /* Incremental Model */

            virtual void
            Add(const updateid_t &id, domain_t domain, const vector <wid_t> &source, const vector <wid_t> &target,
                const alignment_t &alignment) override;

            virtual unordered_map<stream_t, seqid_t> GetLatestUpdatesIdentifier() override;

            virtual void NormalizeContext(context_t *context);

        private:
            const uint8_t order;

            NGramStorage storage;
            BufferedUpdateManager updateManager;

            // Returns the probability (not in log space) of the ngram identified by the words
            // in the range [start, end] stored in Phrase.
            // "start" is the position of the least recent word to be considered;
            // it must be larger than or equal to 0 and strictly lower than ngram.size().
            // "end" is the position of the most recent word to be considered;
            // it must be larger than or equal to 0 and strictly lower than ngram.size().
            cachevalue_t ComputeProbability(const context_t *context, const Phrase &history, const wid_t word,
                                            const size_t start, const size_t end, AdaptiveLMCache *cache) const;

            cachevalue_t ComputeUnigramProbability(const context_t *context, dbkey_t wordKey) const;

            inline count_t OOVClassFrequency(const count_t dictionarySize) const;

            inline count_t OOVClassSize(const count_t dictionarySize) const;

        };

    }
}

#endif //ILM_ADAPTIVELM_H
