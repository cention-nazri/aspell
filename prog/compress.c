#include <stdio.h>

void usage () 
{
  fputs("Compresses or uncompresses sorted word lists.\n"     , stderr);
  fputs("For best result the locale should be set to C.\n"    , stderr);
  fputs("before sorting by setting the environmental\n"       , stderr);
  fputs("variable LANG to \"C\" before sorting.\n"            , stderr);
  fputs("Copyright 2001 by Kevin Atkinson under the LGPL.\n"  , stderr);
  fputs("Usage: word-list-compress c[ompress]|d[ecompress]\n" , stderr);
}

static int get_word(FILE * in, char * w) 
{
  int c;
  while (c = getc(in), c != EOF && c <= 32);
  if (c == EOF) return 0;
  do {
    *w++ = (char)(c);
  } while (c = getc(in), c != EOF && c > 32);
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

    char s1[256];
    char s2[256];
    char * prev = s2;
    char * cur = s1;
    *prev = '\0';

    while (get_word(stdin, cur)) {
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
    i = getc(stdin);
    while (i != -1 ) {
      if (i == 0)
	i = getc(stdin);
      --i;  
      while ((c = getc(stdin)) > 32)
	cur[i++] = (char)c;
      cur[i] = '\0';
      fputs(cur, stdout);
      putc('\n', stdout);
      i = c;
    }
    return 0;

  } else {

    usage();
    return 1;
    
  }
}
