// IEC18004 bar code generation
// This software is provided under the terms of the GPL v2 or later.
// This software is provided free of charge with a full "Money back" guarantee.
// Use entirely at your own risk. We accept no liability. If you don't like that - don't use it.

#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include <ctype.h>
#include <string.h>
#include <time.h>
#include <popt.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <image.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/random.h>
#include "iec18004.h"

 // simple checked response malloc
void *
safemalloc (int n)
{
   void *p = malloc (n);
   if (!p)
      errx (1, "Malloc(%d) failed", n);
   return p;
}

// hex dump - bottom left pixel first
void
dumphex (unsigned char *grid, int W, int H, unsigned char p, int S, int B)
{
   int c = 0,
      y;
   for (y = -B * S; y < (H + B) * S; y++)
   {
      int v = 0,
         x,
         b = 128;
      for (x = -B * S; x < (W + B) * S; x++)
      {
         if (x >= 0 && x < W * S && y >= 0 && y < H * S && (grid[(H - 1 - y / S) * W + (x / S)] & 1))
            v |= b;
         b >>= 1;
         if (!b)
         {
            printf ("%02X", v ^ p);
            v = 0;
            b = 128;
            c++;
         }
      }
      if (b != 128)
      {
         printf ("%02X", v ^ p);
         c++;
      }
      printf (" ");
      c++;
      if (c >= 40)
      {
         printf ("\n");
         c = 0;
      }
   }
   if (c)
      printf ("\n");
}

