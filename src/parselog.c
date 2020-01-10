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

#ifdef HAVE_SYS_TYPES_H /* required for FreeBSD to work */
#  include <sys/types.h>
#endif

#ifdef HAVE_STRING_H
#  include <string.h>
#endif

#ifdef HAVE_MMAP
#  include <sys/mman.h>
#endif

#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif

#ifdef HAVE_CTYPE_H
#  include <ctype.h>
#endif

#include <sys/stat.h>
#include <time.h>

#include <stdio.h>
#include <stdlib.h>
#include "xferstats.h"

extern char MONTHS[12][4];


/*
 * Yanked from phhttpd, copyright (C) 1999, 2000 Red Hat Inc, by Zach Brown
 *
 * This follows the rules laid out in rfc 1808 for resolving
 * relative URLS:
 *
 *          a) All occurrences of "./", where "." is a complete path
 *             segment, are removed.
 *
 *          b) If the path ends with "." as a complete path segment,
 *             that "." is removed.
 *
 *          c) All occurrences of "<segment>/../", where <segment> is a
 *             complete path segment not equal to "..", are removed.
 *             Removal of these path segments is performed iteratively,
 *             removing the leftmost matching pattern on each iteration,
 *             until no matching pattern remains.
 *
 *          d) If the path ends with "<segment>/..", where <segment> is a
 *             complete path segment not equal to "..", that
 *             "<segment>/.." is removed.
 *
 * This is a one pass state machine.  We copy each character as we go.
 * when we hit a sequence of characters we know should be removed,
 * we rewind the destination pointer and carry on.
 */
static void copy_clean_path(char *output, char *string, int left)
{
        int oind = 0;
        char *s = string;
        int start = 1;          /* if > 0, the current char is the start of a path component */
        int inmagic = 0;        /* 1 for ".", 2 for ".." */

        for(; left > 0 ; start--, s++) {
                if( *s == '.' ) {
                        /*
                         * alter our inmagic state depending on the
                         * makeup of the current path component 
                         */
                        if(start > 0) 
				/* we're the first char of a magic component */
                                inmagic++;
                        else 
				/* ok, we're the second '.' in ".." */
                                if(inmagic) inmagic++;

                        if(inmagic == 3) inmagic=0;
                } else 
                if( *s == '\0' || *s == '/' ) {
                        /*
                         * ok, we've finished a path component.  if it was magical, we have
                         * to undo it in the destination string
                         */
			if (start > 0) {
				/* we just saw a '/', drop it on the floor */
				oind--;
			}
                        start = 2; /* next char will start a path component */
                        if(inmagic) {
                                oind -= inmagic; /* skip back over "." or ".." */
                                if(inmagic == 2 && oind > 1) {
                                        /*
                                         * alrighty, we're a ".." and there was a previous path
                                         * component.  Lets nuke it.  Notice that the "> 1"s here
                                         * mean "/../" will turn into "/", this is vague in the
                                         * spec so I'm going with it.
                                         */
                                        oind-=2; /* skip to before the seperating "/" */
                                        for(;;) {
                                                 /* walk back through the previous component */
                                                if(oind && (output[oind-1] == '/')) break;
                                                if(!oind) break;
                                                oind--;
                                        }
                                }
                                inmagic = 0; /* ok, we're not magical anymore  */
                                if(*s == '/') continue; /* the trailing '/' is always ignored */
                        }
                } else inmagic=0; /* we're a normal char, no more magic */

                if( (output[oind++] = *(s)) == '\0') break;
                left--;
        }
        output[oind] = '\0'; /* paranoia */
}


int
dir_in_list(GSList * list, char * path)
{
  GSList * clist;

  if (!list)
    return 0;

  for (clist = list; clist; clist = clist->next)
    if (!strncmp(((only_dir_t *)clist->data)->dir, path,
		 ((only_dir_t *)clist->data)->len))
      return 1;

  return 0;
} /* dir_in_list */


