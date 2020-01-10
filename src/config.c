/*
 * Copyright 1997-2000 Phil Schwan <phil@off.net>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#else
#  error You did not run configure, did you?
#endif

#ifdef HAVE_STRING_H
#  include <string.h> /* needed everywhere */
#else
#  error The <string.h> header file is required to compile xferstats
#endif

#ifdef HAVE_CTYPE_H
#  include <ctype.h> /* needed for isdigit, tolower, isalpha */
#else
#  error The <ctype.h> header file is required to compile xferstats
#endif

#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

#include <glib.h>

#include "xferstats.h"

typedef struct
{
  char *command;
  void (*function)(pointers_t *, char *, char *, char *);
  char args;
} config_function;

/* prototypes for config functions */
static void config_logfile(pointers_t *, char *);
static void config_anon(pointers_t *);
static void config_guest(pointers_t *);
static void config_real(pointers_t *);
static void config_inbound(pointers_t *);
static void config_outbound(pointers_t *);
static void config_hourly(pointers_t *);
static void config_dow(pointers_t *);
static void config_dom(pointers_t *);
static void config_tld(pointers_t *);
static void config_domain(pointers_t *);
static void config_host(pointers_t *);
static void config_dir(pointers_t *);
static void config_file(pointers_t *);
static void config_monthly(pointers_t *);
static void config_size(pointers_t *);
static void config_html(pointers_t *, char *);
static void config_stdin(pointers_t *);
static void config_logtype(pointers_t *, char *);
static void config_dir_depth(pointers_t *, char *);
static void config_dir_filter(pointers_t *, char *);
/* void config_tld_filter(pointers_t *, char *); */
/* This will have to wait for another version */
static void config_number_file_stats(pointers_t *, char *);
static void config_number_dir_stats(pointers_t *, char *);
static void config_number_daily_stats(pointers_t *, char *);
static void config_number_tld_stats(pointers_t *, char *);
static void config_number_domain_stats(pointers_t *, char *);
static void config_number_host_stats(pointers_t *, char *);
static void config_max_report_size(pointers_t *, char *);
static void config_graph_path(pointers_t *, char *);
static void config_no_html_headers(pointers_t *);
static void config_single_page(pointers_t *, char *);
static void config_link_prefix(pointers_t *, char *);
static void config_additive_db(pointers_t *, char *);
static void config_strip_prefix(pointers_t *, char *);
static void config_file_sort_pref(pointers_t *, char *);
static void config_dir_sort_pref(pointers_t *, char *);
static void config_domain_sort_pref(pointers_t *, char *);
static void config_tld_sort_pref(pointers_t *, char *);
static void config_host_sort_pref(pointers_t *, char *);
static void config_dom_sort_pref(pointers_t *, char *);
static void config_dow_sort_pref(pointers_t *, char *);
static void config_hourly_sort_pref(pointers_t *, char *);
static void config_daily_sort_pref(pointers_t *, char *);
static void config_size_sort_pref(pointers_t *, char *);
static void config_monthly_sort_pref(pointers_t *, char *);
static void config_strict_check(pointers_t *);
static void config_begin_date(pointers_t *, char *);
static void config_end_date(pointers_t *, char *);
static void config_chunk_input(pointers_t *, char *);
static void config_refresh(pointers_t *, char *);

