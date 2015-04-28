#include <time.h>
#include <stdlib.h>
#include <memory.h>

#include "Common.h"
#include "Config.h"
#include "OpenGL.h"
#include "Textures.h"
#include "GBI.h"
#include "RSP.h"
#include "gDP.h"
#include "gSP.h"
#include "N64.h"
#include "CRC.h"
#include "convert.h"
//#include "FrameBuffer.h"

#define FORMAT_NONE     0
#define FORMAT_I8       1
#define FORMAT_IA88     2
#define FORMAT_RGBA4444 3
#define FORMAT_RGBA5551 4
#define FORMAT_RGBA8888 5

#ifdef __GNUC__
# define likely(x) __builtin_expect((x),1)
# define unlikely(x) __builtin_expect((x),0)
# define prefetch(x, y) __builtin_prefetch((x),(y))
#else
# define likely(x) (x)
# define unlikely(x) (x)
# define prefetch(x, y)
#endif

TextureCache    cache;

typedef u32 (*GetTexelFunc)( void *src, u16 x, u16 i, u8 palette );

u32 GetNone( void *src, u16 x, u16 i, u8 palette )
{
   return 0x00000000;
}

u32 GetCI4IA_RGBA4444( void *src, u16 x, u16 i, u8 palette )
{
   u8 color4B = ((u8*)src)[(x>>1)^(i<<1)];
   if (x & 1)
      return IA88_RGBA4444( *(u16*)&TMEM[256 + (palette << 4) + (color4B & 0x0F)] );
   return IA88_RGBA4444( *(u16*)&TMEM[256 + (palette << 4) + (color4B >> 4)] );
}

u32 GetCI4IA_RGBA8888( void *src, u16 x, u16 i, u8 palette )
{
   u8 color4B = ((u8*)src)[(x>>1)^(i<<1)];
   if (x & 1)
      return IA88_RGBA8888( *(u16*)&TMEM[256 + (palette << 4) + (color4B & 0x0F)] );
   return IA88_RGBA8888( *(u16*)&TMEM[256 + (palette << 4) + (color4B >> 4)] );
}

u32 GetCI4RGBA_RGBA5551( void *src, u16 x, u16 i, u8 palette )
{
   u8 color4B = ((u8*)src)[(x>>1)^(i<<1)];
   if (x & 1)
      return RGBA5551_RGBA5551( *(u16*)&TMEM[256 + (palette << 4) + (color4B & 0x0F)] );
   return RGBA5551_RGBA5551( *(u16*)&TMEM[256 + (palette << 4) + (color4B >> 4)] );
}

u32 GetCI4RGBA_RGBA8888( void *src, u16 x, u16 i, u8 palette )
{
   u8 color4B = ((u8*)src)[(x>>1)^(i<<1)];
   if (x & 1)
      return RGBA5551_RGBA8888( *(u16*)&TMEM[256 + (palette << 4) + (color4B & 0x0F)] );
   return RGBA5551_RGBA8888( *(u16*)&TMEM[256 + (palette << 4) + (color4B >> 4)] );
}

u32 GetIA31_RGBA8888( void *src, u16 x, u16 i, u8 palette )
{
   u8 color4B = ((u8*)src)[(x>>1)^(i<<1)];
   return IA31_RGBA8888( (x & 1) ? (color4B & 0x0F) : (color4B >> 4) );
}

u32 GetIA31_RGBA4444( void *src, u16 x, u16 i, u8 palette )
{
   u8 color4B = ((u8*)src)[(x>>1)^(i<<1)];
   return IA31_RGBA4444( (x & 1) ? (color4B & 0x0F) : (color4B >> 4) );
}

u32 GetIA31_IA88( void *src, u16 x, u16 i, u8 palette )
{
   u8 color4B = ((u8*)src)[(x>>1)^(i<<1)];
   return IA31_IA88( (x & 1) ? (color4B & 0x0F) : (color4B >> 4) );
}

u32 GetI4_RGBA8888( void *src, u16 x, u16 i, u8 palette )
{
    u8 color4B = ((u8*)src)[(x>>1)^(i<<1)];
    return I4_RGBA8888( (x & 1) ? (color4B & 0x0F) : (color4B >> 4) );
}

u32 GetI4_RGBA4444( void *src, u16 x, u16 i, u8 palette )
{
    u8 color4B = ((u8*)src)[(x>>1)^(i<<1)];
    return I4_RGBA4444( (x & 1) ? (color4B & 0x0F) : (color4B >> 4) );
}

u32 GetI4_I8( void *src, u16 x, u16 i, u8 palette )
{
   u8 color4B = ((u8*)src)[(x>>1)^(i<<1)];
   return I4_I8( (x & 1) ? (color4B & 0x0F) : (color4B >> 4) );
}


u32 GetI4_IA88( void *src, u16 x, u16 i, u8 palette )
{
   u8 color4B = ((u8*)src)[(x>>1)^(i<<1)];
   return I4_IA88( (x & 1) ? (color4B & 0x0F) : (color4B >> 4) );
}

u32 GetCI8IA_RGBA4444( void *src, u16 x, u16 i, u8 palette )
{
   return IA88_RGBA4444( *(u16*)&TMEM[256 + ((u8*)src)[x^(i<<1)]] );
}

u32 GetCI8IA_RGBA8888( void *src, u16 x, u16 i, u8 palette )
{
   return IA88_RGBA8888( *(u16*)&TMEM[256 + ((u8*)src)[x^(i<<1)]] );
}

u32 GetCI8RGBA_RGBA5551( void *src, u16 x, u16 i, u8 palette )
{
   return RGBA5551_RGBA5551( *(u16*)&TMEM[256 + ((u8*)src)[x^(i<<1)]] );
}

