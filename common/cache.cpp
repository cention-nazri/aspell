#include <assert.h>

#include "cache-t.hpp"

namespace acommon {

void Cacheable::copy() const
{
  //CERR << "COPY\n";
  LOCK(&cache->lock);
  refcount++;
}

void GlobalCacheBase::List::del(Cacheable * d)
{
  Cacheable * * cur = &first;
  while (*cur && *cur != d) cur = &((*cur)->next);
  assert(*cur);
  *cur = (*cur)->next;
  d->attached = false;
}
  
void GlobalCacheBase::add(Cacheable * n) {
  assert(n->refcount > 0);
  list.add(n);
  n->cache = this;
}

void GlobalCacheBase::release(Cacheable * d) {
  //CERR << "RELEASE\n";
  LOCK(&lock);
  d->refcount--;
  assert(d->refcount >= 0);
  if (d->refcount != 0) return;
  //CERR << "DEL\n";
  if (d->attached)
    list.del(d);
  delete d;
}

void release_cache_data(GlobalCacheBase * cache, const Cacheable * d)
{
  cache->release(const_cast<Cacheable *>(d));
}

#if 0

struct CacheableImpl : public Cacheable
{
  class CacheConfig;
  typedef String CacheKey;
  bool cache_key_eq(const CacheKey &);
  static PosibErr<CacheableImpl *> get_new(const CacheKey &, CacheConfig *);
};

template
PosibErr<CacheableImpl *> get_cache_data(GlobalCache<CacheableImpl> *, 
                                         CacheableImpl::CacheConfig *, 
                                         const CacheableImpl::CacheKey &);

#endif

}
