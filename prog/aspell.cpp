// This file is part of The New Aspell
// Copyright (C) 2002 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

//
// NOTE: This program currently uses a very ugly mix of the internal
//       API and the external C interface.  The eventual goal is to
//       use only the external C++ interface, however, the external
//       C++ interface is currently incomplete.  The C interface is
//       used in some places because without the strings will not get
//       converted properly when the encoding is not the same as the
//       internal encoding used by Aspell.
// 

#include <deque>
#include <ctype.h>
#include "settings.h"


#ifdef USE_LOCALE
# include <locale.h>
#endif

#include "aspell.h"
//FIXME if Win(dos) is different
#include <sys/types.h>
#include <regex.h>

#include "asc_ctype.hpp"
#include "check_funs.hpp"
#include "config.hpp"
#include "document_checker.hpp"
#include "enumeration.hpp"
#include "errors.hpp"
#include "file_util.hpp"
#include "fstream.hpp"
#include "info.hpp"
#include "iostream.hpp"
#include "posib_err.hpp"
#include "speller.hpp"
#include "stack_ptr.hpp"
#include "string_enumeration.hpp"
#include "string_map.hpp"
#include "word_list.hpp"

#include "speller_impl.hpp"
#include "data.hpp"
#include "directory.hpp"

using namespace acommon;

// action functions declarations

void print_ver();
void print_help();
void expand_expression(Config * config);
void config();

void check(bool interactive);
void pipe();
void filter();
void list();
void dicts();

void master();
void personal();
void repl();
void soundslike();
void munch();
void expand();

void print_error(ParmString msg)
{
  fputs(_("Error: "), stderr);
  fputs(msg, stderr);
  fputs("\n", stderr);
}

void print_error(ParmString msg, ParmString str)
{
  fputs(_("Error: "), stderr);
  fprintf(stderr, msg.str(), str.str());
  fputs("\n", stderr);
}

#define EXIT_ON_ERR(command) \
  do{PosibErrBase pe(command);\
  if(pe.has_err()){print_error(pe.get_err()->mesg); exit(1);}\
  } while(false)
#define EXIT_ON_ERR_SET(command, type, var)\
  type var;\
  do{PosibErr<type> pe(command);\
  if(pe.has_err()){print_error(pe.get_err()->mesg); exit(1);}\
  else {var=pe.data;}\
  } while(false)
#define BREAK_ON_ERR(command) \
  do{PosibErrBase pe(command);\
  if(pe.has_err()){print_error(pe.get_err()->mesg); break;}\
  } while(false)
#define BREAK_ON_ERR_SET(command, type, var)\
  type var;\
  do{PosibErr<type> pe(command);\
  if(pe.has_err()){print_error(pe.get_err()->mesg); break;}\
  else {var=pe.data;}\
  } while(false)


/////////////////////////////////////////////////////////
//
// Command line options functions and classes
// (including main)
//

typedef std::deque<String> Args;
typedef Config        Options;
enum Action {do_create, do_merge, do_dump, do_test, do_other};

Args              args;
StackPtr<Options> options(new_config());
Action            action  = do_other;

struct PossibleOption {
  const char * name;
  char         abrv;
  int          num_arg;
  bool         is_command;
};

#define OPTION(name,abrv,num)         {name,abrv,num,false}
#define COMMAND(name,abrv,num)        {name,abrv,num,true}
#define ISPELL_COMP(abrv,num)         {"",abrv,num,false}

const PossibleOption possible_options[] = {
  OPTION("master",           'd',  1),
  OPTION("personal",         'p',  1),
  OPTION("ignore",            'W', 1),
  OPTION("backup",           'b' , 0),
  OPTION("dont-backup",      'x' , 0),
  OPTION("run-together",     'C',  0),
  OPTION("dont-run-together",'B',  0),
  OPTION("include-guesses",      'm', 0),
  OPTION("dont-include-guesses", 'P', 0),
  
  COMMAND("version",   'v', 0),
  COMMAND("help",      '?', 0),
  COMMAND("config",    '\0', 0),
  COMMAND("check",     'c', 0),
  COMMAND("pipe",      'a', 0),
  COMMAND("filter",    '\0', 0),
  COMMAND("soundslike",'\0', 0),
  COMMAND("munch",     '\0', 0),
  COMMAND("expand",    '\0', 0),
  COMMAND("list",      'l', 0),
  COMMAND("dicts",     '\0', 0),

  COMMAND("dump",   '\0', 1),
  COMMAND("create", '\0', 1),
  COMMAND("merge",  '\0', 1),

  ISPELL_COMP('n',0), ISPELL_COMP('S',0), 
  ISPELL_COMP('w',1), ISPELL_COMP('T',1),

  {"",'\0'}, {"",'\0'}
};

const PossibleOption * possible_options_end = possible_options + sizeof(possible_options)/sizeof(PossibleOption) - 2;

struct ModeAbrv {
  char abrv;
  const char * mode;
  const char * desc;
};
static const ModeAbrv mode_abrvs[] = {
  {'e', "mode=email", N_("enter Email mode.")},
  {'H', "mode=sgml",  N_("enter Html/Sgml mode.")},
  {'t', "mode=tex",   N_("enter TeX mode.")},
};

static const ModeAbrv *  mode_abrvs_end = mode_abrvs + 3;

const PossibleOption * find_option(char c) {
  const PossibleOption * i = possible_options;
  while (i != possible_options_end && i->abrv != c) 
    ++i;
  return i;
}

