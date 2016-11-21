/*
 * Weights.h
 *
 *  Created on: 24 Oct 2015
 *      Author: hieu
 */
#pragma once

#include <iostream>
#include <vector>
#include <map>
#include "TypeDef.h"

namespace Moses2
{

class System;
class FeatureFunction;
class FeatureFunctions;

class Weights
{
public:
  Weights();
  virtual ~Weights();
  void Init(const FeatureFunctions &ffs);

  SCORE operator[](size_t ind) const
  {
    return m_weights[ind];
  }

  std::ostream &Debug(std::ostream &out, const System &system) const;

  std::vector<SCORE> GetWeights(const FeatureFunction &ff) const;

  void SetWeights(const FeatureFunctions &ffs, const std::string &ffName, const std::vector<float> &weights);

  void SetWeights(const FeatureFunctions &ffs, const std::map<std::string, std::vector<float>> &featureWeights);

protected:
  std::vector<SCORE> m_weights;
};

}

