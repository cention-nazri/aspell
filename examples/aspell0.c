// This file is part of The New Aspell
// Copyright (C) 2000-2001 by Kevin Atkinson under the GNU LGPL
// license version 2.0 or 2.1.  You should have received a copy of the
// LGPL license along with this library if you did not you can find it
// at http://www.gnu.org/.

#include <stdio.h>
#include <string.h>

#include "aspell.h"

static void print_word_list(AspellSpeller * speller, 
			    const AspellWordList *wl) 
{
  if (wl == 0) {
    printf("Error: %s\n", aspell_speller_error_message(speller));
  } else {
    AspellStringEnumeration * els = aspell_word_list_elements(wl);
    const char * word;
    while ( (word = aspell_string_enumeration_next(els)) != 0) {
      puts(word);
    }
  }
}

static void check_for_error(AspellSpeller * speller)
{
  if (aspell_speller_error(speller) != 0) {
    printf("Error: %s\n", aspell_speller_error_message(speller));
  }
}

int main(int argc, const char *argv[]) {

  AspellCanHaveError * ret;
  AspellSpeller * speller;
  int have;
  char word[81];
  char * word_end;
  AspellConfig * config;

  if (argc < 2) {
    printf("Usage: %s <language> [<size>|- [[<jargon>|- [<encoding>]]]\n", argv[0]);
    return 1;
  }

  config = new_aspell_config();

  aspell_config_replace(config, "lang", argv[1]);

  aspell_config_replace(config, "sug-mode", "ultra"); 
  // to make things faster, espacally when not compiled with optimiztion,
  // REMOVE BEFORE RELEASE

  if (argc >= 3 && argv[2][0] != '-' && argv[2][1] != '\0')
    aspell_config_replace(config, "size", argv[2]);

  if (argc >= 4 && argv[3][0] != '-')
    aspell_config_replace(config, "jargon", argv[3]);

  if (argc >= 5 && argv[4][0] != '-')
    aspell_config_replace(config, "encoding", argv[4]);

  ret = new_aspell_speller(config);

  delete_aspell_config(config);

  if (aspell_error(ret) != 0) {
    printf("Error: %s\n",aspell_error_message(ret));
    return 2;
  }
  speller = to_aspell_speller(ret);
  config = aspell_speller_config(speller);

  fputs("Using: ",                                      stdout);
  fputs(aspell_config_retrieve(config, "lang"),         stdout);
  fputs("-",                                            stdout);
  fputs(aspell_config_retrieve(config, "jargon"),       stdout);
  fputs("-",                                            stdout);
  fputs(aspell_config_retrieve(config, "size"),         stdout);
  fputs("-",                                            stdout);
  fputs(aspell_config_retrieve(config, "module"),       stdout);
  fputs("\n\n",                                         stdout);

  puts("Type \"h\" for help.\n");

  while (fgets(word, 80, stdin) != 0) {

    /* remove trailing spaces */

    word_end = strchr(word, '\0') - 1;
    while (word_end != word && (*word_end == '\n' || *word_end == ' ')) 
      --word_end;
    ++word_end;
    *word_end = '\0';
    
    putchar('\n');
    switch (word[0]) {
    case '\0':
      break;
    case 'h':
      puts(
	"Usage: \n"
	"  h(elp)      help\n"
	"  c <word>    check if a word is the correct spelling\n"
	"  s <word>    print out a list of suggestions for a word\n"
	"  a <word>    add a word to the personal word list\n"
	"  i <word>    ignore a word for the rest of the session\n"
	"  p           dumps the personal word list\n"
	"  P           dumps the session word list\n"
	"  m           dumps the main  word list\n"
	"  S           saves all word lists\n"
	"  C           clear the cuurent sesstion word list\n"
	"  x           quite\n"	);
      break;
    case 'p':
      print_word_list(speller, aspell_speller_personal_word_list(speller));
      break;
    case 'P':
      print_word_list(speller, aspell_speller_session_word_list(speller));
      break;
    case 'm':
      print_word_list(speller, aspell_speller_main_word_list(speller));
      break;
    case 'S':
      aspell_speller_save_all_word_lists(speller);
      check_for_error(speller);
      break;
    case 'C': 
      aspell_speller_clear_session(speller);
      check_for_error(speller);
      break;
    case 'x':
      goto END;
    case 'c':
      if (strlen(word) < 3) {
	printf("Usage: %c <word>\n", word[0]);
      } else {
	have = aspell_speller_check(speller, word + 2, -1);
	if (have == 1) 
	  puts("correct");
	else if (have == 0)
	  puts("incorrect");
	else
	  printf("Error: %s\n", aspell_speller_error_message(speller));
      }
      break;
    case 's':
      if (strlen(word) < 3) {
	printf("Usage: %c <word>\n", word[0]);
      } else {
	print_word_list(speller, aspell_speller_suggest(speller, word + 2, -1));
      }
      break;
    case 'a':
      if (strlen(word) < 3) {
	printf("Usage: %c <word>\n", word[0]);
      } else {
	aspell_speller_add_to_personal(speller, word + 2, -1);
	check_for_error(speller);
      }
      break;
    case 'i':
      if (strlen(word) < 3) {
	printf("Usage: %c <word>\n", word[0]);
      } else {
	aspell_speller_add_to_session(speller, word + 2, -1);
	check_for_error(speller);
      }
      break;
    default:
      printf("Unknown Command: %s\n", word);
    }
    putchar('\n');
  }
 END:
  delete_aspell_speller(speller);
  return 0;
}
