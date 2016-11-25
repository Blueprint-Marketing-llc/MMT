/*
 * UnknownWordPenalty.cpp
 *
 *  Created on: 28 Oct 2015
 *      Author: hieu
 */
#include <boost/foreach.hpp>
#include "UnknownWordPenalty.h"
#include "../System.h"
#include "../Scores.h"
#include "../InputType.h"
#include "../PhraseBased/Manager.h"
#include "../PhraseBased/TargetPhraseImpl.h"
#include "../PhraseBased/InputPath.h"
#include "../PhraseBased/TargetPhrases.h"
#include "../PhraseBased/Sentence.h"
#include "../DummySCFG/DummySCFG.h"

using namespace std;

namespace Moses2
{

UnknownWordPenalty::UnknownWordPenalty(size_t startInd, const std::string &line) :
    PhraseTable(startInd, line)
{
  m_tuneable = false;
  ReadParameters();
}

UnknownWordPenalty::~UnknownWordPenalty()
{
  // TODO Auto-generated destructor stub
}

void UnknownWordPenalty::ProcessXML(
		const Manager &mgr,
		MemPool &pool,
		const Sentence &sentence,
		InputPaths &inputPaths) const
{
	const Vector<const InputType::XMLOption*> &xmlOptions = sentence.GetXMLOptions();
	BOOST_FOREACH(const InputType::XMLOption *xmlOption, xmlOptions) {
		TargetPhraseImpl *target = TargetPhraseImpl::CreateFromString(pool, *this, mgr.system, xmlOption->GetTranslation());

    if (xmlOption->prob) {
      Scores &scores = target->GetScores();
      scores.PlusEquals(mgr.system, *this, Moses2::TransformScore(xmlOption->prob));
    }

    InputPath *path = inputPaths.GetMatrix().GetValue(xmlOption->startPos, xmlOption->phraseSize - 1);
    const SubPhrase<Moses2::Word> &source = path->subPhrase;

    mgr.system.featureFunctions.EvaluateInIsolation(pool, mgr.system, source, *target);

    TargetPhrases *tps = new (pool.Allocate<TargetPhrases>()) TargetPhrases(pool, 1);

    tps->AddTargetPhrase(*target);
    mgr.system.featureFunctions.EvaluateAfterTablePruning(pool, *tps, source);

    path->AddTargetPhrases(*this, tps);
	}
}

void UnknownWordPenalty::Lookup(const Manager &mgr,
    InputPathsBase &inputPaths) const
{
	BOOST_FOREACH(InputPathBase *pathBase, inputPaths){
	  InputPath *path = static_cast<InputPath*>(pathBase);

	  if (SatisfyBackoff(mgr, *path)) {
		  const SubPhrase<Moses2::Word> &phrase = path->subPhrase;

		  TargetPhrases *tps = Lookup(mgr, mgr.GetPool(), *path);
		  path->AddTargetPhrases(*this, tps);
	  }
	}

}

TargetPhrases *UnknownWordPenalty::Lookup(const Manager &mgr, MemPool &pool,
    InputPath &inputPath) const
{
  const System &system = mgr.system;
  TargetPhrases *tps = NULL;

  // any other pt translate this?
  size_t numPt = mgr.system.mappings.size();
  const TargetPhrases **allTPS =
      static_cast<InputPath&>(inputPath).targetPhrases;
  for (size_t i = 0; i < numPt; ++i) {
    const TargetPhrases *otherTps = allTPS[i];

    if (otherTps && otherTps->GetSize()) {
      return tps;
    }
  }

  const SubPhrase<Moses2::Word> &source = inputPath.subPhrase;
  const Moses2::Word &sourceWord = source[0];
  const Factor *factor = sourceWord[0];

  tps = new (pool.Allocate<TargetPhrases>()) TargetPhrases(pool, 1);

  TargetPhraseImpl *target =
      new (pool.Allocate<TargetPhraseImpl>()) TargetPhraseImpl(pool, *this,
          system, 1);
  Moses2::Word &word = (*target)[0];

  //FactorCollection &fc = system.vocab;
  //const Factor *factor = fc.AddFactor("SSS", false);
  word[0] = factor;

  Scores &scores = target->GetScores();
  scores.PlusEquals(mgr.system, *this, -100);

  MemPool &memPool = mgr.GetPool();
  system.featureFunctions.EvaluateInIsolation(memPool, system, source, *target);

  tps->AddTargetPhrase(*target);
  system.featureFunctions.EvaluateAfterTablePruning(memPool, *tps, source);

  return tps;
}

void UnknownWordPenalty::EvaluateInIsolation(const System &system,
    const Phrase<Moses2::Word> &source, const TargetPhraseImpl &targetPhrase, Scores &scores,
    SCORE &estimatedScore) const
{

}

// SCFG ///////////////////////////////////////////////////////////////////////////////////////////
void UnknownWordPenalty::InitActiveChart(
    MemPool &pool,
    const SCFG::Manager &mgr,
    SCFG::InputPath &path) const
{
}

void UnknownWordPenalty::Lookup(MemPool &pool,
    const SCFG::Manager &mgr,
    size_t maxChartSpan,
    const SCFG::Stacks &stacks,
    SCFG::InputPath &path) const
{
  UTIL_THROW2("not implemented");
}

void UnknownWordPenalty::LookupUnary(MemPool &pool,
    const SCFG::Manager &mgr,
    const SCFG::Stacks &stacks,
    SCFG::InputPath &path) const
{
  UTIL_THROW2("not implemented");
}

void UnknownWordPenalty::LookupNT(
    MemPool &pool,
    const SCFG::Manager &mgr,
    const Moses2::Range &subPhraseRange,
    const SCFG::InputPath &prevPath,
    const SCFG::Stacks &stacks,
    SCFG::InputPath &outPath) const
{
  UTIL_THROW2("Not implemented");
}

void UnknownWordPenalty::LookupGivenWord(
    MemPool &pool,
    const SCFG::Manager &mgr,
    const SCFG::InputPath &prevPath,
    const SCFG::Word &wordSought,
    const Moses2::Hypotheses *hypos,
    const Moses2::Range &subPhraseRange,
    SCFG::InputPath &outPath) const
{
  UTIL_THROW2("Not implemented");
}

void UnknownWordPenalty::LookupGivenNode(
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

}

