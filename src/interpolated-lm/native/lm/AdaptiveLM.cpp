//
// Created by Nicola Bertoldi on 29/07/16.
//

#include "AdaptiveLM.h"
#include <cmath>
#include <iostream>

using namespace std;
using namespace mmt;
using namespace mmt::ilm;

static const float kUnigramEpsilon = 1.f;
static const size_t kDictionaryUpperBound = 10000000;

namespace mmt {
    namespace ilm {
        struct AdaptiveLMHistoryKey : public HistoryKey {
            Phrase words;

            AdaptiveLMHistoryKey() {}

            AdaptiveLMHistoryKey(const Phrase &ngram) {
                words = ngram;
            }

            AdaptiveLMHistoryKey(const Phrase &ngram, size_t length) {
                if (length > 0) {
                    size_t words_length = std::min(length, ngram.size());
                    size_t offset = ngram.size() - words_length;

                    words.resize(words_length);
                    std::copy(ngram.begin() + offset, ngram.end(), words.begin());
                }
            }

            AdaptiveLMHistoryKey(Phrase history, const wid_t word, size_t length) {
                if (length > 0) {
                    size_t words_length = std::min(length, history.size() + 1);
                    size_t offset = history.size() - words_length + 1;

                    words.resize(words_length);
                    std::copy(history.begin() + offset, history.end(), words.begin());
                    words[words_length - 1] = word;
                }
            }

            virtual size_t hash() const override {
                return words.empty() ? 0 : make_key(words, words.size());
            }

            virtual bool operator==(const HistoryKey &o) const override {
                return words == ((AdaptiveLMHistoryKey &) o).words;
            }

            virtual size_t length() const override {
                return words.size();
            }
        };
    }
}

AdaptiveLM::AdaptiveLM(const string &modelPath, uint8_t order, size_t updateBufferSize, double updateMaxDelay) :
        order(order), storage(modelPath, order), updateManager(&storage, updateBufferSize, updateMaxDelay) {
}

float AdaptiveLM::ComputeProbability(const wid_t word, const HistoryKey *historyKey, const context_t *context,
                                     HistoryKey **outHistoryKey, AdaptiveLMCache *cache) const {
    if (context == nullptr || context->empty()) {
        if (outHistoryKey && *outHistoryKey)
            new (*outHistoryKey) AdaptiveLMHistoryKey();
        else if (outHistoryKey)
            *outHistoryKey = new AdaptiveLMHistoryKey();

        return kNaturalLogZeroProbability;
    }

    const AdaptiveLMHistoryKey *inKey = (AdaptiveLMHistoryKey *) historyKey;
    assert(inKey != NULL);

    cachevalue_t result = ComputeProbability(context, inKey->words, word, 0, inKey->words.size(), cache);

    if (outHistoryKey && *outHistoryKey)
      new (*outHistoryKey) AdaptiveLMHistoryKey(inKey->words, word, word == kVocabularyEndSymbol ? 0 : result.length);
    else if (outHistoryKey)
        *outHistoryKey = new AdaptiveLMHistoryKey(inKey->words, word, word == kVocabularyEndSymbol ? 0 : result.length);

    return result.probability > 0. ? log(result.probability) : kNaturalLogZeroProbability;
}

cachevalue_t AdaptiveLM::ComputeProbability(const context_t *context, const Phrase &history, const wid_t word,
                                            const size_t start, const size_t end, AdaptiveLMCache *cache) const {
    dbkey_t historyKey;
    dbkey_t ngramKey;

    if (start == end) {
        historyKey = 0;
        ngramKey = make_key(word);
    } else {
        historyKey = make_key(history, start, end - start);
        ngramKey = make_key(historyKey, word);
    }

    cachevalue_t result;

    bool isCachable = cache && cache->IsCacheable(end - start + 1);
    bool cacheHit = isCachable && cache->Get(ngramKey, &result);

    if (!cacheHit) {
        if (start == end) { // compute the probability of the unigram; the most recent word exists for sure
            result = ComputeUnigramProbability(context, ngramKey);
        } else { //compute recursively the probability of the n-gram (n > 1)
            float interpolatedFstar = 0.f;
            float interpolatedLambda = 0.f;
            uint8_t maxLength = 0;

            for (context_t::const_iterator it = context->begin(); it != context->end(); ++it) {
                counts_t domainHistoryCounts = storage.GetCounts(it->domain, historyKey);

                float fstar = 0.f;
                float lambda = 1.f;
                uint8_t length = 0;

                if (domainHistoryCounts.count > 0) {
                    count_t domainNgramCount = storage.GetCounts(it->domain, ngramKey).count;

                    if (domainNgramCount > 0) {
                        fstar = (float) domainNgramCount / (domainHistoryCounts.count + domainHistoryCounts.successors);
                        length = (uint8_t) min(end - start + 1, (size_t) (order - 1));
                    }

                    lambda = (float) domainHistoryCounts.successors /
                             (domainHistoryCounts.count + domainHistoryCounts.successors);
                }

                interpolatedFstar += it->score * fstar;
                interpolatedLambda += it->score * lambda;
                maxLength = max(maxLength, (uint8_t) length);
            }

            cachevalue_t loResult = ComputeProbability(context, history, word, start + 1, end, cache);

            result.probability = interpolatedFstar + interpolatedLambda * loResult.probability;
            result.length = max(maxLength, loResult.length);
        }

        if (isCachable)
            cache->Put(ngramKey, result);
    }

    return result;
}