int
parse_wuftpd_logfile(pointers_t * pointers)
{
  static ftp_entry_t * ftp_line, * current_ftp_ptr = NULL;
  static char foo[2048], tmp_host[2048], tmp_path[2048], user_type, in_out,
    * logfile;
  static int len;
  static GSList * clist = NULL;
  static FILE * log_stream = NULL;
  int lines = 0;

#ifdef DEBUGS
  fprintf(stderr, "Beginning parse_wuftpd_logfile...(%ld)\n",
	  (long) time(NULL));
#endif

  if (!clist) /* in theory this is only true once */
    clist = pointers->config->logfiles;

  if (pointers->first_ftp_line) {
	  for (ftp_line = pointers->first_ftp_line; ftp_line; ) {
		  current_ftp_ptr = ftp_line->next_ptr;
		  g_free(ftp_line->host);
		  g_free(ftp_line->path);
		  g_free(ftp_line);
		  ftp_line = current_ftp_ptr;
	  }
	  pointers->first_ftp_line = NULL;
  }

  for (; clist; clist = clist->next) {
	  logfile = clist->data;

    if (!pointers->config->use_stdin && !log_stream)
      if (!(log_stream = fopen(logfile, "r"))) {
	perror("parse_wuftpd_logfile: fopen");
	exit(1);
      }
    
    while (1) {
      if (pointers->config->use_stdin) {
	if (feof(stdin))
	  break;
	/* there's probably a better way to do this :) */
	fgets(foo, sizeof(foo), stdin);
      } else {
	if (feof(log_stream))
	  break;
	
	fgets(foo, sizeof(foo), log_stream);
      }
      
      if ((len = strlen(foo)) < 42)
	continue; /* data is too small to be valid */
#ifdef DEBUG3
      fprintf(stderr, "RAW READ: %s\n", foo);
#endif
      ftp_line = G_MEM_CHUNK_ALLOC(pointers->ftpentry_chunk);
      
      ftp_line->date[24] = '\0';
      memcpy(ftp_line->date, foo, 24);
      ftp_line->seconds = -1;
      ftp_line->data = 0;
      ftp_line->next_ptr = NULL;
      if ((foo[len - 2] == 'c' || foo[len - 2] == 'i') &&
	  foo[len - 3] == ' ')
	/* this check isn't foolproof, but it's probably good enough for
	 * now.  if it succeeds, this xferlog has a complete/incomplete
	 * byte at the end of the line */
	sscanf(foo + 25, "%ld %s %lu %s %*c %*s %c %c %*s %*s %*d %*s %c",
	       &ftp_line->seconds, tmp_host, &ftp_line->data, tmp_path,
	       &in_out, &user_type, &ftp_line->complete);
      else {
	sscanf(foo + 25, "%ld %s %lu %s %*c %*s %c %c",
	       &ftp_line->seconds, tmp_host, &ftp_line->data, tmp_path,
	       &in_out, &user_type);
	ftp_line->complete = 'c';
      }
      
      if (ftp_line->date[0] == '\0' || tmp_host[0] == '\0' ||
	  tmp_path[0] == '\0' || ftp_line->data == 0 ||
	  ftp_line->seconds < 0 ||
	  (user_type == 'a' && !pointers->config->anon_traffic) ||
	  (user_type == 'g' && !pointers->config->guest_traffic) ||
	  (user_type == 'r' && !pointers->config->real_traffic) ||
	  (in_out == 'i' && !pointers->config->inbound) ||
	  (in_out == 'o' && !pointers->config->outbound) ||
	  (pointers->config->only_dir &&
	   !dir_in_list(pointers->config->only_dir, tmp_path))) {
#ifdef DEBUG3
	if (ftp_line->date[0] == '\0')
	  fprintf(stderr, "Discarding -- no date.\n");
	else if (tmp_host[0] == '\0')
	  fprintf(stderr, "Discarding -- no host.\n");
	else if (tmp_path[0] == '\0')
	  fprintf(stderr, "Discarding -- no path.\n");
	else if (ftp_line->data == 0)
	  fprintf(stderr, "Discarding -- no data.\n");
	else if (ftp_line->seconds < 0)
	  fprintf(stderr, "Discarding -- no seconds.\n");
	else if (user_type == 'a' && !pointers->config->anon_traffic)
	  fprintf(stderr, "Discarding -- don't want anon traffic\n");
	else if (user_type == 'r' && !pointers->config->real_traffic)
	  fprintf(stderr, "Discarding -- don't want real traffic\n");
	else if (user_type == 'g' && !pointers->config->guest_traffic)
	  fprintf(stderr, "Discarding -- don't want guest traffic\n");
	else if (in_out == 'i' && !pointers->config->inbound)
	  fprintf(stderr, "Discarding -- don't want inbound traffic\n");
	else if (in_out == 'o' && !pointers->config->outbound)
	  fprintf(stderr, "Discarding -- don't want outbound traffic\n");
	else if (pointers->config->only_dir &&
		 !dir_in_list(pointers->config->only_dir, tmp_path))
	  fprintf(stderr, "Discarding -- don't want this directory\n");
	else
	  fprintf(stderr, "Discarding -- no clue why.\n");
#endif
#ifndef SMFS
	G_MEM_CHUNK_FREE(pointers->ftpentry_chunk, ftp_line);
#endif
	continue; /* the line is invalid */
      }
      
      if (pointers->config->strict_check)
	/* These checks can significantly slow processing down, so they're
	 * highly optional. */
	if (ftp_line->date[3] != ' ' || ftp_line->date[7] != ' ' ||
	    ftp_line->date[10] != ' ' || ftp_line->date[19] != ' ' ||
	    ftp_line->date[13] != ':' || ftp_line->date[16] != ':' ||
	    foo[24] != ' ' || !isdigit(ftp_line->date[9]) ||
	    !isdigit(ftp_line->date[11]) || !isdigit(ftp_line->date[12]) ||
	    !isdigit(ftp_line->date[14]) || !isdigit(ftp_line->date[15]) ||
	    !isdigit(ftp_line->date[17]) || !isdigit(ftp_line->date[18]) ||
	    !isdigit(ftp_line->date[20]) || !isdigit(ftp_line->date[21]) ||
	    !isdigit(ftp_line->date[22]) || !isdigit(ftp_line->date[23]) ||
	    !(ftp_line->date[8] == ' ' || ftp_line->date[8] == '1' ||
	      ftp_line->date[8] == '2' || ftp_line->date[8] == '3')) {
#  ifdef DEBUGS
	  fprintf(stderr, "*** Corrupt line discarded.\n");
#  endif
#  ifndef SMFS
	  G_MEM_CHUNK_FREE(pointers->ftpentry_chunk, ftp_line);
#  endif
	  continue; /* the line is invalid */
	}
      
      if ((pointers->config->begin_date[0] &&
	   compare_dates(ftp_line->date,
			 pointers->config->begin_date) == -1) ||
	  (pointers->config->end_date[0] &&
	   compare_dates(ftp_line->date,
			 pointers->config->end_date) == 1)) {
#ifdef DEBUG3
	fprintf(stderr, "Discarding -- not in date range.\n");
#endif
#  ifndef SMFS
	G_MEM_CHUNK_FREE(pointers->ftpentry_chunk, ftp_line);
#  endif
	continue; /* the line is invalid */
      }
      
      if (!ftp_line->seconds)
	ftp_line->seconds = 1;
      
      ftp_line->host = g_malloc((len = strlen(tmp_host)) > MAXHOSTNAMELEN ?
				(len = MAXHOSTNAMELEN + 1) : ++len);
      memcpy(ftp_line->host, tmp_host, len - 1);
      ftp_line->host[len - 1] = '\0';

      if (pointers->config->strip_prefix_len &&
	  !strncmp(tmp_path, pointers->config->strip_prefix,
		   pointers->config->strip_prefix_len)) {
	ftp_line->path = g_malloc((len = strlen(tmp_path) -
				   pointers->config->strip_prefix_len) >
				  MAXPATHLEN ?
				  (len = MAXPATHLEN + 1) : ++len);
	memcpy(ftp_line->path,
	       tmp_path + pointers->config->strip_prefix_len, len - 1);
      } else {
	ftp_line->path = g_malloc((len = strlen(tmp_path)) > MAXPATHLEN ?
				  (len = MAXPATHLEN + 1) : ++len);
	memcpy(ftp_line->path, tmp_path, len - 1);
      }
      ftp_line->path[len - 1] = '\0';
      copy_clean_path(tmp_path, ftp_line->path, len);
      /* tmp_path can only be shorter, never longer, after copy_clean_path */
      memcpy(ftp_line->path, tmp_path, strlen(tmp_path) + 1);
      
      if (!pointers->first_ftp_line) {
	pointers->first_ftp_line = current_ftp_ptr = ftp_line;
#ifdef DEBUG3
	fprintf(stderr, "*** First ftp_line structure created (%p)\n",
		ftp_line);
#endif
      } else {
	current_ftp_ptr->next_ptr = ftp_line;
	current_ftp_ptr = ftp_line;
#ifdef DEBUG3
	fprintf(stderr, "*** New ftp_line structure added (%p)\n",
		ftp_line);
#endif
      }
#ifdef DEBUG3
      fprintf(stderr, "%s %ld %s %lu %s %c %c\n", ftp_line->date,
	      ftp_line->seconds, ftp_line->host, ftp_line->data,
	      ftp_line->path, in_out, user_type);
#endif
      lines++;
      
      if (pointers->config->chunk_input &&
	  lines >= pointers->config->chunk_input)
	return lines;
    }
    
    if (!pointers->config->use_stdin) {
      fclose(log_stream);
      log_stream = NULL;
    }
  }

#ifdef DEBUGS
  fprintf(stderr, "parse_wuftpd_logfile finished. (%ld)\n", (long) time(NULL));
#endif

  return 0;
} /* parse_wuftpd_logfile */