u32 GetCI8RGBA_RGBA8888( void *src, u16 x, u16 i, u8 palette )
{
   return RGBA5551_RGBA8888( *(u16*)&TMEM[256 + ((u8*)src)[x^(i<<1)]] );
}

u32 GetIA44_RGBA8888( void *src, u16 x, u16 i, u8 palette )
{
   return IA44_RGBA8888(((u8*)src)[x^(i<<1)]);
}

u32 GetIA44_RGBA4444( void *src, u16 x, u16 i, u8 palette )
{
   return IA44_RGBA4444(((u8*)src)[x^(i<<1)]);
}

u32 GetIA44_IA88( void *src, u16 x, u16 i, u8 palette )
{
   return IA44_IA88(((u8*)src)[x^(i<<1)]);
}

u32 GetI8_RGBA8888( void *src, u16 x, u16 i, u8 palette )
{
   return I8_RGBA8888(((u8*)src)[x^(i<<1)]);
}

u32 GetI8_I8( void *src, u16 x, u16 i, u8 palette )
{
   return ((u8*)src)[x^(i<<1)];
}

u32 GetI8_IA88( void *src, u16 x, u16 i, u8 palette )
{
   return I8_IA88(((u8*)src)[x^(i<<1)]);
}

u32 GetI8_RGBA4444( void *src, u16 x, u16 i, u8 palette )
{
   return I8_RGBA4444(((u8*)src)[x^(i<<1)]);
}

u32 GetRGBA5551_RGBA8888( void *src, u16 x, u16 i, u8 palette )
{
   return RGBA5551_RGBA8888( ((u16*)src)[x^i] );
}

u32 GetRGBA5551_RGBA5551( void *src, u16 x, u16 i, u8 palette )
{
   return RGBA5551_RGBA5551( ((u16*)src)[x^i] );
}

u32 GetIA88_RGBA8888( void *src, u16 x, u16 i, u8 palette )
{
   return IA88_RGBA8888(((u16*)src)[x^i]);
}

u32 GetIA88_RGBA4444( void *src, u16 x, u16 i, u8 palette )
{
   return IA88_RGBA4444(((u16*)src)[x^i]);
}

u32 GetIA88_IA88( void *src, u16 x, u16 i, u8 palette )
{
   return IA88_IA88(((u16*)src)[x^i]);
}

u32 GetRGBA8888_RGBA8888( void *src, u16 x, u16 i, u8 palette )
{
   return ((u32*)src)[x^i];
}

u32 GetRGBA8888_RGBA4444( void *src, u16 x, u16 i, u8 palette )
{
   return RGBA8888_RGBA4444(((u32*)src)[x^i]);
}


typedef struct
{
    int format;
    GetTexelFunc getTexel;
    int lineShift, maxTexels;
} TextureFormat;


TextureFormat textureFormatIA[4*6] =
{
    // 4-bit
    {   FORMAT_RGBA5551,    GetCI4RGBA_RGBA5551,    4,  4096 }, // RGBA (SELECT)
    {   FORMAT_NONE,        GetNone,                4,  8192 }, // YUV
    {   FORMAT_RGBA5551,    GetCI4RGBA_RGBA5551,    4,  4096 }, // CI
    {   FORMAT_IA88,        GetIA31_IA88,           4,  8192 }, // IA
    {   FORMAT_IA88,        GetI4_IA88,             4,  8192 }, // I
    {   FORMAT_RGBA8888,    GetCI4IA_RGBA8888,      4,  4096 }, // IA Palette
    // 8-bit
    {   FORMAT_RGBA5551,    GetCI8RGBA_RGBA5551,    3,  2048 }, // RGBA (SELECT)
    {   FORMAT_NONE,        GetNone,                3,  4096 }, // YUV
    {   FORMAT_RGBA5551,    GetCI8RGBA_RGBA5551,    3,  2048 }, // CI
    {   FORMAT_IA88,        GetIA44_IA88,           3,  4096 }, // IA
    {   FORMAT_IA88,        GetI8_IA88,             3,  4096 }, // I
    {   FORMAT_RGBA8888,    GetCI8IA_RGBA8888,      3,  2048 }, // IA Palette
    // 16-bit
    {   FORMAT_RGBA5551,    GetRGBA5551_RGBA5551,   2,  2048 }, // RGBA
    {   FORMAT_NONE,        GetNone,                2,  2048 }, // YUV
    {   FORMAT_NONE,        GetNone,                2,  2048 }, // CI
    {   FORMAT_IA88,        GetIA88_IA88,           2,  2048 }, // IA
    {   FORMAT_NONE,        GetNone,                2,  2048 }, // I
    {   FORMAT_NONE,        GetNone,                2,  2048 }, // IA Palette
    // 32-bit
    {   FORMAT_RGBA8888,    GetRGBA8888_RGBA8888,   2,  1024 }, // RGBA
    {   FORMAT_NONE,        GetNone,                2,  1024 }, // YUV
    {   FORMAT_NONE,        GetNone,                2,  1024 }, // CI
    {   FORMAT_NONE,        GetNone,                2,  1024 }, // IA
    {   FORMAT_NONE,        GetNone,                2,  1024 }, // I
    {   FORMAT_NONE,        GetNone,                2,  1024 }, // IA Palette
};

