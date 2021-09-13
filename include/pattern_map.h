#pragma once

#include <istream>
#include <memory>
#include <vector>

#include "lightgrep/api.h"

class PatternMap {
public:
// TODO: make this private
  std::vector<LG_PatternInfo> Patterns;

  PatternMap(unsigned int sizeHint): Patterns(), Shared(false) {
    Patterns.reserve(sizeHint);
  }

  ~PatternMap() {
    clearPatterns();
  }

  PatternMap(const PatternMap& other);

  PatternMap& operator=(const PatternMap& other);

  void addPattern(const char* pattern, const char* chain, uint64_t index);

  size_t bufSize() const;
  std::vector<char> marshall() const;

  static std::unique_ptr<PatternMap> unmarshall(const void* buf, size_t len);

  bool operator==(const PatternMap& rhs) const;

private:
  void usePattern(const char* pattern, const char* chain, uint64_t index);

  void copyOther(const PatternMap& other);

  void clearPatterns();

  bool Shared;
};

bool operator==(const LG_PatternInfo& lhs, const LG_PatternInfo& rhs);

std::ostream& operator<<(std::ostream& out, const PatternMap& p);

std::ostream& operator<<(std::ostream& out, const LG_PatternInfo& pi);
