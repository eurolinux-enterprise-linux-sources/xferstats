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
#endif

#include <stdlib.h>
#include <stdio.h>

#include <glib.h>

#include "xferstats.h"

extern const char DAYS[7][3];
extern const char LONG_DAYS[7][10];
extern const char LONG_MONTHS[12][10];
extern const char NUMBERS[31][3];
extern const char T_NAMES[6][10];

char *
big_number(unsigned LONGLONG bignum, char * buffer, int * size)
{
  /* 2^64 == 18446744073709551616 hence tmpstr needs to be 21 bytes */
  char tmpstr[21];
  int a, b, len;

  len = sprintf(tmpstr, "%" LL_FMT, bignum);

  /* make a = the final length of the string (numbers plus commas) */
  a = len + (b = (len / 3));
  /* allocate ram for the string */
  if (*size < a + 1)
    {
      if (buffer)
	buffer = g_realloc(buffer, a + 1);
      else
	buffer = g_malloc(a + 1);
      *size = a + 1;
    }

  /* copy the digits before the first comma (if any) */
  strncpy(buffer, tmpstr, (a = len % 3));
  buffer[a] = '\0';

  /* loop until there are no more blocks of 3 numbers left */
  for (; b > 0; b--, a += 3)
    {
      /* only put a comma in if there were digits before */
      if (a)
	strcat(buffer, ",");
      /* copy the next 3 numbers into out string */
      strncat(buffer, tmpstr + a, 3);
    }

  return buffer;
} /* big_number */


void
html_graph(float percent, FILE *html, char *path)
{
  float foo;
  char printed = 0;

  if (percent >= 1)
    printed = 1;  

  for (foo = 64; foo >= 1; foo /= 2)
    if (percent > foo)
      {
	fprintf(html, "<IMG SRC=\"%sg%d.jpg\" ALT=\"\">", path ? path : "",
		(int) foo);
	percent -= foo;
      }

  if (percent >= .1)
    printed = 1;

  for (foo = .8; foo >= .1; foo /= 2.0)
    if (percent > foo)
      {
	printed = 1;
	fprintf(html, "<IMG SRC=\"%sg_%d.jpg\" ALT=\"\">",
		path ? path : "", (int) (foo * 10));
	percent -= foo;
      }

  if (!printed)
    fprintf(html, "&nbsp");

} /* html_graph */