static config_function config_commands[] =
{
  {"LOGFILE", (void *)config_logfile, 1},
  {"ANON_TRAFFIC", (void *)config_anon, 0},
  {"GUEST_TRAFFIC", (void *)config_guest, 0},
  {"REAL_TRAFFIC", (void *)config_real, 0},
  {"INBOUND", (void *)config_inbound, 0},
  {"OUTBOUND", (void *)config_outbound, 0},
  {"HOURLY_REPORT", (void *)config_hourly, 0},
  {"DOW_REPORT", (void *)config_dow, 0},
  {"DOM_REPORT", (void *)config_dom, 0},
  {"TLD_REPORT", (void *)config_tld, 0},
  {"DOMAIN_REPORT", (void *)config_domain, 0},
  {"HOST_REPORT", (void *)config_host, 0},
  {"DIR_REPORT", (void *)config_dir, 0},
  {"FILE_REPORT", (void *)config_file, 0},
  {"MONTHLY_REPORT", (void *)config_monthly, 0},
  {"SIZE_REPORT", (void *)config_size, 0},
  {"HTML_OUTPUT", (void *)config_html, 1},
  {"USE_STDIN", (void *)config_stdin, 0},
  {"LOG_TYPE", (void *)config_logtype, 1},
  {"DIR_DEPTH", (void *)config_dir_depth, 1},
  {"DIR_FILTER", (void *)config_dir_filter, 1},
  /* {"TLD_FILTER", (void *)config_tld_filter, 1}, */
  /* This will have to wait for another version */
  {"NUMBER_FILE_STATS", (void *)config_number_file_stats, 1},
  {"NUMBER_DIR_STATS", (void *)config_number_dir_stats, 1},
  {"NUMBER_DAILY_STATS", (void *)config_number_daily_stats, 1},
  {"NUMBER_TLD_STATS", (void *)config_number_tld_stats, 1},
  {"NUMBER_DOMAIN_STATS", (void *)config_number_domain_stats, 1},
  {"NUMBER_HOST_STATS", (void *)config_number_host_stats, 1},
  {"MAX_REPORT_SIZE", (void *)config_max_report_size, 1},
  {"GRAPH_PATH", (void *)config_graph_path, 1},
  {"NO_HTML_HEADERS", (void *)config_no_html_headers, 0},
  {"SINGLE_PAGE", (void *)config_single_page, 1},
  {"LINK_PREFIX", (void *)config_link_prefix, 1},
  {"ADDITIVE_DB", (void *)config_additive_db, 1},
  {"STRIP_PREFIX", (void *)config_strip_prefix, 1},
  {"FILE_SORT_PREF", (void *)config_file_sort_pref, 1},
  {"DIR_SORT_PREF", (void *)config_dir_sort_pref, 1},
  {"DOMAIN_SORT_PREF", (void *)config_domain_sort_pref, 1},
  {"TLD_SORT_PREF", (void *)config_tld_sort_pref, 1},
  {"HOST_SORT_PREF", (void *)config_host_sort_pref, 1},
  {"DOM_SORT_PREF", (void *)config_dom_sort_pref, 1},
  {"DOW_SORT_PREF", (void *)config_dow_sort_pref, 1},
  {"HOURLY_SORT_PREF", (void *)config_hourly_sort_pref, 1},
  {"DAILY_SORT_PREF", (void *)config_daily_sort_pref, 1},
  {"SIZE_SORT_PREF", (void *)config_size_sort_pref, 1},
  {"MONTHLY_SORT_PREF", (void *)config_monthly_sort_pref, 1},
  {"STRICT_CHECK", (void *)config_strict_check, 0},
  {"BEGIN_DATE", (void *)config_begin_date, 1},
  {"END_DATE", (void *)config_end_date, 1},
  {"CHUNK_INPUT", (void *)config_chunk_input, 1},
  {"REFRESH", (void *)config_refresh, 1},
  {"", NULL, 0}
};


static void
config_logfile(pointers_t *pointers, char * logfile)
{
  pointers->config->logfiles = g_slist_append(pointers->config->logfiles,
					      g_strdup(logfile));
} /* config_logfile */


static void
config_anon(pointers_t *pointers)
{
  pointers->config->anon_traffic = 1;
} /* config_anon */


static void
config_guest(pointers_t *pointers)
{
  pointers->config->guest_traffic = 1;
} /* config_guest */


static void
config_real(pointers_t *pointers)
{
  pointers->config->real_traffic = 1;
} /* config_real */


static void
config_inbound(pointers_t *pointers)
{
  pointers->config->inbound = 1;
} /* config_inbound */


static void
config_outbound(pointers_t *pointers)
{
  pointers->config->outbound = 1;
} /* config_outbound */


static void
config_hourly(pointers_t *pointers)
{
  pointers->config->hourly_traffic = 1;
} /* config_hourly */


static void
config_dow(pointers_t *pointers)
{
  pointers->config->dow_traffic = 1;
} /* config_dow */


