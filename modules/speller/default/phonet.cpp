/*  phonetic.c - generic replacement aglogithms for phonetic transformation
    Copyright (C) 2000 Björn Jacke

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License version 2.1 as published by the Free Software Foundation;
 
    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.
 
    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Björn Jacke may be reached by email at bjoern.jacke@gmx.de

    Changelog:

    2000-01-05  Björn Jacke <bjoern.jacke@gmx.de>
                Initial Release insprired by the article about phonetic
                transformations out of c't 25/1999

*/

#include <string.h>
#include <assert.h>

#include <vector>

#include "asc_ctype.hpp"
#include "string.hpp"
#include "phonet.hpp"
#include "errors.hpp"
#include "fstream.hpp"
#include "getdata.hpp"

using namespace acommon;

namespace aspeller {

  const char * const PhonetParms::rules_end = "";
  
  static bool to_bool(const String & str) {
    if (str == "1" || str == "true") return true;
    else return false;
  }
#if 0
  void dump_phonet_rules(ostream & out, const PhonetParms & parms) {
    out << "version         " << parms.version << "\n";
    out << "followup        " << parms.followup << "\n";
    out << "collapse_result " << parms.collapse_result << "\n";
    out << "\n";
    ios::fmtflags flags = out.setf(ios::left);
    for (int i = 0; parms.rules[i] != PhonetParms::rules_end; i += 2) {
      out << setw(20) << parms.rules[i] << " " 
	  << (parms.rules[i+1][0] == '\0' ? "_" : parms.rules[i+1])
	  << "\n";
    }
    out.flags(flags);
  }
#endif

  struct PhonetParmsImpl : public PhonetParms {
    std::vector<const char *> rdata;
    std::vector<char>         data;
    void assign(const PhonetParms * other) {
      abort();
      *this = *(const PhonetParmsImpl *)other;
      this->rules = &rdata.front();
    }
    PhonetParmsImpl * clone() const {
      PhonetParmsImpl * other = new PhonetParmsImpl(*this);
      return other;
    }
    PhonetParmsImpl() {}
    PhonetParmsImpl(const PhonetParmsImpl & other) 
      : PhonetParms(other), rdata(other.rdata.size()), data(other.data)
    {
      fix_pointers(other);
    }
    void fix_pointers(const PhonetParmsImpl & other) {
      if (other.rdata.empty()) return;
      rules = &rdata.front();
      int i = 0;
      for (;other.rules[i] != rules_end; ++i) {
	rules[i] = &data.front() + (&other.data.front() - other.rules[i]);
      }
      rules[i]   = rules_end;
      rules[i+1] = rules_end;
    }
  };
  
  PosibErr<PhonetParms *> load_phonet_rules(const String & file) {
    FStream in;
    RET_ON_ERR(in.open(file, "r"));

    PhonetParmsImpl * parms = new PhonetParmsImpl();

    parms->followup        = true;
    parms->collapse_result = false;

    String key;
    String data;
    int size = 0;
    int num = 0;
    while (true) {
      if (!getdata_pair(in, key, data)) break;
      if (key != "followup" && key != "collapse_result" &&
	  key != "version") {
	++num;
	size += key.size() + 1;
	if (data != "_") {
	  size += data.size() + 1;
	}
      }
    }

    parms->data.reserve(size);
    char * d = &parms->data.front();

    parms->rdata.reserve(2 * num + 2);
    std::vector<const char *>::iterator r = parms->rdata.begin();

    in.restart();

    while (true) {
      if (!getdata_pair(in, key, data)) break;
      if (key == "followup") {
	parms->followup = to_bool(data);
      } else if (key == "collapse_result") {
	parms->collapse_result = to_bool(data);
      } else if (key == "version") {
	parms->version = data;
      } else {
	strncpy(d, key.c_str(), key.size() + 1);
	*r = d;
	++r;
	d += key.size() + 1;
	if (data == "_") {
	  *r = "";
	} else {
	  strncpy(d, data.c_str(), data.size() + 1);
	  *r = d;
	  d += data.size() + 1;
	}
	++r;
      }
    }
    if (d != &parms->data.front() + size) {
      delete parms;
      return make_err(file_error, file);
    }
    if (parms->version.empty()) {
      delete parms;
      return make_err(bad_file_format, file, "You must specify a version string");
    }
    *(r  ) = PhonetParms::rules_end;
    *(r+1) = PhonetParms::rules_end;
    parms->rules = &parms->rdata.front();

    return parms;
  }