// Policy for OOV:
//  OOV is considered as a word class, containing an estimated amount of virtual entries, each with frequency 1
//  OOV "symbol" is not actually inserted in the DB
//  the global frequency of the OOV_class is estimated by means of the function OOV_Count_Estimate, and depend on the actual (and current) dictionary size and on Dictionary_Upper_Bound
//  the probability of the OOV_class is computed as it were a standard word, considering its class frequency
//  the probability of one single OOV_word is computed dividing the OOV_class probability by the "population" of the class (i.e. the OOV_class frequency)
//  OOV_count frequency is virtually added in the training, so that the global size of the training is virtually incremented by this value

// Hence
//  # OOV_class frequency is equal to history_counts.successors
//  # OOV frequency is (virtually) added to the total amount of running words

// Note that the DB is not fixed, but changes over time; none of the previous values can be pre-computed

// If a Dictionary Upper Bound (DBU) larger than the actual dictionary size is given
//  then the OOV_class frequency is set to (DUB - dictionary_size);
//  otherwise the OOV_class freqeucny is set to actual dictionary size
cachevalue_t AdaptiveLM::ComputeUnigramProbability(const context_t *context, dbkey_t wordKey) const {
    bool isOOV = true;
    float interpolatedProbability = 0.f;

    for (context_t::const_iterator it = context->begin(); it != context->end(); ++it) {
        count_t wordCount; // This value includes also the occurrencies of the kVocabularyStartSymbol
        count_t uniqueWordCount;
        storage.GetWordCounts(it->domain, &uniqueWordCount, &wordCount);

        count_t oovFrequency = OOVClassFrequency(uniqueWordCount);
        count_t den = (count_t) (wordCount + oovFrequency + kUnigramEpsilon * uniqueWordCount);

        count_t unigramCount = storage.GetCounts(it->domain, wordKey).count;

        float probability;

        if (unigramCount > 0) {
            isOOV = false;
            probability = (unigramCount + kUnigramEpsilon) / den;
        } else {
            // OOV
            probability = (oovFrequency + kUnigramEpsilon) / den; //compute the probability of the whole OOV class
            probability /= OOVClassSize(uniqueWordCount);  // compute the probability of one single OOV
        }

        interpolatedProbability += it->score * probability;
    }

    cachevalue_t result;
    result.probability = interpolatedProbability;
    result.length = (uint8_t) (isOOV ? 0 : 1);

    return result;
}

HistoryKey *AdaptiveLM::MakeEmptyHistoryKey(HistoryKey *memory) const {
    if(memory)
        return new ((AdaptiveLMHistoryKey *) memory) AdaptiveLMHistoryKey();
    else
        return new AdaptiveLMHistoryKey();
}

HistoryKey *AdaptiveLM::MakeHistoryKey(const Phrase &phrase, HistoryKey *memory) const {
    if(memory)
        return new ((AdaptiveLMHistoryKey *) memory) AdaptiveLMHistoryKey(phrase, order);
    else
        return new AdaptiveLMHistoryKey(phrase, order);
}

size_t AdaptiveLM::GetHistoryKeySize() const {
  return sizeof(AdaptiveLMHistoryKey);
}

inline count_t AdaptiveLM::OOVClassFrequency(const count_t dictionarySize) const {
    return dictionarySize;
}

inline count_t AdaptiveLM::OOVClassSize(const count_t dictionarySize) const {
    if (kDictionaryUpperBound < dictionarySize) {
        return dictionarySize;
    } else {
        return (count_t) (kDictionaryUpperBound - dictionarySize);
    }
}

bool AdaptiveLM::IsOOV(const context_t *context, const wid_t word) const {
    dbkey_t key = make_key(word);

    for (context_t::const_iterator it = context->begin(); it != context->end(); ++it) {
        counts_t domainCounts = storage.GetCounts(it->domain, key);
        if (domainCounts.count > 0)
            return false;
    }

    return true;
}

void AdaptiveLM::Add(const updateid_t &id, domain_t domain, const vector<wid_t> &source, const vector<wid_t> &target,
                     const alignment_t &alignment) {
    updateManager.Add(id, domain, target);
}

vector<updateid_t> AdaptiveLM::GetLatestUpdatesIdentifier() {
    const vector<seqid_t> &streams = storage.GetStreamsStatus();

    vector<updateid_t> result;
    result.reserve(streams.size());

    for (size_t i = 0; i < streams.size(); ++i) {
        if (streams[i] != 0)
            result.push_back(updateid_t((stream_t) i, streams[i]));

    }

    return result;
}


void AdaptiveLM::NormalizeContext(context_t *context) {
    context_t ret;
    float total = 0.0;

    for (auto it = context->begin(); it != context->end(); ++it) {
        counts_t domainCounts;
        storage.GetWordCounts(it->domain, &domainCounts.count, &domainCounts.successors);

        if (domainCounts.count == 0) continue;

        total += it->score;
    }

    if (total == 0.0)
        total = 1.0f;

    for (auto it = context->begin(); it != context->end(); ++it) {
        counts_t domainCounts;
        storage.GetWordCounts(it->domain, &domainCounts.count, &domainCounts.successors);

        if (domainCounts.count == 0) continue;

        it->score /= total;

        ret.push_back(*it);
    }

    // replace new vector into old vector
    context->clear();
    //todo: check if the following insert is correct
    context->insert(context->begin(), ret.begin(), ret.end());
}
