#pragma once

#include <string>
#include <map>

#include "mmt/Translator.h"
#include "legacy/ThreadPool.h"
#include "Weights.h"

namespace Moses2
{

class System;
class ManagerBase;
class Manager;

class TranslationTask: public Task
{
public:
  typedef std::map<std::string, float> weightmap_t;

  TranslationTask(System &system, const std::string &line, long translationId);
  virtual ~TranslationTask();
  virtual void Run();

  void SetContextWeights(const weightmap_t &weights) { m_contextWeights = weights; }
  const weightmap_t &GetContextWeights() const { return m_contextWeights; }

  void SetWeights(const Weights &featureWeights) { m_featureWeights = featureWeights; }
  void SetWeights(const std::map<std::string, std::vector<float>> &featureWeights);
  const Weights &GetWeights() const { return m_featureWeights; }

  TranslationResponse GetResult(size_t nbestListSize) const;

protected:
  ManagerBase *m_mgr;
  weightmap_t m_contextWeights;
  Weights m_featureWeights;
};

}

