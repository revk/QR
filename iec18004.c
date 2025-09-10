// IEC 18004 (QR) encoding
// Copyright (c) 2016 Adrian Kennard Andrews & Arnold Ltd
// This software is provided under the terms of the GPL v2 or later.
// This software is provided free of charge with a full "Money back" guarantee.
// Use entirely at your own risk. We accept no liability. If you don't like that - don't use it.

//#define DEBUG

#if	defined(FB)

#define LIB
#include <heaps.h>
#include "iec18004.h"
#include <reedsol.h>
#include <sys_lib.h>

#define	malloc(n)	HEAP_alloc(heap,n)
#define free(n)		HEAP_free(heap,n)

#elif	defined(ESP_PLATFORM)

#include <ctype.h>
#include <string.h>
#include "iec18004.h"
#include <reedsol.h>
typedef unsigned char ui8;

#else

typedef unsigned char ui8;

#include <stdio.h>
#include <string.h>
#include <popt.h>
#include <time.h>
#include <sys/time.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ctype.h>
#include <err.h>
#include "iec18004.h"
#include <reedsol.h>
#include <assert.h>

#endif

// One day, maybe, Kanji mode - needs iconv UTF-8 to Shift JIS and then coding
// Little point in micro QR as nothing much seems to read them

static char alnum[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ $%*+-./:";

static int eccbytes[40][8] = {  // L, M, Q, H codewords, then L, M, Q, H blocks
   {7, 10, 13, 17, 1, 1, 1, 1}, // 1
   {10, 16, 22, 28, 1, 1, 1, 1},        // 2
   {15, 26, 36, 44, 1, 1, 2, 2},        // 3
   {20, 36, 52, 64, 1, 2, 2, 4},        // 4
   {26, 48, 72, 88, 1, 2, 2 + 2, 2 + 2},        // 5
   {36, 64, 96, 112, 2, 4, 4, 4},       // 6
   {40, 72, 108, 130, 2, 4, 2 + 4, 4 + 1},      // 7
   {48, 88, 132, 156, 2, 2 + 2, 4 + 2, 4 + 2},  // 8
   {60, 110, 160, 192, 2, 3 + 2, 4 + 4, 4 + 4}, // 9
   {72, 130, 192, 224, 2 + 2, 4 + 1, 6 + 2, 6 + 2},     // 10
   {80, 150, 224, 264, 4, 1 + 4, 4 + 4, 3 + 8}, // 11
   {96, 176, 260, 308, 2 + 2, 6 + 2, 4 + 6, 7 + 4},     // 12
   {104, 198, 288, 352, 4, 8 + 1, 8 + 4, 12 + 4},       // 13
   {120, 216, 320, 384, 3 + 1, 4 + 5, 11 + 5, 11 + 5},  // 14
   {132, 240, 360, 432, 5 + 1, 5 + 5, 5 + 7, 11 + 7},   // 15
   {144, 280, 408, 480, 5 + 1, 7 + 3, 15 + 2, 3 + 13},  // 16
   {168, 308, 448, 532, 1 + 5, 10 + 1, 1 + 15, 2 + 17}, // 17
   {180, 338, 504, 588, 5 + 1, 9 + 4, 17 + 1, 2 + 19},  // 18
   {196, 364, 546, 650, 3 + 4, 3 + 11, 17 + 4, 9 + 16}, // 19
   {224, 416, 600, 700, 3 + 5, 3 + 13, 15 + 5, 15 + 10},        // 20
   {224, 442, 644, 750, 4 + 4, 17, 17 + 6, 19 + 6},     // 21
   {252, 476, 690, 816, 2 + 7, 17, 7 + 16, 34}, // 22
   {270, 504, 750, 900, 4 + 5, 4 + 14, 11 + 14, 16 + 14},       // 23
   {300, 560, 810, 960, 6 + 4, 6 + 14, 11 + 16, 30 + 2},        // 24
   {312, 588, 870, 1050, 8 + 4, 8 + 13, 7 + 22, 22 + 13},       // 25
   {336, 644, 952, 1110, 10 + 2, 19 + 4, 28 + 6, 33 + 4},       // 26
   {360, 700, 1020, 1200, 8 + 4, 22 + 3, 8 + 26, 12 + 28},      // 27
   {390, 728, 1050, 1260, 3 + 13, 3 + 23, 4 + 31, 11 + 31},     // 28
   {420, 784, 1140, 1350, 7 + 7, 21 + 7, 1 + 37, 19 + 26},      // 29
   {450, 812, 1200, 1440, 5 + 10, 19 + 10, 15 + 25, 23 + 25},   // 30
   {480, 868, 1290, 1530, 13 + 3, 2 + 29, 42 + 1, 23 + 28},     // 31
   {510, 924, 1350, 1620, 17, 10 + 23, 10 + 35, 19 + 35},       // 32
   {540, 980, 1440, 1710, 17 + 1, 14 + 21, 29 + 19, 11 + 46},   // 33
   {570, 1036, 1530, 1800, 13 + 6, 14 + 23, 44 + 7, 59 + 1},    // 34
   {570, 1064, 1590, 1890, 12 + 7, 12 + 26, 39 + 14, 22 + 41},  // 35
   {600, 1120, 1680, 1980, 6 + 14, 6 + 34, 46 + 10, 2 + 64},    // 36
   {630, 1204, 1770, 2110, 17 + 4, 29 + 14, 49 + 10, 24 + 46},  // 37
   {660, 1260, 1860, 2220, 4 + 18, 13 + 32, 48 + 14, 42 + 32},  // 38
   {720, 1316, 1950, 2310, 20 + 4, 40 + 7, 43 + 22, 10 + 67},   // 39
   {750, 1372, 2040, 2430, 19 + 6, 18 + 31, 34 + 34, 20 + 61},  // 40
};


static unsigned int
bch (int inlen, int errlen, unsigned int poly, unsigned int in)
{
   unsigned int v = in << errlen;
   int n;
   for (n = inlen - 1; n >= 0; n--)
      if (v & (1 << (n + errlen)))
         v ^= (poly << n);
   return (in << errlen) | v;
}

static unsigned int
bch_format (unsigned int f)
{
   return bch (5, 10, 0x537, f) ^ 0x5412;
}

static unsigned int
bch_version (unsigned int v)
{
   return bch (6, 12, 0x1f25, v);
}

static int
qr_bits (int ver, char mode, int len)
{                               // How many bits to encode len characters using mode
   int count = 0;
   switch (mode)
   {
   case 'N':                   // numeric - 10 bits for 3 characters plus 7 for 2 or 4 for 1
      count += 10 * (len / 3);
      if (len % 3 == 2)
         count += 7;
      if (len % 3 == 1)
         count += 4;
      count += 4 + (ver < 10 ? 10 : ver < 27 ? 12 : 14);
      break;
   case 'A':                   // Alphanumeric - 11 bits for 2 characters plus 6 for 1
      count += 11 * (len / 2);
      if (len % 2)
         count += 6;
      count += 4 + (ver < 10 ? 10 : ver < 27 ? 12 : 14);
      break;
   case '8':                   // 8 bit - 8 bits per characters
      count += len * 8;
      count += 4 + (ver < 10 ? 8 : 16);
      break;
   }
   return count;
}

void
qr_mode (char *mode, unsigned char ver, unsigned int len, const char *input)
{                               // Work out a mode to use
   if (!len)
      return;
   int n;
   for (n = 0; n < len; n++)
   {
      if (input[n] && isdigit ((int) input[n]))
         mode[n] = 'N';
      else if (input[n] && strchr (alnum, input[n]))
         mode[n] = 'A';
      else
         mode[n] = '8';
   }
   // Now optimise - we basically consider upgrading a string of one coding to another if that is fewer bits
   n = 0;
   while (n < len)
   {
#ifdef DEBUG
      if (!n)
         fprintf (stderr, "Mode: %.*s\n", len, mode);
#endif
      int run,
        prev = 0,
         next = 0;
      for (run = 0; n + run < len && mode[n + run] == mode[n]; run++);
      if (n)
         for (prev = 1; n - prev > 0 && mode[n - prev] == mode[n - 1]; prev++); // count previous
      if (n + run < len)
         for (next = 1; n + run + next < len && mode[n + run + next] == mode[n + run]; next++);
      if (prev && next && mode[n - 1] < mode[n] && mode[n - 1] == mode[n + run])
      {                         // We are an island should we upgrade to the same as either side
         if (qr_bits (ver, mode[n - 1], prev + run + next) <
             qr_bits (ver, mode[n - 1], prev) + qr_bits (ver, mode[n], run) + qr_bits (ver, mode[n + run], next))
         {                      // yes, upgrade
            while (run--)
            {
               mode[n] = mode[n - 1];
               n++;
            }
            n = 0;              // start again
            continue;
         }
      }
      n += run;
   }
   n = 0;
   while (n < len)
   {
#ifdef DEBUG
      if (!n)
         fprintf (stderr, "Mode: %.*s\n", len, mode);
#endif
      int run,
        prev = 0,
         next = 0;
      for (run = 0; n + run < len && mode[n + run] == mode[n]; run++);
      if (n)
         for (prev = 1; n - prev > 0 && mode[n - prev] == mode[n - 1]; prev++); // count previous
      if (n + run < len)
         for (next = 1; n + run + next < len && mode[n + run + next] == mode[n + run]; next++);
      if (prev && mode[n - 1] < mode[n])
      {                         // Consider upgrading to previous
         if (qr_bits (ver, mode[n - 1], prev + run) < qr_bits (ver, mode[n - 1], prev) + qr_bits (ver, mode[n], run))
         {
            while (run--)
            {
               mode[n] = mode[n - 1];
               n++;
            }
            n = 0;              // start again
            continue;
         }
      } else if (next && mode[n + run] < mode[n])
      {                         // Consider upgrading to next
         if (qr_bits (ver, mode[n + run], run + next) < qr_bits (ver, mode[n], run) + qr_bits (ver, mode[n + run], next))
         {
            while (run--)
            {
               mode[n] = mode[n + run + 1];
               n++;
            }
            n = 0;              // start again
            continue;
         }
      }
      n += run;
   }
}

static int
getmask (int x, int y, int mask)
{                               // mask pattern
   switch (mask & 7)
   {                            // Not i=y, j=x
   case 0:
      return (y + x) % 2 == 0 ? 1 : 0;
   case 1:
      return (y % 2) == 0 ? 1 : 0;
   case 2:
      return (x % 3 == 0) ? 1 : 0;
   case 3:
      return (y + x) % 3 == 0 ? 1 : 0;
   case 4:
      return (y / 2 + x / 3) % 2 == 0 ? 1 : 0;
   case 5:
      return (y * x) % 2 + (y * x) % 3 == 0 ? 1 : 0;
   case 6:
      return ((y * x) % 2 + (y * x) % 3) % 2 == 0 ? 1 : 0;
   case 7:
      return ((y * x) % 3 + (y + x) % 2) % 2 == 0 ? 1 : 0;
   }
   return -1;
}

#define versize(v) ((v)*4+17)

void
qr_padding (unsigned int len, unsigned char *pad)
{                               // Fill in standard padding
   if (!len)
      return;
   pad[0] = 0;
   pad[len - 1] = 0;
   for (unsigned int p = 1; p < len - 1; p++)
      pad[p] = (p & 1) ? 0xEC : 0x11;
}

ui8 *
qr_encode_opts (
#ifdef	FB
                  heap_h heap,
#endif
                  qr_encode_t o)
{                               // Return (malloced) byte array width*width wide (includes mandatory quiet zone)
   static const char ecls[] = "LMQH";
   int n;
   if (o.modep)
      *o.modep = NULL;
   if (o.widthp)
      *o.widthp = 0;
   if (o.verp)
      *o.verp = 0;
   if (o.maskp)
      *o.maskp = 0;
   if (o.eclp)
      *o.eclp = 0;
   if (o.padlenp)
      *o.padlenp = 0;
   if (!o.noquiet && !o.quiet)
      o.quiet = 4;              // default 4 unit quiet
   int ecl = 0;
   if (o.ecl)
   {
      const char *e = strchr (ecls, toupper (o.ecl));
      if (!e)
         return NULL;           // Invalid
      ecl = e - ecls;
   }
   if (o.san > 16 || o.sam > 16 || o.san > o.sam || (o.sam && !o.san))
      return NULL;              // Silly structured append
   if (o.ver > 40)
      return NULL;              // Silly version
   if (o.fnc1 > 2)
      return NULL;              // Silly fnc1
   if (o.mode && !*o.mode)
      o.mode = NULL;            // Silly
   char *mode = malloc (o.len);
   if (!mode)
      return NULL;
   if (o.mode && *o.mode)       // Use provided mode (pad)
      for (n = 0; n < o.len; n++)
      {
         mode[n] = *o.mode;
         if (o.mode[1])
            o.mode++;           // Repeats last mode to end
      }
   if (!o.eci)
   {                            // No ECI set, lets see if we need to set UTF-8, note 5C is Yen in default ECI and 7E is special too
      for (n = 0; n < o.len && !(o.data[n] & 0x80) && o.data[n] != 0x5C && o.data[n] != 0x7E; n++);
      if (n < o.len)
         o.eci = 26;            // Assume UTF-8
   }
   if (o.eci == 26)
   {                            // UTF-8 check valid
      int skip = 0;
      for (n = 0; n < o.len; n++)
      {
         if (skip)
         {
            if ((o.data[n] & 0xC0) != 0x80)
               break;           // Expecting UTF-8 continuation
            skip--;
            continue;
         }
         if ((o.data[n] & 0xF8) == 0xF0)
            skip = 3;
         else if ((o.data[n] & 0xF0) == 0xE0)
            skip = 2;
         else if ((o.data[n] & 0xE0) == 0xC0)
            skip = 1;
         else if (o.data[n] & 0x80)
            break;              // Not sensible UTF-8
      }
      if (n < o.len || skip)
         o.eci = 0;             // Not UTF-8
   }
   if (o.eci == 20)
      o.eci = 0;                // 20 is default (JIS8 and Shift JIS)
#ifdef DEBUG
   fprintf (stderr, "[%d] %.*s ver=%d ecl=%d mask=%d\n", o.len, o.len, o.data, o.ver, ecl, o.mask);
#endif
   int bytes (int v)
   {
      int a = versize (v);
      int pn = 0,
         p = 0;
      if (v > 1)
      {
         pn = (a - 17) / 28 + 2;
         p = pn * pn - 3;
      }
      int b = 64 * 3 + 2 * (a - 16) + p * 25 - (pn > 2 ? (pn - 2) * 10 : 0);;
      return (a * a - b - (v >= 7 ? 67 : 31)) / 8;
   }
   int bits (void)
   {                            // work out bit count 
      int count = 0,
         n = 0;
      if (!o.mode)
         qr_mode (mode, o.ver, o.len, o.data);
      if (o.san)
         count += 12;           // Structured append
      if (o.eci > 16383)
         count += 28;
      else if (o.eci > 127)
         count += 20;
      else if (o.eci)
         count += 12;
      if (o.fnc1)
         count += 4;            // FNC1
      if (o.fnc1 == 2)
         count += 8;            // AI
      while (n < o.len)
      {
         char m = mode[n];
         int q = 0;
         while (n + q < o.len && mode[n + q] == m)
            q++;
         count += qr_bits (o.ver, m, q);
         // validity check
         switch (m)
         {
         case 'N':             // Numeric 
            while (q--)
               if (!o.data[n] || !isdigit ((int) o.data[n++]))
                  return -1;    // Invalid
            break;
         case 'A':             // Alphanumeric
            while (q--)
               if (!o.data[n] || !strchr (alnum, toupper (o.data[n++])))
                  return -1;    // Invalid
            break;
         case '8':             // 8 bit
            n += q;             // all valid
            break;
         default:
            return -1;          // Invalid
         }
      }
      if (o.padlen)
         count = (count + 4 + 7) / 8 * 8 + (o.padlen - 2) * 8;  // Manual padding added, and 0000, even if o.pad is NULL. -2 as first/last are partial
#ifdef DEBUG
      fprintf (stderr, "Ver=%d Bits=%d (%d)\n", o.ver, count, (count + 7) / 8);
#endif
      return count;
   }
   if (!o.ver)
   {                            // Work out version
      int count = 0;
      for (o.ver = 1; o.ver <= 40; o.ver++)
      {
         if (!count || o.ver == 10 || o.ver == 27)
            count = bits ();    // bit count changes at 10 and 27
         if (count < 0)
         {
            free (mode);
            return NULL;        // Not valid
         }
         if ((count + 7) / 8 <= bytes (o.ver) - eccbytes[o.ver - 1][ecl])
            break;              // found one
      }
   } else if (bits () < 0)
   {
      free (mode);
      return NULL;              // not valid
   }
   if (!o.ver || o.ver > 40)
   {
      free (mode);
      return NULL;
   }
   while (versize (o.ver) + o.noquiet * 2 < o.minsize && o.ver < 40)
      o.ver++;
   if (!o.ecl && !o.padlenp && !o.padlen && !o.pad)
   {                            // Can we do better ECL in same size?
      int count = bits ();
      while (ecl < 3 && (count + 7) / 8 <= bytes (o.ver) - eccbytes[o.ver - 1][ecl + 1])
         ecl++;
   }
#ifdef DEBUG
   fprintf (stderr, "ECL=%d Ver=%d Bytes=%d ECC=%d\n", ecl, o.ver, bytes (o.ver), eccbytes[o.ver - 1][ecl]);
   fprintf (stderr, "Data: %.*s\n", o.len, o.data);
   fprintf (stderr, "Mode: %.*s\n", o.len, mode);
#endif
   // Encode data
   int total = bytes (o.ver) - eccbytes[o.ver - 1][ecl];
   ui8 *data = malloc (total);
   if (!data)
      return NULL;
   size_t dataptr = 0;
   unsigned long long v = 0;
   unsigned int b = 0;
   void addbits (int bits, unsigned long long value)
   {                            // Build up the data string
      value &= ((1 << bits) - 1);
      v = ((v << bits) | value);
      b += bits;
      while (b >= 8)
      {
         b -= 8;
         if (dataptr < total)
            data[dataptr++] = (v >> b);
      }
#ifdef DEBUG
      while (bits)
         fputc ('0' + ((v >> --bits) & 1), stderr);
      fputc (' ', stderr);
#endif
   }
   if (o.san)
   {
      addbits (4, 3);           //Structured append
      addbits (4, o.sam - 1);
      addbits (4, o.san - 1);
      addbits (8, o.parity);
   }
   if (o.eci)
   {
      addbits (4, 7);           // ECI
      if (o.eci > 16384)
         addbits (24, 0xC00000 + o.eci);
      else if (o.eci > 127)
         addbits (16, 0x8000 + o.eci);
      else
         addbits (8, o.eci);
   }
   if (o.fnc1 == 1)
      addbits (4, 5);           // FNC1 (1st)
   if (o.fnc1 == 2)
   {
      addbits (4, 9);           // FNC1 (2nd)
      addbits (8, o.ai);
   }
   n = 0;
   while (n < o.len)
   {
      char m = mode[n];
      int q = 0;
      while (n + q < o.len && mode[n + q] == m)
         q++;
      switch (m)
      {
      case 'N':                // Numeric
         addbits (4, 1);        // mode
         addbits (o.ver < 10 ? 10 : o.ver < 27 ? 12 : 14, q);   // Length
         while (q >= 3)
         {
            addbits (10, (o.data[n] - '0') * 100 + (o.data[n + 1] - '0') * 10 + (o.data[n + 2] - '0'));
            n += 3;
            q -= 3;
         }
         if (q == 2)
         {
            addbits (7, (o.data[n] - '0') * 10 + (o.data[n + 1] - '0'));
            n += 2;
         } else if (q == 1)
            addbits (4, (o.data[n++] - '0'));
         break;
      case 'A':                // Alphanumeric
         addbits (4, 2);        // mode
         addbits (o.ver < 10 ? 9 : o.ver < 27 ? 11 : 13, q);    // Length
         while (q >= 2)
         {
            addbits (11, (strchr (alnum, toupper (o.data[n])) - alnum) * 45 + (strchr (alnum, toupper (o.data[n + 1])) - alnum));
            n += 2;
            q -= 2;
         }
         if (q)
            addbits (6, strchr (alnum, toupper (o.data[n++])) - alnum);
         break;
      case '8':                // 8 bit
         addbits (4, 4);        // mode
         addbits (o.ver < 10 ? 8 : 16, q);      // Length
         while (q--)
            addbits (8, o.data[n++]);
         break;
      default:
         free (mode);
         return NULL;
      }
   }
   if (dataptr < total)
      addbits (4, 0);           // terminator
   unsigned int databits = dataptr * 8 + b;
   unsigned int padpos = 0;     // Track pad bytes used
   if (b)
   {                            // Partial
      if (o.pad && padpos < o.padlen)
         addbits (8 - b, o.pad[padpos]);        // pad to byte
      else
         addbits (8 - b, 0);    // pad to byte with zero
   }
   padpos++;                    // First byte is always used for any partial padding, the actual first whole byte is offset 1
   // Padding bytes
   while (dataptr < total)
   {
      if (o.pad && padpos < o.padlen)
         addbits (8, o.pad[padpos]);    // Use padding byte
      else
         addbits (8, (padpos & 1) ? 0xEC : 0x11);
      padpos++;
   }
#ifdef DEBUG
   for (n = 0; n < dataptr; n++)
   {
      fprintf (stderr, "%02X ", data[n]);
      //data[n] = n;
   }
   fprintf (stderr, " (%02X %d)\n", n, n);
#endif
#ifndef	FB
   short *padmap = NULL;
   ui8 *colour = NULL;
#endif
   // Add ECC
   {
      int blocks = eccbytes[o.ver - 1][ecl + 4];
      int ecctotal = eccbytes[o.ver - 1][ecl];
      int eccsize = ecctotal / blocks;
      if (eccsize * blocks != ecctotal)
      {
         free (mode);
         return NULL;
      }
#ifdef FB
      rs_context *context =
#endif
         rs_init (
#ifdef FB
                    heap,
#endif
                    0x11D, eccsize, 0);
      ui8 *ecc = malloc (eccsize);
      if (!ecc)
         return NULL;
      ui8 *final = malloc (total + ecctotal);
      if (!final)
         return NULL;
#ifndef	FB
      if (!(colour = malloc (total + ecctotal)))
         return NULL;
      if (o.padmap)
      {
         if (!(padmap = malloc (sizeof (*padmap) * total)))
            return NULL;
         memset (padmap, -1, sizeof (*padmap) * (total));
      }
#endif
      int datas = total / blocks;
      int datan = blocks - (total - datas * blocks);
#ifdef DEBUG
      fprintf (stderr, "ECC blocks=%d total=%d size=%d data=%d %d*%d %d*%d\n", blocks, ecctotal, eccsize, total, datas, datan,
               datas + 1, blocks - datan);
#endif
      int p = 0,
         q = 0;
      void add (int o)
      {                         // Add byte and set colour
         final[p] = data[o];
#ifndef	FB
         if (o * 8 >= databits)
            // Whole byte pad
            colour[p] = QR_TAG_PAD;
         else if (o * 8 + 7 >= databits)
            colour[p] = QR_TAG_PAD + QR_TAG_DATA;       // Mixed pad and data
         else
            colour[p] = QR_TAG_DATA;    // Whole byte data
         if (padmap && o < total && p < total && o >= (databits - 1) / 8)
            padmap[p] = o - (databits - 1) / 8; // Map which byte of padding this relates to
#endif
         p++;
      }
      for (q = 0; q < datas; q++)
         for (n = 0; n < blocks; n++)
            add (datas * n + (n > datan ? n - datan : 0) + q);
      for (n = datan; n < blocks; n++)
         add (datas * datan + (datas + 1) * (n - datan) + datas);
      p = 0;
      for (n = 0; n < blocks; n++)
      {
         rs_encode (
#ifdef	FB
                      context,
#endif
                      datas, data + p, ecc);
         for (q = 0; q < eccsize; q++)
         {
            int o = total + n + q * blocks;
            final[o] = ecc[q];
#ifndef	FB
            colour[o] = QR_TAG_ECC;
#endif
         }
         p += datas;
         if (n + 1 == datan)
            datas++;
      }
      free (data);
      free (ecc);
      data = final;
      dataptr = total + ecctotal;
   }

#ifdef DEBUG
   for (n = 0; n < dataptr; n++)
      fprintf (stderr, "%02X ", data[n]);
   fprintf (stderr, "\n");
#endif
   int w = versize (o.ver);
   int q = o.quiet;             // Quiet
   ui8 *grid = malloc ((w + q + q) * (w + q + q));
   if (!grid)
      return NULL;
   memset (grid, 0, (w + q + q) * (w + q + q));
   inline int gridxy (int x, int y)
   {                            // Position in grid for x/y where 0/0 is top left of actual code inside the quiet zone
      assert (x >= 0 && x < w && y >= 0 && y < w);
      x += q;
      y += q;
      switch (o.rotate)
      {
      case 1:
         return (w + q + q) * (w + q + q - 1 - x) + y;
      case 2:
         return (w + q + q) * (w + q + q - 1 - y) + (w + q + q - 1 - x);
      case 3:
         return (w + q + q) * x + (w + q + q - 1 - y);
      default:
         return (w + q + q) * y + x;
      }
   }
   inline void set (int x, int y, int v)
   {
      if (x >= 0 && x < w && y >= 0 && y < w)
         grid[gridxy (x, y)] = (v | QR_TAG_SET);
   }
   // Corners
   void target (int x, int y)
   {
      for (n = -1; n < 8; n++)
      {
         set (x + n, y - 1, 0 + QR_TAG_FIXED + QR_TAG_TARGET);
         set (x + n, y + 7, 0 + QR_TAG_FIXED + QR_TAG_TARGET);
      }
      for (n = 0; n < 7; n++)
      {
         set (x - 1, y + n, 0 + QR_TAG_FIXED + QR_TAG_TARGET);
         set (x + 7, y + n, 0 + QR_TAG_FIXED + QR_TAG_TARGET);
      }
      for (n = 0; n < 7; n++)
      {
         set (x + n, y + 0, 1 + QR_TAG_FIXED + QR_TAG_TARGET);
         set (x + n, y + 6, 1 + QR_TAG_FIXED + QR_TAG_TARGET);
      }
      for (n = 1; n < 6; n++)
      {
         set (x + 0, y + n, 1 + QR_TAG_FIXED + QR_TAG_TARGET);
         set (x + 6, y + n, 1 + QR_TAG_FIXED + QR_TAG_TARGET);
      }
      for (n = 1; n < 6; n++)
      {
         set (x + 1, y + n, 0 + QR_TAG_FIXED + QR_TAG_TARGET);
         set (x + 5, y + n, 0 + QR_TAG_FIXED + QR_TAG_TARGET);
      }
      for (n = 2; n < 5; n++)
      {
         set (x + n, y + 1, 0 + QR_TAG_FIXED + QR_TAG_TARGET);
         set (x + n, y + 5, 0 + QR_TAG_FIXED + QR_TAG_TARGET);
      }
      for (n = 2; n < 5; n++)
      {
         set (x + 2, y + n, 1 + QR_TAG_FIXED + QR_TAG_TARGET);
         set (x + 3, y + n, 1 + QR_TAG_FIXED + QR_TAG_TARGET);
         set (x + 4, y + n, 1 + QR_TAG_FIXED + QR_TAG_TARGET);
      }
   }
   target (0, 0);
   target (w - 7, 0);
   target (0, w - 7);
   // Timing
   for (n = 8; n < w - 8; n += 2)
   {
      set (n, 6, 1 + QR_TAG_FIXED + QR_TAG_ALIGN);
      set (6, n, 1 + QR_TAG_FIXED + QR_TAG_ALIGN);
   }
   for (n = 9; n < w - 8; n += 2)
   {
      set (n, 6, 0 + QR_TAG_FIXED + QR_TAG_ALIGN);
      set (6, n, 0 + QR_TAG_FIXED + QR_TAG_ALIGN);
   }
   // Alignment pattern
   if (o.ver > 1)
   {
      int pn = (w - 17) / 28 + 2;
      int ps = ((w - 13) / 2 + pn - 2) / (pn - 1) * 2;
      if (o.ver == 32)
         ps = 26;
      int x,
        y;
      x = 6;
      while (x <= w - 7)
      {
         y = 6;
         while (y <= w - 7)
         {
            if (!((x == 6 && y == 6) || (x == 6 && y == w - 7) || (x == w - 7 && y == 6)))
            {
               for (n = -2; n <= 2; n++)
               {
                  set (x + n, y - 2, 1 + QR_TAG_FIXED + QR_TAG_ALIGN);
                  set (x + n, y + 2, 1 + QR_TAG_FIXED + QR_TAG_ALIGN);
               }
               for (n = -1; n <= 1; n++)
               {
                  set (x - 2, y + n, 1 + QR_TAG_FIXED + QR_TAG_ALIGN);
                  set (x + 2, y + n, 1 + QR_TAG_FIXED + QR_TAG_ALIGN);
                  set (x - 1, y + n, 0 + QR_TAG_FIXED + QR_TAG_ALIGN);
                  set (x + 1, y + n, 0 + QR_TAG_FIXED + QR_TAG_ALIGN);
               }
               set (x, y - 1, 0 + QR_TAG_FIXED + QR_TAG_ALIGN);
               set (x, y + 1, 0 + QR_TAG_FIXED + QR_TAG_ALIGN);
               set (x, y, 1 + QR_TAG_FIXED + QR_TAG_ALIGN);
            }
            if (y == 6)
               y = w - 7 - (pn - 2) * ps;
            else
               y += ps;
         }
         if (x == 6)
            x = w - 7 - (pn - 2) * ps;
         else
            x += ps;
      }
   }
   set (8, w - 8, 1 + QR_TAG_FIXED);
   void setfcode (int mask)
   {                            // Format info
      unsigned int fcode = (((ecl ^ 1) << 3) + (mask & 7));
      fcode = bch_format (fcode);
      for (n = 0; n <= 5; n++)
         set (8, n, (fcode & (1 << n)) ? 1 : 0);
      for (n = 9; n <= 14; n++)
         set (14 - n, 8, (fcode & (1 << n)) ? 1 : 0);
      set (7, 8, (fcode & (1 << 8)) ? 1 : 0);
      set (8, 8, (fcode & (1 << 7)) ? 1 : 0);
      set (8, 7, (fcode & (1 << 6)) ? 1 : 0);
      for (n = 0; n <= 7; n++)
         set (w - n - 1, 8, (fcode & (1 << n)) ? 1 : 0);
      for (n = 8; n <= 14; n++)
         set (8, w - (14 - n) - 1, (fcode & (1 << n)) ? 1 : 0);
   }
   setfcode (o.mask);
   // Version info
   if (o.ver >= 7)
   {
      unsigned int vcode = bch_version (o.ver);
#ifdef DEBUG
      fprintf (stderr, "Vcode=%X\n", vcode);
#endif
      int x,
        y;
      for (x = 0; x < 6; x++)
         for (y = 0; y < 3; y++)
         {
            set (x, w - 11 + y, (vcode & (1 << (y + x * 3))) ? 1 : 0);
            set (w - 11 + y, x, (vcode & (1 << (y + x * 3))) ? 1 : 0);
         }
   }
   // Load data (o.masked)
   {
#ifndef 	FB
      if (o.padmap)
      {
         if (!((*o.padmap) = malloc (sizeof (*o.padmap) * (w + q + q) * (w + q + q))))
            return NULL;
         memset (*o.padmap, -1, sizeof (*o.padmap) * (w + q + q) * (w + q + q));
      }
#endif
      int u = 1;
      int b = 7;
      int x = w - 1;
      int y = w - 1;
      n = 0;
      while (x >= 0 && y >= 0)
      {
         if (!grid[gridxy (x, y)])
         {                      // Store a bit
            int v = QR_TAG_PAD;
            if (n < dataptr)
            {
#ifndef	FB
               v = colour[n];
               if ((v & (QR_TAG_PAD | QR_TAG_DATA)) == (QR_TAG_PAD | QR_TAG_DATA) && 7 - b < (databits & 7))
                  v &= ~QR_TAG_PAD;     // DATA only
               if (o.padmap && (v & QR_TAG_PAD) && n < total && padmap[n] >= 0)
                  (*o.padmap)[gridxy (x, y)] = padmap[n] * 8 + b;
#endif
               v |= (data[n] & (1 << b) ? 1 : 0);
            } else
            {                   // Final bits
               if (o.pad && padpos < o.padlen)
                  v |= ((o.pad[padpos] & (1 << b)) ? 1 : 0);
               if (o.padmap)
                  (*o.padmap)[gridxy (x, y)] = padpos * 8 + b;
            }
            set (x, y, v | QR_TAG_DATA);
            b--;
            if (b < 0)
            {
               b = 7;
               if (n >= dataptr)
                  padpos++;
               else
                  n++;
            }
         }
         if ((x > 6 ? x - 1 : x) & 1)
            x--;
         else
         {
            x++;
            if (u)
            {
               y--;
               if (y < 0)
               {
                  y = 0;
                  x -= 2;
                  if (x == 6)
                     x--;
                  u = 0;
               }
            } else
            {
               y++;
               if (y >= w)
               {
                  y = w - 1;
                  x -= 2;
                  if (x == 6)
                     x--;
                  u = 1;
               }
            }
         }
      }
      if (b < 7)
         padpos++;              // Last byte was used as partial
   }
   unsigned int score (char m)
   {                            // Score based on mask
      unsigned int score = 0;
      setfcode (m);
      int bit (int x, int y)
      {                         // What colour would a bit be based on selected mask being applied
         if (x < 0 || x >= w || y < 0 || y >= w)
            return 0;
         int v = grid[gridxy (x, y)];
         if (v & QR_TAG_DATA)
            v ^= getmask (x, y, m);
         return v & 1;
      }
      // 7.8.3.1
      unsigned int n1 = 3,      // runs 5 or more
         n2 = 3,                // blocks 2x2
         n3 = 40,               // 1:1:3:1:1 patterns
         n4 = 10,               // bad ratio
         c = 0,
         v;
      int x,
        y;
      void checkbit (int b)
      {                         // Check pattern each way
         v = (v << 1) | (b ? 1 : 0);
         if ((v ^ (v >> 1)) & 1)
         {                      // End of a run
            if (c >= 5)
               score += n1 + (c - 5);
            c = 0;
         }
         c++;
         // Pattern
         if ((v & 0xFFF) == 0x0BA || (v & 0xFFF) == 0x5D0)
            score += n3;        // ....X.XXX.X. or .X.XXX.X....
      }
      for (y = 0; y < w; y++)
         for (x = -4; x < w + 5; x++)
            checkbit (x == w + 4 || bit (x, y));
      for (x = 0; x < w; x++)
         for (y = -4; y < w + 5; y++)
            checkbit (y == w + 4 || bit (x, y));
      c = 0;
      for (x = 0; x < w; x++)
         for (y = 0; y < w; y++)
         {
            if ((v = bit (x, y)))
               c++;             // Count black
            if (v == bit (x + 1, y) && v == bit (x, y + 1) && v == bit (x + 1, y + 1))
               score += n2;     // 2x2 blocks
         }
      c = (c * 100 / w / w);    // Percent
      c /= 5;                   // Per 5%
      if (c < 10)
         score += n4 * (10 - c);
      else if (c > 10)
         score += n4 * (c - 10);
      return score;
   }
   {                            // Masking
      int x,
        y;
      if (!o.mask)
      {                         // Auto mask - find best mask using scoring
         unsigned int bestscore = 0;
         signed char best = -1;
         for (char m = 0; m < 8; m++)
         {
            unsigned int s = score (m);
            if (best < 0 || s < bestscore)
            {
               best = m;
               bestscore = s;
            }
         }
         o.mask = best + '0';
         if (o.scorep)
            *o.scorep = bestscore;
      } else if (o.scorep)
         *o.scorep = score (o.mask);
      // Apply mask
      for (x = 0; x < w; x++)
         for (y = 0; y < w; y++)
            if (grid[gridxy (x, y)] & QR_TAG_DATA)      // data bit
               grid[gridxy (x, y)] ^= getmask (x, y, o.mask);
   }
   setfcode (o.mask);
#ifndef	FB
   free (colour);
   if (padmap)
      free (padmap);
#endif
   free (data);
   if (o.modep)
      *o.modep = mode;
   else
      free (mode);
   if (o.widthp)
      *o.widthp = w + q + q;
   if (o.verp)
      *o.verp = o.ver;
   if (o.maskp)
      *o.maskp = '0' + (o.mask & 7);
   if (o.eclp)
      *o.eclp = ecls[ecl];
   if (o.padlenp)
      *o.padlenp = padpos;
   return grid;
}
