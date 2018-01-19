//  icopacker.c
//  icopacker
//
//  compilation:
//    clang -g0 -O3 -std=c11 -fno-pic -fstrict-aliasing -fno-common -fvisibility=hidden -Wno-parentheses icopacker.c -o icopacker
//
//  usage:
//    icopacker <PNG_input_file_directory> <ICO_output_file>
//
//  Created by Dr. Rolf Jansen on 20017-07-18.
//  Copyright © 2017 projectworld.net. All rights reserved.
//
//  Redistribution and use in source and binary forms, with or without modification,
//  are permitted provided that the following conditions are met:
//
//  1. Redistributions of source code must retain the above copyright notice,
//     this list of conditions and the following disclaimer.
//
//  2. Redistributions in binary form must reproduce the above copyright notice,
//     this list of conditions and the following disclaimer in the documentation
//     and/or other materials provided with the distribution.
//
//  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS
//  OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
//  AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER
//  OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
//  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
//  DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
//  IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
//  THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.


#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>


#define no_error 0

// byte order mapping mapLExx()/mapBExx()
// - depending on the endianess of the host the bytes are swapped or not
// - BE/LE may be either the endianess of the source or of the target, because byte swapping is direction agnostic
// - mapping 2 times would return the original order

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

   #define mapLE16(x) (x)
   #define mapLE32(x) (x)
   #define mapLE64(x) (x)

   #define mapBE16(x) SwapInt16(x)
   #define mapBE32(x) SwapInt32(x)
   #define mapBE64(x) SwapInt64(x)

   #if (defined(__i386__) || defined(__x86_64__)) && defined(__GNUC__)

      static inline uint16_t SwapInt16(uint16_t x)
      {
         __asm__("rolw $8,%0" : "+q" (x));
         return x;
      }

      static inline uint32_t SwapInt32(uint32_t x)
      {
         __asm__("bswapl %0" : "+q" (x));
         return x;
      }

   #else

      static inline uint16_t SwapInt16(uint16_t x)
      {
         uint16_t z;
         char *p = (char *)&x;
         char *q = (char *)&z;

         q[0] = p[1];
         q[1] = p[0];

         return z;
      }

      static inline uint32_t SwapInt32(uint32_t x)
      {
         uint32_t z;
         char *p = (char *)&x;
         char *q = (char *)&z;

         q[0] = p[3];
         q[1] = p[2];
         q[2] = p[1];
         q[3] = p[0];

         return z;
      }

   #endif

   #if defined(__x86_64__) && defined(__GNUC__)

      static inline uint64_t SwapInt64(uint64_t x)
      {
         __asm__("bswapq %0" : "+q" (x));
         return x;
      }

   #else

      static inline uint64_t SwapInt64(uint64_t x)
      {
         uint64_t z;
         char *p = (char *)&x;
         char *q = (char *)&z;

         q[0] = p[7];
         q[1] = p[6];
         q[2] = p[5];
         q[3] = p[4];
         q[4] = p[3];
         q[5] = p[2];
         q[6] = p[1];
         q[7] = p[0];

         return z;
      }

   #endif

#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__

   #define mapLE16(x) SwapInt16(x)
   #define mapLE32(x) SwapInt32(x)
   #define mapLE64(x) SwapInt64(x)

   #define mapBE16(x) (x)
   #define mapBE32(x) (x)
   #define mapBE64(x) (x)

   #if defined(__ppc__) && defined(__GNUC__)

      static inline uint16_t SwapInt16(uint16_t x)
      {
         uint16_t z;
         __asm__("lhbrx %0,0,%1" : "=r" (z) : "r" (&x), "m" (x));
         return z;
      }

   #else

      static inline uint16_t SwapInt16(uint16_t x)
      {
         uint16_t z;
         char *p = (char *)&x;
         char *q = (char *)&z;

         q[0] = p[1];
         q[1] = p[0];

         return z;
      }

   #endif

   static inline uint32_t SwapInt32(uint32_t x)
   {
      uint32_t z;
      char *p = (char *)&x;
      char *q = (char *)&z;

      q[0] = p[3];
      q[1] = p[2];
      q[2] = p[1];
      q[3] = p[0];

      return z;
   }

   static inline uint64_t SwapInt64(uint64_t x)
   {
      uint64_t z;
      char *p = (char *)&x;
      char *q = (char *)&z;

      q[0] = p[7];
      q[1] = p[6];
      q[2] = p[5];
      q[3] = p[4];
      q[4] = p[3];
      q[5] = p[2];
      q[6] = p[1];
      q[7] = p[0];

      return z;
   }

#endif


#pragma pack(1)

// ICO file format: https://en.wikipedia.org/wiki/ICO_(file_format)

typedef struct
{
   uint8_t     width, height; // any number between 0 and 255 whereby 0 means image width/height is 256 pixels
   uint8_t     colorcount;    // number of colors in the color palette, should be 0 if the image does not use a color palette
   uint8_t     reserverd;     // should be 0
   union
   {
      uint16_t planescount;   // icon:   number of color planes, should be 0 or 1
      uint16_t hspot;         // cursor: horizontal coordinates of the hotspot in number of pixels from the left
   };
   union
   {
      uint16_t pixeldepth;    // icon:   may be set to 0, since PNG data carries its pixel depth as a property
      uint16_t vspot;         // cursor: vertical coordinates of the hotspot in number of pixels from the top
   };
   uint32_t    imagesize;     // size of the image's data in bytes
   uint32_t    imageoffset;   // offset of BMP or PNG data from the beginning of the ICO/CUR file
} ICOEntry;


