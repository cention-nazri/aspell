#ifndef __ACOMMON_CACHE__
#define __ACOMMON_CACHE__

#include "string.hpp"
#include "posib_err.hpp"

namespace acommon {

class GlobalCacheBase;
template <class Data> class GlobalCache;

template <class Data>
PosibErr<Data *> get_cache_data(GlobalCache<Data> *, 
                                typename Data::CacheConfig *, 
                                const typename Data::CacheKey &);

template <class Data>
void release_cache_data(GlobalCache<Data> *, const Data *);
template <class Data>
static inline void release_cache_data(GlobalCache<const Data> * c, const Data * d)
{
  release_cache_data((GlobalCache<Data> *)c,d);
}

class Cacheable
{
public: // but don't use
  Cacheable * next;
  mutable int refcount;
  bool attached;
  void * cache;
  void copy() const;
  Cacheable() : next(0), refcount(0), attached(false), cache(0) {}
  virtual ~Cacheable() {}
};

template <class Data>
class CachePtr
{
  Data * ptr;

public:
  void reset(Data * p) {
    if (ptr) 
      release_cache_data(static_cast<GlobalCache<Data> *>(ptr->cache), ptr);
    ptr = p;
  }
  void copy(Data * p) {p->copy(); reset(p);}
  Data * release() {Data * tmp = ptr; ptr = 0; return tmp;}

  Data & operator*  () const {return *ptr;}
  Data * operator-> () const {return ptr;}
  Data * get()         const {return ptr;}
  operator Data * ()   const {return ptr;}

  CachePtr() : ptr(0) {}
  CachePtr(const CachePtr & other) {ptr = other.ptr; ptr->copy();}
  void operator=(const CachePtr & other) {copy(other.ptr);}
  ~CachePtr() {reset(0);}

  PosibErr<void> setup(GlobalCache<Data> * cache, 
                       typename Data::CacheConfig * config, 
                       const typename Data::CacheKey & key) {
    PosibErr<Data *> pe = get_cache_data(cache, config, key);
    if (pe.has_err()) return pe;
    reset(pe.data);
    return no_err;
  }
};

}

#endif