static void
config_dom(pointers_t *pointers)
{
  pointers->config->dom_traffic = 1;
} /* config_dom */


static void
config_tld(pointers_t *pointers)
{
  pointers->config->tld_traffic = 1;
} /* config_tld */


static void
config_domain(pointers_t *pointers)
{
  pointers->config->domain_traffic = 1;
} /* config_domain */


static void
config_host(pointers_t *pointers)
{
  pointers->config->host_traffic = 1;
} /* config_host */


static void
config_dir(pointers_t *pointers)
{
  pointers->config->dir_traffic = 1;
} /* config_dir */


static void
config_file(pointers_t *pointers)
{
  pointers->config->file_traffic = 1;
} /* config_file */


static void
config_monthly(pointers_t *pointers)
{
  pointers->config->monthly_traffic = 1;
} /* config_monthly */


static void
config_size(pointers_t *pointers)
{
  pointers->config->size_traffic = 1;
} /* config_size */


static void
config_html(pointers_t * pointers, char * html_location)
{
  pointers->config->html_output = 1;

  if (pointers->config->html_location != NULL)
    g_free(pointers->config->html_location);

  /* Add trailing '/' to html_location if it's not already there */
  if (html_location[strlen(html_location) - 1] != '/')
    {
      pointers->config->html_location = g_malloc(strlen(html_location) + 2);
      strcpy(pointers->config->html_location, html_location);
      strcat(pointers->config->html_location, "/");
    }
  else
    pointers->config->html_location = g_strdup(html_location);
} /* config_html */


static void
config_stdin(pointers_t *pointers)
{
  pointers->config->use_stdin = 1;
} /* config_stdin */


static void
config_logtype(pointers_t *pointers, char *typestr)
{
  int logtype;

  /* check text cases */
  if (!g_strcasecmp(typestr, "wu-ftpd") || !g_strcasecmp(typestr, "wuftpd")
      || !g_strcasecmp(typestr, "wu-ftp") || !g_strcasecmp(typestr, "wuftp"))
    {
      pointers->config->log_type = 0;
      return;
    }
  else if (!g_strcasecmp(typestr, "ncftpd") || !g_strcasecmp(typestr, "ncftp"))
    {
      pointers->config->log_type = 1;
      return;
    }
  else if (!g_strcasecmp(typestr, "apache"))
    {
      pointers->config->log_type = 2;
      return;
    }

  /* check numerical cases and whine if nothing valid was found */
  switch (logtype = atoi(typestr))
    {
    case 1:
    case 2:
    case 3:
      pointers->config->log_type = logtype - 1;
      break;
    default:
      fprintf(stderr, "xferstats: config_logtype: invalid LOG_TYPE (%s).\n",
	      typestr);
      exit(1);
    }
} /* config_logtype */


static void
config_dir_depth(pointers_t *pointers, char *depthstr)
{
  int depth = atoi(depthstr);

  if (depth > UCHAR_MAX)
    {
      pointers->config->depth = UCHAR_MAX;
      fprintf(stderr, "xferstats: config_dir_depth: warning: DIR_DEPTH reduced"
	      " to %d.\n", UCHAR_MAX);
    }
  else if ((pointers->config->depth = depth) < 1)
    {
      fprintf(stderr, "xferstats: config_dir_depth: DIR_DEPTH is too low or "
	      "invalid (%s).  DIR_DEPTH must be >= 1.\n", depthstr);
      exit (1);
    }
} /* config_dir_depth */


static void
config_dir_filter(pointers_t * pointers, char * path)
{
  only_dir_t * item;

  item = g_malloc(sizeof(only_dir_t));

  item->dir = g_malloc((item->len = strlen(path)) + 1);
  strcpy(item->dir, path);

  pointers->config->only_dir = g_slist_append(pointers->config->only_dir,
					      item);
} /* config_dir_filter */


#if 0
/* This will have to wait for another version */
static void
config_tld_filter(pointers_t *pointers, char *tld)
{
  if (pointers->config->only_tld != NULL)
    g_free(pointers->config->only_tld);

  pointers->config->only_tld = g_strdup(tld);
  pointers->config->only_tld_length = strlen(tld);
} /* config_tld_filter */
#endif


