/*
 * xferstats, a logfile parser and report generator
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
#endif

#ifdef HAVE_CTYPE_H
#  include <ctype.h> /* needed for isdigit, tolower, isalpha */
#endif

#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#include <stdlib.h> /* needed everywhere */
#include <stdio.h> /* needed everywhere */
#include <time.h>

#include <glib.h>

#include "xferstats.h"

const char DAYS[7][3] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};
const char LONG_DAYS[7][10] = {"Sunday", "Monday", "Tuesday", "Wednesday",
			       "Thursday", "Friday", "Saturday"};
const char LONG_MONTHS[12][10] = {"January", "February", "March", "April",
				  "May", "June", "July", "August", "September",
				  "October", "November", "December"};
const char MONTHS[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul",
			    "Aug", "Sep", "Oct", "Nov", "Dec"};
/* don't ask ... */
const char NUMBERS[31][3] = {"01", "02", "03", "04", "05", "06", "07", "08",
			     "09", "10", "11", "12", "13", "14", "15", "16",
			     "17", "18", "19", "20", "21", "22", "23", "24",
			     "25", "26", "27", "28", "29", "30", "31"};
const char T_NAMES[6][10] = {"file", "dir", "host", "TLD", "domain", "size"};

typedef struct _GHashNode      GHashNode;

struct _GHashNode
{
  gpointer key;
  gpointer value;
  GHashNode *next;
};

struct _GHashTable
{
  gint size;
  gint nnodes;
  gint frozen;
  GHashNode **nodes;
  GHashFunc hash_func;
  GCompareFunc key_compare_func;
};

/* Very self-explanitory, converts an abbreviated month string to an integer.
 * In the interest of efficiency, it returns as soon as it can exclude other
 * months.  Therefore, "Jan", "Jar", "Jagawagavoovoo" all return 1 */
static inline int
month2int(char *month)
{
  switch (month[0])
    {
    case 'J':
      switch (month[1])
	{
	case 'a':
	  return 1;
	case 'u':
	  switch (month[2])
	    {
	    case 'n':
	      return 6;
	    case 'l':
	      return 7;
	    }
	}
    case 'F':
      return 2;
    case 'M':
      switch (month[2])
	{
	case 'r':
	  return 3;
	case 'y':
	  return 5;
	}
    case 'A':
      switch (month[1])
	{
	case 'p':
	  return 4;
	case 'u':
	  return 8;
	}
    case 'S':
      return 9;
    case 'O':
      return 10;
    case 'N':
      return 11;
    case 'D':
      return 12;
    }

  return 0;
} /* month2int */


/* this routine compares two date strings and tells us which is earlier.  it
 * returns:
 *
 * 0 for identical days
 * 1 for date1 being more in the future than date2
 * -1 for date1 being more in the past than date2 */
inline int
compare_dates(char date1[25], char date2[25])
{
  int dw1, dw2, len = strlen(date1);
  char foo[5] = {0};

  /* check the year */
  if (len == 24)
    {
      memcpy(foo, date1 + 20, 4);
      dw1 = atoi(foo);
      memcpy(foo, date2 + 20, 4);
      dw2 = atoi(foo);
    }
  else if (len == 15)
    {
      memcpy(foo, date1 + 11, 4);
      dw1 = atoi(foo);
      memcpy(foo, date2 + 11, 4);
      dw2 = atoi(foo);
    }
  else
    return 0;

#ifdef DEBUG
  fprintf(stderr, "Year1: %d     Year2: %d\n", dw1, dw2);
#endif
  if (dw1 > dw2)
    return 1;
  else if (dw2 > dw1)
    return -1;

  /* check the month */
  dw1 = month2int(date1 + 4);
  dw2 = month2int(date2 + 4);
#ifdef DEBUG
  fprintf(stderr, "Month1: %d     Month2: %d\n", dw1, dw2);
#endif
  if (dw1 > dw2)
    return 1;
  else if (dw2 > dw1)
    return -1;

  /* lastly, check the day */
  foo[0] = date1[8];
  foo[1] = date1[9];
  foo[2] = '\0';
  dw1 = atoi(foo);
  foo[0] = date2[8];
  foo[1] = date2[9];
  dw2 = atoi(foo);
#ifdef DEBUG
  fprintf(stderr, "Day1: %d     Day2: %d\n", dw1, dw2);
#endif

  if (dw1 == dw2)
    return 0;
  if (dw1 > dw2)
    return 1;
  else
    return -1;
} /* compare_dates */


static inline int
day2int(char *day)
{
  switch (day[0])
    {
    case 'S':
      switch (day[1])
	{
	case 'u':
	  return 1;
	case 'a':
	  return 7;
	}
    case 'M':
      return 2;
    case 'T':
      switch (day[1])
	{
	case 'u':
	  return 3;
	case 'h':
	  return 5;
	}
    case 'W':
      return 4;
    case 'F':
      return 6;
    }

  return 0;
} /* day2int */


int
xf_sort_by_data(const xfmisc_t ** a, const xfmisc_t ** b)
{
  if ((*a)->data > (*b)->data)
    return -1;
  else if ((*a)->data < (*b)->data)
    return 1;
  else
    return 0;
} /* xf_sort_by_data */


int
xf_sort_by_file_count(const xfmisc_t ** a, const xfmisc_t ** b)
{
  if ((*a)->file_count > (*b)->file_count)
    return -1;
  else if ((*a)->file_count < (*b)->file_count)
    return 1;
  else
    return 0;
} /* xf_sort_by_file_count */


int
xf_sort_by_file_size(const xfmisc_t ** a, const xfmisc_t ** b)
{
  if (strlen((*a)->name) > strlen((*b)->name))
    return -1;
  else if (strlen((*a)->name) < strlen((*b)->name))
    return 1;
  else
    return 0;
} /* xf_sort_by_file_size */


int
xf_sort_by_throughput(const xfmisc_t ** a, const xfmisc_t ** b)
{
  if ((*a)->data / (*a)->seconds > (*b)->data / (*b)->seconds)
    return -1;
  else if ((*a)->data / (*a)->seconds < (*b)->data / (*b)->seconds)
    return 1;
  else
    return 0;
} /* xf_sort_by_throughput */


int
xf_sort_alphabetical(const xfmisc_t ** a, const xfmisc_t ** b)
{
  return strcmp((*a)->name, (*b)->name);
} /* xf_sort_alphabetical */


int
xf_sort_by_day(const xfmisc_t ** a, const xfmisc_t ** b)
{
  return compare_dates((char *)(*a)->name, (char *)(*b)->name);
} /* xf_sort_by_file_size */


int
xf_sort_by_number(const xfmisc_t ** a, const xfmisc_t ** b)
{
  if (a < b)
    return -1;
  else if (b < a)
    return 1;
  else
    return 0;

  if ((*a)->name[0] < (*b)->name[0])
    return -1;
  else if ((*a)->name[0] > (*b)->name[0])
    return 1;
  else
    if ((*a)->name[1] < (*b)->name[1])
      return -1;
    else if ((*a)->name[1] > (*b)->name[1])
      return 1;
    else
      return 0;
} /* xf_sort_by_number */


static inline void **
xf_make_array(GHashTable * ht, int * count)
{
  void ** array;
  gint i, j;
  GHashNode * node;

  array = g_malloc(sizeof(void *) * ((*count = ht->nnodes) + 1));

  for (i = j = 0; i < ht->size; i++)
    for (node = ht->nodes[i]; node; node = node->next)
      array[j++] = node->value;

  array[j] = NULL;

  return array;
} /* xf_make_array */


/* This cute little macro is used to check/set the value in the hash tables in
 * generate_misc_data().  Maybe things would be cleaner if I wrote some more
 * macros instead of duplicating so much code in that function :) */
/*
 * What a disaster.  What was I thinking, 3 years ago, when I wrote this?
 * Was I high?
 */
#define xf_set_value(ht, item, current_data_ptr, other_data_ptr, com, fc) \
{									 \
  if ((current_data_ptr = g_hash_table_lookup(ht, item)))		 \
    {									 \
      current_data_ptr->file_count += fc;				 \
      current_data_ptr->seconds += other_data_ptr->seconds;		 \
      current_data_ptr->data += other_data_ptr->data;			 \
      current_data_ptr->completed_count += com;				 \
    }									 \
  else									 \
    {									 \
      current_data_ptr = G_MEM_CHUNK_ALLOC(pointers->xfmisc_chunk);	 \
      current_data_ptr->name = g_strdup(item);				 \
      current_data_ptr->data = other_data_ptr->data;			 \
      current_data_ptr->seconds = other_data_ptr->seconds;		 \
      current_data_ptr->file_count = fc;				 \
      current_data_ptr->completed_count = com;				 \
									 \
      g_hash_table_insert(ht, current_data_ptr->name, current_data_ptr); \
    }									 \
}


/* This function runs down the list starting with first_ftp_line and stuffs
 * data into the pointers->hourly_data[0-23] structs. */
void
generate_hourly_data(pointers_t * pointers, ftp_entry_t * first_ftp_line)
{
  ftp_entry_t * ftp_line;
  int index;
  char hour_str[3] = {0};
  int (*sort_func)(const void *, const void *) = NULL;
  
#ifdef DEBUGS
  fprintf(stderr, "Beginning hourly data generation...\n");
#endif

  for (ftp_line = first_ftp_line; ftp_line; ftp_line = ftp_line->next_ptr)
    {
      /* pluck the hour from the date into a string */
      hour_str[0] = ftp_line->date[11];
      hour_str[1] = ftp_line->date[12];
      
      /* convert it to an integer */
      index = atoi(hour_str);
      
      /* add it to the already-compiled data in the array */
      pointers->hour_data[index]->seconds += ftp_line->seconds;
      pointers->hour_data[index]->file_count++;
      pointers->hour_data[index]->data += ftp_line->data;
    }

  if (pointers->done) {
    switch(pointers->config->hourly_sort_pref) {
    case 0:
      sort_func = (void *)xf_sort_by_data;
      break;
    case 1:
      sort_func = (void *)xf_sort_by_file_count;
      break;
    case 3:
      sort_func = (void *)xf_sort_by_throughput;
      break;
    default:
      sort_func = (void *)xf_sort_by_number;
      break;
    }

    qsort((void *)pointers->hour_data, 24, (size_t) sizeof(void *), sort_func);
  }

#if 0
  if (pointers->config->additive_db)
    {
      FILE * db;

      if (!(db = fopen("hourly.xdb", "w")))
	return;

      if (fwrite(pointers->hour_data, sizeof(xfmult_t), 24, db) < 24)
	{
	  perror("generate_hourly_data: fwrite(hour_data)");
	  return;
	}

      fclose(db);
    }
#endif

#ifdef DEBUGS
  fprintf(stderr, "Hourly data generation complete.\n");
#endif
} /* generate_hourly_data */


