#ifndef __ACOMMON_CACHE_T__
#define __ACOMMON_CACHE_T__

#define NDEBUG

#include <assert.h>
#include <vector>

#include "string.hpp"
#include "lock.hpp"
#include "cache.hpp"

#include "iostream.hpp"

namespace acommon {

class GlobalCacheBase
{
public: // but don't use
  mutable Mutex lock;
};

template <class Data>
class GlobalCache : public GlobalCacheBase
{
public:
  typedef typename Data::CacheKey    Key;
  typedef typename Data::CacheConfig Config;
private:
  class List
  {
    Data * first;
  public:
    List() : first(0) {}
    Data * find(const Key & id) {
      Data * cur = first;
      while (cur && !cur->cache_key_eq(id))
        cur = static_cast<Data *>(cur->next);
      return cur;
    }
    void add(Data * node) {
      node->next = first;
      first = node;
    }
    void del(Data * d) {
      assert(d->refcount == 0);
      Cacheable * * cur = (Cacheable * *)(&first);
      while (*cur && *cur != d) cur = &((*cur)->next);
      assert(*cur);
      *cur = (*cur)->next;
    }
  };
  List list;
public:
  PosibErr<Data *> get(const Key & key, Config * config) {
    Lock l(lock);
    //CERR << "Getting " << key << "\n";
    Data * n = list.find(key);
    if (n) {/*CERR << "FOUND IN CACHE\n";*/ goto ret;}
    { PosibErr<Data *> res = Data::get_new(key, config);
      if (!res) {/*CERR << "ERROR\n";*/ return res;}
      n = res.data;}
    list.add(n);
    n->cache = this;
    //CERR << "LOADED FROM DISK\n";
  ret:
    n->refcount++;
    return n;
  }
  void release(Data * d) {
    //CERR << "RELEASE\n";
    Lock l(lock);
    d->refcount--;
    assert(d->refcount >= 0);
    if (d->refcount != 0) return;
    //CERR << "DEL\n";
    list.del(d);
    delete d;
  }
};

template <class Data>
PosibErr<Data *> get_cache_data(GlobalCache<Data> * cache, 
                                typename Data::CacheConfig * config, 
                                const typename Data::CacheKey & key)
{
  return cache->get(key, config);
}

template <class Data>
void release_cache_data(GlobalCache<Data> * cache, const Data * d)
{
  cache->release(const_cast<Data *>(d));
}

}

#endif