TextureFormat textureFormatRGBA[4*6] =
{
    // 4-bit
    {   FORMAT_RGBA5551,    GetCI4RGBA_RGBA5551,    4,  4096 }, // RGBA (SELECT)
    {   FORMAT_NONE,        GetNone,                4,  8192 }, // YUV
    {   FORMAT_RGBA5551,    GetCI4RGBA_RGBA5551,    4,  4096 }, // CI
    {   FORMAT_RGBA4444,    GetIA31_RGBA4444,       4,  8192 }, // IA
    {   FORMAT_RGBA4444,    GetI4_RGBA4444,         4,  8192 }, // I
    {   FORMAT_RGBA8888,    GetCI4IA_RGBA8888,      4,  4096 }, // IA Palette
    // 8-bit
    {   FORMAT_RGBA5551,    GetCI8RGBA_RGBA5551,    3,  2048 }, // RGBA (SELECT)
    {   FORMAT_NONE,        GetNone,                3,  4096 }, // YUV
    {   FORMAT_RGBA5551,    GetCI8RGBA_RGBA5551,    3,  2048 }, // CI
    {   FORMAT_RGBA4444,    GetIA44_RGBA4444,       3,  4096 }, // IA
    {   FORMAT_RGBA8888,    GetI8_RGBA8888,         3,  4096 }, // I
    {   FORMAT_RGBA8888,    GetCI8IA_RGBA8888,      3,  2048 }, // IA Palette
    // 16-bit
    {   FORMAT_RGBA5551,    GetRGBA5551_RGBA5551,   2,  2048 }, // RGBA
    {   FORMAT_NONE,        GetNone,                2,  2048 }, // YUV
    {   FORMAT_NONE,        GetNone,                2,  2048 }, // CI
    {   FORMAT_RGBA8888,    GetIA88_RGBA8888,       2,  2048 }, // IA
    {   FORMAT_NONE,        GetNone,                2,  2048 }, // I
    {   FORMAT_NONE,        GetNone,                2,  2048 }, // IA Palette
    // 32-bit
    {   FORMAT_RGBA8888,    GetRGBA8888_RGBA8888,   2,  1024 }, // RGBA
    {   FORMAT_NONE,        GetNone,                2,  1024 }, // YUV
    {   FORMAT_NONE,        GetNone,                2,  1024 }, // CI
    {   FORMAT_NONE,        GetNone,                2,  1024 }, // IA
    {   FORMAT_NONE,        GetNone,                2,  1024 }, // I
    {   FORMAT_NONE,        GetNone,                2,  1024 }, // IA Palette
};


TextureFormat *textureFormat = textureFormatIA;

void __texture_format_rgba(int size, int format, TextureFormat *texFormat)
{
   if (size < G_IM_SIZ_16b)
   {
      if (gDP.otherMode.textureLUT == G_TT_NONE)
         *texFormat = textureFormat[size*6 + G_IM_FMT_I];
      else if (gDP.otherMode.textureLUT == G_TT_RGBA16)
         *texFormat = textureFormat[size*6 + G_IM_FMT_CI];
      else
         *texFormat = textureFormat[size*6 + G_IM_FMT_IA];
   }
   else
      *texFormat = textureFormat[size*6 + G_IM_FMT_RGBA];
}

void __texture_format_ci(int size, int format, TextureFormat *texFormat)
{
   switch(size)
   {
      case G_IM_SIZ_4b:
         if (gDP.otherMode.textureLUT == G_TT_IA16)
            *texFormat = textureFormat[G_IM_SIZ_4b*6 + G_IM_FMT_CI_IA];
         else
            *texFormat = textureFormat[G_IM_SIZ_4b*6 + G_IM_FMT_CI];
         break;

      case G_IM_SIZ_8b:
         if (gDP.otherMode.textureLUT == G_TT_NONE)
            *texFormat = textureFormat[G_IM_SIZ_8b*6 + G_IM_FMT_I];
         else if (gDP.otherMode.textureLUT == G_TT_IA16)
            *texFormat = textureFormat[G_IM_SIZ_8b*6 + G_IM_FMT_CI_IA];
         else
            *texFormat = textureFormat[G_IM_SIZ_8b*6 + G_IM_FMT_CI];
         break;

      default:
         *texFormat = textureFormat[size*6 + format];
   }
}

void __texture_format(int size, int format, TextureFormat *texFormat)
{
   if (format == G_IM_FMT_RGBA)
      __texture_format_rgba(size, format, texFormat);
   else if (format == G_IM_FMT_YUV)
      *texFormat = textureFormat[size*6 + G_IM_FMT_YUV];
   else if (format == G_IM_FMT_CI)
      __texture_format_ci(size, format, texFormat);
   else if (format == G_IM_FMT_IA)
   {
      if (gDP.otherMode.textureLUT != G_TT_NONE)
         __texture_format_ci(size, format, texFormat);
      else
         *texFormat = textureFormat[size*6 + G_IM_FMT_IA];
   }
   else if (format == G_IM_FMT_I)
   {
      if (gDP.otherMode.textureLUT == G_TT_NONE)
         *texFormat = textureFormat[size*6 + G_IM_FMT_I];
      else
         __texture_format_ci(size, format, texFormat);
   }
}

int isTexCacheInit = 0;