int
parse_ncftpd_logfile(pointers_t * pointers)
{
  ftp_entry_t * ftp_line, * current_ftp_ptr = NULL;
  char foo[2048], * tmp_char, tmp_host[2048], tmp_path[2048], tmp_date[4],
    user_name[2048], tmp_str[2048], * logfile;
  int temp, len;
  int lines = 0;
  GSList * clist;
#ifdef HAVE_MMAP
  struct stat stat_buf;
  char * map = NULL, * map_curr = NULL, * a, * b;
  int file_fd = -1, size;
#else
  FILE * log_stream = NULL;
#endif

#ifdef DEBUGS
  fprintf(stderr, "Beginning parse_ncftpd_logfile...\n");
#endif

  for (clist = pointers->config->logfiles; clist; clist = clist->next)
    {
      logfile = clist->data;

#ifdef HAVE_MMAP
      if (!pointers->config->use_stdin)
	{
	  if ((file_fd = open(logfile, O_RDONLY)) < 0)
	    {
	      printf("xferstats: fatal: file \"%s\" not found.\n", logfile);
	      exit(1);
	    }
	  stat(logfile, &stat_buf);
#  ifdef _AIX
	  /* Only AIX, it seems, wants this mmap typecast to int... */
	  if ((int)(map_curr = map =
		    (char *)mmap(NULL, stat_buf.st_size, PROT_READ, MAP_SHARED,
				 file_fd, 0)) < 0)
	    {
	      perror("mmap");
	      exit(1);
	    }
#  else
	  if ((map_curr = map =
	       (char *)mmap(NULL, stat_buf.st_size, PROT_READ, MAP_SHARED,
			    file_fd, 0)) < 0)
	    {
	      perror("mmap");
	      exit(1);
	    }
#  endif /* _AIX */
	}
#else /* HAVE_MMAP */
      if (!pointers->config->use_stdin)
	if (!(log_stream = fopen(logfile, "r")))
	  {
	    perror("parse_wuftpd_logfile: fopen");
	    exit(1);
	  }
#endif /* HAVE_MMAP */

      while (1)
	{
#ifdef HAVE_MMAP
	  if (pointers->config->use_stdin)
	    {
	      if (feof(stdin))
		break;
	      /* there's probably a better way to do this :) */
	      fgets(foo, sizeof(foo), stdin);
	    }
	  else
	    {
	      if (map_curr - map >= stat_buf.st_size)
		{
		  munmap(map, stat_buf.st_size);
		  close(file_fd);
		  break;
		}

	      a = memchr(map_curr, '\n', (size = stat_buf.st_size - (map_curr -
								     map)));
	      if (a)
		b = memchr(map_curr, '\0', a - map_curr);
	      else
		b = memchr(map_curr, '\0', size);
	      if (a && b)
		{
		  if (a < b)
		    len = a - map_curr;
		  else
		    len = b - map_curr;
		}
	      else if (a)
		len = a - map_curr;
	      else if (b)
		len = b - map_curr;
	      else
		len = size;

	      memcpy(foo, map_curr,
		     len > sizeof(foo) - 1 ? (len = sizeof(foo) - 1) : len);
	      foo[len] = '\0';	
	      map_curr += len + 1;
	    }
#else /* HAVE_MMAP */
	  if (pointers->config->use_stdin)
	    {
	      if (feof(stdin))
		break;
	      /* there's probably a better way to do this :) */
	      fgets(foo, sizeof(foo), stdin);
	    }
	  else
	    {
	      if (feof(log_stream))
		break;

	      fgets(foo, sizeof(foo), log_stream);
	    }
#endif /* HAVE_MMAP */

	  if (strlen(foo) < 54) continue; /* data is too small to be valid */
#ifdef DEBUG2
	  fprintf(stderr, "RAW READ: %s", foo);
#endif
	  /* this is a fairly ugly quick hack, but it seems to work.  I
	     don't plan on replacing this parsing code util a) ncftpd
	     logfiles change or b) someone sends me better code :) */

	  if ((foo[30] == 'R' || foo[30] == 'S') && foo[31] == ',')
	    {
	      ftp_line = G_MEM_CHUNK_ALLOC(pointers->ftpentry_chunk);
	
	      ftp_line->seconds = -1;
	      ftp_line->next_ptr = NULL;
	  
	      /* This is the new (maybe better, maybe not) way of doing it,
		 with individual byte setting */
	      memcpy(ftp_line->date, "    ", 4);
	      memcpy(tmp_date, foo + 5, 2);
	      tmp_date[2] = '\0';
	      temp = atoi(tmp_date);
	      memcpy(ftp_line->date + 4, MONTHS[temp - 1], 3);
	      ftp_line->date[7] = ' ';
	      memcpy(ftp_line->date + 8, foo + 8, 2);
	      ftp_line->date[10] = ' ';
	      memcpy(ftp_line->date + 11, foo + 11, 8);
	      ftp_line->date[19] = ' ';
	      ftp_line->date[20] = *foo;
	      memcpy(ftp_line->date + 21, foo + 1, 3);
	      continue;

	  if (pointers->config->strict_check)
	    /* These checks can significantly slow processing down, so they're
	     * highly optional. */
	    if (ftp_line->date[13] != ':' || ftp_line->date[16] != ':' ||
		foo[24] != ' ' || !isdigit(ftp_line->date[9]) ||
		!isdigit(ftp_line->date[11]) || !isdigit(ftp_line->date[12]) ||
		!isdigit(ftp_line->date[14]) || !isdigit(ftp_line->date[15]) ||
		!isdigit(ftp_line->date[17]) || !isdigit(ftp_line->date[18]) ||
		!isdigit(ftp_line->date[20]) || !isdigit(ftp_line->date[21]) ||
		!isdigit(ftp_line->date[22]) || !isdigit(ftp_line->date[23]) ||
		!(ftp_line->date[8] == ' ' || ftp_line->date[8] == '1' ||
		  ftp_line->date[8] == '2' || ftp_line->date[8] == '3'))
	      {
#  ifdef DEBUGS
		fprintf(stderr, "*** Corrupt line discarded.\n");
#  endif
#  ifndef SMFS
		G_MEM_CHUNK_FREE(pointers->ftpentry_chunk, ftp_line);
#  endif
		continue; /* the line is invalid */
	      }

	  tmp_char = foo + 32;
	  while ((tmp_char = strchr(tmp_char, ',')) != NULL)
	    *tmp_char = ' ';
	      
	  /* there's an extra %s in the next line to catch the decimal
	   * portion of the 'seconds' */
	  sscanf(foo + 32, "%s %lu %ld %s %s %s %s %s", tmp_path,
		 &ftp_line->data, &ftp_line->seconds, tmp_str, tmp_str,
		 user_name, tmp_str, tmp_host);

	  if ((foo[30] == 'R' && !pointers->config->outbound) ||
	      (foo[30] != 'R' && !pointers->config->inbound) ||
	      !strcmp(ftp_line->date, "") || ftp_line->seconds < 0 ||
	      (pointers->config->only_dir &&
	       !dir_in_list(pointers->config->only_dir, tmp_path)))
	    {
#ifndef SMFS
	      G_MEM_CHUNK_FREE(pointers->ftpentry_chunk, ftp_line);
#endif
	      continue; /* we don't want this log line */
	    }
	  if (!strcmp(user_name, "anonymous"))
	    {
	      if (!pointers->config->anon_traffic)
		{
#ifndef SMFS
		  G_MEM_CHUNK_FREE(pointers->ftpentry_chunk, ftp_line);
#endif
		  continue; /* we don't want anon traffic */
		}
	    }
	  else
	    {
	      if (!pointers->config->real_traffic)
		{
#ifndef SMFS
		  G_MEM_CHUNK_FREE(pointers->ftpentry_chunk, ftp_line);
#endif
		  continue; /* we don't want anon traffic */
		}
	      /* for a real user, the log line (after comma parsing) looks
	       * like:
	       * /foo 12345 5.123 30.069 pschwan  127.0.0.1  OK
	       * which means that the hostname is in tmp_str instead of
	       * tmp_host so we must use it instead */
	      memcpy(tmp_host, tmp_str, MAXHOSTNAMELEN);
	    }
	      
	  if ((pointers->config->begin_date[0] &&
	       compare_dates(ftp_line->date,
			     pointers->config->begin_date) == -1) ||
	      (pointers->config->end_date[0] &&
	       compare_dates(ftp_line->date,
			     pointers->config->end_date) == 1))
	    {
#ifdef DEBUG3
	      fprintf(stderr, "Discarding -- not in date range.\n");
#endif
#  ifndef SMFS
	      G_MEM_CHUNK_FREE(pointers->ftpentry_chunk, ftp_line);
#  endif
	      continue; /* the line is invalid */
	    }

	  if (!ftp_line->seconds)
	    ftp_line->seconds = 1;

	  ftp_line->host =
	    g_malloc((len = strlen(tmp_host)) > MAXHOSTNAMELEN ?
		     (len = MAXHOSTNAMELEN + 1) : ++len);
	  memcpy(ftp_line->host, tmp_host, len - 1);
	  ftp_line->host[len - 1] = '\0';
	  
	  if (!strncmp(tmp_path, pointers->config->strip_prefix,
		       pointers->config->strip_prefix_len))
	    {
	      ftp_line->path =
		g_malloc((len = strlen(tmp_path) -
			  pointers->config->strip_prefix_len)
			 > MAXPATHLEN ? (len = MAXPATHLEN + 1) : ++len);
	      memcpy(ftp_line->path,
		     tmp_path + pointers->config->strip_prefix_len, len - 1);
	    }
	  else
	    {
	      ftp_line->path = g_malloc((len = strlen(tmp_path)) >
					MAXPATHLEN ? (len = MAXPATHLEN + 1)
					: ++len);
	      memcpy(ftp_line->path, tmp_path, len - 1);
	    }
	  ftp_line->path[len - 1] = '\0';
	  
	  if (!pointers->first_ftp_line)
	    {
	      pointers->first_ftp_line = current_ftp_ptr = ftp_line;
#ifdef DEBUG
	      fprintf(stderr, "*** First ftp_line structure created (%p)\n"
		      ftp_line);
#endif
	    }
	  else
	    {
	      while (current_ftp_ptr->next_ptr != NULL)		
		current_ftp_ptr = current_ftp_ptr->next_ptr;
	      current_ftp_ptr->next_ptr = ftp_line;
#ifdef DEBUG
	      fprintf(stderr, "*** New ftp_line structure added (%p)\n",
		      ftp_line);
#endif
	    }
#ifdef DEBUG2
	  fprintf(stderr, "%s %ld %s %lu %s\n", ftp_line->date,
		  ftp_line->seconds, ftp_line->host, ftp_line->data,
		  ftp_line->path);
#endif
	    }
      lines++;
      
      if (pointers->config->chunk_input &&
	  lines >= pointers->config->chunk_input)
	return lines;
	}

#ifndef HAVE_MMAP
      if (!pointers->config->use_stdin)
	fclose(log_stream);
#endif /* HAVE_MMAP */
    }

#ifdef DEBUGS
  fprintf(stderr, "parse_ncftpd_logfile finished.\n");
#endif

  return 0;
} /* parse_ncftpd_logfile */


