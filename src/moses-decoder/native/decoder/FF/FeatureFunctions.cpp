/*
 * FeatureFunctions.cpp
 *
 *  Created on: 27 Oct 2015
 *      Author: hieu
 */

#include <boost/foreach.hpp>
#include "FeatureFunctions.h"
#include "StatefulFeatureFunction.h"
#include "../System.h"
#include "../Scores.h"
#include "../MemPool.h"

#include "../TranslationModel/PhraseTable.h"
#include "../TranslationModel/UnknownWordPenalty.h"
#include "../DummySCFG/DummySCFG.h"
#include "../PhraseBased/TargetPhraseImpl.h"
#include "util/exception.hh"
#include "util/usage.hh"
#include "../legacy/Timer.h"
#include "Logger.h"

using namespace std;

namespace Moses2
{
FeatureFunctions::FeatureFunctions(System &system) :
    m_system(system), m_ffStartInd(0)
{
  //m_registry.PrintFF();
}

FeatureFunctions::~FeatureFunctions()
{
  RemoveAllInColl(m_featureFunctions);
}

void FeatureFunctions::Load()
{
  std::vector<FeatureFunction*> non_pt_ff, pt_ff;

  for(const FeatureFunction *ff : m_featureFunctions) {
    FeatureFunction *nonConstFF = const_cast<FeatureFunction *>(ff);
    if (dynamic_cast<PhraseTable *>(nonConstFF))
      pt_ff.push_back(nonConstFF);
    else
      non_pt_ff.push_back(nonConstFF);
  }

  // load pt last
  std::vector<FeatureFunction*> load_order;
  load_order.insert(load_order.begin(), non_pt_ff.begin(), non_pt_ff.end());
  load_order.insert(load_order.begin(), pt_ff.begin(), pt_ff.end());

  for(FeatureFunction *ff : load_order) {
    LOG(1, "Loading " << ff->GetName());
    Moses2::Timer timer;
    timer.start();
    size_t memBefore = util::RSS();
    ff->Load(m_system);
    size_t memAfter = util::RSS();
    LOG(1, "Finished loading " << ff->GetName() << " in " << timer.get_elapsed_time() << " s " << " using additional " << ((memAfter - memBefore) / 1024) << " kB RAM");
  }
}

void FeatureFunctions::Create()
{
  const Parameter &params = m_system.params;

  const PARAM_VEC *ffParams = params.GetParam("feature");
  UTIL_THROW_IF2(ffParams == NULL, "Must have [feature] section");

  BOOST_FOREACH(const std::string &line, *ffParams){
  //cerr << "line=" << line << endl;
  FeatureFunction *ff = Create(line);

  m_featureFunctions.push_back(ff);

  StatefulFeatureFunction *sfff = dynamic_cast<StatefulFeatureFunction*>(ff);
  if (sfff) {
    sfff->SetStatefulInd(m_statefulFeatureFunctions.size());
    m_statefulFeatureFunctions.push_back(sfff);
  }

  if (ff->HasPhraseTableInd()) {
    ff->SetPhraseTableInd(m_withPhraseTableInd.size());
    m_withPhraseTableInd.push_back(ff);
  }

  PhraseTable *pt = dynamic_cast<PhraseTable*>(ff);
  if (pt) {
    pt->SetPtInd(m_phraseTables.size());
    m_phraseTables.push_back(pt);
  }

  const UnknownWordPenalty *unkWP = dynamic_cast<const UnknownWordPenalty *>(pt);
  if (unkWP) {
	  m_unkWP = unkWP;
  }

}
}

FeatureFunction *FeatureFunctions::Create(const std::string &line)
{
  vector<string> toks = Tokenize(line);

  FeatureFunction *ff = m_registry.Construct(m_ffStartInd, toks[0], line);
  UTIL_THROW_IF2(ff == NULL, "Feature function not created");

  // name
  if (ff->GetName() == "") {
    ff->SetName(GetDefaultName(toks[0]));
  }

  m_ffStartInd += ff->GetNumScores();

  return ff;
}

std::string FeatureFunctions::GetDefaultName(const std::string &stub)
{
  size_t ind;
  boost::unordered_map<std::string, size_t>::iterator iter =
      m_defaultNames.find(stub);
  if (iter == m_defaultNames.end()) {
    m_defaultNames[stub] = 0;
    ind = 0;
  }
  else {
    ind = ++(iter->second);
  }
  return stub + SPrint(ind);
}

const FeatureFunction *FeatureFunctions::FindFeatureFunction(
    const std::string &name) const
{
  BOOST_FOREACH(const FeatureFunction *ff, m_featureFunctions){
	  if (ff->GetName() == name) {
		return ff;
	  }
	}
	return NULL;
}

const PhraseTable *FeatureFunctions::GetPhraseTableExcludeUnknownWordPenalty(size_t ptInd)
{
  // assume only 1 unk wp
  std::vector<const PhraseTable*> tmpVec(m_phraseTables);
  std::vector<const PhraseTable*>::iterator iter;
  for (iter = tmpVec.begin(); iter != tmpVec.end(); ++iter) {
    const PhraseTable *pt = *iter;
    if (pt == m_unkWP) {
      tmpVec.erase(iter);
      break;
    }
  }

  const PhraseTable *pt = tmpVec[ptInd];
  return pt;
}

void FeatureFunctions::EvaluateInIsolation(MemPool &pool, const System &system,
    const Phrase<Moses2::Word> &source, TargetPhraseImpl &targetPhrase) const
{
  SCORE estimatedScore = 0;

  BOOST_FOREACH(const FeatureFunction *ff, m_featureFunctions){
    Scores& scores = targetPhrase.GetScores();
    ff->EvaluateInIsolation(pool, system, source, targetPhrase, scores, estimatedScore);
  }

  targetPhrase.SetEstimatedScore(estimatedScore);
}

void FeatureFunctions::EvaluateInIsolation(
    MemPool &pool,
    const System &system,
    const Phrase<SCFG::Word> &source,
    SCFG::TargetPhraseImpl &targetPhrase) const
{
  UTIL_THROW2("not implemented");
}

void FeatureFunctions::EvaluateAfterTablePruning(MemPool &pool,
    const TargetPhrases &tps, const Phrase<Moses2::Word> &sourcePhrase) const
{
  BOOST_FOREACH(const FeatureFunction *ff, m_featureFunctions) {
    ff->EvaluateAfterTablePruning(pool, tps, sourcePhrase);
  }
}

void FeatureFunctions::EvaluateAfterTablePruning(MemPool &pool, const SCFG::TargetPhrases &tps,
    const Phrase<SCFG::Word> &sourcePhrase) const
{
  UTIL_THROW2("not implemented");
}

void FeatureFunctions::EvaluateWhenAppliedBatch(const Batch &batch) const
{
  BOOST_FOREACH(const StatefulFeatureFunction *ff, m_statefulFeatureFunctions) {
    ff->EvaluateWhenAppliedBatch(m_system, batch);
  }
}

void FeatureFunctions::InitializeForInput(const Manager &mgr) const
{
  BOOST_FOREACH(const FeatureFunction *ff, m_featureFunctions) {
    ff->InitializeForInput(mgr);
  }
}

void FeatureFunctions::CleanUpAfterSentenceProcessing() const
{
  BOOST_FOREACH(const FeatureFunction *ff, m_featureFunctions) {
	  ff->CleanUpAfterSentenceProcessing();
  }
}

void FeatureFunctions::ShowWeights(const Weights &allWeights)
{
  BOOST_FOREACH(const FeatureFunction *ff, m_featureFunctions) {
    cout << ff->GetName();
    if (ff->IsTuneable()) {
      cout << "=";
      vector<SCORE> weights = allWeights.GetWeights(*ff);
      for (size_t i = 0; i < weights.size(); ++i) {
        cout << " " << weights[i];
      }
      cout << endl;
    } else {
      cout << " UNTUNEABLE" << endl;
    }
  }
}

}