static inline bool str_equal(const char * begin, const char * end, 
			     const char * other) 
{
  while(begin != end && *begin == *other)
    ++begin, ++other;
  return (begin == end && *other == '\0');
}

static const PossibleOption * find_option(const char * begin, const char * end) {
  const PossibleOption * i = possible_options;
  while (i != possible_options_end 
	 && !str_equal(begin, end, i->name))
    ++i;
  return i;
}

static const PossibleOption * find_option(const char * str) {
  const PossibleOption * i = possible_options;
  while (i != possible_options_end 
	 && !strcmp(str, i->name) == 0)
    ++i;
  return i;
}

int main (int argc, const char *argv[]) 
{
#ifdef USE_LOCALE
  setlocale (LC_ALL, "");
#endif
#ifdef ENABLE_NLS
  bindtextdomain (PACKAGE, LOCALEDIR);
  textdomain (PACKAGE);
#endif

  EXIT_ON_ERR(options->read_in_settings());

  if (argc == 1) {print_help(); return 0;}

  int i = 1;
  const PossibleOption * o;
  const char           * parm;

  //
  // process command line options by setting the appropriate options
  // in "options" and/or pushing non-options onto "argv"
  //
  PossibleOption other_opt = OPTION("",'\0',0);
  String option_name;
  while (i != argc) {
    if (argv[i][0] == '-') {
      if (argv[i][1] == '-') {
	// a long arg
	const char * c = argv[i] + 2;
	while(*c != '=' && *c != '\0') ++c;
	o = find_option(argv[i] + 2, c);
	if (o == possible_options_end) {
	  option_name.assign(argv[i] + 2, 0, c - argv[i] - 2);
	  const char * base_name = Config::base_name(option_name.c_str());
	  PosibErr<const KeyInfo *> ki = options->keyinfo(base_name);
          if (!ki.has_err(unknown_key)) {
            other_opt.name    = option_name.c_str();
            other_opt.num_arg = ki.data->type == KeyInfoBool ? 0 : 1;
            o = &other_opt;
          }
	} 
	if (*c == '=') ++c;
	parm = c;
      } else {
	// a short arg
	const ModeAbrv * j = mode_abrvs;
	while (j != mode_abrvs_end && j->abrv != argv[i][1]) ++j;
	if (j == mode_abrvs_end) {
	  o = find_option(argv[i][1]);
	  if (argv[i][1] == 'v' && argv[i][2] == 'v')
	    // Hack for -vv
	    parm = argv[i] + 3;
	  else
	    parm = argv[i] + 2;
	} else {
	  other_opt.name = "mode";
	  other_opt.num_arg = 1;
	  o = &other_opt;
	  parm = j->mode + 5;
	}
      }
      if (o == possible_options_end) {
	print_error(_("Invalid Option: %s"), argv[i]);
	return 1;
      }
      if (o->num_arg == 0) {
	if (parm[0] != '\0') {
	  print_error(_(" does not take any parameters."), 
		      String(argv[i], parm - argv[i]));
	  return 1;
	}
	i += 1;
      } else { // o->num_arg == 1
	if (parm[0] == '\0') {
	  if (i + 1 == argc) {
	    print_error(_("You must specify a parameter for %s"), argv[i]);
	    return 1;
	  }
	  parm = argv[i + 1];
	  i += 2;
	} else {
	  i += 1;
	}
      }
      if (o->is_command) {
	args.push_back(o->name);
	if (o->num_arg == 1)
	  args.push_back(parm);
      } else {
	if (o->name[0] != '\0') {
	  EXIT_ON_ERR(options->replace(o->name, parm));
	}
      }
    } else {
      args.push_back(argv[i]);
      i += 1;
    }
  }

  if (args.empty()) {
    print_error(_("You must specify an action"));
    return 1;
  }

  //
  // perform the requested action
  //
  String action_str = args.front();
  args.pop_front();
  if (action_str == "help")
    print_help();
  else if (action_str == "version")
    print_ver();
  else if (action_str == "config")
    config();
  else if (action_str == "dicts")
    dicts();
  else if (action_str == "check")
    check(true);
  else if (action_str == "pipe")
    pipe();
  else if (action_str == "list")
    check(false);
  else if (action_str == "filter")
    filter();
  else if (action_str == "soundslike")
    soundslike();
  else if (action_str == "munch")
    munch();
  else if (action_str == "expand")
    expand();
  else if (action_str == "dump")
    action = do_dump;
  else if (action_str == "create")
    action = do_create;
  else if (action_str == "merge")
    action = do_merge;
  else {
    print_error(_("Unknown Action: %s"),  action_str);
    return 1;
  }

  if (action != do_other) {
    if (args.empty()) {
      print_error(_("Unknown Action: %s"),  action_str);
      return 1;
    }
    String what_str = args.front();
    args.pop_front();
    if (what_str == "config")
      config();
    else if (what_str == "dicts")
      dicts();
    else if (what_str == "master")
      master();
    else if (what_str == "personal")
      personal();
    else if (what_str == "repl")
      repl();
    else {
      print_error(_("Unknown Action: %s"),
		  String(action_str + " " + what_str));
      return 1;
    }
  }

  return 0;
}

/////////////////////////////////////////////////////////
//
// Action Functions
//
//

///////////////////////////
//
// config
//

void config () 
{
  StackPtr<Config> config(new_config());
  EXIT_ON_ERR(config->read_in_settings(options));
  if ((args.size() > 0) &&
      (args[0] == "+e")) {
    args.pop_front();
    if (args.size() == 0) {
      args.push_front("all");
    }
    expand_expression(config);
    args.pop_front();
  }
  if (args.size() == 0) {
    config->write_to_stream(COUT);
  }
  else {
    EXIT_ON_ERR_SET(config->retrieve(args[0]), String, value);
    COUT << value << "\n";
  }
}

