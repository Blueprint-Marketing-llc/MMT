//
// Created by david on 24.10.16.
//

#ifndef MOSES2_TRANSLATOR_H
#define MOSES2_TRANSLATOR_H

#include <string>
#include <cstdint>
#include <utility>
#include <vector>
#include <map>

namespace Moses2 {

/**
 * Hypothesis used in n-best list response.
 */
struct ResponseHypothesis {
  std::string text; //< result target sentence
  float score;
  std::string fvals;
};

/**
 * Decoder response to a translation request of a sentence.
 */
struct TranslationResponse {
  std::string text; //< result target sentence
  int64_t session; //< resulting session ID
  std::vector <ResponseHypothesis> hypotheses; //< n-best list, only filled if requested
  std::vector<std::pair<size_t, size_t>> alignment; //< word alignment using test-trg pairs, e.g. [(0,1), (1,0), ...]
};

/**
 * Translation request of a sentence.
 */
struct TranslationRequest {
  std::string sourceSent;
  size_t nBestListSize; //< set to 0 if no n-best list requested
  uint64_t sessionId; //< 0 means none, 1 means new
  std::map<std::string, float> contextWeights; //< maps from subcorpus name to weight
};

} // namespace Moses2

#endif //MOSES2_TRANSLATOR_H