void
generate_monthly_data(pointers_t * pointers, ftp_entry_t * first_ftp_line)
{
  xfmisc_t * day = NULL;
  ftp_entry_t * ftp_line;
  int index, count;
  int (*sort_func)(const void *, const void *) = NULL;

#ifdef DEBUGS
  fprintf(stderr, "Beginning monthly data generation...\n");
#endif

  if (!pointers->config->number_daily_stats && day)
    {
#ifdef DEBUGS
      fprintf(stderr, "   Using previously generated daily data...\n");
#endif

      for (count = 0; (day = pointers->first_day_ptr[count]); count++)
	{
	  if ((index = month2int(day->name + 4) - 1) >= 0)
	    {
	      /* add it to the already-compiled data in the array */
	      pointers->month_data[index]->seconds += day->seconds;
	      pointers->month_data[index]->file_count += day->file_count;
	      pointers->month_data[index]->data += day->data;
	    }
	}
    }
  else
    for (ftp_line = first_ftp_line; ftp_line; ftp_line = ftp_line->next_ptr)
      if ((index = month2int(ftp_line->date + 4) - 1) >= 0)
	{
	  /* add it to the already-compiled data in the array */
	  pointers->month_data[index]->seconds += ftp_line->seconds;
	  pointers->month_data[index]->file_count++;
	  pointers->month_data[index]->data += ftp_line->data;
	}

  if (pointers->done) {
    switch(pointers->config->monthly_sort_pref) {
    case 0:
      sort_func = (void *)xf_sort_by_data;
      break;
    case 1:
      sort_func = (void *)xf_sort_by_file_count;
      break;
    case 3:
      sort_func = (void *)xf_sort_by_throughput;
      break;
    default:
      sort_func = (void *)xf_sort_by_number;
    }

    qsort((void *)pointers->month_data, 12, (size_t)sizeof(void *), sort_func);
  }

#if 0
  if (pointers->config->additive_db)
    {
      FILE * db;

      if (!(db = fopen("monthly.xdb", "w")))
	return;

      if (fwrite(pointers->month_data, sizeof(xfmult_t), 12, db) < 12)
	{
	  perror("generate_monthly_data: fwrite(month_data)");
	  return;
	}

      fclose(db);
    }
#endif

#ifdef DEBUGS
  fprintf(stderr, "Monthly data generation complete.\n");
#endif
} /* generate_monthly_data */


void
generate_dom_data(pointers_t * pointers, ftp_entry_t * first_ftp_line)
{
  ftp_entry_t * ftp_line;
  xfmisc_t * day = NULL;
  int index, count;
  char day_str[3] = {0};
  int (*sort_func)(const void *, const void *) = NULL;

#ifdef DEBUGS
  fprintf(stderr, "Beginning day-of-the-month data generation...\n");
#endif

  if (!pointers->config->number_daily_stats && day &&
      !pointers->config->chunk_input)
    {
#ifdef DEBUGS
      fprintf(stderr, "   Using previously generated daily data...\n");
#endif

      for (count = 0; (day = pointers->first_day_ptr[count]); count++)
	{
	  /* pluck the day from the date into a string */
	  strncpy(day_str, day->name+8, 2);

	  /* convert it to an appropriate type and decrement it */
	  index = atoi(day_str);

	  if (--index >= 0 && index <= 30)
	    {
	      /* add it to the already-compiled data in the array */
	      pointers->dom_data[index]->seconds += day->seconds;
	      pointers->dom_data[index]->file_count += day->file_count;
	      pointers->dom_data[index]->data += day->data;
	    }
#ifdef DEBUG
	  else
	    fprintf(stderr, "generate_dom_data: Invalid day: %d\n", index);
#endif
	}
    }
  else
    for (ftp_line = first_ftp_line; ftp_line; ftp_line = ftp_line->next_ptr)
      {
	/* pluck the day from the date into a string */
	strncpy(day_str, ftp_line->date + 8, 2);

	/* convert it to an appropriate type and decrement it */
	index = atoi(day_str);

	if (--index >= 0 && index <= 30)
	  {
	    /* add it to the already-compiled data in the array */
	    pointers->dom_data[index]->seconds += ftp_line->seconds;
	    pointers->dom_data[index]->file_count += 1;
	    pointers->dom_data[index]->data += ftp_line->data;
	  }
#ifdef DEBUG
	else
	  fprintf(stderr, "generate_dom_data: Invalid day: %d\n", index);
#endif
      }

  if (pointers->done) {
    switch(pointers->config->dom_sort_pref) {
    case 0:
      sort_func = (void *)xf_sort_by_data;
      break;
    case 1:
      sort_func = (void *)xf_sort_by_file_count;
      break;
    case 3:
      sort_func = (void *)xf_sort_by_throughput;
      break;
    default:
      sort_func = (void *)xf_sort_by_number;
    }

    qsort((void *)pointers->dom_data, 31, (size_t) sizeof(void *), sort_func);
  }

#if 0
  if (pointers->config->additive_db)
    {
      FILE * db;

      if (!(db = fopen("dom.xdb", "w")))
	return;

      if (fwrite(pointers->dom_data, sizeof(xfmult_t), 31, db) < 31)
	{
	  perror("generate_dom_data: fwrite(dom_data)");
	  return;
	}

      fclose(db);
    }
#endif

#ifdef DEBUGS
  fprintf(stderr, "Day-of-the-month data generation complete.\n");
#endif
} /* generate_dom_data */


/* this function creates the structures for the day-of-the-week report. */
void
generate_dow_data(pointers_t * pointers, ftp_entry_t * first_ftp_line)
{
  ftp_entry_t * ftp_line;
  xfmisc_t * day = NULL;
  int index, count;
  int (*sort_func)(const void *, const void *) = NULL;

#ifdef DEBUGS
  fprintf(stderr, "Beginning day-of-the-week data generation...\n");
#endif

  if (!pointers->config->number_daily_stats && day &&
      !pointers->config->chunk_input)
    {
#ifdef DEBUGS
      fprintf(stderr, "   Using previously generated daily data...\n");
#endif

      for (count = 0; (day = pointers->first_day_ptr[count]); count++)
	if ((index = day2int(day->name) - 1) >= 0)
	  {
	    /* add it to the already-compiled data in the array */
	    pointers->weekday_data[index]->seconds += day->seconds;
	    pointers->weekday_data[index]->file_count += day->file_count;
	    pointers->weekday_data[index]->data += day->data;
	  }
#ifdef DEBUG
	else
	  fprintf(stderr, "Invalid day in date \"%s\"\n", day->name);
#endif
    }
  else
    for (ftp_line = first_ftp_line; ftp_line; ftp_line = ftp_line->next_ptr)
      if ((index = day2int(ftp_line->date) - 1) >= 0)
	{
	  /* add it to the already-compiled data in the array */
	  pointers->weekday_data[index]->seconds += ftp_line->seconds;
	  pointers->weekday_data[index]->file_count += 1;
	  pointers->weekday_data[index]->data += ftp_line->data;
	  }
#ifdef DEBUG
      else
	fprintf(stderr, "Invalid day in date \"%s\"\n", day->name);
#endif

  if (pointers->done) {
    switch(pointers->config->dow_sort_pref) {
    case 0:
      sort_func = (void *)xf_sort_by_data;
      break;
    case 1:
      sort_func = (void *)xf_sort_by_file_count;
      break;
    default:
      sort_func = (void *)xf_sort_by_number;
    }

    qsort((void *)pointers->weekday_data, 7, (size_t)sizeof(void *),sort_func);
  }

#if 0
  if (pointers->config->additive_db)
    {
      FILE * db;

      if (!(db = fopen("dow.xdb", "w")))
	return;

      if (fwrite(pointers->weekday_data, sizeof(xfmult_t), 7, db) < 7)
	{
	  perror("generate_dow_data: fwrite(weekday_data)");
	  return;
	}

      fclose(db);
    }
#endif

#ifdef DEBUGS
  fprintf(stderr, "Day-of-the-week data generation complete.\n");
#endif
} /* generate_dow_data */


