//
// Created by Nicola Bertoldi on 05/11/16.
//

#include <util/hashutils.h>
#include <cassert>
#include "TranslationOptionBuilder.h"

using namespace mmt;
using namespace mmt::sapt;


#include "util/Timer.h"
Timer Extract2_timer;
Timer ExtractOptions_timer;
Timer GetOrientation_timer;
Timer allAlignment_timer;
Timer shiftedAlignment_timer;
Timer other1_timer;
Timer other2_timer;
Timer other3_timer;
Timer other4_timer;
Timer other5_timer;
Timer other6_timer;
Timer other7_timer;
Timer other8_timer;
Timer other9_timer;

size_t other6_calls = 0;
size_t other7_calls = 0;
size_t other8_calls = 0;
size_t other9_calls = 0;

// returns lowerBound <= var <= upperBound
#define InRange(lowerBound, var, upperBound) (lowerBound <= var && var <= upperBound)

/* TranslationOption Orientation methods */

/**
 * Check if min and max in the alignment vector v are within the
 * bounds LFT and RGT and update the actual bounds L and R; update
 * the total count of alignment links in the underlying phrase pair.
 *
 * @param v alignment row/column
 * @param LFT hard left limit
 * @param RGT hard right limit
 * @param L current left bound
 * @param R current right bound
 * @param count
 * @return
 */
static bool CheckBounds(const vector<uint16_t> &v, size_t LFT, size_t RGT, uint16_t &L, uint16_t &R, size_t &count) {
    if (v.size() == 0) return 0;
    if (L > v.front() && (L = v.front()) < LFT) return false;
    if (R < v.back() && (R = v.back()) > RGT) return false;
    count += v.size();
    return true;
}

/**
 * Return the number of alignment points in box, or -1 if failure
 *
 * @param row2col
 * @param col2row
 * @param row seed coordinates
 * @param col seed coordinates
 * @param TOP box top limit
 * @param LFT box left limit
 * @param BOT box bottom limit
 * @param RGT box right limit
 * @param top top output
 * @param lft left output
 * @param bot bottom output
 * @param rgt right output
 * @return the number of alignment points in box, or -1 if failure
 */
static int ExpandBlock(const vector<vector<uint16_t>> &row2col, const vector<vector<uint16_t>> &col2row,
                       size_t row, size_t col,
                       const size_t TOP, const size_t LFT, const size_t BOT, const size_t RGT,
                       uint16_t *top = NULL, uint16_t *lft = NULL, uint16_t *bot = NULL, uint16_t *rgt = NULL) {
    if (row < TOP || row > BOT || col < LFT || col > RGT) return -1;

    assert(row < row2col.size());
    assert(col < col2row.size());

    // ====================================================
    // tables grow downwards, so TOP is smaller than BOT!
    // ====================================================

    uint16_t T, L, B, R; // box dimensions

    // if we start on an empty cell, search for the first alignment point
    if (row2col[row].size() == 0 && col2row[col].size() == 0) {
        if (row == TOP) while (row < BOT && !row2col[++row].size());
        else if (row == BOT) while (row > TOP && !row2col[--row].size());

        if (col == LFT) while (col < RGT && !col2row[++col].size());
        else if (col == RGT) while (col > RGT && !col2row[--col].size());

        if (row2col[row].size() == 0 && col2row[col].size() == 0)
            return 0;
    }
    if (row2col[row].size() == 0)
        row = col2row[col].front();
    if (col2row[col].size() == 0)
        col = row2col[row].front();

    if ((T = col2row[col].front()) < TOP) return -1;
    if ((B = col2row[col].back()) > BOT) return -1;
    if ((L = row2col[row].front()) < LFT) return -1;
    if ((R = row2col[row].back()) > RGT) return -1;

    if (B == T && R == L) return 1;

    // start/end of row / column coverage:
    uint16_t rs = (uint16_t) row, re = (uint16_t) row, cs = (uint16_t) col, ce = (uint16_t) col;
    int ret = (int) row2col[row].size();
    for (size_t tmp = 1; tmp; ret += tmp) {
        tmp = 0;
        while (rs > T) if (!CheckBounds(row2col[--rs], LFT, RGT, L, R, tmp)) return -1;
        while (re < B) if (!CheckBounds(row2col[++re], LFT, RGT, L, R, tmp)) return -1;
        while (cs > L) if (!CheckBounds(col2row[--cs], TOP, BOT, T, B, tmp)) return -1;
        while (ce < R) if (!CheckBounds(col2row[++ce], TOP, BOT, T, B, tmp)) return -1;
    }

    if (top) *top = T;
    if (bot) *bot = B;
    if (lft) *lft = L;
    if (rgt) *rgt = R;

    return ret;
}

