// Aspell test program
// Copyright 2000 by Kevin Atkinson under the terms of the LGPL

#include <deque>

#include "settings.h"

#include "fstream.hpp"
#include "iostream.hpp"
#include "language.hpp"
#include "manager_impl.hpp"
#include "string.hpp"
#include "data.hpp"
#include "posib_err.hpp"
#include "config.hpp"
#include "file_util.hpp"
#include "stack_ptr.hpp"
#include "errors.hpp"

using namespace pcommon;
using namespace aspell;

// action functions declarations

void print_ver();
void master();
void personal();
void repl();
void print_help();
void config();
void soundslike();

#define EXIT_ON_ERR(command) \
  do{PosibErrBase pe = command;\
  if(pe.has_err()){CERR<<"Error: "<< pe.get_err()->mesg << "\n"; exit(1);}\
  } while(false)
#define EXIT_ON_ERR_SET(command, type, var)\
  type var;\
  do{PosibErr<type> pe = command;\
  if(pe.has_err()){CERR<<"Error: "<< pe.get_err()->mesg << "\n"; exit(1);}\
  else {var=pe;}\
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

const PossibleOption possible_options[] = {
  COMMAND("version",   'v', 0),
  COMMAND("help",      '?', 0),
  COMMAND("soundslike",'\0', 0),
  COMMAND("config",    '\0', 0),
  COMMAND("filter",    '\0', 0),

  COMMAND("dump",   '\0', 1),
  COMMAND("create", '\0', 1),
  COMMAND("merge",  '\0', 1),

  {"",'\0'}, {"",'\0'}
};

const PossibleOption * possible_options_end = possible_options + sizeof(possible_options)/sizeof(PossibleOption) - 2;

struct ModeAbrv {
  char abrv;
  const char * mode;
  const char * desc;
};
static const ModeAbrv mode_abrvs[] = {};
static const ModeAbrv *  mode_abrvs_end = mode_abrvs + 0;

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
  //options->set_extra(extra, extra+sizeof(extra)/sizeof(KeyInfo));

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
	if (o == possible_options_end) /* try FIXME */ {
	  option_name.assign(argv[i] + 2, 0, c - argv[i] - 2);
	  const char * base_name = Config::base_name(option_name.c_str());
	  PosibErr<const KeyInfo *> ki = options->keyinfo(base_name);
          if (!ki.has_err(unknown_key)) {
            other_opt.name    = option_name.c_str();
            other_opt.num_arg = ki.data->type == KeyInfoBool ? 0 : 1;
            o = &other_opt;
          }
	} /* catch (UnknownKey) {} */
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
	  options->replace(o->name, parm);
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
  else if (action_str == "soundslike")
    soundslike();
  else if (action_str == "dump")
    action = do_dump;
  else if (action_str == "create")
    action = do_create;
  else if (action_str == "merge")
    action = do_merge;
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
    else if (what_str == "master")
      master();
    else if (what_str == "personal")
      personal();
    else if (what_str == "repl")
      repl();
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

void config () {
  StackPtr<Config> config(new_config());
  EXIT_ON_ERR(config->read_in_settings(options));
  config->write_to_stream(stdout);
}

///////////////////////////
//
// master
//

class IstreamVirEmulation : public StringEmulation {
  FStream * in;
  String data;
public:
  IstreamVirEmulation(FStream & i) : in(&i) {}
  IstreamVirEmulation * clone() const {
    return new IstreamVirEmulation(*this);
  }
  void assign (const StringEmulation * other) {
    *this = *static_cast<const IstreamVirEmulation *>(other);
  }
  Value next() {
    *in >> data;
    if (!*in) return 0;
    else return data.c_str();
  }
  bool at_end() const {return *in;}
};

void dump (LocalWordSet lws) 
{
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
  if (args.size() != 0) {
    options->replace("master", args[0].c_str());
  }

  StackPtr<Config> config(new_config());
  EXIT_ON_ERR(config->read_in_settings(options));

  if (action == do_create) {
    
    EXIT_ON_ERR(create_default_readonly_word_set
                (new IstreamVirEmulation(CIN),
                 *config));

  } else if (action == do_merge) {
    
    CERR << "Can't merge a master word list yet.  Sorry\n";
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
  if (args.size() != 0) {
    EXIT_ON_ERR(options->replace("personal", args[0].c_str()));
  }
  options->replace("module", "aspell");
  if (action == do_create || action == do_merge) {
    abort(); // FIXME
#if 0
    StackPtr<Manager> manager(new_manager(options));

    if (action == do_create) {
      if (file_exists(manager->config()->retrieve("personal-path"))) {
        CERR << "Sorry I won't overwrite \"" 
             << manager->config()->retrieve("personal-path") << "\"" << "\n";
        exit (1);
      }
      manager->personal_word_list().data->clear();
    }

    String word;
    while (CIN >> word) 
      manager->add_to_personal(word);

    manager->save_all_word_lists();
#endif

  } else { // action == do_dump

    StackPtr<Config> config(new_config());
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

  if (args.size() != 0) {
    options->replace("repl", args[0].c_str());
  }

  if (action == do_create || action == do_merge) {
    abort(); //fixme
#if 0
    ManagerImpl manager(options);

    if (action == do_create) {
      if (file_exists(manager->config()->retrieve("repl-path"))) {
        CERR << "Sorry I won't overwrite \"" 
             << manager->config()->retrieve("repl-path") << "\"" << "\n";
        exit (1);
      }
      manager->personal_repl().clear();
    }
    
    try {
      String word,repl;

      while (true) {
	get_word_pair(word,repl,':');
	EXIT_ON_ERR(manager->store_repl(word,repl,false));
      }

    } catch (bad_cin) {}

    EXIT_ON_ERR(manager->personal_repl().synchronize());
#endif
  } else if (action == do_dump) {

    StackPtr<Config> config(new_config());
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
  Language lang;
  EXIT_ON_ERR(lang.setup("",options));
  String word;
  while (CIN >> word) {
    COUT << word << '\t' << lang.to_soundslike(word) << "\n";
  } 
}

///////////////////////////
//
// print_ver
//

void print_ver () {
  COUT << "Aspell Util version " << VERSION << " alpha" << "\n";
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
  COUT << "  " /* << setw(27) FIXME */ << command.c_str() << " " << desc << "\n";
}

void print_help () {
  /* COUT.setf(ios::left); FIXME */
  COUT << 
    "\n"
    "Aspell " VERSION " alpha.  Copyright 2000 by Kevin Atkinson.\n"
    "\n"
    "Usage: aspell [options] <command>\n"
    "\n"
    "<command> is one of:\n"
    "  -?|help          display this help message\n"
    "  [dump] config    dumps the current configuration to stdout\n"
    "  soundslike       returns the soundslike equivalent for each word entered\n"
    "  -v|version       prints a version line\n"
    "  dump|create|merge master|personal|repl [word list]\n"
    "    dumps, creates or merges a master, personal, or replacement word list.\n"
    "\n"
    "[options] is any of the following:\n"
    "\n";
  Emulation<KeyInfoEmulation> els = options->possible_elements();
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