void TextureCache_Init(void)
{
   int x, y, i;
   u8 noise[64*64*2];
   u32 dummyTexture[16] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

   isTexCacheInit = 1;
   cache.current[0] = NULL;
   cache.current[1] = NULL;
   cache.top = NULL;
   cache.bottom = NULL;
   cache.numCached = 0;
   cache.cachedBytes = 0;

   if (config.texture.useIA) textureFormat = textureFormatIA;
   else textureFormat = textureFormatRGBA;

   glPixelStorei(GL_PACK_ALIGNMENT, 1);
   glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
   glGenTextures( 32, cache.glNoiseNames );

   srand((unsigned)time(NULL));
   for (i = 0; i < 32; i++)
   {
      glBindTexture( GL_TEXTURE_2D, cache.glNoiseNames[i] );
      for (y = 0; y < 64; y++)
      {
         for (x = 0; x < 64; x++)
         {
            u32 r = rand() & 0xFF;
            noise[y*64*2+x*2] = r;
            noise[y*64*2+x*2+1] = r;
         }
      }
      glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE_ALPHA, 64, 64, 0, GL_LUMINANCE_ALPHA, GL_UNSIGNED_BYTE, noise);
   }

   cache.dummy = TextureCache_AddTop();
   cache.dummy->address = 0;
   cache.dummy->clampS = 1;
   cache.dummy->clampT = 1;
   cache.dummy->clampWidth = 2;
   cache.dummy->clampHeight = 2;
   cache.dummy->crc = 0;
   cache.dummy->format = 0;
   cache.dummy->size = 0;
   cache.dummy->width = 2;
   cache.dummy->height = 2;
   cache.dummy->realWidth = 2;
   cache.dummy->realHeight = 2;
   cache.dummy->maskS = 0;
   cache.dummy->maskT = 0;
   cache.dummy->scaleS = 0.5f;
   cache.dummy->scaleT = 0.5f;
   cache.dummy->shiftScaleS = 1.0f;
   cache.dummy->shiftScaleT = 1.0f;
   cache.dummy->textureBytes = 2 * 2 * 4;
   cache.dummy->tMem = 0;

   glBindTexture( GL_TEXTURE_2D, cache.dummy->glName );
   glTexImage2D( GL_TEXTURE_2D, 0, GL_RGBA, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, dummyTexture);

   cache.cachedBytes = cache.dummy->textureBytes;
   TextureCache_ActivateDummy(0);
   TextureCache_ActivateDummy(1);
   CRC_BuildTable();
}

bool TextureCache_Verify(void)
{
   u16 i;
   CachedTexture *current;

   i = 0;
   current = cache.top;

   while (current)
   {
      i++;
      current = current->lower;
   }

   if (i != cache.numCached)
      return false;

   i = 0;
   current = cache.bottom;
   while (current)
   {
      i++;
      current = current->higher;
   }

   if (i != cache.numCached)
      return false;

   return true;
}

void TextureCache_RemoveBottom(void)
{
   CachedTexture *newBottom = cache.bottom->higher;

   glDeleteTextures( 1, &cache.bottom->glName );
   cache.cachedBytes -= cache.bottom->textureBytes;

   if (cache.bottom == cache.top)
      cache.top = NULL;

   free( cache.bottom );

   cache.bottom = newBottom;

   if (cache.bottom)
      cache.bottom->lower = NULL;

   cache.numCached--;
}

void TextureCache_Remove( CachedTexture *texture )
{
   if ((texture == cache.bottom) && (texture == cache.top))
   {
      cache.top = NULL;
      cache.bottom = NULL;
   }
   else if (texture == cache.bottom)
   {
      cache.bottom = texture->higher;

      if (cache.bottom)
         cache.bottom->lower = NULL;
   }
   else if (texture == cache.top)
   {
      cache.top = texture->lower;

      if (cache.top)
         cache.top->higher = NULL;
   }
   else
   {
      texture->higher->lower = texture->lower;
      texture->lower->higher = texture->higher;
   }

   glDeleteTextures( 1, &texture->glName );
   cache.cachedBytes -= texture->textureBytes;
   free( texture );

   cache.numCached--;
}

CachedTexture *TextureCache_AddTop(void)
{
   CachedTexture *newtop;
   while (cache.cachedBytes > TEXTURECACHE_MAX)
   {
      if (cache.bottom != cache.dummy)
         TextureCache_RemoveBottom();
      else if (cache.dummy->higher)
         TextureCache_Remove( cache.dummy->higher );
   }

   newtop = (CachedTexture*)malloc( sizeof( CachedTexture ) );

   glGenTextures( 1, &newtop->glName );

   newtop->lower = cache.top;
   newtop->higher = NULL;

   if (cache.top)
      cache.top->higher = newtop;

   if (!cache.bottom)
      cache.bottom = newtop;

   cache.top = newtop;

   cache.numCached++;

   return newtop;
}

void TextureCache_MoveToTop( CachedTexture *newtop )
{
   if (newtop == cache.top)
      return;

   if (newtop == cache.bottom)
   {
      cache.bottom = newtop->higher;
      cache.bottom->lower = NULL;
   }
   else
   {
      newtop->higher->lower = newtop->lower;
      newtop->lower->higher = newtop->higher;
   }

   newtop->higher = NULL;
   newtop->lower = cache.top;
   cache.top->higher = newtop;
   cache.top = newtop;
}

void TextureCache_Destroy(void)
{
   while (cache.bottom)
      TextureCache_RemoveBottom();

   glDeleteTextures( 32, cache.glNoiseNames );
   glDeleteTextures( 1, &cache.dummy->glName  );

   cache.top = NULL;
   cache.bottom = NULL;
}

