#ifndef ACOMMON_CACHE__HPP
#define ACOMMON_CACHE__HPP

#include "posib_err.hpp"

namespace acommon {

class GlobalCacheBase;
template <class Data> class GlobalCache;

template <class Data>
PosibErr<Data *> get_cache_data(GlobalCache<Data> *, 
                                typename Data::CacheConfig *, 
                                const typename Data::CacheKey &);

class Cacheable;
void release_cache_data(GlobalCacheBase *, const Cacheable *);
static inline void release_cache_data(const GlobalCacheBase * c, const Cacheable * d)
{
  release_cache_data(const_cast<GlobalCacheBase *>(c),d);
}

class Cacheable
{
public: // but don't use
  Cacheable * next;
  mutable int refcount;
  bool attached;
  GlobalCacheBase * cache;
  void copy() const;
  void release() const {release_cache_data(cache,this);}
  Cacheable(GlobalCacheBase * c = 0) : next(0), refcount(1), attached(false), cache(c) {}
  virtual ~Cacheable() {}
};

template <class Data>
class CachePtr
{
  Data * ptr;

public:
  void reset(Data * p) {
    if (ptr) ptr->release();
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