int
main (int argc, const char *argv[])
{
   int c;
   int W = 0,
      H = 0;
   int barcodelen = 0;
   char *outfile = NULL;
   char *infile = NULL;
   char *inenv = NULL;
   char *barcode = NULL;
   char *format = NULL;
   char *eccstr = NULL;
   char *modestr = NULL;
   char *pad = NULL;
   char *mask = NULL;
   char *overlay = NULL;
   char *kicadtag = "${CURRENT_DATE}";
   char *kicadfont = "OCR-B";
   char *kicadlayer = "F.Cu";
   char *kicadpad = "-";
   char *dark = "#000000";
   char *light = "#FFFFFF";
   int ai = 0;
   int ver = 0;
   int eci = 0;
   int fnc1 = 0;
   int sam = 0;
   int san = 0;
   int parity = 0;
   int formatcode = 0;
   int noquiet = 0;
   int rotate = -1;
   int minsize = 0;
   int overlayrepeat = 0;
   int randompad = 0;
   int round = 0;
   double scale = -1,
      dpi = -1;
   int S = -1;
   unsigned char *grid = 0;
   poptContext optCon;          // context for parsing command-line options
   const struct poptOption optionsTable[] = {
      {"barcode", 'c', POPT_ARG_STRING, &barcode, 0, "Barcode", "text"},
      {"infile", 'i', POPT_ARG_STRING, &infile, 0, "Barcode file", "filename"},
      {"inenv", '$', POPT_ARG_STRING, &inenv, 0, "Barcode from environment variable", "envname"},
      {"mode", 'm', POPT_ARG_STRING, &modestr, 0, "Mode", "N/A/8/K"},
      {"ecl", 'e', POPT_ARG_STRING, &eccstr, 0, "EC level", "L/M/Q/H"},
      {"version", 'v', POPT_ARG_INT, &ver, 0, "Version(size)", "1-40"},
      {"mask", 'x', POPT_ARG_STRING, &mask, 0, "Mask", "0-7"},
      {"eci", 'E', POPT_ARG_INT, &eci, 0, "ECI (default UTF-8 if needed)", "N"},
      {"fnc1", 'F', POPT_ARG_INT, &fnc1, 0, "FNC1", "1/2"},
      {"ai", 0, POPT_ARG_INT, &ai, 0, "Application Indication (for FNC1=2)", "0-255"},
      {"number", 'M', POPT_ARG_INT, &sam, 0, "Structured append", "M"},
      {"total", 'N', POPT_ARG_INT, &san, 0, "Structured append", "N"},
      {"parity", 0, POPT_ARG_INT, &parity, 0, "Structured append parity", "0-255"},
      {"dark", 0, POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &dark, 0, "Dark colour (png, eps, and svg)", "Colour"},
      {"light", 0, POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &light, 0, "Light colour (png, eps, and svg)", "Colour"},
      {"outfile", 'o', POPT_ARG_STRING, &outfile, 0, "Output filename", "filename or -"},
      {"svg", 0, POPT_ARG_VAL, &formatcode, 'v', "SVG"},
      {"path", 0, POPT_ARG_VAL, &formatcode, 'V', "SVG path"},
      {"png", 0, POPT_ARG_VAL, &formatcode, 'p', "PNG"},
      {"kicad", 0, POPT_ARG_VAL, &formatcode, 'k', "KiCad foorprint"},
      {"kicad-tag", 0, POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &kicadtag, 0, "KiCad tag below QR", "text"},
      {"kicad-font", 0, POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &kicadfont, 0, "KiCad font", "font"},
      {"kicad-layer", 0, POPT_ARG_STRING | POPT_ARGFLAG_SHOW_DEFAULT, &kicadlayer, 0, "KiCad layer", "layer"},
      {"data", 0, POPT_ARG_VAL, &formatcode, 'd', "PNG Data URI"},
      {"png-colour", 0, POPT_ARGFLAG_DOC_HIDDEN | POPT_ARG_VAL, &formatcode, 'P', "PNG"},
      {"eps", 0, POPT_ARG_VAL, &formatcode, 'e', "EPS"},
      {"ps", 0, POPT_ARG_VAL, &formatcode, 'g', "Postscript"},
      {"text", 0, POPT_ARG_VAL, &formatcode, 't', "Text"},
      {"text-rv", 0, POPT_ARG_VAL, &formatcode, 'T', "Text (reverse video)"},
      {"h-text", 0, POPT_ARG_VAL, &formatcode, 'a', "Half-height text"},
      {"h-text-rv", 0, POPT_ARG_VAL, &formatcode, 'A', "Half-height text (reverse video)"},
      {"q-text", 0, POPT_ARG_VAL, &formatcode, 'q', "Quarter-size text"},
      {"q-text-rv", 0, POPT_ARG_VAL, &formatcode, 'Q', "Quarter-size text (reverse video)"},
      {"binary", 0, POPT_ARG_VAL, &formatcode, 'b', "Binary"},
      {"hex", 0, POPT_ARG_VAL, &formatcode, 'h', "Hex"},
      {"info", 0, POPT_ARG_VAL, &formatcode, 'i', "Info"},
      {"size", 0, POPT_ARG_VAL, &formatcode, 'x', "Size"},
      {"scale", 0, POPT_ARG_INT, &S, 0, "Scale", "pixels"},
      {"mm", 0, POPT_ARG_DOUBLE, &scale, 0, "Size of pixels", "mm"},
      {"dpi", 0, POPT_ARG_DOUBLE, &dpi, 0, "Size of pixels", "dpi"},
      {"pad", 0, POPT_ARG_STRING, &pad, 0, "Custom padding", "Text"},
      {"overlay", 0, POPT_ARG_STRING, &overlay, 0, "Custom padding overlay", ".X./X.X pattern or $var or @file"},
      {"repeat", 0, POPT_ARG_NONE, &overlayrepeat, 0, "Repeat overlay"},
      {"random", 0, POPT_ARG_NONE, &randompad, 0, "Random padding"},
      {"no-quiet", 'Q', POPT_ARG_NONE, &noquiet, 0, "No quiet space"},
      {"right", 'r', POPT_ARG_VAL, &rotate, 3, "Rotate right"},
      {"down", 0, POPT_ARG_VAL, &rotate, 2, "Rotate 180"},
      {"left", 'l', POPT_ARG_VAL, &rotate, 1, "Rotate left"},
      {"up", 0, POPT_ARG_VAL, &rotate, 0, "Rotate 0"},
      {"min-size", 0, POPT_ARG_INT, &minsize, 0, "Min size", "N"},
      {"round", 0, POPT_ARG_NONE, &round, 0, "Non standard round"},
      {"format", 'f', POPT_ARGFLAG_DOC_HIDDEN | POPT_ARG_STRING, &format, 0, "Output format",
       "x=size/t[s]=text/e[s]=EPS/b=bin/h[s]=hex/p[s]=PNG/g[s]=ps/v[s]=svg"},
      POPT_AUTOHELP {
                     NULL, 0, 0, NULL, 0}
   };
   optCon = poptGetContext (NULL, argc, argv, optionsTable, 0);
   poptSetOtherOptionHelp (optCon, "[barcode]");
   if ((c = poptGetNextOpt (optCon)) < -1)
      errx (1, "%s: %s\n", poptBadOption (optCon, POPT_BADOPTION_NOALIAS), poptStrerror (c));

   if (poptPeekArg (optCon) && !barcode && !infile && !inenv)
      barcode = (char *) poptGetArg (optCon);
   if (poptPeekArg (optCon) || (!barcode && !infile && !inenv) || (barcode && infile) || (barcode && inenv) || (infile && inenv))
   {
      poptPrintUsage (optCon, stderr, 0);
      return -1;
   }
   unsigned int colour (const char *c)
   {
      if (*c == '#')
         c++;
      int l = strlen (c);
      const char *p;
      for (p = c; isxdigit (*p); p++);
      if (l < 3 || l == 5 || l == 7 || l > 8 || *p)
         errx (1, "Colour is #RGB, #RGBA, #RRGGBB, or #RRGGBBAA");
      unsigned int v = 0;
      for (p = c; *p; p++)
      {
         int n = (*p & 0xF) + (isalpha (*p) ? 9 : 0);
         if (l > 4)
            v = (v << 4) + n;
         else
            v = (v << 8) + (n << 4) + n;
      }
      if (l == 3 || l == 6)
         v <<= 8;
      v = (v >> 8) + (v << 24); // A at top.
      return v;
   }
   unsigned int darkcolour = colour (dark);
   unsigned int lightcolour = colour (light);

   if (formatcode && format)
      errx (1, "--format is deprecated");
   char formatspace[2] = { };
   if (formatcode)
      *(format = formatspace) = formatcode;
   if (!format)
      format = "t";             // Default

   if (scale >= 0 && dpi >= 0)
      errx (1, "--mm or --dpi");
   if (dpi > 0)
      scale = 25.4 / dpi;
   if (scale >= 0 && S >= 0 && *format != 'e' && *format != 'g')
      errx (1, "--scale or --mm/--dpi");

   if (format && *format && format[1])  // Old scale after format
      S = atoi (format + 1);    // scale
   if (S < 0)
      S = scale;
   if (S <= 0)
      S = 1;
   if (scale < 0)
      scale = 0;

   if (outfile && !strcmp (outfile, "data:"))
   {                            // Legacy format for data:
      if (*format != 'p')
         errx (1, "data: only for png");
      outfile = NULL;
      *format = 'd';
   }
   if (outfile && strcmp (outfile, "-") && !freopen (outfile, "w", stdout))
      err (1, "%s", outfile);

   if (inenv && !(barcode = getenv (inenv)))
      errx (1, "Cannot access $%s", inenv);

   if (infile)
   {                            // read from file
      FILE *f = stdin;
      if (strcmp (infile, "-"))
         f = fopen (infile, "rb");
      barcode = safemalloc (8001);
      if (!f)
         err (1, "%s", infile);
      barcodelen = fread (barcode, 1, 8000, f);
      if (barcodelen < 0)
         err (1, "%s", infile);
      barcode[barcodelen] = 0;  // null terminate anyway
      fclose (f);
   } else
      barcodelen = strlen (barcode);
   char ecl = 0;
   if (eccstr && *eccstr && !strchr ("LMQH", ecl = toupper (*eccstr)))
      errx (1, "ECC mode unknown");
   char *newmode = NULL;
   char newmask = 0,
      newecl = 0;
   unsigned char newver = 0;
   unsigned int score = 0;
   if (overlay)
   {                            // Overlay in padding
      if (rotate < 0)
         rotate = 1;
      if (*overlay == '$' && !(overlay = getenv (overlay + 1)))
         errx (1, "No overlay");
      else if (*overlay == '@')
      {
         struct stat s;
         if (stat (overlay + 1, &s) < 0)
            err (1, "Cannot access %s", overlay + 1);
         if (s.st_size > 50000)
            errx (1, "Silly size overlay");
         int i = open (overlay + 1, O_RDONLY);
         if (i < 0)
            err (1, "Cannot open %s", overlay + 1);
         if (read (i, overlay = malloc (s.st_size + 1), s.st_size) != s.st_size)
            err (1, "Read fail");
         close (i);
         overlay[s.st_size] = 0;
      }
      short *padmap = NULL;
      int padlen = 0;
      if (!ver && !minsize)
         padlen = barcodelen;   // Force some padding
    grid = qr_encode (barcodelen, barcode, ver, ecl, mask ? *mask : 0, modestr, &W, eci: eci, fnc1: fnc1, ai: ai, sam: sam, san: san, parity: parity, noquiet: noquiet, maskp: &newmask, verp: &newver, eclp: &newecl, padmap: &padmap, minsize: minsize, rotate: rotate, padlenp: &padlen, modep: &newmode, scorep:&score);
      H = W;
      if (padlen > 2)
      {                         // Padding available
         unsigned char newpad[padlen];
         qr_padding (padlen, newpad);
         if (randompad)
         {
            if (getrandom (newpad, padlen, 0) != padlen)
               err (1, "Random fail");
            free (grid);
          grid = qr_encode (barcodelen, barcode, newver, newecl, newmask, modestr, &W, eci: eci, fnc1: fnc1, ai: ai, sam: sam, san: san, parity: parity, noquiet: noquiet, padlen: padlen, pad: newpad, padmap: &padmap, minsize: minsize, rotate: rotate, scorep:&score);
         }
         // Find size of overlay
         int ow = 0,
            oh = 0;
         char *o = overlay;
         if (*o)
            while (1)
            {
               char *l = o;
               while (*o && *o != '\n' && *o != '/')
                  o++;
               if (o - l > ow)
                  ow = o - l;
               oh++;
               if ((*o == '\n' && !o[1]) || !*o++)
                  break;
            }
         int q = (noquiet ? 0 : 4);
         int ox = (W - ow) / 2,
            oy = (H - oh) / 2;
         // Find a good place for overlay
         int hits (int ox, int oy)
         {                      // How many hits on pattern at this offset
            int h = 0;
            int y = oy;
            o = overlay;
            while (1)
            {
               int x = ox;
               while (*o && *o != '\n' && *o != '/')
               {
                  int b;
                  if (*o == ' ')
                     h++;       // Don't care
                  else if (x >= 0 && x < W && y >= 0 && y < W)
                  {             // On grid
                     if (padmap[y * W + x] >= 0)
                        h++;    // We can set, so match
                     else if (*o == '.' && !(grid[y * W + x] & QR_TAG_BLACK))
                        h++;    // White match
                     else if (*o != '.' && (grid[y * W + x] & QR_TAG_BLACK))
                        h++;    // Black match
                  }
                  x++;
                  o++;
               }
               if ((*o == '\n' && !o[1]) || !*o++)
                  break;
               y++;
            }
            return h;
         }
         if (!overlayrepeat)
         {                      // Location with max hits, and if a choice, closest to centre
            int bx = -1,
               by = -1,
               b = -1,
               b2 = 0;
            for (int x = 0; x < W; x++)
               for (int y = 0; y < H; y++)
               {
                  int h = hits (x, y);
                  int h2 = (x - ox) * (x - ox) + (y - oy) * (y - oy);   // Far from centre
                  if (h > b || (h == b && (h2 < b2)))
                  {
                     bx = x;
                     by = y;
                     b = h;
                     b2 = h2;
                  }
               }
            ox = bx;
            oy = by;
         } else
         {                      // Location with most whole instances
            int bx = -1,
               by = -1,
               b1 = -1,
               b2 = -1;
            for (int x = 0; x < ow; x++)
               for (int y = 0; y < oy; y++)
               {
                  int h1 = 0,
                     h2 = 0;
                  for (int rx = x - ow; rx < W; rx += ow)
                     for (int ry = y - ow; ry < W; ry += ow)
                     {
                        int h = hits (rx, ry);
                        if (h == ow * oy)
                           h1++;
                        h2 += h;
                     }
                  //if (h1 > b1 || (h1 == b1 && h2 > b2))
                  if (h2 > b2)
                  {
                     bx = x;
                     by = y;
                     b1 = h1;
                     b2 = h2;
                  }
               }
         }
         void set (int x, int y, int v)
         {
            int b;
            if (x < 0 || x >= W || y < 0 || y >= W || (b = padmap[y * W + x]) < 0)
               return;
            if ((grid[y * W + x] & QR_TAG_BLACK) ? !v : v)
               newpad[b / 8] ^= (1 << (b & 7));
         }
         int y = oy;
         o = overlay;
         while (1)
         {
            int x = ox;
            while (*o && *o != '\n' && *o != '/')
            {
               if (*o != ' ')
               {
                  if (overlayrepeat)
                     for (int X = x - ow * (W / ow + 1); X < W; X += ow)
                        for (int Y = y - oh * (W / oh + 1); Y < W; Y += oh)
                           set (X, Y, *o != '.');
                  else
                     set (x, y, *o != '.');
               }
               x++;
               o++;
            }
            if ((*o == '\n' && !o[1]) || !*o++)
               break;
            y++;
         }
         free (grid);
       grid = qr_encode (barcodelen, barcode, newver, newecl, newmask, modestr, &W, eci: eci, fnc1: fnc1, ai: ai, sam: sam, san: san, parity: parity, noquiet: noquiet, padlen: padlen, pad: newpad, padmap: &padmap, minsize: minsize, rotate: rotate, scorep:&score);
      }
   } else
   {                            // Simple
      if (rotate < 0)
         rotate = 0;
      int padlen = 0;
    grid = qr_encode (barcodelen, barcode, ver, ecl, mask ? *mask : 0, modestr, &W, eci: eci, fnc1: fnc1, ai: ai, sam: sam, san: san, parity: parity, noquiet: noquiet, padlen: pad ? strlen (pad) : 0, pad: pad, maskp: &newmask, verp: &newver, eclp: &newecl, modep: &newmode, minsize: minsize, rotate: rotate, scorep: &score, padlenp:&padlen);
      H = W;
      if (randompad && padlen > 2)
      {                         // Padding available
         unsigned char newpad[padlen];
         if (getrandom (newpad, padlen, 0) != padlen)
            err (1, "Random fail");
         free (grid);
       grid = qr_encode (barcodelen, barcode, newver, newecl, newmask, modestr, &W, eci: eci, fnc1: fnc1, ai: ai, sam: sam, san: san, parity: parity, noquiet: noquiet, padlen: padlen, pad: newpad, minsize: minsize, rotate: rotate, scorep:&score);
      }
   }

   // output
   if (tolower (*format) != 'i' && (!grid || !W))
      errx (1, "No barcode produced\n");
   switch (tolower (*format))
   {
   case 'i':                   // info 
      printf ("Version:	%d\n", newver);
      printf ("ECL:		%c\n", newecl);
      printf ("Mask:		%c\n", newmask);
      printf ("Penalty score:	%d\n", score);
      printf ("Size:		%d\n", W);
      printf ("Encoding:	%s\n", newmode);
      break;
   case 'x':                   // size - include space
      printf ("%d", W);
      break;
   case 'h':                   // hex
      dumphex (grid, W, H, 0, S, 0);
      break;
   case 'b':                   // bin
      {
         int y;
         for (y = H * S - 1; y >= 0; y--)       // from top
         {
            int v = 0,
               x,
               b = 128;
            for (x = 0; x < W * S; x++)
            {
               if (grid[(y / S) * W + (x / S)] & 1)
                  v |= b;
               b >>= 1;
               if (!b)
               {
                  putchar (v);
                  v = 0;
                  b = 128;
               }
            }
            if (b != 128)
               putchar (v);
         }
      }
      break;
   case 't':                   // text
      {
         const unsigned char invert = (*format == 'T') ? 0 : 1;
         int y;
         for (y = 0; y < H * S; y++)
         {
            int x;
            for (x = 0; x < (W * S); x++)
               printf ("%s", (grid[W * (y / S) + (x / S)] & 1) ^ invert ? "█" : " ");
            printf ("\n");
         }
      }
      break;
   case 'a':                   // half-height text
      {
         const char *blocks[4] = { " ", "▀", "▄", "█" };
         const unsigned char invert = (*format == 'A') ? 0 : 1;
         int y;
         for (y = 0; y < H * S; y += 2)
         {
            const bool has_bottom = y + 1 < H * S;
            int x;
            for (x = 0; x < W * S; x++)
            {
               unsigned int top = grid[W * (y / S) + (x / S)] & 1;
               unsigned int bottom = has_bottom ? (grid[W * ((y + 1) / S) + (x / S)] & 1) : 0;
               printf ("%s", blocks[(top ^ invert) | ((bottom ^ invert) << 1)]);
            }
            printf ("\n");
         }
      }
      break;
   case 'q':                   // quarter-size text
      {
         const char *blocks[16] = {
            " ", "▘", "▝", "▀",
            "▖", "▌", "▞", "▛",
            "▗", "▚", "▐", "▜",
            "▄", "▙", "▟", "█"
         };
         const unsigned char invert = (*format == 'Q') ? 0 : 1;
         int y;
         for (y = 0; y < H * S; y += 2)
         {
            const bool has_bottom = y + 1 < H * S;
            int x;
            for (x = 0; x < W * S; x += 2)
            {
               const bool has_right = x + 1 < W * S;
               unsigned int top_left = grid[W * (y / S) + (x / S)] & 1;
               unsigned int top_right = has_right ? (grid[W * (y / S) + ((x + 1) / S)] & 1) : 0;
               unsigned int bottom_left = has_bottom ? (grid[W * ((y + 1) / S) + (x / S)] & 1) : 0;
               unsigned int bottom_right = (has_bottom && has_right) ? (grid[W * ((y + 1) / S) + ((x + 1) / S)] & 1) : 0;
               printf ("%s", blocks[(top_left ^ invert)
                                    | ((top_right ^ invert) << 1)
                                    | ((bottom_left ^ invert) << 2) | ((bottom_right ^ invert) << 3)]);
            }
            printf ("\n");
         }
      }
      break;
   case 'e':                   // EPS
      printf ("%%!PS-Adobe-3.0 EPSF-3.0\n" "%%%%Creator: IEC18004 barcode/stamp generator\n" "%%%%BarcodeData: %s\n"
              "%%%%BarcodeSize: %dx%d\n" "%%%%DocumentData: Clean7Bit\n" "%%%%LanguageLevel: 1\n" "%%%%Pages: 1\n"
              "%%%%BoundingBox: 0 0 %d %d\n" "%%%%EndComments\n" "%%%%Page: 1 1\n", barcode, W * S, H * S,
              (int) ((double) W * (scale * 72 / 25.4 ? : S) + .99), (int) ((double) H * (scale * 72 / 25.4 ? : S) + 0.99));
   case 'g':                   // PS
      //printf ("%d %d 1[1 0 0 1 -%d -%d]{<\n", W * S, H * S, S, S);
      if (scale)
         printf ("%.4f dup scale ", (scale * 72 / 25.4 / S));
      if (lightcolour != 0xFFFFFF || darkcolour != 0)
      {
         printf ("%d %d 8[1 0 0 1 0 0]{<\n", W * S, H * S);
         for (int y = 0; y < H * S; y++)
         {
            for (int x = 0; x < W * S; x++)
            {
               if (grid[(H - 1 - y / S) * W + (x / S)] & 1)
                  printf ("%02X%02X%02X", (darkcolour >> 16) & 0xFF, (darkcolour >> 8) & 0xFF, darkcolour & 0xFF);
               else
                  printf ("%02X%02X%02X", (lightcolour >> 16) & 0xFF, (lightcolour >> 8) & 0xFF, lightcolour & 0xFF);
            }
            printf ("\n");
         }
         printf (">}false 3 colorimage\n");
      } else
      {
         printf ("%d %d 1[1 0 0 1 0 0]{<\n", W * S, H * S);
         dumphex (grid, W, H, 0xFF, S, 0);
         printf (">}image\n");
      }
      break;
   case 'v':                   // svg
      {
         if (round)
         {                      // Non standard
            printf
               ("<svg xmlns:svg=\"http://www.w3.org/2000/svg\" xmlns=\"http://www.w3.org/2000/svg\" version=\"1.0\" width=\"%d\" height=\"%d\"><rect width=\"%d\" height=\"%d\" fill=\"white\"/><g fill=\"black\" stroke=\"none\">",
                W, H, W, H);
            for (int y = 0; y < H; y++)
               for (int x = 0; x < W; x++)
                  if (grid[y * W + x] & QR_TAG_BLACK)
                  {
                     if (grid[y * W + x] & QR_TAG_TARGET)
                        printf ("<rect x=\"%d\" y=\"%d\" width=\"1\" height=\"1\"/>", x, y);
                     else
                        printf ("<circle cx=\"%d.5\" cy=\"%d.5\" r=\"0.5\"/>", x, y);
                  }
            printf ("</g></svg>");
         } else
         {
            Image *i;
            i = ImageNew (W, H, 2);
            i->Colour[0] = lightcolour;
            i->Colour[1] = darkcolour;
            for (int y = 0; y < H; y++)
               for (int x = 0; x < W; x++)
                  if (grid[y * W + x] & 1)
                     ImagePixel (i, x, y) = 1;
            if (isupper (*format))
               ImageSVGPath (i, stdout, 1);
            else
               ImageWriteSVG (i, stdout, 0, -1, barcode, scale);
            ImageFree (i);
         }
      }
      break;
   case 'd':                   // png data
   case 'p':                   // png
      {
         int x,
           y;
         Image *i;
         if (*format == 'P')
         {                      // Special mode to make coloured QR code with different logical parts
            i = ImageNew (W * S, H * S, 12);
            i->Colour[0] = 0xFFFF88;    // Quiet
            i->Colour[1] = 0x000000;
            i->Colour[2] = 0xFFCCCC;    // ECC
            i->Colour[3] = 0x440000;
            i->Colour[4] = 0xCCFFCC;    // PAD
            i->Colour[5] = 0x004400;
            i->Colour[6] = 0xCCCCFF;    // DATA
            i->Colour[7] = 0x000044;
            i->Colour[8] = lightcolour; // FIXED
            i->Colour[9] = darkcolour;
            i->Colour[10] = 0xCCCCCC;   // FORMAT
            i->Colour[11] = 0x444444;
            for (y = 0; y < H * S; y++)
               for (x = 0; x < W * S; x++)
               {
                  unsigned char u = grid[(H - 1 - y / S) * W + (x / S)],
                     o = (u & 1);
                  if (u & QR_TAG_ECC)
                     o += 2;
                  else if (u & QR_TAG_PAD)
                     o += 4;
                  else if (u & QR_TAG_DATA)
                     o += 6;
                  else if (u & QR_TAG_FIXED)
                     o += 8;
                  else if (u & QR_TAG_SET)
                     o += 10;
                  ImagePixel (i, x, (H * S) - y - 1) = o;
               }
         } else
         {
            i = ImageNew (W * S, H * S, 2);
            i->Colour[0] = lightcolour;
            i->Colour[1] = darkcolour;
            for (y = 0; y < H * S; y++)
               for (x = 0; x < W * S; x++)
                  if (grid[(y / S) * W + (x / S)] & 1)
                     ImagePixel (i, x, y) = 1;
         }
         if (*format == 'd')
         {                      // data URI
            char *buf;
            size_t len;
            FILE *f = open_memstream (&buf, &len);
            ImageWritePNG (i, f, 0, -1, barcode);
            fclose (f);
            printf ("data:image/png;base64,");
            static const char BASE64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
            int b = 0,
               v = 0,
               i = 0;
            while (i < len)
            {
               unsigned char c = buf[i++];
               b += 8;
               v = (v << 8) | c;
               while (b >= 6)
               {
                  b -= 6;
                  putchar (BASE64[(v >> b) & 0x3F]);
               }
            }
            if (b)
            {
               b += 8;
               v = (v << 8);
               b -= 6;
               putchar (BASE64[(v >> b) & 0x3F]);
            }
            while (b)
            {
               if (b < 6)
                  b += 8;
               b -= 6;
               putchar ('=');
            }
            free (buf);
         } else
            ImageWritePNG (i, stdout, 0, -1, barcode);
         ImageFree (i);
      }
      break;
   case 'k':                   // KiCad footprint
      {
         if (scale <= 0)
            scale = 0.2;        // Default (seems this does work in copper, may need to be bigger for silk screen)
         float U = scale * S,
            w = W * U / 2,
            h = H * U / 2,
            q = 0.05,
            t = 0;
         printf ("(module QR (layer %s)\n", kicadlayer);
         printf ("(attr exclude_from_pos_files exclude_from_bom allow_soldermask_bridges)\n");
         if (*kicadtag)
         {
            printf ("(fp_text user \"%s\" (at 0 3.6) (layer %s) (effects (font ", kicadtag, kicadlayer);
            if (*kicadfont)
               printf ("(face \"%s\") ", kicadfont);
            printf ("(size 0.5 0.5) (thickness 0.1))))\n");
            t = 1.4;
         }
         printf ("(fp_text reference REF** (at 0 %f) (layer F.SilkS) hide (effects (font (size 1 1) (thickness 0.15))))\n", h + 1);
         printf ("(fp_text value QR (at 0 %f) (layer F.Fab) hide (effects (font (size 1 1) (thickness 0.15))))\n", h + 1);
         printf ("(fp_line (start %f %f) (end %f %f) (layer F.Fab) (width 0.1))\n", -w, -h, -w, h + t);
         printf ("(fp_line (start %f %f) (end %f %f) (layer F.Fab) (width 0.1))\n", -w, h + t, w, h + t);
         printf ("(fp_line (start %f %f) (end %f %f) (layer F.Fab) (width 0.1))\n", w, h + t, w, -h);
         printf ("(fp_line (start %f %f) (end %f %f) (layer F.Fab) (width 0.1))\n", w, -h, -w, -h);
         for (int y = 0; y < H; y++)
            for (int x = 0; x < W; x++)
               if (grid[y * W + x] & 1)
               {
                  int r,
                    b;
                  for (r = x + 1; r < W && (grid[y * W + r] & 1); r++);
                  for (b = y + 1; b < H; b++)
                  {
                     int z;
                     for (z = x; z < r && (grid[b * W + z] & 1); z++);
                     if (z < r)
                        break;
                  }
                  printf ("(pad \"%s\" smd rect (at %f %f) (size %f %f) (layers %s) (clearance %f))\n", kicadpad,
                          U * (x + r) / 2 - w, h - U * (b + y) / 2, U * (r - x), U * (b - y), kicadlayer, U);
                  for (int X = x; X < r; X++)
                     for (int Y = y; Y < b; Y++)
                        grid[Y * W + X] = 0;
               }
         if (strstr (kicadlayer, "Cu"))
         {
            printf ("(fp_line (start %f %f) (end %f %f) (layer F.CrtYd) (width 0.1))\n", -w, -h, -w, h + t);
            printf ("(fp_line (start %f %f) (end %f %f) (layer F.CrtYd) (width 0.1))\n", -w, h + t, w, h + t);
            printf ("(fp_line (start %f %f) (end %f %f) (layer F.CrtYd) (width 0.1))\n", w, h + t, w, -h);
            printf ("(fp_line (start %f %f) (end %f %f) (layer F.CrtYd) (width 0.1))\n", w, -h, -w, -h);
            printf ("(fp_poly (pts (xy %f %f) (xy %f %f) (xy %f %f) (xy %f %f)) (layer F.Mask) (width 0))\n", -w, -h, -w, h, w, h,
                    w, -h);
            printf ("(zone (net 0) (net_name \"\") (layer \"F.Cu\") (hatch edge 0.508)\n" "(connect_pads (clearance 0))\n"
                    "(min_thickness 0.254)\n"
                    "(keepout (tracks not_allowed) (vias not_allowed) (copperpour not_allowed) (footprints not_allowed))\n"
                    "(fill (thermal_gap 0.508) (thermal_bridge_width 0.508))\n"
                    "(polygon (pts (xy %f %f) (xy %f %f) (xy %f %f) (xy %f %f))))\n", w + q, h + t + q, -w - q, h + t + q, -w - q,
                    -h - q, w + q, -h - q);
         }
         printf ("(fp_text user \"%s\" (at 0 2.5 unlocked) (layer \"F.Fab\")\n"
                 "(effects (font (size 0.1 0.1) (thickness 0.02))))\n", barcode);
         printf (")\n");
      }
      break;
   default:
      errx (1, "Unknown output format %s\n", format);
      break;
   }
   return 0;
}
