
#include <ctype.h>
#include <string.h>
#include <stdlib.h>

#include "itemize.hpp"
#include "mutable_container.hpp"

namespace pcommon {

  struct ItemizeItem {
    char   action;
    const char * name;
    ItemizeItem() : action('\0'), name(0) {}
  };

  class ItemizeTokenizer {
  private:
    char * list;
    char * i;
  public:
    ItemizeTokenizer(const char * l);
    ~ItemizeTokenizer();
  private:
    ItemizeTokenizer(const ItemizeTokenizer & other) ;
    ItemizeTokenizer & operator=(const ItemizeTokenizer & other);
  public:
    ItemizeItem next();
  };

  ItemizeTokenizer::ItemizeTokenizer(const char * l) 
  {
    size_t size = strlen(l) + 1;
    list = new char[size];
    i = list;
    strncpy(list, l, size);
  }

  ItemizeTokenizer::~ItemizeTokenizer() 
  {
    delete[] list;
  }


  ItemizeItem ItemizeTokenizer::next() 
  {
    ItemizeItem li;
    while (*i != '\0' && (isspace(*i) || *i == ',')) ++i;
    if (*i == '\0') return li;
    li.action = *i;
    if (*i == '+' || *i == '-') {
      ++i;
    } else if (*i == '!') {
      li.name = "";
      ++i;
      return li;
    } else {
      li.action = '+';
    }
    while (*i != '\0' && *i != ',' && isspace(*i)) ++i;
    if (*i == '\0' || *i == ',') return next();
    li.name = i;
    while (*i != '\0' && *i != ',') ++i;
    while (isspace(*(i-1))) --i;
    if (*i != '\0') {
      *i = '\0';
      ++i;
    }
    return li;
  }


  void itemize (ParmString s, MutableContainer & d) {
    ItemizeTokenizer els(s);
    ItemizeItem li;
    while (li = els.next(), li.name != 0) {
      switch (li.action) {
      case '+':
	d.add(li.name);
	break;
      case '-':
	d.remove(li.name);
	break;
      case '!':
	d.clear();
	break;
      default:
	abort();
      }
    }
  }

}
