//
// Created by Davide  Caroselli on 07/09/16.
//

#include "NGramBatch.h"
#include <stdlib.h>

using namespace mmt;
using namespace mmt::ilm;

NGramBatch::NGramBatch(uint8_t order, size_t maxSize, const vector<seqid_t> &_streams) : order(order), maxSize(maxSize),
                                                                                         size(0) {
    streams = _streams;
}

bool NGramBatch::Add(const domain_t domain, const vector<wid_t> &sentence, const count_t count) {
    if (size >= maxSize)
        return false;

    AddToBatch(domain, sentence, count);

    return true;
}

bool NGramBatch::Add(const updateid_t &id, const domain_t domain, const vector<wid_t> &sentence, const count_t count) {
    if (size >= maxSize)
        return false;

    if (!SetStreamIfValid(id.stream_id, id.sentence_id))
        return true;

    AddToBatch(domain, sentence, count);

    return true;
}

inline void NGramBatch::AddToBatch(const domain_t domain, const vector<wid_t> &sentence, const count_t count) {
    auto el = ngrams_map.emplace(domain, ngram_table_t());
    ngram_table_t &ngrams = el.first->second;

    if (el.second)
        ngrams.resize(order);

    // Create word array with start and end symbols
    size_t words_length = sentence.size() + 2;
    wid_t *words = (wid_t *) calloc(words_length, sizeof(wid_t));
    std::copy(sentence.begin(), sentence.end(), &words[1]);

    words[0] = kVocabularyStartSymbol;
    words[words_length - 1] = kVocabularyEndSymbol;

    // Fill table
    for (size_t iword = 0; iword < words_length; ++iword) {
        dbkey_t key = 0;

        for (size_t iorder = 0; iorder < order; ++iorder) {
            if (iword + iorder >= words_length)
                break;

            wid_t word = words[iword + iorder];
            dbkey_t current = iorder == 0 ? word : make_key(key, word);

            auto e = ngrams[iorder].emplace(current, ngram_t());
            ngram_t &ngram = e.first->second;
            ngram.predecessor = key;
            ngram.counts.count += count;

            if (e.second)
                this->size++;

            key = current;
        }
    }

    delete words;
}

void NGramBatch::Reset(const vector<seqid_t> &_streams) {
    streams = _streams;
    ngrams_map.clear();
    size = 0;
}

bool NGramBatch::SetStreamIfValid(stream_t stream, seqid_t sentence) {
    if (streams.size() <= stream)
        streams.resize(stream + 1, -1);

    if (streams[stream] < sentence) {
        streams[stream] = sentence;
        return true;
    } else {
        return false;
    }
}

const vector<seqid_t> &NGramBatch::GetStreams() const {
    return streams;
}

unordered_map<domain_t, ngram_table_t> &NGramBatch::GetNGrams() {
    return ngrams_map;
}
