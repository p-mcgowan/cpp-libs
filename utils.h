#include <ctime>     // date and time
#include <fstream>
#include <iomanip>   // right justify (setw, std::right)
#include <iostream>
#include <map>
#include <sstream>
#include <stdlib.h>  // atoi
#include <string>
#include <unistd.h>  // isatty
#include <vector>
#include <iterator>  // istream_iterator

inline bool fExists(std::string f);
std::string getDate(const char *format);
std::string getOpts(const char *valids,
  std::vector<std::string> tokens,
  std::map<std::string, std::string> *opts,
  std::vector<std::string> *params);
std::string getPath(std::string pref, std::string post);
std::string toCurrency(int cents);
std::vector<std::string> tokenize(int argc, char **argv);

std::string parseArgs(std::vector<std::string> token,
  std::map<std::string, std::string> *opts, std::vector<int> *params);


/* Parses command line options and parameters and assigns them to variables,
 * returning null if all options are successful, an error string otherwise.
 * An option not expecting arguments is assigned "true".
 *
 * Input:
 *   cost char *valids:
 *     The format string for options. Determines which options
 *     have required and/or optional arguments.
 *       Format: KEY[,ALTKEY...][:REQUIRED | :REQUIRED:OPTIONAL]...
 *   std::vector<std::string> tokens:
 *     The tokenized program input.
 *   std::map<std::string, std::string> *opts:
 *     A map to populate with option keys => arguments.
 *   std::vector<std::string> *params
 *     A vector to populate with non-option arguments.
 *
 * Output:
 *   std::string:
 *     If succesful, a null string is returned. Otherwise, returns a string
 *     with the applicable error message.
 *
 * Eg:
 *   getOpts("-e,--example:0:1 -t -x:1", tokens, &opts, &params)
 *
 */
typedef struct paramStruct {
  int required;
  int optional;
  std::string key;
} vArgs;

std::string getOpts(const char *valids,
  std::vector<std::string> tokens,
  std::map<std::string, std::string> *opts,
  std::vector<std::string> *params) {

  std::stringstream ss(valids);
  std::vector<std::string> v{std::istream_iterator<std::string>{ss},
  std::istream_iterator<std::string>{}};
  std::map<std::string, vArgs*> validOpts;

  vArgs *va;
  for (std::vector<std::string>::iterator it = v.begin(); it < v.end(); it++) {
    // Iterate over the format string(s) and initialize argument variables
    int mode = 0;
    va = new vArgs{};
    va->required = 0;
    va->optional = 0;
    va->key = std::string("");
    std::string hash;

    // Parse each space-separated format string, assigning the first occurence of
    // an argument key to be the hash for the option
    for (int i = 0; i < (*it).length(); i++) {
      switch ((*it)[i]) {
        case ',':
          // Each comma separates a key
          validOpts[hash] = va;
          if (va->key == "") {
            va->key = hash;
          }
          hash = "";
          break;
        case ':':
          // Each colon separates keys from required parameters
          switch (mode) {
            case 0:
              if ((va->key) == "") {
                va->key = hash;
              }
              validOpts[hash] = va;
              break;
            case 1:
              va->required = atoi(hash.c_str());
              break;
            default:
              va->optional = atoi(hash.c_str());
              break;
          }
          hash = "";
          mode++;
          break;
        default:
          // If neither a comma or colon appears, build the string character
          // by character
          hash += (*it)[i];
          if (i + 1 == (*it).length()) {
            switch (mode) {
              case 0:
                validOpts[hash] = va;
                if ((va->key) == "") {
                  va->key = hash;
                }
                break;
              case 1:
                va->required = atoi(hash.c_str());
                break;
              default:
                va->optional = atoi(hash.c_str());
                break;
            }
          }
          break;
      }
    }
  }
    
  bool endOfArgs = false;
  // Parse the program arguments
  for (std::vector<std::string>::iterator it = tokens.begin();
    it < tokens.end(); it++) {
    if (validOpts[*it] == NULL) {
      // If not a valid option, check if it is invalid, the end-of-arguments flag,
      // or a parameter
      if ((*it)[0] == '-' && *it != "--" && !endOfArgs) {
        return "invalid option -- \"" + *it +"\"";
      } else {
        if (*it == "--") {
          endOfArgs = true;
        } else {
          params->insert(params->end(), (*it));
        }
      }
    } else {
      // If it is a valid option, check if it requires (and is provided) arguments
      vArgs *va = validOpts[*it];
      (*opts)[va->key] = "";
      int req = va->required;
      int opt = va->optional;
      if (opt == 0 && req == 0) {
        (*opts)[va->key] = "true";
      }
      while (req > 0) {
        if (it + 1 == tokens.end()) {
          return *it + " requires an argument";
        }
        (*opts)[va->key] += *(++it) + " ";
        req--;
      }
      while (opt > 0 && validOpts[*it] == NULL && *(it + 1) != "--") {
        if (it + 1 != tokens.end() && validOpts[*(it + 1)] == NULL) {
          (*opts)[va->key] += *(++it) + " ";
          opt--;
        } else {
          break;
        }
      }
    }
  }

  // Cleanup
  std::map<vArgs*, int> vm;
  for (std::map<std::string, vArgs*>::iterator clean = validOpts.begin();
    clean != validOpts.end(); clean++) {
    if (!vm[clean->second]) {
      vm[clean->second] = 1;
      free(clean->second);
    }
  }
  validOpts.clear();

  // Everything succeeded
  return "";
}


