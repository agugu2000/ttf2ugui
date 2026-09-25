/* -------------------------------------------------------------------------------- */
/* -- µGUI - Generic GUI module (C)Achim Döbler, 2015                            -- */
/* -------------------------------------------------------------------------------- */
// µGUI is a generic GUI module for embedded systems.
// This is a free software that is open for education, research and commercial
// developments under license policy of following terms.
//
//  Copyright (C) 2015, Achim Döbler, all rights reserved.
//  URL: http://www.embeddedlightning.com/
//
// * The µGUI module is a free software and there is NO WARRANTY.
// * No restriction on use. You can use, modify and redistribute it for
//   personal, non-profit or commercial products UNDER YOUR RESPONSIBILITY.
// * Redistributions of source code must retain the above copyright notice.
//
/* -------------------------------------------------------------------------------- */
#include "ugui.h"
#include <stdint.h>

/* Static functions */
static UG_RESULT _UG_WindowDrawTitle( UG_WINDOW* wnd );
static void _UG_WindowUpdate( UG_WINDOW* wnd );
static UG_RESULT _UG_WindowClear( UG_WINDOW* wnd );
static void _UG_FontSelect( UG_FONT *font);
static UG_S16 _UG_PutChar( UG_CHAR chr, UG_S16 x, UG_S16 y, UG_COLOR fc, UG_COLOR bc);
static UG_S16 _UG_PutGlyph( UG_GLYPH *g, UG_S16 x, UG_S16 y, UG_COLOR fc, UG_COLOR bc, UG_U8 trans );
#ifdef UGUI_USE_UTF8
static UG_U16 _UG_DecodeUTF8(char **str);
#endif

// static UG_U16 ptr_8to16(const UG_U8* p){
//   UG_U16 d = *p++;
//   return ((d<<8) | *p);
// }

static UG_U16 _ru16(const UG_U8 *p) {
   return (UG_U16)((p[0] << 8) | p[1]);
}

static UG_U32 _ru32(const UG_U8 *p) {
    return ((UG_U32)p[0] << 24) | ((UG_U32)p[1] << 16) | ((UG_U32)p[2] << 8) | (UG_U32)p[3];
}

static const UG_COLOR pal_window[] = {
    C_PAL_WINDOW
};

/* Pointer to the gui */
static UG_GUI* gui;

UG_S16 UG_Init( UG_GUI* g, UG_DEVICE *device )
{
   UG_U8 i;

   g->device = device;
#if defined(UGUI_USE_CONSOLE)
   g->console.x_start = 4;
   g->console.y_start = 4;
   g->console.x_end = g->device->x_dim - g->console.x_start-1;
   g->console.y_end = g->device->y_dim - g->console.x_start-1;
   g->console.x_pos = g->console.x_end;
   g->console.y_pos = g->console.y_end;
#endif
   g->char_h_space = 1;
   g->char_v_space = 1;
   g->transparent_font = 0;
   g->shadow_font = 0;
   g->font=NULL;
   g->currentFont.format = UG_FONT_FMT_OLD;
   g->currentFont.font_type = 0;
   g->currentFont.is_old_font = 0;
   g->currentFont.max_ink_w = 0;
   g->currentFont.max_ink_h = 0;
   g->currentFont.notdef_adv = 0;
   g->currentFont.ascender = 0;
   g->currentFont.descender = 0;
   g->currentFont.number_of_chars = 0;
   g->currentFont.total_size = 0;
   g->currentFont.codepoints = NULL;
   g->currentFont.metrics = NULL;
   g->currentFont.data_offsets = NULL;
   g->currentFont.new_data = NULL;
   g->currentFont.old_char_width = 0;
   g->currentFont.old_char_height = 0;
   g->currentFont.old_number_of_chars = 0;
   g->currentFont.old_number_of_offsets = 0;
   g->currentFont.old_bytes_per_char = 0;
   g->currentFont.old_widths_present = 0;
   g->currentFont.old_widths = NULL;
   g->currentFont.old_offsets = NULL;
   g->currentFont.old_range_flags = NULL;
   g->currentFont.old_data = NULL;
   g->currentFont.font = NULL;
   g->desktop_color = C_DESKTOP_COLOR;
   g->fore_color = C_WHITE;
   g->back_color = C_BLACK;
   g->next_window = NULL;
   g->active_window = NULL;
   g->last_window = NULL;

   /* Clear drivers */
   for(i=0;i<NUMBER_OF_DRIVERS;i++)
   {
      g->driver[i].driver = NULL;
      g->driver[i].state = 0;
   }

   gui = g;
   return 1;
}

UG_S16 UG_SelectGUI( UG_GUI* g )
{
   gui = g;
   return 1;
}

UG_GUI* UG_GetGUI( void )
{
   return gui;
}

UG_U16 UG_GetFontWidth( UG_FONT* font )
{
   const UG_U8 *p = (const UG_U8 *)font;
   if (p[0] & 0x80) return p[1];
   return (UG_U16)((p[2] << 8) | p[3]);
}

UG_U16 UG_GetFontHeight( UG_FONT* font )
{
   const UG_U8 *p = (const UG_U8 *)font;
   if (p[0] & 0x80) return p[2];
   return (UG_U16)((p[4] << 8) | p[5]);
}

/*
 * Sets the GUI font
 */
void UG_FontSelect( UG_FONT* font )
{
  gui->font = font;
}

void UG_FillScreen( UG_COLOR c )
{
   UG_FillFrame(0,0,gui->device->x_dim-1,gui->device->y_dim-1,c);
}

void UG_FillFrame( UG_S16 x1, UG_S16 y1, UG_S16 x2, UG_S16 y2, UG_COLOR c )
{
   UG_S16 n,m;

   if ( x2 < x1 )
     swap(x1,x2);
   if ( y2 < y1 )
     swap(y1,y2);

   /* Is hardware acceleration available? */
   if ( gui->driver[DRIVER_FILL_FRAME].state & DRIVER_ENABLED )
   {
      if( ((UG_RESULT(*)(UG_S16 x1, UG_S16 y1, UG_S16 x2, UG_S16 y2, UG_COLOR c))gui->driver[DRIVER_FILL_FRAME].driver)(x1,y1,x2,y2,c) == UG_RESULT_OK ) return;
   }

   for( m=y1; m<=y2; m++ )
   {
      for( n=x1; n<=x2; n++ )
      {
         gui->device->pset(n,m,c);
      }
   }
}

void UG_FillRoundFrame( UG_S16 x1, UG_S16 y1, UG_S16 x2, UG_S16 y2, UG_S16 r, UG_COLOR c )
{
   UG_S16  x,y,xd;

   if ( x2 < x1 )
     swap(x1,x2);
   if ( y2 < y1 )
     swap(y1,y2);

   if ( r<=0 ) return;

   xd = 3 - (r << 1);
   x = 0;
   y = r;

   UG_FillFrame(x1 + r, y1, x2 - r, y2, c);

   while ( x <= y )
   {
     if( y > 0 )
     {
        UG_DrawLine(x2 + x - r, y1 - y + r, x2+ x - r, y + y2 - r, c);
        UG_DrawLine(x1 - x + r, y1 - y + r, x1- x + r, y + y2 - r, c);
     }
     if( x > 0 )
     {
        UG_DrawLine(x1 - y + r, y1 - x + r, x1 - y + r, x + y2 - r, c);
        UG_DrawLine(x2 + y - r, y1 - x + r, x2 + y - r, x + y2 - r, c);
     }
     if ( xd < 0 )
     {
        xd += (x << 2) + 6;
     }
     else
     {
        xd += ((x - y) << 2) + 10;
        y--;
     }
     x++;
   }
}

void UG_DrawMesh( UG_S16 x1, UG_S16 y1, UG_S16 x2, UG_S16 y2, UG_U16 spacing, UG_COLOR c )
{
   UG_U16 p;

   if ( x2 < x1 )
     swap(x1,x2);
   if ( y2 < y1 )
     swap(y1,y2);

   for( p=y1; p<y2; p+=spacing )
   {
     UG_DrawLine(x1, p, x2, p, c);
   }
   UG_DrawLine(x1, y2, x2, y2, c);

   for( p=x1; p<x2; p+=spacing )
   {
     UG_DrawLine(p, y1, p, y2, c);
   }
   UG_DrawLine(x2, y1, x2, y2, c);
}

void UG_DrawFrame( UG_S16 x1, UG_S16 y1, UG_S16 x2, UG_S16 y2, UG_COLOR c )
{
   UG_DrawLine(x1,y1,x2,y1,c);
   UG_DrawLine(x1,y2,x2,y2,c);
   UG_DrawLine(x1,y1,x1,y2,c);
   UG_DrawLine(x2,y1,x2,y2,c);
}

void UG_DrawRoundFrame( UG_S16 x1, UG_S16 y1, UG_S16 x2, UG_S16 y2, UG_S16 r, UG_COLOR c )
{
   if(r == 0)
   {
      UG_DrawFrame(x1, y1, x2, y2, c);
      return;
   }

   if ( x2 < x1 )
     swap(x1,x2);
   if ( y2 < y1 )
     swap(y1,y2);
   if(r) r++;               // Fix for corner radius looking weird, this makes the same outline as UG_FillRoundFrame
   if ( r > x2 ) return;
   if ( r > y2 ) return;

   UG_DrawLine(x1+r, y1, x2-r, y1, c);
   UG_DrawLine(x1+r, y2, x2-r, y2, c);
   UG_DrawLine(x1, y1+r, x1, y2-r, c);
   UG_DrawLine(x2, y1+r, x2, y2-r, c);
   UG_DrawArc(x1+r, y1+r, r, 0x0C, c);
   UG_DrawArc(x2-r, y1+r, r, 0x03, c);
   UG_DrawArc(x1+r, y2-r, r, 0x30, c);
   UG_DrawArc(x2-r, y2-r, r, 0xC0, c);
}

void UG_DrawPixel( UG_S16 x0, UG_S16 y0, UG_COLOR c )
{
   gui->device->pset(x0,y0,c);
}

void UG_DrawCircle( UG_S16 x0, UG_S16 y0, UG_S16 r, UG_COLOR c )
{
   UG_S16 x,y,xd,yd,e;

   if ( x0<0 ) return;
   if ( y0<0 ) return;
   if ( r<=0 ) return;

   xd = 1 - (r << 1);
   yd = 0;
   e = 0;
   x = r;
   y = 0;

   while ( x >= y )
   {
      gui->device->pset(x0 - x, y0 + y, c);
      gui->device->pset(x0 - x, y0 - y, c);
      gui->device->pset(x0 + x, y0 + y, c);
      gui->device->pset(x0 + x, y0 - y, c);
      gui->device->pset(x0 - y, y0 + x, c);
      gui->device->pset(x0 - y, y0 - x, c);
      gui->device->pset(x0 + y, y0 + x, c);
      gui->device->pset(x0 + y, y0 - x, c);

      y++;
      e += yd;
      yd += 2;
      if ( ((e << 1) + xd) > 0 )
      {
         x--;
         e += xd;
         xd += 2;
      }
   }
}

