// This file is part of The New Aspell
// Copyright (C) 2004 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include "convert_filter.hpp"
#include "config.hpp"
#include "indiv_filter.hpp"
#include "fstream.hpp"
#include "convert_impl.hpp"
#include "cache-t.hpp"
#include "getdata.hpp"
#include "file_data_util.hpp"

#include "gettext.h"

namespace {

  using namespace acommon;

  //////////////////////////////////////////////////////////////////////
  //
  // UniConv
  //

  struct UniConvEntry
  {
    typedef Uni16 From;
    Uni16 from;
    typedef Uni16 To;
    const Uni16 * to;
    static const From from_non_char = (From)(-1);
    static const To   to_non_char   = (To)  (-1);
    static const unsigned max_to = UINT_MAX;
    void * sub_table;
    void set_to_to_non_char();
  }
#ifdef __GNUC__    
    __attribute__ ((aligned (sizeof(void *) * 4)))
#endif
  ;

  static const Uni16 uni_conv_empty[2] = {0, UniConvEntry::to_non_char};
  static const Uni16 uni_conv_non_char_str[2] = {UniConvEntry::to_non_char, 0};

  inline void UniConvEntry::set_to_to_non_char() 
  {
    to = uni_conv_non_char_str;
  }

  typedef NormTable<UniConvEntry> UniConvTable;

  struct UniConvTables : public Cacheable {
    typedef const Config CacheConfig;
    typedef const char * CacheKey;
    String key;
    bool cache_key_eq(const char * l) const  {return key == l;}
    static PosibErr<UniConvTables *> get_new(const String &, const Config *);
    struct FromSingle {
      const char * name;
      UniConvTable * table;
    };
    UniConvTable * to_single;
    Vector<FromSingle> from_single;
    ObjStack data;
    ~UniConvTables();
  };

  static GlobalCache<UniConvTables> uni_conv_tables_cache("uni_conv_tables");

  //////////////////////////////////////////////////////////////////////
  //
  // read in uni conv tables
  //

  struct UniConv {
    const Uni16 * from;
    const Uni16 * to;
    UniConv(const Uni16 * f, const Uni16 * t)
      : from(f), to(t) {}
  };

  static inline bool operator< (const UniConv & X, const UniConv & Y)
  {
    const Uni16 * x = X.from;
    const Uni16 * y = Y.from;
    while (*x == *y && *x) ++x, ++y;
    return *x < *y;
  }
  
  struct WorkingFrom {
    const char * name;
    Vector<UniConv> data;
  };

  struct TableFromSortedList {
  private:
    Vector<UniConv>::const_iterator table_begin, i, begin, end;
    unsigned offset;
    void operator=(const TableFromSortedList &);
  public:
    typedef UniConvEntry T;
    TableFromSortedList(Vector<UniConv> & l) 
      : begin(l.begin()), end(l.end()), offset(0) {}
    TableFromSortedList(const TableFromSortedList & other) {}
    int size; // only valid after init is called
    // "cur" and "have_sub_table" will be updated on each call to get_next()
    T * cur;
    bool have_sub_table; // if a sub_table exits get_sub_table MUST be called
    PosibErr<void> init() // sets up the table for input and sets size
    {
      size = 1;
      i = begin;
      Uni16 prev_char = i->from[offset];
      for (; i != end; ++i)
        if (prev_char != i->from[offset]) {++size; prev_char = i->from[offset];}
      i = begin;
      return no_err;
    }
    PosibErr<bool> get_next() // fills in next entry pointed to by
                              // cur and sets have_sub_table
    {
      if (i == end) return false;
      cur->from = i->from[offset];
      if (!i->from[offset + 1]) 
        cur->to = i->to;
      else
        cur->to = uni_conv_empty;
      Uni16 prev_char = i->from[offset];
      table_begin = i->from[offset + 1] ? i : i + 1;
      while (i != end && i->from[offset] == prev_char) ++i;
      have_sub_table = i - table_begin > 0;
      return true;
    }
    void get_sub_table(TableFromSortedList & d) 
    {
      d.begin = table_begin;
      d.end   = i;
      d.offset = offset + 1;
    }
  };

  struct Uni16Conv {
    StackPtr<Convert> conv;
    CharVector tbuf;
    ConvertBuffer cbuf;
    Uni16 * operator() (const MutableString & str, ObjStack & fbuf) {
      unescape(str);
      tbuf.clear();
      conv->convert(str.str, str.size, tbuf, cbuf);
      for (unsigned i = 0; i != sizeof(Uni16); ++i) tbuf.append('\0');
      Uni16 * res = (Uni16 *)fbuf.alloc_top(tbuf.size());
      memcpy(res, tbuf.data(), tbuf.size());
      return res;
    }
  };

  static PosibErrBase 
  table_error(ParmStr file_name, const DataPair & d, int cols)
  {
    char cols_str[4];
    sprintf(cols_str, "%d", cols);
    return make_err(invalid_table_entry, cols_str).with_file(file_name, d.line_num);
  }
      