void
generate_daily_data(pointers_t * pointers, ftp_entry_t * first_ftp_line)
{
  ftp_entry_t * ftp_line = first_ftp_line;
  xfmisc_t * current_day_ptr;
  char temp_date[16];
  static GHashTable * ht = NULL;
  int count;
  int (*sort_func)(const void *, const void *) = NULL;

#ifdef DEBUGS
  fprintf(stderr, "Beginning daily data generation...\n");
#endif

  if (!ht)
    ht = g_hash_table_new(g_str_hash, g_str_equal);
  
  /* If [low, high]_date hasn't already been set (perhaps by the additive DB
   * code, or a (god forbid) previous execution of this function) set them to
   * something so that we can compare later */
  if (!pointers->low_date[0])
    memcpy(pointers->low_date, ftp_line->date, 25);
  if (!pointers->high_date[0])
    memcpy(pointers->high_date, ftp_line->date, 25);

  for (; ftp_line; ftp_line = ftp_line->next_ptr)
    {
      pointers->file_count++;
      pointers->data += ftp_line->data;
      if (ftp_line->complete == 'c') {
	pointers->complete_count++;
      }

      if (compare_dates(ftp_line->date, pointers->high_date) > 0)
	memcpy(pointers->high_date, ftp_line->date, 25);
      else if (compare_dates(ftp_line->date, pointers->low_date) < 0)
	memcpy(pointers->low_date, ftp_line->date, 25);

      memcpy(temp_date, ftp_line->date, 10);
      memcpy(temp_date + 10, ftp_line->date + 19, 5);
      temp_date[15] = '\0';

      xf_set_value(ht, temp_date, current_day_ptr, ftp_line,
		   (ftp_line->complete == 'c' ? 1 : 0), 1);
    }

  if (pointers->done) {
    pointers->first_day_ptr = (xfmisc_t **)xf_make_array(ht, &count);
    g_hash_table_destroy(ht);
  
    switch(pointers->config->daily_sort_pref) {
    case 0:
      sort_func = (void *)xf_sort_by_data;
      break;
    case 1:
      sort_func = (void *)xf_sort_by_file_count;
      break;
    case 3:
      sort_func = (void *)xf_sort_by_throughput;
      break;
    default:
      sort_func = (void *)xf_sort_by_day;
      break;
    }

    qsort((void *)pointers->first_day_ptr, (size_t) count,
	  (size_t) sizeof(void *), sort_func);
  }

#if 0
  if (pointers->config->additive_db)
    {
      FILE * db;

      if (!(db = fopen("daily.xdb", "w")))
	return;

      for (current_day_ptr = pointers->first_day_ptr; current_day_ptr;
	   current_day_ptr = current_day_ptr->next_ptr)
	{
	  if (fwrite(current_day_ptr->name, sizeof(char),
		     sizeof(current_day_ptr->name) / sizeof(char), db) <
	      sizeof(current_day_ptr->name) / sizeof(char))
	    {
	      perror("generate_daily_data: fwrite(name)");
	      return;
	    }
	  if (fwrite(&current_day_ptr->data, sizeof(current_day_ptr->data), 1,
		     db) < 1)
	    {
	      perror("generate_daily_data: fwrite(data)");
	      return;
	    }
	  if (fwrite(&current_day_ptr->file_count,
		     sizeof(current_day_ptr->file_count), 1, db) < 1)
	    {
	      perror("generate_daily_data: fwrite(file_count)");
	      return;
	    }
	  if (fwrite(&current_day_ptr->seconds,
		     sizeof(current_day_ptr->seconds), 1, db) < 1)
	    {
	      perror("generate_daily_data: fwrite(seconds)");
	      return;
	    }
	}

      fclose(db);

      if (!(db = fopen("totals.xdb", "w")))
	return;

      if (fwrite(pointers->low_date, sizeof(char),
		 sizeof(pointers->low_date) / sizeof(char), db) <
	  sizeof(pointers->low_date) / sizeof(char))
	{
	  perror("generate_daily_data: fwrite(low_date)");
	  return;
	}
      if (fwrite(pointers->high_date, sizeof(char),
		 sizeof(pointers->high_date) / sizeof(char), db) <
	  sizeof(pointers->high_date) / sizeof(char))
	{
	  perror("generate_daily_data: fwrite(high_date)");
	  return;
	}
      if (fwrite(&pointers->data, sizeof(pointers->data), 1, db)
	  < 1)
	{
	  perror("generate_daily_data: fwrite(data)");
	  return;
	}
      if (fwrite(&pointers->file_count, sizeof(pointers->file_count), 1, db)
	  < 1)
	{
	  perror("generate_daily_data: fwrite(file_count)");
	  return;
	}

      fclose(db);
    }
#endif

#ifdef DEBUGS
  fprintf(stderr, "Daily data generation complete.\n");
#endif
} /* generate_daily_data */


/* This is probably the worst function in xferstats to read or change.
 * It performs size functions: file, directory, host, domains, TLD and file
 * size data generation  because of the similar nature in which they're
 * processed.  Passed in addition to the pointers struct is the type (defined
 * in xferstats.h). */