void UG_FillCircle( UG_S16 x0, UG_S16 y0, UG_S16 r, UG_COLOR c )
{
   UG_S16  x,y,xd;

   if ( x0<0 ) return;
   if ( y0<0 ) return;
   if ( r<=0 ) return;

   xd = 3 - (r << 1);
   x = 0;
   y = r;

   while ( x <= y )
   {
     if( y > 0 )
     {
        UG_DrawLine(x0 - x, y0 - y,x0 - x, y0 + y, c);
        UG_DrawLine(x0 + x, y0 - y,x0 + x, y0 + y, c);
     }
     if( x > 0 )
     {
        UG_DrawLine(x0 - y, y0 - x,x0 - y, y0 + x, c);
        UG_DrawLine(x0 + y, y0 - x,x0 + y, y0 + x, c);
     }
     if ( xd < 0 )
     {
        xd += (x << 2) + 6;
     }
     else
     {
        xd += ((x - y) << 2) + 10;
        y--;
     }
     x++;
   }
   UG_DrawCircle(x0, y0, r,c);
}

void UG_DrawArc( UG_S16 x0, UG_S16 y0, UG_S16 r, UG_U8 s, UG_COLOR c )
{
   UG_S16 x,y,xd,yd,e;

   if ( x0<0 ) return;
   if ( y0<0 ) return;
   if ( r<=0 ) return;

   xd = 1 - (r << 1);
   yd = 0;
   e = 0;
   x = r;
   y = 0;

   while ( x >= y )
   {
      // Q1
      if ( s & 0x01 ) gui->device->pset(x0 + x, y0 - y, c);
      if ( s & 0x02 ) gui->device->pset(x0 + y, y0 - x, c);

      // Q2
      if ( s & 0x04 ) gui->device->pset(x0 - y, y0 - x, c);
      if ( s & 0x08 ) gui->device->pset(x0 - x, y0 - y, c);

      // Q3
      if ( s & 0x10 ) gui->device->pset(x0 - x, y0 + y, c);
      if ( s & 0x20 ) gui->device->pset(x0 - y, y0 + x, c);

      // Q4
      if ( s & 0x40 ) gui->device->pset(x0 + y, y0 + x, c);
      if ( s & 0x80 ) gui->device->pset(x0 + x, y0 + y, c);

      y++;
      e += yd;
      yd += 2;
      if ( ((e << 1) + xd) > 0 )
      {
         x--;
         e += xd;
         xd += 2;
      }
   }
}

void UG_DrawLine( UG_S16 x1, UG_S16 y1, UG_S16 x2, UG_S16 y2, UG_COLOR c )
{
   UG_S16 n, dx, dy, sgndx, sgndy, dxabs, dyabs, x, y, drawx, drawy;

   /* Is hardware acceleration available? */
   if ( gui->driver[DRIVER_DRAW_LINE].state & DRIVER_ENABLED )
   {
      if( ((UG_RESULT(*)(UG_S16 x1, UG_S16 y1, UG_S16 x2, UG_S16 y2, UG_COLOR c))gui->driver[DRIVER_DRAW_LINE].driver)(x1,y1,x2,y2,c) == UG_RESULT_OK ) return;
   }

   dx = x2 - x1;
   dy = y2 - y1;
   dxabs = (dx>0)?dx:-dx;
   dyabs = (dy>0)?dy:-dy;
   sgndx = (dx>0)?1:-1;
   sgndy = (dy>0)?1:-1;
   x = dyabs >> 1;
   y = dxabs >> 1;
   drawx = x1;
   drawy = y1;

   gui->device->pset(drawx, drawy,c);

   if( dxabs >= dyabs )
   {
      for( n=0; n<dxabs; n++ )
      {
         y += dyabs;
         if( y >= dxabs )
         {
            y -= dxabs;
            drawy += sgndy;
         }
         drawx += sgndx;
         gui->device->pset(drawx, drawy,c);
      }
   }
   else
   {
      for( n=0; n<dyabs; n++ )
      {
         x += dxabs;
         if( x >= dyabs )
         {
            x -= dyabs;
            drawx += sgndx;
         }
         drawy += sgndy;
         gui->device->pset(drawx, drawy,c);
      }
   }
}


/* Draw a triangle */
void UG_DrawTriangle( UG_S16 x1, UG_S16 y1, UG_S16 x2, UG_S16 y2, UG_S16 x3, UG_S16 y3, UG_COLOR c ){
  UG_DrawLine(x1, y1, x2, y2, c);
  UG_DrawLine(x2, y2, x3, y3, c);
  UG_DrawLine(x3, y3, x1, y1, c);
}

/* Fill a triangle */
void UG_FillTriangle( UG_S16 x1, UG_S16 y1, UG_S16 x2, UG_S16 y2, UG_S16 x3, UG_S16 y3, UG_COLOR c ){

  UG_S16 a, b, y, last;

  /* Sort coordinates by Y order (y3 >= y2 >= y1) */
  if (y1 > y2) {
    swap(y1, y2); swap(x1, x2);
  }
  if (y2 > y3) {
    swap(y3, y2); swap(x3, x2);
  }
  if (y1 > y2) {
    swap(y1, y2); swap(x1, x2);
  }

  /* Handle awkward all-on-same-line case as its own thing */
  if (y1 == y3) {
    a = b = x1;
    if (x2 < a) {
      a = x2;
    } else if (x2 > b) {
      b = x2;
    }
    if (x3 < a) {
      a = x3;
    } else if (x3 > b) {
      b = x3;
    }
    UG_DrawLine(a, y1, b + 1, y1, c);
    return;
  }

  UG_S16
  dx01 = x2 - x1,
  dy01 = y2 - y1,
  dx02 = x3 - x1,
  dy02 = y3 - y1,
  dx12 = x3 - x2,
  dy12 = y3 - y2,
  sa   = 0,
  sb   = 0;

  /* For upper part of triangle, find scanline crossings for segments
   * 0-1 and 0-2.  If y2=y3 (flat-bottomed triangle), the scanline y2
   * is included here (and second loop will be skipped, avoiding a /0
   * error there), otherwise scanline y2 is skipped here and handled
   * in the second loop...which also avoids a /0 error here if y1=y2
   * (flat-topped triangle).
   */
  if (y2 == y3) {
    last = y2;   /* Include y2 scanline */
  } else {
    last = y2 - 1; /* Skip it */
  }

  for (y = y1; y <= last; y++) {
    a   = x1 + sa / dy01;
    b   = x1 + sb / dy02;
    sa += dx01;
    sb += dx02;
    /* longhand:
       a = x1 + (x2 - x1) * (y - y1) / (y2 - y1);
       b = x1 + (x3 - x1) * (y - y1) / (y3 - y1);
       */
    if (a > b) {
      swap(a, b);
    }
    UG_DrawLine(a, y, b + 1, y, c);
  }

  /* For lower part of triangle, find scanline crossings for segments
   * 0-2 and 1-2.  This loop is skipped if y2=y3.
   */
  sa = dx12 * (y - y2);
  sb = dx02 * (y - y1);
  for (; y <= y3; y++) {
    a   = x2 + sa / dy12;
    b   = x1 + sb / dy02;
    sa += dx12;
    sb += dx02;
    /* longhand:
       a = x2 + (x3 - x2) * (y - y2) / (y3 - y2);
       b = x1 + (x3 - x1) * (y - y1) / (y3 - y1);
       */
    if (a > b) {
      swap(a, b);
    }
    UG_DrawLine(a, y, b + 1, y, c);
  }
}

void UG_PutString( UG_S16 x, UG_S16 y, char* str )
{
   UG_S16 xp,yp;
   UG_CHAR chr;
   UG_GLYPH g;
   UG_S16 line_h;

   _UG_FontSelect(gui->font);

   /* Industry standard: line height = ascender - descender */
   line_h = (UG_S16)gui->currentFont.ascender
          - (UG_S16)gui->currentFont.descender;
   if (line_h <= 0) line_h = 1;

   xp=x; yp=y;

   if (gui->currentFont.format == UG_FONT_FMT_NEW) {
      yp += (UG_S16)gui->currentFont.ascender;   /* line top -> baseline */
   }
   /* Old format: keep yp = y (line top), y_off = 0 */

   while ( *str != 0 )
   {
      #ifdef UGUI_USE_UTF8
      if(! gui->currentFont.is_old_font){
         chr = _UG_DecodeUTF8(&str);
      }
      else{
         chr = (UG_U8)*str++;
      }
      #else
      chr = *str++;
      #endif

      if ( chr == '\n' )
      {
         xp = gui->device->x_dim;
         continue;
      }
      if (_UG_GetGlyph(chr, &g) != 0) {
          UG_S16 adv = (UG_S16)gui->currentFont.notdef_adv;
          if (adv == 0) adv = (UG_S16)gui->currentFont.max_ink_w;
          xp += adv + gui->char_h_space;
          continue;
      }
      if ( xp + (UG_S16)g.adv > gui->device->x_dim - 1 )
      {
         xp = x;
         yp += line_h + gui->char_v_space;
      }

      _UG_PutGlyph(&g, xp, yp, gui->fore_color, gui->back_color, gui->transparent_font);

      xp += g.adv + gui->char_h_space;
   }
   if((gui->driver[DRIVER_FILL_AREA].state & DRIVER_ENABLED))
     ((void*(*)(UG_S16, UG_S16, UG_S16, UG_S16))gui->driver[DRIVER_FILL_AREA].driver)(-1,-1,-1,-1);
}

void UG_PutChar( UG_CHAR chr, UG_S16 x, UG_S16 y, UG_COLOR fc, UG_COLOR bc )
{
    _UG_FontSelect(gui->font);
    _UG_PutChar(chr,x,y,fc,bc);
    if((gui->driver[DRIVER_FILL_AREA].state & DRIVER_ENABLED))
      ((void*(*)(UG_S16, UG_S16, UG_S16, UG_S16))gui->driver[DRIVER_FILL_AREA].driver)(-1,-1,-1,-1);   // -1 to indicate finish
}

