#include <time.h>
#include "data.hpp"

using namespace acommon;

namespace aspeller {

  class WritableBaseCode {
  protected:
    String suffix;
    String compatibility_suffix;
    
    time_t cur_file_date;
    
    String compatibility_file_name;
    
    WritableBaseCode(const char * s, const char * cs)
      : suffix(s), compatibility_suffix(cs) {}
    virtual ~WritableBaseCode() {}
    
    virtual PosibErr<void> save(FStream &, ParmString) = 0;
    virtual PosibErr<void> merge(FStream &, ParmString, 
				 Config * config = 0) = 0;
    
    virtual const char * file_name() = 0;
    
    virtual PosibErr<void> set_file_name(ParmString name) = 0;
    virtual PosibErr<void> update_file_info(FStream & f) = 0;
    
    PosibErr<void> save2(FStream &, ParmString);
    PosibErr<void> update(FStream &, ParmString);
    PosibErr<void> save(bool do_update);
    PosibErr<void> update_file_date_info(FStream &);
    PosibErr<void> load(ParmString, Config *);
    PosibErr<void> merge(ParmString);
    PosibErr<void> save_as(ParmString);
  };
  
  template <typename Base>
  class WritableBase : public Base, public WritableBaseCode 
  {
  protected:
    PosibErr<void> set_file_name(ParmString name) {
      return Base::set_file_name(name);
    }
    PosibErr<void> update_file_info(FStream & f) {
      return Base::update_file_info(f);
    }
  public:
    WritableBase(const char * s, const char * cs) 
      : WritableBaseCode(s,cs) {}
    
    const char * file_name() {
      return Base::file_name();
    }
    
    PosibErr<void> load(ParmString f, Config * c, 
			SpellerImpl *, const LocalWordSetInfo *) { 
      return WritableBaseCode::load(f,c);
    };
    PosibErr<void> merge(ParmString f) {
      return WritableBaseCode::merge(f);
    };
    PosibErr<void> save_as(ParmString f) {
      return WritableBaseCode::save_as(f);
    }
    PosibErr<void> synchronize() {
      return WritableBaseCode::save(true);
    }
    PosibErr<void> save_noupdate() {
      return WritableBaseCode::save(false);
    }
  };
}