///////////////////////////
//
// dicts
//

void dicts() 
{
  const DictInfoList * dlist = get_dict_info_list(options);

  StackPtr<DictInfoEnumeration> dels(dlist->elements());

  const DictInfo * entry;

  while ( (entry = dels->next()) != 0) 
  {
    COUT << entry->name << "\n";
  }

}


///////////////////////////
//
// pipe
//

// precond: strlen(str) > 0
char * trim_wspace (char * str)
{
  int last = strlen(str) - 1;
  while (asc_isspace(str[0])) {
    ++str;
    --last;
  }
  while (last > 0 && asc_isspace(str[last])) {
    --last;
  }
  str[last + 1] = '\0';
  return str;
}

bool get_word_pair(char * line, char * & w1, char * & w2)
{
  w2 = strchr(line, ',');
  if (!w2) {
    print_error(_("Invalid Input"));
    return false;
  }
  *w2 = '\0';
  ++w2;
  w1 = trim_wspace(line);
  w2 = trim_wspace(w2);
  return true;
}

void print_elements(const AspellWordList * wl) {
  AspellStringEnumeration * els = aspell_word_list_elements(wl);
  int count = 0;
  const char * w;
  String line;
  while ( (w = aspell_string_enumeration_next(els)) != 0 ) {
    ++count;
    line += w;
    line += ", ";
  }
  line.resize(line.size() - 2);
  COUT << count << ": " << line << "\n";
}

void status_fun(void * d, Token, int correct)
{
  if (*static_cast<bool *>(d) && correct)
    COUT << "*\n";
}

DocumentChecker * new_checker(AspellSpeller * speller, 
			      bool & print_star) 
{
  EXIT_ON_ERR_SET(new_document_checker(reinterpret_cast<Speller *>(speller)),
		  StackPtr<DocumentChecker>, checker);
  checker->set_status_fun(status_fun, &print_star);
  return checker.release();
}

#define BREAK_ON_SPELLER_ERR\
  do {if (aspell_speller_error(speller)) {\
    print_error(aspell_speller_error_message(speller)); break;\
  } } while (false)

