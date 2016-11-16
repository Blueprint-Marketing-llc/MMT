//
// Created by Davide  Caroselli on 27/09/16.
//

#include <algorithm>
#include <boost/math/distributions/binomial.hpp>
#include <suffixarray/SuffixArray.h>
#include <util/hashutils.h>

#include "PhraseTable.h"
#include "UpdateManager.h"
#include "TranslationOptionBuilder.h"

using namespace mmt;
using namespace mmt::sapt;

#include "util/Timer.h"
Timer GetAllTranslationOptions_timer;
Timer MakeTranslationOptions_timer;
Timer MakeTranslationOptions2_timer;
Timer CollectorExtend_timer;
Timer GetRandomSamples_timer;
Timer FreqScore_timer;
Timer LexScore_timer;
Timer Extract_timer;
Timer CountOccurrencesSource_timer;
Timer CountOccurrencesTarget_timer;

size_t CountOccurrencesSource_calls = 0;
size_t CountOccurrencesTarget_calls = 0;


struct PhraseTable::pt_private {
    SuffixArray *index;
    UpdateManager *updates;
    Aligner *aligner;

    size_t numberOfSamples;
};

PhraseTable::PhraseTable(const string &modelPath, const Options &options, Aligner *aligner) {
    self = new pt_private();
    self->index = new SuffixArray(modelPath, options.prefix_length);
    self->updates = new UpdateManager(self->index, options.update_buffer_size, options.update_max_delay);
    self->aligner = aligner;
    self->numberOfSamples = options.samples;
}

PhraseTable::~PhraseTable() {
    delete self->updates;
    delete self->index;
    delete self;
}

void PhraseTable::Add(const updateid_t &id, const domain_t domain, const std::vector<wid_t> &source,
                      const std::vector<wid_t> &target, const alignment_t &alignment) {
    self->updates->Add(id, domain, source, target, alignment);
}

unordered_map<stream_t, seqid_t> PhraseTable::GetLatestUpdatesIdentifier() {
    const vector<seqid_t> &streams = self->index->GetStreams();

    unordered_map<stream_t, seqid_t> result;
    result.reserve(streams.size());

    for (size_t i = 0; i < streams.size(); ++i) {
        if (streams[i] >= 0)
            result[(stream_t) i] = streams[i];
    }

    return result;
}

/* Translation Options scoring */

static void GetLexicalScores(Aligner *aligner, const vector<wid_t> &phrase, const TranslationOption &option,
                             float &fwdScore, float &bwdScore) {
    vector<vector<float>> fwdWordProb(option.targetPhrase.size());
    vector<vector<float>> bwdWordProb(phrase.size());
    size_t sSize = phrase.size();
    size_t tSize = option.targetPhrase.size();

    // Computes the lexical probabilities on the best alignment only
    for (auto a = option.alignment.begin(); a != option.alignment.end(); ++a) {
        wid_t sWord = phrase[a->first];
        wid_t tWord = option.targetPhrase[a->second];
        fwdWordProb[a->second].push_back(aligner->GetForwardProbability(sWord, tWord));  // P(tWord | sWord)
        bwdWordProb[a->first].push_back(aligner->GetBackwardProbability(sWord, tWord));  // P(sWord | tWord)

    }
    fwdScore = 0.0;
    for (size_t ti = 0; ti < tSize; ++ti) {
        float tmpProb = 0.0;
        size_t tmpSize = fwdWordProb[ti].size();

        if (tmpSize > 0) {
            for (size_t i = 0; i < tmpSize; ++i) {
                tmpProb += fwdWordProb[ti][i];
            }
            tmpProb /= tmpSize;
        } else {
            tmpProb = aligner->GetTargetNullProbability(option.targetPhrase[ti]);
        }
        // should never happen that tmpProb <= 0
        fwdScore += (tmpProb <= 0.0) ? -9 : log(tmpProb);
    }

    bwdScore = 0.0;
    for (size_t si = 0; si < sSize; ++si) {
        float tmpProb = 0.0;
        size_t tmpSize = bwdWordProb[si].size();

        if (tmpSize > 0) {
            for (size_t i = 0; i < tmpSize; ++i) {
                tmpProb += bwdWordProb[si][i];
            }
            tmpProb /= tmpSize;
        } else {
            tmpProb = aligner->GetSourceNullProbability(phrase[si]);
        }

        // should never happen that tmpProb <= 0
        bwdScore += (tmpProb <= 0.0) ? -9 : log(tmpProb);
    }
}