  PosibErr<UniConvTables *> 
  UniConvTables::get_new(const String & name, const Config * c)
  {
    String dir1,dir2,file_name;
    fill_data_dir(c, dir1, dir2);
    find_file(file_name,dir1,dir2,name,".conv");

    FStream in;
    RET_ON_ERR(in.open(file_name, "r"));

    Uni16Conv conv;
    conv.conv = new_convert(*c, "utf-8", "ucs-2", NormNone);
    UniConvTables * t = new UniConvTables;
    Vector<UniConv> working_to;
    Vector<WorkingFrom> working_from;
    working_from.resize(1);
    int num_cols = 2;
    working_from[0].name = t->data.dup_bottom("multi");
    String buf;
    DataPair d;
    while (getdata_pair(in, d, buf)) {
      to_lower(d.key);
      if (d.key == "table")
      {
        while (getdata_pair(in, d, buf)) {
          if (d.value.empty()) {
            return table_error(file_name, d, num_cols);
            abort(); // FIXME
          } else {
            const Uni16 * single = (d.key != "." 
                                    ? conv(d.key, t->data) 
                                    : uni_conv_empty);
            for (unsigned i = 0; i != working_from.size(); ++i) {
              bool res = split(d);
              if (!res) return table_error(file_name, d, num_cols);
              Uni16 * multi  = conv(d.key, t->data);
              working_to.push_back(UniConv(multi, single));
              if (single[0])
                working_from[i].data.push_back(UniConv(single, multi));
            }
            while (split(d)) {
              Uni16 * multi  = conv(d.key, t->data);
              working_to.push_back(UniConv(multi, single));
            }
          }
        }
      } else if (d.key == "name") {
        // ignored for now
      } else {
        return make_err(unknown_key, d.key).with_file(file_name, d.line_num);
      }
    }
    {
      std::sort(working_to.begin(), working_to.end());
      TableFromSortedList in0(working_to);
      t->to_single = create_norm_table<UniConvEntry>(in0);
    }
    t->from_single.resize( working_from.size());
    for (unsigned i = 0; i != working_from.size(); ++i) 
    {
      t->from_single[i].name = working_from[i].name;
      std::sort(working_from[i].data.begin(), working_from[i].data.end());
      TableFromSortedList in0(working_from[i].data);
      t->from_single[i].table = create_norm_table<UniConvEntry>(in0);
    }
    return t;
  }

  UniConvTables::~UniConvTables()
  {
    if (to_single)
      free_norm_table(to_single);
    for (Vector<FromSingle>::iterator i = from_single.begin();
         i != from_single.end();
         ++i)
      free_norm_table(i->table);
  }

  //////////////////////////////////////////////////////////////////////
  //
  // convert filter
  //

  class ConvertFilter : public IndividualFilter
  {
  public:
    CachePtr<UniConvTables> tables;
    bool decoder;
    ConvertFilterParms parms;
    typedef UniConvEntry E;
    NormTable<E> * data;
    FilterCharVector buf;
    PosibErr<bool> setup(Config *);
    void reset() {}
    void process(FilterChar * & start, FilterChar * & stop);
    ConvertFilter(bool decoder0, // otherwise decoder
                  const ConvertFilterParms & parms0)
      : decoder(decoder0), parms(parms0) {}
  };

  PosibErr<bool> ConvertFilter::setup(Config * c)
  {
    name_ = parms.name;
    if (parms.order_num == -1)
      parms.order_num = decoder ? 0.10 : 0.90;
    order_num_ =  parms.order_num;
    if (parms.file.empty()) {
      String key = "f-"; key += parms.name; key += "-file";
      parms.file = c->retrieve(key);
      if (parms.file.empty()) return make_err(empty_value, parms.name + "-file");
    }
    if (!decoder && parms.form.empty()) {
      String key = "f-"; key += parms.name; key += "-form";
      parms.form = c->retrieve(key);
    }
    RET_ON_ERR(acommon::setup(tables, &uni_conv_tables_cache, c, parms.file.str()));
    if (decoder) {
      data = tables->to_single;
    } else {
      if (parms.form == "single") {
        return false;
      } else if (parms.form == "multi") {
        data = tables->from_single[0].table;
      } else {
        String key = parms.name + "-form";
        return make_err(bad_value, key, parms.form, _("a valid form"));
      }
    }
    return true;
  }
  
  void ConvertFilter::process(FilterChar * & start, FilterChar * & stop)
  {
    buf.clear();
    const FilterChar * cur = start;
    while (cur != stop) {
      NormLookupRet<E, FilterChar> res = norm_lookup<E>(data, cur, stop, 0, cur);
      int w = res.last - cur + 1;
      if (res.to == 0) {
        assert(w == 1);
        buf.append(*cur);
      } else if (res.to[0]) {
        buf.append(res.to[0], w);
        for (unsigned i = 1; res.to[i]; ++i)
          buf.append(res.to[i], 0);
      } else if (!buf.empty()) {
        buf.back().width += w;
      } else {
        buf.append(' ', w);
      }
      cur+= w;
    }
    buf.append(0);
    start = buf.pbegin();
    stop  = buf.pend() - 1;
  }
}

namespace acommon {

  IndividualFilter * new_convert_filter(bool d, const ConvertFilterParms & p)
  {
    return new ConvertFilter(d, p);
  }
  
}