void
display_daily_totals(pointers_t * pointers)
{
  xfmisc_t * current_day_ptr;
  char * buffer = NULL, filename[MAXPATHLEN];
  int displayed_count = 0, count = 0, size = 0;
  unsigned long num_days = 0;
  double percent;
  unsigned LONGLONG highest = 0;
  FILE * html =  NULL;

#ifdef DEBUGS
  fprintf(stderr, "Beginning display_daily_totals\n");
#endif

  strncpy(pointers->low_date + 10, pointers->low_date + 19, 5);
  strncpy(pointers->high_date + 10, pointers->high_date + 19, 5);
  pointers->low_date[15] = pointers->high_date[15] = '\0';

  for (num_days = 0; pointers->first_day_ptr[num_days]; num_days++)
    if (pointers->first_day_ptr[num_days]->data > highest)
      highest = pointers->first_day_ptr[num_days]->data;
  
  /* dump our totals information--here is as good a place as any */
  if (pointers->config->html_output)
    {
      if (pointers->config->single_page)
        {
	  g_snprintf(filename, MAXPATHLEN - 1, "%s%s",
		     pointers->config->html_location ?
		     pointers->config->html_location : "",
		     pointers->config->single_page);
	  if (!(html = fopen(filename, "w")))
	    {
	      perror("fopen");
	      exit(1);
	    }
        }
      else
	{
	  g_snprintf(filename, MAXPATHLEN - 1, "%stotals.html",
		     pointers->config->html_location ?
		     pointers->config->html_location : "");
	  if (!(html = fopen(filename, "w")))
	    {
	      perror("fopen");
	      exit(1);
	    }
	}
      /* first for HTML */
      if (!pointers->config->no_html_headers && !pointers->config->single_page)
	fprintf(html, "<HTML>"); 
      if (!pointers->config->no_html_headers) {
	fprintf(html, "<TITLE>xferstats: %s to %s</TITLE>\n",
		pointers->low_date, pointers->high_date);
        if (pointers->config->refresh)
	  fprintf(html, "<META HTTP-EQUIV=\"Refresh\" CONTENT=%d>\n",
		  pointers->config->refresh);
      }
      if (!pointers->config->no_html_headers && !pointers->config->single_page)
	fprintf(html, "<BODY>");

      fprintf(html, "<CENTER><H1>TOTALS FOR SUMMARY PERIOD <I>%s</I> TO "
	      "<I>%s</I> (%s days)</H1><HR>\n",
	      pointers->low_date, pointers->high_date,
	      (buffer = big_number(num_days, buffer, &size)));
      fprintf(html, "<TABLE BORDER = 0 WIDTH = 70%%>\n");
      fprintf(html, "<TR><TH ALIGN=LEFT>Files Transmitted During Summary "
	      "Period</TH><TH ALIGN=RIGHT>%s</TH></TR>\n",
	      (buffer = big_number(pointers->file_count, buffer, &size)));
      fprintf(html, "<TR><TH ALIGN=LEFT>Average Files Transmitted Daily</TH>"
	      "<TH ALIGN=RIGHT>%s</TH><TR>\n",
	      (buffer =
	       big_number(pointers->file_count / num_days, buffer, &size)));
      fprintf(html, "<TR><TH ALIGN=LEFT>Bytes Transmitted During Summary "
	      "Period</TH><TH ALIGN=RIGHT>%s</TH></TR>\n",
	      (buffer = big_number(pointers->data, buffer, &size)));
      fprintf(html, "<TR><TH ALIGN=LEFT>Average Bytes Transmitted Per File"
	      "</TH><TH ALIGN=RIGHT>%s</TH></TR>\n",
	      (buffer = big_number(pointers->data/pointers->file_count,
				   buffer, &size)));
      fprintf(html, "<TR><TH ALIGN=LEFT>Average Bytes Transmitted Daily</TH>"
	      "<TH ALIGN=RIGHT>%s</TH></TR></TABLE><BR>\n",
	      (buffer = big_number(pointers->data / num_days, buffer, &size)));
      if (!pointers->config->single_page)
	{
	  fprintf(html, "<HR><A HREF=\"daily.html\">Daily Transmission "
		  "Statistics </A><BR>");
	  fprintf(html, "<A HREF=\"monthly.html\">Monthly Transmission "
		  "Statistics </A><BR>");
	  fprintf(html, "<A HREF=\"hour.html\">Hourly Transmission "
		  "Statistics</A> <BR>");
	  fprintf(html, "<A HREF=\"dow.html\">Day-of-the-Week Transmission "
		  "Statistics</A><BR>");
	  fprintf(html, "<A HREF=\"dom.html\">Day-of-the-Month Transmission "
		  "Statistics</A><BR><BR>");
	  fprintf(html, "<A HREF=\"host.html\">Transfer Statistics By Host</A>"
		  "<BR>");
	  fprintf(html, "<A HREF=\"domain.html\">Transfer Statistics By Domain"
		  "</A><BR>");
	  fprintf(html, "<A HREF=\"tld.html\">Transfer Statistics By Top Level"
		  " Domain</A><BR><BR>");
	  fprintf(html, "<A HREF=\"file.html\">Transfer Statistics By File</A>"
		  "<BR>");
	  fprintf(html, "<A HREF=\"size.html\">Transfer Statistics By File "
		  "Size</A><BR>");
	  fprintf(html, "<A HREF=\"dir.html\">Transfer Statistics By Directory"
		  "</A></CENTER>\n");
	}
      fprintf(html, "<BR><BR><font size=\"-1\"><I><CENTER>Statistics generated"
	      " by <A HREF=\"http://xferstats.off.net/\">xferstats</A> "
	      "%s</CENTER></I></FONT><BR>\n", VERSION);
      if (!pointers->config->single_page)
	{
	  if (!pointers->config->no_html_headers)
	    fprintf(html, "</BODY></HTML>\n");
	  fclose(html);
	  g_snprintf(filename, MAXPATHLEN - 1, "%sdaily.html",
		     pointers->config->html_location ?
		     pointers->config->html_location : "");
	  if (!(html = fopen(filename, "w")))
	    {
	      perror("fopen");
	      exit(1);
	    }
	}
      else
	fprintf(html, "<HR>");
      if (!pointers->config->no_html_headers) {
	  fprintf(html, "<TITLE>xferstats: %s to %s</TITLE><BODY>\n",
		  pointers->low_date, pointers->high_date);
          if (pointers->config->refresh)
            fprintf(html, "<META HTTP-EQUIV=\"Refresh\" CONTENT=%d>\n",
                    pointers->config->refresh);

	}
    }
  else
    {
      /* and now for text */
      printf("TOTALS FOR SUMMARY PERIOD %s TO %s\n\n", pointers->low_date,
	     pointers->high_date);
      printf("Files Transmitted During Summary Period     %16s\n",
	     (buffer = big_number(pointers->file_count, buffer, &size)));
      printf("Average Files Transmitted Daily             %16s\n",
	     (buffer = big_number(pointers->file_count / num_days, buffer,
				  &size)));
      printf("Bytes Transmitted During Summary Period     %16s\n\n",
	     (buffer = big_number(pointers->data, buffer, &size)));
      printf("Average Bytes Transmitted Per File          %16s\n",
	     (buffer = big_number(pointers->data/pointers->file_count, buffer,
				  &size)));
      printf("Average Bytes Transmitted Daily             %16s\n\n",
	     (buffer = big_number(pointers->data / num_days, buffer, &size)));
    }

  count = 0;
  if (pointers->config->daily_sort_pref == 2) {
	  /* skip all of the entries before the last 'n' dictated by
	   * number_daily_stats */
	  if (pointers->config->number_daily_stats)
		  count = num_days - pointers->config->number_daily_stats + 1;
	  if (count < 0) count = 0;
  }

  for (; (current_day_ptr = pointers->first_day_ptr[count]); count++)
    {
      displayed_count++;

      if (pointers->config->number_daily_stats &&
	  displayed_count >= pointers->config->number_daily_stats)
	break;

      if (displayed_count == 1 || !pointers->config->number_daily_stats ||
	  pointers->config->number_daily_stats > num_days ||
	  displayed_count > num_days - pointers->config->number_daily_stats)
	{
	  if (displayed_count == 1 ||
	      (pointers->config->max_report_size &&
	       !(displayed_count % (pointers->config->max_report_size + 1))))
	    {
	      /* print our headers */
	      if (pointers->config->html_output)
		{
		  /* first for HTML */

		  /* this isn't the first set of headers, so close the previous
		   * table */
		  if (displayed_count != 1)
		    fprintf(html, "</TABLE><BR><IMG SRC=\"%sg1.jpg\" "
			    "ALT=\"g1\"> = %s bytes<BR><HR>\n",
			    pointers->config->graph_path ?
			    pointers->config->graph_path : "",
			    (buffer =
			     big_number(highest / 50, buffer, &size)));
		  else
		    {
		      fprintf(html, "<CENTER>");

		      if (!pointers->config->number_daily_stats ||
			  pointers->config->number_daily_stats >= num_days)
			fprintf(html, "<U><H1>Daily Transmission Statistics "
				"for <I>%s</I> to <I>%s</I></H1></U><BR>\n",
				pointers->low_date, pointers->high_date);
		      else
			fprintf(html, "<U><H1>Daily Transmission Statistics "
				"(%d day maximum)</H1></U><BR>\n",
				pointers->config->number_daily_stats);
		    }
		  fprintf(html, "<TABLE BORDER=2><TR><TH></TH><TH>Number Of"
			  "</TH><TH>Number of</TH><TH>Average</TH><TH>Percent "
			  "Of</TH><TH>Percent Of</TH></TR>\n");
		  fprintf(html, "<TR><TH>Date</TH><TH>Files Sent</TH><TH>"
			  "Bytes Sent</TH><TH>Xmit Rate</TH><TH>Files Sent"
			  "</TH><TH>Bytes Sent</TH></TR>\n");
		}
	      else
		{
		  if (displayed_count == 1)
		    {
		      /* then for text */
		      if (!pointers->config->number_daily_stats ||
			  pointers->config->number_daily_stats >= num_days)
			printf("Daily Transmission Statistics\n\n");
		      else
			printf("Daily Transmission Statistics (%d day maximum)"
			       "\n\n", pointers->config->number_daily_stats);
		    }
		  else
		    putchar('\n');

		  printf("                 Number Of    Number of    Average   Percent Of  Percent Of\n");
		  printf("     Date        Files Sent  Bytes  Sent  Xmit Rate  Files Sent  Bytes Sent\n");
		  printf("---------------  ----------  -----------  ---------  ----------  ----------\n");
		}
	    }
	}

      if (pointers->config->html_output)
	{
	  fprintf(html, "<TR><TD NOWRAP>%s</TD><TD ALIGN=RIGHT>%u</TD>"
		  "<TD ALIGN=RIGHT>%s</TD><TD NOWRAP ALIGN=RIGHT>%5.1f KB/s"
		  "</TD><TD ALIGN=RIGHT>%10.2f</TD><TD ALIGN=RIGHT>%10.2f"
		  "</TD>", current_day_ptr->name, current_day_ptr->file_count,
		  (buffer = big_number(current_day_ptr->data, buffer, &size)),
		  (double) current_day_ptr->data /
		  current_day_ptr->seconds / 1024,
		  (double) current_day_ptr->file_count /
		  pointers->file_count * 100,
		 (double) current_day_ptr->data / pointers->data * 100);
	  percent = (double) current_day_ptr->data / highest * 50;
	  fprintf(html, "<TD NOWRAP ALIGN=LEFT>");
	  html_graph(percent, html, pointers->config->graph_path);
	  fprintf(html, "</TD></TR>\n");
	}
      else
	printf("%s  %10u  %11" LL_FMT "  %5.1f K/s  %10.2f  %10.2f\n",
	       current_day_ptr->name, current_day_ptr->file_count,
	       current_day_ptr->data,
	       (double) current_day_ptr->data /
	       current_day_ptr->seconds / 1024,
	       (double) current_day_ptr->file_count /
	       pointers->file_count * 100,
	       (double) current_day_ptr->data / pointers->data * 100);
    }

  if (pointers->config->html_output)
    {
      fprintf(html, "</TABLE><BR><IMG SRC=\"%sg1.jpg\" ALT=\"g1\"> = %s "
	      "bytes<BR>\n", pointers->config->graph_path ?
	      pointers->config->graph_path : "",
	      (buffer = big_number(highest / 50, buffer, &size)));
      if (!pointers->config->single_page)
	{
	  fprintf(html, "<HR><A HREF=\"totals.html\">Return to Totals and "
		  "Index </A></CENTER>\n");
	  if (!pointers->config->no_html_headers)
	    fprintf(html, "</BODY></HTML>\n");
	}
      fclose(html);
    }

  /* Don't leak the buffer used for big_number()! */
  g_free(buffer);

#ifdef DEBUGS
  fprintf(stderr, "display_daily_totals complete\n");
#endif
} /* display_daily_totals */


