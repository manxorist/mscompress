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

typedef struct state
{
  char buffer[N + F];
  int tree[N + 1 + N + N + 256];
  int pos;
} state;

#define dad (self->tree+1)
#define lson (self->tree+1+N)
#define rson (self->tree+1+N+N)
#define root (self->tree+1+N+N+N)
#define NIL -1

#define max(x,y) ((x)>(y)?(x):(y))
#define min(x,y) ((y)>(x)?(x):(y))

static int
tree_insert (state *self, int i, int run)
{
  int c, j, k, l, n, match;
  int *p;

  k = l = 1;
  match = THRESHOLD - 1;
  p = &root[(unsigned char) self->buffer[i]];
  lson[i] = rson[i] = NIL;

  c = 0;
  while ((j = *p) != NIL)
    {
      n = min (k, l);
      while (n < run && (c = (self->buffer[j + n] - self->buffer[i + n])) == 0)
	n++;

      if (n > match)
	{
	  match = n;
	  self->pos = j;
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
	  dad[lson[j]] = lson + i - self->tree;
	  dad[rson[j]] = rson + i - self->tree;
	  lson[i] = lson[j];
	  rson[i] = rson[j];
	  break;
	}
    }
  dad[i] = p - self->tree;
  *p = i;
  return match;
}

static void
tree_remove (state *self, int z)
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
	      self->tree[dad[j]] = lson[j];
	      dad[lson[j]] = dad[j];
	      lson[j] = lson[z];
	      dad[lson[z]] = lson + j - self->tree;
	    }
	  rson[j] = rson[z];
	  dad[rson[z]] = rson + j - self->tree;
	}
      dad[j] = dad[z];
      self->tree[dad[z]] = j;
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
compress (FILE * in, char *inname, FILE * out, char *outname, int raw)
{
  state *self;
  int ch, i, run, len, match, size, mask;
  char buf[17];
  unsigned char headermagic[10];
  unsigned char headersize[4];
  uint32_t filesize;

  /* 28.5 kB */
  self = malloc (sizeof (state));
  if (!self)
    {
      fprintf (stderr, "%s: Not enough memory!\n", inname);
      return -1;
    }

  if (fseek (in, 0, SEEK_END) < 0)
    {
      perror (inname);
      free (self);
      return -1;
    }
  filesize = ftell (in);
  if (fseek (in, 0, SEEK_SET) < 0)
    {
      perror (inname);
      free (self);
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

  if (!raw)
    {

      /* Write header to the output file */
      if (fwrite (headermagic, 1, sizeof (headermagic), out) != sizeof (headermagic))
        {
          perror (outname);
          free (self);
          return -1;
        }

      if (fwrite (headersize, 1, sizeof (headersize), out) != sizeof (headersize))
        {
          perror (outname);
          free (self);
          return -1;
        }
    }

  for (i = 0; i < 256; i++)
    root[i] = NIL;
  for (i = NIL; i < N; i++)
    dad[i] = NIL;
  size = mask = 1;
  buf[0] = 0;
  i = N - F - F;
  for (len = 0; len < F && (ch = getbyte (in)) != -1; len++)
    {
      self->buffer[i + F] = ch;
      i = (i + 1) % N;
    }
  run = len;
  do
    {
      ch = getbyte (in);
      if (i >= N - F)
	{
	  tree_remove (self, i + F - N);
	  self->buffer[i + F] = self->buffer[i + F - N] = ch;
	}
      else
	{
	  tree_remove (self, i + F);
	  self->buffer[i + F] = ch;
	}
      match = tree_insert (self, i, run);
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
	      buf[size++] = self->pos;
	      buf[size++] = ((self->pos >> 4) & 0xF0) + (match - 3);
	      len -= match;
	    }
	  else
	    {
	      buf[0] |= mask;
//printf("%u[%c]",size,self->buffer[i]);
	      buf[size++] = self->buffer[i];
	      len--;
	    }
	  if (!((mask += mask) & 0xFF))
	    {
	      if (fwrite (buf, 1, size, out) != size)
		{
		  perror (outname);
		  free (self);
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
	free (self);
	return -1;
      }

  free (self);
  return 0;
}

void
usage (char *progname)
{
  printf ("Usage: %s [-h] [-V] [-c] [file ...]\n"
	  " -h --help        give this help\n"
	  " -V --version     display version number\n"
	  " -c               compress to stdout\n"
	  " -r               comress to raw stream without file header\n"
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
  int usestdout;
  int raw;

  argv0 = argv[0];

  usestdout = 0;
  raw = 0;
  while ((c = getopt (argc, argv, "hVcr")) != -1)
    {
      switch (c)
	{
	case 'h':
	  usage (argv0);
	case 'V':
	  printf ("mscompress version %s "__DATE__ " \n",
			version_string);
	  return 0;
	case 'c':
	  usestdout = 1;
	  break;
	case 'r':
	  raw = 1;
	  break;
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

  if (usestdout && argc != 1)
    {
      fprintf (stderr, "%s: -c requires exactly 1 file specified\n", argv0);
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

      if (usestdout)
        {
          out = stdout;
        }
      if (!usestdout)
        {
          out = fopen (name, "w+b");
        }
      if (out == NULL)
	{
	  perror (name);
	  return 1;
	}

      compress (in, argv[0], out, name, raw);

      if (fflush (out) != 0)
        {
          perror (name);
          return 1;
        }

      free (name);

      fclose (in);
      if (!usestdout)
        {
          fclose (out);
        }

      argc--;
      argv++;
    }

  return 0;
}