void TextureCache_LoadBackground( CachedTexture *texInfo )
{
   u32 *dest;
   u8 *swapped, *src;
   u32 numBytes, bpl;
   u32 x, y, j, tx, ty;
   u16 clampSClamp,  clampTClamp;

   int bytePerPixel=0;
   TextureFormat   texFormat;
   GetTexelFunc    getTexel;
   GLint glWidth=0, glHeight=0;
   GLenum glType=0;
   GLenum glFormat=0;

   __texture_format(texInfo->size, texInfo->format, &texFormat);

   if (texFormat.format == FORMAT_NONE)
   {
      LOG(LOG_WARNING, "No Texture Conversion function available, size=%i format=%i\n", texInfo->size, texInfo->format);
   }

   switch(texFormat.format)
   {
      case FORMAT_I8:
         glFormat = GL_LUMINANCE;
         glType = GL_UNSIGNED_BYTE;
         bytePerPixel = 1;
         break;
      case FORMAT_IA88:
         glFormat = GL_LUMINANCE_ALPHA;
         glType = GL_UNSIGNED_BYTE;
         bytePerPixel = 2;
         break;
      case FORMAT_RGBA4444:
         glFormat = GL_RGBA;
         glType = GL_UNSIGNED_SHORT_4_4_4_4;
         bytePerPixel = 2;
         break;
      case FORMAT_RGBA5551:
         glFormat = GL_RGBA;
         glType = GL_UNSIGNED_SHORT_5_5_5_1;
         bytePerPixel = 2;
         break;
      case FORMAT_RGBA8888:
         glFormat = GL_RGBA;
         glType = GL_UNSIGNED_BYTE;
         bytePerPixel = 4;
         break;
   }

   glWidth = texInfo->realWidth;
   glHeight = texInfo->realHeight;
   texInfo->textureBytes = (glWidth * glHeight) * bytePerPixel;
   getTexel = texFormat.getTexel;

   bpl = gSP.bgImage.width << gSP.bgImage.size >> 1;
   numBytes = bpl * gSP.bgImage.height;
   swapped = (u8*) malloc(numBytes);
   dest = (u32*) malloc(texInfo->textureBytes);

   if (!dest || !swapped)
   {
      LOG(LOG_ERROR, "TextureCache_LoadBackground - malloc failed!\n");
      return;
   }

   UnswapCopy(&gfx_info.RDRAM[gSP.bgImage.address], swapped, numBytes);

   clampSClamp = texInfo->width - 1;
   clampTClamp = texInfo->height - 1;

   j = 0;
   for (y = 0; y < texInfo->realHeight; y++)
   {
      ty = min(y, clampTClamp);
      src = &swapped[bpl * ty];
      for (x = 0; x < texInfo->realWidth; x++)
      {
         tx = min(x, clampSClamp);
         if (bytePerPixel == 4)
            ((u32*)dest)[j++] = getTexel(src, tx, 0, texInfo->palette);
         else if (bytePerPixel == 2)
            ((u16*)dest)[j++] = getTexel(src, tx, 0, texInfo->palette);
         else if (bytePerPixel == 1)
            ((u8*)dest)[j++] = getTexel(src, tx, 0, texInfo->palette);
      }
   }

   glTexImage2D( GL_TEXTURE_2D, 0, glFormat, glWidth, glHeight, 0, glFormat, glType, dest);

   free(dest);
   free(swapped);


   if (config.texture.enableMipmap)
      glGenerateMipmap(GL_TEXTURE_2D);
}

void TextureCache_Load(int _tile, CachedTexture *texInfo )
{
   u32 *dest;

   void *src;
   u16 x, y, i, j, tx, ty, line;
   u16 mirrorSBit, maskSMask, clampSClamp;
   u16 mirrorTBit, maskTMask, clampTClamp;

	GLint mipLevel = 0, maxLevel = 0;
   int bytePerPixel=0;
   TextureFormat   texFormat;
   GetTexelFunc    getTexel;
   GLint glWidth=0, glHeight=0;
   GLenum glType=0;
   GLenum glFormat=0;

   __texture_format(texInfo->size, texInfo->format, &texFormat);

   if (texFormat.format == FORMAT_NONE)
   {
      LOG(LOG_WARNING, "No Texture Conversion function available, size=%i format=%i\n", texInfo->size, texInfo->format);
   }

   switch(texFormat.format)
   {
      case FORMAT_I8:
         glFormat = GL_LUMINANCE;
         glType = GL_UNSIGNED_BYTE;
         bytePerPixel = 1;
         break;
      case FORMAT_IA88:
         glFormat = GL_LUMINANCE_ALPHA;
         glType = GL_UNSIGNED_BYTE;
         bytePerPixel = 2;
         break;
      case FORMAT_RGBA4444:
         glFormat = GL_RGBA;
         glType = GL_UNSIGNED_SHORT_4_4_4_4;
         bytePerPixel = 2;
         break;
      case FORMAT_RGBA5551:
         glFormat = GL_RGBA;
         glType = GL_UNSIGNED_SHORT_5_5_5_1;
         bytePerPixel = 2;
         break;
      case FORMAT_RGBA8888:
         glFormat = GL_RGBA;
         glType = GL_UNSIGNED_BYTE;
         bytePerPixel = 4;
         break;
   }

   glWidth = texInfo->realWidth;
   glHeight = texInfo->realHeight;
   texInfo->textureBytes = (glWidth * glHeight) * bytePerPixel;
   getTexel = texFormat.getTexel;

   dest = (u32*)malloc(texInfo->textureBytes);

   if (!dest)
   {
      LOG(LOG_ERROR, "TextureCache_Load - malloc failed!\n");
      return;
   }

	if (/*config.generalEmulation.enableLOD != 0 &&*/gSP.texture.level > gSP.texture.tile + 1)
		maxLevel = _tile == 0 ? 0 : gSP.texture.level - gSP.texture.tile - 1;

	texInfo->max_level = maxLevel;

   line = texInfo->line;

   if (texInfo->size == G_IM_SIZ_32b)
      line <<= 1;

   {
      if (texInfo->maskS)
      {
         clampSClamp = texInfo->clampS ? texInfo->clampWidth - 1 : (texInfo->mirrorS ? (texInfo->width << 1) - 1 : texInfo->width - 1);
         maskSMask = (1 << texInfo->maskS) - 1;
         mirrorSBit = texInfo->mirrorS ? (1 << texInfo->maskS) : 0;
      }
      else
      {
         clampSClamp = min( texInfo->clampWidth, texInfo->width ) - 1;
         maskSMask = 0xFFFF;
         mirrorSBit = 0x0000;
      }

      if (texInfo->maskT)
      {
         clampTClamp = texInfo->clampT ? texInfo->clampHeight - 1 : (texInfo->mirrorT ? (texInfo->height << 1) - 1: texInfo->height - 1);
         maskTMask = (1 << texInfo->maskT) - 1;
         mirrorTBit = texInfo->mirrorT ? (1 << texInfo->maskT) : 0;
      }
      else
      {
         clampTClamp = min( texInfo->clampHeight, texInfo->height ) - 1;
         maskTMask = 0xFFFF;
         mirrorTBit = 0x0000;
      }

      // Hack for Zelda warp texture
      if (((texInfo->tMem << 3) + (texInfo->width * texInfo->height << texInfo->size >> 1)) > 4096)
         texInfo->tMem = 0;

      // limit clamp values to min-0 (Perfect Dark has height=0 textures, making negative clamps)
      if (clampTClamp & 0x8000)
         clampTClamp = 0;
      if (clampSClamp & 0x8000)
         clampSClamp = 0;

      j = 0;
      for (y = 0; y < texInfo->realHeight; y++)
      {
         ty = min(y, clampTClamp) & maskTMask;

         if (y & mirrorTBit)
            ty ^= maskTMask;

         src = &TMEM[(texInfo->tMem + line * ty) & 511];

         i = (ty & 1) << 1;
         for (x = 0; x < texInfo->realWidth; x++)
         {
            tx = min(x, clampSClamp) & maskSMask;

            if (x & mirrorSBit)
               tx ^= maskSMask;

            if (bytePerPixel == 4)
               ((u32*)dest)[j] = getTexel(src, tx, i, texInfo->palette);
            else if (bytePerPixel == 2)
               ((u16*)dest)[j] = getTexel(src, tx, i, texInfo->palette);
            else if (bytePerPixel == 1)
               ((u8*)dest)[j] = getTexel(src, tx, i, texInfo->palette);
            j++;
         }
      }

      glTexImage2D( GL_TEXTURE_2D, 0, glFormat, glWidth, glHeight, 0, glFormat, glType, dest);

      if (config.texture.enableMipmap)
         glGenerateMipmap(GL_TEXTURE_2D);
   }
   free(dest);
}