void
generate_misc_data(pointers_t *pointers, ftp_entry_t * first_ftp_line,
		   int type)
{
  GHashTable * ht;

  ftp_entry_t * ftp_line = first_ftp_line;
  xfmisc_t * other_data_ptr, * current_data_ptr = NULL;
  int a, count = 1;
  void ** pointers_ptr = NULL, ** array;
  char name[MAXPATHLEN], * tmpchar, * prevchar;
  int (*sort_func)(const void *, const void *) = NULL;
  
#ifdef DEBUGS
  fprintf(stderr, "Beginning %s data generation...(%ld)\n", T_NAMES[type],
	  (long) time(NULL));
#endif

  switch (type) {
  case T_FILE:
    pointers_ptr = pointers->first_file_ptr;
    if (!pointers->file_ht)
      ht = pointers->file_ht = g_hash_table_new(g_str_hash, g_str_equal);
    else
      ht = pointers->file_ht;
    break;
  case T_DIR:
    pointers_ptr = pointers->first_dir_ptr;
    if (!pointers->dir_ht)
      ht = pointers->dir_ht = g_hash_table_new(g_str_hash, g_str_equal);
    else
      ht = pointers->dir_ht;
    break;
  case T_HOST:
    pointers_ptr = pointers->first_host_ptr;
    if (!pointers->host_ht)
      ht = pointers->host_ht = g_hash_table_new(g_str_hash, g_str_equal);
    else
      ht = pointers->host_ht;
    break;
  case T_TLD:
    pointers_ptr = pointers->first_tld_ptr;
    if (!pointers->tld_ht)
      ht = pointers->tld_ht = g_hash_table_new(g_str_hash, g_str_equal);
    else
      ht = pointers->tld_ht;
    break;
  case T_DOMAIN:
    pointers_ptr = pointers->first_domain_ptr;
    if (!pointers->domain_ht)
      ht = pointers->domain_ht = g_hash_table_new(g_str_hash, g_str_equal);
    else
      ht = pointers->domain_ht;
    break;
  case T_SIZE:
    pointers_ptr = pointers->first_size_ptr;
    if (!pointers->size_ht)
      ht = pointers->size_ht = g_hash_table_new(g_str_hash, g_str_equal);
    else
      ht = pointers->size_ht;
    break;
  default:
    fprintf(stderr, "generate_misc_data: invalid type.  if you get this, "
	    "please mail\n phil@off.net with the version number (%s).  "
	    "This is important!\n", VERSION);
    return;
  }

  if (type == T_DOMAIN && pointers->config->host_traffic &&
      pointers->first_host_ptr && !pointers->config->number_host_stats &&
      !pointers->config->chunk_input)
    {
      /* get domain data from host data--since it's already partly-compiled, it
       * saves time, as there will be fewer (or at the very least, the same
       * number of) records. */

#ifdef DEBUGS
      fprintf(stderr, "   Using previously generated host data...\n");
#endif

      for (other_data_ptr = (xfmisc_t *)pointers->first_host_ptr[0];
	   other_data_ptr;
	   other_data_ptr = (xfmisc_t *)pointers->first_host_ptr[count++])
	{
	  /* check to see if it's resolved first */
	  if ((tmpchar = strrchr(other_data_ptr->name, '.')) &&
	      tmpchar + 1)
	    {
#ifdef DEBUG2
	      fprintf(stderr, "First letter of TLD: %d:%c\n",
		      *(tmpchar + 1), *(tmpchar + 1));
#endif
	      if (isdigit((int)*(tmpchar + 1)))
		strncpy(name, "unresolved", sizeof(name) - 1);
	      else
		{
		  /* now check to see if we have another '.' */
		  if (tmpchar == other_data_ptr->name)
		    {
		      /* tmpchar is already the first character (why there
		       * would be any sort of valid domain name that BEGINS
		       * with a period is beyond me... */

		      /* convert all to lower case */
		      for(prevchar = tmpchar + 1;
			  (*prevchar = tolower((int)*prevchar)); prevchar++);

		      strncpy(name, tmpchar + 1, MAXHOSTNAMELEN - 1);
		      name[MAXHOSTNAMELEN - 1] = '\0';
		    }
		  else
		    {
		      *tmpchar = '\0';
		      if (!(prevchar = strrchr(other_data_ptr->name, '.')))
			{
			  /* there is only one '.' in the host */

			  *tmpchar = '.';

			  /* convert all to lower case */
			  for(prevchar = other_data_ptr->name;
			      (*prevchar = tolower((int)*prevchar));
			      prevchar++)
			    ;

			  strncpy(name, other_data_ptr->name,
				  MAXHOSTNAMELEN - 1);
			  name[MAXHOSTNAMELEN - 1] = '\0';
			}
		      else
			{
			  /* take everything after that '.' */

			  *tmpchar = '.';
			  tmpchar = prevchar;

			  /* convert all to lower case */
			  for(prevchar = tmpchar + 1;
			      (*prevchar = tolower((int)*prevchar));
			      prevchar++)
			    ;

			  strncpy(name, tmpchar + 1, MAXHOSTNAMELEN - 1);
			  name[MAXHOSTNAMELEN - 1] = '\0';
			}
		    }
		}
	    }
	  else
	    {
	      /* No '.' in the name, eh?  Probably something that's in
	       * /etc/hosts.  *ching* Unresolved. */
	      strncpy(name, "unresolved", sizeof(name) - 1);
	    }

	  xf_set_value(ht, name, current_data_ptr, other_data_ptr,
		       other_data_ptr->completed_count,
		       other_data_ptr->file_count);
	}
    }
  else if (type == T_DIR && pointers->config->file_traffic &&
	   pointers->first_file_ptr && !pointers->config->number_file_stats &&
	   !pointers->config->chunk_input)
    {
      /* Take the dir data from the file data, for the same reason as above */

#ifdef DEBUGS
      fprintf(stderr, "   Using previously generated file data...\n");
#endif

      for (other_data_ptr = (xfmisc_t *)pointers->first_file_ptr[0];
	   other_data_ptr;
	   other_data_ptr = (xfmisc_t *)pointers->first_file_ptr[count++])
	{
	  strncpy(name, other_data_ptr->name, MAXPATHLEN - 1);
	  /* if we're doing directory processing, cut off at least the
	   * filename (and probably some directories) based on
	   * pointers->config->depth */
	  if (pointers->config->depth)
	    {
	      if ((tmpchar = strrchr(name, '/')))
		*(tmpchar + 1) = '\0';
	      for (a = 1, prevchar = name + 1, tmpchar = name;
		   a <= pointers->config->depth; prevchar = tmpchar, a++)
		{
		  if ((tmpchar = strchr(tmpchar + 1, '/')) == NULL)
		    {
		      *prevchar = '\0';
		      break;
		    }
		  if (a == pointers->config->depth)
		    *tmpchar = '\0';
		}
	    }
	  else
	    if ((tmpchar = strrchr(name, '/')))
	      *tmpchar = '\0';
	  
#ifdef DEBUG2
	  fprintf(stderr, "path: %s dir: %s\n", other_data_ptr->name, name);
#endif

	  xf_set_value(ht, name, current_data_ptr, other_data_ptr,
		       other_data_ptr->completed_count,
		       other_data_ptr->file_count);
	}
    }
  else if (type == T_TLD && pointers->config->domain_traffic &&
	   pointers->first_domain_ptr &&
	   !pointers->config->number_domain_stats &&
	   !pointers->config->chunk_input)
    {
      /* Take the TLD data from the domain data, for the same reason as the
       * two above blocks... */

#ifdef DEBUGS
      fprintf(stderr, "   Using previously generated domain data...\n");
#endif

      for (other_data_ptr = (xfmisc_t *)pointers->first_domain_ptr[0];
	   other_data_ptr;
	   other_data_ptr = (xfmisc_t *)pointers->first_domain_ptr[count++])
	{
	  strncpy(name, other_data_ptr->name, MAXHOSTNAMELEN - 1);
	  /* if we're doing TLD processing, we only want what's after the
	   * last '.' */
	  if ((tmpchar = strrchr(name, '.')) &&
	      tmpchar + 1)
	    {
	      strncpy(name, tmpchar + 1, MAXHOSTNAMELEN - 1);
#ifdef DEBUG2
	      fprintf(stderr, "First letter of TLD: %d:%c\n", name[0],
		      name[0]);
#endif
	      if (isdigit((int)name[0]))
		strncpy(name, "unresolved", sizeof(name) - 1);
	      else
		/* convert all to lower case */
		for(tmpchar = name; (*tmpchar = tolower((int)*tmpchar));
		    tmpchar++);
	    }
	  else
	    strncpy(name, "unresolved", sizeof(name) - 1);
#ifdef DEBUG2
	  fprintf(stderr, "host: %s TLD: %s\n", other_data_ptr->name, name);
#endif

	  xf_set_value(ht, name, current_data_ptr, other_data_ptr,
		       other_data_ptr->completed_count,
		       other_data_ptr->file_count);
	}
    }
  else
    {
      for (; ftp_line; ftp_line = ftp_line->next_ptr)
	{
	  /* if we're doing file or host processing, all we need to do is
	   * strncpy the name and go */
	  switch (type)
	    {
	    case T_FILE:
	      strncpy(name, ftp_line->path, MAXPATHLEN - 1);
	      break;
	    case T_HOST:
	      strncpy(name, ftp_line->host, MAXHOSTNAMELEN - 1);
	      break;
	    case T_DIR:
	      strncpy(name, ftp_line->path, MAXPATHLEN - 1);
	      /* if we're doing directory processing, cut off at least the
	       * filename (and probably some directories) based on
	       * pointers->config->depth */
	      if (pointers->config->depth)
		{
		  if ((tmpchar = strrchr(name, '/')))
		    *(tmpchar + 1) = '\0';
		  for (a = 1, prevchar = name + 1, tmpchar = name;
		       a <= pointers->config->depth;
		       prevchar = tmpchar, a++)
		    {
		      if ((tmpchar = strchr(tmpchar + 1, '/')) == NULL)
			{
			  *prevchar = '\0';
			  break;
			}
		      if (a == pointers->config->depth)
			*tmpchar = '\0';
		    }
		}
	      else
		if ((tmpchar = strrchr(name, '/')))
		  *tmpchar = '\0';
	      
#ifdef DEBUG2
	      fprintf(stderr, "date: %s path: %s dir: %s\n", ftp_line->date,
		      ftp_line->path, name);
#endif
	      break;
	    case T_TLD:
	      strncpy(name, ftp_line->host, MAXHOSTNAMELEN - 1);
	      /* if we're doing TLD processing, we only want what's after the
	       * last '.' */
	      if ((tmpchar = strrchr(name, '.')) &&
		  tmpchar + 1)
		{
		  strncpy(name, tmpchar + 1, MAXHOSTNAMELEN - 1);
#ifdef DEBUG2
		  fprintf(stderr, "First letter of TLD: %d:%c\n", name[0],
			  name[0]);
#endif
		  if (isdigit((int)name[0]))
		    strncpy(name, "unresolved", sizeof(name) - 1);
		  else
		    /* convert all to lower case */
		    for(tmpchar = name; (*tmpchar = tolower((int)*tmpchar));
			tmpchar++);
		}
	      else
		strncpy(name, "unresolved", sizeof(name) - 1);
#ifdef DEBUG2
	      fprintf(stderr, "host: %s TLD: %s\n", ftp_line->host, name);
#endif
	      break;
	    case T_DOMAIN:
	      /* if we're doing domain processing, check to see if it's
	       * resolved first */
	      if ((tmpchar = strrchr(ftp_line->host, '.')) &&
		  tmpchar + 1)
		{
#ifdef DEBUG2
		  fprintf(stderr, "First letter of TLD: %d:%c\n",
			  *(tmpchar + 1), *(tmpchar + 1));
#endif
		  if (isdigit((int)*(tmpchar + 1)))
		    {
		      strncpy(name, "unresolved", sizeof(name) - 1);
		      break;
		    }
		}
	      else
		{
		  /* No '.' in the name, eh?  Probably something that's in
		   * /etc/hosts.  *ching* Unresolved. */
		  strncpy(name, "unresolved", sizeof(name) - 1);
		  break;
		}

	      /* now check to see if we have another '.' */
	      if (tmpchar == ftp_line->host)
		{
		  /* tmpchar is already the first character (why there would
		   * be any sort of valid domain name that BEGINS with a period
		   * is beyond me... */

		  /* convert all to lower case */
		  for(prevchar = tmpchar + 1;
		      (*prevchar = tolower((int)*prevchar)); prevchar++);

		  strncpy(name, tmpchar + 1, MAXHOSTNAMELEN - 1);
		  name[MAXHOSTNAMELEN - 1] = '\0';
		  break;
		}

	      *tmpchar = '\0';
	      if ((prevchar = strrchr(ftp_line->host, '.')) == NULL)
		{
		  /* there is only one '.' in the host */

		  *tmpchar = '.';

		  /* convert all to lower case */
		  for(prevchar = ftp_line->host;
		      (*prevchar = tolower((int)*prevchar)); prevchar++);

		  strncpy(name, ftp_line->host, MAXHOSTNAMELEN - 1);
		  name[MAXHOSTNAMELEN - 1] = '\0';
		}
	      else
		{
		  /* take everything after that '.' */

		  *tmpchar = '.';
		  tmpchar = prevchar;

		  /* convert all to lower case */
		  for(prevchar = tmpchar + 1;
		      (*prevchar = tolower((int)*prevchar)); prevchar++);

		  strncpy(name, tmpchar + 1, MAXHOSTNAMELEN - 1);
		  name[MAXHOSTNAMELEN - 1] = '\0';
		}
	      break;
	    case T_SIZE:
	      g_snprintf(name, sizeof(name) - 1, "%lu0", ftp_line->data);

	      name[0] = '1';

	      for (tmpchar = name + 1; *(tmpchar + 1); tmpchar++)
		*tmpchar = '0';

	      break;
	    } /* switch(type) */
	  
	  xf_set_value(ht, name, current_data_ptr, ftp_line,
		       (ftp_line->complete == 'c' ? 1 : 0), 1);
	}
    }

  if (pointers->done) {
#ifdef DEBUG
    fprintf(stderr, "Starting to sort...\n");
#endif

    /* In recognition of time wasted working on sorting only to replace it all
     * with a basic libc function, I give you the following quote:
     *
     * "There are people out there who will do this, and they are very scary."
     *     -- Alan Cox, "Space Aliens Ate My Mouse", Linux Expo, May 30 1998
     */

    array = xf_make_array(ht, &count);
    g_hash_table_destroy(ht);
    
    switch(type) {
    case T_FILE:
      switch(pointers->config->file_sort_pref) {
      case 0:
	sort_func = (void *)xf_sort_by_data;
	break;
      case 2:
	sort_func = (void *)xf_sort_alphabetical;
	break;
      case 3:
	sort_func = (void *)xf_sort_by_throughput;
	break;
      default:
	sort_func = (void *)xf_sort_by_file_count;
	break;
      }
      break;
    case T_HOST:
      switch(pointers->config->host_sort_pref) {
      case 1:
	sort_func = (void *)xf_sort_by_file_count;
	break;
      case 2:
	sort_func = (void *)xf_sort_alphabetical;
	break;
      case 3:
	sort_func = (void *)xf_sort_by_throughput;
	break;
      default:
	sort_func = (void *)xf_sort_by_data;
	break;
      }
      break;
    case T_DIR:
      switch(pointers->config->dir_sort_pref) {
      case 1:
	sort_func = (void *)xf_sort_by_file_count;
	break;
      case 2:
	sort_func = (void *)xf_sort_alphabetical;
	break;
      case 3:
	sort_func = (void *)xf_sort_by_throughput;
	break;
      default:
	sort_func = (void *)xf_sort_by_data;
	break;
      }
      break;
    case T_TLD:
      switch(pointers->config->tld_sort_pref) {
      case 1:
	sort_func = (void *)xf_sort_by_file_count;
	break;
      case 2:
	sort_func = (void *)xf_sort_alphabetical;
	break;
      case 3:
	sort_func = (void *)xf_sort_by_throughput;
	break;
      default:
	sort_func = (void *)xf_sort_by_data;
	break;
      }
      break;
    case T_DOMAIN:
      switch(pointers->config->domain_sort_pref) {
      case 1:
	sort_func = (void *)xf_sort_by_file_count;
	break;
      case 2:
	sort_func = (void *)xf_sort_alphabetical;
	break;
      case 3:
	sort_func = (void *)xf_sort_by_throughput;
	break;
      default:
	sort_func = (void *)xf_sort_by_data;
	break;
      }
      break;
    case T_SIZE:
      switch(pointers->config->size_sort_pref) {
      case 0:
	sort_func = (void *)xf_sort_by_data;
	break;
      case 1:
	sort_func = (void *)xf_sort_by_file_count;
	break;
      case 3:
	sort_func = (void *)xf_sort_by_throughput;
	break;
      default:
	sort_func = (void *)xf_sort_by_file_size;
	break;
      }
      break;
    }

    qsort((void *)array, (size_t) count, (size_t) sizeof(void *), sort_func);

    switch(type) {
    case T_FILE:
      pointers->first_file_ptr = array;
      break;
    case T_HOST:
      pointers->first_host_ptr = array;
      break;
    case T_DIR:
      pointers->first_dir_ptr = array;
      break;
    case T_TLD:
      pointers->first_tld_ptr = array;
      break;
    case T_DOMAIN:
      pointers->first_domain_ptr = array;
      break;
    case T_SIZE:
      pointers->first_size_ptr = array;
      break;
    }

    if (pointers->config->additive_db) {
      FILE * db;

      switch(type) {
      case T_FILE:
	if (!(db = fopen("file.xdb", "w")))
	  return;
	break;
      case T_HOST:
	if (!(db = fopen("host.xdb", "w")))
	  return;
	break;
      case T_DIR:
	if (!(db = fopen("dir.xdb", "w")))
	  return;
	break;
      case T_TLD:
	if (!(db = fopen("tld.xdb", "w")))
	  return;
	break;
      case T_DOMAIN:
	if (!(db = fopen("domain.xdb", "w")))
	  return;
	break;
      case T_SIZE:
	if (!(db = fopen("size.xdb", "w")))
	  return;
	break;
      default:
	return;
      }
      
      for (current_data_ptr = array[count = 0]; current_data_ptr;
	   current_data_ptr = array[++count]) {
	if (fwrite(current_data_ptr->name, sizeof(char),
		   strlen(current_data_ptr->name) + 1, db) <
	    strlen(current_data_ptr->name) + 1) {
	  perror("generate_misc_data: fwrite(name)");
	  return;
	}
	if (fwrite(&current_data_ptr->data, sizeof(current_data_ptr->data),
		   1, db) < 1) {
	  perror("generate_misc_data: fwrite(data)");
	  return;
	}
	if (fwrite(&current_data_ptr->file_count,
		   sizeof(current_data_ptr->file_count), 1, db) < 1) {
	  perror("generate_misc_data: fwrite(file_count)");
	  return;
	}
	if (fwrite(&current_data_ptr->seconds,
		   sizeof(current_data_ptr->seconds), 1, db) < 1) {
	  perror("generate_misc_data: fwrite(seconds)");
	  return;
	}
      }

      fclose(db);
    }
  }

#ifdef DEBUGS
  fprintf(stderr, "%s data generation complete (%ld)\n", T_NAMES[type],
	  (long) time(NULL));
#endif
} /* generate_misc_data */