void
display_misc_totals(pointers_t * pointers, int type, char * htmlname)
{
  xfmisc_t * current_path_ptr;
  void ** pointers_ptr = NULL;
  unsigned int count, number = 0, a;
  char * buffer = NULL, filename[MAXPATHLEN], temp_name[55] = {0};
  double percent = 0;
  int size = 0;
  unsigned LONGLONG highest_count = 0;
  FILE *html = NULL;
  
#ifdef DEBUGS
  fprintf(stderr, "Beginning display_%s_totals\n", T_NAMES[type]);
#endif
  
  switch(type)
    {
    case T_DIR:
      number = pointers->config->number_dir_stats;
      pointers_ptr = pointers->first_dir_ptr;
      break;
    case T_FILE:
      number = pointers->config->number_file_stats;
      pointers_ptr = pointers->first_file_ptr;
      break;
    case T_TLD:
      number = pointers->config->number_tld_stats;
      pointers_ptr = pointers->first_tld_ptr;
      break;
    case T_HOST:
      number = pointers->config->number_host_stats;
      pointers_ptr = pointers->first_host_ptr;
      break;
    case T_DOMAIN:
      number = pointers->config->number_domain_stats;
      pointers_ptr = pointers->first_domain_ptr;
      break;
    case T_SIZE:
      number = 0;
      pointers_ptr = pointers->first_size_ptr;
      break;
    }

  if (*pointers_ptr == NULL)
    {
#ifdef DEBUGS
      fprintf(stderr, "No data to display.\n");
#endif
      return;
    }

  switch (type)
    {
    case T_DIR:
    case T_TLD:
    case T_HOST:
    case T_DOMAIN:
      highest_count = ((xfmisc_t *)pointers_ptr[0])->data;
      break;
    case T_FILE:
      highest_count = ((xfmisc_t *)pointers_ptr[0])->file_count;
      break;
    case T_SIZE:
      for (current_path_ptr = pointers_ptr[count = 0]; current_path_ptr;
	   current_path_ptr = pointers_ptr[++count])
	if (current_path_ptr->data > highest_count)
	  highest_count = current_path_ptr->data;
      break;
    }

  if (highest_count < 50)
    highest_count = 50;

  for (current_path_ptr = (xfmisc_t *)pointers_ptr[0], count = 1;
       current_path_ptr != NULL;
       current_path_ptr = (xfmisc_t *)pointers_ptr[count++])
    {
      if (number && count > number)
	break;
      
      if (count == 1 ||
	  (pointers->config->max_report_size &&
	   !(count % (pointers->config->max_report_size + 1))))
	{
	  if (pointers->config->html_output)
	    {
	      /* this isn't the first set of headers, so close the previous
	       * table */
	      if (count != 1)
		fprintf(html, "</TABLE><BR><IMG SRC=\"%sg1.jpg\" ALT=\"g1\">"
			" = %s %s<BR><HR>\n", pointers->config->graph_path ?
			pointers->config->graph_path : "",
			(buffer = big_number(highest_count / 50, buffer,
					     &size)), 
			(type == T_FILE) ? "transfers" : "bytes");
	      else
		{
		  if (pointers->config->single_page)
		    {
		      g_snprintf(filename, sizeof(filename) - 1,
			         "%s%s",
			         pointers->config->html_location ?
			         pointers->config->html_location : "",
				 pointers->config->single_page);
		      if (!(html = fopen(filename, "a")))
			{
			  perror("fopen");
			  exit(1);
			}
		      fprintf(html, "<HR>");
		    }
		  else
		    {
		      g_snprintf(filename, sizeof(filename) - 1,
				 "%s%s",
				 pointers->config->html_location ?
				 pointers->config->html_location : "",
				 htmlname);
		      if (!(html = fopen(filename, "w")))
			{
			  perror("fopen");
			  exit(1);
			}
		      if (!pointers->config->no_html_headers) {
			fprintf(html, "<HTML><TITLE>xferstats: %s to %s"
				"</TITLE><BODY>", pointers->low_date,
				pointers->high_date);
 		        if (pointers->config->refresh)
		          fprintf(html, "<META HTTP-EQUIV=\"Refresh\" CONTENT=%d>\n",
		                  pointers->config->refresh);
		      }
		    }
		}
	      
	      switch(type)
		{
		case T_DIR:
		  if (count == 1)
		    {
		      if (number)
			fprintf(html, "<CENTER><U><H1>Top %d Total Transfers "
				"from each Archive Section for <I>%s</I> to "
				"<I>%s</I></H1></U><BR>\n",
				number, pointers->low_date,
				pointers->high_date);
		      else
			fprintf(html, "<CENTER><U><H1>Total Transfers from "
				"each Archive Section for <I>%s</I> to <I>%s"
				"</I></H1></U><BR>\n",
				pointers->low_date, pointers->high_date);
		    }
		  fprintf(html, "<TABLE BORDER=2><TR><TH></TH><TH>Number Of"
			  "</TH><TH>Completed</TH><TH>Number of</TH><TH>"
			  "Average</TH><TH>Percent "
			  "Of</TH><TH>Percent Of</TH></TR>\n");
		  fprintf(html, "<TR><TH>Archive Section</TH><TH>Files Sent"
			  "</TH><TH>Transfers</TH><TH>Bytes Sent</TH><TH>Xmit "
			  "Rate</TH><TH>Files"
			  " Sent</TH><TH>Bytes Sent</TH></TR>\n");
		  break;
		case T_FILE:
		  if (count == 1)
		    {
		      if (number)
			fprintf(html, "<CENTER><U><H1>Top %d Most Transferred "
				"Files for <I>%s</I> to <I>%s</I></H1></U>"
				"<BR>\n", number, pointers->low_date,
				pointers->high_date);
		      else
			fprintf(html, "<CENTER><U><H1>All Transferred Files "
				"for <I>%s</I> to <I>%s</I></H1></U><BR>\n",
				pointers->low_date, pointers->high_date);
		    }
		  fprintf(html, "<TABLE BORDER=2><TR><TH></TH><TH>Number Of"
			  "</TH><TH>Completed</TH><TH>Number of</TH><TH>"
			  "Average</TH><TH>Percent Of</TH><TH>Percent Of</TH>"
			  "</TR>\n");
		  fprintf(html, "<TR><TH>Filename</TH><TH>Files Sent</TH><TH>"
			  "Transfers</TH><TH>Bytes Sent</TH><TH>Xmit Rate</TH>"
			  "<TH>Files Sent</TH><TH>Bytes Sent</TH></TR>\n");
		  break;
		case T_TLD:
		  if (count == 1)
		    {
		      if (number)
			fprintf(html, "<CENTER><U><H1>Top %d Top Level Domains"
				" for <I>%s</I> to <I>%s</I> (By bytes)</H1>"
				"</U><BR>\n", number, pointers->low_date,
				pointers->high_date);
		      else
			fprintf(html, "<CENTER><U><H1>Top Level Domain "
				"Statistics for <I>%s</I> to <I>%s</I> (By "
				"bytes)</H1></U><BR>\n", pointers->low_date,
				pointers->high_date);
		    }
		  fprintf(html, "<TABLE BORDER=2><TR><TH></TH><TH>Number Of"
			  "</TH><TH>Number of</TH><TH>Average</TH><TH>Percent "
			  "Of</TH><TH>Percent Of</TH></TR>\n");
		  fprintf(html, "<TR><TH>Top Level Domain</TH><TH>Files Sent"
			  "</TH><TH>Bytes Sent</TH><TH>Xmit Rate</TH><TH>Files"
			  " Sent</TH><TH>Bytes Sent</TH></TR>\n");
		  break;
		case T_HOST:
		  if (count == 1)
		    {
		      if (number)
			fprintf(html, "<CENTER><U><H1>Top %d Hosts for <I>%s"
				"</I> to <I>%s</I></H1></U><BR>\n",
				number, pointers->low_date,
				pointers->high_date);
		      else
			fprintf(html, "<CENTER><U><H1>Host Statistics for "
				"<I>%s</I> to <I>%s</I></H1></U>"
				"<BR>\n", pointers->low_date,
				pointers->high_date);
		    }
		  fprintf(html, "<TABLE BORDER=2><TR><TH></TH><TH>Number Of"
			  "</TH><TH>Number of</TH><TH>Average</TH><TH>Percent "
			  "Of</TH><TH>Percent Of</TH></TR>\n");
		  fprintf(html, "<TR><TH>Host</TH><TH>Files Sent</TH><TH>Bytes"
			  " Sent</TH><TH>Xmit Rate</TH><TH>Files Sent</TH><TH>"
			  "Bytes Sent</TH></TR>\n");
		  break;
		case T_DOMAIN:
		  if (count == 1)
		    {
		      if (number)
			fprintf(html, "<CENTER><U><H1>Top %d Domains for <I>%s"
				"</I> to <I>%s</I></H1></U><BR>\n",
				number, pointers->low_date,
				pointers->high_date);
		      else
			fprintf(html, "<CENTER><U><H1>Domain Statistics for "
				"<I>%s</I> to <I>%s</I></H1></U>"
				"<BR>\n", pointers->low_date,
				pointers->high_date);
		    }
		  fprintf(html, "<TABLE BORDER=2><TR><TH></TH><TH>Number Of"
			  "</TH><TH>Number of</TH><TH>Average</TH><TH>Percent "
			  "Of</TH><TH>Percent Of</TH></TR>\n");
		  fprintf(html, "<TR><TH>Domain</TH><TH>Files Sent</TH><TH>"
			  "Bytes Sent</TH><TH>Xmit Rate</TH><TH>Files Sent"
			  "</TH><TH>Bytes Sent</TH></TR>\n");
		  break;
		case T_SIZE:
		  fprintf(html, "<CENTER><U><H1>File Size Statistics for <I>"
			  "%s</I> to <I>%s</I></H1></U><BR>\n",
			  pointers->low_date, pointers->high_date);
		  fprintf(html, "<TABLE BORDER=2><TR><TH></TH><TH>Number Of"
			  "</TH><TH>Number of</TH><TH>Average</TH><TH>Percent "
			  "Of</TH><TH>Percent Of</TH></TR>\n");
		  fprintf(html, "<TR><TH>Size</TH><TH>Files Sent</TH><TH>"
			  "Bytes Sent</TH><TH>Xmit Rate</TH><TH>Files Sent"
			  "</TH><TH>Bytes Sent</TH></TR>\n");
		  break;
		}	      
	    }
	  else
	    {
	      switch(type)
		{
		case T_DIR:
		  if (count == 1)
		    {
		      if (number)
			printf("%c\nTop %d Total Transfers from each Archive "
			       "Section\n\n", 12, number);
		      else
			printf("%c\nTotal Transfers from each Archive Section"
			       "\n\n", 12);
		    }
		  printf("                                                                ---- %% Of ----\n");
		  printf("                       Archive Section                    Files  Files   Bytes\n");
		  break;
		case T_FILE:
		  if (count == 1)
		    {
		      if (number)
			printf("%c\nTop %d Most Transferred Files\n\n", 12,
			       number);
		      else
			printf("%c\nAll Transferred Files\n\n", 12);
		    }
		  printf("                                                                ---- %% Of ----\n");
		  printf("                           Filename                       Files  Files   Bytes\n");
		  printf(" -------------------------------------------------------- ----- ------- -------\n");
		  break;
		case T_TLD:
		  if (count == 1)
		    {
		      if (number)
			printf("%c\nTop %d Top Level Domains\n\n", 12, number);
		      else
			printf("%c\nTop Level Domain Statistics\n\n", 12);
		    }
		  printf("                 Number Of    Number of    Average   Percent Of  Percent Of\n");
		  printf("    Domain       Files Sent  Bytes  Sent  Xmit Rate  Files Sent  Bytes Sent\n");
		  printf("---------------  ----------  -----------  ---------  ----------  ----------\n");
		  break;
		case T_HOST:
		  if (count == 1)
		    {
		      if (number)
			printf("%c\nTop %d Hosts\n\n", 12, number);
		      else
			printf("%c\nHost Statistics\n\n", 12);
		    }
		  printf("                                   Number Of   Number of  Percent Of Percent Of\n");
		  printf("               Host                Files Sent Bytes  Sent Files Sent Bytes Sent\n");
		  printf("---------------------------------- ---------- ----------- ---------- ----------\n");
		  break;
		case T_DOMAIN:
		  if (count == 1)
		    {
		      if (number)
			printf("%c\nTop %d Domains\n\n", 12,
			       number);
		      else
			printf("%c\nDomain Statistics\n\n", 12);
		    }
		  printf("                                   Number Of   Number of  Percent Of Percent Of\n");
		  printf("              Domain               Files Sent Bytes  Sent Files Sent Bytes Sent\n");
		  printf("---------------------------------- ---------- ----------- ---------- ----------\n");
		  break;
		case T_SIZE:
		  printf("%c\nFile Size Statistics\n\n", 12);
		  printf("                 Number Of    Number of    Average   Percent Of  Percent Of\n");
		  printf("     Size        Files Sent  Bytes  Sent  Xmit Rate  Files Sent  Bytes Sent\n");
		  printf("---------------  ----------  -----------  ---------  ----------  ----------\n");
		  break;
		}
	    }
	}
      
      if (pointers->config->html_output)
	{
	  if ((type == T_DIR || type == T_FILE) &&
	      pointers->config->link_prefix)
	    fprintf(html, "<TR><TD><A HREF=\"%s%s\">%s</A></TD>"
		    "<TD ALIGN=RIGHT>%u</TD><TD ALIGN=RIGHT>%u</TD>"
		    "<TD ALIGN=RIGHT>%s</TD>"
		    "<TD NOWRAP ALIGN=RIGHT>%5.1f KB/s</TD><TD ALIGN=RIGHT>"
		    "%10.2f</TD><TD ALIGN=RIGHT>%10.2f</TD>",
		    pointers->config->link_prefix, current_path_ptr->name,
		    current_path_ptr->name, current_path_ptr->file_count,
		    current_path_ptr->completed_count,
		    (buffer = big_number(current_path_ptr->data, buffer,
					 &size)),
		    (double) current_path_ptr->data /
		    current_path_ptr->seconds / 1024,
		    (double) current_path_ptr->file_count /
		    pointers->file_count * 100,
		    (double)current_path_ptr->data / pointers->data * 100);
	  else
	    fprintf(html, "<TR><TD>%s%s</TD><TD ALIGN=RIGHT>%u</TD>"
		    "<TD ALIGN=RIGHT>%u</TD>"
		    "<TD ALIGN=RIGHT>%s</TD><TD NOWRAP ALIGN=RIGHT>"
		    "%5.1f KB/s</TD><TD ALIGN=RIGHT>%10.2f</TD>"
		    "<TD ALIGN=RIGHT>%10.2f</TD>",
		    type == T_SIZE ? "&lt;" : "", current_path_ptr->name,
		    current_path_ptr->file_count,
		    current_path_ptr->completed_count,
		    (buffer = big_number(current_path_ptr->data, buffer,
					 &size)),
		    (double) current_path_ptr->data /
		    current_path_ptr->seconds / 1024,
		    (double) current_path_ptr->file_count /
		    pointers->file_count * 100,
		    (double)current_path_ptr->data / pointers->data * 100);
	  switch(type)
	    {
	    case T_DIR:
	    case T_TLD:
	    case T_HOST:
	    case T_DOMAIN:
	    case T_SIZE:
	      percent = (double) current_path_ptr->data / highest_count * 50;
	      break;
	    case T_FILE:
	      percent =
		(double) current_path_ptr->file_count / highest_count * 50;
	      break;
	    }
	  fprintf(html, "<TD NOWRAP ALIGN=LEFT>");
	  html_graph(percent, html, pointers->config->graph_path);
	  fprintf(html, "</TD></TR>\n");
	}
      else
	{
	  switch(type)
	    {
	    case T_DIR:
	    case T_FILE:
	      if ((a = strlen(current_path_ptr->name)) > 54)
		{
		  strcpy(temp_name, current_path_ptr->name + a - 54);
		  printf("...%-53s %5u %7.2f %7.2f\n", temp_name,
			 current_path_ptr->file_count,
			 (double) current_path_ptr->file_count /
			 pointers->file_count * 100,
			 (double) current_path_ptr->data /
			 pointers->data * 100);
		}
	      else
		printf(" %-56s %5u %7.2f %7.2f\n", current_path_ptr->name,
		       current_path_ptr->file_count,
		       (double) current_path_ptr->file_count /
		       pointers->file_count * 100,
		       (double) current_path_ptr->data / pointers->data * 100);
	      break;
	    case T_TLD:
	      printf("%-15s  %10u  %11" LL_FMT "  %5.1f K/s  %10.2f  %10.2f\n",
		     current_path_ptr->name, current_path_ptr->file_count,
		     current_path_ptr->data, (double) current_path_ptr->data /
		     current_path_ptr->seconds / 1024,
		     (double) current_path_ptr->file_count /
		     pointers->file_count * 100,
		     (double) current_path_ptr->data / pointers->data * 100);
	      break;
	    case T_SIZE:
	      printf("<%-14s  %10u  %11" LL_FMT "  %5.1f K/s  %10.2f  "
		     "%10.2f\n",
		     current_path_ptr->name, current_path_ptr->file_count,
		     current_path_ptr->data, (double) current_path_ptr->data /
		     current_path_ptr->seconds / 1024,
		     (double) current_path_ptr->file_count /
		     pointers->file_count * 100,
		     (double) current_path_ptr->data / pointers->data * 100);
	      break;
	    case T_HOST:
	    case T_DOMAIN:
	      strncpy(temp_name, current_path_ptr->name, 31);
	      temp_name[31] = '\0';
	      printf("%-34s %10u %11" LL_FMT " %10.2f %10.2f\n", temp_name,
		     current_path_ptr->file_count, current_path_ptr->data,
		     (double) current_path_ptr->file_count /
		     pointers->file_count * 100,
		     (double) current_path_ptr->data / pointers->data * 100);
	    }
	}
    }
  
  if (pointers->config->html_output)
    {
      fprintf(html, "</TABLE><BR><IMG SRC=\"%sg1.jpg\" ALT=\"g1\"> = %s %s"
	      "<BR>\n", pointers->config->graph_path ?
	      pointers->config->graph_path : "",
	      (buffer = big_number(highest_count / 50, buffer, &size)),
	      type == T_FILE ? "transfers" : "bytes");
      if (!pointers->config->single_page)
	{
	  fprintf(html, "<HR><A HREF=\"totals.html\">Return to Totals and "
		  "Index </A></CENTER>\n");
	  if (!pointers->config->no_html_headers)
	    fprintf(html, "</BODY></HTML>\n");
	}
      fclose(html);
    }
  
  /* Don't leak the buffer used for big_number()! */
  g_free(buffer);
  
#ifndef SMFS
#if 0
  /* if this is the file data generation phase AND we are going to later
   * generate dir data, save all of the file data to work from.
   *
   * if this is the host data generation phase AND we are going to later
   * generate domain data, save all of the host data to work from.
   *
   * if this is the domain data generation phase AND we are going to later
   * generate TLD data, save all of the domain data to work from.
   *
   * This will all save us time later!
   */

  if ((type != T_FILE || !pointers->config->dir_traffic) &&
      (type != T_HOST || !pointers->config->domain_traffic) &&
      (type != T_DOMAIN || !pointers->config->tld_traffic))
    {
      /* If none of those cases are true, free up the current arena and
       * array */
      for (current_arena_ptr = *arena;
	   current_arena_ptr != NULL;)
	{
	  next_arena_ptr = current_arena_ptr->next_ptr;
	  g_free(current_arena_ptr);
	  current_arena_ptr = next_arena_ptr;
	}
      
      *arena = NULL;

      if (pointers_ptr)
	g_free(pointers_ptr);

      switch(type)
	{
	case T_DIR:
	  pointers->first_dir_ptr = NULL;
	  break;
	case T_FILE:
	  pointers->first_file_ptr = NULL;
	  break;
	case T_TLD:
	  pointers->first_tld_ptr = NULL;
	  break;
	case T_HOST:
	  pointers->first_host_ptr = NULL;
	  break;
	case T_DOMAIN:
	  pointers->first_domain_ptr = NULL;
	  break;
	case T_SIZE:
	  pointers->first_size_ptr = NULL;
	  break;
	}
    }

  if (type == T_DIR && pointers->file_arena)
    {
      for (current_arena_ptr = pointers->file_arena;
	   current_arena_ptr != NULL;)
	{
	  next_arena_ptr = current_arena_ptr->next_ptr;
	  g_free(current_arena_ptr);
	  current_arena_ptr = next_arena_ptr;
	}

      if (pointers->first_file_ptr)
	g_free(pointers->first_file_ptr);

      pointers->first_file_ptr = NULL;
      pointers->file_arena = NULL;
    }

  if (type == T_DOMAIN && pointers->host_arena)
    {
      for (current_arena_ptr = pointers->host_arena;
	   current_arena_ptr != NULL;)
	{
	  next_arena_ptr = current_arena_ptr->next_ptr;
	  g_free(current_arena_ptr);
	  current_arena_ptr = next_arena_ptr;
	}

      if (pointers->first_host_ptr)
	g_free(pointers->first_host_ptr);

      pointers->first_host_ptr = NULL;
      pointers->host_arena = NULL;
    }

  if (type == T_TLD && pointers->domain_arena)
    {
      for (current_arena_ptr = pointers->domain_arena;
	   current_arena_ptr != NULL;)
	{
	  next_arena_ptr = current_arena_ptr->next_ptr;
	  g_free(current_arena_ptr);
	  current_arena_ptr = next_arena_ptr;
	}

      if (pointers->first_domain_ptr)
	g_free(pointers->first_domain_ptr);

      pointers->first_domain_ptr = NULL;
      pointers->domain_arena = NULL;
    }
#else /* ENABLE_ARENAS */

#endif /* ENABLE_ARENAS */
#endif /* ifndef SMFS */

#ifdef DEBUGS
  fprintf(stderr, "display_%s_totals complete\n", T_NAMES[type]);
#endif
} /* display_misc_totals */


