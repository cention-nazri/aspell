// This file is part of The New Aspell
// Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL license
// version 2.0 or 2.1.  You should have received a copy of the LGPL
// license along with this library if you did not you can find
// it at http://www.gnu.org/.

// Aspell implementation header file.
// Applications that just use the aspell library should not include 
// these files as they are subject to change.
// Aspell Modules MUST include some of the implementation files and
// spell checkers MAY include some of these files.
// If ANY of the implementation files are included you also link with
// libaspell-impl to protect you from changes in these files.

#ifndef ASPELL_SPELLER__HPP
#define ASPELL_SPELLER__HPP

#include "can_have_error.hpp"
#include "copy_ptr.hpp"
#include "mutable_string.hpp"
#include "posib_err.hpp"
#include "parm_string.hpp"
#include "vector.hpp"

namespace acommon {

  typedef void * SpellerLtHandle;

  class Config;
  class WordList;

  class Speller : public CanHaveError
  {
  private:
    SpellerLtHandle lt_handle_;
    Speller(const Speller &);
    Speller & operator= (const Speller &);
  public:
    Vector<char> temp_str_0;
    Vector<char> temp_str_1;
    void (* from_encoded_)(ParmString, Vector<char> &);
    void (* to_encoded_  )(ParmString, Vector<char> &);
  protected:
    CopyPtr<Config> config_;
    Speller(SpellerLtHandle h) : lt_handle_(h), from_encoded_(0), to_encoded_(0) {}
  public:
    SpellerLtHandle lt_handle() const {return lt_handle_;}

    Config * config() {return config_;}
    const Config * config() const {return config_;}


    // the setup class will take over for config
    virtual PosibErr<void> setup(Config *) = 0;

    ////////////////////////////////////////////////////////////////
    // 
    // Strings from this point on are expected to be in the 
    // encoding specified by encoding()
    //

    virtual PosibErr<bool> check(MutableString) = 0;
  
    virtual PosibErr<void> add_to_personal(MutableString) = 0;
    virtual PosibErr<void> add_to_session (MutableString) = 0;
    
    // because the word lists may potently have to convert from one
    // encoding to another the pointer returned by the enumeration is only
    // valid to the next call.

    virtual PosibErr<const WordList *> personal_word_list() const = 0;
    virtual PosibErr<const WordList *> session_word_list () const = 0;
    virtual PosibErr<const WordList *> main_word_list () const = 0;
  
    virtual PosibErr<void> save_all_word_lists() = 0;
  
    virtual PosibErr<void> clear_session() = 0;

    virtual PosibErr<const WordList *> suggest(MutableString) = 0;
    // return null on error
    // the word list returned by suggest is only valid until the next
    // call to suggest
  
    virtual PosibErr<void> store_replacement(MutableString, 
					     MutableString) = 0;
  
    virtual ~Speller() {}

  };

  PosibErr<Speller *> new_speller(Config * c);

}

#endif
