#ifndef FRONTEND_THEME_H
#define FRONTEND_THEME_H

/* Cores no formato ABGR8888 (0xAABBGGRR) esperado pelo vídeo. */
#ifdef RGB
#undef RGB /* o windows.h define um RGB próprio (COLORREF) */
#endif
#define RGB(r, g, b) (0xFF000000u | ((uint32_t) (b) << 16) | ((uint32_t) (g) << 8) | (uint32_t) (r))

#define THEME_WINDOW RGB(0x1B, 0x1D, 0x21)  /* fundo da janela */
#define THEME_BAR RGB(0x15, 0x17, 0x1A)     /* barra de título e de status */
#define THEME_PANEL RGB(0x1F, 0x22, 0x26)
#define THEME_PANEL2 RGB(0x23, 0x26, 0x2B)
#define THEME_BORDER RGB(0x2A, 0x2D, 0x33)
#define THEME_BORDER2 RGB(0x34, 0x38, 0x40)
#define THEME_TEXT RGB(0xE5, 0xE7, 0xEB)
#define THEME_DIM RGB(0x9C, 0xA3, 0xAF)     /* texto secundário */
#define THEME_ACCENT RGB(0x4F, 0x46, 0xE5)  /* seleção e destaque */
#define THEME_WHITE RGB(0xFF, 0xFF, 0xFF)   /* texto sobre o destaque */
#define THEME_FPS RGB(0x34, 0xD3, 0x99)
#define THEME_HOVER RGB(0x2C, 0x30, 0x37)
#define THEME_STRIPE RGB(0x20, 0x23, 0x28)  /* listra alternada da tabela */

/* Medidas da biblioteca (pixels do framebuffer da interface). */
#define UI_W 1280
#define UI_H 720
#define TITLE_H 32
#define MENU_H 28
#define TOOL_H 52
#define STATUS_H 28
#define SIDEBAR_W 208
#define HEADER_H 32
#define ROW_H 40
#define THUMB 32
#define SEARCH_W 280

#endif