u32 TextureCache_CalculateCRC( u32 t, u32 width, u32 height )
{
   u32 crc;
   u32 y, /*i,*/ bpl, lineBytes, line;
   unsigned n;
   void *src;

   bpl = width << gSP.textureTile[t]->size >> 1;
   lineBytes = gSP.textureTile[t]->line << 3;

   line = gSP.textureTile[t]->line;
   if (gSP.textureTile[t]->size == G_IM_SIZ_32b)
      line <<= 1;

   crc = 0xFFFFFFFF;

   n = 1;

   for (y = 0; y < height; y += n)
   {
      src = (void*) &TMEM[(gSP.textureTile[t]->tmem + (y * line)) & 511];
      crc = Hash_Calculate( crc, src, bpl );
   }

   if (gDP.otherMode.textureLUT != G_TT_NONE || gSP.textureTile[t]->format == G_IM_FMT_CI)
   {
      if (gSP.textureTile[t]->size == G_IM_SIZ_4b)
         crc = Hash_Calculate( crc, &gDP.paletteCRC16[gSP.textureTile[t]->palette], 4 );
      else if (gSP.textureTile[t]->size == G_IM_SIZ_8b)
         crc = Hash_Calculate( crc, &gDP.paletteCRC256, 4 );
   }
   return crc;
}

void TextureCache_ActivateTexture( u32 t, CachedTexture *texture )
{
   glActiveTexture( GL_TEXTURE0 + t );
   glBindTexture( GL_TEXTURE_2D, texture->glName );

   // Set filter mode. Almost always bilinear, but check anyways
   if ((gDP.otherMode.textureFilter == G_TF_BILERP) || (gDP.otherMode.textureFilter == G_TF_AVERAGE))
   {
      glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR );
      glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR );
   }
   else
   {
      glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST );
      glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST );
   }

   // Set clamping modes
   glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (texture->clampS) ? GL_CLAMP_TO_EDGE : GL_REPEAT );
   glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (texture->clampT) ? GL_CLAMP_TO_EDGE : GL_REPEAT );

   if (config.texture.maxAnisotropy > 0)
   {
      glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, config.texture.maxAnisotropy);
   }

   texture->lastDList = __RSP.DList;
   TextureCache_MoveToTop( texture );
   cache.current[t] = texture;
}