void
parse_xferstats_databases(pointers_t * pointers)
{
  FILE * db;
  char tmpstr[MAXPATHLEN];
  int type;

  if ((db = fopen("totals.xdb", "r")))
    {
#ifdef DEBUGS
  fprintf(stderr, "Reading databases from previous runs.");
#endif
      if (fread(pointers->low_date, sizeof(char), sizeof(pointers->low_date) /
		sizeof(char), db) < sizeof(pointers->low_date) / sizeof(char))
	perror("parse_xferstats_databases: fread(low_date)");
      else if (fread(pointers->high_date, sizeof(char),
		     sizeof(pointers->high_date) / sizeof(char), db) <
	       sizeof(pointers->high_date) / sizeof(char))
	perror("parse_xferstats_databases: fread(high_date)");
      else if (fread(&pointers->data, sizeof(pointers->data), 1, db) < 1)
	perror("parse_xferstats_databases: fread(pointers->data)");
      else if (fread(&pointers->file_count, sizeof(pointers->file_count), 1,
		     db) < 1)
	perror("parse_xferstats_databases: fread(pointers->file_count)");

      fclose(db);
    }
  else
    return;
#if 0
    {
      perror("parse_xferstats_databases: fopen(totals.xdb)");
      return;
    }
#endif

#if 0
  if ((db = fopen("daily.xdb", "r")))
    {
      xfmisc_t * current_day_ptr = pointers->first_day_ptr, * new_day_ptr;

#ifdef DEBUGS
      fputc('.', stderr);
#endif
      if (current_day_ptr)
	for (; current_day_ptr->next_ptr;
	     current_day_ptr = current_day_ptr->next_ptr)
	  ;

      while (!feof(db))
	{
	  new_day_ptr = G_MEM_CHUNK_ALLOC(pointers->xfmisc_chunk);

	  if (fread(new_day_ptr->name, sizeof(char),
		    sizeof(new_day_ptr->name) / sizeof(char), db) <
	      sizeof(new_day_ptr->name) / sizeof(char))
	    {
#ifndef SMFS
	    g_free(new_day_ptr);
#endif
	    }
	  else if (fread(&new_day_ptr->data, sizeof(new_day_ptr->data), 1, db)
		   < 1)
	    perror("parse_xferstats_databases: fread(day->data)");
	  else if (fread(&new_day_ptr->file_count,
			 sizeof(new_day_ptr->file_count), 1, db) < 1)
	    perror("parse_xferstats_databases: fread(day->file_count)");
	  else if (fread(&new_day_ptr->seconds, sizeof(new_day_ptr->seconds),
			 1, db) < 1)
	    perror("parse_xferstats_databases: fread(day->seconds)");
	  else
	    {
	      if (current_day_ptr)
		{
		  current_day_ptr->next_ptr = new_day_ptr;
		  pointers->last_day_ptr = current_day_ptr = new_day_ptr;
		}
	      else
		pointers->last_day_ptr = pointers->first_day_ptr =
		  current_day_ptr = new_day_ptr;

	      new_day_ptr->next_ptr = NULL;
	    }
	}

      fclose(db);
    }
  else
    {
      perror("parse_xferstats_databases: fopen(daily.xdb)");
      return;
    }
#endif

#if 0
  if ((db = fopen("dow.xdb", "r")))
    {
#ifdef DEBUGS
      fputc('.', stderr);
#endif
      if (fread(pointers->weekday_data, sizeof(xfmult_t), 7, db) < 7)
	{
	  perror("parse_xferstats_databases: fread(weekday_data)");
	  return;
	}

      fclose(db);
    }
  else
    {
      perror("parse_xferstats_databases: fopen(dow.xdb)");
      return;
    }

  if ((db = fopen("hourly.xdb", "r")))
    {
#ifdef DEBUGS
      fputc('.', stderr);
#endif
      if (fread(pointers->hour_data, sizeof(xfmult_t), 24, db) < 24)
	{
	  perror("parse_xferstats_databases: fread(hour_data)");
	  return;
	}

      fclose(db);
    }
  else
    {
      perror("parse_xferstats_databases: fopen(hourly.xdb)");
      return;
    }

  if ((db = fopen("monthly.xdb", "r")))
    {
#ifdef DEBUGS
      fputc('.', stderr);
#endif
      if (fread(pointers->month_data, sizeof(xfmult_t), 12, db) < 12)
	{
	  perror("parse_xferstats_databases: fread(month_data)");
	  return;
	}

      fclose(db);
    }
  else
    {
      perror("parse_xferstats_databases: fopen(monthly.xdb)");
      return;
    }
#endif

  for (type = T_FILE; type <= T_SIZE; type++)
    {
      xfmisc_t * new_data_ptr;
      void *** pointers_ptr, ** current_data_ptr;
      int num = 0, size = 128;

      switch(type)
	{
	case T_FILE:
	  if (!(db = fopen("file.xdb", "r")))
	    return;
	  pointers_ptr = &pointers->first_file_ptr;
	  current_data_ptr = pointers->first_file_ptr;
	  break;
	case T_DIR:
	  if (!(db = fopen("dir.xdb", "r")))
	    return;
	  pointers_ptr = &pointers->first_dir_ptr;
	  current_data_ptr = pointers->first_dir_ptr;
	  break;
	case T_HOST:
	  if (!(db = fopen("host.xdb", "r")))
	    return;
	  pointers_ptr = &pointers->first_host_ptr;
	  current_data_ptr = pointers->first_host_ptr;
	  break;
	case T_TLD:
	  if (!(db = fopen("tld.xdb", "r")))
	    return;
	  pointers_ptr = &pointers->first_tld_ptr;
	  current_data_ptr = pointers->first_tld_ptr;
	  break;
	case T_DOMAIN:
	  if (!(db = fopen("domain.xdb", "r")))
	    return;
	  pointers_ptr = &pointers->first_domain_ptr;
	  current_data_ptr = pointers->first_domain_ptr;
	  break;
	case T_SIZE:
	  if (!(db = fopen("size.xdb", "r")))
	    return;
	  pointers_ptr = &pointers->first_size_ptr;
	  current_data_ptr = pointers->first_size_ptr;
	  break;
	default:
	  return;
	  break;
	}
      
#ifdef DEBUGS
      fputc('.', stderr);
#endif
      
      if (current_data_ptr)
	for (; current_data_ptr; current_data_ptr = *pointers_ptr[++num])
	  num++;
      
      while (!feof(db))
	{
	  new_data_ptr = G_MEM_CHUNK_ALLOC(pointers->xfmisc_chunk);
	  
	  fgets(tmpstr, sizeof(tmpstr), db);
	  if (!tmpstr[0])
	    {
#ifndef SMFS
	      g_free(new_data_ptr);
#endif
	    }
	  else
	    {
	      new_data_ptr->name = g_strdup(tmpstr);
	      printf("tmpstr: %s\n", tmpstr);
	      if (fread(&new_data_ptr->data, sizeof(new_data_ptr->data), 1, db)
		  < 1)
		{
		  printf("(%d) type: %d\n",
			 tmpstr[0], type);
		  perror("parse_xferstats_databases: fread(misc->data)");
		}
	      else if (fread(&new_data_ptr->file_count,
			     sizeof(new_data_ptr->file_count), 1, db) < 1)
		perror("parse_xferstats_databases: fread(misc->file_count)");
	      else if (fread(&new_data_ptr->seconds,
			     sizeof(new_data_ptr->seconds), 1, db) < 1)
		perror("parse_xferstats_databases: fread(misc->seconds)");
	      else
		{
		  if (*pointers_ptr)
		    {
		      if (num >= size - 1)
			*pointers_ptr =
			  (void **)g_realloc(*pointers_ptr, (size += 128)
					     * sizeof(void *));
		    }
		  else
		    *pointers_ptr = g_malloc(128 * sizeof(void *));
		  
		  (*pointers_ptr)[num++] = (void *)new_data_ptr;
		}
	    }
	}

      if (*pointers_ptr)
	(*pointers_ptr)[num + 1] = NULL;

      fclose(db);
    }

#ifdef DEBUGS
      fputc('\n', stderr);
#endif
} /* parse_xferstats_databases */


