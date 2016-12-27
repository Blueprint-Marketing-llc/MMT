//
// Created by Nicola Bertoldi on 27/09/16.
//

#ifndef PHRASETABLE_SADB_H
#define PHRASETABLE_SADB_H
#pragma once

#include "Phrase.h"
#include "PhraseTable.h"
#include "sapt/PhraseTable.h"
#include <mmt/sentence.h>
#include <mmt/IncrementalModel.h>
#include "moses/TypeDef.h"
#include "moses/Util.h"
#include <mmt/logging/Logger.h>


#ifdef WITH_THREADS

#include <boost/thread.hpp>

#endif

namespace Moses2 {
    using namespace mmt;
    using namespace mmt::sapt;

    class ChartParser;

    class ChartCellCollectionBase;

    class ChartRuleLookupManager;

    class TargetPhrases;

    class LexicalReordering;

    class PhraseTableSADB : public Moses2::PhraseTable {
        friend std::ostream &operator<<(std::ostream &, const PhraseTableSADB &);

    public:
        typedef std::map<std::string, float> weightmap_t;

        PhraseTableSADB(size_t startInd, const std::string &line);

        ~PhraseTableSADB();

        void Load(System &system) override;

        //void InitializeForInput(void const &ttask); // should be override

        void SetParameter(const std::string &key, const std::string &value) override;

        virtual mmt::IncrementalModel *GetIncrementalModel() const override {
            return m_pt;
        }

        virtual void InitializeForInput(const Manager &mgr) const override;

        void InitializeForInput(const Manager &mgr);

        virtual TargetPhrases *Lookup(const Manager &mgr, MemPool &pool,
                                      InputPath &inputPath) const override;

        // scfg
        virtual void InitActiveChart(
                MemPool &pool,
                const SCFG::Manager &mgr,
                SCFG::InputPath &path) const override;

        virtual void Lookup(const Manager &mgr, InputPathsBase &inputPaths) const override;

        virtual void Lookup(
                MemPool &pool,
                const SCFG::Manager &mgr,
                size_t maxChartSpan,
                const SCFG::Stacks &stacks,
                SCFG::InputPath &path) const override;


    protected:

#ifdef WITH_THREADS
        //boost::thread_specific_ptr<void> m_ttask;
        boost::thread_specific_ptr<context_t> t_context_vec;
#else
        boost::scoped_ptr <context_t> *t_context_vec;
#endif

        // SCFG
        virtual void LookupGivenNode(
                MemPool &pool,
                const SCFG::Manager &mgr,
                const SCFG::ActiveChartEntry &prevEntry,
                const SCFG::Word &wordSought,
                const Moses2::Hypotheses *hypos,
                const Moses2::Range &subPhraseRange,
                SCFG::InputPath &outPath) const override;

    private:
        mmt::logging::Logger logger;
        mmt::sapt::PhraseTable *m_pt;
        string m_modelPath;
        mmt::sapt::Options pt_options;

        const LexicalReordering *m_lr_func; // associated lexical reordering function
        std::string m_lr_func_name; // name of associated lexical reordering function

        inline vector <wid_t> ParsePhrase(const SubPhrase<Moses2::Word> &subPhrase) const;

        TargetPhrases *
        MakeTargetPhrases(const Manager &mgr, const Phrase<Moses2::Word> &sourcePhrase,
                          const vector <mmt::sapt::TranslationOption> &options) const;
    };

} // namespace Moses

#endif //PHRASETABLE_SADB_H
