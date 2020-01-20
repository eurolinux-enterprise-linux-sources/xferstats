#ifndef _XFERSTATS_H
#define _XFERSTATS_H

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

#include <stdlib.h>
#include <stdio.h>

#include <glib.h>

#include "config.h"

#ifdef BROKEN_LL
#  define LL_FMT "lu"
#else
#  ifdef IS_DEC
#    define LL_FMT "Lu"
#  else
#    define LL_FMT "llu"
#  endif
#endif

/* glibc's malloc is so damn efficient, chunks actually slow it down.  so only
 * use g_mem_chunks on non-glibc systems */
#ifdef __GLIBC__
#  define G_MEM_CHUNK_ALLOC(bar) g_malloc(bar)
#  define G_MEM_CHUNK_ALLOC0(bar) g_malloc0(bar)
#  define G_MEM_CHUNK_NEW(foo, bar, baz, whee) bar
#  define G_BLOW_CHUNKS() ;
#  define G_MEM_CHUNK_FREE(foo, bar) g_free(bar)
#else
#  define G_MEM_CHUNK_ALLOC g_mem_chunk_alloc
#  define G_MEM_CHUNK_ALLOC0 g_mem_chunk_alloc0
#  define G_MEM_CHUNK_NEW g_mem_chunk_new
#  define G_BLOW_CHUNKS g_blow_chunks
#  define G_MEM_CHUNK_FREE g_mem_chunk_free
#endif

#define VERSION "2.16"

/* Also self-explanitory.  The default place to look for the xferstats config
 * information */
#define DEFAULT_CONFIG "/usr/local/etc/xferstats.cfg"

/* The default xferlog filename */
#define FILENAME "/var/log/xferlog"

/* 64 and 1024 are the wu-ftpd default values and unless you have some
 * reason to change them, I'd leave them alone.  If you do decide to change
 * them, MAXPATHLEN -must- be larger than MAXHOSTNAMELEN. */
#define MAXHOSTNAMELEN 64
#ifdef PATH_MAX
#  define MAXPATHLEN PATH_MAX
#else /* PATH_MAX */
#  define MAXPATHLEN 1024
#endif /* PATH_MAX */

/* my types */
enum
{
  T_FILE,
  T_DIR,
  T_HOST,
  T_TLD,
  T_DOMAIN,
  T_SIZE
};

/* struct used for file, directory, host, domain, and top-level domain data */
typedef struct _xfmisc_t
{
  char * name;
  unsigned LONGLONG data;
  unsigned LONGLONG seconds;
  unsigned int file_count;
  unsigned int completed_count;
} xfmisc_t;

typedef struct _only_dir_t
{
  char * dir;
  unsigned short len; /* to avoid having to strlen it a zillion times */
} only_dir_t;

typedef struct _ftp_entry_t
{
  /* the date the file was ftped */
  char date[25];

  /* the time it took to transfer the file (in seconds) */
  long seconds;

  /* pointer to the hostname of the ftper */
  char * host;

  /* the size of the file (in bytes, of course) */
  unsigned long data;

  /* pointer to the path and filename */
  char * path;
  
  /* if the file was transferred completely, this is 'c', else 'i' */
  char complete;

  /* pointer to the next file */
  struct _ftp_entry_t * next_ptr;
} ftp_entry_t;

