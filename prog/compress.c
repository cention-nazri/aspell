/*
 * Copyright (c) 2000-2001
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

#include <stdio.h>

#if defined(__CYGWIN__) || defined (_WIN32)

#  include <io.h>
#  include <fcntl.h>

#  define SETBIN(fno)  _setmode( _fileno( fno ), _O_BINARY )

#else

#  define SETBIN(fno)

#endif

#define BUFSIZE 256

void usage () 
{
  fputs("Compresses or uncompresses sorted word lists.\n"     , stderr);
  fputs("For best result the locale should be set to C\n"    , stderr);
  fputs("before sorting by setting the environmental\n"       , stderr);
  fputs("variable LANG to \"C\" before sorting.\n"            , stderr);
  fputs("Copyright 2001,2004 by Kevin Atkinson.\n"  , stderr);
  fputs("Usage: word-list-compress c[ompress]|d[ecompress]\n" , stderr);
}

// bufsize > 2
static int get_word(FILE * in, char * w, size_t bufsize) 
{
  int c;
  while (c = getc(in), c != EOF && c <= 32);
  if (c == EOF) return 0;
  do {
    *w++ = (char)(c);
    --bufsize;
  } while (c = getc(in), c != EOF && c > 32 && bufsize > 1);
  *w = '\0';
  ungetc(c, in);
  if (c == EOF) return 0;
  else return 1;
}

int main (int argc, const char *argv[]) {

  if (argc != 2) {

    usage();
    return 1;
    
  } else if (argv[1][0] == 'c') {

    char s1[BUFSIZE];
    char s2[BUFSIZE];
    char * prev = s2;
    char * cur = s1;
    *prev = '\0';

    SETBIN (stdout);

    while (get_word(stdin, cur, BUFSIZE)) {
      int i = 0;
      /* get the length of the prefix */
      while (prev[i] != '\0' && cur[i] != '\0' && prev[i] == cur[i])
	++i;
      if (i > 31) {
	putc('\0', stdout);
      }
      putc(i+1, stdout);
      fputs(cur+i, stdout);
      if (cur == s1) {
	prev = s1; cur = s2;
      } else {
	prev = s2; cur = s1;
      }
    }
    return 0;

  } else if (argv[1][0] == 'd') {
    
    char cur[256];
    int i;
    int c;

    SETBIN (stdin);

    i = getc(stdin);
    while (i != -1 ) {
      if (i == 0)
	i = getc(stdin);
      --i;
      if (i < 0) goto error;
      while ((c = getc(stdin)) > 32 && i < BUFSIZE)
	cur[i++] = (char)c;
      if (i == BUFSIZE) goto error;
      cur[i] = '\0';
      fputs(cur, stdout);
      putc('\n', stdout);
      i = c;
    }
    return 0;

   error:
    fputs("ERROR: Corrupt Input.\n", stderr);
    return 2;

  } else {

    usage();
    return 1;
    
  }
}