  // FIXME: Get this information from the language class
  void init_phonet_charinfo(PhonetParms & parms) {
    
    const unsigned char * vowels_low = 
      (const unsigned char *) "àáâãäåæçèéêëìíîïðñòóôõöøùúûüýþßÿ";
    const unsigned char * vowels_cap =
      (const unsigned char *) "ÀÁÂÃÄÅÆÇÈÉÊËÌÍÎÏÐÑÒÓÔÕÖØÙÚÛÜÝÞßÿ";
    
    int i;
    /**  create "to_upper" and "is_alpha" arrays  **/
    for (i = 0; i < 256; i++) {
      parms.to_upper[i] = asc_islower(i) ?  (unsigned char) asc_toupper(i) 
	                                 : (unsigned char) i;
      parms.is_alpha[i] = asc_isalpha(i);
    }
    for (i = 0; vowels_low[i] != '\0'; i++) {
      /**  toupper doen't handel "unusual" characters  **/
      parms.to_upper[vowels_low[i]] = vowels_cap[i];
      parms.to_upper[vowels_cap[i]] = vowels_cap[i];
      parms.is_alpha[vowels_low[i]] = parms.is_alpha[vowels_cap[i]] = true;
    }

  }

  void init_phonet_hash(PhonetParms & parms) {

    int i, k;

    for (i = 0; i < parms.hash_size; i++) {
      parms.hash[i] = -1;
    }

    for (i = 0; parms.rules[i] != PhonetParms::rules_end; i += 2) {
      /**  set hash value  **/
      k = (unsigned char) parms.rules[i][0];

      if (parms.hash[k] < 0) {
	parms.hash[k] = i;
      }
    }
  }


#ifdef PHONET_TRACE
  void trace_info(char * text, int n, char * error, 
		  const PhonetParms & parms) 
  {
    /**  dump tracing info  **/
    
    printf ("%s %d:  \"%s\"  >  \"%s\" %s", text, ((n/2)+1), parms.rules[n],
	    parms.rules[n+1], error);
  }
#endif

