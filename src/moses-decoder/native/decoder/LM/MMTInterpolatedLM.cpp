// $Id$

/***********************************************************************
Moses - factored phrase-based language decoder
Copyright (C) 2006 University of Edinburgh

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
***********************************************************************/

#include <limits>
#include <iostream>
#include <fstream>
#include <algorithm>

#include "MMTInterpolatedLM.h"

#include "../Phrase.h"
#include "../Scores.h"
#include "../System.h"
#include "../PhraseBased/Hypothesis.h"
#include "../PhraseBased/Manager.h"
#include "../PhraseBased/TargetPhraseImpl.h"
#include "TranslationTask.h"
#include <mmt/logging/Log.h>

#define ParseWord(w) (boost::lexical_cast<wid_t>((w)))

using namespace std;
using namespace Moses;

namespace Moses2 {

class ILMState : public FFState {
    friend class MMTInterpolatedLM;

    friend ostream &operator<<(ostream &out, const ILMState &obj);

public:
    size_t hash() const {
      return state->hash();
    }

    bool operator==(const FFState &o) const {
      const ILMState &other = static_cast<const ILMState &>(o);
      return (*state == *(other.state));
    }

    ILMState(HistoryKey *st) : state(st) {}

    virtual std::string ToString() const override {
      stringstream ss;
      ss << state;
      return ss.str();
    }

private:
    HistoryKey *state;
};

// friend
ostream &operator<<(ostream &out, const ILMState &obj) {
    out << " obj.state:|" << obj.state << "|";
    return out;
}

MMTInterpolatedLM::MMTInterpolatedLM(size_t startInd, const std::string &line) : StatefulFeatureFunction(startInd, line), m_nGramOrder(0), m_enableOOVFeature(false), m_factorType(0) {
    ReadParameters();

    Log(TRACE, GetName()
        << " MMTInterpolatedLM::MMTInterpolatedLM() m_nGramOrder:|"
        << m_nGramOrder << "|");
    Log(TRACE, GetName()
        << " MMTInterpolatedLM::MMTInterpolatedLM() m_modelPath:|"
        << m_modelPath << "|");
    Log(TRACE, GetName()
        << " MMTInterpolatedLM::MMTInterpolatedLM() m_factorType:|"
        << m_factorType << "|");
}

MMTInterpolatedLM::~MMTInterpolatedLM() {
    delete m_lm;
}

void MMTInterpolatedLM::Load(System &system) {
    if (m_nGramOrder == 0)
        m_nGramOrder = lm_options.order;
    else
        m_nGramOrder = min(m_nGramOrder, (size_t) lm_options.order);

    m_lm = new InterpolatedLM(m_modelPath, lm_options);
}

FFState *MMTInterpolatedLM::BlankState(MemPool &pool, const System &sys) const {
    mmt::ilm::Phrase phrase(1);
    phrase[0] = kVocabularyStartSymbol;
    auto *s = new(pool.Allocate<ILMState>()) ILMState(m_lm->MakeHistoryKey(phrase, (HistoryKey *) pool.Allocate(m_lm->GetHistoryKeySize())));
    assert(s->state->hash() >= 0);
    return s;
}

void MMTInterpolatedLM::EmptyHypothesisState(FFState &state, const ManagerBase &mgr,
                                             const InputType &input, const Hypothesis &hypo) const
{
    mmt::ilm::Phrase phrase(1);
    phrase[0] = kVocabularyStartSymbol;
    ILMState &ourState = static_cast<ILMState &>(state);
    // to do: if there was no "system pool", this could well just re-use existing state memory. BlankState() has been called on it before.
    //ourState.state = m_lm->MakeHistoryKey(phrase, (HistoryKey *) mgr.GetPool().Allocate(m_lm->GetHistoryKeySize()));
    ourState.state = m_lm->MakeHistoryKey(phrase, ourState.state);
    assert(ourState.state->hash() >= 0);
}

void
MMTInterpolatedLM::TransformPhrase(const Phrase<Moses2::Word> &phrase, std::vector<wid_t> &phrase_vec,
                                   const size_t startGaps,
                                   const size_t endGaps) const {
    for (size_t i = 0; i < startGaps; ++i) {
        phrase_vec.push_back(kVocabularyStartSymbol); //insert start symbol
    }
    for (size_t i = 0; i < phrase.GetSize(); ++i) {
        wid_t id = ParseWord(phrase[i][m_factorType]->GetString().as_string());
        phrase_vec.push_back(id);
    }
    for (size_t i = 0; i < endGaps; ++i) {
        phrase_vec.push_back(kVocabularyEndSymbol); //insert end symbol
    }
}

void
MMTInterpolatedLM::SetWordVector(const Hypothesis &hypo, mmt::ilm::Phrase &phrase_vec, const size_t startGaps,
                                 const size_t endGaps, const size_t from, const size_t to) const {
    for (size_t i = 0; i < startGaps; ++i) {
        phrase_vec.push_back(kVocabularyStartSymbol); //insert start symbol
    }

    for (size_t position = from; position < to; ++position) {
        phrase_vec.push_back(ParseWord(hypo.GetWord(position)[m_factorType]->GetString().as_string()));
    }

    for (size_t i = 0; i < endGaps; ++i) {
        phrase_vec.push_back(kVocabularyEndSymbol); //insert end symbol
    }
}

void
MMTInterpolatedLM::CalcScore(const Phrase<Moses2::Word> &phrase, float &fullScore, float &ngramScore,
                             size_t &oovCount, const System &system) const {
    Log(TRACE,
            "void MMTInterpolatedLM::CalcScore(const Phrase &phrase, ...) const START phrase:|" << phrase.Debug(system) << "|"
                                                                                                <<
                                                                                                std::endl);

    fullScore = 0;
    ngramScore = 0;
    oovCount = 0;

    if (phrase.GetSize() == 0) return;

    std::vector<wid_t> phrase_vec;
    TransformPhrase(phrase, phrase_vec, 0, 0);

    size_t boundary = m_nGramOrder - 1;

    context_t *context_vec = t_context_vec.get();
    if (context_vec == nullptr) {
        Log(TRACE, "void MMTInterpolatedLM::CalcScore(const Phrase &phrase, ...) const context is null");
    } else if (context_vec->empty()) {
        Log(TRACE, "void MMTInterpolatedLM::CalcScore(const Phrase &phrase, ...) const context is empty");
    } else {
        Log(TRACE,
                "void MMTInterpolatedLM::CalcScore(const Phrase &phrase, ...) const context is not empty not null, size:|"
                    <<
                    context_vec->size() << "|");
    }

    CachedLM *lm = t_cached_lm.get();

    HistoryKey *cursorHistoryKey = lm->MakeEmptyHistoryKey();
    for (size_t position = 0; position < phrase_vec.size(); ++position) {
        HistoryKey *outHistoryKey = NULL;
        double prob = lm->ComputeProbability(phrase_vec.at(position), cursorHistoryKey, context_vec, &outHistoryKey);

        delete cursorHistoryKey;
        cursorHistoryKey = outHistoryKey;

        fullScore += prob;
        if (position >= boundary) {
            ngramScore += prob;
        }
    }

    delete cursorHistoryKey;

    if (m_enableOOVFeature) {
        for (size_t position = 0; position < phrase_vec.size(); ++position) {
            // verifying whether the actual word is an OOV, and in case increase the oovCount
            if (lm->IsOOV(context_vec, phrase_vec.at(position)))
                ++oovCount;
        }
    }
}

void
MMTInterpolatedLM::EvaluateInIsolation(MemPool &pool, const System &system,
                                       const Phrase<Moses2::Word> &source,
                                       const TargetPhraseImpl &targetPhrase, Scores &scores,
                                       SCORE &estimatedScore) const
{
    Log(DEBUG, "void LanguageModel::EvaluateInIsolation(const Phrase &source, const TargetPhrase &targetPhrase, ...)");
    // contains factors used by this LM
    float fullScore, nGramScore;
    size_t oovCount;

    Log(DEBUG, "targetPhrase:|" << targetPhrase.Debug(system) << "|");
    Log(DEBUG, "pthread_self():" << pthread_self() << endl);

    CalcScore(targetPhrase, fullScore, nGramScore, oovCount, system);

    float estimateScore = fullScore - nGramScore;

    if (m_enableOOVFeature) {
        float score[2], estimateScores[2];
        score[0] = nGramScore;
        score[1] = oovCount;
        scores.PlusEquals(system, *this, score);

        estimateScores[0] = estimateScore;
        estimateScores[1] = 0;
        SCORE weightedScore = Scores::CalcWeightedScore(system, *this,
                                                        estimateScores);
        estimatedScore += weightedScore;
    } else {
        scores.PlusEquals(system, *this, nGramScore);

        SCORE weightedScore = Scores::CalcWeightedScore(system, *this,
                                                        estimateScore);
        estimatedScore += weightedScore;
    }

    Log(DEBUG, "CalcScore of targetPhrase:|" << targetPhrase.Debug(system) << "|: ngr=" << nGramScore << " est=" << estimateScore);
}

void MMTInterpolatedLM::EvaluateWhenApplied(const ManagerBase &mgr,
                                            const Hypothesis &hypo, const FFState &prevState, Scores &scores,
                                            FFState &state) const {
    Log(TRACE, "FFState* MMTInterpolatedLM::EvaluateWhenApplied(const Hypothesis &hypo, const FFState *ps, ScoreComponentCollection *out) const");

    ILMState &outState = static_cast<ILMState &>(state);

    if (hypo.GetTargetPhrase().GetSize() == 0) {
        //ILMState &inState = const_cast<ILMState &>(static_cast<const ILMState &>(prevState));
        const ILMState &inState = static_cast<const ILMState &>(prevState);
        outState.state = inState.state;
    }

    //[begin, end) in STL-like fashion.
    const int begin = (const int) hypo.GetCurrTargetWordsRange().GetStartPos();
    const int end = (const int) (hypo.GetCurrTargetWordsRange().GetEndPos() + 1);

    int adjust_end = (int) (begin + m_nGramOrder - 1);

    if (adjust_end > end) {
        adjust_end = end;
    }

    mmt::ilm::Phrase phrase_vec;
    SetWordVector(hypo, phrase_vec, 0, 0, begin, adjust_end);

    context_t *context_vec = t_context_vec.get();
    if (context_vec == nullptr) {
        Log(TRACE, "void MMTInterpolatedLM::EvaluateWhenApplied(const Phrase &phrase, ...) const context is null");
    } else if (context_vec->empty()) {
        Log(TRACE,
                "void MMTInterpolatedLM::EvaluateWhenApplied(const Phrase &phrase, ...) const context is empty");
    } else {
        Log(TRACE,
                "void MMTInterpolatedLM::EvaluateWhenApplied(const Phrase &phrase, ...) const context is not empty not null, size:|"
                    <<
                    context_vec->size() << "|");
    }

    CachedLM *lm = t_cached_lm.get();
    double score = 0.0;

    const ILMState &inState = static_cast<const ILMState &>(prevState);
    HistoryKey *initialState = inState.state;

    uint8_t buffer[lm->GetHistoryKeySize()];
    HistoryKey *cursorHistoryKey = NULL;

    HistoryKey *buf0 = outState.state;
    HistoryKey *buf1 = (HistoryKey *) buffer;

    // ensure that the last state written goes into outState.state (which is valid outside this scope)
    if(phrase_vec.size() % 2 == 0)
      swap(buf0, buf1);

    lm->MakeEmptyHistoryKey(buf1); // sentinel for dtor call

    for (size_t position = 0; position < phrase_vec.size(); ++position) {
        HistoryKey *outHistoryKey = lm->MakeEmptyHistoryKey(buf0);

        double prob = lm->ComputeProbability(phrase_vec.at(position),
                                             cursorHistoryKey ? cursorHistoryKey : initialState,
                                             context_vec, &outHistoryKey);

        swap(buf0, buf1);

        cursorHistoryKey = outHistoryKey;
        score += prob;
    }
    assert(cursorHistoryKey == outState.state || cursorHistoryKey == NULL);

    // adding probability of having sentenceEnd symbol, after this phrase;
    // this could happen only when all source words are covered
    if (hypo.GetBitmap().IsComplete()) {
        mmt::ilm::Phrase ngram_vec;
        int adjust_begin = end - ((int) m_nGramOrder - 1);
        size_t startGaps = 0;
        if (adjust_begin < 0) {
            startGaps = 1;
            adjust_begin = 0;
        }

        //if the phrase is too short, one StartSentenceSymbol (see startGaps) is added
        SetWordVector(hypo, ngram_vec, startGaps, 0, adjust_begin, end);

        uint8_t bufferTmp[lm->GetHistoryKeySize()];
        HistoryKey *tmpHistoryKey = lm->MakeHistoryKey(ngram_vec, (HistoryKey *) bufferTmp);
        score += lm->ComputeProbability(kVocabularyEndSymbol, tmpHistoryKey, t_context_vec.get(), &cursorHistoryKey);
    } else {
        // need to set the LM state
        if (adjust_end < end) { // the LMstate of this target phrase refers to the last m_lmtb_size-1 words

            mmt::ilm::Phrase ngram_vec;
            int adjust_begin = end - ((int) m_nGramOrder - 1);
            if (adjust_begin < 0) {
                adjust_begin = 0;
            }

            // because of the size of the phrase (Larger then m_ngram_order) it is not possible that
            // the relevant words for the state contain the StartSentenceSymbol
            SetWordVector(hypo, ngram_vec, 0, 0, adjust_begin, end);

            cursorHistoryKey = lm->MakeHistoryKey(ngram_vec, cursorHistoryKey);
        }
    }

    scores.PlusEquals(mgr.system, *this, score); // score is already expressed as natural log probability

    assert(outState.state == cursorHistoryKey);
    assert(outState.state->hash() >= 0);
}

void MMTInterpolatedLM::InitializeForInput(const Manager &mgr) const {
    // fetch feature weights
    FeatureFunction::InitializeForInput(mgr);

    // moses2 keeps const pointers to feature functions, so we must be 'void() const' unless you change everything else.
    const_cast<MMTInterpolatedLM *>(this)->InitializeForInput(mgr);
}

void MMTInterpolatedLM::InitializeForInput(const Manager &mgr) {
    // we assume here that translation is run in one single thread for each ttask
    // (no parallelization at a finer granularity involving MMT InterpolatedLM)

    // This function is called prior to actual translation and allows the class
    // to set up thread-specific information such as context weights

    // DO NOT modify members of 'this' here. We are being called from different
    // threads, and there is no locking here.

    /*
    SPTR<ContextScope> const &scope = ttask->GetScope();
    SPTR<weightmap_t const> weights = scope->GetContextWeights();
    */
    const weightmap_t *weights = &mgr.task.GetContextWeights();

    if (weights) {
        context_t *context_vec = new context_t;

        for (weightmap_t::const_iterator it = weights->begin(); it != weights->end(); ++it) {
            context_vec->push_back(cscore_t(ParseWord(it->first), it->second));
        }

        m_lm->NormalizeContext(context_vec);
        t_context_vec.reset(context_vec);
    }

    t_cached_lm.reset(new CachedLM(m_lm, 5));
}

void MMTInterpolatedLM::CleanUpAfterSentenceProcessing() const {
    // moses2 keeps const pointers to feature functions, so we must be 'void() const' unless you change everything else.
    const_cast<MMTInterpolatedLM *>(this)->CleanUpAfterSentenceProcessing();
}

void MMTInterpolatedLM::CleanUpAfterSentenceProcessing() {
    t_context_vec.reset();
    t_cached_lm.reset();
}

void MMTInterpolatedLM::SetParameter(const std::string &key, const std::string &value) {
    if (key == "factor") {
        m_factorType = boost::lexical_cast<FactorType>(value);
    } else if (key == "path") {
        m_modelPath = value;
        Log(TRACE, "m_modelPath:" << m_modelPath);
    } else if (key == "order") {
        m_nGramOrder = Scan<size_t>(value);
    } else if (key == "huge-page-size") {
      lm_options.huge_page_size = Scan<size_t>(value);
    } else if (key == "adaptivity-ratio") {
        lm_options.adaptivity_ratio = Scan<float>(value);
        Log(TRACE, "lm_options.adaptivity_ratio:" << lm_options.adaptivity_ratio);
    } else {
        StatefulFeatureFunction::SetParameter(key, value);
    }
}

} // namespace Moses2
