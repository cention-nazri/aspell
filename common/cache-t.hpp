#ifndef ACOMMON_CACHE_T__HPP
#define ACOMMON_CACHE_T__HPP

#include "lock.hpp"
#include "cache.hpp"

//#include "iostream.hpp"

namespace acommon {

class GlobalCacheBase
{
public:
  mutable Mutex lock;
protected:
  class List
  {
    Cacheable * first;
  public:
    List() : first(0) {}
    template <class D>
    D * find(const typename D::CacheKey & id, D * = 0) {
      D * cur = static_cast<D *>(first);
      while (cur && !cur->cache_key_eq(id))
        cur = static_cast<D *>(cur->next);
      return cur;
    }
    void add(Cacheable * node) {
      node->next = first;
      node->attached = true;
      first = node;
    }
    void del(Cacheable * d);
  };
  List list;
public:
  void add(Cacheable * n);
  void release(Cacheable * d);
};

template <class D>
class GlobalCache : public GlobalCacheBase
{
public:
  typedef D Data;
  typedef typename Data::CacheKey Key;
public:
  Data * find(const Key & key) {
    Data * dummy;
    // needed due to gcc (< 3.4) bug.
    return list.find(key,dummy);
  }
};

template <class Data>
PosibErr<Data *> get_cache_data(GlobalCache<Data> * cache, 
                                typename Data::CacheConfig * config, 
                                const typename Data::CacheKey & key)
{
  LOCK(&cache->lock);
  Data * n = cache->find(key);
  //CERR << "Getting " << key << "\n";
  if (n) {
    n->copy();
    return n;
  }
  PosibErr<Data *> res = Data::get_new(key, config);
  if (res.has_err()) {
    //CERR << "ERROR\n"; 
    return res;
  }
  n = res.data;
  cache->add(n);
  //CERR << "LOADED FROM DISK\n";
  return n;
}

}

#endif
