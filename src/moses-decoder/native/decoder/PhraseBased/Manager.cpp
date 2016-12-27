/*
 * Manager.cpp
 *
 *  Created on: 23 Oct 2015
 *      Author: hieu
 */
#include <boost/foreach.hpp>
#include <boost/functional/hash.hpp>
#include <boost/unordered_set.hpp>
#include <vector>
#include <sstream>
#include "Manager.h"
#include "TargetPhraseImpl.h"
#include "InputPath.h"
#include "Sentence.h"

#include "Normal/Search.h"
#include "CubePruningMiniStack/Search.h"
#include "Batch/Search.h"

#include "../TrellisPaths.h"
#include "../System.h"
#include "../Phrase.h"
#include "../InputPathsBase.h"
#include "../TranslationModel/PhraseTable.h"
#include "../TranslationModel/UnknownWordPenalty.h"
#include "../legacy/Range.h"
#include "../legacy/Timer.h"
#include "../PhraseBased/TargetPhrases.h"

#include "../legacy/util/usage.hh"
#include <boost/thread.hpp>

using namespace std;

namespace Moses2 {
    Manager::Manager(System &sys, const TranslationTask &task,
                     const std::string &inputStr, long translationId) :
            ManagerBase(sys, task, inputStr, translationId), m_search(NULL), m_bitmaps(NULL),
            logger("decoder.Manager") {
    }

    Manager::~Manager() {
        delete m_search;
        delete m_bitmaps;
    }

    void Manager::Init() {
        // init pools etc
        InitPools();

        FactorCollection &vocab = system.GetVocab();
        m_input = Moses2::Sentence::CreateFromString(GetPool(), vocab, system, m_inputStr);

        m_bitmaps = new Bitmaps(GetPool());

        const PhraseTable &firstPt = *system.featureFunctions.m_phraseTables[0];
        m_initPhrase = new(GetPool().Allocate<TargetPhraseImpl>()) TargetPhraseImpl(
                GetPool(), firstPt, system, 0);

        const Sentence &sentence = static_cast<const Sentence &>(GetInput());
        //cerr << "sentence=" << sentence.GetSize() << " " << sentence.Debug(system) << endl;

        m_inputPaths.Init(sentence, *this);

        // xml
        const UnknownWordPenalty *unkWP = system.featureFunctions.GetUnknownWordPenalty();
        UTIL_THROW_IF2(unkWP == NULL, "There must be a UnknownWordPenalty FF");
        unkWP->ProcessXML(*this, GetPool(), sentence, m_inputPaths);

        m_bitmaps->Init(sentence.GetSize(), vector<bool>(0));

        switch (system.options.search.algo) {
            case Normal:
                m_search = new NSNormal::Search(*this);
                break;
            case NormalBatch:
                m_search = new NSBatch::Search(*this);
                break;
            case CubePruning:
            case CubePruningMiniStack:
                m_search = new NSCubePruningMiniStack::Search(*this);
                break;
            default:
                cerr << "Unknown search algorithm" << endl;
                abort();
        }

        system.featureFunctions.InitializeForInput(*this);

        Moses2::Timer timer;
        timer.start();

        // lookup with every pt
        const std::vector<const PhraseTable *> &pts = system.mappings;
        for (size_t i = 0; i < pts.size(); ++i) {
            const PhraseTable &pt = *pts[i];
            //cerr << "Looking up from " << pt.GetName() << endl;
            pt.Lookup(*this, m_inputPaths);
        }

        LogInfo(logger) << "Collecting options took " << timer.get_elapsed_time() << " seconds";

        //m_inputPaths.DeleteUnusedPaths();
        CalcFutureScore();
    }

    void Manager::Decode() {
        //cerr << "Start Decode " << this << endl;

        Init();

        Moses2::Timer timer;
        timer.start();

        m_search->Decode();

        LogInfo(logger) << "Search took " << timer.get_elapsed_time() << " seconds";
        LogInfo(logger) << "Worker " << boost::this_thread::get_id() << " mempool stats: Pool: "
                        << (GetPool().Size() / 1024) << " kB, SystemPool: " << (GetSystemPool().Size() / 1024) << " kB";
        LogInfo(logger) << "Global memory stats: RSS: " << (util::RSS() / 1024) << " kB, RSSMax: "
                        << (util::RSSMax() / 1024) << " kB";

        //cerr << "Finished Decode " << this << endl;
    }