void pipe() 
{
#ifndef WIN32
  // set up stdin and stdout to be line buffered
  assert(setvbuf(stdin, 0, _IOLBF, 0) == 0); 
  assert(setvbuf(stdout, 0, _IOLBF, 0) == 0);
#endif

  bool terse_mode = true;
  bool do_time = options->retrieve_bool("time");
  bool suggest = options->retrieve_bool("suggest");
  bool include_guesses = options->retrieve_bool("include-guesses");
  clock_t start,finish;
  start = clock();

  AspellCanHaveError * ret 
    = new_aspell_speller(reinterpret_cast<AspellConfig *>(options.get()));
  if (aspell_error(ret)) {
    print_error(aspell_error_message(ret));
    exit(1);
  }
  AspellSpeller * speller = to_aspell_speller(ret);
  Config * config = reinterpret_cast<Speller *>(speller)->config();
  if (do_time)
    COUT << _("Time to load word list: ")
         << (clock() - start)/(double)CLOCKS_PER_SEC << "\n";
  bool print_star = true;
  StackPtr<DocumentChecker> checker(new_checker(speller, print_star));
  int c;
  const char * w;
  CharVector buf;
  char * line;
  char * word;
  char * word2;
  int    ignore;
  PosibErrBase err;

  print_ver();

  for (;;) {
    buf.clear();
    fflush(stdout);
    while (c = getchar(), c != '\n' && c != EOF)
      buf.push_back(static_cast<char>(c));
    buf.push_back('\n'); // always add new line so strlen > 0
    buf.push_back('\0');
    line = buf.data();
    ignore = 0;
    switch (line[0]) {
    case '\n':
      if (c != EOF) continue;
      else          break;
    case '*':
      word = trim_wspace(line + 1);
      aspell_speller_add_to_personal(speller, word, -1);
      BREAK_ON_SPELLER_ERR;
      break;
    case '&':
      word = trim_wspace(line + 1);
      aspell_speller_add_to_personal
	(speller, 
	 reinterpret_cast<Speller *>(speller)->to_lower(word), -1);
      BREAK_ON_SPELLER_ERR;
      break;
    case '@':
      word = trim_wspace(line + 1);
      aspell_speller_add_to_session(speller, word, -1);
      BREAK_ON_SPELLER_ERR;
      break;
    case '#':
      aspell_speller_save_all_word_lists(speller);
      BREAK_ON_SPELLER_ERR;
      break;
    case '+':
      word = trim_wspace(line + 1);
      err = config->replace("mode", word);
      if (err.get_err())
	config->replace("mode", "tex");
      reload_filters(reinterpret_cast<Speller *>(speller));
      checker.del();
      checker = new_checker(speller, print_star);
      break;
    case '-':
      config->remove("filter");
      reload_filters(reinterpret_cast<Speller *>(speller));
      checker.del();
      checker = new_checker(speller, print_star);
      break;
    case '~':
      break;
    case '!':
      terse_mode = true;
      print_star = false;
      break;
    case '%':
      terse_mode = false;
      print_star = true;
      break;
    case '$':
      if (line[1] == '$') {
	switch(line[2]) {
	case 'r':
	  switch(line[3]) {
	  case 'a':
	    if (get_word_pair(line + 4, word, word2))
	      aspell_speller_store_replacement(speller, word, -1, word2, -1);
	    break;
	  }
	  break;
	case 'c':
	  switch (line[3]) {
	  case 's':
	    if (get_word_pair(line + 4, word, word2))
	      BREAK_ON_ERR(err = config->replace(word, word2));
            if (strcmp(word,"suggest") == 0)
              suggest = config->retrieve_bool("suggest");
	    break;
	  case 'r':
	    word = trim_wspace(line + 4);
	    BREAK_ON_ERR_SET(config->retrieve(word), 
			     PosibErr<String>, ret);
	    break;
	  }
	  break;
	case 'p':
	  switch (line[3]) {
	  case 'p':
	    print_elements(aspell_speller_personal_word_list(speller));
	    break;
	  case 's':
	    print_elements(aspell_speller_session_word_list(speller));
	    break;
	  }
	  break;
	case 'l':
	  COUT << config->retrieve("lang") << "\n";
	  break;
	}
	break;
      } else {
	// continue on (no break)
      }
    case '^':
      ignore = 1;
    default:
      line += ignore;
      checker->process(line, strlen(line));
      while (Token token = checker->next_misspelling()) {
	word = line + token.offset;
	word[token.len] = '\0';
        String guesses, guess;
        const CheckInfo * ci = reinterpret_cast<Speller *>(speller)->check_info();
        aspeller::CasePattern casep 
          = aspeller::case_pattern(reinterpret_cast<aspeller::SpellerImpl *>
                                   (speller)->lang(), word);
        while (ci) {
          guess.clear();
          if (ci->pre_add && ci->pre_add[0])      guess << ci->pre_add << "+";
          guess << ci->word;
          if (ci->pre_strip && ci->pre_strip[0]) guess << "-" << ci->pre_strip;
          if (ci->suf_strip && ci->suf_strip[0]) guess << "-" << ci->suf_strip;
          if (ci->suf_add   && ci->suf_add[0])   guess << "+" << ci->suf_add;
          guesses << ", " 
                  << aspeller::fix_case(reinterpret_cast<aspeller::SpellerImpl * >(speller)->lang(),
                                        casep, guess);
          ci = ci->next;
        }
	start = clock();
        const AspellWordList * suggestions = 0;
        if (suggest) 
          suggestions = aspell_speller_suggest(speller, word, -1);
	finish = clock();
	if (suggestions && !aspell_word_list_empty(suggestions)) 
        {
	  COUT << "& " << word 
	       << " " << aspell_word_list_size(suggestions) 
	       << " " << token.offset + ignore
	       << ":";
	  AspellStringEnumeration * els 
	    = aspell_word_list_elements(suggestions);
	  if (options->retrieve_bool("reverse")) {
	    Vector<String> sugs;
	    sugs.reserve(aspell_word_list_size(suggestions));
	    while ( ( w = aspell_string_enumeration_next(els)) != 0)
	      sugs.push_back(w);
	    Vector<String>::reverse_iterator i = sugs.rbegin();
	    while (true) {
	      COUT << " " << *i;
	      ++i;
	      if (i == sugs.rend()) break;
	      COUT << ",";
	    }
	  } else {
	    while ( ( w = aspell_string_enumeration_next(els)) != 0) {
	      COUT << " " << w;
	      if (!aspell_string_enumeration_at_end(els))
		COUT << ",";
	    }
	  }
	  delete_aspell_string_enumeration(els);
          if (include_guesses)
            COUT << guesses;
	  COUT << "\n";
	} else {
          if (guesses.empty())
            COUT << "# " << word << " " 
                 << token.offset + ignore
                 << "\n";
          else
            COUT << "? " << word << " 0 " 
                 << token.offset + ignore
                 << ": " << guesses.c_str() + 2;
	}
	if (do_time)
          COUT << _("Suggestion Time: ")
               << (finish-start)/(double)CLOCKS_PER_SEC << "\n";
      }
      COUT << "\n";
    }
    if (c == EOF) break;
  }

  delete_aspell_speller(speller);
}

///////////////////////////
//
// check
//

enum UserChoice {None, Ignore, IgnoreAll, Replace, ReplaceAll, 
		 Add, AddLower, Exit, Abort};

struct Mapping {
  char primary[9];
  UserChoice reverse[256];
  void to_aspell();
  void to_ispell();
  char & operator[] (UserChoice c) {return primary[c];}
  UserChoice & operator[] (char c) 
    {return reverse[static_cast<unsigned char>(c)];}
};

void abort_check();