typedef struct _config_t
{
  /* list of logfiles to be processed */
  GSList * logfiles;

  /* config file name */
  char * config_file;

  /* report on files ftped by real users.  0 == no */
  char real_traffic;

  /* report on files ftped by anonymous users.  0 == no */
  char anon_traffic;
  
  /* report on files ftped by guest users.  0 == no */
  char guest_traffic;
  
  /* report on inbound files (products of a PUT) */
  char inbound;

  /* report on outbound files (products of a GET) */
  char outbound;

  /* generate hourly reports.  0 == no */
  char hourly_traffic;

  /* generate day of the week reports.  0 == no */
  char dow_traffic;
  
  /* generate day of the month reports.  0 == no */
  char dom_traffic;
  
  /* generate domain reports.  0 == no */
  char domain_traffic;
  
  /* generate top-level domain reports.  0 == no */
  char tld_traffic;
  
  /* generate hosts reports.  0 == no */
  char host_traffic;
  
  /* generate total traffic by directory reports.  0 == no */
  char dir_traffic;
  
  /* generate file traffic report.  0 == no */
  char file_traffic;
  
  /* generate monthly traffic report.  0 == no */
  char monthly_traffic;
  
  /* generate file-size report.  0 == no */
  char size_traffic;
  
  /* generate HTML output.  0 == no */
  char html_output;

  /* the path for the HTML.  NULL == current directory */
  char * html_location;

  /* graph path */
  char * graph_path;

  /* get the log from the standard input.  0 == no */
  char use_stdin;
  
  /* the log type being processed (0 == wu-ftpd, 1 == ncftpd) */
  unsigned char log_type;
  
  /* how deep in the directory tree should the total report by section
     compare? */
  char depth;
  
  /* what section should be displayed.  NULL == all sections */
  GSList * only_dir;

  /* what domain should be displayed.  NULL == all domains */
  /* char *only_tld; */
  /* to avoid having to strlen it a zillion times */
  /* int only_tld_length; */
  /* This will have to wait for another version. */
  
  /* how many files should be reported? */
  int number_file_stats;
  
  /* how many days should be listed in the daily stats? */
  int number_daily_stats;
  
  /* how many sections should be listed in the total report by section? */
  int number_dir_stats;
  
  /* how many top level domains should be listed in the TLD statistics? */
  int number_tld_stats;

  /* how many domains should be listed in the domain statistics? */
  int number_domain_stats;

  /* how many hosts should be listed in the domain statistics? */
  int number_host_stats;

  /* what's the max # of records in any one report? (does not apply to
   * hourly, day-of-the-week, day-of-the-month, or yearly reports)  Must
   * be a number larger than 9 (or 0 to disable the limit) */
  int max_report_size;

  /* if nonzero, write all reports to stdout instead of their respective
   * individual files */
  char stdout_output;

  /* if nonzero, don't put <HTML><BODY> ... </BODY></HTML> bits in HTML
   * output */
  char no_html_headers;

  /* if non-NULL, output everything as one HTML page, without links, into
   * the filename specified (defaults to xferstats.html)
   *
   * single_page implies no_html_headers automatically.*/
  char *single_page;

  /* if non-NULL, this will be prepended to every path and filename in HTML
   * output to make each file/directory a hyperlink to the actual
   * file/directory */
  char * link_prefix;

  /* If non-NULL, xferstats will read databases from this directory (if they
   * exist), append data from the logfile being parsed, and write updated
   * databases into this directory.  The practical upshot of this is that one
   * can run xferstats multiple times to add logfiles to the totals while not
   * having to re-parse (or even keep) previous logfiles. */
  char * additive_db;

  /* The string to strip from the front of paths. */
  char * strip_prefix;
  unsigned short strip_prefix_len;

  /* How should xferstats sort the various reports?
   *
   * 0 -- by number of bytes downloaded
   * 1 -- by number of files downloaded
   * 2 -- by "name".  This means different things depending on the usage.  For
   *      the hourly report, it lists the hours of the day in order.  For the
   *      file size report, it lists the sizes in order.  You get the idea.
   */
  char file_sort_pref;
  char dir_sort_pref;
  char domain_sort_pref;
  char tld_sort_pref;
  char host_sort_pref;
  char dom_sort_pref;
  char dow_sort_pref;
  char hourly_sort_pref;
  char daily_sort_pref;
  char size_sort_pref;
  char monthly_sort_pref;

  /* Should we strictly check the log file to try to weed out corrupt lines? */
  char strict_check;

  /* filter all traffic before this date */
  char begin_date[25];
  /* filter all traffic after this date */
  char end_date[25];

  int chunk_input;

  unsigned int refresh;
} config_t;

typedef struct
{
#ifdef __GLIBC__
  size_t xfmisc_chunk;
  size_t ftpentry_chunk;
#else
  GMemChunk * xfmisc_chunk;
  GMemChunk * ftpentry_chunk;
#endif

  /* the logfile ptr */
  FILE * file_ptr;

  /* configuration data */
  config_t * config;

  /* the start of all of the data */
  ftp_entry_t * first_ftp_line;
  xfmisc_t ** first_day_ptr;
  void ** first_tld_ptr;
  void ** first_domain_ptr;
  void ** first_host_ptr;
  void ** first_file_ptr;
  void ** first_dir_ptr;
  void ** first_size_ptr;
  xfmisc_t ** hour_data;
  xfmisc_t ** weekday_data;
  xfmisc_t ** dom_data;
  xfmisc_t ** month_data;

  /* counters for global totals */
  unsigned long file_count;
  unsigned long complete_count;
  unsigned LONGLONG data;

  /* the first and last day that we have data for */
  char low_date[25];
  char high_date[25];

  GHashTable * file_ht;
  GHashTable * dir_ht;
  GHashTable * host_ht;
  GHashTable * tld_ht;
  GHashTable * domain_ht;
  GHashTable * size_ht;

  char done;
} pointers_t;

/* prototypes */
/* config.c */
int init_config(pointers_t * pointers);

/* display.c */
void display_daily_totals(pointers_t * pointers);
void display_misc_totals(pointers_t * pointers, int type, char * htmlname);
void display_hourly_totals(pointers_t * pointers);
void display_monthly_totals(pointers_t * pointers);
void display_dow_totals(pointers_t * pointers);
void display_dom_totals(pointers_t * pointers);

/* parselog.c */
int parse_wuftpd_logfile(pointers_t * pointers);
int parse_ncftpd_logfile(pointers_t * pointers);
int parse_apache_logfile(pointers_t * pointers);

/* xferstats.c */
inline int compare_dates(char date1[25], char date2[25]);

#endif /* _XFERSTATS_H */