#if defined(UGUI_USE_CONSOLE)
void UG_ConsolePutString( char* str )
{
   UG_CHAR chr;
   UG_GLYPH g;
   UG_S16 line_h;
   UG_S16 y_draw;

   _UG_FontSelect(gui->font);
   /* Industry standard: line height = ascender - descender */
   line_h = (UG_S16)gui->currentFont.ascender
          - (UG_S16)gui->currentFont.descender;
   if (line_h <= 0) line_h = 1;

   while ( *str != 0 )
   {
      #ifdef UGUI_USE_UTF8
      if(! gui->currentFont.is_old_font){
        chr = _UG_DecodeUTF8(&str);
      }
      else{
        chr = (UG_U8)*str++;
      }
      #else
      chr = *str++;
      #endif
      if ( chr == '\n' )
      {
         gui->console.x_pos = gui->device->x_dim;
         continue;
      }

      if (_UG_GetGlyph(chr, &g) != 0) {
          UG_S16 adv = (UG_S16)gui->currentFont.notdef_adv;
          if (adv == 0) adv = (UG_S16)gui->currentFont.max_ink_w;
          gui->console.x_pos += adv + gui->char_h_space;
          continue;
      }
      gui->console.x_pos += g.adv+gui->char_h_space;

      if ( gui->console.x_pos+g.adv > gui->console.x_end )
      {
         gui->console.x_pos = gui->console.x_start;
         gui->console.y_pos += line_h+gui->char_v_space;
      }
      if ( gui->console.y_pos+ line_h > gui->console.y_end )
      {
         gui->console.x_pos = gui->console.x_start;
         gui->console.y_pos = gui->console.y_start;
         UG_FillFrame(gui->console.x_start,gui->console.y_start,gui->console.x_end,gui->console.y_end,gui->console.back_color);
      }

      y_draw = gui->console.y_pos;
      if (gui->currentFont.format == UG_FONT_FMT_NEW) {
         y_draw += (UG_S16)gui->currentFont.ascender;
      }
      _UG_PutGlyph(&g, gui->console.x_pos, y_draw, gui->console.fore_color, gui->console.back_color, gui->transparent_font);
   }
   if((gui->driver[DRIVER_FILL_AREA].state & DRIVER_ENABLED))
     ((void*(*)(UG_S16, UG_S16, UG_S16, UG_S16))gui->driver[DRIVER_FILL_AREA].driver)(-1,-1,-1,-1);
}

void UG_ConsoleSetArea( UG_S16 xs, UG_S16 ys, UG_S16 xe, UG_S16 ye )
{
   gui->console.x_start = xs;
   gui->console.y_start = ys;
   gui->console.x_end = xe;
   gui->console.y_end = ye;
}

void UG_ConsoleSetForecolor( UG_COLOR c )
{
   gui->console.fore_color = c;
}

void UG_ConsoleSetBackcolor( UG_COLOR c )
{
   gui->console.back_color = c;
}
#endif

void UG_SetForecolor( UG_COLOR c )
{
   gui->fore_color = c;
}

void UG_SetBackcolor( UG_COLOR c )
{
   gui->back_color = c;
}

UG_S16 UG_GetXDim( void )
{
   return gui->device->x_dim;
}

UG_S16 UG_GetYDim( void )
{
   return gui->device->y_dim;
}

void UG_FontSetHSpace( UG_U16 s )
{
   gui->char_h_space = s;
}

void UG_FontSetVSpace( UG_U16 s )
{
   gui->char_v_space = s;
}

void UG_FontSetTransparency( UG_U8 t )
{
  gui->transparent_font=t;
}

UG_U8 UG_FontGetTransparency( void )
{
  return gui->transparent_font;
}

void UG_FontSetShadow( UG_U8 t )
{
  gui->shadow_font=t;
}

UG_U8 UG_FontGetShadow( void )
{
  return gui->shadow_font;
}
/* -------------------------------------------------------------------------------- */
/* -- INTERNAL FUNCTIONS                                                         -- */
/* -------------------------------------------------------------------------------- */
/*
 * Parses a pointer to a string, and converts it to Unicode.
 * Automatically increasing the pointer address, so the calling function doesn't need to take care of that.
 *
 * Based on https://github.com/olikraus/u8g2/blob/master/csrc/u8x8_8x8.c
 *
 */
#ifdef UGUI_USE_UTF8
UG_CHAR _UG_DecodeUTF8(char **str) {
    unsigned char c = **str;
    uint32_t encoding = 0;
    int bytes_left = 0;

    if (c < 0x80) {
        (*str)++;
        return c;
    } else if (c < 0xE0) {
        if (c < 0xC2) { (*str)++; return -1; }   /* overlong / invalid */
        encoding = c & 0x1F;
        bytes_left = 1;
    } else if (c < 0xF0) {
        encoding = c & 0x0F;
        bytes_left = 2;
    } else if (c < 0xF8) {
        encoding = c & 0x07;
        bytes_left = 3;
    } else {
        // Invalid byte, should handle error
        (*str)++;
        return -1;
    }

    (*str)++;
    while (bytes_left > 0) {
        c = **str;
        if (c == 0) {
            /* Unexpected end of string, do not advance, let caller see '\0' */
            return 0;
        }
        if ((c & 0xC0) != 0x80) {
            // Invalid continuation byte
            (*str)++;
            return -1;
        }
        encoding = (encoding << 6) | (c & 0x3F);
        (*str)++;
        bytes_left--;
    }

    if (encoding > 0xFFFF) return -1;

    return (UG_CHAR)encoding;
}
#endif

/*
 *  Load char bitmap address into p, return the font width
 */
UG_S16 _UG_GetGlyph(UG_CHAR cp, UG_GLYPH *g) {
    static UG_CHAR last_cp;
    static UG_FONT *last_font;
    static UG_GLYPH last_g;
    static UG_U8 last_valid;

    if (gui->currentFont.font == last_font && cp == last_cp && last_valid) {
        *g = last_g;
        return 0;
    }

    if (gui->currentFont.format == UG_FONT_FMT_NEW) {
        UG_U32 n = gui->currentFont.number_of_chars;
        if (n == 0 || !gui->currentFont.codepoints) { last_valid = 0; return -1; }
        const UG_U8 *cps = gui->currentFont.codepoints;
        UG_U32 lo = 0, hi = n, skip = 0; UG_U8 found = 0;
        while (lo < hi) {
            UG_U32 mid = lo + (hi - lo) / 2;
            UG_U16 c = _ru16(cps + mid * 2);
            if (c == cp) { skip = mid; found = 1; break; }
            if (c < cp) lo = mid + 1; else hi = mid;
        }
        if (!found) { last_valid = 0; return -1; }
        const UG_U8 *m = gui->currentFont.metrics + skip * UG_FONT_METRICS_SIZE;
        g->w = _ru16(m + 0);
        g->h = _ru16(m + 2);
        g->x_off = (UG_S16)_ru16(m + 4);
        g->y_off = (UG_S16)_ru16(m + 6);
        g->adv = _ru16(m + 8);
        g->bit_order = UG_BIT_ORDER_LSB_LEFT;
        UG_U32 off = _ru32(gui->currentFont.data_offsets + skip * 4);
        g->data = gui->currentFont.new_data + off;
    } else {
        UG_CHAR enc = cp;
        if (gui->currentFont.is_old_font) {
            switch (enc) {
                case 0xF6: enc = 0x94; break;
                case 0xD6: enc = 0x99; break;
                case 0xFC: enc = 0x81; break;
                case 0xDC: enc = 0x9A; break;
                case 0xE4: enc = 0x84; break;
                case 0xC4: enc = 0x8E; break;
                case 0xB5: enc = 0xE6; break;
                case 0xB0: enc = 0xF8; break;
            }
        }
        UG_U16 n = gui->currentFont.old_number_of_offsets;
        UG_U16 start = 0, skip = 0;
        UG_U8 range = 0, found = 0;
        for (UG_U16 t = 0; t < n; t++) {
            UG_U16 co = _ru16(gui->currentFont.old_offsets + t * 2);
            if (gui->currentFont.old_range_flags[t]) {
                start = co; range = 1;
            } else if (range) {
                if (enc >= start && enc <= co) { skip += (enc - start); found = 1; break; }
                else if (enc < start) break;
                skip += ((co - start) + 1);
                range = 0;
            } else {
                if (enc == co) { found = 1; break; }
                else if (enc < co) break;
                skip++;
            }
        }
        if (!found) { last_valid = 0; return -1; }
        g->w = gui->currentFont.old_char_width;
        g->h = gui->currentFont.old_char_height;
        g->x_off = 0;
        g->y_off = 0;
        g->adv = gui->currentFont.old_widths
               ? gui->currentFont.old_widths[skip]
               : gui->currentFont.old_char_width;
        g->bit_order = UG_BIT_ORDER_LSB_LEFT;
        g->data = gui->currentFont.old_data + (UG_U32)skip * (UG_U32)gui->currentFont.old_bytes_per_char;
    }

    last_font = gui->currentFont.font;
    last_cp = cp;
    last_g = *g;
    last_valid = 1;
    return 0;
}

/*
 * Updates the current font data
 */
void _UG_FontSelect(UG_FONT *font) {
    if (gui->currentFont.font == font)
        return;

    const UG_U8 *p = (const UG_U8 *)font;
    gui->currentFont.font = font;
    gui->currentFont.is_old_font = (p[0] & 0x80) ? 1 : 0;

    if (p[0] & 0x80) {
        /* old format */
        gui->currentFont.format = UG_FONT_FMT_OLD;
      // gui->currentFont.font_type = p[0] & 0x7F;
        gui->currentFont.font_type = UG_FONT_TYPE_1BPP;
        gui->currentFont.old_char_width  = p[1];
        gui->currentFont.old_char_height = p[2];
        gui->currentFont.notdef_adv = gui->currentFont.old_char_width;

        /* Old format: cell model, line height = cell height */
        gui->currentFont.ascender  = (UG_S16)gui->currentFont.old_char_height;
        gui->currentFont.descender = 0;

        gui->currentFont.old_number_of_chars   = (UG_U16)((p[3] << 8) | p[4]);
        gui->currentFont.old_number_of_offsets = (UG_U16)((p[5] << 8) | p[6]);
        gui->currentFont.old_bytes_per_char    = (UG_U16)((p[9] << 8) | p[10]);
        gui->currentFont.old_widths_present    = p[11];
        const UG_U8 *q = p + 12;
        if (gui->currentFont.old_widths_present) {
            gui->currentFont.old_widths = q;
            q += gui->currentFont.old_number_of_chars;
        } else {
            gui->currentFont.old_widths = NULL;
        }
        gui->currentFont.old_offsets = q;
        q += gui->currentFont.old_number_of_offsets * 2;
        gui->currentFont.old_range_flags = q;
        q += gui->currentFont.old_number_of_offsets;
        gui->currentFont.old_data = q;
    } else {
        /* new format */
        gui->currentFont.format = UG_FONT_FMT_NEW;
        gui->currentFont.font_type = p[0] & 0x01;
        gui->currentFont.max_ink_w = (UG_U16)((p[2] << 8) | p[3]);
        gui->currentFont.max_ink_h = (UG_U16)((p[4] << 8) | p[5]);
        gui->currentFont.notdef_adv = (UG_U16)((p[14] << 8) | p[15]);

        /* Industry standard: ascender / descender (always present in regenerated fonts) */
        gui->currentFont.ascender  = (UG_S16)((p[16] << 8) | p[17]);
        gui->currentFont.descender = (UG_S16)((p[18] << 8) | p[19]);

        gui->currentFont.number_of_chars = ((UG_U32)p[6] << 24) | ((UG_U32)p[7] << 16) |
                                           ((UG_U32)p[8] << 8) | (UG_U32)p[9];
        gui->currentFont.total_size = ((UG_U32)p[10] << 24) | ((UG_U32)p[11] << 16) |
                                      ((UG_U32)p[12] << 8) | (UG_U32)p[13];
        const UG_U8 *q = p + UG_FONT_HEADER_SIZE;
        gui->currentFont.codepoints = q;
        q += (size_t)gui->currentFont.number_of_chars * UG_FONT_CODEPOINT_SIZE;
        gui->currentFont.metrics = q;
        q += (size_t)gui->currentFont.number_of_chars * UG_FONT_METRICS_SIZE;
        gui->currentFont.data_offsets = q;
        q += (size_t)gui->currentFont.number_of_chars * UG_FONT_DATA_OFFSET_SIZE;
        gui->currentFont.new_data = q;
    }
}