void
display_hourly_totals(pointers_t * pointers)
{
  int i, size = 0;
  double percent;
  char *buffer = NULL, filename[MAXPATHLEN];
  FILE *html = NULL;

#ifdef DEBUGS
  fprintf(stderr, "Beginning display_hourly_totals\n");
#endif

  if (pointers->config->html_output)
    {
      if (!pointers->config->single_page)
	{
	  g_snprintf(filename, sizeof(filename) - 1, "%shour.html",
		     pointers->config->html_location ?
		     pointers->config->html_location : "");
	  if (!(html = fopen(filename, "w")))
	    {
	      perror("fopen");
	      exit(1);
	    }
	  if (!pointers->config->no_html_headers) {
	    fprintf(html, "<HTML><TITLE>xferstats: %s to %s</TITLE><BODY>\n",
		    pointers->low_date, pointers->high_date);
            if (pointers->config->refresh)
              fprintf(html, "<META HTTP-EQUIV=\"Refresh\" CONTENT=%d>\n",
                      pointers->config->refresh);
          }
	}
      else
	{
	  g_snprintf(filename, sizeof(filename) - 1, "%s%s",
		     pointers->config->html_location ?
		     pointers->config->html_location : "",
		     pointers->config->single_page);
	  if (!(html = fopen(filename, "a")))
	    {
	      perror("fopen");
	      exit(1);
	    }
	  fprintf(html, "<HR>");
	}

      fprintf(html, "<CENTER><H1><U>Hourly Transmission Statistics for <I>%s"
	      "</I> to <I>%s</I></U></H1><BR><TABLE BORDER=2>\n",
	      pointers->low_date, pointers->high_date);
      fprintf(html, "<TR><TH></TH><TH>Number Of</TH><TH>Number of</TH><TH>"
	      "Average</TH><TH>Percent Of</TH><TH>Percent Of</TH></TR>\n");
      fprintf(html, "<TR><TH>Time</TH><TH>Files Sent</TH><TH>Bytes Sent</TH>"
	      "<TH>Xmit Rate</TH><TH>Files Sent</TH><TH>Bytes Sent</TH></TR>"
	      "\n");
      
      for (i = 0; i <= 23; i++)
	if (pointers->hour_data[i]->file_count)
	  {
	    fprintf(html, "<TR><TD>%s</TD><TD ALIGN=RIGHT>%u</TD>"
		    "<TD ALIGN=RIGHT>%s</TD><TD NOWRAP ALIGN=RIGHT>%6.2f KB/s"
		    "</TD><TD ALIGN=RIGHT>%10.2f</TD><TD ALIGN=RIGHT>%10.2f"
		    "</TD>", pointers->hour_data[i]->name,
		    pointers->hour_data[i]->file_count,
		    (buffer = big_number(pointers->hour_data[i]->data, buffer,
					 &size)),
		    (double) pointers->hour_data[i]->data /
		    pointers->hour_data[i]->seconds / 1024,
		    (double) pointers->hour_data[i]->file_count /
		    pointers->file_count * 100,
		    (percent = (double) pointers->hour_data[i]->data /
		     pointers->data * 100));
	    fprintf(html, "<TD NOWRAP ALIGN=LEFT>");
	    html_graph(percent, html, pointers->config->graph_path);
	    fprintf(html, "</TD></TR>\n");
	  }
      fprintf(html, "</TABLE><BR>\n<IMG SRC=\"%sg1.jpg\" ALT=\"g1\"> = %s "
	      "bytes<BR>\n", pointers->config->graph_path ?
	      pointers->config->graph_path : "",
	      (buffer = big_number((double) pointers->data / 50, buffer,
				   &size)));
      if (!pointers->config->single_page)
	{
	  fprintf(html, "<HR><A HREF=\"totals.html\">Return to Totals and "
		  "Index </A></CENTER>\n");
	  if (!pointers->config->no_html_headers)
	    fprintf(html, "</BODY></HTML>\n");
	}
      fclose(html);
    }
  else
    {
      printf("%c\nHourly Transmission Statistics\n\n", 12);
      printf("      Number Of    Number of     Average    Percent Of  Percent Of\n");
      printf("Time  Files Sent  Bytes  Sent  Xmit   Rate  Files Sent  Bytes Sent\n");
      printf("----  ----------  -----------  -----------  ----------  ----------\n");
      for (i = 0; i <= 23; i++)
	if (pointers->hour_data[i]->file_count)
	  printf("%2s    %10u  %11" LL_FMT "  %6.2f KB/s  %10.2f  %10.2f\n",
		 pointers->hour_data[i]->name,
		 pointers->hour_data[i]->file_count,
		 pointers->hour_data[i]->data,
		 (double) pointers->hour_data[i]->data /
		 pointers->hour_data[i]->seconds / 1024,
		 (double) pointers->hour_data[i]->file_count /
		 pointers->file_count * 100,
		 (double) pointers->hour_data[i]->data / pointers->data * 100);
    }

  /* Don't leak the buffer used for big_number()! */
  g_free(buffer);

#ifdef DEBUGS
  fprintf(stderr, "display_hourly_totals complete\n");
#endif
} /* display_hourly_totals */


