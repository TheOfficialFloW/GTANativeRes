/*
  GTA Native Resolution Patch
  Copyright (C) 2019, TheFloW

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <pspsdk.h>
#include <pspkernel.h>
#include <pspge.h>
#include <pspgu.h>

#include <stdio.h>
#include <string.h>

#include <systemctrl.h>

PSP_MODULE_INFO("GTANativeResolution", 0, 1, 0);

// TODO: fix street lamp flickering
// TODO: fix rays (look at the scene in lcs after Toni steps out of the bus)
// TODO: fix small savedata screen
// TODO: fix multiplayer character position and size in vcs
// TODO: fix multiplayer character highlight in lcs

#define MAKE_CALL(a, f) _sw(0x0C000000 | (((u32)(f) >> 2) & 0x03FFFFFF), a);

#define MAKE_JUMP(a, f) _sw(0x08000000 | (((u32)(f) & 0x0FFFFFFC) >> 2), a);

#define HIJACK_FUNCTION(a, f, ptr) \
{ \
  u32 _func_ = a; \
  static u32 patch_buffer[3]; \
  _sw(_lw(_func_), (u32)patch_buffer); \
  _sw(_lw(_func_ + 4), (u32)patch_buffer + 8);\
  MAKE_JUMP((u32)patch_buffer + 4, _func_ + 8); \
  _sw(0x08000000 | (((u32)(f) >> 2) & 0x03FFFFFF), _func_); \
  _sw(0, _func_ + 4); \
  ptr = (void *)patch_buffer; \
}

#define DRAW_NATIVE 0x4BCDEF00

#define PITCH 960
#define WIDTH 960
#define HEIGHT 544
#define PIXELFORMAT 0

#define DISPLAY_BUFFER 0x0A000000

#define GE_LIST 0x0A100000
#define GE_LIST_SIZE 0x00100000

#define VRAM 0x04000000
#define VRAM_DRAW_BUFFER_OFFSET 0
#define VRAM_DEPTH_BUFFER_OFFSET 0x00100000

#define VRAM_TEXTURE_BUFFER 0x0A300000
#define VRAM_BACKUP_BUFFER 0x0A400000

#define CUSTOM_GE_LIST 0x0A500000

#define TEXTURE_MODE_CMD 0xC8D00002 // original: 0xC8000002

STMOD_HANDLER previous;

void sceGuEnd(unsigned int *list);

u32 *ge_list_offset;
u32 turn_offset, ge_list_1_offset, ge_list_2_offset;

int lcs = 0;

int (* initGu)(int mode, int pixelformat, int width, int height, int pitch);
int initGuPatched(int mode, int pixelformat, int width, int height, int pitch) {
  *(u32 *)DRAW_NATIVE = 1;
  return initGu(0, PIXELFORMAT, WIDTH, HEIGHT, PITCH);
}

void (* drawDisplay)();
void drawDisplayPatched() {
  u32 turn, curr, list;

  if (lcs) {
    turn = *(u32 *)turn_offset;
  } else {
    asm volatile ("lw %0, -8716($gp)" : "=r" (turn));
  }

  if (!turn) {
    curr = ge_list_1_offset;
    list = *(u32 *)(ge_list_1_offset + 4);
  } else {
    curr = ge_list_2_offset;
    list = *(u32 *)(ge_list_2_offset + 4);
  }

  if (!lcs) {
    *(u32 *)(ge_list_1_offset + 4) = CUSTOM_GE_LIST;
    *(u32 *)(ge_list_2_offset + 4) = CUSTOM_GE_LIST;
  }

  drawDisplay();

  sceGuStart(0, (void *)(*(u32 *)curr - 8));
  sceGuTexSync();
  sceGuCopyImage(PIXELFORMAT, 0, 0, WIDTH, HEIGHT, PITCH,
                 (void *)(0x40000000 | (VRAM + VRAM_DRAW_BUFFER_OFFSET)),
                 0, 0, PITCH, (void *)(0x40000000 | DISPLAY_BUFFER));
  sceGuTexSync();
  sceGuFinish();
}

void setTexture(u32 *list, void *buffer) {
  sceGuStart(0, (void *)*list);
  sceGuTexImage(0, 512, 512, PITCH, buffer);
  sceGuEnd(list);
}

void (* drawTexture)(u32 *list, int x0, int y0, int u0, int v0, int x1, int y1, int u1, int v1, int z);
void drawTexturePatched(u32 *list, int x0, int y0, int u0, int v0, int x1, int y1, int u1, int v1, int z) {
  int top = 0;
  int bottom = 960 * 272 * 2;
  int left = 144 * 2;
  int right = 480 * 2;

  sceGuStart(0, (void *)*list);
  sceGuCopyImage(0, 0, 0, 64, 64, 64,
                 (void *)(0x40000000 | (VRAM + VRAM_DEPTH_BUFFER_OFFSET)),
                 0, 0, 64, (void *)(0x40000000 | VRAM_BACKUP_BUFFER));
  sceGuTexSync();
  sceGuDrawBuffer(0, (void *)VRAM_DEPTH_BUFFER_OFFSET, 64);
  sceGuEnd(list);

  setTexture(list, (void *)(VRAM + VRAM_DRAW_BUFFER_OFFSET + top + left));
  drawTexture(list, 00, 00, 0, 0, 32, 32, 320, 272, 0);
  setTexture(list, (void *)(VRAM + VRAM_DRAW_BUFFER_OFFSET + top + right));
  drawTexture(list, 32, 00, 0, 0, 64, 32, 320, 272, 0);
  setTexture(list, (void *)(VRAM + VRAM_DRAW_BUFFER_OFFSET + bottom + left));
  drawTexture(list, 00, 32, 0, 0, 32, 64, 320, 272, 0);
  setTexture(list, (void *)(VRAM + VRAM_DRAW_BUFFER_OFFSET + bottom + right));
  drawTexture(list, 32, 32, 0, 0, 64, 64, 320, 272, 0);
}

void drawTexture2Patched(u32 *list, int x0, int y0, int u0, int v0, int x1, int y1, int u1, int v1, int z) {
  drawTexture(list, x0, y0, u0, v0, x1, y1, u1, v1, z);

  sceGuStart(0, (void *)*list);
  sceGuTexSync();
  sceGuCopyImage(0, 0, 0, 64, 64, 64,
                 (void *)(0x40000000 | (VRAM + VRAM_DEPTH_BUFFER_OFFSET)),
                 0, 0, 64, (void *)(0x40000000 | VRAM_TEXTURE_BUFFER));
  sceGuTexSync();
  sceGuCopyImage(0, 0, 0, 64, 64, 64,
                 (void *)(0x40000000 | VRAM_BACKUP_BUFFER),
                 0, 0, 64, (void *)(0x40000000 | (VRAM + VRAM_DEPTH_BUFFER_OFFSET)));
  sceGuTexSync();
  sceGuEnd(list);
}

// Affects the sky too
int (* drawReflection)(int a0);
int drawReflectionPatched(int a0) {
  sceGuStart(0, (void *)*ge_list_offset);
  sceGuEnable(GU_DITHER);
  sceGuEnd(ge_list_offset);

  int res = drawReflection(a0);

  sceGuStart(0, (void *)*ge_list_offset);
  sceGuDisable(GU_DITHER);
  sceGuEnd(ge_list_offset);

  return res;
}
/*
#define FPS_VAR 0x4BCDEF04

SceInt64 cur_micros = 0, delta_micros = 0, last_micros = 0;
u32 frames = 0;
float fps = 0.0f;

SceInt64 sceKernelGetSystemTimeWidePatched(void) {
  SceInt64 cur_micros = sceKernelGetSystemTimeWide();

  if (cur_micros >= (last_micros + 1000000)) {
    delta_micros = cur_micros - last_micros;
    last_micros = cur_micros;
    fps = (frames / (double)delta_micros) * 1000000.0f;
    memcpy((void *)FPS_VAR, &fps, 4);
    frames = 0;
  }

  frames++;

  return cur_micros;
}
*/
// ULUS10160
void PatchVCS(u32 text_addr) {
  ge_list_offset = (u32 *)(text_addr + 0x003C3370);
  ge_list_1_offset = text_addr + 0x00672200;
  ge_list_2_offset = text_addr + 0x006719C0;

  // Redirect vram offsets
  u32 store_draw_buffer = _lw(text_addr + 0x00202B18);
  u32 store_depth_buffer = _lw(text_addr + 0x00202B24);
  _sw(0x3C170000 | (VRAM_DRAW_BUFFER_OFFSET >> 16), text_addr + 0x00202B18);
  _sw(0x36F70000 | (VRAM_DRAW_BUFFER_OFFSET & 0xFFFF), text_addr + 0x00202B1C);
  _sw(store_draw_buffer, text_addr + 0x00202B20); // draw buffer
  _sw(0x3C060000 | (VRAM_DEPTH_BUFFER_OFFSET >> 16), text_addr + 0x00202B24);
  _sw(0x34C60000 | (VRAM_DEPTH_BUFFER_OFFSET & 0xFFFF), text_addr + 0x00202B28);
  _sw(store_depth_buffer, text_addr + 0x00202B2C); // depth buffer

  // Redirect GE list
  u32 store_ge_list_buffer = _lw(text_addr + 0x00202CA8);
  u32 store_ge_list_2_buffer = _lw(text_addr + 0x00202CB8);
  _sw(0x3C040000 | (GE_LIST >> 16), text_addr + 0x00202CA0);
  _sw(0x34840000 | (GE_LIST & 0xFFFF), text_addr + 0x00202CA4);
  _sw(store_ge_list_buffer, text_addr + 0x00202CA8);
  _sw(0x3C040000 | ((GE_LIST + GE_LIST_SIZE) >> 16), text_addr + 0x00202CAC);
  _sw(0x34840000 | ((GE_LIST + GE_LIST_SIZE) & 0xFFFF), text_addr + 0x00202CB0);
  _sw(0x03E00008, text_addr + 0x00202CB4);
  _sw(store_ge_list_2_buffer, text_addr + 0x00202CB8);

  _sw(0x3C050000 | (GE_LIST_SIZE >> 16), text_addr + 0x00202EB0);
  _sw(0x34A50000 | (GE_LIST_SIZE & 0xFFFF), text_addr + 0x00202EB4);

  // Redirect texture buffer
  u32 store_texture_buffer = _lw(text_addr + 0x00277B2C);
  _sw(0x3C040000 | (VRAM_TEXTURE_BUFFER >> 16), text_addr + 0x00277B2C);
  _sw(0x34840000 | (VRAM_TEXTURE_BUFFER & 0xFFFF), text_addr + 0x00277B30);
  _sw(store_texture_buffer, text_addr + 0x00277B34);
  _sw(0x2404FFFF, text_addr + 0x00277B38);

  // Patch GU init
  HIJACK_FUNCTION(text_addr + 0x002029DC, initGuPatched, initGu);

  // Patch draw display
  HIJACK_FUNCTION(text_addr + 0x00203C0C, drawDisplayPatched, drawDisplay);

  // Ignore drawings to display
  _sw(0, text_addr + 0x00204268);
  _sw(0, text_addr + 0x0020432C);
  _sw(0, text_addr + 0x00204398);

  // Fix map scissoring

  // map
  _sw(0x34060000 | 960, text_addr + 0x00177924);
  _sw(0x34070000 | 448, text_addr + 0x0017793C);

  // legend box
  _sw(0x34060000 | 960, text_addr + 0x00177BCC);
  _sw(0x34070000 | 448, text_addr + 0x00177BD4);

  // Fix loading bar proportion
  _sw(0x34040000 | 512, text_addr + 0x00131494);
  _sw(0x34040000 | 512, text_addr + 0x00131558);
  _sw(0x34040000 | 512, text_addr + 0x001315D8);
  _sw(0x34040000 | 512, text_addr + 0x00131664);

  _sw(0x34040000 | 320, text_addr + 0x001314AC);
  _sw(0x34050000 | 320, text_addr + 0x00131568);
  _sw(0x34050000 | 320, text_addr + 0x001315E8);
  _sw(0x34040000 | 320, text_addr + 0x00131674);

  // Fix aim gun
  _sw(0x3C0941E0, text_addr + 0x0021461C); // size
  _sw(0x3C0A43F0, text_addr + 0x00214624); // x
  _sw(0x3C0B4388, text_addr + 0x00214628); // y

  // Fix rocket launcher aim size
  _sh(0x4296, text_addr + 0x001BCC44);

  // Fix aim unknown
  _sw(0x3C0440C0, text_addr + 0x001BCFC4); // size
  _sw(0x3C0443F0, text_addr + 0x001BCFCC); // x
  _sw(0x3C044388, text_addr + 0x001BCFDC); // y
  _sw(0x3C044334, text_addr + 0x001BCFE4); // unknown

  // Fix aim sniper
  _sw(0x3C0543F0, text_addr + 0x001BD13C); // x
  _sw(0x3C054388, text_addr + 0x001BD148); // y
  _sw(0x3C054334, text_addr + 0x001BD154); // unknown
  _sw(0x3C054180, text_addr + 0x001BD170); // size

  // Fix ray proportion. TODO
  _sw(0x34040000 | 512, text_addr + 0x002A4400);
  _sw(0x34040000 | 320, text_addr + 0x002A4424);

  // Fix reflection
  _sh(PIXELFORMAT, text_addr + 0x00277EBC);
  _sh(PITCH, text_addr + 0x00277F2C);

  drawTexture = (void *)(text_addr + 0x000DBA08);
  MAKE_CALL(text_addr + 0x00277FE0, drawTexturePatched);
  MAKE_CALL(text_addr + 0x00278304, drawTexture2Patched);

  HIJACK_FUNCTION(text_addr + 0x00157BA4, drawReflectionPatched, drawReflection);

  // Patch mipmap level mode
  _sh(TEXTURE_MODE_CMD >> 16, text_addr + 0x000014AC);
  _sh(TEXTURE_MODE_CMD & 0xFFFF, text_addr + 0x000014B0);
  _sh(TEXTURE_MODE_CMD >> 16, text_addr + 0x00155398);
  _sh(TEXTURE_MODE_CMD & 0xFFFF, text_addr + 0x0015539C);
  _sh(TEXTURE_MODE_CMD >> 16, text_addr + 0x002769E4);
  _sh(TEXTURE_MODE_CMD & 0xFFFF, text_addr + 0x002769E8);
  _sh(TEXTURE_MODE_CMD >> 16, text_addr + 0x002785FC);
  _sh(TEXTURE_MODE_CMD & 0xFFFF, text_addr + 0x0027860C);

  // MAKE_CALL(text_addr + 0x002030D4, sceKernelGetSystemTimeWidePatched);

  // Cap to 20FPS
  // _sh(3, text_addr + 0x002030B4);
}

