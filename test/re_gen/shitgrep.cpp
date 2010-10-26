#include <pcre.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <cerrno>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

#include <boost/function.hpp>

class Regex {

public:
  Regex(const char* pattern) {
    // Compile the pattern
    const char* error_str;
    int error_off;

    re = pcre_compile(
      pattern,
      PCRE_DOTALL | PCRE_NO_AUTO_CAPTURE,
      &error_str,
      &error_off,
      NULL
    );

    if (re == NULL) {
      std::ostringstream ss;
      ss << "regex compilation failed at offset " << error_off
         << ": " << error_str;

      throw std::runtime_error(ss.str());
    }

    // Study the pattern
    extra = pcre_study(re, 0, &error_str);
    if (error_str != NULL) {
      std::ostringstream ss;
      ss << "regex study failed: " << error_str;

      throw std::runtime_error(ss.str());
    }
  }
    
  ~Regex() {
    free(re);
    free(extra);
  }

  pcre* re;
  pcre_extra *extra;
};

unsigned int match(
  const char* pattern, unsigned int patnum,
  const char* text, size_t text_len, const char* charset,
  boost::function<void (int,int,unsigned int,const char*,const char*)> callback)
{
  const Regex re(pattern);

  //
  // Match the text against the pattern
  //
  const unsigned int ovector_size = 3000;
  const unsigned int wspace_size = 1000;

  int ovector[ovector_size];
  int wspace[wspace_size];

  unsigned int offset = 0;
  unsigned int total = 0;

  int matches;

  do {
    matches = pcre_dfa_exec(
      re.re, 
      re.extra,
      text,
      text_len,
      offset,
      PCRE_DFA_SHORTEST,
      ovector,
      ovector_size,
      wspace,
      wspace_size
    );

    if (matches == PCRE_ERROR_NOMATCH) break;

    if (matches == 0) {
      std::ostringstream ss;
      ss << "match vector too small!";
    
      throw std::runtime_error(ss.str());
    }

    if (matches < 0) {
      std::ostringstream ss;
      ss << "pcre_dfa_exec: " << matches;

      throw std::runtime_error(ss.str());
    }

    // run the callback for each match
    for (unsigned int i = 0; i < (unsigned int) matches; ++i) {
      callback(ovector[2*i], ovector[2*i+1] - ovector[2*i],
               patnum, pattern, charset);
    }
    
    total += matches;
    offset = ovector[0]+1;

  } while (matches > 0);

  return total;

/*
  const unsigned int ovector_size = 3000;
  int ovector[ovector_size];

  int matches = pcre_exec( 
    re.re,
    re.extra,
    text,
    text_len,
    0,
    0,
    ovector,
    ovector_size
  );

  if (matches == PCRE_ERROR_NOMATCH) return 0;

  if (matches < 0) {
    std::ostringstream ss;
    ss << "pcre_exec: " << matches;

    throw std::runtime_error(ss.str());
  }

  // run the callback for each match
  for (unsigned int i = 0; i < (unsigned int) matches; ++i) {
    callback(ovector[2*i], ovector[2*i+1] - ovector[2*i],
             patnum, pattern, charset);
  }

  return matches;
*/
}

void match_printer(int mpos, int mlen, unsigned int patnum,
                   const char* pattern, const char* charset) {
  std::cout << mpos << '\t'
            << mlen << '\t'
            << patnum << '\t'
            << pattern << '\t'
            << charset << '\n';  
}

const char* help_short() {
  return
    "Usage: shitgrep PATTERN_FILE FILE\n"
    "Try `shitgrep --help' for more information.";
}

const char* help_long() {
  return
    "Usage: shitgrep PATTERN_FILE FILE\n"
    "Search for each pattern from PATTERN_FILE in FILE.\n"
    "\n"
    "Patterns are given one per line in PATTERN_FILE.\n"
    "\n"
    "shitgrep is a state-of-the-art search tool which supports single-\n"
    "threaded, non-distributed searching, and offers search times which\n"
    "rival looking for matches by hand.\n";
}

int main(int argc, char** argv)
{
  using namespace std;

  //
  // Parse the arguments
  //

  if (argc == 2) {
    if (!strcmp(argv[1], "-h")) {
      // -h prints the short help
      cerr << help_short() << endl;
      return 0;
    } 
    else if (!strcmp(argv[1], "--help")) {
      // --help prints the long help
      cerr << help_long() << endl;
      return 0;
    }
  }

  if (argc < 3) {
    cerr << "too few arguments!\n"
         << help_short() << endl;
    return 1;
  }
  
  if (argc > 3) {
    cerr << "too many arguments!\n"
         << help_short() << endl;
    return 1;
  }

  //
  // Memory-map the text
  //
  int fd = open(argv[2], O_RDWR);
  if (fd == -1) {
    cerr << "open: " << strerror(errno) << endl;
    return errno;
  }

  // get the file size
  struct stat st;
  if (fstat(fd, &st) == -1) {
    cerr << "stat: " << strerror(errno) << endl;
    return errno;
  }

  const size_t text_len = st.st_size;

  // We extend the text by one byte so it can be null-terminated.
  // Note that you can't extend the file while it's mmapped, so
  // we have to do this beforehand.
  if (ftruncate(fd, text_len+1) == -1) {
    cerr << "ftruncate: " << strerror(errno) << endl;
    return errno;
  }

  void* addr = mmap(NULL, text_len+1, PROT_READ, MAP_PRIVATE, fd, 0);
  if (addr == MAP_FAILED) {
    cerr << "mmap: " << strerror(errno) << endl;
    return errno;
  }

  const char* text = static_cast<char*>(addr);

  //
  // Iterate over the pattern file
  //
  ifstream ifs(argv[1], ios::in | ios::binary);
  if (!ifs.is_open()) {
    cerr << "failed to open " << argv[1] << ": " << strerror(errno) << endl;
    return errno;
  }

  unsigned int matches = 0;
  unsigned int patnum = 0;

  string pattern;

  while (!ifs.eof()) {
    getline(ifs, pattern);
    if (pattern.empty()) continue; // skip empty lines 

    //
    // Match, shitgrep! Match!
    //
    matches += match(
      pattern.c_str(),
      patnum++,
      text,
      text_len,
      "ASCII",
      match_printer
    );
  }

  cerr << matches << " matches" << endl;

  //
  // Cleanup
  //

  // unmap the file
  if (munmap(addr, text_len+1) == -1) {
    cerr << "munmap: " << strerror(errno) << endl;
    return errno;
  }

  // chop off the null terminator we added
  if (ftruncate(fd, text_len) == -1) {
    cerr << "ftruncate: " << strerror(errno) << endl;
    return errno;
  }

  // close the file
  if (close(fd) == -1) {
    cerr << "close: " << strerror(errno) << endl;
    return errno;
  }
}