void
display_monthly_totals(pointers_t * pointers)
{
  int i, size = 0;
  double percent;
  char *buffer = NULL, filename[MAXPATHLEN];
  FILE *html = NULL;

#ifdef DEBUGS
  fprintf(stderr, "Beginning display_monthly_totals\n");
#endif

  if (pointers->config->html_output)
    {
      if (!pointers->config->single_page)
	{
	  g_snprintf(filename, sizeof(filename) - 1, "%smonthly.html",
		     pointers->config->html_location ?
		     pointers->config->html_location : "");
	  if (!(html = fopen(filename, "w")))
	    {
	      perror("fopen");
	      exit(1);
	    }
	  if (!pointers->config->no_html_headers) {
	    fprintf(html, "<HTML><TITLE>xferstats: %s to %s</TITLE><BODY>\n",
		    pointers->low_date, pointers->high_date);
            if (pointers->config->refresh)
              fprintf(html, "<META HTTP-EQUIV=\"Refresh\" CONTENT=%d>\n",
                      pointers->config->refresh);
	  }
	}
      else
	{
	  g_snprintf(filename, sizeof(filename) - 1, "%s%s",
		     pointers->config->html_location ?
		     pointers->config->html_location : "",
		     pointers->config->single_page);
	  if (!(html = fopen(filename, "a")))
	    {
	      perror("fopen");
	      exit(1);
	    }
	  fprintf(html, "<HR>");
	}

      fprintf(html, "<CENTER><H1><U>Monthly Transmission Statistics for <I>%s"
	      "</I> to <I>%s</I></U></H1><BR><TABLE BORDER=2>\n",
	      pointers->low_date, pointers->high_date);
      fprintf(html, "<TR><TH></TH><TH>Number Of</TH><TH>Number of</TH><TH>"
	      "Average</TH><TH>Percent Of</TH><TH>Percent Of</TH></TR>\n");
      fprintf(html, "<TR><TH>Month</TH><TH>Files Sent</TH><TH>Bytes Sent</TH>"
	      "<TH>Xmit Rate</TH><TH>Files Sent</TH><TH>Bytes Sent</TH></TR>"
	      "\n");
      
      for (i = 0; i <= 11; i++)
	if (pointers->month_data[i]->file_count)
	  {
	    fprintf(html, "<TR><TD>%s</TD><TD ALIGN=RIGHT>%u</TD>"
		    "<TD ALIGN=RIGHT>%s</TD><TD NOWRAP ALIGN=RIGHT>%6.2f KB/s"
		    "</TD><TD ALIGN=RIGHT>%10.2f</TD><TD ALIGN=RIGHT>%10.2f"
		    "</TD>", pointers->month_data[i]->name,
		    pointers->month_data[i]->file_count,
		    (buffer = big_number(pointers->month_data[i]->data, buffer,
					 &size)),
		    (double) pointers->month_data[i]->data /
		    pointers->month_data[i]->seconds / 1024,
		    (double) pointers->month_data[i]->file_count /
		    pointers->file_count * 100,
		    (percent = (double) pointers->month_data[i]->data /
		     pointers->data * 100));
	    fprintf(html, "<TD NOWRAP ALIGN=LEFT>");
	    html_graph(percent, html, pointers->config->graph_path);
	    fprintf(html, "</TD></TR>\n");
	  }
      fprintf(html, "</TABLE><BR>\n<IMG SRC=\"%sg1.jpg\" ALT=\"g1\"> = %s "
	      "bytes<BR>\n", pointers->config->graph_path ?
	      pointers->config->graph_path : "",
	      (buffer = big_number((double) pointers->data / 50, buffer,
				   &size)));
      if (!pointers->config->single_page)
	{
	  fprintf(html, "<HR><A HREF=\"totals.html\">Return to Totals and "
		  "Index </A></CENTER>\n");
	  if (!pointers->config->no_html_headers)
	    fprintf(html, "</BODY></HTML>\n");
	}
      fclose(html);
    }
  else
    {
      printf("%c\nMonthly Transmission Statistics\n\n", 12);
      printf("           Number Of    Number of     Average    Percent Of  Percent Of\n");
      printf("  Month    Files Sent  Bytes  Sent  Xmit   Rate  Files Sent  Bytes Sent\n");
      printf("---------  ----------  -----------  -----------  ----------  ----------\n");
      for (i = 0; i <= 11; i++)
	if (pointers->month_data[i]->file_count)
	  printf("%-9s  %10u  %11" LL_FMT "  %6.2f KB/s  %10.2f  %10.2f\n",
		 pointers->month_data[i]->name,
		 pointers->month_data[i]->file_count,
		 pointers->month_data[i]->data,
		 (double) pointers->month_data[i]->data /
		 pointers->month_data[i]->seconds / 1024,
		 (double) pointers->month_data[i]->file_count /
		 pointers->file_count * 100,
		 (double) pointers->month_data[i]->data /
		 pointers->data * 100);
    }

  /* Don't leak the buffer used for big_number()! */
  g_free(buffer);

#ifdef DEBUGS
  fprintf(stderr, "display_monthly_totals complete\n");
#endif
} /* display_monthly_totals */