/* -------------------------------------------------------------------------------- */
/* -- Generic clipping: given a glyph's top-left corner (x0,y0) and size w,h,      */
/* -- compute the visible rectangle inside the screen.                             */
/* -------------------------------------------------------------------------------- */
typedef struct {
    UG_S16 X0, Y0, X1, Y1;   /* Visible screen rectangle, half-open [X0,X1) [Y0,Y1) */
    UG_S16 ox, oy;           /* Local-coordinate origin inside the glyph            */
    UG_S16 vw, vh;           /* Visible width and height                            */
    UG_U8  visible;          /* Non-zero if at least one pixel is visible           */
} UG_CLIP;

static UG_CLIP _UG_ClipGlyph(UG_S16 x0, UG_S16 y0, UG_U16 w, UG_U16 h)
{
    UG_CLIP c;
    UG_S16 x1 = x0 + (UG_S16)w;
    UG_S16 y1 = y0 + (UG_S16)h;

    c.X0 = x0; c.Y0 = y0; c.X1 = x1; c.Y1 = y1;
    if (c.X0 < 0) c.X0 = 0;
    if (c.Y0 < 0) c.Y0 = 0;
    if (c.X1 > gui->device->x_dim) c.X1 = gui->device->x_dim;
    if (c.Y1 > gui->device->y_dim) c.Y1 = gui->device->y_dim;

    c.visible = (c.X0 < c.X1) && (c.Y0 < c.Y1);
    c.ox = c.X0 - x0;
    c.oy = c.Y0 - y0;
    c.vw = c.X1 - c.X0;
    c.vh = c.Y1 - c.Y0;
    return c;
}

/* -------------------------------------------------------------------------------- */
/* -- Generic 1BPP blit: shared by body and shadow passes.                         */
/* -------------------------------------------------------------------------------- */
static void _UG_BlitGlyph1BPP(const UG_GLYPH *g,
                              UG_S16 x0, UG_S16 y0,
                              UG_COLOR fg, UG_COLOR bg,
                              UG_U8 transparent)
{
    UG_CLIP c = _UG_ClipGlyph(x0, y0, g->w, g->h);
    if (!c.visible) return;

    UG_U16 bytes_per_row = (g->w + 7) / 8;

    for (UG_S16 j = 0; j < c.vh; j++) {
        UG_S16 v = c.oy + j;
        const UG_U8 *row = g->data + (UG_U32)v * bytes_per_row;
        for (UG_S16 i = 0; i < c.vw; i++) {
            UG_S16 u = c.ox + i;
            UG_U8 byte = row[u >> 3];
            UG_U8 bit;
            if (g->bit_order == UG_BIT_ORDER_MSB_LEFT)
                bit = (byte >> (7 - (u & 7))) & 1;
            else
                bit = (byte >> (u & 7)) & 1;

            UG_S16 X = c.X0 + i;
            UG_S16 Y = c.Y0 + j;

            if (bit) {
                gui->device->pset(X, Y, fg);
            } else if (!transparent) {
                gui->device->pset(X, Y, bg);
            }
        }
    }
}

/* -------------------------------------------------------------------------------- */
/* -- Glyph rendering: shadow first, then the body.                                */
/* -------------------------------------------------------------------------------- */
static UG_S16 _UG_PutGlyph( UG_GLYPH *g, UG_S16 x, UG_S16 y, UG_COLOR fc, UG_COLOR bc, UG_U8 trans )
{
    if (g->w == 0 || g->h == 0)
        return (UG_S16)g->adv;

    UG_S16 draw_x = x + g->x_off;
    UG_S16 draw_y = y - g->y_off;

    UG_U8 driver = (gui->driver[DRIVER_FILL_AREA].state & DRIVER_ENABLED);

    /* ================= 1BPP fonts ================= */
    if (gui->currentFont.font_type == UG_FONT_TYPE_1BPP) {

        /* ---------- Shadow pass: offset by (+1,+1), ink only ----------
         * Note: shadow is only rendered on the non-accelerated path.
         * When DRIVER_FILL_AREA is enabled, the shadow pass is skipped
         * to keep the batched pixel-push logic simple.
         */
        if (!driver && gui->shadow_font) {
            UG_COLOR shadow_color =
                ((((fc & 0xFF)   * 128 + (bc & 0xFF)   * 128) >> 8) & 0xFF)   |
                ((((fc & 0xFF00) * 128 + (bc & 0xFF00) * 128) >> 8) & 0xFF00) |
                ((((fc & 0xFF0000) * 128 + (bc & 0xFF0000) * 128) >> 8) & 0xFF0000);

            _UG_BlitGlyph1BPP(g,
                              draw_x + 1, draw_y + 1,
                              shadow_color, shadow_color,
                              1 /* ink only, no background */);
        }

        /* ---------- Body pass ---------- */
        if (driver) {
            /* ---- Hardware acceleration: keep the original FILL_AREA logic ---- */
            UG_CLIP c = _UG_ClipGlyph(draw_x, draw_y, g->w, g->h);
            if (c.visible) {
                UG_U16 bytes_per_row = (g->w + 7) / 8;
                UG_S16 x0 = 0, y0 = 0, fpixels = 0, bpixels = 0;

                void (*push_pixels)(UG_U16, UG_COLOR) =
                    ((void*(*)(UG_S16, UG_S16, UG_S16, UG_S16))
                     gui->driver[DRIVER_FILL_AREA].driver)
                    (c.X0, c.Y0, c.X1 - 1, c.Y1 - 1);

                for (UG_S16 j = 0; j < c.vh; j++) {
                    UG_S16 v = c.oy + j;
                    const UG_U8 *row = g->data + (UG_U32)v * bytes_per_row;
                    for (UG_S16 i = 0; i < c.vw; i++) {
                        UG_S16 u = c.ox + i;
                        UG_U8 byte = row[u >> 3];
                        UG_U8 bit;
                        if (g->bit_order == UG_BIT_ORDER_MSB_LEFT)
                            bit = (byte >> (7 - (u & 7))) & 1;
                        else
                            bit = (byte >> (u & 7)) & 1;

                        if (bit) {
                            if (bpixels && !trans) { push_pixels(bpixels, bc); bpixels = 0; }
                            if (!fpixels && trans) { x0 = c.X0 + i; y0 = c.Y0 + j; }
                            fpixels++;
                        } else {
                            if (fpixels) {
                                if (!trans) {
                                    push_pixels(fpixels, fc);
                                    fpixels = 0;
                                } else {
                                    while (fpixels) {
                                        UG_U16 width = (c.X0 + c.vw) - x0;
                                        if (x0 == c.X0 || fpixels < width) {
                                            push_pixels = ((void*(*)(UG_S16, UG_S16, UG_S16, UG_S16))
                                                           gui->driver[DRIVER_FILL_AREA].driver)
                                                          (x0, y0, x0 + width - 1, y0 + (fpixels / c.vw));
                                            push_pixels(fpixels, fc);
                                            fpixels = 0;
                                        } else {
                                            push_pixels = ((void*(*)(UG_S16, UG_S16, UG_S16, UG_S16))
                                                           gui->driver[DRIVER_FILL_AREA].driver)
                                                          (x0, y0, x0 + width - 1, y0);
                                            push_pixels(fpixels, fc);
                                            fpixels -= width;
                                            x0 = c.X0;
                                            y0++;
                                        }
                                    }
                                }
                            }
                            bpixels++;
                        }
                    }
                }

                if (bpixels && !trans) {
                    push_pixels(bpixels, bc);
                } else if (fpixels) {
                    if (!trans) {
                        push_pixels(fpixels, fc);
                    } else {
                        while (fpixels) {
                            UG_U16 width = (c.X0 + c.vw) - x0;
                            if (x0 == c.X0 || fpixels < width) {
                                push_pixels = ((void*(*)(UG_S16, UG_S16, UG_S16, UG_S16))
                                               gui->driver[DRIVER_FILL_AREA].driver)
                                              (x0, y0, x0 + width - 1, y0 + (fpixels / c.vw));
                                push_pixels(fpixels, fc);
                                fpixels = 0;
                            } else {
                                push_pixels = ((void*(*)(UG_S16, UG_S16, UG_S16, UG_S16))
                                               gui->driver[DRIVER_FILL_AREA].driver)
                                              (x0, y0, x0 + width - 1, y0);
                                push_pixels(fpixels, fc);
                                fpixels -= width;
                                x0 = c.X0;
                                y0++;
                            }
                        }
                    }
                }

                ((void*(*)(UG_S16, UG_S16, UG_S16, UG_S16))
                 gui->driver[DRIVER_FILL_AREA].driver)(-1, -1, -1, -1);
            }
        } else {
            /* ---- No hardware acceleration: use the generic blit ---- */
            _UG_BlitGlyph1BPP(g,
                              draw_x, draw_y,
                              fc, bc,
                              trans);
        }
    }
#if defined(UGUI_USE_COLOR_RGB888) || defined(UGUI_USE_COLOR_RGB565)
    else {
        /* ================= 8BPP grayscale fonts ================= */
        UG_CLIP c = _UG_ClipGlyph(draw_x, draw_y, g->w, g->h);
        if (c.visible) {
            void (*push_pixels)(UG_U16, UG_COLOR) = NULL;
            if (driver) {
                push_pixels = ((void*(*)(UG_S16, UG_S16, UG_S16, UG_S16))
                               gui->driver[DRIVER_FILL_AREA].driver)
                              (c.X0, c.Y0, c.X1 - 1, c.Y1 - 1);
            }

            for (UG_S16 j = 0; j < c.vh; j++) {
                UG_S16 v = c.oy + j;
                const UG_U8 *row = g->data + (UG_U32)v * g->w;
                for (UG_S16 i = 0; i < c.vw; i++) {
                    UG_S16 u = c.ox + i;
                    UG_U8 b = row[u];
                    if (trans && b == 0) continue;
                    UG_COLOR color =
                        ((((fc & 0xFF) * b + (bc & 0xFF) * (256 - b)) >> 8) & 0xFF) |
                        ((((fc & 0xFF00) * b + (bc & 0xFF00) * (256 - b)) >> 8) & 0xFF00) |
                        ((((fc & 0xFF0000) * b + (bc & 0xFF0000) * (256 - b)) >> 8) & 0xFF0000);
                    if (driver) push_pixels(1, color);
                    else gui->device->pset(c.X0 + i, c.Y0 + j, color);
                }
            }

            if (driver)
                ((void*(*)(UG_S16, UG_S16, UG_S16, UG_S16))
                 gui->driver[DRIVER_FILL_AREA].driver)(-1, -1, -1, -1);
        }
    }
#endif

    return (UG_S16)g->adv;
}

