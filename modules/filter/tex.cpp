// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include "asc_ctype.hpp"
#include "config.hpp"
#include "string.hpp"
#include "indiv_filter.hpp"
#include "mutable_container.hpp"
#include "string_map.hpp"
#include "clone_ptr-t.hpp"
#include "vector.hpp"
#include "errors.hpp"

namespace acommon {


  class TexFilter : public IndividualFilter 
  {
  private:
    enum InWhat {Name, Opt, Parm, Other, Swallow};
    struct Command {
      InWhat in_what;
      String name;
      const char * do_check;
      Command() {}
      Command(InWhat w) : in_what(w), do_check("P") {}
    };

    bool in_comment;
    bool prev_backslash;
    Vector<Command> stack;

    class CommandsIn : public MutableContainer {
      StringMap * real_;
    public:
      CommandsIn(StringMap * r) : real_(r) {}
      PosibErr<bool> add(ParmString to_add);
      PosibErr<bool> remove(ParmString to_rem);
      PosibErr<void> clear();
    };
    
    ClonePtr<StringMap> commands;
    bool check_comments;
    
    inline void push_command(InWhat);
    inline void pop_command();

    bool end_option(char u, char l);

    inline bool process_char(char c);
    
  public:
    PosibErr<void> setup(Config *);
    void reset();
    void process(char *, unsigned int size);
  };

  //
  //
  //

  inline void TexFilter::push_command(InWhat w) {
    stack.push_back(Command(w));
  }

  inline void TexFilter::pop_command() {
    stack.pop_back();
    if (stack.empty())
      push_command(Parm);
  }

  //
  //
  //

  PosibErr<void> TexFilter::setup(Config * opts) 
  {
    name_ = "tex";
    order_num_ = 0.35;
    commands.reset(new_string_map());
    CommandsIn commands_in(commands);
    RET_ON_ERR(opts->retrieve_list("tex-command", &commands_in));
    check_comments = opts->retrieve_bool("tex-check-comments");
    reset();
    return no_err;
  }
  
  void TexFilter::reset() 
  {
    in_comment = false;
    prev_backslash = false;
    stack.resize(0);
    push_command(Parm);
  }

#  define top stack.back()

  // yes this should be inlined, it is only called once
  inline bool TexFilter::process_char(char c) 
  {
    // deal with comments
    if (c == '%' && !prev_backslash) in_comment = true;
    if (in_comment && c == '\n')     in_comment = false;
    if (in_comment)                  return !check_comments;

    if (top.in_what == Name) {
      if (asc_isalpha(c)) {

	top.name += c;
	return true;

      } else {

	if (top.name.empty() && (c == '@')) {
	  top.name += c;
	  return true;
	}
	  
	top.in_what = Other;

	if (top.name.empty()) {
	  top.name.clear();
	  top.name += c;
	  top.do_check = commands->lookup(top.name.c_str());
	  if (top.do_check == 0) top.do_check = "";
	  return !asc_isspace(c);
	}

	top.do_check = commands->lookup(top.name.c_str());
	if (top.do_check == 0) top.do_check = "";

	if (asc_isspace(c)) { // swallow extra spaces
	  top.in_what = Swallow;
	  return true;
	} else if (c == '*') { // ignore * at end of commands
	  return true;
	}
	
	// continue o...
      }

    } else if (top.in_what == Swallow) {

      if (asc_isspace(c))
	return true;
      else
	top.in_what = Other;
    }

    if (c == '{')
      while (*top.do_check == 'O' || *top.do_check == 'o') 
	++top.do_check;

    if (*top.do_check == '\0')
      pop_command();

    if (c == '{') {

      if (top.in_what == Parm || top.in_what == Opt || top.do_check == '\0')
	push_command(Parm);

      top.in_what = Parm;
      return true;
    }

    if (top.in_what == Other) {

      if (c == '[') {

	top.in_what = Opt;
	return true;

      } else if (isspace(c)) {

	return true;

      } else {
	
	pop_command();

      }

    } 

    if (c == '\\') {
      push_command(Name);
      return true;
    }

    if (top.in_what == Parm) {

      if (c == '}')
	return end_option('P','p');
      else
	return *top.do_check == 'p';

    } else if (top.in_what == Opt) {

      if (c == ']')
	return end_option('O', 'o');
      else
	return *top.do_check == 'o';

    }

    return false;
  }