void
display_dow_totals(pointers_t * pointers)
{
  int i, size = 0;
  double percent;
  char *buffer = NULL, filename[MAXPATHLEN];
  FILE *html = NULL;

#ifdef DEBUGS
  fprintf(stderr, "display_dow_totals beginning\n");
#endif

  if (pointers->config->html_output)
    {
      if (!pointers->config->single_page)
	{
	  g_snprintf(filename, sizeof(filename) - 1, "%sdow.html",
		     pointers->config->html_location ?
		     pointers->config->html_location : "");
	  if (!(html = fopen(filename, "w")))
	    {
	      perror("fopen");
	      exit(1);
	    }
	  if (!pointers->config->no_html_headers) {
	    fprintf(html, "<HTML><TITLE>xferstats: %s to %s</TITLE><BODY>\n",
		    pointers->low_date, pointers->high_date);
            if (pointers->config->refresh)
              fprintf(html, "<META HTTP-EQUIV=\"Refresh\" CONTENT=%d>\n",
                      pointers->config->refresh);
	  }
	}
      else
	{
	  g_snprintf(filename, sizeof(filename) - 1, "%s%s",
		     pointers->config->html_location ?
		     pointers->config->html_location : "",
		     pointers->config->single_page);
	  if (!(html = fopen(filename, "a")))
	    {
	      perror("fopen");
	      exit(1);
	    }
	  fprintf(html, "<HR>");
	}

      fprintf(html, "<CENTER><H1><U>Days of the Week Transmission Statistics "
	      "for <I>%s</I> to <I>%s</I></U></H1><BR><TABLE BORDER=2>\n",
	      pointers->low_date, pointers->high_date);
      fprintf(html, "<TR><TH></TH><TH>Number Of</TH><TH>Number of</TH><TH>"
	      "Average</TH><TH>Percent Of</TH><TH>Percent Of</TH></TR>\n");
      fprintf(html, "<TR><TH>Day</TH><TH>Files Sent</TH><TH>Bytes Sent</TH>"
	      "<TH>Xmit Rate</TH><TH>Files Sent</TH><TH>Bytes Sent</TH></TR>"
	      "\n");
      
      for (i = 0; i <= 6; i++)
	if (pointers->weekday_data[i]->file_count)
	  {
	    fprintf(html, "<TR><TD>%s</TD><TD ALIGN=RIGHT>%u</TD>"
		    "<TD ALIGN=RIGHT>%s</TD><TD NOWRAP ALIGN=RIGHT>%6.2f KB/s"
		    "</TD><TD ALIGN=RIGHT>%10.2f</TD><TD ALIGN=RIGHT>%10.2f"
		    "</TD>", pointers->weekday_data[i]->name,
		    pointers->weekday_data[i]->file_count,
		    (buffer = big_number(pointers->weekday_data[i]->data,
					 buffer, &size)),
		    (double) pointers->weekday_data[i]->data /
		    pointers->weekday_data[i]->seconds / 1024,
		    (double) pointers->weekday_data[i]->file_count /
		    pointers->file_count * 100,
		    percent = ((double) pointers->weekday_data[i]->data /
			       pointers->data * 100));
	    fprintf(html, "<TD NOWRAP ALIGN=LEFT>");
	    html_graph(percent, html, pointers->config->graph_path);
	    fprintf(html, "</TD></TR>\n");
	  }
      fprintf(html, "</TABLE><BR>\n<IMG SRC=\"%sg1.jpg\" ALT=\"g1\"> = %s "
	      "bytes<BR>\n", pointers->config->graph_path ?
	      pointers->config->graph_path : "",
	      (buffer = big_number((double) pointers->data / 50, buffer,
				   &size)));
      if (!pointers->config->single_page)
	{
	  fprintf(html, "<HR><A HREF=\"totals.html\">Return to Totals and "
		  "Index </A></CENTER>\n");
	  if (!pointers->config->no_html_headers)
	    fprintf(html, "</BODY></HTML>\n");
	}
      fclose(html);
    }
  else
    {
      printf("%c\nDays of the Week Transmission Statistics\n\n", 12);
      printf("           Number Of    Number of     Average    Percent Of  Percent Of\n");
      printf("   Day     Files Sent  Bytes  Sent  Xmit   Rate  Files Sent  Bytes Sent\n");
      printf("---------  ----------  -----------  -----------  ----------  ----------\n");
      for (i = 0; i <= 6; i++)
	if (pointers->weekday_data[i]->file_count)
	  printf("%-9s  %10u  %11" LL_FMT "  %6.2f KB/s  %10.2f  %10.2f\n",
		 pointers->weekday_data[i]->name,
		 pointers->weekday_data[i]->file_count,
		 pointers->weekday_data[i]->data,
		 (double) pointers->weekday_data[i]->data /
		 pointers->weekday_data[i]->seconds / 1024,
		 (double) pointers->weekday_data[i]->file_count /
		 pointers->file_count * 100,
		 (double) pointers->weekday_data[i]->data /
		 pointers->data * 100);
    }

  /* Don't leak the buffer used for big_number()! */
  g_free(buffer);

#ifdef DEBUGS
  fprintf(stderr, "display_dow_totals complete\n");
#endif
} /* display_dow_totals */


