/*
	generic
	Written by Mike Perron (2012-2013)

	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at http://mozilla.org/MPL/2.0/.

	General functions to patch things together for krakws.
*/
#ifndef KWS_GENERIC_F
#define KWS_GENERIC_F

#define KWS_FREE(x) (free(x), (x) = NULL)

#define WORDS_DELIMINATOR " \t\r\n"

// String functions
extern char **chop_words(const char *src);
extern void unescape_url(char *url);
extern void sanitize_str(char *str);
extern void unquote_str(char *str);

// Config file parsing
enum SEEK_MODE {
	SEEK_FORWARD_ONLY,
	SEEK_RESET_OK,
	SEEK_POST_REWIND // Do not use.
};
extern FILE *get_conf_stream(char *cname, const char *mode);
extern char *get_conf_line(char *fname, char *value);
extern char *get_conf_line_s(FILE *stream, char *value, enum SEEK_MODE mode);

// Write an error to a FILE stream.
enum debug_stream_op { GET, SET };
extern FILE *mod_debug_stream(enum debug_stream_op op, FILE *stream);
extern int error_code(int code, const char *msg, ...);

// char* stack.
enum stack_operation { PUSH, POP, GET_STACK, COUNT };
extern void push_str(char *str);
extern char *pop_str();
extern char **get_str_stack();
extern long get_stack_size();

// Kraknet Stuff.
extern char *mod_home(char *mod);
extern int mod_find(char *mod, char *script, char *args);
extern int mod_find_p(char *mod, char *script, char *args, char **ret);
extern int mod_find_ps(char *mod_script, char *args, char **ret);

// Misc.
extern int kws_fclose(FILE **stream);

#endif