// ULUS10041
void PatchLCS(u32 text_addr) {
  ge_list_offset = (u32 *)(text_addr + 0x0038ACD0);
  turn_offset = text_addr + 0x00352AE8;
  ge_list_1_offset = text_addr + 0x00659340;
  ge_list_2_offset = text_addr + 0x00658B00;

  // Redirect vram offsets
  u32 store_draw_buffer = _lw(text_addr + 0x002B0100);
  u32 store_depth_buffer = _lw(text_addr + 0x002B010C);
  _sw(0x3C090000 | (VRAM_DRAW_BUFFER_OFFSET >> 16), text_addr + 0x002B0100);
  _sw(0x35290000 | (VRAM_DRAW_BUFFER_OFFSET & 0xFFFF), text_addr + 0x002B0104);
  _sw(store_draw_buffer, text_addr + 0x002B0108); // draw buffer
  _sw(0x3C060000 | (VRAM_DEPTH_BUFFER_OFFSET >> 16), text_addr + 0x002B010C);
  _sw(0x34C60000 | (VRAM_DEPTH_BUFFER_OFFSET & 0xFFFF), text_addr + 0x002B0110);
  _sw(store_depth_buffer, text_addr + 0x002B0114); // depth buffer

  // Redirect GE list
  _sw(0x3C040000 | (GE_LIST >> 16), text_addr + 0x002AEEC0);
  _sw(0x34840000 | (GE_LIST & 0xFFFF), text_addr + 0x002AEEC4);
  _sw(0x3C060000 | (GE_LIST_SIZE >> 16), text_addr + 0x002AEECC);
  _sw(0x24C60000 | (GE_LIST_SIZE & 0xFFFF), text_addr + 0x002AEED4);

  _sw(0x3C050000 | (GE_LIST_SIZE >> 16), text_addr + 0x002AF124);
  _sw(0x34A50000 | (GE_LIST_SIZE & 0xFFFF), text_addr + 0x002AF128);

  // Redirect texture buffer
  u32 store_texture_buffer = _lw(text_addr + 0x002A7290);
  _sw(0x3C040000 | (VRAM_TEXTURE_BUFFER >> 16), text_addr + 0x002A7290);
  _sw(0x34840000 | (VRAM_TEXTURE_BUFFER & 0xFFFF), text_addr + 0x002A7294);
  _sw(store_texture_buffer, text_addr + 0x002A7298);
  _sw(0x2404FFFF, text_addr + 0x002A729C);

  // Patch GU init
  HIJACK_FUNCTION(text_addr + 0x002AFF64, initGuPatched, initGu);

  // Patch draw display
  HIJACK_FUNCTION(text_addr + 0x002B02F8, drawDisplayPatched, drawDisplay);

  // Ignore drawings to display
  _sw(0, text_addr + 0x002B098C);
  _sw(0, text_addr + 0x002B0A60);
  _sw(0, text_addr + 0x002B0ACC);

  // Fix maps cursor
  _sw(0x34040000 | 272, text_addr + 0x002d6fc8);
  _sw(0x34050000 | 272, text_addr + 0x002d711c);
  _sw(0x34040000 | 272, text_addr + 0x002d72a4);
  _sw(0x34040000 | 272, text_addr + 0x002d741c);
  _sw(0x34040000 | 272, text_addr + 0x002d7548);
  _sw(0x34050000 | 272, text_addr + 0x002d76b8);
  _sw(0x34040000 | 272, text_addr + 0x002d77a4);
  _sw(0x34040000 | 272, text_addr + 0x002d77b0);
  _sw(0x34040000 | 272, text_addr + 0x002d77f4);
  _sw(0x34040000 | 272, text_addr + 0x002d7928);
  _sw(0x34050000 | 272, text_addr + 0x002d7ba4);
  _sw(0x34040000 | 272, text_addr + 0x002d7c90);
  _sw(0x34040000 | 272, text_addr + 0x002d7c9c);
  _sw(0x34040000 | 272, text_addr + 0x002d7ce0);
  _sw(0x34050000 | 272, text_addr + 0x002d7ec8);
  _sw(0x34040000 | 272, text_addr + 0x002d7f4c);
  _sw(0x34040000 | 272, text_addr + 0x002d80c0);
  _sw(0x34040000 | 272, text_addr + 0x002d8140);
  _sw(0x34050000 | 272, text_addr + 0x002d858c);
  _sw(0x34050000 | 272, text_addr + 0x002d8644);

  _sw(0x34040000 | 480, text_addr + 0x002d6fb0);
  _sw(0x34040000 | 480, text_addr + 0x002d7110);
  _sw(0x34040000 | 480, text_addr + 0x002d7118);
  _sw(0x34040000 | 480, text_addr + 0x002d728c);
  _sw(0x34040000 | 480, text_addr + 0x002d7404);
  _sw(0x34040000 | 480, text_addr + 0x002d7530);
  _sw(0x34040000 | 480, text_addr + 0x002d76b4);
  _sw(0x34040000 | 480, text_addr + 0x002d7830);
  _sw(0x34040000 | 480, text_addr + 0x002d7870);
  _sw(0x34040000 | 480, text_addr + 0x002d7910);
  _sw(0x34040000 | 480, text_addr + 0x002d7ba0);
  _sw(0x34040000 | 480, text_addr + 0x002d7d1c);
  _sw(0x34040000 | 480, text_addr + 0x002d7d5c);
  _sw(0x34050000 | 480, text_addr + 0x002d82a8);
  _sw(0x34040000 | 480, text_addr + 0x002d832c);
  _sw(0x34040000 | 480, text_addr + 0x002d8494);
  _sw(0x34040000 | 480, text_addr + 0x002d8514);
  _sw(0x34040000 | 480, text_addr + 0x002d8588);
  _sw(0x34040000 | 480, text_addr + 0x002d8640);

  // Fix maps cursor
  _sw(0x34040000 | 272, text_addr + 0x002e0158);
  _sw(0x34040000 | 480, text_addr + 0x002e0198);
  _sw(0x34040000 | 480, text_addr + 0x002e01f8);

  // Fix map background size
  _sw(0x34050000 | 272, text_addr + 0x002db7b0);
  _sw(0x34050000 | 272, text_addr + 0x002db860);
  _sw(0x34040000 | 480, text_addr + 0x002DB7A8);
  _sw(0x34040000 | 480, text_addr + 0x002DB858);

  // Fix map scissoring
  _sw(0x24E7FFA0, text_addr + 0x002DB808);
  _sw(0x24E7FFA0, text_addr + 0x002DB8B8);

  // Fix loading bar proportion
  _sw(0x34040000 | 512, text_addr + 0x001BDC04);
  _sw(0x34040000 | 512, text_addr + 0x001BDCD0);
  _sw(0x34040000 | 512, text_addr + 0x001BDD5C);
  _sw(0x34040000 | 512, text_addr + 0x001BDDE0);

  _sw(0x34040000 | 320, text_addr + 0x001BDC28);
  _sw(0x34050000 | 320, text_addr + 0x001BDCE0);
  _sw(0x34050000 | 320, text_addr + 0x001BDD6C);
  _sw(0x34040000 | 320, text_addr + 0x001BDDF0);

  // Fix aim gun
  _sw(0x3C0941E0, text_addr + 0x000657A0); // size
  _sw(0x3C0A43F0, text_addr + 0x000657A8); // x
  _sw(0x3C0B4388, text_addr + 0x000657AC); // y

  // Fix rocket launcher aim size
  _sh(0x4296, text_addr + 0x00185318);

  // Fix aim unknown
  _sw(0x3C0440C0, text_addr + 0x00185614); // size
  _sw(0x3C0443F0, text_addr + 0x0018561C); // x
  _sw(0x3C044388, text_addr + 0x0018562C); // y
  _sw(0x3C044334, text_addr + 0x00185634); // unknown

  // Fix aim sniper
  _sw(0x3C0543F0, text_addr + 0x00185790); // x
  _sw(0x3C054388, text_addr + 0x0018579C); // y
  _sw(0x3C054334, text_addr + 0x001857A8); // unknown
  _sw(0x3C054180, text_addr + 0x001857C4); // size

  // Fix ray scissoring
  _sw(0x34050000 | 1024, text_addr + 0x00149ff0);
  _sw(0x34050000 | 640, text_addr + 0x0014a004);
  _sw(0x34050000 | 1024, text_addr + 0x0014a040);
  _sw(0x34050000 | 640, text_addr + 0x0014a054);

  // Fix ray scissoring
  _sw(0x34040000 | 1024, text_addr + 0x002CC930);
  _sw(0x34050000 | 640, text_addr + 0x002CC938);

  // Fix ray proportion
  _sw(0x34040000 | 512, text_addr + 0x00221ad4);
  _sw(0x34040000 | 320, text_addr + 0x00221afc);

  // Fix proportion (health plus)
  _sw(0x34050200, text_addr + 0x002d5b60);
  _sw(0x34040200, text_addr + 0x002d5b84);
  _sw(0x34050140, text_addr + 0x002d5ba4);
  _sw(0x34040140, text_addr + 0x002d5bc8);

  // Fix reflection
  _sh(PIXELFORMAT, text_addr + 0x002A7644);
  _sh(PITCH, text_addr + 0x002A76B8);

  drawTexture = (void *)(text_addr + 0x0027AE00);
  MAKE_CALL(text_addr + 0x002A776C, drawTexturePatched);
  MAKE_CALL(text_addr + 0x002A7ABC, drawTexture2Patched);

  HIJACK_FUNCTION(text_addr + 0x002C5104, drawReflectionPatched, drawReflection);

  // Patch mipmap level mode
  _sh(TEXTURE_MODE_CMD >> 16, text_addr + 0x000B0AD0);
  _sh(TEXTURE_MODE_CMD & 0xFFFF, text_addr + 0x000B0AD4);
  _sh(TEXTURE_MODE_CMD >> 16, text_addr + 0x00150AEC);
  _sh(TEXTURE_MODE_CMD & 0xFFFF, text_addr + 0x00150AF0);
  _sh(TEXTURE_MODE_CMD >> 16, text_addr + 0x002A6350);
  _sh(TEXTURE_MODE_CMD & 0xFFFF, text_addr + 0x002A6354);
  _sh(TEXTURE_MODE_CMD >> 16, text_addr + 0x002A7DC4);
  _sh(TEXTURE_MODE_CMD & 0xFFFF, text_addr + 0x002A7DD4);

  // MAKE_CALL(text_addr + 0x002AF398, sceKernelGetSystemTimeWidePatched);

  // Cap to 20FPS
  // _sh(3, text_addr + 0x002AF378);
}

int OnModuleStart(SceModule2 *mod) {
  char *modname = mod->modname;
  u32 text_addr = mod->text_addr;

  if (strcmp(modname, "GTA3") == 0) {
    if (strcmp((char *)(text_addr + 0x00307F54), "GTA3") == 0) {
      lcs = 1;
      PatchLCS(text_addr);
    } else if (strcmp((char *)(text_addr + 0x0036F8D8), "GTA3") == 0) {
      lcs = 0;
      PatchVCS(text_addr);
    }

    sceKernelDcacheWritebackAll();
  }

  if (!previous)
    return 0;

  return previous(mod);
}

int module_start() {
  previous = sctrlHENSetStartModuleHandler(OnModuleStart);
  return 0;
}