static Orientation GetForwardOrientation(vector<vector<uint16_t>> &a1, vector<vector<uint16_t>> &a2,
                                         size_t s1, size_t e1, size_t s2, size_t e2) {
    if (e2 == a2.size()) // end of target sentence
        return MonotonicOrientation;

    size_t y = e2, L = e2, R = a2.size() - 1; // won't change
    size_t x = e1, T = e1, B = a1.size() - 1;
    if (e1 < a1.size() && ExpandBlock(a1, a2, x, y, T, L, B, R) >= 0)
        return MonotonicOrientation;

    B = x = s1 - 1;
    T = 0;
    if (s1 && ExpandBlock(a1, a2, x, y, T, L, B, R) >= 0)
        return SwapOrientation;

    while (e2 < a2.size() && a2[e2].size() == 0) ++e2;

    if (e2 == a2.size()) // should never happen, actually
        return NoOrientation;
    else if (a2[e2].back() < s1)
        return DiscontinuousLeftOrientation;
    else if (a2[e2].front() >= e1)
        return DiscontinuousRightOrientation;
    else
        return NoOrientation;
}

static Orientation GetBackwardOrientation(vector<vector<uint16_t>> &a1, vector<vector<uint16_t>> &a2,
                                          size_t s1, size_t e1, size_t s2, size_t e2) {
    if (s1 == 0 && s2 == 0)
        return MonotonicOrientation;
    else if (s2 == 0)
        return DiscontinuousRightOrientation;
    else if (s1 == 0)
        return DiscontinuousLeftOrientation;

    size_t y = s2 - 1, L = 0, R = s2 - 1; // won't change
    size_t x = s1 - 1, T = 0, B = s1 - 1;
    if (ExpandBlock(a1, a2, x, y, T, L, B, R) >= 0)
        return MonotonicOrientation;

    T = x = e1;
    B = a1.size() - 1;
    if (ExpandBlock(a1, a2, x, y, T, L, B, R) >= 0)
        return SwapOrientation;

    while (s2-- && a2[s2].size() == 0);

    return (a2[s2].size() == 0 ? NoOrientation :
            a2[s2].back() < s1 ? DiscontinuousRightOrientation :
            a2[s2].front() >= e1 ? DiscontinuousLeftOrientation :
            NoOrientation);
}

/* Alignments functions */

static inline int CompareAlignments(const mmt::alignment_t &a, const mmt::alignment_t &b) {
    // Defines an order between two alignments, based on the number of alignment points:
    //  - the alignment with fewer points is lower than that with more points
    //  - in case of equality, the order is defined by the first different source or target position
    //    of the alignment points, scanned sequentially
    if (a.size() != b.size())
        return (int) (a.size() - b.size());

    for (size_t i = 0; i < a.size(); ++i) {
        if (a[i].first != b[i].first) return a[i].first - b[i].first;
        if (a[i].second != b[i].second) return a[i].second - b[i].second;
    }

    return 0;
}

/* TranslationOptionBuilder methods */

