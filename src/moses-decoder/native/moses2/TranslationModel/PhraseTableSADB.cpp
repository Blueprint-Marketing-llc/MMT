// vim:tabstop=2
#include "PhraseTableSADB.h"
//#include "moses/TranslationModel/CYKPlusParser/ChartRuleLookupManagerSkeleton.h"
//#include "moses/StaticData.h"
//#include "moses/TranslationTask.h"
#include "../PhraseBased/Manager.h"
#include "../PhraseBased/Sentence.h"
#include "../PhraseBased/TargetPhrases.h"
#include "../PhraseBased/TargetPhraseImpl.h"
#include "../MemPool.h"
#include "TranslationTask.h"

#include <boost/lexical_cast.hpp>
#include <algorithm>
#include "FF/LexicalReordering/LRModel.h"
#include "FF/LexicalReordering/LexicalReordering.h"

#define ParseWord(w) (boost::lexical_cast<wid_t>((w)))

#ifdef VERBOSE
#undef VERBOSE
#endif
#define VERBOSE(l, x) ((void) 0)

using namespace std;
using namespace Moses;
using namespace mmt::sapt;

namespace Moses2 {
    //used the constant of mmt::sapt to get entries of the vector cnt,
    //used the cnstant of LRModel   for the probabilities
    void fill_lr_vec2(LRModel::ModelType mdl, const vector<size_t> &cnt, const float total, float *v) {
        if (mdl == LRModel::Monotonic) {
            float denom = log(total + 2);
            v[LRModel::M] = log(cnt[MonotonicOrientation] + 1.) - denom;
            v[LRModel::NM] = log(total - cnt[MonotonicOrientation] + 1) - denom;
        } else if (mdl == LRModel::LeftRight) {
            float denom = log(total + 2);
            v[LRModel::R] = log(cnt[MonotonicOrientation] + cnt[DiscontinuousRightOrientation] + 1.) - denom;
            v[LRModel::L] = log(cnt[SwapOrientation] + cnt[DiscontinuousLeftOrientation] + 1.) - denom;
        } else if (mdl == LRModel::MSD) {
            float denom = log(total + 3);
            v[LRModel::M] = log(cnt[MonotonicOrientation] + 1) - denom;
            v[LRModel::S] = log(cnt[SwapOrientation] + 1) - denom;
            v[LRModel::D] = log(cnt[DiscontinuousRightOrientation] +
                                cnt[DiscontinuousLeftOrientation] + 1) - denom;
        } else if (mdl == LRModel::MSLR) {
            float denom = log(total + 4);
            v[LRModel::M] = log(cnt[MonotonicOrientation] + 1) - denom;
            v[LRModel::S] = log(cnt[SwapOrientation] + 1) - denom;
            v[LRModel::DL] = log(cnt[DiscontinuousLeftOrientation] + 1) - denom;
            v[LRModel::DR] = log(cnt[DiscontinuousRightOrientation] + 1) - denom;
        } else
            UTIL_THROW2("Reordering type not recognized!");
    }

    void fill_lr_vec(LRModel::Direction const &dir,
                     LRModel::ModelType const &mdl,
                     const vector<size_t> &dfwd,
                     const vector<size_t> &dbwd,
                     vector<float> &v) {
        // how many distinct scores do we have?
        size_t num_scores = (mdl == LRModel::MSLR ? 4 : mdl == LRModel::MSD ? 3 : 2);
        size_t offset;
        if (dir == LRModel::Bidirectional) {
            offset = num_scores;
            num_scores *= 2;
        } else offset = 0;

        v.resize(num_scores);

        // determine the denominator
        float total = 0;
        for (size_t i = 0; i <= mmt::sapt::kTranslationOptionDistortionCount; ++i) {
            total += dfwd[i];
        }
        if (dir != LRModel::Forward) { // i.e., Backward or Bidirectional
            fill_lr_vec2(mdl, dbwd, total, &v[0]);
        }
        if (dir != LRModel::Backward) { // i.e., Forward or Bidirectional
            fill_lr_vec2(mdl, dfwd, total, &v[offset]);
        }
    }

