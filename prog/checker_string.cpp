// This file is part of The New Aspell
// Copyright (C) 2002 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

#include "checker_string.hpp"
#include "speller.hpp"
#include "document_checker.hpp"
#include "copy_ptr-t.hpp"

static int get_line(FILE * in, CharVector & d)
{
  d.resize(0);
  int i;
  while ((i = getc(in)), i != EOF)
  {
    d.push_back(static_cast<char>(i));
    if (i == '\n') break;
  }
  return d.size();
}

CheckerString::CheckerString(Speller * speller, 
			     FILE * in, FILE * out, 
			     int num_lines)
  : in_(in), out_(out), speller_(speller)
{
  lines_.reserve(num_lines + 1);
  for (; num_lines > 0; --num_lines)
  {
    lines_.resize(lines_.size() + 1);
    int s = get_line(in_, lines_.back());
    if (s == 0) break;
  }
  if (lines_.back().size() != 0)
    lines_.resize(lines_.size() + 1);

  end_ = lines_.end() - 1;
  cur_line_ = lines_.begin();
  diff_ = 0;
  has_repl_ = false;

  checker_.reset(new_document_checker(speller, 0, 0));
  checker_->process(cur_line_->data(), cur_line_->size());
}

CheckerString::~CheckerString()
{
  for (cur_line_ = first_line(); !off_end(cur_line_); next_line(cur_line_))
  {
    fwrite(cur_line_->data(), cur_line_->size(), 1, out_);
    cur_line_->resize(0);
  }
  if (in_ != stdin)
    fclose(in_);
  if (out_ != stdout && out_ != stdout)
    fclose(out_);
}

bool CheckerString::read_next_line()
{
  if (feof(in_)) return false;
  Lines::iterator next = end_;
  inc(next);
  if (next == cur_line_) return false;
  int s = get_line(in_, *end_);
  if (s == 0) return false;
  end_ = next;
  if (end_->size() > 0)
    fwrite(end_->data(), end_->size(), 1, out_);
  end_->resize(0);
  return true;
}

bool CheckerString::next_misspelling()
{
  if (off_end(cur_line_)) return false;
  if (has_repl_) {
    has_repl_ = false;
    CharVector word;
    bool correct = speller_->check(get_word(word));
    if (!correct)
      return true;
    diff_ += word_size_ - tok_.len;
    word_size_ = 0;
  }
  while ((tok_ = checker_->next_misspelling()).len == 0) {
    next_line(cur_line_);
    diff_ = 0;
    if (off_end(cur_line_)) return false;
    checker_->process(cur_line_->data(), cur_line_->size());
  }
  word_size_  = tok_.len;
  word_begin_ = cur_line_->begin() + tok_.offset + diff_;
  return true;
}

void CheckerString::replace(ParmString repl)
{
  assert(word_size_ > 0);
  int offset = word_begin_ - cur_line_->begin();
  cur_line_->replace(word_begin_, word_begin_ + word_size_,
		     repl.str(), repl.str() + repl.size());
  word_begin_ = cur_line_->begin() + offset;
  word_size_ = repl.size();
  has_repl_ = true;
}

int dist(CheckerString::Iterator smaller, 
	 CheckerString::Iterator larger)
{
  if (smaller.line_ == larger.line_)
    return larger.i_ - smaller.i_;
  int d = 0;
  d += smaller.line_->end() - smaller.i_;
  smaller.cs_->inc(smaller.line_);
  while (smaller.line_ != larger.line_) {
    d += smaller.line_->size();
    smaller.cs_->inc(smaller.line_);
  }
  d += larger.i_ - larger.line_->begin() + 1;
  return d;
}
