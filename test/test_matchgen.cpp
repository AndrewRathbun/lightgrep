#include <scope/test.h>

#include "graph.h"
#include "matchgen.h"
#include "nfabuilder.h"
#include "parser.h"
#include "parsetree.h"
#include "pattern.h"

#include <algorithm>
#include <iostream>
#include <iterator>
#include <set>

void fixture(const char* pattern, const char** expected, uint32_t max_matches, uint32_t max_loops) {
  NFABuilder nfab;
  ParseTree tree;
  SCOPE_ASSERT(parse(Pattern(pattern, false, false), tree));
  SCOPE_ASSERT(nfab.build(tree));

  std::set<std::string> aset;
  matchgen(*nfab.getFsm(), aset, max_matches, max_loops);

  std::set<std::string> eset(expected, expected + max_matches);
  SCOPE_ASSERT(eset == aset);
}

SCOPE_TEST(aSampleMatches) {
  const char* pattern = "a";
  const char* expected[] = { "a" };
  fixture(pattern, expected, std::extent<decltype(expected)>::value, 1);
}

/*
SCOPE_TEST(aOrbSampleMatches) {
  const char* pattern = "a|b";
  const char* expected[] = { "a", "b" };
  fixture(pattern, expected, std::extent<decltype(expected)>::value, 1);
}
*/

/*
SCOPE_TEST(LPaOrbRPLPcOrdRPSampleMatches) {
  const char* pattern = "(a|b)(c|d)";
  const char* expected[] = { "ac", "ad", "bc", "bd" };
  fixture(pattern, expected, std::extent<decltype(expected)>::value, 1);
}
*/

SCOPE_TEST(aPSampleMatches1) {
  const char* pattern = "a+";
  const char* expected[] = { "a" };
  fixture(pattern, expected, std::extent<decltype(expected)>::value, 1);
}

/*
SCOPE_TEST(aPSampleMatches2) {
  const char* pattern = "a+";
  const char* expected[] = { "a", "aa" };
  fixture(pattern, expected, std::extent<decltype(expected)>::value, 2);
}

SCOPE_TEST(aPSampleMatches3) {
  const char* pattern = "a+";
  const char* expected[] = { "a", "aa", "aaa" };
  fixture(pattern, expected, std::extent<decltype(expected)>::value, 3);
}
*/
