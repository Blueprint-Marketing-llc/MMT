//
// Created by Davide  Caroselli on 14/01/16.
//

#include <stdio.h>
#include <iostream>
#include <thread>

#include "mmt/MosesDecoder.h"

using namespace mmt::decoder;

int run(MosesDecoder *decoder, const std::string *source, std::string *target) {
  using namespace std;

  translation_t translation;
  translation = decoder->translate(*source, 0, NULL, 0);
  *target = translation.text;

  return 0;
}

void batch_run(MosesDecoder *decoder) {
  using namespace std;
  vector<string> lines;
  string line;

  while(getline(cin, line)) {
    lines.push_back(line);
  }

  vector<string> translations;
  translations.resize(lines.size());
  thread threads[lines.size()];

  size_t i = 0;
  size_t nthreads = 8;
  for(i = 0; i < nthreads && i < lines.size(); i++) {
    threads[i] = thread(&run, decoder, &lines[i], &translations[i]);
    //threads[i].join();
    //cout << translations[i] << endl;
  }

  size_t j = 0;
  for(; j < lines.size(); j++) {
    threads[j].join();
    cout << translations[j] << endl;
    if(i < lines.size()) {
      threads[i] = thread(&run, decoder, &lines[i], &translations[i]);
      i++;
    }
  }
}

int main(int argc, const char *argv[]) {
  if (argc < 2) {
    printf("USAGE: jnitest MOSES_INI\n");
    return 1;
  }

  const char *moses_ini = argv[1];
  std::cout << "Moses INI: " << moses_ini << "\n";

  MosesDecoder *decoder = MosesDecoder::createInstance(moses_ini, nullptr, nullptr);

  // Features
  std::vector<feature_t> features = decoder->getFeatures();
  for (int i = 0; i < features.size(); i++) {
    std::cout << "Feature: " << features[i].name << " ";
    std::vector<float> weights = decoder->getFeatureWeights(features[i]);

    for (int j = 0; j < weights.size(); j++)
      std::cout << weights[j] << " ";

    std::cout << "\n";
  }
  std::cout << "\n";

  if(argc == 3) {
    batch_run(decoder);
    return 0;
  }

  // Translation
  //std::string text = "system information support";
  std::string text = "1092 10121 10010 1049 1006";
  translation_t translation;

  //Vocabulary::Lookup(const vector<vector<string>> &buffer, vector<vector<wid_t>> *output, bool putIfAbsent)
  // but we have no Vocabulary.

  std::map<std::string, float> ibm;;
  ibm["2"] = 1.f;
  std::map<std::string, float> europarl;
  europarl["1"] = 1.f;

  translation = decoder->translate(text, 0, NULL, 0);
  std::cout << "Translation: " << translation.text << "\n";
  std::cout << "Alignment: ";
  for(std::vector<std::pair<size_t, size_t> >::iterator it = translation.alignment.begin(); it != translation.alignment.end(); it++)
    std::cout << it->first << "-" << it->second << " ";
  std::cout << "\n";
  translation = decoder->translate(text, 0, &ibm, 0);
  std::cout << "Translation IBM: " << translation.text << "\n";
  translation = decoder->translate(text, 0, &europarl, 0);
  std::cout << "Translation EUR: " << translation.text << "\n";
  std::cout << "\n";

  int64_t ibmSession = decoder->openSession(ibm);
  int64_t europarlSession = decoder->openSession(europarl);

  std::cout << "Opened Translation Session IBM ID: " << ibmSession << "\n";
  std::cout << "Opened Translation Session EUR ID: " << europarlSession << "\n";

  translation = decoder->translate(text, ibmSession, NULL, 0);
  std::cout << "Translation Session IBM: " << translation.text << "\n";
  translation = decoder->translate(text, europarlSession, NULL, 0);
  std::cout << "Translation Session EUR: " << translation.text << "\n";
  std::cout << "\n";

  translation = decoder->translate(text, 0, NULL, 10);
  for (int i = 0; i < translation.hypotheses.size(); i++) {
    hypothesis_t hyp = translation.hypotheses[i];
    std::cout << "Translation HYP: " << hyp.text << " ||| " << hyp.fvals << " ||| " << hyp.score << "\n";
  }

  return 0;
}