static void
config_number_file_stats(pointers_t *pointers, char *numstr)
{
  long number = atoi(numstr);

  if (number > INT_MAX)
    {
      pointers->config->number_file_stats = INT_MAX;
      fprintf(stderr,"xferstats: config_number_file_stats: warning: "
	      "NUMBER_FILE_STATS reduced to %d.\n", INT_MAX);
    }
  else if ((pointers->config->number_file_stats = number) < 0)
    {
      fprintf(stderr, "xferstats: config_number_file_stats: NUMBER_FILE_STATS "
	      "is too low or invalid.  It must be >= 0.\n");
      exit(1);
    }
} /* config_number_file_stats */


static void
config_number_dir_stats(pointers_t *pointers, char *numstr)
{
  long number = atoi(numstr);

  if (number > INT_MAX)
    {
      pointers->config->number_dir_stats = INT_MAX;
      fprintf(stderr,"xferstats: config_number_dir_stats: warning: "
	      "NUMBER_DIR_STATS reduced to %d.\n", INT_MAX);
    }
  else if ((pointers->config->number_dir_stats = number) < 0)
    {
      fprintf(stderr, "xferstats: config_number_dir_stats: NUMBER_DIR_STATS "
	      "is too low or invalid.  It must be >= 0.\n");
      exit(1);
    }
} /* config_number_dir_stats */


static void
config_number_daily_stats(pointers_t *pointers, char *numstr)
{
  long number = atoi(numstr);

  if (number > INT_MAX)
    {
      pointers->config->number_daily_stats = INT_MAX;
      fprintf(stderr,"xferstats: config_number_daily_stats: warning: "
	      "NUMBER_DAILY_STATS reduced to %d.\n", INT_MAX);
    }
  else if ((pointers->config->number_daily_stats = number) < 0)
    {
      fprintf(stderr, "xferstats: config_number_daily_stats: "
	      "NUMBER_DAILY_STATS is too low or invalid.  It must be >= 0.\n");
      exit(1);
    }
} /* config_number_daily_stats */


static void
config_number_tld_stats(pointers_t *pointers, char *numstr)
{
  long number = atoi(numstr);

  if (number > INT_MAX)
    {
      pointers->config->number_tld_stats = INT_MAX;
      fprintf(stderr,"xferstats: config_number_tld_stats: warning: "
	      "NUMBER_TLD_STATS reduced to %d.\n", INT_MAX);
    }
  else if ((pointers->config->number_tld_stats = number) < 0)
    {
      fprintf(stderr, "xferstats: config_number_tld_stats: NUMBER_TLD_STATS "
	      "is too low or invalid.  It must be >= 0.\n");
      exit(1);
    }
} /* config_number_tld_stats */


static void
config_number_domain_stats(pointers_t *pointers, char *numstr)
{
  long number = atoi(numstr);

  if (number > INT_MAX)
    {
      pointers->config->number_domain_stats = INT_MAX;
      fprintf(stderr,"xferstats: config_number_domain_stats: warning: "
	      "NUMBER_DOMAIN_STATS reduced to %d.\n", INT_MAX);
    }
  else if ((pointers->config->number_domain_stats = number) < 0)
    {
      fprintf(stderr, "xferstats: config_number_domain_stats: "
	      "NUMBER_DOMAIN_STATS is too low or invalid.  It must be >= "
	      "0.\n");
      exit(1);
    }
} /* config_number_domain_stats */


static void
config_number_host_stats(pointers_t *pointers, char *numstr)
{
  long number = atoi(numstr);

  if (number > INT_MAX)
    {
      pointers->config->number_host_stats = INT_MAX;
      fprintf(stderr,"xferstats: config_number_host_stats: warning: "
	      "NUMBER_HOST_STATS reduced to %d.\n", INT_MAX);
    }
  else if ((pointers->config->number_host_stats = number) < 0)
    {
      fprintf(stderr, "xferstats: config_number_host_stats: NUMBER_HOST_STATS"
	      " is too low or invalid.  It must be >= 0.\n");
      exit(1);
    }
} /* config_number_host_stats */