void check(bool interactive)
{
  String file_name;
  String new_name;
  FILE * in = 0;
  FILE * out = 0;
  Mapping mapping;

  if (interactive) {
    if (args.size() == 0) {
      print_error(_("You must specify a file name."));
      exit(-1);
    }
    
    file_name = args[0];
    new_name = file_name;
    new_name += ".new";

    in = fopen(file_name.c_str(), "r");
    if (!in) {
      print_error(_("Could not open the file \"%s\" for reading"), file_name);
      exit(-1);
    }
    
    out = fopen(new_name.c_str(), "w");
    if (!out) {
      print_error(_("Could not open the file \"%s\"  for writing. File not saved."), file_name);
      exit(-1);
    }

    if (!options->have("mode"))
      set_mode_from_extension(options, file_name);
    
    String m = options->retrieve("keymapping");
    if (m == "aspell")
      mapping.to_aspell();
    else if (m == "ispell")
      mapping.to_ispell();
    else {
      print_error(_("Invalid keymapping: %s"), m);
      exit(-1);
    }

  } else {
    in = stdin;
  }

  AspellCanHaveError * ret 
    = new_aspell_speller(reinterpret_cast<AspellConfig *>(options.get()));
  if (aspell_error(ret)) {
    print_error(aspell_error_message(ret));
    exit(1);
  }
  AspellSpeller * speller = to_aspell_speller(ret);

  state = new CheckerString(speller,in,out,64);
 
  word_choices = new Choices;

  menu_choices = new Choices;
  menu_choices->push_back(Choice(mapping[Ignore],     _("Ignore")));
  menu_choices->push_back(Choice(mapping[IgnoreAll],  _("Ignore all")));
  menu_choices->push_back(Choice(mapping[Replace],    _("Replace")));
  menu_choices->push_back(Choice(mapping[ReplaceAll], _("Replace all")));
  menu_choices->push_back(Choice(mapping[Add],        _("Add")));
  menu_choices->push_back(Choice(mapping[AddLower],   _("Add Lower")));
  menu_choices->push_back(Choice(mapping[Abort],      _("Abort")));
  menu_choices->push_back(Choice(mapping[Exit],       _("Exit")));

  String new_word;
  Vector<String> sug_con;
  StackPtr<StringMap> replace_list(new_string_map());
  const char * w;

  if (interactive)
    begin_check();

  while (state->next_misspelling()) {

    CharVector word0;
    char * word = state->get_word(word0);

    if (interactive) {

      //
      // check if it is in the replace list
      //

      if ((w = replace_list->lookup(word)) != 0) {
	state->replace(w);
	continue;
      }

      //
      // print the line with the misspelled word highlighted;
      //

      display_misspelled_word();

      //
      // print the suggestions and menu choices
      //

      const AspellWordList * suggestions = aspell_speller_suggest(speller, 
								  word, -1);
      AspellStringEnumeration * els = aspell_word_list_elements(suggestions);
      sug_con.resize(0);
      while (sug_con.size() != 10 
	     && (w = aspell_string_enumeration_next(els)) != 0)
	sug_con.push_back(w);
      delete_aspell_string_enumeration(els);

      // disable suspend
      unsigned int suggestions_size = sug_con.size();
      unsigned int suggestions_mid = suggestions_size / 2;
      if (suggestions_size % 2) suggestions_mid++; // if odd
      word_choices->resize(0);
      for (unsigned int j = 0; j != suggestions_mid; ++j) {
	word_choices->push_back(Choice('0' + j+1, sug_con[j]));
	if (j + suggestions_mid != suggestions_size) 
	  word_choices
	    ->push_back(Choice(j+suggestions_mid+1 == 10 
			       ? '0' 
			       : '0' + j+suggestions_mid+1,
			       sug_con[j+suggestions_mid]));
      }
      //enable suspend
      display_menu();

    choice_prompt:

      prompt("? ");

    choice_loop:

      //
      // Handle the users choice
      //

      int choice;
      get_choice(choice);
      
      if (choice == '0') choice = '9' + 1;
    
      switch (mapping[choice]) {
      case Exit:
	goto exit_loop;
      case Abort:
	prompt(_("Are you sure you want to abort? "));
	get_choice(choice);
	if (choice == 'y' || choice == 'Y')
	  goto abort_loop;
	goto choice_prompt;
      case Ignore:
	break;
      case IgnoreAll:
	aspell_speller_add_to_session(speller, word, -1);
	break;
      case Add:
	aspell_speller_add_to_personal(speller, word, -1);
	break;
      case AddLower:
	aspell_speller_add_to_personal
	  (speller, 
	   reinterpret_cast<Speller *>(speller)->to_lower(word), -1);
	break;
      case Replace:
      case ReplaceAll:
	prompt(_("With: "));
	get_line(new_word);
	if (new_word.size() == 0)
	  goto choice_prompt;
	if (new_word[0] >= '1' && new_word[0] < (char)suggestions_size + '1')
	  new_word = sug_con[new_word[0]-'1'];
	state->replace(new_word);
	if (mapping[choice] == ReplaceAll)
	  replace_list->replace(word, new_word);
	break;
      default:
	if (choice >= '1' && choice < (char)suggestions_size + '1') { 
	  state->replace(sug_con[choice-'1']);
	} else {
	  error(_("Sorry that is an invalid choice!"));
	  goto choice_loop;
	}
      }

    } else { // !interactive
      
      COUT << word << "\n";
      
    }
  }
exit_loop:
  {
    aspell_speller_save_all_word_lists(speller);
    state.del(); // to close the file handles
    delete_aspell_speller(speller);
    
    bool keep_backup = options->retrieve_bool("backup");
    String backup_name = file_name;
    backup_name += ".bak";
    if (keep_backup)
      rename_file(file_name, backup_name);
    rename_file(new_name, file_name);
    
    //end_check();
    
    return;
  }
abort_loop:
  {
    state.del(); // to close the file handles
    delete_aspell_speller(speller);

    remove_file(new_name);

    return;
  }
}

void Mapping::to_aspell() 
{
  memset(this, 0, sizeof(Mapping));
  primary[Ignore    ] = 'i';
  reverse['i'] = Ignore;
  reverse[' '] = Ignore;
  reverse['\n'] = Ignore;

  primary[IgnoreAll ] = 'I';
  reverse['I'] = IgnoreAll;

  primary[Replace   ] = 'r';
  reverse['r'] = Replace;

  primary[ReplaceAll] = 'R';
  reverse['R'] = ReplaceAll;

  primary[Add       ] = 'a';
  reverse['A'] = Add;
  reverse['a'] = Add;

  primary[AddLower  ] = 'l';
  reverse['L'] = AddLower;
  reverse['l'] = AddLower;

  primary[Abort     ] = 'b';
  reverse['b'] = Abort;
  reverse['B'] = Abort;
  reverse[control('c')] = Abort;

  primary[Exit      ] = 'x';
  reverse['x'] = Exit;
  reverse['X'] = Exit;
}

