
#include <stdio.h>
#include <string.h>

#include "pspell.h"

static void print_word_list(PspellManager * manager, 
			    const PspellWordList *wl) 
{
  if (wl == 0) {
    printf("Error: %s\n", pspell_manager_error_message(manager));
  } else {
    PspellStringEnumeration * els = pspell_word_list_elements(wl);
    const char * word;
    while ( (word = pspell_string_enumeration_next(els)) != 0) {
      puts(word);
    }
  }
}

static void check_for_error(PspellManager * manager)
{
  if (pspell_manager_error(manager) != 0) {
    printf("Error: %s\n", pspell_manager_error_message(manager));
  }
}

int main(int argc, const char *argv[]) {

  PspellCanHaveError * ret;
  PspellManager * manager;
  int have;
  char word[81];
  char * word_end;
  PspellConfig * config;

  if (argc < 2) {
    printf("Usage: %s <language> [<size>|- [[<jargon>|- [<encoding>]]]\n", argv[0]);
    return 1;
  }

  config = new_pspell_config();

  pspell_config_replace(config, "lang", argv[1]);

  pspell_config_replace(config, "sug-mode", "fast"); 
  // to make things faster, espacally when not compiled with optimiztion,
  // REMOVE BEFORE RELEASE

  if (argc >= 3 && argv[2][0] != '-' && argv[2][1] != '\0')
    pspell_config_replace(config, "size", argv[2]);

  if (argc >= 4 && argv[3][0] != '-')
    pspell_config_replace(config, "jargon", argv[3]);

  if (argc >= 5 && argv[4][0] != '-')
    pspell_config_replace(config, "encoding", argv[4]);

  ret = new_pspell_manager(config);

  delete_pspell_config(config);

  if (pspell_error(ret) != 0) {
    printf("Error: %s\n",pspell_error_message(ret));
    return 2;
  }
  manager = to_pspell_manager(ret);
  config = pspell_manager_config(manager);

  fputs("Using: ",                                      stdout);
  fputs(pspell_config_retrieve(config, "lang"),         stdout);
  fputs("-",                                            stdout);
  fputs(pspell_config_retrieve(config, "jargon"),       stdout);
  fputs("-",                                            stdout);
  fputs(pspell_config_retrieve(config, "size"),         stdout);
  fputs("-",                                            stdout);
  fputs(pspell_config_retrieve(config, "module"),       stdout);
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
	"  m           dumps the master  word list\n"
	"  S           saves all word lists\n"
	"  C           clear the cuurent sesstion word list\n"
	"  x           quite\n"	);
      break;
    case 'p':
      print_word_list(manager, pspell_manager_personal_word_list(manager));
      break;
    case 'P':
      print_word_list(manager, pspell_manager_session_word_list(manager));
      break;
#if 0
    case 'm':
      print_word_list(manager, pspell_manager_master_word_list(manager));
      break;
#endif
    case 'S':
      pspell_manager_save_all_word_lists(manager);
      check_for_error(manager);
      break;
    case 'C': 
      pspell_manager_clear_session(manager);
      check_for_error(manager);
      break;
    case 'x':
      goto END;
    case 'c':
      if (strlen(word) < 3) {
	printf("Usage: %c <word>\n", word[0]);
      } else {
	have = pspell_manager_check(manager, word + 2, -1);
	if (have == 1) 
	  puts("correct");
	else if (have == 0)
	  puts("incorrect");
	else
	  printf("Error: %s\n", pspell_manager_error_message(manager));
      }
      break;
    case 's':
      if (strlen(word) < 3) {
	printf("Usage: %c <word>\n", word[0]);
      } else {
	print_word_list(manager, pspell_manager_suggest(manager, word + 2, -1));
      }
      break;
    case 'a':
      if (strlen(word) < 3) {
	printf("Usage: %c <word>\n", word[0]);
      } else {
	pspell_manager_add_to_personal(manager, word + 2, -1);
	check_for_error(manager);
      }
      break;
    case 'i':
      if (strlen(word) < 3) {
	printf("Usage: %c <word>\n", word[0]);
      } else {
	pspell_manager_add_to_session(manager, word + 2, -1);
	check_for_error(manager);
      }
      break;
    default:
      printf("Unknown Command: %s\n", word);
    }
    putchar('\n');
  }
 END:
  delete_pspell_manager(manager);
  return 0;
}