static void
config_max_report_size(pointers_t *pointers, char *numstr)
{
  long number = atoi(numstr);

  if (number > INT_MAX)
    {
      pointers->config->max_report_size = INT_MAX;
      fprintf(stderr,"xferstats: config_max_report_size: warning: "
	      "MAX_REPORT_SIZE reduced to %d.\n", INT_MAX);
    }
  else if ((pointers->config->max_report_size = number) < 0)
    {
      fprintf(stderr, "xferstats: config_max_report_size: MAX_REPORT_SIZE is "
	      "too low or invalid.  It must be >= 0.\n");
      exit(1);
    }
} /* config_max_report_size */


static void
config_graph_path(pointers_t * pointers, char * path)
{
  if (pointers->config->graph_path)
    g_free(pointers->config->graph_path);

  pointers->config->graph_path = g_strdup(path);
} /* config_graph_path */


static void
config_no_html_headers(pointers_t *pointers)
{
  pointers->config->no_html_headers = 1;
} /* config_no_html_headers */


static void
config_single_page(pointers_t *pointers, char *filename)
{
  if (pointers->config->single_page != NULL)
    g_free(pointers->config->single_page);

  pointers->config->single_page = g_strdup(filename);
} /* config_single_page */


static void
config_link_prefix(pointers_t * pointers, char * prefix)
{
  if (pointers->config->link_prefix != NULL)
    g_free(pointers->config->link_prefix);

  pointers->config->link_prefix = g_strdup(prefix);
} /* config_link_prefix */


static void
config_additive_db(pointers_t * pointers, char * path)
{
  if (pointers->config->additive_db != NULL)
    g_free(pointers->config->additive_db);

  /* Add trailing '/' to the path if it's not already there */
  if (path[strlen(path) - 1] != '/')
    {
      pointers->config->additive_db = g_malloc(strlen(path) + 2);
      strcpy(pointers->config->additive_db, path);
      strcat(pointers->config->additive_db, "/");
    }
  else
    pointers->config->additive_db = g_strdup(path);
} /* config_additive_db */


static void
config_strip_prefix(pointers_t * pointers, char * prefix)
{
  if (pointers->config->strip_prefix != NULL)
    g_free(pointers->config->strip_prefix);

  pointers->config->strip_prefix =
    g_malloc((pointers->config->strip_prefix_len = strlen(prefix)) + 1);
  strcpy(pointers->config->strip_prefix, prefix);
} /* config_strip_prefix */


static void
config_file_sort_pref(pointers_t * pointers, char * pref)
{
  pointers->config->file_sort_pref = (char) atoi(pref);
} /* config_file_sort_pref */


static void
config_dir_sort_pref(pointers_t * pointers, char * pref)
{
  pointers->config->dir_sort_pref = (char) atoi(pref);
} /* config_dir_sort_pref */


static void
config_domain_sort_pref(pointers_t * pointers, char * pref)
{
  pointers->config->domain_sort_pref = (char) atoi(pref);
} /* config_domain_sort_pref */


static void
config_tld_sort_pref(pointers_t * pointers, char * pref)
{
  pointers->config->tld_sort_pref = (char) atoi(pref);
} /* config_tld_sort_pref */


static void
config_host_sort_pref(pointers_t * pointers, char * pref)
{
  pointers->config->host_sort_pref = (char) atoi(pref);
} /* config_host_sort_pref */


static void
config_dom_sort_pref(pointers_t * pointers, char * pref)
{
  pointers->config->dom_sort_pref = (char) atoi(pref);
} /* config_dom_sort_pref */


static void
config_dow_sort_pref(pointers_t * pointers, char * pref)
{
  pointers->config->dow_sort_pref = (char) atoi(pref);
} /* config_dow_sort_pref */


static void
config_hourly_sort_pref(pointers_t * pointers, char * pref)
{
  pointers->config->hourly_sort_pref = (char) atoi(pref);
} /* config_hourly_sort_pref */


static void
config_daily_sort_pref(pointers_t * pointers, char * pref)
{
  pointers->config->daily_sort_pref = (char) atoi(pref);
} /* config_daily_sort_pref */


