
#include "config.hpp"
#include "data.hpp"
#include "file_util.hpp"
#include "file_util.hpp"
#include "fstream.hpp"
#include "getdata.hpp"
#include "string.hpp"
#include "parm_string.hpp"
#include "errors.hpp"
#include "vector.hpp"

namespace {

  using namespace acommon;
  using namespace aspeller;

  typedef Vector<LocalDict> Wss;

  class MultiDictImpl : public Dictionary
  {
  public:
    MultiDictImpl() : Dictionary(multi_dict, "MultiDictImpl") {}
    PosibErr<void> load(ParmString, Config &, LocalDictList *, 
                        SpellerImpl *, const LocalDictInfo *);
    DictsEnumeration * dictionaries() const;
  private:
    Wss wss;
  };

  PosibErr<void> MultiDictImpl::load(ParmString fn, 
                                     Config & config, 
                                     LocalDictList * new_dicts,
                                     SpellerImpl * speller,
                                     const LocalDictInfo * li)
  {
    String dir = figure_out_dir("",fn);
    FStream in;
    RET_ON_ERR(in.open(fn, "r"));
    set_file_name(fn);
    String buf; DataPair d;
    while(getdata_pair(in, d, buf)) 
    {
      if (d.key == "add") {

	LocalDict res;
	res.set(0, config);
        RET_ON_ERR(add_data_set(d.value, config, res, new_dicts, speller, &res, dir));
        RET_ON_ERR(set_check_lang(res.dict->lang()->name(), config));
	wss.push_back(res);

      } else {
	
	return make_err(unknown_key, d.key).with_file(fn, d.line_num);

      }
    }

    return no_err;
  }

  struct Parms
  {
    typedef const LocalDict * Value;
    typedef Wss::const_iterator Iterator;
    Iterator end;
    Parms(Iterator e) : end(e) {}
    bool endf(Iterator i)   const {return i == end;}
    Value end_state()       const {return 0;}
    Value deref(Iterator i) const {return &*i;}
  };

  DictsEnumeration * MultiDictImpl::dictionaries() const
  {
    return new MakeEnumeration<Parms>(wss.begin(), wss.end());
  }
}

namespace aspeller {

  MultiDict * new_default_multi_dict() 
  {
    return new MultiDictImpl();
  }

}
