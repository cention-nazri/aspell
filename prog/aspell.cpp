// This file is part of The New Aspell
// Copyright (C) 2002 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

//
// NOTE: There is a lot of duplicate code between proj/aspell.cpp and
//       modules/speller/default/aspell-util.cpp.  So when fixing bugs
//       check if the same bug is present in the other file and fix 
//       them both at the same time.
// 
//       This program currently uses a very ugly mix of the internal
//       API and the external C interface.  The eventual goal is to
//       use only the external C++ interface, however, the external
//       C++ interface is currently incomplete.  The C interface is
//       used in some places because without the strings will not get
//       converted properly when the encoding is not the same as the
//       internal encoding used by aspell.
// 

#include <deque>

#include <ctype.h>

#include "settings.h"

#include "aspell.h"

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

using namespace acommon;

// action functions declarations

void print_ver();
void print_help();
void config();

void check(bool interactive);
void pipe();
void filter();
void list();
void dicts();


#define EXIT_ON_ERR(command) \
  do{PosibErrBase pe = command;\
  if(pe.has_err()){CERR<<"Error: "<< pe.get_err()->mesg << "\n"; exit(1);}\
  } while(false)
#define EXIT_ON_ERR_SET(command, type, var)\
  type var;\
  do{PosibErr<type> pe = command;\
  if(pe.has_err()){CERR<<"Error: "<< pe.get_err()->mesg << "\n"; exit(1);}\
  else {var=pe.data;}\
  } while(false)
#define BREAK_ON_ERR(command) \
  do{PosibErrBase pe = command;\
  if(pe.has_err()){CERR<<"Error: "<< pe.get_err()->mesg << "\n"; return;}\
  } while(false)
#define BREAK_ON_ERR_SET(command, type, var)\
  type var;\
  do{PosibErr<type> pe = command;\
  if(pe.has_err()){CERR<<"Error: "<< pe.get_err()->mesg << "\n"; break;}\
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

  COMMAND("version",   'v', 0),
  COMMAND("help",      '?', 0),
  COMMAND("config",    '\0', 0),
  COMMAND("check",     'c', 0),
  COMMAND("pipe",      'a', 0),
  COMMAND("filter",    '\0', 0),
  COMMAND("list",      'l', 0),
  COMMAND("dicts",     '\0', 0),

  COMMAND("dump",   '\0', 1),

  ISPELL_COMP('n',0), ISPELL_COMP('P',0), ISPELL_COMP('m',0),
  ISPELL_COMP('S',0), ISPELL_COMP('w',1), ISPELL_COMP('T',1),

  {"",'\0'}, {"",'\0'}
};

const PossibleOption * possible_options_end = possible_options + sizeof(possible_options)/sizeof(PossibleOption) - 2;