void TextureCache_ActivateDummy( u32 t)
{
   glActiveTexture(GL_TEXTURE0 + t);
   glBindTexture(GL_TEXTURE_2D, cache.dummy->glName );
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

int _background_compare(CachedTexture *current, u32 crc)
{
   if ((current != NULL) &&
         (current->crc == crc) &&
         (current->width == gSP.bgImage.width) &&
         (current->height == gSP.bgImage.height) &&
         (current->format == gSP.bgImage.format) &&
         (current->size == gSP.bgImage.size))
      return 1;
   else
      return 0;
}

void TextureCache_UpdateBackground(void)
{
   u32 numBytes, crc;
   CachedTexture *current;
   CachedTexture *pCurrent;

   numBytes = gSP.bgImage.width * gSP.bgImage.height << gSP.bgImage.size >> 1;
   crc = Hash_Calculate( 0xFFFFFFFF, &gfx_info.RDRAM[gSP.bgImage.address], numBytes );

   if (gDP.otherMode.textureLUT != G_TT_NONE || gSP.bgImage.format == G_IM_FMT_CI)
   {
      if (gSP.bgImage.size == G_IM_SIZ_4b)
         crc = Hash_Calculate( crc, &gDP.paletteCRC16[gSP.bgImage.palette], 4 );
      else if (gSP.bgImage.size == G_IM_SIZ_8b)
         crc = Hash_Calculate( crc, &gDP.paletteCRC256, 4 );
   }

   //before we traverse cache, check to see if texture is already bound:
   if (_background_compare(cache.current[0], crc))
      return;

   current = (CachedTexture*)cache.top;
   while (current)
   {
      if (_background_compare(current, crc))
      {
         TextureCache_ActivateTexture( 0, current );
         cache.hits++;
         return;
      }
      current = current->lower;
   }
   cache.misses++;

   glActiveTexture(GL_TEXTURE0);

   pCurrent = TextureCache_AddTop();

   glBindTexture( GL_TEXTURE_2D, pCurrent->glName );

   pCurrent->address = gSP.bgImage.address;
   pCurrent->crc = crc;

   pCurrent->format = gSP.bgImage.format;
   pCurrent->size = gSP.bgImage.size;

   pCurrent->width = gSP.bgImage.width;
   pCurrent->height = gSP.bgImage.height;

   pCurrent->clampWidth = gSP.bgImage.width;
   pCurrent->clampHeight = gSP.bgImage.height;
   pCurrent->palette = gSP.bgImage.palette;
   pCurrent->maskS = 0;
   pCurrent->maskT = 0;
   pCurrent->mirrorS = 0;
   pCurrent->mirrorT = 0;
   pCurrent->clampS = 0;
   pCurrent->clampT = 0;
   pCurrent->line = 0;
   pCurrent->tMem = 0;
   pCurrent->lastDList = __RSP.DList;

   pCurrent->realWidth = gSP.bgImage.width;
   pCurrent->realHeight = gSP.bgImage.height;

   pCurrent->scaleS = 1.0f / (f32)(pCurrent->realWidth);
   pCurrent->scaleT = 1.0f / (f32)(pCurrent->realHeight);

   pCurrent->shiftScaleS = 1.0f;
   pCurrent->shiftScaleT = 1.0f;

   pCurrent->offsetS = 0.5f;
   pCurrent->offsetT = 0.5f;

   TextureCache_LoadBackground( pCurrent );
   TextureCache_ActivateTexture( 0, pCurrent );

   cache.cachedBytes += pCurrent->textureBytes;
   cache.current[0] = pCurrent;
}

int _texture_compare(u32 t, CachedTexture *current, u32 crc,  u32 width, u32 height, u32 clampWidth, u32 clampHeight)
{
   return  ((current != NULL) &&
         (current->crc == crc) &&
         (current->width == width) &&
         (current->height == height) &&
         (current->clampWidth == clampWidth) &&
         (current->clampHeight == clampHeight) &&
         (current->maskS == gSP.textureTile[t]->masks) &&
         (current->maskT == gSP.textureTile[t]->maskt) &&
         (current->mirrorS == gSP.textureTile[t]->mirrors) &&
         (current->mirrorT == gSP.textureTile[t]->mirrort) &&
         (current->clampS == gSP.textureTile[t]->clamps) &&
         (current->clampT == gSP.textureTile[t]->clampt) &&
         (current->format == gSP.textureTile[t]->format) &&
         (current->size == gSP.textureTile[t]->size));
}

static
void _updateShiftScale(u32 _t, CachedTexture *_pTexture)
{
	_pTexture->shiftScaleS = 1.0f;
	_pTexture->shiftScaleT = 1.0f;

	if (gSP.textureTile[_t]->shifts > 10)
		_pTexture->shiftScaleS = (f32)(1 << (16 - gSP.textureTile[_t]->shifts));
	else if (gSP.textureTile[_t]->shifts > 0)
		_pTexture->shiftScaleS /= (f32)(1 << gSP.textureTile[_t]->shifts);

	if (gSP.textureTile[_t]->shiftt > 10)
		_pTexture->shiftScaleT = (f32)(1 << (16 - gSP.textureTile[_t]->shiftt));
	else if (gSP.textureTile[_t]->shiftt > 0)
		_pTexture->shiftScaleT /= (f32)(1 << gSP.textureTile[_t]->shiftt);
}

void TextureCache_Update( u32 t )
{
   CachedTexture *current;
   CachedTexture *pCurrent;
   u32 crc, maxTexels;
   u32 tileWidth, maskWidth, loadWidth, lineWidth, clampWidth, height;
   u32 tileHeight, maskHeight, loadHeight, lineHeight, clampHeight, width;
   u32 maskSize;
   TextureFormat texFormat;

   switch (gSP.textureTile[t]->textureMode)
   {
      case TEXTUREMODE_BGIMAGE:
         TextureCache_UpdateBackground();
         return;
#if 0
	case TEXTUREMODE_FRAMEBUFFER:
		FrameBuffer_ActivateBufferTexture( _t, gSP.textureTile[_t]->frameBuffer );
		return;
	case TEXTUREMODE_FRAMEBUFFER_BG:
		FrameBuffer_ActivateBufferTextureBG( _t, gSP.textureTile[_t]->frameBuffer );
		return;
#endif
   }

   __texture_format(gSP.textureTile[t]->size, gSP.textureTile[t]->format, &texFormat);
   maxTexels = texFormat.maxTexels;
   // Here comes a bunch of code that just calculates the texture size...I wish there was an easier way...
   tileWidth = gSP.textureTile[t]->lrs - gSP.textureTile[t]->uls + 1;
   tileHeight = gSP.textureTile[t]->lrt - gSP.textureTile[t]->ult + 1;
   maskWidth = 1 << gSP.textureTile[t]->masks;
   maskHeight = 1 << gSP.textureTile[t]->maskt;
   maskSize = 1 << (gSP.textureTile[t]->masks + gSP.textureTile[t]->maskt);
   loadWidth = gDP.loadTile->lrs - gDP.loadTile->uls + 1;
   loadHeight = gDP.loadTile->lrt - gDP.loadTile->ult + 1;
   lineWidth = gSP.textureTile[t]->line << texFormat.lineShift;
   if (lineWidth) // Don't allow division by zero
      lineHeight = min( maxTexels / lineWidth, tileHeight );
   else
      lineHeight = 0;
   if (likely(gSP.textureTile[t]->masks && (maskSize <= maxTexels))) {
      width = maskWidth;
   } else if (likely((tileWidth * tileHeight) <= maxTexels)) {
      width = tileWidth;
   } else if (likely(gDP.tiles[0].textureMode != TEXTUREMODE_TEXRECT)) {
      if (gDP.loadTile->loadType == LOADTYPE_TILE)
         width = loadWidth;
      else
         width = lineWidth;
   } else {
      u32 texRectWidth = gDP.texRect.width - gSP.textureTile[t]->uls;
      u32 texRectHeight = gDP.texRect.height - gSP.textureTile[t]->ult;
      if ((tileWidth * texRectHeight) <= maxTexels)
         width = tileWidth;
      else if ((texRectWidth * tileHeight) <= maxTexels)
         width = gDP.texRect.width;
      else if ((texRectWidth * texRectHeight) <= maxTexels)
         width = gDP.texRect.width;
      else if (gDP.loadTile->loadType == LOADTYPE_TILE)
         width = loadWidth;
      else
         width = lineWidth;
   }
   if (likely(gSP.textureTile[t]->maskt && (maskSize <= maxTexels))) {
      height = maskHeight;
   } else if (likely((tileWidth * tileHeight) <= maxTexels)) {
      height = tileHeight;
   } else if (likely(gDP.tiles[0].textureMode != TEXTUREMODE_TEXRECT)) {
      if (gDP.loadTile->loadType == LOADTYPE_TILE)
         height = loadHeight;
      else
         height = lineHeight;
   } else {
      u32 texRectWidth = gDP.texRect.width - gSP.textureTile[t]->uls;
      u32 texRectHeight = gDP.texRect.height - gSP.textureTile[t]->ult;
      if ((tileWidth * texRectHeight) <= maxTexels)
         height = gDP.texRect.height;
      else if ((texRectWidth * tileHeight) <= maxTexels)
         height = tileHeight;
      else if ((texRectWidth * texRectHeight) <= maxTexels)
         height = gDP.texRect.height;
      else if (gDP.loadTile->loadType == LOADTYPE_TILE)
         height = loadHeight;
      else
         height = lineHeight;
   }
   clampWidth = gSP.textureTile[t]->clamps ? tileWidth : width;
   clampHeight = gSP.textureTile[t]->clampt ? tileHeight : height;
   if (clampWidth > 256)
      gSP.textureTile[t]->clamps = 0;
   if (clampHeight > 256)
      gSP.textureTile[t]->clampt = 0;
   // Make sure masking is valid
   if (maskWidth > width)
   {
      gSP.textureTile[t]->masks = powof( width );
      maskWidth = 1 << gSP.textureTile[t]->masks;
   }
   if (maskHeight > height)
   {
      gSP.textureTile[t]->maskt = powof( height );
      maskHeight = 1 << gSP.textureTile[t]->maskt;
   }
   crc = TextureCache_CalculateCRC( t, width, height );

   //before we traverse cache, check to see if texture is already bound:
   if (_texture_compare(t, cache.current[t], crc, width, height, clampWidth, clampHeight))
   {
      _updateShiftScale(t, cache.current[t]);
      cache.hits++;
      return;
   }

   current = cache.top;
   while (current)
   {
      if (_texture_compare(t, current, crc, width, height, clampWidth, clampHeight))
      {
         TextureCache_ActivateTexture( t, current );
         cache.hits++;
         return;
      }
      current = current->lower;
   }
   cache.misses++;
   glActiveTexture( GL_TEXTURE0 + t);

   pCurrent = TextureCache_AddTop();

   glBindTexture( GL_TEXTURE_2D, pCurrent->glName );

   pCurrent->address = gDP.textureImage.address;

   pCurrent->crc = crc;

   pCurrent->format = gSP.textureTile[t]->format;
   pCurrent->size   = gSP.textureTile[t]->size;

   pCurrent->width = width;
   pCurrent->height = height;

   pCurrent->clampWidth = clampWidth;
   pCurrent->clampHeight = clampHeight;

   pCurrent->palette = gSP.textureTile[t]->palette;

   pCurrent->maskS = gSP.textureTile[t]->masks;
   pCurrent->maskT = gSP.textureTile[t]->maskt;
   pCurrent->mirrorS = gSP.textureTile[t]->mirrors;
   pCurrent->mirrorT = gSP.textureTile[t]->mirrort;
   pCurrent->clampS = gSP.textureTile[t]->clamps;
   pCurrent->clampT = gSP.textureTile[t]->clampt;
   pCurrent->line = gSP.textureTile[t]->line;
   pCurrent->tMem = gSP.textureTile[t]->tmem;
   pCurrent->lastDList = __RSP.DList;

   if (pCurrent->clampS)
      pCurrent->realWidth = clampWidth;
   else if (pCurrent->mirrorS)
      pCurrent->realWidth = maskWidth << 1;
   else
      pCurrent->realWidth = width;
   
   if (pCurrent->clampT)
      pCurrent->realHeight = clampHeight;
   else if (pCurrent->mirrorT)
      pCurrent->realHeight = maskHeight << 1;
   else
      pCurrent->realHeight = height;

   pCurrent->scaleS = 1.0f / (f32)(pCurrent->realWidth);
   pCurrent->scaleT = 1.0f / (f32)(pCurrent->realHeight);

   pCurrent->offsetS = 0.5f;
   pCurrent->offsetT = 0.5f;

   pCurrent->shiftScaleS = 1.0f;
   pCurrent->shiftScaleT = 1.0f;

	_updateShiftScale(t, pCurrent);

   TextureCache_Load(t, pCurrent );

   TextureCache_ActivateTexture( t, pCurrent );

   cache.cachedBytes += pCurrent->textureBytes;

   cache.current[t] = pCurrent;
}

void TextureCache_ActivateNoise(u32 t)
{
   glActiveTexture(GL_TEXTURE0 + t);
   glBindTexture(GL_TEXTURE_2D, cache.glNoiseNames[__RSP.DList & 0x1F]);
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT );
   glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT );
}