    PhraseTableSADB::PhraseTableSADB(size_t startInd, const std::string &line)
            : Moses2::PhraseTable(startInd, line), m_lr_func(nullptr) {
        this->m_numScores = mmt::sapt::kTranslationOptionScoreCount;
        ReadParameters();

        assert(m_input.size() == 1);
        assert(m_output.size() == 1);

        VERBOSE(3, GetScoreProducerDescription()
                << " PhraseTableSADB::PhraseTableSADB() m_filePath:|"
                << m_filePath << "|" << std::endl);
        VERBOSE(3, GetScoreProducerDescription()
                << " PhraseTableSADB::PhraseTableSADB() table-limit:|"
                << m_tableLimit << "|" << std::endl);
        VERBOSE(3, GetScoreProducerDescription()
                << " PhraseTableSADB::PhraseTableSADB() cache-size:|"
                << m_maxCacheSize << "|" << std::endl);
        VERBOSE(3, GetScoreProducerDescription()
                << " PhraseTableSADB::PhraseTableSADB() m_inputFactors:|"
                << m_inputFactors << "|" << std::endl);
        VERBOSE(3, GetScoreProducerDescription()
                << " PhraseTableSADB::PhraseTableSADB() m_outputFactors:|"
                << m_outputFactors << "|" << std::endl);
        VERBOSE(3, GetScoreProducerDescription()
                << " PhraseTableSADB::PhraseTableSADB() m_numScoreComponents:|"
                << m_numScoreComponents << "|" << std::endl);
        VERBOSE(3, GetScoreProducerDescription()
                << " PhraseTableSADB::PhraseTableSADB() m_input.size():|"
                << m_input.size() << "|" << std::endl);

        // caching for memory pt is pointless
        m_maxCacheSize = 0;
    }

    void PhraseTableSADB::Load(System &system) {
        //m_options = system.options;
        //SetFeaturesToApply();
        m_pt = new mmt::sapt::PhraseTable(m_modelPath, pt_options, system.aligner);

        if (m_lr_func_name.size() && m_lr_func == NULL) {
            const FeatureFunction *lr = system.featureFunctions.FindFeatureFunction(m_lr_func_name);
            m_lr_func = dynamic_cast<const LexicalReordering *>(lr);
            UTIL_THROW_IF2(lr == NULL,
                           "FF " << m_lr_func_name << " does not seem to be a lexical reordering function!");
            // todo: verify that lr_func implements a hierarchical reordering model
        }
    }

    PhraseTableSADB::~PhraseTableSADB() {
        delete m_pt;
    }

    inline vector<wid_t> PhraseTableSADB::ParsePhrase(const SubPhrase<Moses2::Word> &phrase) const {
        vector<wid_t> result(phrase.GetSize());

        for (size_t i = 0; i < phrase.GetSize(); i++) {
            result[i] = ParseWord(phrase[i].GetString(m_input));
        }

        return result;
    }

    TargetPhrases *
    PhraseTableSADB::MakeTargetPhrases(const Manager &mgr, const Phrase<Moses2::Word> &sourcePhrase,
                                       const vector<mmt::sapt::TranslationOption> &options) const
    {
        auto &pool = mgr.GetPool();
        FactorCollection &vocab = mgr.system.GetVocab();
        TargetPhrases *tps = new (pool.Allocate<TargetPhrases>()) TargetPhrases(pool, options.size());

        //transform the SAPT translation Options into Moses Target Phrases
        for (auto target_options_it = options.begin();
             target_options_it != options.end(); ++target_options_it)
        {
            const vector<wid_t> &tpw = target_options_it->targetPhrase;
            TargetPhraseImpl *tp = new (pool.Allocate<TargetPhraseImpl>()) TargetPhraseImpl(pool, *this, mgr.system, tpw.size());

            // words
            for (size_t i = 0; i < tpw.size(); i++) {
                Moses2::Word &word = (*tp)[i];
                word.CreateFromString(vocab, mgr.system, to_string(tpw[i]));
            }

            // align
            std::set<std::pair<size_t, size_t> > aln;
            for (auto alignment_it = target_options_it->alignment.begin();
                 alignment_it != target_options_it->alignment.end(); ++alignment_it)
            {
                aln.insert(std::make_pair(size_t(alignment_it->first), size_t(alignment_it->second)));
            }
            tp->SetAlignTerm(aln);

            // scores
            tp->GetScores().Assign(mgr.system, *this, target_options_it->scores);

            vector<float> scoresExtra;
            Moses2::fill_lr_vec(m_lr_func->GetModel().GetDirection(),
                                m_lr_func->GetModel().GetModelType(),
                                target_options_it->orientations.forward,
                                target_options_it->orientations.backward,
                                scoresExtra);
            tp->scoreProperties = pool.Allocate<SCORE>(scoresExtra.size());
            std::copy(scoresExtra.begin(), scoresExtra.end(), tp->scoreProperties);
            // property-index=0 must be in moses.ini property line for LR model

            mgr.system.featureFunctions.EvaluateInIsolation(pool, mgr.system, sourcePhrase, *tp);

            tps->AddTargetPhrase(*tp);
        }

        tps->SortAndPrune(m_tableLimit);
        mgr.system.featureFunctions.EvaluateAfterTablePruning(pool, *tps, sourcePhrase);

        return tps;
    }

