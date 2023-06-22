/*
  liblightgrep: not the worst forensics regexp engine
  Copyright (C) 2023, Aon Cyber Solutions

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <catch2/catch_test_macros.hpp>

#include "automata.h"
#include "fwd_pointers.h"
#include "nfabuilder.h"
#include "parser.h"

/*
std::string analyze(const NFA& nfa) {
  return "";
}

NFAPtr construct(const std::string& pattern) {
  Pattern pat(pattern);
  ParseTree tree;
  parse(pat, tree);
  NFABuilder builder;
  builder.build(tree);
  return builder.getFsm();
}

TEST_CASE("test_simple_factor") {
  NFAPtr nfa = construct("glushkov");
  REQUIRE(nfa);
  REQUIRE(nfa->verticesSize() == 9u);
  // std::string longest_factor = analyze(*nfa);
  // REQUIRE(longest_factor == "glushkov");
}

TEST_CASE("test_nondominant_factor") {
  NFAPtr nfa = construct("(abc|def).*ghi");
  REQUIRE(nfa);
  REQUIRE(nfa->verticesSize() == 11u);
  // std::string longest_factor = analyze(*nfa);
  // REQUIRE(longest_factor == "ghi");
}

TEST_CASE("test_one_more") {
  NFAPtr nfa = construct("a+b+c+");
  REQUIRE(nfa);
  REQUIRE(nfa->verticesSize() == 4u);
  // std::string longest_factor = analyze(*nfa);
  // REQUIRE(longest_factor == "abc");
}

TEST_CASE("test_zero_more") {
  NFAPtr nfa = construct("fooa*b*c*");
  REQUIRE(nfa);
  REQUIRE(nfa->verticesSize() == 7u);
  // std::string longest_factor = analyze(*nfa);
  // REQUIRE(longest_factor == "foobar");
}

TEST_CASE("test_zero_or_one") {
  NFAPtr nfa = construct("a?fooc?");
  REQUIRE(nfa);
  REQUIRE(nfa->verticesSize() == 6u);
  // std::string longest_factor = analyze(*nfa);
  // REQUIRE(longest_factor == "foo");
}

TEST_CASE("test_case_insensitive_factor") {
  NFAPtr nfa = construct("foo(h|H)(t|T)(t|T)(p|P)");
  REQUIRE(nfa);
  REQUIRE(nfa->verticesSize() == 12u);
  // std::string longest_factor = analyze(*nfa);
  // REQUIRE(longest_factor == "foo");
}

TEST_CASE("test_too_short") {
  NFAPtr nfa = construct("fox*bar");
  REQUIRE(nfa);
  REQUIRE(nfa->verticesSize() == 7u);
  // std::string longest_factor = analyze(*nfa);
  // REQUIRE(longest_factor == "bar");
}
*/