static float lbop(float succ, float tries, float confidence) {
    if (confidence == 0)
        return succ / tries;
    else
        return (float) boost::math::binomial_distribution<>::find_lower_bound_on_p(tries, succ, confidence);
}

static void MakeTranslationOptions(SuffixArray *index, Aligner *aligner,
                                   const vector<wid_t> &phrase, const vector<sample_t> &samples,
                                   vector<TranslationOption> &output) {

    size_t validSamples = 0;
    vector<TranslationOptionBuilder> builders;
    Extract_timer.start();
    TranslationOptionBuilder::Extract(phrase, samples, builders, validSamples);
    Extract_timer.stop();

    static constexpr float confidence = 0.01;

    // Compute frequency-based and (possibly) lexical-based scores for all options
    // create the actual Translation option objects, setting the "best" alignment.
    size_t SampleSourceFrequency = validSamples;
    CountOccurrencesSource_timer.start();
    ++CountOccurrencesSource_calls;
    size_t GlobalSourceFrequency = index->CountOccurrences(true, phrase);
    CountOccurrencesSource_timer.stop();

    for (auto entry = builders.begin(); entry != builders.end(); ++entry) {
        /*cerr << "MakeTranslationOptions(...) source phrase:";
        for (size_t i=0;i<phrase.size();++i) { cerr << phrase[i] << " ";} ;
        cerr << " target phrase:";
        for (size_t i=0;i<entry->GetPhrase().size();++i) { cerr << entry->GetPhrase()[i] << " ";};
        cerr << endl;
*/
        CountOccurrencesTarget_timer.start();
        ++CountOccurrencesTarget_calls;
        size_t GlobalTargetFrequency = index->CountOccurrences(false, entry->GetPhrase());
        CountOccurrencesTarget_timer.stop();

        FreqScore_timer.start();
        float fwdScore = log(lbop(entry->GetCount(),
                                  std::max(entry->GetCount(), SampleSourceFrequency),
                                  confidence));
        float bwdScore = log(lbop(entry->GetCount(),
                                  std::max(entry->GetCount(), (size_t) round((float) SampleSourceFrequency * GlobalTargetFrequency / GlobalSourceFrequency)),
                                  confidence));
        FreqScore_timer.stop();

        float fwdLexScore = 0.f;
        float bwdLexScore = 0.f;

        TranslationOption option;
        option.alignment = entry->GetBestAlignment();
        option.targetPhrase = entry->GetPhrase();
        option.orientations = entry->GetOrientations();

        LexScore_timer.start();
        if (aligner)
            GetLexicalScores(aligner, phrase, option, fwdLexScore, bwdLexScore);
        LexScore_timer.stop();

        option.scores[ForwardProbabilityScore] = fwdScore;
        option.scores[BackwardProbabilityScore] = min(0.f, bwdScore);
        option.scores[ForwardLexicalScore] = fwdLexScore;
        option.scores[BackwardLexicalScore] = bwdLexScore;

        output.push_back(option);
    }
}

/* SAPT methods */

vector<TranslationOption> PhraseTable::GetTranslationOptions(const vector<wid_t> &phrase, context_t *context) {
    vector<sample_t> samples;
    GetRandomSamples_timer.start();
    self->index->GetRandomSamples(phrase, self->numberOfSamples, samples, context);
    GetRandomSamples_timer.stop();

    vector<TranslationOption> result;
    MakeTranslationOptions2_timer.start();
    MakeTranslationOptions(self->index, self->aligner, phrase, samples, result);
    MakeTranslationOptions2_timer.stop();

    return result;
}