void TranslationOptionBuilder::ExtractOptions(const vector<wid_t> &sourceSentence, const vector<wid_t> &targetSentence,
                                              const alignment_t &allAlignment, const alignment_t &inBoundAlignment,
                                              const vector<bool> &targetAligned,
                                              int sourceStart, int sourceEnd, int targetStart, int targetEnd,
                                              optionsmap_t &map, bool &isValidOption) {

    other8_timer.start();
    ++other8_calls;
    //cerr << "TranslationOptionBuilder::ExtractOptions() other8_calls=" << other8_calls << " sourceStart=" << sourceStart << " sourceEnd=" << sourceEnd <<" targetStart=" << targetStart <<" targetEnd=" << targetEnd << endl;
    //cerr << "source:" << sourceSentence.size() << " |"; for (size_t i=0;i<sourceSentence.size();++i) { cerr << sourceSentence[i] << " ";} ;cerr << "|" << endl;
    //cerr << "target:" << targetSentence.size() << " |"; for (size_t i=0;i<targetSentence.size();++i) { cerr << targetSentence[i] << " ";} ;cerr << "|" << endl;
    //cerr << "allAlignment:" << allAlignment.size() << " |"; for (size_t i=0;i<allAlignment.size();++i) { cerr << allAlignment[i].first << "-" << allAlignment[i].second << " ";} ;cerr << "|" << endl;
    //cerr << "inBoundAlignment:" << inBoundAlignment.size() << " |"; for (size_t i=0;i<inBoundAlignment.size();++i) { cerr << inBoundAlignment[i].first << "-" << inBoundAlignment[i].second << " ";} ;cerr << "|" << endl;
    //cerr << "targetAligned:" << targetAligned.size() << " |"; for (size_t i=0;i<targetAligned.size();++i) { cerr << targetAligned[i] << " ";} ;cerr << "|" << endl;
    int ts = targetStart;
    while (true) {
        ++other7_calls;
        int te = targetEnd;
        //cerr << "TranslationOptionBuilder::ExtractOptions() STR other7_calls=" << other7_calls << " ts=" << ts << " te=" << te << endl;
        other7_timer.start();
        while (true) {
            other6_timer.start();
            ++other6_calls;
            //cerr << "TranslationOptionBuilder::ExtractOptions() other6_calls=" << other6_calls << endl;

            //other1_timer.start();
            vector<wid_t> targetPhrase(targetSentence.begin() + ts, targetSentence.begin() + te + 1);
            //other1_timer.stop();

            //shiftedAlignment_timer.start();
            // Reset the word positions within the phrase pair, regardless the sentence context
            alignment_t shiftedAlignment = inBoundAlignment;
            for (auto a = shiftedAlignment.begin(); a != shiftedAlignment.end(); ++a) {
                a->first -= sourceStart;
                a->second -= ts;
            }
            //shiftedAlignment_timer.stop();

            //other5_timer.start();
            size_t slen1 = sourceSentence.size();
            size_t slen2 = targetSentence.size();

            length_t start = (length_t) sourceStart;
            length_t stop = (length_t) (sourceEnd + 1);

            std::vector<std::vector<uint16_t>> aln1(slen1); //long as the source sentence (or target if flipped)
            std::vector<std::vector<uint16_t>> aln2(slen2); //long as the target sentence (or source if flipped)
            vector<bool> forbidden(slen2);

            length_t src, trg;
            length_t lft = (length_t) forbidden.size();
            length_t rgt = 0;
            //other5_timer.stop();

            //allAlignment_timer.start();
            for (auto align = allAlignment.begin(); align != allAlignment.end(); ++align) {
                src = align->first;
                trg = align->second;

                assert(src < slen1);
                assert(trg < slen2);

                if (src < start || src >= stop) {
                    forbidden.at(trg) = true;
                } else {
                    lft = std::min(lft, trg);
                    rgt = std::max(rgt, trg);
                }

                aln1[src].push_back(trg);
                aln2[trg].push_back(src);
            }
            //allAlignment_timer.stop();

            //other2_timer.start();
            bool computeOrientation = true;

            if (lft > rgt) {
                computeOrientation = false;
            } else {
                for (size_t i = lft; i <= rgt; ++i) {
                    if (forbidden[i])
                        computeOrientation = false;
                }
            }
            //other2_timer.stop();

            //other3_timer.start();
            size_t s1, s2 = lft;
            for (s1 = s2; s1 && !forbidden[s1 - 1]; --s1) {};
            size_t e1 = rgt + 1, e2;
            for (e2 = e1; e2 < forbidden.size() && !forbidden[e2]; ++e2) {};
            //other3_timer.stop();

            //GetOrientation_timer.start();
            Orientation fwdOrientation = NoOrientation;
            Orientation bwdOrientation = NoOrientation;
            if (computeOrientation) {
                fwdOrientation = GetForwardOrientation(aln1, aln2, start, stop, s1, e2);
                bwdOrientation = GetBackwardOrientation(aln1, aln2, start, stop, s1, e2);
            }
            //GetOrientation_timer.stop();

            //other4_timer.start();
            auto builder = map.emplace(targetPhrase, targetPhrase);
            builder.first->second.Add(shiftedAlignment);
            builder.first->second.orientations.AddToForward(fwdOrientation);
            builder.first->second.orientations.AddToBackward(bwdOrientation);
            isValidOption = true;
            //other4_timer.stop();

            te += 1;
            // if fe is in word alignment or out-of-bounds
            if (te == (int) targetSentence.size() || targetAligned[te]) {
                other6_timer.stop();
                break;
            }

            other6_timer.stop();

        }
        other7_timer.stop();


        other9_timer.start();
        ++other9_calls;
        //cerr << "TranslationOptionBuilder::ExtractOptions() MID other7_calls=" << other7_calls << " ts=" << ts << " te=" << te << endl;
        ts -= 1;
        // if fs is in word alignment or out-of-bounds
        if (ts < 0 || targetAligned[ts]) {
            other9_timer.stop();
            //other7_timer.stop();

            //cerr << "TranslationOptionBuilder::ExtractOptions() BRK other7_calls=" << other7_calls << " ts=" << ts << " te=" << te << endl;
            break;
        }
        other9_timer.stop();


        //cerr << "TranslationOptionBuilder::ExtractOptions() END other7_calls=" << other7_calls << " ts=" << ts << " te=" << te << endl;
        //other7_timer.stop();
    }
    other8_timer.stop();

}

