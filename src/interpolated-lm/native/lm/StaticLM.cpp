//
// Created by Davide  Caroselli on 07/09/16.
//

#include "StaticLM.h"
#include "Phrase.h"

using namespace mmt::ilm;

namespace mmt {
    namespace ilm {
        struct KenLMHistoryKey : public HistoryKey {
            lm::ngram::State state;

            KenLMHistoryKey(const lm::ngram::State &state) : state(state) {};

            KenLMHistoryKey(const HistoryKey &o) {
                const KenLMHistoryKey &other = static_cast<const KenLMHistoryKey &>(o);
                state = other.state;
            }

            virtual size_t hash() const override {
                return hash_value(state);
            }

            virtual bool operator==(const HistoryKey &o) const override {
                const KenLMHistoryKey &other = static_cast<const KenLMHistoryKey &>(o);
                return state == other.state;
            }

            virtual size_t length() const override {
                return state.Length();
            }
        };
    }
}

StaticLM::StaticLM(const string &modelPath) {
    const lm::ngram::Config config;
    model = new lm::ngram::Model(modelPath.c_str(), config);
}

StaticLM::~StaticLM() {
    delete model;
}

HistoryKey *StaticLM::MakeHistoryKey(const Phrase &phrase, HistoryKey *memory) const {
    lm::ngram::State state0 = model->NullContextState();
    lm::ngram::State state1;

    for (Phrase::const_iterator it = phrase.begin(); it != phrase.end(); ++it) {
        lm::WordIndex vocab;

        // convert integer wid_t code into lm::WordIndex (through a double conversion from/to std::string)
        if (*it == kVocabularyStartSymbol) {
            vocab = model->GetVocabulary().BeginSentence();
        } else {
            vocab = model->GetVocabulary().Index(std::to_string(*it));
        }
        model->Score(state0, vocab, state1);
        std::swap(state0, state1);
    }

    if(memory)
        return new (memory) KenLMHistoryKey(state0);
    else
        return new KenLMHistoryKey(state0);
}

HistoryKey *StaticLM::MakeEmptyHistoryKey(HistoryKey *memory) const {
    if(memory)
        return new (memory) KenLMHistoryKey(model->NullContextState());
    else
        return new KenLMHistoryKey(model->NullContextState());
}

size_t StaticLM::GetHistoryKeySize() const {
    return sizeof(KenLMHistoryKey);
}

bool StaticLM::IsOOV(const context_t *context, const wid_t word) const {
    lm::WordIndex vocab = model->GetVocabulary().Index(std::to_string(word));
    return (vocab == model->GetVocabulary().NotFound());
}

float StaticLM::ComputeProbability(const wid_t word, const HistoryKey *historyKey, const context_t *context,
                                   HistoryKey **outHistoryKey) const {
    // get the input state
    const KenLMHistoryKey *inKey = static_cast<const KenLMHistoryKey *> (historyKey);
    assert(inKey != NULL);

    const lm::ngram::State &in_state = inKey->state;
    const lm::base::Vocabulary &vocabulary = model->GetVocabulary();

    const lm::WordIndex wordIndex = (word == kVocabularyEndSymbol) ? vocabulary.EndSentence() : vocabulary.Index(std::to_string(word));

    lm::ngram::State state;
    float prob = model->FullScore(in_state, wordIndex, state).prob;

    if (outHistoryKey && *outHistoryKey)
        new (*outHistoryKey) KenLMHistoryKey(word == kVocabularyEndSymbol ? model->NullContextState() : state);
    else if (outHistoryKey)
        *outHistoryKey = new KenLMHistoryKey(word == kVocabularyEndSymbol ? model->NullContextState() : state);

    return prob * 2.30258509299405f; // log10 to natural log
}