  int phonet (const char * inword, char * target, 
	      const PhonetParms & parms)
  {
    assert (target != NULL && inword != NULL);
    /**       Do phonetic transformation.       **/
    /**  "len" = length of "inword" incl. '\0'. **/

    /**  result:  >= 0:  length of "target"    **/
    /**            otherwise:  error            **/


    int  i,j,k=0,n,p,z;
    int  k0,n0,p0=-333,z0;
    int len = strlen(inword)+1;
    std::vector<char> word(len);
    char c, c0;
    const char * s;

    typedef unsigned char uchar;
    
    /**  to_upperize string  **/
    for (i = 0; inword[i] != '\0'; i++)
      word[i] = parms.to_upper[(uchar)inword[i]];
    word[i] = '\0';

    /**  check word  **/
    i = j = z = 0;
    while ((c = word[i]) != '\0') {
      #ifdef PHONET_TRACE
         cout << "\nChecking position " << j << ":  word = \""
              << word+i << "\",";
         printf ("  target = \"%.*s\"", j, target);
      #endif
      n = parms.hash[(uchar) c];
      z0 = 0;

      if (n >= 0) {
        /**  check all rules for the same letter  **/
        while (parms.rules[n][0] == c) {
          #ifdef PHONET_TRACE
             trace_info ("\n> Checking rule No.",n,"",parms);
          #endif

          /**  check whole string  **/
          k = 1;   /** number of found letters  **/
          p = 5;   /** default priority  **/
          s = parms.rules[n];
          s++;     /**  important for (see below)  "*(s-1)"  **/
          
          while (*s != '\0'  &&  word[i+k] == *s
                 &&  !asc_isdigit (*s)  &&  strchr ("(-<^$", *s) == NULL) {
            k++;
            s++;
          }
          if (*s == '(') {
            /**  check letters in "(..)"  **/
            if (parms.is_alpha[(uchar)word[i+k]]  // ...could be implied?
                && strchr(s+1, word[i+k]) != NULL) {
              k++;
              while (*s != ')')
                s++;
              s++;
            }
          }
          p0 = (int) *s;
          k0 = k;
          while (*s == '-'  &&  k > 1) {
            k--;
            s++;
          }
          if (*s == '<')
            s++;
          if (asc_isdigit (*s)) {
            /**  determine priority  **/
            p = *s - '0';
            s++;
          }
          if (*s == '^'  &&  *(s+1) == '^')
            s++;

          if (*s == '\0'
              || (*s == '^'  
                 && (i == 0  ||  ! parms.is_alpha[(uchar)word[i-1]])
                 && (*(s+1) != '$'
                 || (! parms.is_alpha[(uchar)word[i+k0]] )))
              || (*s == '$'  &&  i > 0  
                 &&  parms.is_alpha[(uchar)word[i-1]]
                 && (! parms.is_alpha[(uchar)word[i+k0]] ))) {
            /**  search for followup rules, if:     **/
            /**  parms.followup and k > 1  and  NO '-' in searchstring **/
            c0 = word[i+k-1];
            n0 = parms.hash[(uchar) c0];
//
            if (parms.followup  &&  k > 1  &&  n0 >= 0
                &&  p0 != (int) '-'  &&  word[i+k] != '\0') {
              /**  test follow-up rule for "word[i+k]"  **/
              while (parms.rules[n0][0] == c0) {
                #ifdef PHONET_TRACE
                    trace_info ("\n> > follow-up rule No.",n0,"... ",parms);
                #endif

                /**  check whole string  **/
                k0 = k;
                p0 = 5;
                s = parms.rules[n0];
                s++;
                while (*s != '\0'  &&  word[i+k0] == *s
                       && ! asc_isdigit(*s)  &&  strchr("(-<^$",*s) == NULL) {
                  k0++;
                  s++;
                }
                if (*s == '(') {
                  /**  check letters  **/
                  if (parms.is_alpha[(uchar)word[i+k0]]
                      &&  strchr (s+1, word[i+k0]) != NULL) {
                    k0++;
                    while (*s != ')'  &&  *s != '\0')
                      s++;
                    if (*s == ')')
                      s++;
                  }
                }
                while (*s == '-') {
                  /**  "k0" gets NOT reduced   **/
                  /**  because "if (k0 == k)"  **/
                  s++;
                }
                if (*s == '<')
                  s++;
                if (asc_isdigit (*s)) {
                  p0 = *s - '0';
                  s++;
                }

                if (*s == '\0'
                    /**  *s == '^' cuts  **/
                    || (*s == '$'  &&  ! parms.is_alpha[(uchar)word[i+k0]])) {
                  if (k0 == k) {
                    /**  this is just a piece of the string  **/
                    #ifdef PHONET_TRACE
                        cout << "discarded (too short)";
                    #endif
                    n0 += 2;
                    continue;
                  }

                  if (p0 < p) {
                    /**  priority too low  **/
                    #ifdef PHONET_TRACE
                        cout << "discarded (priority)";
                    #endif
                    n0 += 2;
                    continue;
                  }
                  /**  rule fits; stop search  **/
                  break;
                }
                #ifdef PHONET_TRACE
                    cout << "discarded";
                #endif
                n0 += 2;
              } /**  End of "while (parms.rules[n0][0] == c0)"  **/

              if (p0 >= p  && parms.rules[n0][0] == c0) {
                #ifdef PHONET_TRACE
                    trace_info ("\n> Rule No.", n,"",parms);
                    trace_info ("\n> not used because of follow-up",
                                      n0,"",parms);
                #endif
                n += 2;
                continue;
              }
            } /** end of follow-up stuff **/

            /**  replace string  **/
            #ifdef PHONET_TRACE
                trace_info ("\nUsing rule No.", n,"\n",parms);
            #endif
            s = parms.rules[n+1];
            p0 = (parms.rules[n][0] != '\0'
                 &&  strchr (parms.rules[n]+1,'<') != NULL) ? 1:0;
            if (p0 == 1 &&  z == 0) {
              /**  rule with '<' is used  **/
              if (j > 0  &&  *s != '\0'
                 && (target[j-1] == c  ||  target[j-1] == *s)) {
                j--;
              }
              z0 = 1;
              z = 1;
              k0 = 0;
              while (*s != '\0'  &&  word[i+k0] != '\0') {
                word[i+k0] = *s;
                k0++;
                s++;
              }
              if (k > k0)
                strcpy (&word[0]+i+k0, &word[0]+i+k);

              /**  new "actual letter"  **/
              c = word[i];
            }
            else { /** no '<' rule used **/
              i += k - 1;
              z = 0;
              while (*s != '\0'
                     &&  *(s+1) != '\0'  &&  j < len-1) {
                if (j == 0  ||  target[j-1] != *s) {
                  target[j] = *s;
                  j++;
                }
                s++;
              }
              /**  new "actual letter"  **/
              c = *s;
              if (parms.rules[n][0] != '\0'
                 &&  strstr (parms.rules[n]+1, "^^") != NULL) {
                if (c != '\0') {
                  target[j] = c;
                  j++;
                }
                strcpy (&word[0], &word[0]+i+1);
                i = 0;
                z0 = 1;
              }
            }
            break;
          }  /** end of follow-up stuff **/
          n += 2;
        } /**  end of while (parms.rules[n][0] == c)  **/
      } /**  end of if (n >= 0)  **/
      if (z0 == 0) {
        if (k && (assert(p0!=-333),!p0) &&  j < len-1  &&  c != '\0'
           && (!parms.collapse_result  ||  j == 0  ||  target[j-1] != c)){
           /**  condense only double letters  **/
          target[j] = c;
	  ///printf("\n setting \n");
          j++;
        }
        #ifdef PHONET_TRACE
        else if (p0 || !k)
          cout << "\nNo rule found; character \"" << word[i] << "\" skipped\n";
        #endif

        i++;
        z = 0;
	k=0;
      }
    }  /**  end of   while ((c = word[i]) != '\0')  **/

    target[j] = '\0';
    return (j);

  }  /**  end of function "phonet"  **/
}

#if 0

int main (int argc, char *argv[]) {
  using namespace autil;

  if (argc < 3) {
    printf ("Usage:  phonet <data file> <word>\n");
    return(1);
  }

  char phone_word[strlen(argv[2])+1]; /**  max possible length of words  **/

  PhonetParms * parms;
  ifstream f(argv[1]);
  parms = load_phonet_rules(f);

  init_phonet_charinfo(*parms);
  init_phonet_hash(*parms);
  phonet (argv[2],phone_word,*parms);
  printf ("%s\n", phone_word);
  return(0);
}
#endif