    void PhraseTableSADB::SetParameter(const std::string &key, const std::string &value) {

        if (key == "path") {
            m_modelPath = Scan<std::string>(value);
            VERBOSE(3, "m_modelPath:" << m_modelPath << std::endl);
        } else if (key == "sample-limit") {
            pt_options.samples = Scan<int>(value);
            VERBOSE(3, "pt_options.sample:" << pt_options.samples << std::endl);
        } else {
            PhraseTable::SetParameter(key, value);
        }
    }

    void PhraseTableSADB::Lookup(const Manager &mgr, InputPathsBase &inputPaths) const
    {
        auto &sent = dynamic_cast<const Sentence &>(mgr.GetInput());
        SubPhrase<Moses2::Word> sourceSentence = sent.GetSubPhrase(0, sent.GetSize());

        context_t *context = t_context_vec.get();

        vector<wid_t> sentence = ParsePhrase(sourceSentence);
        mmt::sapt::translation_table_t ttable = m_pt->GetAllTranslationOptions(sentence, context);

        for(InputPathBase *pathBase : inputPaths) {
          InputPath *path = static_cast<InputPath*>(pathBase);
          vector<wid_t> phrase = ParsePhrase(path->subPhrase);

          auto options = ttable.find(phrase);
          if (options != ttable.end()) {
            auto tps = MakeTargetPhrases(mgr, path->subPhrase, options->second);
            path->AddTargetPhrases(*this, tps);
          }
        }
    }

    void PhraseTableSADB::InitializeForInput(const Manager &mgr) const {
        // fetch feature weights
        FeatureFunction::InitializeForInput(mgr);

        // moses2 keeps const pointers to feature functions, so we must be 'void() const' unless you change everything else.
        const_cast<PhraseTableSADB *>(this)->InitializeForInput(mgr);
    }

    void PhraseTableSADB::InitializeForInput(const Manager &mgr) {
        // we assume here that translation is run in one single thread for each ttask
        // (no parallelization at a finer granularity involving PhraseDictionarySADB)

        // This function is called prior to actual translation and allows the class
        // to set up thread-specific information such as context weights

        // DO NOT modify members of 'this' here. We are being called from different
        // threads, and there is no locking here.
        const weightmap_t *weights = &mgr.task.GetContextWeights();

        if (weights) {
            context_t *context_vec = new context_t;

            for (weightmap_t::const_iterator it = weights->begin(); it != weights->end(); ++it) {
                context_vec->push_back(cscore_t(ParseWord(it->first), it->second));
            }

            t_context_vec.reset(context_vec);
        }
    }

    TargetPhrases *PhraseTableSADB::Lookup(const Manager &mgr, MemPool &pool,
        InputPath &inputPath) const
    {
        // performance wise, this is not wise.
        // Instead, use Lookup(InputPathsBase &) for a whole sentence at a time.
        UTIL_THROW2("Not implemented");
        return nullptr;
    }

    // scfg
    void PhraseTableSADB::InitActiveChart(
        MemPool &pool,
        const SCFG::Manager &mgr,
        SCFG::InputPath &path) const
    {
        UTIL_THROW2("Not implemented");
    }

    void PhraseTableSADB::Lookup(
        MemPool &pool,
        const SCFG::Manager &mgr,
        size_t maxChartSpan,
        const SCFG::Stacks &stacks,
        SCFG::InputPath &path) const
    {
        UTIL_THROW2("Not implemented");
    }

    void PhraseTableSADB::LookupGivenNode(
        MemPool &pool,
        const SCFG::Manager &mgr,
        const SCFG::ActiveChartEntry &prevEntry,
        const SCFG::Word &wordSought,
        const Moses2::Hypotheses *hypos,
        const Moses2::Range &subPhraseRange,
        SCFG::InputPath &outPath) const
    {
        UTIL_THROW2("Not implemented");
    }



    //TO_STRING_BODY(PhraseTableSADB);

// friend
    ostream &operator<<(ostream &out, const PhraseTableSADB &phraseDict) {
        return out;
    }

}