TranslationOptionBuilder::TranslationOptionBuilder(const vector<wid_t> &phrase) : phrase(phrase), count(0) {}

const alignment_t &TranslationOptionBuilder::GetBestAlignment() const {
    assert(!alignments.empty());

    auto bestEntry = alignments.begin();

    for (auto entry = alignments.begin(); entry != alignments.end(); ++entry) {
        // the best alignment is the one with the highest frequency or, in case of equal frequency,
        // it is the "highest" alignment (i.e. the more "dense", as explained in function CompareAlignments)
        if (entry->second > bestEntry->second ||
            (entry->second == bestEntry->second && CompareAlignments(entry->first, bestEntry->first) > 0))
            bestEntry = entry;
    }

    return bestEntry->first;
}

void TranslationOptionBuilder::Add(const alignment_t &alignment) {
    alignments[alignment]++;
    count++;
}

void TranslationOptionBuilder::Extract(const vector<wid_t> &sourcePhrase, const vector<sample_t> &samples,
                                       vector<TranslationOptionBuilder> &output, size_t &validSamples) {
    optionsmap_t map;

    for (auto sample = samples.begin(); sample != samples.end(); ++sample) { //loop over sampled sentence pairs
        // Create bool vector to know whether a target word is aligned.
        vector<bool> targetAligned(sample->target.size(), false);
        for (auto alignPoint = sample->alignment.begin(); alignPoint != sample->alignment.end(); ++alignPoint)
            targetAligned[alignPoint->second] = true;

        // Loop over offset of a sampled sentence pair
        for (auto offset = sample->offsets.begin(); offset != sample->offsets.end(); ++offset) {
            Extract2_timer.start();
            TranslationOptionBuilder::Extract(sourcePhrase, *sample, *offset, targetAligned, map, validSamples);
            Extract2_timer.stop();

        }
    }

    output.reserve(map.size());
    for (auto entry = map.begin(); entry != map.end(); ++entry)
        output.push_back(entry->second);
}

