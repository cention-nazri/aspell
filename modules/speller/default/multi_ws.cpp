
#include <vector>

#include "config.hpp"
#include "data.hpp"
#include "file_util.hpp"
#include "file_util.hpp"
#include "fstream.hpp"
#include "getdata.hpp"
#include "string.hpp"
#include "parm_string.hpp"
#include "errors.hpp"

namespace aspeller {

  class MultiWS : public BasicMultiSet
  {
  public:
    PosibErr<void> load(ParmString, Config *, SpellerImpl *, const LocalWordSetInfo * li);

    Enum * detailed_elements() const;
    unsigned int      size()     const;
    
  public: //but don't use
    typedef std::vector<Value> Wss;
    struct ElementsParms;
  private:
    Wss wss;
  };
  
  PosibErr<void> MultiWS::load(ParmString fn, 
			       Config * config, SpellerImpl * speller, 
			       const LocalWordSetInfo * li)
  {
    String dir = figure_out_dir("",fn);
    FStream in;
    RET_ON_ERR(in.open(fn, "r"));
    set_file_name(fn);
    bool strip_accents;
    if (config->have("strip-accents"))
      strip_accents = config->retrieve_bool("strip-accents");
    else if (li == 0)
      strip_accents = false;
    else
      strip_accents = li->convert.strip_accents;
    char buf[256]; DataPair d;
    while(getdata_pair(in, d, buf, 256)) 
    {
      if (d.key == "strip-accents") {
	if (config->have("strip-accents")) {
	  // do nothing
	} if (d.value == "true") {
	  strip_accents = true;
	} else if (d.value == "false") {
	  strip_accents = false;
	} else {
	  return make_err(bad_value, "strip-accents", d.value, "true or false").with_file(fn);
	}
      } else if (d.key == "add") {
	LocalWordSet ws;
	ws.local_info.set(0, config, strip_accents);
        RET_ON_ERR_SET(add_data_set(d.value, *config, speller, &ws.local_info, dir),LoadableDataSet  *,wstemp);
        ws.word_set=wstemp;
        RET_ON_ERR(set_check_lang(ws.word_set->lang()->name(), config));
	ws.local_info.set_language(ws.word_set->lang());
	wss.push_back(ws);

      } else {
	
	return make_err(unknown_key, d.key).with_file(fn);

      }
    }

    return no_err;
  }

  struct MultiWS::ElementsParms
  {
    typedef Wss::value_type     Value;
    typedef Wss::const_iterator Iterator;
    Iterator end;
    ElementsParms(Iterator e) : end(e) {}
    bool endf(Iterator i)   const {return i == end;}
    Value end_state()       const {return Value();}
    Value deref(Iterator i) const {return *i;}
  };

  MultiWS::Enum * MultiWS::detailed_elements() const
  {
    return new MakeEnumeration<ElementsParms>(wss.begin(), wss.end());
  }
  
  unsigned int MultiWS::size() const 
  {
    return wss.size();
  }

  BasicMultiSet * new_default_multi_word_set() 
  {
    return new MultiWS();
  }

}
