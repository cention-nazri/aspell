/*
 * Copyright (c) 2005
 * Kevin Atkinson
 *
 * Permission to use, copy, modify, distribute and sell this software
 * and its documentation for any purpose is hereby granted without
 * fee, provided that the above copyright notice appear in all copies  
 * and that both that copyright notice and this permission notice 
 * appear in supporting documentation.  Kevin Atkinson makes no
 * representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#if defined(__CYGWIN__) || defined (_WIN32)

#  include <io.h>
#  include <fcntl.h>

#  define SETBIN(fno)  _setmode( _fileno( fno ), _O_BINARY )

#else

#  define SETBIN(fno)

#endif

typedef struct Word {
  char * str;
  size_t alloc;
} Word;

#define INSURE_SPACE(cur,p,need)\
  do {\
    size_t pos = p - (cur)->str;\
    if (pos + need + 1 < (cur)->alloc) break;\
    (cur)->alloc = (cur)->alloc*3/2;\
    (cur)->str = (char *)realloc((cur)->str, (cur)->alloc);\
    p = (cur)->str + pos;\
  } while (0)
    
int main (int argc, const char *argv[]) {

  if (argc != 2) {

    goto usage;
    
  } else if (strcmp(argv[1], "-z") == 0) {
    
    Word w1,w2;
    Word * prev = &w1;
    Word * cur  = &w2;
    char * w = 0;
    char * p = 0;
    int c,l;

    w1.str = (char *)malloc(256);
    w1.alloc = 256;
    w2.str = (char *)malloc(256);
    w2.alloc = 256;

    SETBIN (stdout);

    putc(2, stdout);

    c = 0;
    while (c != EOF)
    {
      /* get next word */
      w = cur->str;
      while (c = getc(stdin), c != EOF && c != '\n') {
        if (c >= 32) {
          INSURE_SPACE(cur, w, 1);
          *w++ = c;
        } else {
          INSURE_SPACE(cur, w, 2);
          *w++ = 31;
          *w++ = c + 32;
        }
      }
      *w = 0;
      p = prev->str;
      w = cur->str;

      /* get the length of the prefix */
      l = 0;
      while (p[l] != '\0' && w[l] != '\0' && p[l] == w[l]) ++l;
      
      /* prefix compress, and write word */
      if (l < 30) {
        putc(l, stdout);
      } else {
        int i = l - 30;
        putc(30, stdout);
        while (i >= 255) {putc(255, stdout); i -= 255;}
	putc(i, stdout);
      } 
      fputs(w+l, stdout);

      /* swap prev and next */
      {
        Word * tmp = cur;
        cur = prev;
        prev = tmp;
      }
    }
    
    free(w1.str);
    free(w2.str);

  } else if (strcmp(argv[1], "-d") == 0) {
    
    Word cur;
    int c;
    char * w;

    cur.str = (char *)malloc(256);
    cur.alloc = 256;
    w = cur.str;

    SETBIN (stdin);

    c = getc(stdin);
    
    if (c == 2) 
    {
      c = getc(stdin);
      for (;;) {
        w = cur.str + c;
        if (c == 30) {
          while (c = getc(stdin), c == 255) w += 255;
          w += c;
        }
        while (c = getc(stdin), c > 30) {
          INSURE_SPACE(&cur,w,1);
          *w++ = (char)c;
        }
        if (c == EOF && !cur.str[0]) break;
        *w = '\0';
        for (w = cur.str; *w; w++) {
          if (*w != 31) putc(*w, stdout);
          else          putc(*++w - 32, stdout);
        }
        putc('\n', stdout);
      }
    } 
    else if (c == 1) 
    {
      while (c != -1) {
        if (c == 0)
          c = getc(stdin);
        --c;
        w = cur.str + 1;
        while (c = getc(stdin), c > 32) {
          INSURE_SPACE(&cur,w,1);
          *w++ = (char)c;
        }
        *w = '\0';
        fputs(cur.str, stdout);
        putc('\n', stdout);
      }

      free (cur.str);
    }
    else
    {
      fprintf(stderr, "Unknown format.\n");
      return 1;
    }

  } else {

    goto usage;

  }

  return 0;

  usage:

  fprintf(stderr,
          "Compresses or decompresses sorted word lists.\n"
          "For best result the the environmental variable\n"
          "LC_COLLATE should be set to \"C\" before sorting.\n"
          "Usgae:\n"
          "  To Compress:   %s -z\n"
          "  To Decompress: %s -d\n", argv[0], argv[0]);
  return 1;
}