void
display_dom_totals(pointers_t * pointers)
{
  int i, size = 0;
  double percent;
  char *buffer = NULL, filename[MAXPATHLEN];
  FILE *html = NULL;

#ifdef DEBUGS
  fprintf(stderr, "display_dom_totals beginning\n");
#endif

  if (pointers->config->html_output)
    {
      if (!pointers->config->single_page)
	{
	  g_snprintf(filename, sizeof(filename) - 1, "%sdom.html",
		     pointers->config->html_location ?
		     pointers->config->html_location : "");
	  if (!(html = fopen(filename, "w")))
	    {
	      perror("fopen");
	      exit(1);
	    }
	  if (!pointers->config->no_html_headers) {
	    fprintf(html, "<HTML><TITLE>xferstats: %s to %s</TITLE><BODY>\n",
		    pointers->low_date, pointers->high_date);
            if (pointers->config->refresh)
              fprintf(html, "<META HTTP-EQUIV=\"Refresh\" CONTENT=%d>\n",
                      pointers->config->refresh);
	  }
	}
      else
	{
	  g_snprintf(filename, sizeof(filename) - 1, "%s%s",
		     pointers->config->html_location ?
		     pointers->config->html_location : "",
		     pointers->config->single_page);
	  if (!(html = fopen(filename, "a")))
	    {
	      perror("fopen");
	      exit(1);
	    }
	  fprintf(html, "<HR>");
	}

      fprintf(html, "<CENTER><H1><U>Days of the Month Transmission Statistics "
	      "for <I>%s</I> to <I>%s</I></U></H1><BR><TABLE BORDER=2>\n",
	      pointers->low_date, pointers->high_date);
      fprintf(html, "<TR><TH></TH><TH>Number Of</TH><TH>Number of</TH><TH>"
	      "Average</TH><TH>Percent Of</TH><TH>Percent Of</TH></TR>\n");
      fprintf(html, "<TR><TH>Day</TH><TH>Files Sent</TH><TH>Bytes Sent</TH>"
	      "<TH>Xmit Rate</TH><TH>Files Sent</TH><TH>Bytes Sent</TH>"
	      "</TR>\n");
      
      for (i = 0; i <= 30; i++)
	if (pointers->dom_data[i]->file_count)
	  {
	    fprintf(html, "<TR><TD>%s</TD><TD ALIGN=RIGHT>%u</TD>"
		    "<TD ALIGN=RIGHT>%s</TD><TD NOWRAP ALIGN=RIGHT>%6.2f KB/s"
		    "</TD><TD ALIGN=RIGHT>%10.2f</TD><TD ALIGN=RIGHT>%10.2f"
		    "</TD>", pointers->dom_data[i]->name,
		    pointers->dom_data[i]->file_count,
		    (buffer = big_number(pointers->dom_data[i]->data, buffer,
					 &size)),
		    (double) pointers->dom_data[i]->data /
		    pointers->dom_data[i]->seconds / 1024,
		    (double) pointers->dom_data[i]->file_count /
		    pointers->file_count * 100,
		    (percent = (double) pointers->dom_data[i]->data /
		     pointers->data * 100));
	    fprintf(html, "<TD NOWRAP ALIGN=LEFT>");
	    html_graph(percent, html, pointers->config->graph_path);
	    fprintf(html, "</TD></TR>\n");
	  }
      fprintf(html, "</TABLE><BR>\n<IMG SRC=\"%sg1.jpg\" ALT=\"g1\"> = %s "
	      "bytes<BR>\n", pointers->config->graph_path ?
	      pointers->config->graph_path : "",
	      (buffer = big_number((double) pointers->data / 50, buffer,
				   &size)));
      if (!pointers->config->single_page)
	{
	  fprintf(html, "<HR><A HREF=\"totals.html\">Return to Totals and "
		  "Index </A></CENTER>\n");
	  if (!pointers->config->no_html_headers)
	    fprintf(html, "</BODY></HTML>\n");
	}
      fclose(html);
    }
  else
    {
      printf("%c\nDays of the Month Transmission Statistics\n\n", 12);
      printf("     Number Of    Number of     Average    Percent Of  Percent Of\n");
      printf("Day  Files Sent  Bytes  Sent  Xmit   Rate  Files Sent  Bytes Sent\n");
      printf("---  ----------  -----------  -----------  ----------  ----------\n");
      for (i = 0; i <= 30; i++)
	if (pointers->dom_data[i]->file_count)
	    printf("%2s   %10u  %11" LL_FMT "  %6.2f KB/s  %10.2f  %10.2f\n",
		   pointers->dom_data[i]->name,
		   pointers->dom_data[i]->file_count,
		   pointers->dom_data[i]->data,
		   (double) pointers->dom_data[i]->data /
		   pointers->dom_data[i]->seconds / 1024,
		   (double) pointers->dom_data[i]->file_count /
		   pointers->file_count * 100,
		   (double) pointers->dom_data[i]->data /
		   pointers->data * 100);
    }

  /* Don't leak the buffer used for big_number()! */
  g_free(buffer);

#ifdef DEBUGS
  fprintf(stderr, "display_dom_totals complete\n");
#endif
} /* display_dom_totals */
