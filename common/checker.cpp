/* This file is part of The New Aspell
 * Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#include "checker.hpp"
#include "convert.hpp"
#include "speller.hpp"
#include "config.hpp"

namespace acommon {

  Checker::Checker() 
    : callback_(0), last_id(0), span_strings_(false), first(0), last(0)
  {
    separator_.append(FilterChar(0x10, 0));
    separator_.append(FilterChar(0x10, 0));
  }

  Checker::~Checker()
  {
    free_segments();
  }

  void Checker::free_segments(Segment * l)
  {
    while (first && first != l) {
      Segment * next = first->next;
      delete first;
      first = next;
    }
    if (first)
      first->prev = 0;
    else
      last = 0;
  }

  void Checker::init(Speller * speller)
  {
    conv_ = speller->to_internal_;
  }

  const FilterChar SegmentIterator::empty_str[1] = {FilterChar(0,0)};

  void Checker::reset()
  {
    free_segments();
    Segment * seg = new Segment;
    first = seg;
    last = seg;
    i_reset(first);
  }

  Segment * Checker::fill_segment(Segment * seg, 
                                  const char * str, int size, 
                                  void * which,
                                  Filter * filter)
  {
    proc_str_.clear();
    conv_->decode(str, size, proc_str_);
    proc_str_.append(0);
    FilterChar * begin = proc_str_.pbegin();
    FilterChar * end   = proc_str_.pend() - 1;
    if (filter)
      filter->process(begin, end);
    else
      conv_->filter(begin, end);
    SegmentData * buf = new SegmentData;
    conv_->encode(begin, end, *buf);
    if (seg == 0) seg = new Segment;
    seg->begin = buf->pbegin();
    seg->end = buf->pend();
    seg->which = which;
    seg->offset = 0;
    seg->data = buf;
    return seg;
  }

  void Checker::process(const char * str, int size, void * which)
  {
    Segment * seg = fill_segment(0, str, size, which, filter_);
    seg->id = last_id++;
    seg->prev = last;
    last->next = seg;
    last = seg;
    if (!span_strings_) add_separator();
  }

  // precond: at least one segment already in list
  void Checker::add_separator()
  {
    Segment * seg = new Segment;
    seg->begin = separator_.pbegin();
    seg->end = separator_.pend();
    seg->which = last->which;
    seg->offset = 0;
    seg->id = last_id++;
    seg->prev = last;
    last->next = seg;
    last = seg;
  }

  void Checker::replace(const char * str, int size)
  {
    Segment * seg = 0;
    if (token.b.seg == token.e.seg &&
        token.b.pos == token.b.seg->begin && token.e.pos == token.e.seg->end)
    {

      seg = fill_segment(token.b.seg, str, size, token.b.seg->which, 0);

    } else {
      
      seg = fill_segment(0, str, size, token.b.seg->which, 0);
      seg->id = token.b.seg->id;
      
      Segment * prev_seg = token.b.seg;
      Segment * next_seg = (token.b.seg == token.e.seg 
                            ? new Segment(*token.e.seg) : token.e.seg);
      
      // Free any segments between token.b and token, exclusive, as
      // they are no longer needed, and there won't be any refrences
      // to them after this function
      if (token.b.seg != token.e.seg) {
        Segment * tmp = token.b.seg;
        while (tmp->next != token.e.seg) {
          tmp = tmp->next;
          delete tmp->prev;
        }
      }
      
      prev_seg->end = token.b.pos;
      prev_seg->next = seg;

      seg->prev = prev_seg;
      seg->offset = token.begin.offset;
      seg->next = next_seg;
      
      next_seg->begin = token.e.pos;
      next_seg->offset = token.end.offset; 
      next_seg->prev = seg;
      if (next_seg->next) next_seg->next->prev = next_seg;
      else                last = next_seg;
    }
    unsigned tok_width = token.end.offset - token.begin.offset;
    unsigned seg_width = FilterChar::sum(seg->begin, seg->end);
    int diff = seg_width - tok_width;
    Segment * c = seg->next;
    while (c && c->id == seg->id) {
      c->offset += diff;
      c = c->next;
    }

    i_recheck(seg);   
  }

  bool SegmentIterator::adv_seg(Checker * c)
  {
    if (!seg->next) c->need_more(seg);
    seg = seg->next;
    if (seg) {
      pos = seg->begin;
      offset = seg->offset;
      return true;
    } else {
      pos = empty_str;
      offset = 0;
      return false;
    }
  }

}