int
parse_apache_logfile(pointers_t * pointers)
{
  ftp_entry_t * ftp_line, * current_ftp_ptr = NULL;
  char foo[2048], tmp_host[2048], tmp_path[2048], tmp_user[2048], * logfile;
  int len;
  int lines = 0;
  GSList * clist;
#ifdef HAVE_MMAP
  struct stat stat_buf;
  int file_fd = -1, size;
  char * map = NULL, * map_curr = NULL, * a, * b;
#else /* HAVE_MMAP */
  FILE * log_stream = NULL;
#endif /* HAVE_MMAP */

#ifdef DEBUGS
  fprintf(stderr, "Beginning parse_apache_logfile...(%ld)\n",
	  (long) time(NULL));
#endif

  for (clist = pointers->config->logfiles; clist; clist = clist->next)
    {
      logfile = clist->data;

#ifdef HAVE_MMAP
      if (!pointers->config->use_stdin)
	{
	  if ((file_fd = open(logfile, O_RDONLY)) < 0)
	    {
	      printf("xferstats: fatal: file \"%s\" not found.\n", logfile);
	      exit(1);
	    }
	  stat(logfile, &stat_buf);
#  ifdef _AIX
	  /* Only AIX, it seems, wants this mmap typecast to int... */
	  if ((int)(map_curr = map =
		    (char *)mmap(NULL, stat_buf.st_size, PROT_READ, MAP_SHARED,
				 file_fd, 0)) < 0)
	    {
	      perror("mmap");
	      exit(1);
	    }
#  else
	  /* Linux doesn't care, and Solaris segfaults if it's an int.  God
	   * how I love Linux... */
	  if ((map_curr = map =
	       (char *)mmap(NULL, stat_buf.st_size, PROT_READ, MAP_SHARED,
			    file_fd, 0)) < 0)
	    {
	      perror("mmap");
	      exit(1);
	    }
#  endif /* _AIX */
	}
#else /* HAVE_MMAP */
      if (!pointers->config->use_stdin)
	if (!(log_stream = fopen(logfile, "r")))
	  {
	    perror("parse_wuftpd_logfile: fopen");
	    exit(1);
	  }
#endif /* HAVE_MMAP */

      while (1)
	{
#ifdef HAVE_MMAP
	  if (pointers->config->use_stdin)
	    {
	      if (feof(stdin))
		break;
	      /* there's probably a better way to do this :) */
	      fgets(foo, 2047, stdin);
	    }
	  else
	    {
	      if (map_curr - map >= stat_buf.st_size)
		{
		  munmap(map, stat_buf.st_size);
		  close(file_fd);
		  break;
		}
	      
	      a = memchr(map_curr, '\n', (size = stat_buf.st_size - (map_curr -
								     map)));
	      if (a)
		b = memchr(map_curr, '\0', a - map_curr);
	      else
		b = memchr(map_curr, '\0', size);
	      if (a && b)
		{
		  if (a < b)
		    len = a - map_curr;
		  else
		    len = b - map_curr;
		}
	      else if (a)
		len = a - map_curr;
	      else if (b)
		len = b - map_curr;
	      else
		len = size;
	      
	      memcpy(foo, map_curr,
		     len > sizeof(foo) - 1 ? (len = sizeof(foo) - 1) : len);
	      foo[len] = '\0';	
	      map_curr += len + 1;
	    }
#else /* HAVE_MMAP */
	  if (pointers->config->use_stdin)
	    {
	      if (feof(stdin))
		break;
	      /* there's probably a better way to do this :) */
	      fgets(foo, 2047, stdin);
	    }
	  else
	    {
	      if (feof(log_stream))
		break;

	      fgets(foo, sizeof(foo), log_stream);
	    }
#endif /* HAVE_MMAP */

	  if (strlen(foo) < 34) continue; /* data is too small to be valid */
#ifdef DEBUG3
	  fprintf(stderr, "RAW READ: %s", foo);
#endif
	  ftp_line = G_MEM_CHUNK_ALLOC(pointers->ftpentry_chunk);

	  ftp_line->date[24] = '\0';
	  strncpy(ftp_line->date, foo, 24);
	  
	  ftp_line->seconds = -1;
	  ftp_line->next_ptr = NULL;
	  
	  sscanf(foo + 25, "%ld %s %lu %s %s", &ftp_line->seconds,
		 tmp_host, &ftp_line->data, tmp_path, tmp_user);
	  
	  if (ftp_line->date[0] == '\0' || tmp_host[0] == '\0' ||
	      tmp_path[0] == '\0' || ftp_line->data == 0 ||
	      ftp_line->seconds < 0 ||
	      (!strcmp("-", tmp_user) && !pointers->config->anon_traffic) ||
	      (strcmp("-", tmp_user) && !pointers->config->real_traffic) ||
	      !pointers->config->outbound ||
	      (pointers->config->only_dir &&
	       !dir_in_list(pointers->config->only_dir, tmp_path)))
	    {
#ifndef SMFS
	      G_MEM_CHUNK_FREE(pointers->ftpentry_chunk, ftp_line);
#endif
	      continue; /* the line is invalid */
	    }
	  
	  if (pointers->config->strict_check)
	    /* These checks can significantly slow processing down, so they're
	     * highly optional. */
	    if (ftp_line->date[3] != ' ' || ftp_line->date[7] != ' ' ||
		ftp_line->date[10] != ' ' || ftp_line->date[19] != ' ' ||
		ftp_line->date[13] != ':' || ftp_line->date[16] != ':' ||
		foo[24] != ' ' || !isdigit(ftp_line->date[9]) ||
		!isdigit(ftp_line->date[11]) || !isdigit(ftp_line->date[12]) ||
		!isdigit(ftp_line->date[14]) || !isdigit(ftp_line->date[15]) ||
		!isdigit(ftp_line->date[17]) || !isdigit(ftp_line->date[18]) ||
		!isdigit(ftp_line->date[20]) || !isdigit(ftp_line->date[21]) ||
		!isdigit(ftp_line->date[22]) || !isdigit(ftp_line->date[23]) ||
		!(ftp_line->date[8] == ' ' || ftp_line->date[8] == '1' ||
		  ftp_line->date[8] == '2' || ftp_line->date[8] == '3'))
	      {
#  ifdef DEBUGS
		fprintf(stderr, "*** Corrupt line discarded.\n");
#  endif
#  ifndef SMFS
		G_MEM_CHUNK_FREE(pointers->ftpentry_chunk, ftp_line);
#  endif
		continue; /* the line is invalid */
	      }

	  if ((pointers->config->begin_date[0] &&
	       compare_dates(ftp_line->date,
			     pointers->config->begin_date) == -1) ||
	      (pointers->config->end_date[0] &&
	       compare_dates(ftp_line->date,
			     pointers->config->end_date) == 1))
	    {
#ifdef DEBUG3
	      fprintf(stderr, "Discarding -- not in date range.\n");
#endif
#  ifndef SMFS
	      G_MEM_CHUNK_FREE(pointers->ftpentry_chunk, ftp_line);
#  endif
	      continue; /* the line is invalid */
	    }

	  if (!ftp_line->seconds)
	    ftp_line->seconds = 1;
	  
	  ftp_line->host = g_malloc((len = strlen(tmp_host)) > MAXHOSTNAMELEN ?
				    (len = MAXHOSTNAMELEN + 1) : ++len);
	  strncpy(ftp_line->host, tmp_host, len - 1);
	  ftp_line->host[len - 1] = '\0';
	  
	  if (!strncmp(tmp_path, pointers->config->strip_prefix,
		       pointers->config->strip_prefix_len))
	    {
	      ftp_line->path = g_malloc((len = strlen(tmp_path) -
					 pointers->config->strip_prefix_len) >
					MAXPATHLEN ?
					(len = MAXPATHLEN + 1) : ++len);
	      memcpy(ftp_line->path,
		     tmp_path + pointers->config->strip_prefix_len, len - 1);
	    }
	  else
	    {
	      ftp_line->path = g_malloc((len = strlen(tmp_path)) > MAXPATHLEN ?
					(len = MAXPATHLEN + 1) : ++len);
	      memcpy(ftp_line->path, tmp_path, len - 1);
	    }
	  ftp_line->path[len - 1] = '\0';
      
	  if (pointers->first_ftp_line == NULL)
	    {
	      pointers->first_ftp_line = current_ftp_ptr = ftp_line;
#ifdef DEBUG3
	      fprintf(stderr, "*** First ftp_line structure created (%p)\n",
		      ftp_line);
#endif
	    }
	  else
	    {
	      current_ftp_ptr->next_ptr = ftp_line;
	      current_ftp_ptr = current_ftp_ptr->next_ptr;
#ifdef DEBUG3
	      fprintf(stderr, "*** New ftp_line structure added (%p)\n",
		      ftp_line);
#endif
	    }
#ifdef DEBUG3
	  fprintf(stderr, "%s %ld %s %lu %s\n", ftp_line->date,
		  ftp_line->seconds, ftp_line->host, ftp_line->data,
		  ftp_line->path);
#endif
      lines++;
      
      if (pointers->config->chunk_input &&
	  lines >= pointers->config->chunk_input)
	return lines;
	}

#ifndef HAVE_MMAP
      if (!pointers->config->use_stdin)
	fclose(log_stream);
#endif /* HAVE_MMAP */
    }

#ifdef DEBUGS
  fprintf(stderr, "parse_apache_logfile finished. (%ld)\n", (long) time(NULL));
#endif

  return 0;
} /* parse_apache_logfile */