static void
usage(char bad_arg)
{
  if (bad_arg)
    printf("Invalid argument -- %c\n", bad_arg);

  printf("%s\n", VERSION);
  printf("usage: xferstats [options] [filename(s)]\n\n"
	 "Any command line arguments inside <> are required, inside [] are optional.  All\n"
	 "defaults listed assume that the configuration file has not changed these\n"
	 "settings.  If in doubt, check the configuration file or explicitly set them on\n"
	 "the command line.\n"
	 "\n"
	 " -                    get the log from a file (default)\n"
	 " +                    get the log from stdin\n"
	 " -c <config file>     specify a path and filename for the configuration file\n"
	 " -T <number>          logfile type (wu-ftpd = 0, ncftpd = 1)\n"
	 "\n"
	 "The following options are enabled with a \"+\" and disabled with a \"-\".  See the\n"
	 "man page for more information.  Any arguments apply only to enabling:\n"
	 "\n"
	 "  C                   Strict data validity checking\n"
	 "  H                   HTML output\n"
	 "  n			no HTML headers option (see man page)\n"
	 "  s			single-page output (see man page)\n"
	 "  r                   real user data\n"
	 "  a                   anonymous user data\n"
	 "  g                   guest user data\n"
	 "  i                   inbound traffic data\n"
	 "  u                   outbound traffic data\n"
	 "  h                   report on hourly traffic\n"
	 "  m                   report on monthly traffic\n"
	 "  S                   report on file size traffic\n"
	 "  w                   report on days-of-the-week traffic\n"
	 "  M                   report on days-of-the-month traffic\n"
	 "  f [number]          report on file traffic\n"
	 "  d [number]          report on directory traffic\n"
	 "  t [number]          report on top-level domain traffic\n"
	 "  O [number]          report on domain traffic\n"
	 "  o [number]          report on host traffic\n"
	 "\n"
	 "\n"
	 " +L <number>          limit the report-by-day report to <number> listings\n"
	 " +A                   include all users, all reports\n"
#if 0 /* This will have to wait for another version */
	 " +T <TLD>             report only on traffic from the <TLD> top-level domain\n"
#endif
	 " -D <number>          depth of path detail for directories (default 3)\n"
	 " +D <directory>       directory to report on, for example: +D /pub will\n"
	 "                      report only on paths under /pub\n"
	 " +B <timestamp>       the starting date for all reports (all prior transfers\n"
	 "                      will be filtered).  Jan DD HH:MM:ss YYYY\n"
	 " +E <timestamp>       the ending date, in the same format\n"
	 " +P <prefix>		strip the specified prefix from pathnames\n"
	 "\n"
	 " -v, --version        display version information\n"
	 " --help               this listing\n"
	 "\n"
	 "Report bugs to phil@off.net\n");
} /* usage */


static void
check_cmd_arg(int argc, char * argv[], int * i, int * j, unsigned int * value,
	      char * arg_str)
{
  long foo;

  if (argv[*i] + *j + 1 == '\0')
    {
      if (*i < (argc - 1) && *argv[*i + 1] != '-' && *argv[*i + 1] != '+') 
	{
	  if ((foo = atoi(argv[++*i])) < 0)
	    {
	      fprintf(stderr, "fatal: the %s parameter accepts only an "
		      "optional number (>= 0) following it.\n", arg_str);
	      exit(1);
	    }
	  else
	    {
	      *value = foo;
	      *j = strlen(argv[*i]);
	    }
	}
    }
  else
    if ((foo = atoi(argv[*i] + *j + 1)) < 0)
      {
	fprintf(stderr, "fatal: the %s parameter accepts only an optional "
		"number (>= 0) following it.\n", arg_str);
	exit(1);
      }
    else
      {
	*value = foo;
	*j = strlen(argv[*i]);
      }
} /* check_cmd_arg */