  void TexFilter::process(char * str, unsigned int size)
  {
    char * i = str;
    char * stop = str + size;
    while (i != stop) {
      if (process_char(*i))
	*i = ' ';
      ++i;
    }
  }

  bool TexFilter::end_option(char u, char l) {
    top.in_what = Other;
    if (*top.do_check == u || *top.do_check == l)
      ++top.do_check;
    return true;
  }

  //
  // TexFilter::CommandsIn
  //

  PosibErr<bool> TexFilter::CommandsIn::add(ParmString value) {
    int p1 = 0;
    while (!asc_isspace(value[p1])) {
      if (value[p1] == '\0') 
	return make_err(bad_value, value,"","a string of o,O,p, or P");
      ++p1;
    }
    int p2 = p1 + 1;
    while (asc_isspace(value[p2])) {
      if (value[p2] == '\0') 
	return make_err(bad_value, value,"","a string of o,O,p, or P");
      ++p2;
    }
    String t1; t1.assign(value,p1);
    String t2; t2.assign(value+p2);
    return real_->replace(t1, t2);
  }
  
  PosibErr<bool> TexFilter::CommandsIn::remove(ParmString value) {
    int p1 = 0;
    while (!asc_isspace(value[p1]) && value[p1] != '\0') ++p1;
    String temp; temp.assign(value,p1);
    return real_->remove(temp);
  }
  
  PosibErr<void> TexFilter::CommandsIn::clear() {
    return real_->clear();
  }

  //
  //
  //

  IndividualFilter * new_tex_filter() 
  {
    return new TexFilter();
  }

  static const KeyInfo tex_options[] = {
    {"tex-command", KeyInfoList, 
       // counters
       "addtocounter pp,"
       "addtolength pp,"
       "alpha p,"
       "arabic p,"
       "fnsymbol p,"
       "roman p,"
       "stepcounter p,"
       "setcounter pp,"
       "usecounter p,"
       "value p,"
       "newcounter po,"
       "refstepcounter p,"
       // cross ref
       "label p,"
       "pageref p,"
       "ref p,"
       // Definitions
       "newcommand poOP,"
       "renewcommand poOP,"
       "newenvironment poOPP,"
       "renewenvironment poOPP,"
       "newtheorem poPo,"
       "newfont pp,"
       // Document Classes
       "documentclass op,"
       "usepackage op,"
       // Environments
       "begin po,"
       "end p,"
       // Lengths
       "setlength pp,"
       "addtolength pp,"
       "settowidth pp,"
       "settodepth pp,"
       "settoheight pp,"
       // Line & Page Breaking
       "enlargethispage p,"
       "hyphenation p,"
       // Page Styles
       "pagenumbering p,"
       "pagestyle p,"
       // Spaces & Boxes
       "addvspace p,"
       "framebox ooP,"
       "hspace p,"
       "vspace p,"
       "makebox ooP,"
       "parbox ooopP,"
       "raisebox pooP,"
       "rule opp,"
       "sbox pO,"
       "savebox pooP,"
       "usebox p,"
       // Splitting the Input
       "include p,"
       "includeonly p,"
       "input p,"
       // Table of Contents
       "addcontentsline ppP,"
       "addtocontents pP,"
       // Typefaces
       "fontencoding p,"
       "fontfamily p,"
       "fontseries p,"
       "fontshape p,"
       "fontsize pp,"
       "usefont pppp,"
       // Misc
       "documentstyle op,"
       "cite p,"
       "nocite p,"
       "psfig p,"
       "selectlanguage p,"
       "includegraphics op,"
       "bibitem op,"
       // Geometry Package
       "geometry p,"
       ,"TeX commands"},
      {"tex-check-comments", KeyInfoBool, "false",
	 "check TeX comments"}
  };
  const KeyInfo * tex_options_begin = tex_options;
  const KeyInfo * tex_options_end = tex_options + 2;
}