    void Manager::CalcFutureScore() {
        const Sentence &sentence = static_cast<const Sentence &>(GetInput());
        size_t size = sentence.GetSize();
        m_estimatedScores =
                new(GetPool().Allocate<EstimatedScores>()) EstimatedScores(GetPool(),
                                                                           size);
        m_estimatedScores->InitTriangle(-numeric_limits<SCORE>::infinity());

        // walk all the translation options and record the cheapest option for each span
        BOOST_FOREACH(
        const InputPathBase *path, m_inputPaths){
            const Range &range = path->range;
            SCORE bestScore = -numeric_limits<SCORE>::infinity();

            size_t numPt = system.mappings.size();
            for (size_t i = 0; i < numPt; ++i) {
                const TargetPhrases *tps = static_cast<const InputPath *>(path)->targetPhrases[i];
                if (tps) {
                    BOOST_FOREACH(
                    const TargetPhraseImpl *tp, *tps) {
                        SCORE score = tp->GetFutureScore();
                        if (score > bestScore) {
                            bestScore = score;
                        }
                    }
                }
            }
            m_estimatedScores->SetValue(range.GetStartPos(), range.GetEndPos(), bestScore);
        }

        // now fill all the cells in the strictly upper triangle
        //   there is no way to modify the diagonal now, in the case
        //   where no translation option covers a single-word span,
        //   we leave the +inf in the matrix
        // like in chart parsing we want each cell to contain the highest score
        // of the full-span trOpt or the sum of scores of joining two smaller spans

        for (size_t colstart = 1; colstart < size; colstart++) {
            for (size_t diagshift = 0; diagshift < size - colstart; diagshift++) {
                size_t sPos = diagshift;
                size_t ePos = colstart + diagshift;
                for (size_t joinAt = sPos; joinAt < ePos; joinAt++) {
                    float joinedScore = m_estimatedScores->GetValue(sPos, joinAt)
                                        + m_estimatedScores->GetValue(joinAt + 1, ePos);
                    // uncomment to see the cell filling scheme
                    // TRACE_ERR("[" << sPos << "," << ePos << "] <-? ["
                    // 	  << sPos << "," << joinAt << "]+["
                    // 	  << joinAt+1 << "," << ePos << "] (colstart: "
                    // 	  << colstart << ", diagshift: " << diagshift << ")"
                    // 	  << endl);

                    if (joinedScore > m_estimatedScores->GetValue(sPos, ePos))
                        m_estimatedScores->SetValue(
                                sPos, ePos, joinedScore);
                }
            }
        }

        //cerr << "Square matrix:" << endl;
        //cerr << *m_estimatedScores << endl;
    }

    std::string Manager::OutputBest() const {
        stringstream out;
        Moses2::FixPrecision(out);

        const Hypothesis *bestHypo = m_search->GetBestHypo();
        if (bestHypo) {
            if (system.options.output.ReportHypoScore) {
                out << bestHypo->GetScores().GetTotalScore() << " ";
            }

            bestHypo->OutputToStream(out);
            //cerr << "BEST TRANSLATION: " << *bestHypo;
        } else {
            if (system.options.output.ReportHypoScore) {
                out << "0 ";
            }
            //cerr << "NO TRANSLATION " << m_input->GetTranslationId() << endl;
        }

        return out.str();
        //cerr << endl;
    }

    std::string Manager::OutputNBest() {
        arcLists.Sort();

        boost::unordered_set <size_t> distinctHypos;

        TrellisPaths<TrellisPath> contenders;
        m_search->AddInitialTrellisPaths(contenders);

        long transId = GetTranslationId();

        // MAIN LOOP
        stringstream out;
        //Moses2::FixPrecision(out);

        size_t maxIter = system.options.nbest.nbest_size * system.options.nbest.factor;
        size_t bestInd = 0;
        for (size_t i = 0; i < maxIter; ++i) {
            if (bestInd >= system.options.nbest.nbest_size || contenders.empty()) {
                break;
            }

            //cerr << "bestInd=" << bestInd << endl;
            TrellisPath *path = contenders.Get();

            bool ok = false;
            if (system.options.nbest.only_distinct) {
                string tgtPhrase = path->OutputTargetPhrase(system);
                //cerr << "tgtPhrase=" << tgtPhrase << endl;
                boost::hash <std::string> string_hash;
                size_t hash = string_hash(tgtPhrase);

                if (distinctHypos.insert(hash).second) {
                    ok = true;
                }
            } else {
                ok = true;
            }

            if (ok) {
                ++bestInd;
                out << transId << " ||| ";
                path->OutputToStream(out, system);
                out << "\n";
            }

            // create next paths
            path->CreateDeviantPaths(contenders, arcLists, GetPool(), system);

            delete path;
        }

        return out.str();
    }

    void Manager::OutputNBest(size_t nbest_size, std::vector <ResponseHypothesis> &nbest) const {
        nbest.clear();
        arcLists.Sort();

        boost::unordered_set <size_t> distinctHypos;

        TrellisPaths<TrellisPath> contenders;
        m_search->AddInitialTrellisPaths(contenders);

        // MAIN LOOP

        size_t maxIter = nbest_size * system.options.nbest.factor;
        size_t bestInd = 0;
        for (size_t i = 0; i < maxIter; ++i) {
            if (bestInd >= nbest_size || contenders.empty()) {
                break;
            }

            //cerr << "bestInd=" << bestInd << endl;
            TrellisPath *path = contenders.Get();

            bool ok = false;
            if (system.options.nbest.only_distinct) {
                string tgtPhrase = path->OutputTargetPhrase(system);
                //cerr << "tgtPhrase=" << tgtPhrase << endl;
                boost::hash <std::string> string_hash;
                size_t hash = string_hash(tgtPhrase);

                if (distinctHypos.insert(hash).second) {
                    ok = true;
                }
            } else {
                ok = true;
            }

            if (ok) {
                ++bestInd;

                string tgtPhrase = path->OutputTargetPhrase(system);

                stringstream fvals;
                path->GetScores().OutputBreakdown(fvals, system);

                nbest.push_back(ResponseHypothesis{tgtPhrase, path->GetScores().GetTotalScore(), fvals.str()});
            }

            // create next paths
            path->CreateDeviantPaths(contenders, arcLists, GetPool(), system);

            delete path;
        }
    }

    std::string Manager::OutputTransOpt() {
        return "";
    }

    void Manager::OutputAlignment(std::vector <std::pair<size_t, size_t>> &alignment) const {
        alignment.clear();
        m_search->GetBestHypo()->OutputAlignment(system, alignment);
    }

}