void Mapping::to_ispell() 
{
  memset(this, 0, sizeof(Mapping));
  primary[Ignore    ] = ' ';
  reverse[' '] = Ignore;
  reverse['\n'] = Ignore;

  primary[IgnoreAll ] = 'A';
  reverse['A'] = IgnoreAll;
  reverse['a'] = IgnoreAll;

  primary[Replace   ] = 'R';
  reverse['R'] = ReplaceAll;
  reverse['r'] = Replace;

  primary[ReplaceAll] = 'E';
  reverse['E'] = ReplaceAll;
  reverse['e'] = Replace;

  primary[Add       ] = 'I';
  reverse['I'] = Add;
  reverse['i'] = Add;

  primary[AddLower  ] = 'U';
  reverse['U'] = AddLower;
  reverse['u'] = AddLower;

  primary[Abort     ] = 'Q';
  reverse['Q'] = Abort;
  reverse['q'] = Abort;
  reverse[control('c')] = Abort;

  primary[Exit      ] = 'X';
  reverse['X'] = Exit;
  reverse['x'] = Exit;
}

///////////////////////////
//
// filter
//

void filter()
{
  //assert(setvbuf(stdin, 0, _IOLBF, 0) == 0);
  //assert(setvbuf(stdout, 0, _IOLBF, 0) == 0);
  CERR << _("Sorry \"filter\" is currently unimplemented.\n");
  exit(3);
}


///////////////////////////
//
// print_ver
//

void print_ver () {
  COUT << "@(#) International Ispell Version 3.1.20 " 
       << "(but really Aspell " << VERSION << ")" << "\n";
}

///////////////////////////////////////////////////////////////////////
//
// These functions use implementation details of the default speller
// module
//

///////////////////////////
//
// master
//

class IstreamVirEnumeration : public StringEnumeration {
  FStream * in;
  String data;
public:
  IstreamVirEnumeration(FStream & i) : in(&i) {}
  IstreamVirEnumeration * clone() const {
    return new IstreamVirEnumeration(*this);
  }
  void assign (const StringEnumeration * other) {
    *this = *static_cast<const IstreamVirEnumeration *>(other);
  }
  Value next() {
    *in >> data;
    if (!*in) return 0;
    else return data.c_str();
  }
  bool at_end() const {return *in;}
};

void dump (aspeller::LocalWordSet lws) 
{
  using namespace aspeller;

  switch (lws.word_set->basic_type) {
  case DataSet::basic_word_set:
    {
      BasicWordSet  * ws = static_cast<BasicWordSet *>(lws.word_set);
      BasicWordSet::Emul els = ws->detailed_elements();
      BasicWordInfo wi;
      while (wi = els.next(), wi)
	wi.write(COUT,*(ws->lang()), lws.local_info.convert) << "\n";
    }
    break;
  case DataSet::basic_multi_set:
    {
      BasicMultiSet::Emul els 
	= static_cast<BasicMultiSet *>(lws.word_set)->detailed_elements();
      LocalWordSet ws;
      while (ws = els.next(), ws) 
	dump (ws);
    }
    break;
  default:
    abort();
  }
}

void master () {
  using namespace aspeller;

  if (args.size() != 0) {
    options->replace("master", args[0].c_str());
  }

  StackPtr<Config> config(new_basic_config());
  EXIT_ON_ERR(config->read_in_settings(options));

  if (action == do_create) {
    
    EXIT_ON_ERR(create_default_readonly_word_set
                (new IstreamVirEnumeration(CIN),
                 *config));

  } else if (action == do_merge) {
    
    print_error(_("Can't merge a master word list yet. Sorry."));
    exit (1);
    
  } else if (action == do_dump) {

    EXIT_ON_ERR_SET(add_data_set(config->retrieve("master-path"), 
                                 *config),
                    LoadableDataSet *, mas);
    LocalWordSetInfo wsi;
    wsi.set(mas->lang(), config);
    dump(LocalWordSet(mas,wsi));
    delete mas;
    
  }
}

///////////////////////////
//
// personal
//

void personal () {
  using namespace aspeller;

  if (args.size() != 0) {
    EXIT_ON_ERR(options->replace("personal", args[0].c_str()));
  }
  options->replace("module", "aspeller");
  if (action == do_create || action == do_merge) {
    CERR << _("Sorry \"create/merge personal\" is currently unimplemented.\n");
    exit(3);

    // FIXME
#if 0
    StackPtr<Speller> speller(new_speller(options));

    if (action == do_create) {
      if (file_exists(speller->config()->retrieve("personal-path"))) {
        print_error(_("Sorry I won't overwrite \"%s\""), 
		    speller->config()->retrieve("personal-path"));
        exit (1);
      }
      speller->personal_word_list().data->clear();
    }

    String word;
    while (CIN >> word) 
      speller->add_to_personal(word);

    speller->save_all_word_lists();
#endif

  } else { // action == do_dump

    StackPtr<Config> config(new_basic_config());
    EXIT_ON_ERR(config->read_in_settings(options));

    WritableWordSet * per = new_default_writable_word_set();
    per->load(config->retrieve("personal-path"), config);
    WritableWordSet::Emul els = per->detailed_elements();
    LocalWordSetInfo wsi;
    wsi.set(per->lang(), config);
    BasicWordInfo wi;
    while (wi = els.next(), wi) {
      wi.write(COUT,*(per->lang()), wsi.convert);
      COUT << "\n";
    }
    delete per;
  }
}