UG_S16 _UG_PutChar( UG_CHAR chr, UG_S16 x, UG_S16 y, UG_COLOR fc, UG_COLOR bc )
{
    UG_GLYPH g;
    if (_UG_GetGlyph(chr, &g) != 0)
        return -1;
    return _UG_PutGlyph(&g, x, y, fc, bc, gui->transparent_font);
}

#ifdef UGUI_USE_TOUCH
static void _UG_ProcessTouchData( UG_WINDOW* wnd )
{
   UG_S16 xp,yp;
   UG_U16 i,objcnt;
   UG_OBJECT* obj;
   UG_U8 objstate;
   UG_U8 objtouch;
   UG_U8 tchstate;

   xp = gui->touch.xp;
   yp = gui->touch.yp;
   tchstate = gui->touch.state;

   objcnt = wnd->objcnt;
   for(i=0; i<objcnt; i++)
   {
      obj = (UG_OBJECT*)&wnd->objlst[i];
      objstate = obj->state;
      objtouch = obj->touch_state;
      if ( !(objstate & OBJ_STATE_FREE) && (objstate & OBJ_STATE_VALID) && (objstate & OBJ_STATE_VISIBLE) && !(objstate & OBJ_STATE_REDRAW))
      {
         /* Process touch data */
         if ( (tchstate) && xp != -1 )
         {
            if ( !(objtouch & OBJ_TOUCH_STATE_IS_PRESSED) )
            {
               objtouch |= OBJ_TOUCH_STATE_PRESSED_OUTSIDE_OBJECT | OBJ_TOUCH_STATE_CHANGED;
               objtouch &= ~(OBJ_TOUCH_STATE_RELEASED_ON_OBJECT | OBJ_TOUCH_STATE_RELEASED_OUTSIDE_OBJECT);
            }
            objtouch &= ~OBJ_TOUCH_STATE_IS_PRESSED_ON_OBJECT;
            if ( xp >= obj->a_abs.xs )
            {
               if ( xp <= obj->a_abs.xe )
               {
                  if ( yp >= obj->a_abs.ys )
                  {
                     if ( yp <= obj->a_abs.ye )
                     {
                        objtouch |= OBJ_TOUCH_STATE_IS_PRESSED_ON_OBJECT;
                        if ( !(objtouch & OBJ_TOUCH_STATE_IS_PRESSED) )
                        {
                           objtouch &= ~OBJ_TOUCH_STATE_PRESSED_OUTSIDE_OBJECT;
                           objtouch |= OBJ_TOUCH_STATE_PRESSED_ON_OBJECT;
                        }
                     }
                  }
               }
            }
            objtouch |= OBJ_TOUCH_STATE_IS_PRESSED;
         }
         else if ( objtouch & OBJ_TOUCH_STATE_IS_PRESSED )
         {
            if ( objtouch & OBJ_TOUCH_STATE_IS_PRESSED_ON_OBJECT )
            {
               objtouch |= OBJ_TOUCH_STATE_RELEASED_ON_OBJECT;
            }
            else
            {
               objtouch |= OBJ_TOUCH_STATE_RELEASED_OUTSIDE_OBJECT;
            }
            if ( objtouch & OBJ_TOUCH_STATE_IS_PRESSED )
            {
               objtouch |= OBJ_TOUCH_STATE_CHANGED;
            }
            objtouch &= ~(OBJ_TOUCH_STATE_PRESSED_OUTSIDE_OBJECT | OBJ_TOUCH_STATE_PRESSED_ON_OBJECT | OBJ_TOUCH_STATE_IS_PRESSED);
         }
      }
      obj->touch_state = objtouch;
   }
}
#endif

static void _UG_UpdateObjects( UG_WINDOW* wnd )
{
   UG_U16 i,objcnt;
   UG_OBJECT* obj;
   UG_U8 objstate;
   #ifdef UGUI_USE_TOUCH
   UG_U8 objtouch;
   #endif

   /* Check each object, if it needs to be updated? */
   objcnt = wnd->objcnt;
   for(i=0; i<objcnt; i++)
   {
      obj = (UG_OBJECT*)&wnd->objlst[i];
      objstate = obj->state;
      #ifdef UGUI_USE_TOUCH
      objtouch = obj->touch_state;
      #endif
      if ( !(objstate & OBJ_STATE_FREE) && (objstate & OBJ_STATE_VALID) )
      {
         if ( objstate & OBJ_STATE_UPDATE )
         {
            obj->update(wnd,obj);
         }
         #ifdef UGUI_USE_TOUCH
         if ( (objstate & OBJ_STATE_VISIBLE) && (objstate & OBJ_STATE_TOUCH_ENABLE) )
         {
            if ( (objtouch & (OBJ_TOUCH_STATE_CHANGED | OBJ_TOUCH_STATE_IS_PRESSED)) )
            {
               obj->update(wnd,obj);
            }
         }
         #endif
      }
   }
}

static void _UG_HandleEvents( UG_WINDOW* wnd )
{
   UG_U16 i,objcnt;
   UG_OBJECT* obj;
   UG_U8 objstate;
   static UG_MESSAGE msg;
   msg.src = NULL;

   /* Handle window-related events */
   //ToDo

   /* Handle object-related events */
   msg.type = MSG_TYPE_OBJECT;
   objcnt = wnd->objcnt;
   for(i=0; i<objcnt; i++)
   {
      obj = (UG_OBJECT*)&wnd->objlst[i];
      objstate = obj->state;
      if ( !(objstate & OBJ_STATE_FREE) && (objstate & OBJ_STATE_VALID) )
      {
         if ( obj->event != OBJ_EVENT_NONE )
         {
            msg.src = obj;
            msg.id = obj->type;
            msg.sub_id = obj->id;
            msg.event = obj->event;

            wnd->cb( &msg );

            obj->event = OBJ_EVENT_NONE;
         }
      }
   }
}

/* -------------------------------------------------------------------------------- */
/* -- INTERNAL API FUNCTIONS                                                         -- */
/* -------------------------------------------------------------------------------- */

void _UG_PutText(UG_TEXT* txt)
{
   if(!txt->font || !txt->str){
     return;
   }

   UG_S16 ye=txt->a.ye;
   UG_S16 ys=txt->a.ys;

   _UG_FontSelect(txt->font);
   /* Industry standard: line height = ascender - descender */
   UG_S16 char_height = (UG_S16)gui->currentFont.ascender
                      - (UG_S16)gui->currentFont.descender;

   if (char_height <= 0) return;

   if ( (ye - ys) < char_height ){
     return;
   }

   UG_U16 rc;
   UG_S16 xp,yp;
   UG_S16 xs=txt->a.xs;
   UG_S16 xe=txt->a.xe;
   UG_U8  align=txt->align;
   UG_S16 char_h_space=txt->h_space;
   UG_S16 char_v_space=txt->v_space;
   UG_GLYPH g;
   UG_CHAR chr;
   char* str = txt->str;
   char* c = str;

   rc=1;
   c=str;

   while (1)
   {
     #ifdef UGUI_USE_UTF8
     if(! gui->currentFont.is_old_font){
       chr = _UG_DecodeUTF8(&c);
     }
     else{
       chr = (UG_U8)*c++;
     }
     #else
     chr = *c++;
     #endif
     if(!chr) break;
     if ( chr == '\n' ) rc++;
   }

   yp = 0;
   if ( align & (ALIGN_V_CENTER | ALIGN_V_BOTTOM) )
   {
      yp = ye - ys + 1;
      yp -= char_height*rc;
      yp -= char_v_space*(rc-1);
      if ( yp < 0 ){
        return;
      }
   }
   if ( align & ALIGN_V_CENTER ) yp >>= 1;
   yp += ys;

   /* New format: line top -> baseline. Old format: keep line top (y_off = 0). */
   if (gui->currentFont.format == UG_FONT_FMT_NEW) {
      yp += (UG_S16)gui->currentFont.ascender;
   }

   while( 1 )
   {
      UG_U16 wl = 0;
      c=str;
      while(1)
      {
        #ifdef UGUI_USE_UTF8
        if(! gui->currentFont.is_old_font){
          chr = _UG_DecodeUTF8(&c);
        }
        else{
          chr = (UG_U8)*c++;
        }
        #else
        chr = *c++;
        #endif
        if( chr == 0 || chr == '\n'){
          break;
        }
         if (_UG_GetGlyph(chr, &g) != 0) {
             UG_S16 adv = (UG_S16)gui->currentFont.notdef_adv;
             if (adv == 0) adv = (UG_S16)gui->currentFont.max_ink_w;
             wl += adv + char_h_space;
             continue;
         }
         wl += g.adv + char_h_space;
      }
      wl -= char_h_space;

      xp = xe - xs + 1;
      xp -= wl;
      if ( xp < 0 ) break;

      if ( align & ALIGN_H_LEFT ) xp = 0;
      else if ( align & ALIGN_H_CENTER ) xp >>= 1;
      xp += xs;


      while(1){
         #ifdef UGUI_USE_UTF8
         if(! gui->currentFont.is_old_font){
           chr = _UG_DecodeUTF8(&str);
         }
         else{
           chr = (UG_U8)*str++;
         }
         #else
         chr = *str++;
         #endif
         if ( chr == 0 ){
           return;
         }
         else if(chr=='\n'){
           break;
         }
         if (_UG_GetGlyph(chr, &g) != 0) {
             UG_S16 adv = (UG_S16)gui->currentFont.notdef_adv;
             if (adv == 0) adv = (UG_S16)gui->currentFont.max_ink_w;
             xp += adv + char_h_space;
             continue;
         }
         _UG_PutGlyph(&g,xp,yp,txt->fc,txt->bc,gui->transparent_font);
         xp += g.adv + char_h_space;
      }
      yp += char_height + char_v_space;
   }
}