static void
config_size_sort_pref(pointers_t * pointers, char * pref)
{
  pointers->config->size_sort_pref = (char) atoi(pref);
} /* config_size_sort_pref */


static void
config_monthly_sort_pref(pointers_t * pointers, char * pref)
{
  pointers->config->monthly_sort_pref = (char) atoi(pref);
} /* config_monthly_sort_pref */


static void
config_strict_check(pointers_t * pointers)
{
  pointers->config->strict_check = 1;
} /* config_strict_check */


static void
config_begin_date(pointers_t * pointers, char * bar)
{
  if (strlen(bar) != 20)
    {
      fprintf(stderr, "fatal: the BEGIN_DATE must be in the "
	      "Jan DD HH:MM:ss YYYY format\n");
      exit(1);
    }

  if (bar[3] != ' ' || bar[6] != ' ' || bar[15] != ' ' || bar[9] != ':' ||
      bar[12] != ':' || !isdigit(bar[5]) || !isdigit(bar[7]) ||
      !isdigit(bar[8]) || !isdigit(bar[10]) || !isdigit(bar[11]) ||
      !isdigit(bar[13]) || !isdigit(bar[14]) || !isdigit(bar[16]) ||
      !isdigit(bar[17]) || !isdigit(bar[18]) || !isdigit(bar[19]) ||
      !(bar[4] == ' ' || bar[4] == '0' || bar[4] == '1' || bar[4] == '2' ||
	bar[4] == '3'))
    {
      fprintf(stderr, "fatal: the BEGIN_DATE must be in the "
	      "Jan DD HH:MM:ss YYYY format\n");
      exit(1);
    }
  memset(pointers->config->begin_date, ' ', 4);
  memcpy(pointers->config->begin_date + 4, bar, 21);
} /* config_begin_date */


static void
config_end_date(pointers_t * pointers, char * bar)
{
  if (strlen(bar) != 20)
    {
      fprintf(stderr, "fatal: the END_DATE must be in the "
	      "Jan DD HH:MM:ss YYYY format\n");
      exit(1);
    }

  if (bar[3] != ' ' || bar[6] != ' ' || bar[15] != ' ' || bar[9] != ':' ||
      bar[12] != ':' || !isdigit(bar[5]) || !isdigit(bar[7]) ||
      !isdigit(bar[8]) || !isdigit(bar[10]) || !isdigit(bar[11]) ||
      !isdigit(bar[13]) || !isdigit(bar[14]) || !isdigit(bar[16]) ||
      !isdigit(bar[17]) || !isdigit(bar[18]) || !isdigit(bar[19]) ||
      !(bar[4] == ' ' || bar[4] == '0' || bar[4] == '1' || bar[4] == '2' ||
	bar[4] == '3'))
    {
      fprintf(stderr, "fatal: the END_DATE must be in the "
	      "Jan DD HH:MM:ss YYYY format\n");
      exit(1);
    }
  memset(pointers->config->end_date, ' ', 4);
  memcpy(pointers->config->end_date + 4, bar, 21);
} /* config_end_date */


static void
config_chunk_input(pointers_t * pointers, char * lines)
{
  pointers->config->chunk_input = atoi(lines);
} /* config_chunk_input */


static void
config_refresh(pointers_t * pointers, char * refresh)
{
  pointers->config->refresh = atoi(refresh);
} /* config_refresh */


/* parse_config_line returns 0 if there is no valid data, 1 if there is only a
 * valid "command", 2 if there is a valid "command" and a valid "arg1", 3 if
 * there is also a valid "arg2", and 4 if the command and all 3 args are
 * valid */