void
parse_cmdline(int argc, char *argv[], config_t * config)
{
  int i = 1, j, cmdline_logfile = 0;
  char c, no_more_options = 0, * logfile_name;
  long foo;
  only_dir_t * item;
  GSList * clist;
  
  for (; i < argc; i++)
    {
      if (!no_more_options && !strncmp(argv[i], "--", 2))
	{
	  /* argv[i] contains a long option (or is --) */
	  if (argv[i][2] == '\0')
	    no_more_options = 1;
	  else if (!strcmp(argv[i]+2, "help"))
	    {
	      usage('\0');
	      exit(0);
	    }
	  else if (!strcmp(argv[i]+2, "version"))
	    {
	      printf("%s\n", VERSION);
	      printf("Copyright (C) 1997-1999 Phil Schwan\n");
	      exit(0);
	    }
	}
      else if (!no_more_options && *argv[i] == '-')
	{
	  /* argv[i] contains a disabling option */
	  if (*(argv[i] + (j = 1)) == '\0')
	    config->use_stdin = 0;
	  while ((c = *(argv[i] + j)) != '\0')
	    {
	      switch (c)
		{
		case 'c':
		  /* this (config_file) was parsed in get_config_arg */
		  if (i < argc - 1)
		    /* set 'j' to be the pointer to the null at the end of the
		     * -next- argument (the one that contains the config file).
		     * I'm not going to explain this in detail every time I
		     * do this :) */
		    j = strlen(argv[++i]);
		  continue;
		case 'T':
		  if (i < argc - 1 && *argv[i + 1] != '-' &&
		      *argv[i + 1] != '+') 
		    {
		      if (!g_strcasecmp(argv[i + 1], "wu-ftpd") ||
			  !g_strcasecmp(argv[i + 1], "wuftpd") ||
			  !g_strcasecmp(argv[i + 1], "wu-ftp") ||
			  !g_strcasecmp(argv[i + 1], "wuftp"))
			config->log_type = 0;
		      else if (!g_strcasecmp(argv[i + 1], "ncftpd") ||
			       !g_strcasecmp(argv[i + 1], "ncftp"))
			config->log_type = 1;
		      else if (!g_strcasecmp(argv[i + 1], "apache"))
			  config->log_type = 2;
		      else
			{
			  fprintf(stderr, "fatal: the -T parameter requires a "
				  "log type following it.\n");
			  exit(1);
			}
		      j = strlen(argv[++i]);
		      continue;
		    }
		  else
		    {
		      fprintf(stderr, "fatal: the -T parameter requires a log "
			      "type following it.\n");
		      exit(1);
		    }
		case 'C':
		  config->strict_check =  0;
		  break;
		case 'H':
		  config->html_output = 0;
		  break;
		case 'n':
		  config->no_html_headers = 0;
		  break;
		case 's':
		  if (config->single_page)
		    g_free(config->single_page);
		  config->single_page = NULL;
		  break;
		case 'r':
		  config->real_traffic = 0;
		  break;
		case 'a':
		  config->anon_traffic = 0;
		  break;
		case 'g':
		  config->guest_traffic = 0;
		  break;
		case 'i':
		  config->inbound = 0;
		  break;
		case 'u':
		  config->outbound = 0;
		  break;
		case 'h':
		  config->hourly_traffic = 0;
		  break;
		case 'w':
		  config->dow_traffic = 0;
		  break;
		case 'M':
		  config->dom_traffic = 0;
		  break;
		case 't':
		  config->tld_traffic = 0;
		  break;
		case 'O':
		  config->domain_traffic = 0;
		  break;
		case 'o':
		  config->host_traffic = 0;
		  break;
		case 'm':
		  config->monthly_traffic = 0;
		  break;
		case 'S':
		  config->size_traffic = 0;
		  break;
		case 'd':
		  config->dir_traffic = 0;
		  break;
		case 'f':
		  config->file_traffic = 0;
		  break;
		case 'D':
		  /* we duplicate the stuff in check_cmd_arg because this
		   * arguments requires (instead of being optional) a
		   * value > 0 (instead of >= 0) */
		  if (*(argv[i] + j + 1) == '\0')
		    {
		      /* this is the end of this block of -xXxD so the argument
		       * is in the next arg... */
		      if (i < (argc - 1) && *argv[i + 1] != '-' &&
			  *argv[i + 1] != '+') 
			{
			  if ((foo = atoi(argv[i + 1])) < 0)
			    {
			      fprintf(stderr, "fatal: the -D parameter "
				      "requires a number (> 0) following "
				      "it.\n");
			      exit(1);
			    }
			  else
			    {
			      config->depth = foo;
			      j = strlen(argv[++i]);
			    }
			}
		      else
			{
			  fprintf(stderr, "fatal: the -D parameter requires a "
				  "number (> 0) following it.\n");
			  exit(1);
			}
		    }
		  else
		    if ((foo = atoi(argv[i] + j + 1)) < 0)
		      {
			fprintf(stderr, "fatal: the -D parameter requires a "
				"number (> 0) following it.\n");
			exit(1);
		      }
		    else
		      {
			config->depth = foo;
			j = strlen(argv[i]);
		      }
		  continue;
		case 'v':
                  printf("%s\n", VERSION);
                  printf("Copyright (C) 1997-1999 Phil Schwan\n");
                  exit(0);
		default:
		  usage(argv[i][j]);
		  exit(1);
		}
	      j++;
	    }
	}
      else if (!no_more_options && *argv[i] == '+')
	{
	  /* argv[i] contains an enabling option */
	  if (*(argv[i] + (j = 1)) == '\0')
	    config->use_stdin = 1;
	  while ((c = *(argv[i] + j)) != '\0')
	    {
	      switch (c)
		{
		case 'C':
		  config->strict_check = 2;
		  break;
		case 'a':
		  config->anon_traffic = 2;
		  break;
		case 'g':
		  config->guest_traffic = 2;
		  break;
		case 'r':
		  config->real_traffic = 2;
		  break;
		case 'i':
		  config->inbound = 2;
		  break;
		case 'u':
		  config->outbound = 2;
		  break;
		case 'h':
		  config->hourly_traffic = 2;
		  break;
		case 'w':
		  config->dow_traffic = 2;
		  break;
		case 'M':
		  config->dom_traffic = 2;
		  break;
		case 'm':
		  config->monthly_traffic = 2;
		  break;
		case 'S':
		  config->size_traffic = 2;
		  break;
		case 'd':
		  config->dir_traffic = 2;
		  check_cmd_arg(argc, argv, &i, &j,
				&config->number_dir_stats, "+d");
		  continue;
		case 'O':
		  config->domain_traffic = 2;
		  check_cmd_arg(argc, argv, &i, &j,
				&config->number_domain_stats, "+O");
		  continue;
		case 'o':
		  config->host_traffic = 2;
		  check_cmd_arg(argc, argv, &i, &j,
				&config->number_host_stats, "+o");
		  continue;
		case 't':
		  config->tld_traffic = 2;
		  check_cmd_arg(argc, argv, &i, &j,
				&config->number_tld_stats, "+d");
		  continue;
		case 'f':
		  config->file_traffic = 2;
		  check_cmd_arg(argc, argv, &i, &j,
				&config->number_file_stats, "+f");
		  continue;
		case 'L':
		  check_cmd_arg(argc, argv, &i, &j,
				&config->number_daily_stats, "+L");
		  continue;
		case 'R':
		  check_cmd_arg(argc, argv, &i, &j,
				&config->max_report_size, "+R");
		  continue;
		case 'H':
		  config->html_output = 1;
		  break;
		case 'n':
		  config->no_html_headers = 1;
		  break;
		case 's':
		  if (config->single_page)
		    g_free(config->single_page);
		  config->single_page = g_strdup("xferstats.html");
		  break;
		case 'D':
		  if (i < (argc - 1) && *argv[i + 1] != '-' &&
		      *argv[i + 1] != '+')
		    {
		      item = g_malloc(sizeof(only_dir_t));

		      item->dir = g_strdup(argv[i + 1]);
		      item->len = strlen(argv[i + 1]);

		      config->only_dir = g_slist_append(config->only_dir,
							item);
		    }
		  else
		    {
		      fprintf(stderr, "fatal: the +D parameter requires a "
			      "path following it.\n");
		      exit(1);
		    }
		  j = strlen(argv[++i]);
		  continue;
		case 'P':
		  if (i < (argc - 1) && *argv[i + 1] != '-' &&
		      *argv[i + 1] != '+')
		    {
			/* FIXME: allow a list of stripped prefixes? */
			if (config->strip_prefix != NULL)
				g_free(config->strip_prefix);

			config->strip_prefix =
				g_malloc((config->strip_prefix_len =
					  strlen(argv[i + 1])) + 1);
			strcpy(config->strip_prefix, argv[i + 1]);
		    }
		  else
		    {
		      fprintf(stderr, "fatal: the +P parameter requires a "
			      "string following it.\n");
		      exit(1);
		    }
		  j = strlen(argv[++i]);
		  continue;
		case 'B':
		  if (i < (argc - 1) && *argv[i + 1] != '-' &&
		      *argv[i + 1] != '+')
		    {
		      char * bar = argv[i + 1];

		      if (strlen(argv[i + 1]) != 20)
			{
			  fprintf(stderr, "fatal: the date must be in the "
				  "Jan DD HH:MM:ss YYYY format\n");
			  exit(1);
			}

		      if (bar[3] != ' ' || bar[6] != ' ' ||
			  bar[15] != ' ' || bar[9] != ':' ||
			  bar[12] != ':' || !isdigit(bar[5]) ||
			  !isdigit(bar[7]) || !isdigit(bar[8]) ||
			  !isdigit(bar[10]) || !isdigit(bar[11]) ||
			  !isdigit(bar[13]) || !isdigit(bar[14]) ||
			  !isdigit(bar[16]) || !isdigit(bar[17]) ||
			  !isdigit(bar[18]) || !isdigit(bar[19]) ||
			  !(bar[4] == ' ' || bar[4] == '0' ||
			    bar[4] == '1' || bar[4] == '2' ||
			    bar[4] == '3'))
			{
			  fprintf(stderr, "fatal: the date must be in the "
				  "Jan DD HH:MM:ss YYYY format\n");
			  exit(1);
			}
		      memset(config->begin_date, ' ', 4);
		      memcpy(config->begin_date + 4, argv[i + 1], 21);
		    }
		  else
		    {
		      fprintf(stderr, "fatal: the +B parameter requires a "
			      "path following it.\n");
		      exit(1);
		    }
		  j = strlen(argv[++i]);
		  continue;
		case 'E':
		  if (i < (argc - 1) && *argv[i + 1] != '-' &&
		      *argv[i + 1] != '+')
		    {
		      char * bar = argv[i + 1];

		      if (strlen(argv[i + 1]) != 20)
			{
			  fprintf(stderr, "fatal: the date must be in the "
				  "Jan DD HH:MM:ss YYYY format\n");
			  exit(1);
			}

		      if (bar[3] != ' ' || bar[6] != ' ' ||
			  bar[15] != ' ' || bar[9] != ':' ||
			  bar[12] != ':' || !isdigit(bar[5]) ||
			  !isdigit(bar[7]) || !isdigit(bar[8]) ||
			  !isdigit(bar[10]) || !isdigit(bar[11]) ||
			  !isdigit(bar[13]) || !isdigit(bar[14]) ||
			  !isdigit(bar[16]) || !isdigit(bar[17]) ||
			  !isdigit(bar[18]) || !isdigit(bar[19]) ||
			  !(bar[4] == ' ' || bar[4] == '0' ||
			    bar[4] == '1' || bar[4] == '2' ||
			    bar[4] == '3'))
			{
			  fprintf(stderr, "fatal: the date must be in the "
				  "Jan DD HH:MM:ss YYYY format\n");
			  exit(1);
			}
		      memset(config->end_date, ' ', 4);
		      memcpy(config->end_date + 4, argv[i + 1], 21);
		    }
		  else
		    {
		      fprintf(stderr, "fatal: the +E parameter requires a "
			      "path following it.\n");
		      exit(1);
		    }
		  j = strlen(argv[++i]);
		  continue;
#if 0 /* This will have to wait for another version */
		case 'T':
		  if (i < (argc - 1) && *argv[i + 1] != '-' &&
		      *argv[i + 1] != '+')
		    if (*argv[i + 1] != '/')
		      {
			config->only_dir = g_malloc((config->only_dir_length =
				   strlen(argv[i + 1]) + 1) + 1);
			config->only_dir[0] = '/';
        		memcpy(config->only_dir + 1, argv[i + 1],
			       config->only_dir_length);
		      }
		    else
        	      {
			config->only_dir = g_strdup(argv[i + 1]);
			config->only_dir_length = strlen(argv[i + 1]);
		      }
		  else
		    {
		      fprintf(stderr, "fatal: the +T parameter requires a "
			      "top-level domain following it.\n");
		      exit(1);
		    }
		  j = strlen(argv[++i]);
		  continue;
#endif
		case 'A':
		  config->anon_traffic = 2;
		  config->real_traffic = 2;
		  config->guest_traffic = 2;
		  config->inbound = 2;
		  config->outbound = 2;

		  config->hourly_traffic = 2;
		  config->monthly_traffic = 2;

		  config->tld_traffic = 2;
		  config->host_traffic = 2;
		  config->domain_traffic = 2;
		  config->dir_traffic = 2;
		  config->file_traffic = 2;
		  config->size_traffic = 2;

		  config->dow_traffic = 2;
		  config->dom_traffic = 2;
		  break;
		default:
		  usage(argv[i][j]);
		  exit(1);
		}
	      j++;
	    }
	}
      else
	{
	  /* it's a filename */
	  logfile_name = g_strdup(argv[i]);

	  if (!cmdline_logfile)
	    {
	      /* this is the first logfile that we've gotten from the command
	       * line.  destroy any existing logfiles, as they're logfiles that
	       * are from the config file, and we don't want those! */
	      cmdline_logfile = 1;

	      if (config->logfiles)
		{
		  for (clist = config->logfiles; clist; clist = clist->next)
		    g_free(clist->data);

		  g_slist_free(config->logfiles);
		  config->logfiles = NULL;
		}
	    }

	  config->logfiles = g_slist_append(config->logfiles, logfile_name);
	  /* as soon as we get a filename, we're done getting options */
	  no_more_options = 1;
	}
    }
} /* parse_cmdline */