UG_OBJECT* _UG_SearchObject( UG_WINDOW* wnd, UG_U8 type, UG_U8 id )
{
   UG_U8 i;
   UG_OBJECT* obj=(UG_OBJECT*)wnd->objlst;

   for(i=0;i<wnd->objcnt;i++)
   {
      obj = (UG_OBJECT*)(&wnd->objlst[i]);
      if ( !(obj->state & OBJ_STATE_FREE) && (obj->state & OBJ_STATE_VALID) )
      {
         if ( (obj->type == type) && (obj->id == id) )
         {
            /* Requested object found! */
            return obj;
         }
      }
   }
   return NULL;
}

void _UG_DrawObjectFrame( UG_S16 xs, UG_S16 ys, UG_S16 xe, UG_S16 ye, UG_COLOR* p )
{
   // Frame 0
   UG_DrawLine(xs, ys  , xe-1, ys  , *p++);
   UG_DrawLine(xs, ys+1, xs  , ye-1, *p++);
   UG_DrawLine(xs, ye  , xe  , ye  , *p++);
   UG_DrawLine(xe, ys  , xe  , ye-1, *p++);
   // Frame 1
   UG_DrawLine(xs+1, ys+1, xe-2, ys+1, *p++);
   UG_DrawLine(xs+1, ys+2, xs+1, ye-2, *p++);
   UG_DrawLine(xs+1, ye-1, xe-1, ye-1, *p++);
   UG_DrawLine(xe-1, ys+1, xe-1, ye-2, *p++);
   // Frame 2
   UG_DrawLine(xs+2, ys+2, xe-3, ys+2, *p++);
   UG_DrawLine(xs+2, ys+3, xs+2, ye-3, *p++);
   UG_DrawLine(xs+2, ye-2, xe-2, ye-2, *p++);
   UG_DrawLine(xe-2, ys+2, xe-2, ye-3, *p);
}

UG_OBJECT* _UG_GetFreeObject( UG_WINDOW* wnd )
{
   UG_U8 i;
   UG_OBJECT* obj=(UG_OBJECT*)wnd->objlst;

   for(i=0;i<wnd->objcnt;i++)
   {
      obj = (UG_OBJECT*)(&wnd->objlst[i]);
      if ( (obj->state & OBJ_STATE_FREE) && (obj->state & OBJ_STATE_VALID) )
      {
         /* Free object found! */
         return obj;
      }
   }
   return NULL;
}

UG_RESULT _UG_DeleteObject( UG_WINDOW* wnd, UG_U8 type, UG_U8 id )
{
   UG_OBJECT* obj=NULL;

   obj = _UG_SearchObject( wnd, type, id );

   /* Object found? */
   if ( obj != NULL )
   {
      /* We dont't want to delete a visible or busy object! */
      if ( (obj->state & OBJ_STATE_VISIBLE) || (obj->state & OBJ_STATE_UPDATE) ) return UG_RESULT_FAIL;
      obj->state = OBJ_STATE_INIT;
      obj->data = NULL;
      obj->event = 0;
      obj->id = 0;
      #ifdef UGUI_USE_TOUCH
      obj->touch_state = 0;
      #endif
      obj->type = 0;
      obj->update = NULL;
      return UG_RESULT_OK;
   }
   return UG_RESULT_FAIL;
}

#ifdef UGUI_USE_PRERENDER_EVENT
void _UG_SendObjectPrerenderEvent( UG_WINDOW *wnd, UG_OBJECT *obj )
{
   UG_MESSAGE msg;
   msg.event = OBJ_EVENT_PRERENDER;
   msg.type = MSG_TYPE_OBJECT;
   msg.id = obj->type;
   msg.sub_id = obj->id;
   msg.src = obj;

   wnd->cb(&msg);
}
#endif

#ifdef UGUI_USE_POSTRENDER_EVENT
void _UG_SendObjectPostrenderEvent( UG_WINDOW *wnd, UG_OBJECT *obj )
{
   UG_MESSAGE msg;
   msg.event = OBJ_EVENT_POSTRENDER;
   msg.type = MSG_TYPE_OBJECT;
   msg.id = obj->type;
   msg.sub_id = obj->id;
   msg.src = obj;

   wnd->cb(&msg);
}
#endif

UG_U32 _UG_ConvertRGB565ToRGB888(UG_U16 c)
{
   UG_U32 r, g, b;

   r = (c >> 11) & 0x1F;
   r = (r << 3) | (r >> 2);
   r <<= 16;

   g = (c >> 5) & 0x3F;
   g = (g << 2) | (g >> 4);
   g <<= 8;

   b = c & 0x1F;
   b = (b << 3) | (b >> 2);

   return (r | g | b);
}

/* -------------------------------------------------------------------------------- */
/* -- DRIVER FUNCTIONS                                                           -- */
/* -------------------------------------------------------------------------------- */
void UG_DriverRegister( UG_U8 type, void* driver )
{
   if ( type >= NUMBER_OF_DRIVERS ) return;

   gui->driver[type].driver = driver;
   gui->driver[type].state = DRIVER_REGISTERED | DRIVER_ENABLED;
}

void UG_DriverEnable( UG_U8 type )
{
   if ( type >= NUMBER_OF_DRIVERS ) return;
   if ( gui->driver[type].state & DRIVER_REGISTERED )
   {
      gui->driver[type].state |= DRIVER_ENABLED;
   }
}

void UG_DriverDisable( UG_U8 type )
{
   if ( type >= NUMBER_OF_DRIVERS ) return;
   if ( gui->driver[type].state & DRIVER_REGISTERED )
   {
      gui->driver[type].state &= ~DRIVER_ENABLED;
   }
}

/* -------------------------------------------------------------------------------- */
/* -- MISCELLANEOUS FUNCTIONS                                                    -- */
/* -------------------------------------------------------------------------------- */
void UG_Update( void )
{
   UG_WINDOW* wnd;

   /* Is somebody waiting for this update? */
   if ( gui->state & UG_STATUS_WAIT_FOR_UPDATE ) gui->state &= ~UG_STATUS_WAIT_FOR_UPDATE;

   /* Keep track of the windows */
   if ( gui->next_window != gui->active_window )
   {
      if ( gui->next_window != NULL )
      {
         gui->last_window = gui->active_window;
         gui->active_window = gui->next_window;

         /* Do we need to draw an inactive title? */
         if ((gui->last_window != NULL) && (gui->last_window->style & WND_STYLE_SHOW_TITLE) && (gui->last_window->state & WND_STATE_VISIBLE) )
         {
            /* Do both windows differ in size */
            if ( (gui->last_window->xs != gui->active_window->xs) || (gui->last_window->xe != gui->active_window->xe) || (gui->last_window->ys != gui->active_window->ys) || (gui->last_window->ye != gui->active_window->ye) )
            {
               /* Redraw title of the last window */
               _UG_WindowDrawTitle( gui->last_window );
            }
         }
         gui->active_window->state &= ~WND_STATE_REDRAW_TITLE;
         gui->active_window->state |= WND_STATE_UPDATE | WND_STATE_VISIBLE;
      }
   }

   /* Is there an active window */
   if ( gui->active_window != NULL )
   {
      wnd = gui->active_window;

      /* Does the window need to be updated? */
      if ( wnd->state & WND_STATE_UPDATE )
      {
         /* Do it! */
         _UG_WindowUpdate( wnd );
      }

      /* Is the window visible? */
      if ( wnd->state & WND_STATE_VISIBLE )
      {
         #ifdef UGUI_USE_TOUCH
         _UG_ProcessTouchData( wnd );
         #endif
         _UG_UpdateObjects( wnd );
         _UG_HandleEvents( wnd );
      }
   }
   if(gui->device->flush){
     gui->device->flush();
   }
}

void UG_WaitForUpdate( void )
{
   gui->state |= UG_STATUS_WAIT_FOR_UPDATE;
   #ifdef UGUI_USE_MULTITASKING
   while ( (volatile UG_U8)gui->state & UG_STATUS_WAIT_FOR_UPDATE ){};
   #endif
   #ifndef UGUI_USE_MULTITASKING
   while ( (UG_U8)gui->state & UG_STATUS_WAIT_FOR_UPDATE ){};
   #endif
}

void UG_DrawBMP( UG_S16 xp, UG_S16 yp, UG_BMP* bmp )
{
   UG_S16 x,y;

   if ( bmp->p == NULL ) return;

   #if defined UGUI_USE_COLOR_RGB888 || defined UGUI_USE_COLOR_RGB565
   if ( bmp->bpp == BMP_BPP_16){

     /* Is hardware acceleration available? */

      if ( gui->driver[DRIVER_DRAW_BMP].state & DRIVER_ENABLED)
      {
        ((void(*)(UG_S16, UG_S16, UG_BMP* bmp))gui->driver[DRIVER_DRAW_BMP].driver)(xp,yp, bmp);
        return;
      }
      else if ( gui->driver[DRIVER_FILL_AREA].state & DRIVER_ENABLED)
      {
         void(*push_pixels)(UG_U16, UG_COLOR) = ((void*(*)(UG_S16, UG_S16, UG_S16, UG_S16))gui->driver[DRIVER_FILL_AREA].driver)(xp,yp,xp+bmp->width-1,yp+bmp->height-1);
         UG_U16 *p = (UG_U16*)bmp->p;
         for(y=0;y<bmp->height;y++)
         {
           for(x=0;x<bmp->width;x++)
           {
             #ifdef UGUI_USE_COLOR_RGB888
             push_pixels(1, _UG_ConvertRGB565ToRGB888(*p++)); /* Convert RGB565 to RGB888 */
             #elif defined UGUI_USE_COLOR_RGB565
             push_pixels(1, *p++);
             #endif
           }
           yp++;
         }
         return;
      }

     UG_U16 *p = (UG_U16*)bmp->p;
     for(y=0;y<bmp->height;y++)
     {
        for(x=0;x<bmp->width;x++)
        {
          #ifdef UGUI_USE_COLOR_RGB888
           UG_DrawPixel( xp+x , yp , _UG_ConvertRGB565ToRGB888(*p++) ); /* Convert RGB565 to RGB888 */
          #elif defined UGUI_USE_COLOR_RGB565
           UG_DrawPixel( xp+x , yp , *p++ );
          #endif
        }
        yp++;
     }
   }
   #endif
}

#ifdef UGUI_USE_TOUCH
void UG_TouchUpdate( UG_S16 xp, UG_S16 yp, UG_U8 state )
{
   gui->touch.xp = xp;
   gui->touch.yp = yp;
   gui->touch.state = state;
}
#endif