translation_table_t PhraseTable::GetAllTranslationOptions(const vector<wid_t> &sentence, context_t *context) {
    GetAllTranslationOptions_timer.start();
    translation_table_t ttable;

    for (size_t start = 0; start < sentence.size(); ++start) {
        Collector *collector = self->index->NewCollector(context);

        vector<wid_t> phrase;
        vector<wid_t> phraseDelta;

        for (size_t end = start; end < sentence.size(); ++end) {
            wid_t word = sentence[end];
            phrase.push_back(word);
            phraseDelta.push_back(word);

            if (ttable.find(phrase) == ttable.end()) {
                vector<sample_t> samples;
                CollectorExtend_timer.start();
                collector->Extend(phraseDelta, self->numberOfSamples, samples);
                phraseDelta.clear();
                CollectorExtend_timer.stop();

                if (samples.empty())
                    break;

                vector<TranslationOption> options;
                MakeTranslationOptions_timer.start();
                MakeTranslationOptions(self->index, self->aligner, phrase, samples, options);
                MakeTranslationOptions_timer.stop();

                ttable[phrase] = options;
                //cerr << "PhraseTable::GetAllTranslationOptions(...) CollectorExtend_timer=" << CollectorExtend_timer << " seconds total" << endl;
                //cerr << "PhraseTable::GetAllTranslationOptions(...) MakeTranslationOptions_timer=" << MakeTranslationOptions_timer << " seconds total" << endl;
            }
        }
       delete collector;
    }
    GetAllTranslationOptions_timer.stop();
    return ttable;
}

void PhraseTable::ResetCounters() const{


    Collector *collector = self->index->NewCollector(NULL);
    collector->ResetCounter();
    delete collector;


    cerr << "PhraseTable::GetAllTranslationOptions(...) CollectorExtend timer=" << CollectorExtend_timer << " seconds" << endl;
    cerr << "PhraseTable::GetAllTranslationOptions(...) CountOccurrencesSource calls=" << CountOccurrencesSource_calls << " timer=" << CountOccurrencesSource_timer << " seconds" << endl;
    cerr << "PhraseTable::GetAllTranslationOptions(...) CountOccurrencesTarget calls=" << CountOccurrencesTarget_calls << " timer=" << CountOccurrencesTarget_timer << " seconds" << endl;


    const vector<wid_t> xxx = {};
    TranslationOptionBuilder builder(xxx);
    builder.ResetCounter();

    cerr << "PhraseTable::GetAllTranslationOptions(...) Extract timer=" << Extract_timer << " seconds" << endl;
    cerr << "PhraseTable::GetAllTranslationOptions(...) FreqScore timer=" << FreqScore_timer << " seconds" << endl;
    cerr << "PhraseTable::GetAllTranslationOptions(...) LexScore timer=" << LexScore_timer << " seconds" << endl;
    cerr << "PhraseTable::GetAllTranslationOptions(...) MakeTranslationOptions timer=" << MakeTranslationOptions_timer << " seconds" << endl;
    cerr << "PhraseTable::GetAllTranslationOptions(...) GetAllTranslationOptions timer=" << GetAllTranslationOptions_timer << " seconds" << endl;
    cerr << "PhraseTable::GetAllTranslationOptions(...) GetRandomSamples timer=" << GetRandomSamples_timer << " seconds" << endl;

    CollectorExtend_timer.reset();
    MakeTranslationOptions_timer.reset();
    GetAllTranslationOptions_timer.reset();
    GetRandomSamples_timer.reset();
    FreqScore_timer.reset();
    LexScore_timer.reset();
    Extract_timer.reset();
    CountOccurrencesSource_timer.reset();
    CountOccurrencesTarget_timer.reset();
    CountOccurrencesSource_calls=0;
    CountOccurrencesTarget_calls=0;
};