void
get_config_arg(int argc, char *argv[], pointers_t *pointers)
{
  int argno = 0;

  for (; argno < argc; argno++)
    if (!strcmp(argv[argno], "-c") && argno < (argc - 1))
      {
	if (!(pointers->config->config_file = g_strdup(argv[argno + 1])))
	  {
	    perror("get_config_arg: g_strdup");
	    exit(0);
	  }
	return;
      }
} /* get_config_arg */


void
xferstats_init(pointers_t *pointers)
{
  pointers->xfmisc_chunk = G_MEM_CHUNK_NEW("xfmisc_t mem chunk",
					   sizeof(xfmisc_t),
					   1024, G_ALLOC_ONLY);
  pointers->ftpentry_chunk = G_MEM_CHUNK_NEW("ftp_entry_t mem chunk",
					     sizeof(ftp_entry_t),
					     1024, G_ALLOC_ONLY);

  pointers->config->only_dir =
    pointers->config->logfiles = NULL;

  /* initialize our config structure with remaining default values */
  pointers->config->log_type = 1;
  pointers->config->number_file_stats =
    pointers->config->number_daily_stats =
    pointers->config->number_dir_stats =
    pointers->config->number_tld_stats =
    pointers->config->number_domain_stats =
    pointers->config->number_host_stats = 50;

  pointers->config->real_traffic =
    pointers->config->anon_traffic =
    pointers->config->guest_traffic =
    pointers->config->inbound =
    pointers->config->outbound =
    pointers->config->tld_traffic =
    pointers->config->domain_traffic =
    pointers->config->host_traffic =
    pointers->config->hourly_traffic =
    pointers->config->dow_traffic =
    pointers->config->dom_traffic =
    pointers->config->dir_traffic =
    pointers->config->file_traffic =
    pointers->config->monthly_traffic =
    pointers->config->size_traffic =
    pointers->config->use_stdin =
    pointers->config->html_output =
    pointers->config->no_html_headers =
    pointers->config->strip_prefix_len =
    pointers->config->strict_check = 0;

  /* -1 is an invalid value which will trigger the defaults */
  pointers->config->file_sort_pref =
    pointers->config->dir_sort_pref =
    pointers->config->domain_sort_pref =
    pointers->config->tld_sort_pref =
    pointers->config->host_sort_pref =
    pointers->config->dom_sort_pref =
    pointers->config->dow_sort_pref =
    pointers->config->hourly_sort_pref =
    pointers->config->daily_sort_pref =
    pointers->config->monthly_sort_pref =
    pointers->config->size_sort_pref = -1;

  pointers->config->max_report_size = 30;
  pointers->config->depth = 4;
  pointers->low_date[0] =
    pointers->high_date[0] =
    pointers->config->begin_date[0] =
    pointers->config->end_date[0] = 0;

#if 0
  /* This will have to wait for another version */
  pointers->config->only_tld = NULL;
  pointers->config->only_tld_length = 0;
#endif

  pointers->config->config_file =
    pointers->config->single_page =
    pointers->config->graph_path =
    pointers->config->link_prefix =
    pointers->config->html_location =
    pointers->config->strip_prefix =
    pointers->config->additive_db = NULL;

  /* initialize the pointers and clear the totals/dates */
  pointers->first_ftp_line = NULL;
  pointers->first_day_ptr = NULL;
  pointers->first_tld_ptr =
    pointers->first_domain_ptr =
    pointers->first_host_ptr =
    pointers->first_file_ptr =
    pointers->first_dir_ptr =
    pointers->first_size_ptr = NULL;
  pointers->file_count =
    pointers->data =
    pointers->config->refresh =
    pointers->config->chunk_input = 0;

  pointers->file_ht =
    pointers->tld_ht =
    pointers->domain_ht =
    pointers->host_ht =
    pointers->dir_ht =
    pointers->size_ht = NULL;

  pointers->done = 0;

  /* allocate and clean out these arrays.  these arrays are generated in this
   * fashion instead of statically so that I can use libc's qsort().  I hope
   * the compiler can optimize some of this crap :). */
  {
    int a;

    pointers->hour_data = g_malloc(24 * sizeof(void *));
    for (a = 0; a < 24; a++)
      {
	pointers->hour_data[a] = G_MEM_CHUNK_ALLOC0(pointers->xfmisc_chunk);
	pointers->hour_data[a]->name = (char *)NUMBERS[a];
      }

    pointers->weekday_data = g_malloc(7 * sizeof(void *));
    for (a = 0; a < 7; a++)
      {
	pointers->weekday_data[a] = G_MEM_CHUNK_ALLOC0(pointers->xfmisc_chunk);
	pointers->weekday_data[a]->name = (char *)LONG_DAYS[a];
      }

    pointers->dom_data = g_malloc(31 * sizeof(void *));
    for (a = 0; a < 31; a++)
      {
	pointers->dom_data[a] = G_MEM_CHUNK_ALLOC0(pointers->xfmisc_chunk);
	pointers->dom_data[a]->name = (char *)NUMBERS[a];
      }

    pointers->month_data = g_malloc(12 * sizeof(void *));
    for (a = 0; a < 12; a++)
      {
	pointers->month_data[a] = G_MEM_CHUNK_ALLOC0(pointers->xfmisc_chunk);
	pointers->month_data[a]->name = (char *)LONG_MONTHS[a];
      }
  }
} /* xferstats_init */


int
main(int argc, char **argv)
{
  pointers_t * pointers;
  char * default_logfile;

  /* unbuffer stdout and stderr so output isn't lost */
  setbuf(stdout, NULL);
  setbuf(stderr, NULL);
  
  /* allocate memory for our pointers */
  pointers = g_malloc(sizeof(pointers_t));
  pointers->config = g_malloc(sizeof(config_t));

  /* init lots and lots of vars */
  xferstats_init(pointers);

  /* grab the -c <configfile> argument if it exists */
  get_config_arg(argc, argv, pointers);

  /* do the config-file thing */
  init_config(pointers);

  /* change the config structure based on command line args */
  parse_cmdline(argc, argv, pointers->config);

  /* make sure our max report size is something sane (>= 10 or 0) */
  if (pointers->config->max_report_size &&
      pointers->config->max_report_size < 10)
    pointers->config->max_report_size = 10;

  /* load any databases that may exist */
  if (pointers->config->additive_db)
    {
      parse_xferstats_databases(pointers);
      /* something else--if we have additive_db turned on, we must zero out the
       * stat limits  so that all are processed */
      pointers->config->number_file_stats =
	pointers->config->number_daily_stats =
	pointers->config->number_dir_stats =
	pointers->config->number_tld_stats =
	pointers->config->number_domain_stats =
	pointers->config->number_host_stats = 0;
    }

  if (!pointers->config->logfiles)
    {
      /* no logfiles have been specified in the config file or on the command
       * line, add the default */
      pointers->config->logfiles = NULL;
      if (!(default_logfile = g_strdup(FILENAME)))
	{
	  perror("main: g_strdup(FILENAME)");
	  exit(1);
	}
      pointers->config->logfiles = g_slist_append(pointers->config->logfiles,
						  default_logfile);
    }

  /* load the xferlog into ftp_line structures */
  while (!pointers->done) {
    switch(pointers->config->log_type) {
    case 0:
      if (!parse_wuftpd_logfile(pointers))
	pointers->done = 1;
      break;
    case 1:
      if (!parse_ncftpd_logfile(pointers))
	pointers->done = 1;
      break;
    case 2:
      if (!parse_apache_logfile(pointers))
	pointers->done = 1;
      break;
    }

    G_BLOW_CHUNKS();

    if (!pointers->first_ftp_line) {
      fprintf(stderr, "No data to process.\n");
      exit(0);
    }

    generate_daily_data(pointers, pointers->first_ftp_line);
    if (pointers->config->dow_traffic)
      generate_dow_data(pointers, pointers->first_ftp_line);
    if (pointers->config->dom_traffic)
      generate_dom_data(pointers, pointers->first_ftp_line);
    if (pointers->config->monthly_traffic)
      generate_monthly_data(pointers, pointers->first_ftp_line);
    if (pointers->config->file_traffic)
      generate_misc_data(pointers, pointers->first_ftp_line, T_FILE);
    if (pointers->config->dir_traffic)
      generate_misc_data(pointers, pointers->first_ftp_line, T_DIR);
    if (pointers->config->host_traffic)
      generate_misc_data(pointers, pointers->first_ftp_line, T_HOST);
    if (pointers->config->domain_traffic)
      generate_misc_data(pointers, pointers->first_ftp_line, T_DOMAIN);
    if (pointers->config->tld_traffic)
      generate_misc_data(pointers, pointers->first_ftp_line, T_TLD);
    if (pointers->config->hourly_traffic)
      generate_hourly_data(pointers, pointers->first_ftp_line);
    if (pointers->config->size_traffic)
      generate_misc_data(pointers, pointers->first_ftp_line, T_SIZE);
  }

  display_daily_totals(pointers);
  if (pointers->config->dow_traffic)
    display_dow_totals(pointers);
  if (pointers->config->dom_traffic)
    display_dom_totals(pointers);
  if (pointers->config->monthly_traffic)
    display_monthly_totals(pointers);
  if (pointers->config->file_traffic)
    display_misc_totals(pointers, T_FILE, "file.html");
  if (pointers->config->dir_traffic)
    display_misc_totals(pointers, T_DIR, "dir.html");
  if (pointers->config->host_traffic)
    display_misc_totals(pointers, T_HOST, "host.html");
  if (pointers->config->domain_traffic)
    display_misc_totals(pointers, T_DOMAIN, "domain.html");
  if (pointers->config->tld_traffic)
    display_misc_totals(pointers, T_TLD, "tld.html");
  if (pointers->config->hourly_traffic)
    display_hourly_totals(pointers);
  if (pointers->config->size_traffic)
    display_misc_totals(pointers, T_SIZE, "size.html");

#ifdef DEBUGS
  /* display memory information to stderr */
  g_mem_chunk_info();
  g_mem_profile();
#endif
  return 0;
} /* main */