/* -------------------------------------------------------------------------------- */
/* -- WINDOW FUNCTIONS                                                           -- */
/* -------------------------------------------------------------------------------- */
UG_RESULT UG_WindowCreate( UG_WINDOW* wnd, UG_OBJECT* objlst, UG_U8 objcnt, void (*cb)( UG_MESSAGE* ) )
{
   UG_U8 i;
   UG_OBJECT* obj=NULL;

   if ( (wnd == NULL) || (objlst == NULL) || (objcnt == 0) ) return UG_RESULT_FAIL;

   /* Initialize all objects of the window */
   for(i=0; i<objcnt; i++)
   {
      obj = (UG_OBJECT*)&objlst[i];
      obj->state = OBJ_STATE_INIT;
      obj->data = NULL;
   }

   /* Initialize window */
   wnd->objcnt = objcnt;
   wnd->objlst = objlst;
   wnd->state = WND_STATE_VALID;
   wnd->fc = C_FORE_COLOR;
   wnd->bc = C_BACK_COLOR;
   wnd->xs = 0;
   wnd->ys = 0;
   wnd->xe = UG_GetXDim()-1;
   wnd->ye = UG_GetYDim()-1;
   wnd->cb = cb;
   wnd->style = WND_STYLE_3D | WND_STYLE_SHOW_TITLE;

   /* Initialize window title-bar */
   wnd->title.str = NULL;
   if (gui != NULL) wnd->title.font = gui->font;
   else wnd->title.font = NULL;
   wnd->title.h_space = 2;
   wnd->title.v_space = 2;
   wnd->title.align = ALIGN_CENTER_LEFT;
   wnd->title.fc = C_TITLE_FORE_COLOR;
   wnd->title.bc = C_TITLE_BACK_COLOR;
   wnd->title.ifc = C_INACTIVE_TITLE_FORE_COLOR;
   wnd->title.ibc = C_INACTIVE_TITLE_BACK_COLOR;
   wnd->title.height = 15;

   return UG_RESULT_OK;
}

UG_RESULT UG_WindowDelete( UG_WINDOW* wnd )
{
   if ( wnd == gui->active_window ) return UG_RESULT_FAIL;

   /* Only delete valid windows */
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      wnd->state = 0;
      wnd->cb = NULL;
      wnd->objcnt = 0;
      wnd->objlst = NULL;
      wnd->xs = 0;
      wnd->ys = 0;
      wnd->xe = 0;
      wnd->ye = 0;
      wnd->style = 0;
      return UG_RESULT_OK;
   }
   return UG_RESULT_FAIL;
}

UG_RESULT UG_WindowShow( UG_WINDOW* wnd )
{
   if ( wnd != NULL )
   {
      /* Force an update, even if this is the active window! */
      wnd->state |= WND_STATE_VISIBLE | WND_STATE_UPDATE;
      wnd->state &= ~WND_STATE_REDRAW_TITLE;
      gui->next_window = wnd;
      return UG_RESULT_OK;
   }
   return UG_RESULT_FAIL;
}

UG_RESULT UG_WindowHide( UG_WINDOW* wnd )
{
   if ( wnd != NULL )
   {
      if ( wnd == gui->active_window )
      {
         /* Is there an old window which just lost the focus? */
         if ( (gui->last_window != NULL) && (gui->last_window->state & WND_STATE_VISIBLE) )
         {
            if ( (gui->last_window->xs > wnd->xs) || (gui->last_window->ys > wnd->ys) || (gui->last_window->xe < wnd->xe) || (gui->last_window->ye < wnd->ye) )
            {
               _UG_WindowClear( wnd );
            }
            gui->next_window = gui->last_window;
         }
         else
         {
            gui->active_window->state &= ~WND_STATE_VISIBLE;
            gui->active_window->state |= WND_STATE_UPDATE;
         }
      }
      else
      {
         /* If the old window is visible, clear it! */
         _UG_WindowClear( wnd );
      }
      return UG_RESULT_OK;
   }
   return UG_RESULT_FAIL;
}

UG_RESULT UG_WindowResize( UG_WINDOW* wnd, UG_S16 xs, UG_S16 ys, UG_S16 xe, UG_S16 ye )
{
   UG_S16 pos;
   UG_S16 xmax,ymax;

   xmax = UG_GetXDim()-1;
   ymax = UG_GetYDim()-1;

   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      /* Do some checks... */
      if ( (xs < 0) || (ys < 0) ) return UG_RESULT_FAIL;
      if ( (xe > xmax) || (ye > ymax) ) return UG_RESULT_FAIL;
      pos = xe-xs;
      if ( pos < 10 ) return UG_RESULT_FAIL;
      pos = ye-ys;
      if ( pos < 10 ) return UG_RESULT_FAIL;

      /* ... and if everything is OK move the window! */
      wnd->xs = xs;
      wnd->ys = ys;
      wnd->xe = xe;
      wnd->ye = ye;

      if ( (wnd->state & WND_STATE_VISIBLE) && (gui->active_window == wnd) )
      {
         if ( wnd->ys ) UG_FillFrame(0, 0, xmax,wnd->ys-1,gui->desktop_color);
         pos = wnd->ye+1;
         if ( !(pos > ymax) ) UG_FillFrame(0, pos, xmax,ymax,gui->desktop_color);
         if ( wnd->xs ) UG_FillFrame(0, wnd->ys, wnd->xs-1,wnd->ye,gui->desktop_color);
         pos = wnd->xe+1;
         if ( !(pos > xmax) ) UG_FillFrame(pos, wnd->ys,xmax,wnd->ye,gui->desktop_color);

         wnd->state &= ~WND_STATE_REDRAW_TITLE;
         wnd->state |= WND_STATE_UPDATE;
      }
      return UG_RESULT_OK;
   }
   return UG_RESULT_FAIL;
}

UG_RESULT UG_WindowAlert( UG_WINDOW* wnd )
{
   UG_COLOR c;
   c = UG_WindowGetTitleTextColor( wnd );
   if ( UG_WindowSetTitleTextColor( wnd, UG_WindowGetTitleColor( wnd ) ) == UG_RESULT_FAIL ) return UG_RESULT_FAIL;
   if ( UG_WindowSetTitleColor( wnd, c ) == UG_RESULT_FAIL ) return UG_RESULT_FAIL;
   return UG_RESULT_OK;
}

UG_RESULT UG_WindowSetForeColor( UG_WINDOW* wnd, UG_COLOR fc )
{
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      wnd->fc = fc;
      wnd->state |= WND_STATE_UPDATE;
      return UG_RESULT_OK;
   }
   return UG_RESULT_FAIL;
}

UG_RESULT UG_WindowSetBackColor( UG_WINDOW* wnd, UG_COLOR bc )
{
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      wnd->bc = bc;
      wnd->state |= WND_STATE_UPDATE;
      return UG_RESULT_OK;
   }
   return UG_RESULT_FAIL;
}

UG_RESULT UG_WindowSetTitleTextColor( UG_WINDOW* wnd, UG_COLOR c )
{
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      wnd->title.fc = c;
      wnd->state |= WND_STATE_UPDATE | WND_STATE_REDRAW_TITLE;
      return UG_RESULT_OK;
   }
   return UG_RESULT_FAIL;
}

UG_RESULT UG_WindowSetTitleColor( UG_WINDOW* wnd, UG_COLOR c )
{
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      wnd->title.bc = c;
      wnd->state |= WND_STATE_UPDATE | WND_STATE_REDRAW_TITLE;
      return UG_RESULT_OK;
   }
   return UG_RESULT_FAIL;
}

UG_RESULT UG_WindowSetTitleInactiveTextColor( UG_WINDOW* wnd, UG_COLOR c )
{
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      wnd->title.ifc = c;
      wnd->state |= WND_STATE_UPDATE | WND_STATE_REDRAW_TITLE;
      return UG_RESULT_OK;
   }
   return UG_RESULT_FAIL;
}

UG_RESULT UG_WindowSetTitleInactiveColor( UG_WINDOW* wnd, UG_COLOR c )
{
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      wnd->title.ibc = c;
      wnd->state |= WND_STATE_UPDATE | WND_STATE_REDRAW_TITLE;
      return UG_RESULT_OK;
   }
   return UG_RESULT_FAIL;
}

UG_RESULT UG_WindowSetTitleText( UG_WINDOW* wnd, char* str )
{
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      wnd->title.str = str;
      wnd->state |= WND_STATE_UPDATE | WND_STATE_REDRAW_TITLE;
      return UG_RESULT_OK;
   }
   return UG_RESULT_FAIL;
}

UG_RESULT UG_WindowSetTitleTextFont( UG_WINDOW* wnd, UG_FONT* font )
{
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      wnd->state |= WND_STATE_UPDATE | WND_STATE_REDRAW_TITLE;
      wnd->title.font = font;
      if ( wnd->title.height <= (UG_GetFontHeight(font) + 1) )
      {
         wnd->title.height = UG_GetFontHeight(font) + 2;
         wnd->state &= ~WND_STATE_REDRAW_TITLE;
      }
      return UG_RESULT_OK;
   }
   return UG_RESULT_FAIL;
}

UG_RESULT UG_WindowSetTitleTextHSpace( UG_WINDOW* wnd, UG_S8 hs )
{
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      wnd->title.h_space = hs;
      wnd->state |= WND_STATE_UPDATE | WND_STATE_REDRAW_TITLE;
      return UG_RESULT_OK;
   }
   return UG_RESULT_FAIL;
}

UG_RESULT UG_WindowSetTitleTextVSpace( UG_WINDOW* wnd, UG_S8 vs )
{
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      wnd->title.v_space = vs;
      wnd->state |= WND_STATE_UPDATE | WND_STATE_REDRAW_TITLE;
      return UG_RESULT_OK;
   }
   return UG_RESULT_FAIL;
}

UG_RESULT UG_WindowSetTitleTextAlignment( UG_WINDOW* wnd, UG_U8 align )
{
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      wnd->title.align = align;
      wnd->state |= WND_STATE_UPDATE | WND_STATE_REDRAW_TITLE;
      return UG_RESULT_OK;
   }
   return UG_RESULT_FAIL;
}

UG_RESULT UG_WindowSetTitleHeight( UG_WINDOW* wnd, UG_U8 height )
{
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      wnd->title.height = height;
      wnd->state &= ~WND_STATE_REDRAW_TITLE;
      wnd->state |= WND_STATE_UPDATE;
      return UG_RESULT_OK;
   }
   return UG_RESULT_FAIL;
}

UG_RESULT UG_WindowSetXStart( UG_WINDOW* wnd, UG_S16 xs )
{
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      wnd->xs = xs;
      if ( UG_WindowResize( wnd, wnd->xs, wnd->ys, wnd->xe, wnd->ye) == UG_RESULT_FAIL ) return UG_RESULT_FAIL;
      return UG_RESULT_OK;
   }
   return UG_RESULT_FAIL;
}

UG_RESULT UG_WindowSetYStart( UG_WINDOW* wnd, UG_S16 ys )
{
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      wnd->ys = ys;
      if ( UG_WindowResize( wnd, wnd->xs, wnd->ys, wnd->xe, wnd->ye) == UG_RESULT_FAIL ) return UG_RESULT_FAIL;
      return UG_RESULT_OK;
   }
   return UG_RESULT_FAIL;
}

