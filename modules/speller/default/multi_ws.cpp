
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

  class MultiWS : public MultiDict
  {
  public:
    PosibErr<void> load(ParmString, const Config &, LocalDictList *, 
                        SpellerImpl *, const LocalDictInfo *);

    Enum * detailed_elements() const;
    unsigned int      size()     const;
    
  public: //but don't use
    typedef std::vector<Value> Wss;
    struct ElementsParms;
  private:
    Wss wss;
  };

  PosibErr<void> MultiWS::load(ParmString fn, 
                               const Config & config, 
                               LocalDictList * new_dicts,
                               SpellerImpl * speller,
                               const LocalDictInfo * li)
  {
    String dir = figure_out_dir("",fn);
    FStream in;
    RET_ON_ERR(in.open(fn, "r"));
    set_file_name(fn);
    bool strip_accents;
    if (config.have("strip-accents"))
      strip_accents = config.retrieve_bool("strip-accents");
    else if (li == 0)
      strip_accents = false;
    else
      strip_accents = li->convert.strip_accents;
    String buf; DataPair d;
    while(getdata_pair(in, d, buf)) 
    {
      if (d.key == "strip-accents") {
	if (config.have("strip-accents")) {
	  // do nothing
	} if (d.value == "true") {
	  strip_accents = true;
	} else if (d.value == "false") {
	  strip_accents = false;
	} else {
	  return make_err(bad_value, "strip-accents", d.value, "true or false").with_file(fn, d.line_num);
	}
      } else if (d.key == "add") {

	LocalDict res;
	res.set(0, config, strip_accents);
        RET_ON_ERR(add_data_set(d.value, config, res, new_dicts, speller, &res, dir));
        RET_ON_ERR(set_check_lang(res.dict->lang()->name(), &config));
	wss.push_back(res);

      } else {
	
	return make_err(unknown_key, d.key).with_file(fn, d.line_num);

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

  MultiDict * new_default_multi_dict() 
  {
    return new MultiWS();
  }

}
