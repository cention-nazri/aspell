// This file is part of The New Aspell
// Copyright (C) 2002 by Kevin Atkinson under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.

#include <string.h>

#include "config.hpp"
#include "errors.hpp"
#include "filter.hpp"

namespace acommon {
  
  extern const ConfigModule * filter_modules_begin;
  extern const ConfigModule * filter_modules_end;
  extern void activate_dynamic_filteroptions(Config *config);

  extern char mode_string[128];
  char mode_error[128] = {"one of "};
  extern const char * filter_modes;

  struct StaticInit {
    StaticInit() {
      strcat(mode_string, " = ");
      strcat(mode_string, filter_modes);
      strcat(mode_error,  filter_modes);
    }
  };

  static const StaticInit static_init;

  class ModeNotifierImpl : public Notifier
  {
  private:
    ModeNotifierImpl();
    ModeNotifierImpl(const ModeNotifierImpl &);
    ModeNotifierImpl & operator= (const ModeNotifierImpl &);
  public:
    Config * config;
    ModeNotifierImpl(Config * c) : config(c) {}
    
    ModeNotifierImpl * clone(Config * c) const {return new ModeNotifierImpl(c);}
    PosibErr<void> item_updated(const KeyInfo * ki, ParmString value) {
      if (strcmp(ki->name, "mode") == 0) {

	String temp = "fm-";
	temp += value;
	PosibErr<String> m = config->retrieve(temp);
	if (m.get_err())
	  return make_err(bad_value, "mode", value, mode_error);

	config->replace("rem-all-filter","");
	unsigned int begin = 0, end = 0;

	do {
	  
	  while (end < m.data.size() && m.data[end] != ',') ++end;
	  String fil = m.data.substr(begin, end - begin);
	  RET_ON_ERR(config->replace("add-filter", fil.c_str()));
	  begin = end + 1;
	  end   = begin;
	  
	} while (end < m.data.size());
	
      }
      return no_err;
    }
  };

  Config * new_config() {
    Config * config = new_basic_config();
    config->set_filter_modules(filter_modules_begin, filter_modules_end);
    config->add_notifier(new ModeNotifierImpl(config));
    activate_dynamic_filteroptions(config);
    return config;
  }

}
