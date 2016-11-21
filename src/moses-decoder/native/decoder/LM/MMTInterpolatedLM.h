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

#ifndef moses_MMTInterpolatedLM_h
#define moses_MMTInterpolatedLM_h

#include <string>
#include <vector>

#include <ilm/InterpolatedLM.h>
#include <ilm/CachedLM.h>
#include <mmt/IncrementalModel.h>

#include "../FF/StatefulFeatureFunction.h"
#include "../legacy/Factor.h"
//#include "moses/Hypothesis.h"
//#include "moses/TypeDef.h"
#include "moses/Util.h"

#ifdef WITH_THREADS

#include <boost/thread.hpp>

#endif

using namespace mmt;
using namespace mmt::ilm;

namespace Moses2 {

    class MMTInterpolatedLM : public StatefulFeatureFunction {
    public:
        typedef std::map<std::string, float> weightmap_t;

    protected:
        InterpolatedLM *m_lm;
        string m_modelPath;
        mmt::ilm::Options lm_options;
        FactorType m_factorType;
        size_t			m_nGramOrder; //! max n-gram length contained in this LM
        bool m_enableOOVFeature;

#ifdef WITH_THREADS
        boost::thread_specific_ptr<context_t> t_context_vec;
        boost::thread_specific_ptr<CachedLM> t_cached_lm;
#else
        boost::scoped_ptr<context_t> *t_context_vec;
#endif

    public:
        MMTInterpolatedLM(size_t startInd, const std::string &line);

        ~MMTInterpolatedLM();

        void SetParameter(const std::string &key, const std::string &value) override;

        virtual void Load(System &system) override;

        virtual FFState* BlankState(MemPool &pool, const System &sys) const override;

        //! return the state associated with the empty hypothesis for a given sentence
        virtual void EmptyHypothesisState(FFState &state, const ManagerBase &mgr,
                                          const InputType &input, const Hypothesis &hypo) const override;

        void CalcScore(const Phrase<Moses2::Word> &phrase, float &fullScore, float &ngramScore, size_t &oovCount) const;

        virtual void
        EvaluateInIsolation(MemPool &pool, const System &system, const Phrase<Moses2::Word> &source,
                            const TargetPhraseImpl &targetPhrase, Scores &scores,
                            SCORE &estimatedScore) const override;

        virtual void EvaluateWhenApplied(const ManagerBase &mgr,
                                         const Hypothesis &hypo, const FFState &prevState, Scores &scores,
                                         FFState &state) const override;

        virtual void
        EvaluateInIsolation(MemPool &pool, const System &system, const Phrase<SCFG::Word> &source,
                            const TargetPhrase<SCFG::Word> &targetPhrase, Scores &scores,
                            SCORE &estimatedScore) const override
        {
          UTIL_THROW2("not implemented");
        }

        virtual void EvaluateWhenApplied(const SCFG::Manager &mgr,
                                         const SCFG::Hypothesis &hypo, int featureID, Scores &scores,
                                         FFState &state) const override
        {
          UTIL_THROW2("not implemented");
        }

        virtual void InitializeForInput(const Manager &mgr) const override;
        void InitializeForInput(const Manager &mgr);

        virtual void CleanUpAfterSentenceProcessing() const override;
        void CleanUpAfterSentenceProcessing();

        virtual mmt::IncrementalModel *GetIncrementalModel() const override {
            return m_lm;
        }

    private:
        void TransformPhrase(const Phrase<Moses2::Word> &phrase, std::vector<wid_t> &phrase_vec, const size_t startGaps,
                             const size_t endGaps) const;

        void SetWordVector(const Hypothesis &hypo, mmt::ilm::Phrase &phrase_vec, const size_t startGaps,
                           const size_t endGaps, const size_t from, const size_t to) const;
    };

}

#endif
