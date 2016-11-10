#include "TranslationTask.h"
#include "System.h"
#include "InputType.h"
#include "PhraseBased/Manager.h"
#include "SCFG/Manager.h"

using namespace std;

namespace Moses2
{

TranslationTask::TranslationTask(System &system,
		const std::string &line,
		long translationId) : m_nbestListSize(0)
{
  if (system.isPb) {
	  m_mgr.reset(new Manager(system, *this, line, translationId));
  }
  else {
	  m_mgr.reset(new SCFG::Manager(system, *this, line, translationId));
  }
  m_featureWeights = system.GetWeights();
}

TranslationTask::~TranslationTask()
{
}

void TranslationTask::Run()
{

  m_mgr->Decode();

  string out;

  out = m_mgr->OutputBest() + "\n";
  m_mgr->system.bestCollector->Write(m_mgr->GetTranslationId(), out);

  if (m_mgr->system.options.nbest.nbest_size && m_mgr->system.nbestCollector) {
    out = m_mgr->OutputNBest();
    m_mgr->system.nbestCollector->Write(m_mgr->GetTranslationId(), out);
  }

  if (!m_mgr->system.options.output.detailed_transrep_filepath.empty()) {
    out = m_mgr->OutputTransOpt();
    m_mgr->system.detailedTranslationCollector->Write(m_mgr->GetTranslationId(), out);
  }

  if(m_resultCallback)
    m_resultCallback(GetResult(m_nbestListSize));

  // note that thread-local objects (memory pools) hold Hypotheses that we release
  // when we return here and signal Task completion to the ThreadPool.
  // Hence, the TranslationTask becomes invalid afterwards.


  // avoid trouble with a late destructor call to ~Manager, ~Search, ~Stack.
  // destroy now, before we release the global memory stuff.
  m_mgr.reset();
}

void TranslationTask::SetWeights(const std::map<std::string, std::vector<float>> &featureWeights)
{
  m_featureWeights.SetWeights(m_mgr->system.featureFunctions, featureWeights);
}

TranslationResponse TranslationTask::GetResult(size_t nbestListSize) const
{
  TranslationResponse response;
  response.session = 0;
  response.text = m_mgr->OutputBest();
  if(nbestListSize) {
    if(!m_mgr->system.options.nbest.nbest_size)
      // otherwise, we fail with an empty ManagerBase::arcLists?! error looks like: "looking for:0xa39da20 have 0" terminate called after throwing an instance of 'util::Exception'
      // class Scores only adds scores if nbest_size != 0.
      throw new std::runtime_error("system.options.nbest.nbest_size must not be 0 when requesting an n-best list");
    m_mgr->OutputNBest(nbestListSize, response.hypotheses);
  }
  m_mgr->OutputAlignment(response.alignment);
  return response;
}

}