///////////////////////////
//
// repl
//

void repl() {
  using namespace aspeller;

  if (args.size() != 0) {
    options->replace("repl", args[0].c_str());
  }

  if (action == do_create || action == do_merge) {

    CERR << _("Sorry \"create/merge repl\" is currently unimplemented.\n");
    exit(3);

    // FIXME
#if 0
    SpellerImpl speller(options);

    if (action == do_create) {
      if (file_exists(speller->config()->retrieve("repl-path"))) {
        print_error(_("Sorry I won't overwrite \"%s\""),
		    speller->config()->retrieve("repl-path"));
        exit (1);
      }
      speller->personal_repl().clear();
    }
    
    try {
      String word,repl;

      while (true) {
	get_word_pair(word,repl,':');
	EXIT_ON_ERR(speller->store_repl(word,repl,false));
      }

    } catch (bad_cin) {}

    EXIT_ON_ERR(speller->personal_repl().synchronize());
#endif
  } else if (action == do_dump) {

    StackPtr<Config> config(new_basic_config());
    EXIT_ON_ERR(config->read_in_settings());

    WritableReplacementSet * repl = new_default_writable_replacement_set();
    repl->load(config->retrieve("repl-path"), config);
    WritableReplacementSet::Emul els = repl->elements();
 
    ReplacementList rl;
    while ( !(rl = els.next()).empty() ) {
      while (!rl.elements->at_end()) {
	COUT << rl.misspelled_word << ": " << rl.elements->next() << "\n";
      }
      delete rl.elements;
    }
    delete repl;
  }

}

//////////////////////////
//
// soundslike
//

void soundslike() {
  using namespace aspeller;
  CachePtr<Language> lang;
  PosibErr<Language *> res = new_language(*options);
  if (!res) {print_error(res.get_err()->mesg); exit(1);}
  lang.reset(res.data);
  String word;
  while (CIN >> word) {
    COUT << word << '\t' << lang->to_soundslike(word) << "\n";
  } 
}

//////////////////////////
//
// munch
//

void munch() 
{
  using namespace aspeller;
  CachePtr<Language> lang;
  PosibErr<Language *> res = new_language(*options);
  if (!res) {print_error(res.get_err()->mesg); exit(1);}
  lang.reset(res.data);
  String word;
  CheckList * cl = new_check_list();
  while (CIN >> word) {
    lang->affix()->munch(word, cl);
    COUT << word;
    for (const aspeller::CheckInfo * ci = check_list_data(cl); ci; ci = ci->next)
    {
      COUT << ' ' << ci->word << '/';
      if (ci->pre_flag != 0) COUT << static_cast<char>(ci->pre_flag);
      if (ci->suf_flag != 0) COUT << static_cast<char>(ci->suf_flag);
    }
    COUT << '\n';
  }
  delete_check_list(cl);
}

//////////////////////////
//
// expand
//

void expand() 
{
  int level = 1;
  if (args.size() != 0)
    level = atoi(args[0].c_str());
  
  using namespace aspeller;
  CachePtr<Language> lang;
  PosibErr<Language *> res = new_language(*options);
  if (!res) {print_error(res.get_err()->mesg); exit(1);}
  lang.reset(res.data);
  String word;
  CheckList * cl = new_check_list();
  while (CIN >> word) {
    CharVector buf; buf.append(word.c_str(), word.size() + 1);
    char * w = buf.data();
    char * af = strchr(w, '/');
    af[0] = '\0';
    lang->affix()->expand(ParmString(w, af-w), ParmString(af + 1), cl);
    const aspeller::CheckInfo * ci = check_list_data(cl);
    if (level <= 2) {
      if (level == 2) 
        COUT << word << ' ';
      while (ci) {
        COUT << ci->word;
        ci = ci->next;
        if (ci) COUT << ' ';
      }
      COUT << '\n';
    } else if (level >= 3) {
      while (ci) {
        COUT << word << ' ' << ci->word << '\n';
        ci = ci->next;
      }
    }
  }
  delete_check_list(cl);
}


///////////////////////////////////////////////////////////////////////


///////////////////////////
//
// print_help
//

void print_help_line(char abrv, char dont_abrv, const char * name, 
		     KeyInfoType type, const char * desc, bool no_dont = false) 
{
  String command;
  if (abrv != '\0') {
    command += '-';
    command += abrv;
    if (dont_abrv != '\0') {
      command += '|';
      command += '-';
      command += dont_abrv;
    }
    command += ',';
  }
  command += "--";
  if (type == KeyInfoBool && !no_dont) command += "[dont-]";
  if (type == KeyInfoList) command += "add|rem-";
  command += name;
  if (type == KeyInfoString || type == KeyInfoList) 
    command += "=<str>";
  if (type == KeyInfoInt)
    command += "=<int>";
  printf("  %-27s %s\n", command.c_str(), gettext (desc));
}