typedef struct
{
   uint16_t    reserved;      // must be 0
   uint16_t    filetype;      // 1 for icon, 2 for cursor
   uint16_t    imgcount;      // number of images in the file
   ICOEntry    entry[0];
} ICODirectory;

#pragma pack()


int main(int argc, const char *argv[])
{
   if (argc == 3)
   {
      char *pngdir = (char *)argv[1];
      int   pngdl  = (int)strlen(pngdir);
      struct stat st;
      if (lstat(pngdir, &st) == no_error && S_ISDIR(st.st_mode))
      {
         DIR           *dp;
         struct dirent *ep, bp;

         if (dp = opendir(pngdir))
         {
            ICODirectory *icodir;
            uint8_t      *icodat;
            uint32_t      dircnt = 10,
                          count  = 0,
                          bufsiz = 1048576,
                          offset = 0;

            if ((icodir = malloc(sizeof(ICODirectory) + dircnt*sizeof(ICOEntry))) && (icodat = malloc(bufsiz)))
            {
               *icodir = (ICODirectory){0, mapLE16(1), 0, {}};

               while (count <= UINT16_MAX && readdir_r(dp, &bp, &ep) == no_error && ep)
                  if (ep->d_name[0] != '.' && ep->d_type == DT_REG && *(int32_t *)(ep->d_name+ep->d_namlen-4) == *(int32_t *)".png")
                  {
                     int  pl = pngdl+1+ep->d_namlen;
                     char fp[pl+1];
                     strcpy(fp, pngdir);
                     fp[pngdl] = '/';
                     strcpy(fp+pngdl+1, ep->d_name);

                     int pngfd;
                     if (lstat(fp, &st) == no_error && st.st_size <= UINT32_MAX && (pngfd = open(fp, O_RDONLY)) != -1)
                     {
                        if ((count               < dircnt || (icodir = reallocf(icodir, sizeof(ICODirectory) + (dircnt += 10)*sizeof(ICOEntry))))
                         && (offset + st.st_size < bufsiz || (icodat = reallocf(icodat, bufsiz+st.st_size+1048576))))
                        {
                           uint32_t *png = (uint32_t *)(icodat+offset);
                           uint32_t  w, h;

                           if (read(pngfd, png, st.st_size) == st.st_size
                            && png[0] == *(uint32_t *)"\x89PNG"         // PNG signature -- https://en.wikipedia.org/wiki/Portable_Network_Graphics
                            && png[1] == *(uint32_t *)"\r\n\x1A\n"      // signature boundary
                            && mapBE32(png[2]) == 13                    // image header chunk size must be 13
                            && png[3] == *(uint32_t *)"IHDR"            // image header chunk type must be IHDR
                            && 0 < (w = mapBE32(png[4])) && w <= 256    // accept image width in the range from 1 to 256
                            && 0 < (h = mapBE32(png[5])) && h <= 256)   // accept image height in the range from 1 to 256
                           {
                              uint16_t pixeldepth;
                              uint8_t *colorspec = (uint8_t *)&png[6];  // evaluate the pixel depth
                              switch (colorspec[1])
                              {
                                 case 0: // grayscale
                                    pixeldepth = 1*colorspec[0];
                                    break;

                                 case 2: // rgb
                                    pixeldepth = 3*colorspec[0];
                                    break;

                                 case 3: // indexed
                                    pixeldepth = 1*colorspec[0];
                                    break;

                                 case 4: // grayscale + alpha
                                    pixeldepth = 2*colorspec[0];
                                    break;

                                 case 6: // rgb + alpha
                                    pixeldepth = 4*colorspec[0];
                                    break;

                                 default:
                                    goto ignore;
                              }

                              if (pixeldepth <= 32)
                              {
                                 icodir->entry[count++] = (ICOEntry){(uint8_t)((w != 256) ? w : 0),
                                                                     (uint8_t)((h != 256) ? h : 0),
                                                                     0, 0,
                                                                     {mapLE16(1)}, {mapLE16(pixeldepth)},
                                                                     mapLE32((uint32_t)st.st_size),
                                                                     offset /* imageoffset will be mapped to little endian in the wrapping-up stage */};
                                 offset += st.st_size;
                                 printf("added %s, %ux%u, %hubit\n", ep->d_name, h, w, pixeldepth);
                              }
                           }
                        }

                     ignore:
                        close(pngfd);
                     }
                  }

               int icofd;
               if (count && offset && (icofd = open(argv[2], O_WRONLY|O_CREAT|O_TRUNC|O_EXLOCK, 0644)) != -1)
               {
                  icodir->imgcount = mapLE16((uint16_t)count);

                  uint32_t dirsiz = sizeof(ICODirectory) + count*sizeof(ICOEntry);
                  for (int i = 0; i < count; i++)
                     icodir->entry[i].imageoffset = mapLE32(dirsiz + icodir->entry[i].imageoffset);

                  if (write(icofd, icodir, dirsiz))
                     write(icofd, icodat, offset);

                  printf("\n%u PNG files from %s have been packed into\nthe ICO file %s,\nhaving a total size of %u bytes.\n\n", count, argv[1], argv[2], dirsiz+offset);
                  close(icofd);
               }

               free(icodat);
               free(icodir);
            }

            closedir(dp);
         }
      }
   }

   else
      printf("Usage: %s <PNG_input_file_directory> <ICO_output_file>\n\n", argv[0]);

   return 0;
}