UG_RESULT UG_WindowSetXEnd( UG_WINDOW* wnd, UG_S16 xe )
{
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      wnd->xe = xe;
      if ( UG_WindowResize( wnd, wnd->xs, wnd->ys, wnd->xe, wnd->ye) == UG_RESULT_FAIL ) return UG_RESULT_FAIL;
      return UG_RESULT_OK;
   }
   return UG_RESULT_FAIL;
}

UG_RESULT UG_WindowSetYEnd( UG_WINDOW* wnd, UG_S16 ye )
{
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      wnd->ye = ye;
      if ( UG_WindowResize( wnd, wnd->xs, wnd->ys, wnd->xe, wnd->ye) == UG_RESULT_FAIL ) return UG_RESULT_FAIL;
      return UG_RESULT_OK;
   }
   return UG_RESULT_FAIL;
}

UG_RESULT UG_WindowSetStyle( UG_WINDOW* wnd, UG_U8 style )
{
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      /* 3D or 2D? */
      if ( style & WND_STYLE_3D )
      {
         wnd->style |= WND_STYLE_3D;
      }
      else
      {
         wnd->style &= ~WND_STYLE_3D;
      }
      /* Show title-bar? */
      if ( style & WND_STYLE_SHOW_TITLE )
      {
         wnd->style |= WND_STYLE_SHOW_TITLE;
      }
      else
      {
         wnd->style &= ~WND_STYLE_SHOW_TITLE;
      }
      wnd->state |= WND_STATE_UPDATE;
      return UG_RESULT_OK;
   }
   return UG_RESULT_FAIL;
}

UG_COLOR UG_WindowGetForeColor( UG_WINDOW* wnd )
{
   UG_COLOR c = C_BLACK;
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      c = wnd->fc;
   }
   return c;
}

UG_COLOR UG_WindowGetBackColor( UG_WINDOW* wnd )
{
   UG_COLOR c = C_BLACK;
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      c = wnd->bc;
   }
   return c;
}

UG_COLOR UG_WindowGetTitleTextColor( UG_WINDOW* wnd )
{
   UG_COLOR c = C_BLACK;
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      c = wnd->title.fc;
   }
   return c;
}

UG_COLOR UG_WindowGetTitleColor( UG_WINDOW* wnd )
{
   UG_COLOR c = C_BLACK;
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      c = wnd->title.bc;
   }
   return c;
}

UG_COLOR UG_WindowGetTitleInactiveTextColor( UG_WINDOW* wnd )
{
   UG_COLOR c = C_BLACK;
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      c = wnd->title.ifc;
   }
   return c;
}

UG_COLOR UG_WindowGetTitleInactiveColor( UG_WINDOW* wnd )
{
   UG_COLOR c = C_BLACK;
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      c = wnd->title.ibc;
   }
   return c;
}

char* UG_WindowGetTitleText( UG_WINDOW* wnd )
{
   char* str = NULL;
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      str = wnd->title.str;
   }
   return str;
}

UG_FONT* UG_WindowGetTitleTextFont( UG_WINDOW* wnd )
{
   UG_FONT* f = NULL;
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      f = (UG_FONT*)wnd->title.font;
   }
   return f;
}

UG_S8 UG_WindowGetTitleTextHSpace( UG_WINDOW* wnd )
{
   UG_S8 hs = 0;
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      hs = wnd->title.h_space;
   }
   return hs;
}

UG_S8 UG_WindowGetTitleTextVSpace( UG_WINDOW* wnd )
{
   UG_S8 vs = 0;
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      vs = wnd->title.v_space;
   }
   return vs;
}

UG_U8 UG_WindowGetTitleTextAlignment( UG_WINDOW* wnd )
{
   UG_U8 align = 0;
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      align = wnd->title.align;
   }
   return align;
}

UG_U8 UG_WindowGetTitleHeight( UG_WINDOW* wnd )
{
   UG_U8 h = 0;
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      h = wnd->title.height;
   }
   return h;
}

UG_S16 UG_WindowGetXStart( UG_WINDOW* wnd )
{
   UG_S16 xs = -1;
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      xs = wnd->xs;
   }
   return xs;
}

UG_S16 UG_WindowGetYStart( UG_WINDOW* wnd )
{
   UG_S16 ys = -1;
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      ys = wnd->ys;
   }
   return ys;
}

UG_S16 UG_WindowGetXEnd( UG_WINDOW* wnd )
{
   UG_S16 xe = -1;
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      xe = wnd->xe;
   }
   return xe;
}

UG_S16 UG_WindowGetYEnd( UG_WINDOW* wnd )
{
   UG_S16 ye = -1;
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      ye = wnd->ye;
   }
   return ye;
}

UG_U8 UG_WindowGetStyle( UG_WINDOW* wnd )
{
   UG_U8 style = 0;
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      style = wnd->style;
   }
   return style;
}

UG_RESULT UG_WindowGetArea( UG_WINDOW* wnd, UG_AREA* a )
{
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      a->xs = wnd->xs;
      a->ys = wnd->ys;
      a->xe = wnd->xe;
      a->ye = wnd->ye;
      if ( wnd->style & WND_STYLE_3D )
      {
         a->xs+=3;
         a->ys+=3;
         a->xe-=3;
         a->ye-=3;
      }
      if ( wnd->style & WND_STYLE_SHOW_TITLE )
      {
         a->ys+= wnd->title.height+1;
      }
      return UG_RESULT_OK;
   }
   return UG_RESULT_FAIL;
}

UG_S16 UG_WindowGetInnerWidth( UG_WINDOW* wnd )
{
   UG_S16 w = 0;
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      w = wnd->xe-wnd->xs;

      /* 3D style? */
      if ( wnd->style & WND_STYLE_3D ) w-=6;

      if ( w < 0 ) w = 0;
   }
   return w;
}

UG_S16 UG_WindowGetOuterWidth( UG_WINDOW* wnd )
{
   UG_S16 w = 0;
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      w = wnd->xe-wnd->xs;

      if ( w < 0 ) w = 0;
   }
   return w;
}

UG_S16 UG_WindowGetInnerHeight( UG_WINDOW* wnd )
{
   UG_S16 h = 0;
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      h = wnd->ye-wnd->ys;

      /* 3D style? */
      if ( wnd->style & WND_STYLE_3D ) h-=6;

      /* Is the title active */
      if ( wnd->style & WND_STYLE_SHOW_TITLE ) h-=wnd->title.height;

      if ( h < 0 ) h = 0;
   }
   return h;
}

UG_S16 UG_WindowGetOuterHeight( UG_WINDOW* wnd )
{
   UG_S16 h = 0;
   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      h = wnd->ye-wnd->ys;

      if ( h < 0 ) h = 0;
   }
   return h;
}

static UG_RESULT _UG_WindowDrawTitle( UG_WINDOW* wnd )
{
   UG_TEXT txt;
   UG_S16 xs,ys,xe,ye;

   if ( (wnd != NULL) && (wnd->state & WND_STATE_VALID) )
   {
      xs = wnd->xs;
      ys = wnd->ys;
      xe = wnd->xe;
      ye = wnd->ye;

      /* 3D style? */
      if ( wnd->style & WND_STYLE_3D )
      {
         xs+=3;
         ys+=3;
         xe-=3;
         ye-=3;
      }

      /* Is the window active or inactive? */
      if ( wnd == gui->active_window )
      {
         txt.bc = wnd->title.bc;
         txt.fc = wnd->title.fc;
      }
      else
      {
         txt.bc = wnd->title.ibc;
         txt.fc = wnd->title.ifc;
      }

      /* Draw title */
      UG_FillFrame(xs,ys,xe,ys+wnd->title.height-1,txt.bc);

      /* Draw title text */
      txt.str = wnd->title.str;
      txt.font = wnd->title.font;
      txt.a.xs = xs+3;
      txt.a.ys = ys;
      txt.a.xe = xe;
      txt.a.ye = ys+wnd->title.height-1;
      txt.align = wnd->title.align;
      txt.h_space = wnd->title.h_space;
      txt.v_space = wnd->title.v_space;
      _UG_PutText( &txt );

      /* Draw line */
      UG_DrawLine(xs,ys+wnd->title.height,xe,ys+wnd->title.height,pal_window[11]);
      return UG_RESULT_OK;
   }
   return UG_RESULT_FAIL;
}

static void _UG_WindowUpdate( UG_WINDOW* wnd )
{
   UG_U16 i,objcnt;
   UG_OBJECT* obj;
   UG_S16 xs,ys,xe,ye;

   xs = wnd->xs;
   ys = wnd->ys;
   xe = wnd->xe;
   ye = wnd->ye;

   wnd->state &= ~WND_STATE_UPDATE;
   /* Is the window visible? */
   if ( wnd->state & WND_STATE_VISIBLE )
   {
      /* 3D style? */
      if ( (wnd->style & WND_STYLE_3D) && !(wnd->state & WND_STATE_REDRAW_TITLE) )
      {
         _UG_DrawObjectFrame(xs,ys,xe,ye,(UG_COLOR*)pal_window);
         xs+=3;
         ys+=3;
         xe-=3;
         ye-=3;
      }
      /* Show title bar? */
      if ( wnd->style & WND_STYLE_SHOW_TITLE )
      {
         _UG_WindowDrawTitle( wnd );
         ys += wnd->title.height+1;
         if ( wnd->state & WND_STATE_REDRAW_TITLE )
         {
            wnd->state &= ~WND_STATE_REDRAW_TITLE;
            return;
         }
      }
      /* Draw window area? */
      UG_FillFrame(xs,ys,xe,ye,wnd->bc);

      /* Force each object to be updated! */
      objcnt = wnd->objcnt;
      for(i=0; i<objcnt; i++)
      {
         obj = (UG_OBJECT*)&wnd->objlst[i];
         if ( !(obj->state & OBJ_STATE_FREE) && (obj->state & OBJ_STATE_VALID) && (obj->state & OBJ_STATE_VISIBLE) ) obj->state |= (OBJ_STATE_UPDATE | OBJ_STATE_REDRAW);
      }
   }
   else
   {
      UG_FillFrame(wnd->xs,wnd->ys,wnd->xe,wnd->ye,gui->desktop_color);
   }
}

static UG_RESULT _UG_WindowClear( UG_WINDOW* wnd )
{
   if ( wnd != NULL )
   {
      if (wnd->state & WND_STATE_VISIBLE)
      {
         wnd->state &= ~WND_STATE_VISIBLE;
         UG_FillFrame( wnd->xs, wnd->ys, wnd->xe, wnd->ye, gui->desktop_color );

         if ( wnd != gui->active_window )
         {
            /* If the current window is visible, update it! */
            if ( gui->active_window->state & WND_STATE_VISIBLE )
            {
               gui->active_window->state &= ~WND_STATE_REDRAW_TITLE;
               gui->active_window->state |= WND_STATE_UPDATE;
            }
         }
      }
      return UG_RESULT_OK;
   }
   return UG_RESULT_FAIL;
}