void TranslationOptionBuilder::Extract(const vector<wid_t> &sourcePhrase, const sample_t &sample, int offset,
                                       vector<bool> &targetAligned, optionsmap_t &map, size_t &validSamples) {
    // Search for source and target bounds
    int sourceStart = offset;
    int sourceEnd = (int) (sourceStart + sourcePhrase.size() - 1);

    int targetStart = (int) (sample.target.size() - 1);
    int targetEnd = -1;

    for (auto alignPoint = sample.alignment.begin(); alignPoint != sample.alignment.end(); ++alignPoint) {
        if (InRange(sourceStart, alignPoint->first, sourceEnd)) {
            targetStart = min((int) alignPoint->second, targetStart);
            targetEnd = max((int) alignPoint->second, targetEnd);
        }
    }

    // Check target bounds validity
    if (targetEnd - targetStart < 0)
        return;

    if (targetEnd < 0) // 0-based indexing.
        return;

    // Collect alignment points within the block (sourceStart,targetStart)-(sourceEnd,targetEnd)
    // Check whether any alignment point exists outside the block,
    // but with either the source position within the source inBounds
    // or tha target position within the target inBounds
    // In this case do not proceed with the option extraction

    alignment_t inBoundsAlignment;
    bool isValidAlignment = true;
    for (auto alignPoint = sample.alignment.begin(); alignPoint != sample.alignment.end(); ++alignPoint) {
        bool srcInbound = InRange(sourceStart, alignPoint->first, sourceEnd);
        bool trgInbound = InRange(targetStart, alignPoint->second, targetEnd);

        if (srcInbound != trgInbound) {
            isValidAlignment = false;
            break;
        }

        if (srcInbound)
            inBoundsAlignment.push_back(*alignPoint);

    }

    if (isValidAlignment) {
        bool isValidOption = false;
        ExtractOptions_timer.start();
        // Extract the TranslationOptions
        TranslationOptionBuilder::ExtractOptions(sample.source, sample.target, sample.alignment, inBoundsAlignment,
                                                 targetAligned,
                                                 sourceStart, sourceEnd, targetStart, targetEnd, map,
                                                 isValidOption);
        ExtractOptions_timer.stop();
        if (isValidOption)
            ++validSamples;
    }
}

void TranslationOptionBuilder::ResetCounter(){
    cerr << "TranslationOptionBuilder::ResetCounter() shiftedAlignment_timer=" << shiftedAlignment_timer << " seconds total" << endl;
    cerr << "TranslationOptionBuilder::ResetCounter() allAlignment_timer=" << allAlignment_timer << " seconds total" << endl;
    cerr << "TranslationOptionBuilder::ResetCounter() GetOrientation_timer=" << GetOrientation_timer << " seconds total" << endl;
    cerr << "TranslationOptionBuilder::ResetCounter() other1_timer=" << other1_timer << " seconds total" << endl;
    cerr << "TranslationOptionBuilder::ResetCounter() other2_timer=" << other2_timer << " seconds total" << endl;
    cerr << "TranslationOptionBuilder::ResetCounter() other3_timer=" << other3_timer << " seconds total" << endl;
    cerr << "TranslationOptionBuilder::ResetCounter() other4_timer=" << other4_timer << " seconds total" << endl;
    cerr << "TranslationOptionBuilder::ResetCounter() other5_timer=" << other5_timer << " seconds total" << endl;
    cerr << "TranslationOptionBuilder::ResetCounter() other6_calls=" << other6_calls << " other6_timer=" << other6_timer << " seconds total" << endl;
    cerr << "TranslationOptionBuilder::ResetCounter() other7_calls=" << other7_calls << " other7_timer=" << other7_timer << " seconds total" << endl;
    cerr << "TranslationOptionBuilder::ResetCounter() other8_calls=" << other8_calls << " other8_timer=" << other8_timer << " seconds total" << endl;
    cerr << "TranslationOptionBuilder::ResetCounter() other9_calls=" << other9_calls << " other9_timer=" << other9_timer << " seconds total" << endl;
    cerr << "TranslationOptionBuilder::ResetCounter() ExtractOptions_timer=" << ExtractOptions_timer << " seconds total" << endl;
    cerr << "TranslationOptionBuilder::ResetCounter() Extract2_timer=" << Extract2_timer << " seconds total" << endl;

    Extract2_timer.reset();
    ExtractOptions_timer.reset();
    GetOrientation_timer.reset();
    allAlignment_timer.reset();
    shiftedAlignment_timer.reset();
    other1_timer.reset();
    other2_timer.reset();
    other3_timer.reset();
    other4_timer.reset();
    other5_timer.reset();
    other6_timer.reset();
    other7_timer.reset();
    other8_timer.reset();
    other9_timer.reset();
    other6_calls=0;
    other7_calls=0;
    other8_calls=0;
    other9_calls=0;
}