/* Takes arguments from stdin (and pipe / redirect) and puts them into
 * a string vector for simpler and more robust parsing
 *
 * Input:
 *   char **argv: The program arguments
 *   int argc: The number of program arguments
 *
 * Output:
 *   std::vector<std::string> t: The vector of arguments as strings
 *
 * Eg:
 *   tokenize(["argc1", "10"], 2) => <"argc1", "10">
 *
 */
std::vector<std::string> tokenize(int argc, char **argv) {
  // Pull args from argv
  std::vector<std::string> t(argv+1, argv+argc);
  if (!isatty(fileno(stdin))) {
    // Pull (additional) args from stdin / pipe
    std::string s;
    std::vector<std::string>::iterator it = t.begin();
    while (std::cin >> s) {
      t.push_back(s);
    }
  }
  return t;
}


/* Checks if a file exists
 *
 * Input:
 *   std::string f: The filename to test for
 *
 * Output:
 *   true if the file exists, false otherwise
 *
 */
inline bool fExists(std::string f) {
  return (access(f.c_str(), F_OK) != -1);
}


/* Provides a filename to write to. If no preferred name is specified,
 * a timestamp is provided
 *
 * Input:
 *   (optional) std::string pref:
 *     The preferred filename to write to
 *   (optional) std::string post:
 *     A string to append to the path (when using timestamp)
 *
 * Output:
 *   If pref is null, output is a timestamp, otherwise tries to write
 *   to pref. If the file exists, prompts the user to overwrite. If post
 *   is specified, appends it to the resulting path.
 *
 * Eg:
 *   getPath() => "yymmdd.hhmm" or "yymmdd.hhmmss"
 *   getPath("test.txt") => "test.txt" or ""
 *
 */
std::string getPath(std::string pref = "", std::string post = "") {
  std::string path;
  if (pref != "") {
    path = pref + post;
    if (fExists(path)) {
      std::cout << "File " << path << " exists, overwrite [y/N]? ";
      std::string input;
      std::cin >> input;
      if (!input.compare("y") || !input.compare("Y")) {
        return path;
      } else {
        return "";
      }
    }
  } else {
    path = getDate("ymd.HM") + post;
    if (fExists(path)) {
      path = getDate("ymd.HMS") + post;
    }
  }
  return path;
}


/* Returns the current date in the specified format
 *
 * Input:
 *   const char *format:
 *     The desired date format. Can be any number of [dmyYHMS.]. Other 
 *     characters are treated as literals
 *
 * Output:
 *   std::string:
 *     The desired format
 *
 * Eg:
 *   getDate("d/m/y-H:M:S") => "03/04/15-01:23:45"
 *
 */
std::string getDate(const char *format) {
  std::stringstream ss;

  time_t now = time(0);
  tm *t = localtime(&now);
  std::string s(format);

  for (int i = 0; i < s.length(); i++) {
    switch ((int)format[i]) {
      case 'd':
        ss << std::setfill('0') << std::setw(2) << t->tm_mday;
      break;
      case 'm':
        ss << std::setfill('0') << std::setw(2) << t->tm_mon + 1;
      break;
      case 'y':
        ss << t->tm_year - 100;
      break;
      case 'Y':
        ss << t->tm_year + 1900;
      break;
      case 'H':
        ss << std::setfill('0') << std::setw(2) << t->tm_hour;
      break;
      case 'M':
        ss << std::setfill('0') << std::setw(2) << t->tm_min;
      break;
      case 'S':
        ss << std::setfill('0') << std::setw(2) << t->tm_sec;
      break;
      case '.':
        ss << '.';
      break;
      default:
        ss << format[i];
      break;
    }
  }
  return ss.str();
}


/* Takes integer cents and formats it into dollars as a string
 *
 *
 * Input:
 *   int cents:
 *     The number of cents
 *
 * Output:
 *   std::string:
 *     A string with ...XX.YY format
 *
 * Eg:
 *   toCurrency(1234) => "12.34"
 *
 */
std::string toCurrency(int cents) {
  std::stringstream ss;
  ss << std::setfill('0') << std::setw(1) << cents / 100
     << '.' << std::setw(2) << cents % 100;
  return ss.str();
}