void expand_expression(Config * config){
  StringList filtpath;
  StringList optpath;
  PathBrowser optionpath;
  PathBrowser filterpath;
  String candidate;
  String toload;
  size_t locate_ending=0;
  size_t eliminate_path=0;
  size_t hold_eliminator=0;
//FIXME if Win(dos) is different
  regex_t seekfor;
  
  if (args.size() != 0) {
    if (args[0] == "all") {
      args[0]=".*";
    }
    config->retrieve_list("filter-path",&filtpath);
    config->retrieve_list("option-path",&optpath);
    filterpath=filtpath;
    optionpath=optpath;
    if (regcomp(&seekfor,args[0].c_str(),REG_NEWLINE|REG_NOSUB)) {
      make_err(invalid_expression,"help",args[0]);
      return;
    }
    while (filterpath.expand_file_part(&seekfor,candidate)) {
      if (((locate_ending=candidate.rfind("-filter.so")) !=
           candidate.length() - 10)) {
        if ((locate_ending=candidate.rfind(".flt")) !=
            candidate.length() - 4) {
          continue;
        }
        else {
          candidate.erase(locate_ending,4);
          eliminate_path=0;
          while ((hold_eliminator=candidate.find('/',eliminate_path)) < 
                 candidate.length()) {
            eliminate_path=hold_eliminator+1;
          }
          toload=candidate.erase(0,eliminate_path);
          if (regexec(&seekfor,toload.c_str(),0,NULL,0)) {
            continue;
          }
        }
      }
      else {
        candidate.erase(locate_ending,10);
        eliminate_path=0;
        while ((hold_eliminator=candidate.find('/',eliminate_path)) < 
               candidate.length()) {
          eliminate_path=hold_eliminator+1;
        }
        if (candidate.find("lib",eliminate_path) != eliminate_path) {
          continue;
        }
        candidate.erase(0,eliminate_path);
        candidate.erase(0,3);
        locate_ending=candidate.length();
        toload=candidate;
        if (regexec(&seekfor,toload.c_str(),0,NULL,0)) {
          continue;
        }
        candidate+="-filter.opt";
        if (!optionpath.expand_filename(candidate)) {
          continue;
        }
      }
      config->replace("add-filter",toload.c_str());
    }
    regfree(&seekfor);
  }
}

void print_help () {
  char * expandedoptionname=NULL;
  char * tempstring=NULL;
  size_t expandedsize=0;

  expand_expression(options);
  printf(_(
    "\n"
    "Aspell %s alpha.  Copyright 2000 by Kevin Atkinson.\n"
    "\n"
    "Usage: aspell [options] <command>\n"
    "\n"
    "<command> is one of:\n"
    "  -?|help [<expr>] display this help message\n"
    "                    and help for filters matching <expr> if installed\n"
    "  -c|check <file>  to check a file\n"
    "  -a|pipe          \"ispell -a\" compatibility mode\n"
    "  -l|list          produce a list of misspelled words from standard input\n"
    "  [dump] config [-e <expr>]  dumps the current configuration to stdout\n"
    "  config [+e <expr>] <key>   prints the current value of an option\n"
    "  soundslike       returns the sounds like equivalent for each word entered\n"
    "  munch            generate possible root words and affixes\n"
    "  expand [1-4]     expands affix flags\n"
    "  filter           passes standard input through filters\n"
    "  -v|version       prints a version line\n"
    "  dump|create|merge master|personal|repl [word list]\n"
    "    dumps, creates or merges a master, personal, or replacement word list.\n"
    "\n"
    "  <expr>           regular expression matching filtername(s) or `all'\n"
    "\n"
    "[options] is any of the following:\n"
    "\n"), VERSION);
  Enumeration<KeyInfoEnumeration> els = options->possible_elements();
  const KeyInfo * k;
  while (k = els.next(), k) {
    if (k->desc == 0) continue;
    if ((k->type == KeyInfoDescript) &&
        !strncmp(k->name,"filter-",7)){
      printf(_("\n"
               "  %s Filter: %s\n"
               "\tNOTE: in ambiguous case prefix following options by `filter-'\n"),
               &(k->name)[7],k->desc);
      if (expandedoptionname != NULL) {
        free(expandedoptionname);
        expandedsize=0;
      }
      if (!strncmp(k->name,"filter-",7)) {
        expandedoptionname=strdup(&(k->name[7]));
        expandedsize=strlen(k->name)-7;
      }
      else {
        expandedoptionname=strdup(k->name);
        expandedsize=strlen(k->name);
      }
      continue;
    }
    else if (k->type == KeyInfoDescript) {
      if (expandedoptionname != NULL) {
        free(expandedoptionname);
        expandedsize=0;
        expandedoptionname=NULL;
      }
    }
    if ((tempstring=(char*)malloc(expandedsize+strlen(k->name)+2)) == NULL) {
      expandedoptionname=NULL;
      continue;
    }
    tempstring[0]='\0';
    if ((strlen(k->name) < expandedsize) ||
        (expandedsize && strncmp(k->name,expandedoptionname,expandedsize))) {
      tempstring=strncat(tempstring,expandedoptionname,expandedsize);
      tempstring=strncat(tempstring,"-",1);
    }
    tempstring=strncat(tempstring,k->name,strlen(k->name));
    const PossibleOption * o = find_option(tempstring);
    print_help_line(o->abrv, 
		    strncmp((o+1)->name, "dont-", 5) == 0 ? (o+1)->abrv : '\0',
		    tempstring, k->type, k->desc);
    if (strcmp(tempstring, "mode") == 0) {
      for (const ModeAbrv * j = mode_abrvs;
           j != mode_abrvs_end;
           ++j)
      {
        print_help_line(j->abrv, '\0', j->mode, KeyInfoBool, j->desc, true);
      }
    }
    if (tempstring != NULL) {
      free(tempstring);
      tempstring=NULL;
    }
  }
  if (expandedoptionname!=NULL) {
    free(expandedoptionname);
  }

}

