
#include "cache-t.hpp"

#ifdef USE_FILE_INO
#  include <sys/types.h>
#  include <sys/stat.h>
#  include <unistd.h>
#endif

namespace acommon {

void Cacheable::copy() const
{
  //CERR << "COPY\n";
  GlobalCacheBase * c = static_cast<GlobalCacheBase *>(cache);
  c->lock.lock();
  refcount++;
  c->lock.unlock();
}

struct CacheableImpl : public Cacheable
{
  class CacheConfig;
  typedef String CacheKey;
  bool cache_key_eq(const CacheKey &);
  static PosibErr<CacheableImpl *> get_new(const CacheKey &, CacheConfig *);
};


#if 0

template
PosibErr<CacheableImpl *> get_cache_data(GlobalCache<CacheableImpl> *, 
                                         CacheableImpl::CacheConfig *, 
                                         const CacheableImpl::CacheKey &);

template
void release_cache_data(GlobalCache<CacheableImpl> *, const CacheableImpl *);

#endif

}
