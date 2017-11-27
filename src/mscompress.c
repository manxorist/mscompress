/*
 *  mscompress: Microsoft "compress.exe/expand.exe" compatible compressor
 *
 *  Copyright (c) 2000 Martin Hinner <mhi@penguin.cz>
 *  Algorithm & data structures by M. Winterhoff <100326.2776@compuserve.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <stdint.h>
#include <getopt.h>

extern char *version_string;

#define N 4096
#define F 16
#define THRESHOLD 3

#define dad (node+1)
#define lson (node+1+N)
#define rson (node+1+N+N)
#define root (node+1+N+N+N)
#define NIL -1

char *buffer;
int *node;
int pos;

#define max(x,y) ((x)>(y)?(x):(y))
#define min(x,y) ((y)>(x)?(x):(y))

int
insert (int i, int run)
{
  int c, j, k, l, n, match;
  int *p;

  k = l = 1;
  match = THRESHOLD - 1;
  p = &root[(unsigned char) buffer[i]];
  lson[i] = rson[i] = NIL;

  c = 0;
  while ((j = *p) != NIL)
    {
      n = min (k, l);
      while (n < run && (c = (buffer[j + n] - buffer[i + n])) == 0)
	n++;

      if (n > match)
	{
	  match = n;
	  pos = j;
	}
      if (c < 0)
	{
	  p = &lson[j];
	  k = n;
	}
      else if (c > 0)
	{
	  p = &rson[j];
	  l = n;
	}
      else
	{
	  dad[j] = NIL;
	  dad[lson[j]] = lson + i - node;
	  dad[rson[j]] = rson + i - node;
	  lson[i] = lson[j];
	  rson[i] = rson[j];
	  break;
	}
    }
  dad[i] = p - node;
  *p = i;
  return match;
}

void
delete (int z)
{
  int j;

  if (dad[z] != NIL)
    {
      if (rson[z] == NIL)
	{
	  j = lson[z];
	}
      else if (lson[z] == NIL)
	{
	  j = rson[z];
	}
      else
	{
	  j = lson[z];
	  if (rson[j] != NIL)
	    {
	      do
		{
		  j = rson[j];
		}
	      while (rson[j] != NIL);
	      node[dad[j]] = lson[j];
	      dad[lson[j]] = dad[j];
	      lson[j] = lson[z];
	      dad[lson[z]] = lson + j - node;
	    }
	  rson[j] = rson[z];
	  dad[rson[z]] = rson + j - node;
	}
      dad[j] = dad[z];
      node[dad[z]] = j;
      dad[z] = NIL;
    }
}

int
getbyte (FILE * in)
{
  unsigned char b;

  if (fread (&b, 1, sizeof (b), in) != sizeof (b))
    return -1;
  return b;
}

int
compress (FILE * in, char *inname, FILE * out, char *outname)
{
  int ch, i, run, len, match, size, mask;
  char buf[17];
  unsigned char headermagic[10];
  unsigned char headersize[4];
  uint32_t filesize;

  /* 28.5 kB */
  buffer = malloc (N + F + (N + 1 + N + N + 256) * sizeof (int));
  if (!buffer)
    {
      fprintf (stderr, "%s: Not enough memory!\n", inname);
      return -1;
    }

  if (fseek (in, 0, SEEK_END) < 0)
    {
      perror (inname);
      return -1;
    }
  filesize = ftell (in);
  if (fseek (in, 0, SEEK_SET) < 0)
    {
      perror (inname);
      return -1;
    }

  /* Fill in header */
  headermagic[0+0] = 0x53;	/* SZDD */
  headermagic[0+1] = 0x5A;
  headermagic[0+2] = 0x44;
  headermagic[0+3] = 0x44;

  headermagic[4+0] = 0x88;
  headermagic[4+1] = 0xf0;
  headermagic[4+2] = 0x27;
  headermagic[4+3] = 0x33;

  headermagic[8+0] = 0x41;
  headermagic[8+1] = inname[strlen (inname) - 1];

  headersize[0] = (filesize >>  0) & 0xff;
  headersize[1] = (filesize >>  8) & 0xff;
  headersize[2] = (filesize >> 16) & 0xff;
  headersize[3] = (filesize >> 24) & 0xff;

  /* Write header to the output file */
  if (fwrite (headermagic, 1, sizeof (headermagic), out) != sizeof (headermagic))
    {
      perror (outname);
      free (buffer);
      return -1;
    }

  if (fwrite (headersize, 1, sizeof (headersize), out) != sizeof (headersize))
    {
      perror (outname);
      free (buffer);
      return -1;
    }

  node = (int *) (buffer + N + F);
  for (i = 0; i < 256; i++)
    root[i] = NIL;
  for (i = NIL; i < N; i++)
    dad[i] = NIL;
  size = mask = 1;
  buf[0] = 0;
  i = N - F - F;
  for (len = 0; len < F && (ch = getbyte (in)) != -1; len++)
    {
      buffer[i + F] = ch;
      i = (i + 1) % N;
    }
  run = len;
  do
    {
      ch = getbyte (in);
      if (i >= N - F)
	{
	  delete (i + F - N);
	  buffer[i + F] = buffer[i + F - N] = ch;
	}
      else
	{
	  delete (i + F);
	  buffer[i + F] = ch;
	}
      match = insert (i, run);
      if (ch == -1)
	{
	  run--;
	  len--;
	}
      if (len++ >= run)
	{
	  if (match >= THRESHOLD)
	    {
//printf("%u{%u,%u}",size,pos, match-3);
	      buf[size++] = pos;
	      buf[size++] = ((pos >> 4) & 0xF0) + (match - 3);
	      len -= match;
	    }
	  else
	    {
	      buf[0] |= mask;
//printf("%u[%c]",size,buffer[i]);
	      buf[size++] = buffer[i];
	      len--;
	    }
	  if (!((mask += mask) & 0xFF))
	    {
	      if (fwrite (buf, 1, size, out) != size)
		{
		  perror (outname);
		  free (buffer);
		  return -1;
		}
	      size = mask = 1;
	      buf[0] = 0;
	    }
	}
      i = (i + 1) & (N - 1);
    }
  while (len > 0);

  if (size > 1)
    if (fwrite (buf, 1, size, out) != size)
      {
	perror (outname);
	free (buffer);
	return -1;
      }

  free (buffer);
  return 0;
}