struct ModeAbrv {
  char abrv;
  const char * mode;
  const char * desc;
};
static const ModeAbrv mode_abrvs[] = {
  {'e', "mode=email","enter Email mode."},
  {'H', "mode=sgml", "enter Html/Sgml mode."},
  {'t', "mode=tex",  "enter TeX mode."},
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
  EXIT_ON_ERR(options->read_in_settings());

  if (argc == 1) {print_help(); return 0;}

  int i = 1;
  const PossibleOption * o;
  const char           * parm;

  //
  // process command line options by setting the oprepreate options
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
	CERR << "Error: Invalid Option: " << argv[i] << "\n";
	return 1;
      }
      if (o->num_arg == 0) {
	if (parm[0] != '\0') {
	  CERR << "Error: " << String(argv[i], parm - argv[i])
	       << " does not take any parameters." << "\n";
	  return 1;
	}
	i += 1;
      } else { // o->num_arg == 1
	if (parm[0] == '\0') {
	  if (i + 1 == argc) {
	    CERR << "Error: You must specify a parameter for " 
		 << argv[i] << "\n";
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
    CERR << "Error: You must specify an action" << "\n";
    return 1;
  }

  //
  // perform the requisted action
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
  else if (action_str == "dump")
    action = do_dump;
  else {
    CERR << "Error: Unknown Action: " << action_str << "\n";
    return 1;
  }

  if (action != do_other) {
    if (args.empty()) {
      CERR << "Error: Unknown Action: " << action_str << "\n";
      return 1;
    }
    String what_str = args.front();
    args.pop_front();
    if (what_str == "config")
      config();
    else if (what_str == "dicts")
      dicts();
    else {
      CERR << "Error: Unknown Action: " << action_str 
	   << " " << what_str << "\n";
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

  if (args.size() == 0)
    config->write_to_stream(COUT);
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
  DictInfoList * dlist = get_dict_info_list(options);

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

char * trim_wspace (char * str)
{
  unsigned int last = strlen(str) - 1;
  while (isspace(str[0])) {
    ++str;
    --last;
  }
  while (isspace(str[last])) {
    str[last] = '\0';
    --last;
  }
  return str;
}

bool get_word_pair(char * line, char * & w1, char * & w2)
{
  w2 = strchr(line, ',');
  if (!w2) {
    CERR << "ERROR: Invalid Input\n";
    return false;
  }
  *w2 = '\0';
  ++w2;
  w1 = trim_wspace(line);
  w1 = trim_wspace(w2);
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
  EXIT_ON_ERR_SET(new_document_checker(reinterpret_cast<Speller *>(speller),
				       0,0), 
		  StackPtr<DocumentChecker>, checker);
  checker->set_status_fun(status_fun, &print_star);
  return checker.release();
}

void pipe() 
{
  // set up stdin and stdout to be line buffered
  assert(setvbuf(stdin, 0, _IOLBF, 0) == 0); 
  assert(setvbuf(stdout, 0, _IOLBF, 0) == 0);

  bool terse_mode = true;
  bool do_time = options->retrieve_bool("time");
  clock_t start,finish;
  start = clock();

  AspellCanHaveError * ret 
    = new_aspell_speller(reinterpret_cast<AspellConfig *>(options.get()));
  if (aspell_error(ret)) {
    CERR << "Error: " << aspell_error_message(ret) << "\n";
    exit(1);
  }
  AspellSpeller * speller = to_aspell_speller(ret);
  Config * config = reinterpret_cast<Config *>(config);
  if (do_time)
    COUT << "Time to load word list: " 
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
    while (c = getchar(), c != '\n' && c != EOF)
      buf.push_back(static_cast<char>(c));
    buf.push_back('\0');
    if (c == EOF) break;
    line = buf.data();
    ignore = 0;
    switch (line[0]) {
    case '\n':
      continue;
    case '*':
      word = trim_wspace(line + 1);
      aspell_speller_add_to_personal(speller, word, -1);
      break;
    case '&':
      word = trim_wspace(line + 1);
      aspell_speller_add_to_personal
	(speller, 
	 reinterpret_cast<Speller *>(speller)->to_lower(word), -1);
      break;
    case '@':
      word = trim_wspace(line + 1);
      aspell_speller_add_to_session(speller, word, -1);
      break;
    case '#':
      aspell_speller_save_all_word_lists(speller);
      if (aspell_speller_error(speller))
	CERR << "Error: " << aspell_speller_error_message(speller) << "\n";
      break;
    case '+':
      word = trim_wspace(line + 1);
      err = config->replace("mode", word);
      if (err.get_err())
	config->replace("mode", "tex");
      checker.del();
      checker = new_checker(speller, print_star);
      break;
    case '-':
      config->remove("filter");
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
	    if (get_word_pair(line + 3, word, word2))
	      aspell_speller_store_replacement(speller, word, -1, word2, -1);
	    break;
	  }
	  break;
	case 'c':
	  switch (line[3]) {
	  case 's':
	    if (get_word_pair(line + 3, word, word2))
	      BREAK_ON_ERR(err = config->replace(word, word2));
	    break;
	  case 'r':
	    word = trim_wspace(line + 3);
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
      checker->process(line + ignore, strlen(line));
      while (Token token = checker->next_misspelling()) {
	word = line + token.offset;
	word[token.len] = '\0';
	start = clock();
        const AspellWordList * suggestions 
	  = aspell_speller_suggest(speller, word, -1);
	finish = clock();
	if (!aspell_word_list_empty(suggestions)) {
	  COUT << "& " << word 
	       << " " << aspell_word_list_size(suggestions) 
	       << " " << token.offset
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
	  COUT << "\n";
	} else {
	  COUT << "# " << word << " " 
	       << token.offset
	       << "\n";
	}
	if (do_time)
	  cout << "Suggestion Time: " 
	       << (finish-start)/(double)CLOCKS_PER_SEC << endl;
      }
      COUT << "\n";
    }
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
  String backup_name;
  FILE * in = 0;
  FILE * out = 0;
  Mapping mapping;

  if (interactive) {
    if (args.size() == 0) {
      CERR << "Error: You must specify a file name.\n";
      exit(-1);
    }
    
    file_name = args[0];
    backup_name = file_name;

    backup_name += ".bak";
    if (! rename_file(file_name, backup_name) ) {
      CERR << "Error: Could not rename the file \"" << file_name 
	   << "\" to \"" << backup_name << "\".\n";
      exit(-1);
    }

    in = fopen(backup_name.c_str(), "r");
    if (!in) {
      CERR << "Error: Could not open the file \"" << file_name
	   << "\" for reading.\n";
      exit(-1);
    }
    
    out = fopen(file_name.c_str(), "w");
    if (!out) {
      CERR << "Error: Could not open the file \"" << file_name
           << "\" for writing.  File not saved.\n";
      exit(-1);
    }
    
    String m = options->retrieve("keymapping");
    if (m == "aspell")
      mapping.to_aspell();
    else if (m == "ispell")
      mapping.to_ispell();
    else {
      CERR << "Error: Invalid keymapping: " << m << "\n";
      exit(-1);
    }

  } else {
    in = stdin;
  }

  AspellCanHaveError * ret 
    = new_aspell_speller(reinterpret_cast<AspellConfig *>(options.get()));
  if (aspell_error(ret)) {
    CERR << "Error: " << aspell_error_message(ret) << "\n";
    exit(1);
  }
  AspellSpeller * speller = to_aspell_speller(ret);

  state = new CheckerString(speller,in,out,64);
 
  word_choices = new Choices;

  menu_choices = new Choices;
  menu_choices->push_back(Choice(mapping[Ignore],     "Ignore"));
  menu_choices->push_back(Choice(mapping[IgnoreAll],  "Ignore all"));
  menu_choices->push_back(Choice(mapping[Replace],    "Replace"));
  menu_choices->push_back(Choice(mapping[ReplaceAll], "Replace all"));
  menu_choices->push_back(Choice(mapping[Add],        "Add"));
  menu_choices->push_back(Choice(mapping[AddLower],   "Add Lower"));
  menu_choices->push_back(Choice(mapping[Abort],      "Abort"));
  menu_choices->push_back(Choice(mapping[Exit],       "Exit"));

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

      char choice;
      get_choice(choice);
      
      if (choice == '0') choice = '9' + 1;
    
      switch (mapping[choice]) {
      case Exit:
	goto exit_loop;
      case Abort:
	prompt("Are you sure you want to abort? ");
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
	prompt("With: ");
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
	  error("Sorry that is an invalid choice!");
	  goto choice_loop;
	}
      }

    } else { // !interactive
      
      cout << word << "\n";
      
    }
  }
exit_loop:
  {
    aspell_speller_save_all_word_lists(speller);
    state.del(); // to close the file handles
    delete_aspell_speller(speller);
    
    bool keep_backup = options->retrieve_bool("backup");
    if (!keep_backup)
      remove_file(backup_name);
    
    //end_check();
    
    return;
  }
abort_loop:
  {
    state.del(); // to close the file handles
    delete_aspell_speller(speller);

    rename_file(backup_name, file_name);

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
  assert(setvbuf(stdin, 0, _IOLBF, 0) == 0);
  assert(setvbuf(stdout, 0, _IOLBF, 0) == 0);
  
}


///////////////////////////
//
// print_ver
//

void print_ver () {
  COUT << "@(#) International Ispell Version 3.1.20 " 
       << "(but really Aspell " << VERSION << ")" << "\n";
}

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
  printf("  %-27s %s\n", command.c_str(), desc);
}

void print_help () {
  printf(
    "\n"
    "Aspell " VERSION " alpha.  Copyright 2000 by Kevin Atkinson.\n"
    "\n"
    "Usage: aspell [options] <command>\n"
    "\n"
    "<command> is one of:\n"
    "  -?|help          display this help message\n"
    "  -c|check <file>  to check a file\n"
    "  -a|pipe          \"ispell -a\" compatibility mode\n"
    "  -l|list          produce a list of misspelled words from standard input\n"
    "  [dump] config    dumps the current configuration to stdout\n"
    "  config <key>     prints the current value of an option\n"
    "  filter           passes standard input through filters\n"
    "  -v|version       prints a version line\n"
    "\n"
    "[options] is any of the following:\n"
    "\n");
  Enumeration<KeyInfoEnumeration> els = options->possible_elements();
  const KeyInfo * k;
  while (k = els.next(), k) {
    if (k->desc == 0) continue;
    const PossibleOption * o = find_option(k->name);
    print_help_line(o->abrv, 
		    strncmp((o+1)->name, "dont-", 5) == 0 ? (o+1)->abrv : '\0',
		    k->name, k->type, k->desc);
    if (strcmp(k->name, "mode") == 0) {
      for (const ModeAbrv * j = mode_abrvs;
           j != mode_abrvs_end;
           ++j)
      {
        print_help_line(j->abrv, '\0', j->mode, KeyInfoBool, j->desc, true);
      }
    }
  }

}