static int
parse_config_line(char *line, char **command, char *args[3])
{
  int arg_no = 0;

  *command = NULL; args[0] = NULL; args[1] = NULL; args[2] = NULL;
  /* zip past whitespace */
  while (*line == ' ') line++;
  
  /* see if there's anything left to parse */
  if (*line == '#' || *line == '\0' || *line == '\n')
    return 0;

  /* set the beginning of "command" */
  *command = line;
  /* don't care about #s unless they're prepended by a space */
  while (*line != ' ' && *line != '\0' && *line != '\n') line++;
  /* it's possible that there was no terminating carriage return.  if we find
   * a null, terminate "command" and return */
  if (*line == '\0')
    return 1;
  else if (*line == '\n')
    {
      /* terminate "command" */
      *line = '\0';
      return 1;
    }
  /* terminate "command" */
  *line = '\0';

  /* loop until we reach the end of the line or we have 3 arguments
   * (our maximum) */
  for (; arg_no <= 2; arg_no++)
    {
      /* set the beginning of arg */
      args[arg_no] = ++line;
      /* don't care about #s unless they're prepended by a space */
      while (*line != ' ' && *line != '\0' && *line != '\n') line++;
      /* catch nulls again... */
      if (*line == '\0')
	return (arg_no + 2);
      else if (*line == '\n')
	{
	  /* terminate arg */
	  *line = '\0';
	  return (arg_no + 2);
	}
      /* terminate arg */
      *line = '\0';
    }

  fprintf(stderr, "\"The obvious mathematical breakthrough would be "
	  "development of an easy way to factor large prime numbers.\" -- "
	  "Bill Gates, The Road Ahead, 1995");
  fprintf(stderr, "WARNING: something is whacked in parse_config_line, you "
	  "should never see this.\n");
  return 0;
} /* parse_config_line */


int
init_config(pointers_t *pointers)
{
  char tmp_str[1024], *command, *args[3];
  FILE *file_ptr = NULL;
  int index, valid;
  char *upcase_char;
  
  if (pointers->config->config_file == NULL)
    {
      if ((file_ptr = fopen(DEFAULT_CONFIG, "r")) == NULL)
	{
	  fprintf(stderr, "xferstats: init_config: %s: ", DEFAULT_CONFIG);
	  perror("fopen");
	  return 0;
	  exit(1);
	}
    }
  else
    if ((file_ptr = fopen(pointers->config->config_file, "r")) == NULL)
      {
	fprintf(stderr, "xferstats: init_config: %s: ",
		pointers->config->config_file);
	perror("fopen");
	return 0;
	exit(1);
      }

  while (!feof(file_ptr))
    {
      tmp_str[0] = '\0';
      fgets(tmp_str, sizeof(tmp_str) - 1, file_ptr);
#ifdef DEBUG
      fprintf(stderr, "init_config: RAW: %s", tmp_str);
#endif

      /* parse the config line into 'command' and 'args' */
      if ((valid = parse_config_line(tmp_str, &command, args) - 1) < 0)
	/* parse_config_line couldn't find a 'command' let alone args...move
	 * along... */
	continue;

#ifdef DEBUG
      fprintf(stderr, "command: %s arg1: %s arg2: %s arg3: %s\n",
	     command ? command : "(nil)",
	     args[0] ? args[0] : "(nil)",
	     args[1] ? args[1] : "(nil)",
	     args[2] ? args[2] : "(nil)");
#endif

      /* convert all to upper case */
      for (upcase_char = command;
	   (*upcase_char = toupper((int)*upcase_char)); upcase_char++);

      /* scan the table to see if this matches any of the commands */
      for (index = 0; config_commands[index].function != NULL; index++)
	if (!strcmp(config_commands[index].command, command))
	  {
	    /* correct # of args? */
	    if (valid == config_commands[index].args)
	      switch(valid)
		{
		case 0:
		  config_commands[index].function(pointers, NULL, NULL, NULL);
		  break;
		case 1:
		  config_commands[index].function(pointers, args[0], NULL,
						  NULL);
		  break;
		case 2:
		  config_commands[index].function(pointers, args[0], args[1],
						  NULL);
		  break;
		case 3:
		  config_commands[index].function(pointers, args[0], args[1],
						  args[2]);
		  break;
		}
	    else
	      /* incorrect # of args, whine about it */
	      fprintf(stderr, "xferstats: warning: bad config line \"%s\" "
		      "(wrong # of args)\n", tmp_str);
	    break;
	  }

      /* command wasn't found, whine about it */
      if (config_commands[index].function == NULL)
	fprintf(stderr, "xferstats: warning: bad config line \"%s\" (unknown "
		"config command)\n", tmp_str);
    }

  fclose(file_ptr);
  return 0;
} /* init_config */