void
usage (char *progname)
{
  printf ("Usage: %s [-h] [-V] [file ...]\n"
	  " -h --help        give this help\n"
	  " -V --version     display version number\n"
	  " file...          files to compress."
	  "\n"
	  "Report bugs to <mhi@penguin.cz>\n", progname);
  exit (0);
}

int
main (int argc, char **argv)
{
  FILE *in, *out;
  char *argv0;
  int c;
  char *name;

  argv0 = argv[0];

  while ((c = getopt (argc, argv, "hV")) != -1)
    {
      switch (c)
	{
	case 'h':
	  usage (argv0);
	case 'V':
	  printf ("mscompress version %s "__DATE__ " \n",
			version_string);
	  return 0;
	default:
	  usage (argv0);
	}
    }

  argc -= optind;
  argv += optind;

  if (argc == 0)
    {
      fprintf (stderr, "%s: No files specified\n", argv0);
      usage (argv0);
    }

  while (argc)
    {
      if (strlen (argv[0]) < 1)
        {
          continue;
        }
      if (argv[0][strlen (argv[0]) - 1] == '_')
	{
	  fprintf (stderr, "%s: Already ends with underscore -- ignored\n", argv[0]);
	  argc--;
	  argv++;
	  continue;
	}

      in = fopen (argv[0], "rb");
      if (in == NULL)
	{
	  perror (argv[0]);
	  return 1;
	}

      name = malloc (strlen (argv[0]) + 1);
      if (name == NULL)
        {
          perror (argv[0]);
          return 1;
        }
      strcpy (name, argv[0]);
      name[strlen (name) - 1] = '_';

      out = fopen (name, "w+b");
      if (out == NULL)
	{
	  perror (name);
	  return 1;
	}

      compress (in, argv[0], out, name);

      free (name);

      fclose (in);
      fclose (out);

      argc--;
      argv++;
    }

  return 0;
}
