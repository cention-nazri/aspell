/* Automatically generated file.  Do not edit directly. */

/* This file is part of The New Aspell
 * Copyright (C) 2001 by Kevin Atkinson under the GNU LGPL
 * license version 2.0 or 2.1.  You should have received a copy of the
 * LGPL license along with this library if you did not you can find it
 * at http://www.gnu.org/.                                              */

#include "error.hpp"
#include "errors.hpp"

namespace acommon {


static const ErrorInfo perror_other_obj = {
  0, // isa
  0, // mesg
  0, // num_parms
  {} // parms
};
const ErrorInfo * const perror_other = &perror_other_obj;

static const ErrorInfo perror_operation_not_supported_obj = {
  0, // isa
  0, // mesg
  0, // num_parms
  {} // parms
};
const ErrorInfo * const perror_operation_not_supported = &perror_operation_not_supported_obj;

static const ErrorInfo perror_cant_copy_obj = {
  perror_operation_not_supported, // isa
  0, // mesg
  0, // num_parms
  {} // parms
};
const ErrorInfo * const perror_cant_copy = &perror_cant_copy_obj;

static const ErrorInfo perror_file_obj = {
  0, // isa
  "%file:1:", // mesg
  1, // num_parms
  {"file"} // parms
};
const ErrorInfo * const perror_file = &perror_file_obj;

static const ErrorInfo perror_cant_open_file_obj = {
  perror_file, // isa
  "The file \"%file:1\" can not be opened", // mesg
  1, // num_parms
  {"file"} // parms
};
const ErrorInfo * const perror_cant_open_file = &perror_cant_open_file_obj;

static const ErrorInfo perror_cant_read_file_obj = {
  perror_cant_open_file, // isa
  "The file \"%file:1\" can not be opened for reading.", // mesg
  1, // num_parms
  {"file"} // parms
};
const ErrorInfo * const perror_cant_read_file = &perror_cant_read_file_obj;

static const ErrorInfo perror_cant_write_file_obj = {
  perror_cant_open_file, // isa
  "The file \"%file:1\" can not be opened for writing.", // mesg
  1, // num_parms
  {"file"} // parms
};
const ErrorInfo * const perror_cant_write_file = &perror_cant_write_file_obj;

static const ErrorInfo perror_invalid_name_obj = {
  perror_file, // isa
  "The file name \"%file:1\" is invalid.", // mesg
  1, // num_parms
  {"file"} // parms
};
const ErrorInfo * const perror_invalid_name = &perror_invalid_name_obj;

static const ErrorInfo perror_bad_file_format_obj = {
  perror_file, // isa
  "The file \"%file:1\" is not in the proper format.", // mesg
  1, // num_parms
  {"file"} // parms
};
const ErrorInfo * const perror_bad_file_format = &perror_bad_file_format_obj;

static const ErrorInfo perror_dir_obj = {
  0, // isa
  0, // mesg
  1, // num_parms
  {"dir"} // parms
};
const ErrorInfo * const perror_dir = &perror_dir_obj;

static const ErrorInfo perror_cant_read_dir_obj = {
  perror_dir, // isa
  "The directory \"%dir:1\" can not be opened for reading.", // mesg
  1, // num_parms
  {"dir"} // parms
};
const ErrorInfo * const perror_cant_read_dir = &perror_cant_read_dir_obj;

static const ErrorInfo perror_config_obj = {
  0, // isa
  0, // mesg
  1, // num_parms
  {"key"} // parms
};
const ErrorInfo * const perror_config = &perror_config_obj;

static const ErrorInfo perror_unknown_key_obj = {
  perror_config, // isa
  "The key \"%key:1\" is unknown.", // mesg
  1, // num_parms
  {"key"} // parms
};
const ErrorInfo * const perror_unknown_key = &perror_unknown_key_obj;

static const ErrorInfo perror_cant_change_value_obj = {
  perror_config, // isa
  "The value for option \"%key:1\" can not be changed.", // mesg
  1, // num_parms
  {"key"} // parms
};
const ErrorInfo * const perror_cant_change_value = &perror_cant_change_value_obj;

static const ErrorInfo perror_bad_key_obj = {
  perror_config, // isa
  "The key \"%key:1\" is not %accepted:2 and is thus invalid.", // mesg
  2, // num_parms
  {"key", "accepted"} // parms
};
const ErrorInfo * const perror_bad_key = &perror_bad_key_obj;

static const ErrorInfo perror_bad_value_obj = {
  perror_config, // isa
  "The value \"%value:2\" is not %accepted:3 and is thus invalid for the key \"%key:1\".", // mesg
  3, // num_parms
  {"key", "value", "accepted"} // parms
};
const ErrorInfo * const perror_bad_value = &perror_bad_value_obj;

static const ErrorInfo perror_duplicate_obj = {
  perror_config, // isa
  0, // mesg
  1, // num_parms
  {"key"} // parms
};
const ErrorInfo * const perror_duplicate = &perror_duplicate_obj;

static const ErrorInfo perror_language_related_obj = {
  0, // isa
  0, // mesg
  1, // num_parms
  {"lang"} // parms
};
const ErrorInfo * const perror_language_related = &perror_language_related_obj;

static const ErrorInfo perror_unknown_language_obj = {
  perror_language_related, // isa
  "The language \"%lang:1\" is not known.", // mesg
  1, // num_parms
  {"lang"} // parms
};
const ErrorInfo * const perror_unknown_language = &perror_unknown_language_obj;

static const ErrorInfo perror_unknown_soundslike_obj = {
  perror_language_related, // isa
  "The soundslike \"%sl:2\" is not known.", // mesg
  2, // num_parms
  {"lang", "sl"} // parms
};
const ErrorInfo * const perror_unknown_soundslike = &perror_unknown_soundslike_obj;

static const ErrorInfo perror_language_not_supported_obj = {
  perror_language_related, // isa
  "The language \"%lang:1\" is not supported.", // mesg
  1, // num_parms
  {"lang"} // parms
};
const ErrorInfo * const perror_language_not_supported = &perror_language_not_supported_obj;

static const ErrorInfo perror_no_wordlist_for_lang_obj = {
  perror_language_related, // isa
  "No word lists can be found for the language \"%lang:1\".", // mesg
  1, // num_parms
  {"lang"} // parms
};
const ErrorInfo * const perror_no_wordlist_for_lang = &perror_no_wordlist_for_lang_obj;

static const ErrorInfo perror_mismatched_language_obj = {
  perror_language_related, // isa
  "...", // mesg
  2, // num_parms
  {"lang", "prev"} // parms
};
const ErrorInfo * const perror_mismatched_language = &perror_mismatched_language_obj;

static const ErrorInfo perror_encoding_obj = {
  0, // isa
  0, // mesg
  1, // num_parms
  {"encod"} // parms
};
const ErrorInfo * const perror_encoding = &perror_encoding_obj;

static const ErrorInfo perror_unknown_encoding_obj = {
  perror_encoding, // isa
  "The encoding \"%encod:1\" is not known.", // mesg
  1, // num_parms
  {"encod"} // parms
};
const ErrorInfo * const perror_unknown_encoding = &perror_unknown_encoding_obj;

static const ErrorInfo perror_encoding_not_supported_obj = {
  perror_encoding, // isa
  "The encoding \"%encod:1\" is not supported.", // mesg
  1, // num_parms
  {"encod"} // parms
};
const ErrorInfo * const perror_encoding_not_supported = &perror_encoding_not_supported_obj;

static const ErrorInfo perror_conversion_not_supported_obj = {
  perror_encoding, // isa
  "The conversion from \"%encod:1\" to \"%encod2:2\" is not supported.", // mesg
  2, // num_parms
  {"encod", "encod2"} // parms
};
const ErrorInfo * const perror_conversion_not_supported = &perror_conversion_not_supported_obj;

static const ErrorInfo perror_pipe_obj = {
  0, // isa
  0, // mesg
  0, // num_parms
  {} // parms
};
const ErrorInfo * const perror_pipe = &perror_pipe_obj;

static const ErrorInfo perror_cant_create_pipe_obj = {
  perror_pipe, // isa
  0, // mesg
  0, // num_parms
  {} // parms
};
const ErrorInfo * const perror_cant_create_pipe = &perror_cant_create_pipe_obj;

static const ErrorInfo perror_process_died_obj = {
  perror_pipe, // isa
  0, // mesg
  0, // num_parms
  {} // parms
};
const ErrorInfo * const perror_process_died = &perror_process_died_obj;

static const ErrorInfo perror_bad_input_obj = {
  0, // isa
  0, // mesg
  0, // num_parms
  {} // parms
};
const ErrorInfo * const perror_bad_input = &perror_bad_input_obj;

static const ErrorInfo perror_invalid_word_obj = {
  perror_bad_input, // isa
  "The word \"%word:1\" is invalid.", // mesg
  1, // num_parms
  {"word"} // parms
};
const ErrorInfo * const perror_invalid_word = &perror_invalid_word_obj;

static const ErrorInfo perror_word_list_flags_obj = {
  perror_bad_input, // isa
  0, // mesg
  0, // num_parms
  {} // parms
};
const ErrorInfo * const perror_word_list_flags = &perror_word_list_flags_obj;

static const ErrorInfo perror_invalid_flag_obj = {
  perror_word_list_flags, // isa
  0, // mesg
  0, // num_parms
  {} // parms
};
const ErrorInfo * const perror_invalid_flag = &perror_invalid_flag_obj;

static const ErrorInfo perror_conflicting_flags_obj = {
  perror_word_list_flags, // isa
  0, // mesg
  0, // num_parms
  {} // parms
};
const ErrorInfo * const perror_conflicting_flags = &perror_conflicting_flags_obj;



}

