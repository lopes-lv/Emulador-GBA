#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <SDL.h>
#include <mgba/core/log.h>
#include <mgba/core/version.h>

#include "audio.h"
#include "config.h"
#include "folderpick.h"
#include "game.h"
#include "hotkeys.h"
#include "input.h"
#include "library.h"
#include "menu.h"
#include "nav.h"
#include "overlay.h"
#include "pacer.h"
#include "romscan.h"
#include "settings.h"
#include "stick.h"
#include "turbo.h"
#include "theme.h"
#include "ui_draw.h"
#include "ui_metrics.h"
#include "video.h"
#include "view.h"

#ifdef _WIN32
#include <windows.h>
#include <shellapi.h>

/* O mGBA espera caminhos em UTF-8; o argv do Windows vem na página de código ANSI. */
static char *utf8_arg(int index) {
	int wargc = 0;
	wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
	if (!wargv) return NULL;
	char *out = NULL;
	if (index < wargc) {
		int len = WideCharToMultiByte(CP_UTF8, 0, wargv[index], -1, NULL, 0, NULL, NULL);
		out = len > 0 ? malloc((size_t) len) : NULL;
		if (out) WideCharToMultiByte(CP_UTF8, 0, wargv[index], -1, out, len, NULL, NULL);
	}
	LocalFree(wargv);
	return out;
}
#endif

#define MENU_TITLE "Retr\xC3\xB4 Emu"
#define RT_THRESHOLD 16000

/* Só mostra avisos e erros do núcleo; logs de SWI/stub/debug ficam de fora. */
static void quiet_log(struct mLogger *logger, int category, enum mLogLevel level, const char *format, va_list args) {
	(void) logger;
	if (!(level & (mLOG_FATAL | mLOG_ERROR | mLOG_WARN))) return;
	fprintf(stderr, "[%s] ", mLogCategoryName(category));
	vfprintf(stderr, format, args);
	fputc('\n', stderr);
}

typedef struct {
	Config config;
	char config_path[1024];
	int config_path_ok;
	char meta_path[1024]; /* library.ini, ao lado do config.ini */
	char thumbs_dir[1024]; /* miniaturas, na pasta "thumbs" ao lado do config.ini */
	Menu menu;            /* tela Controles */
	View view;
	Library lib;
	int demo;
	int lib_loaded;       /* a biblioteca já foi varrida (falso se o jogo abriu por argumento) */
	Game *game;           /* != NULL enquanto joga */
	char game_path[1024];
	Uint64 game_start_ms;
	SDL_GameController *pad; /* primeiro controle conectado; NULL = só teclado */
	uint32_t *ui_fb;      /* quadro da biblioteca, do tamanho real da saída de vídeo (muda ao redimensionar) */
	int ui_fb_w, ui_fb_h;
	int dirty;            /* algo mudou: redesenhar a biblioteca */
	int last_blink;       /* fase do cursor piscante no último quadro */
	int msg_shown;        /* havia aviso na barra de status no último quadro */
	int running;
	/* contador de FPS do jogo (quadros emulados por segundo) */
	int frames;
	Uint64 fps_since;
	/* avanço rápido */
	Pacer pacer;
	Turbo turbo;          /* repetição dos botões segurados */
	Stick stick;          /* analógico esquerdo como direcional */
	Overlay overlay;      /* menu do jogo: painel por cima do jogo pausado */
	NavRepeat nav;        /* repetição da direção segurada no painel */
	Stick nav_stick;      /* analógico lido só para navegar no painel */
	uint32_t *ov_fb;      /* camada do painel, do tamanho real da saída (transparente fora do painel) */
	int ov_w, ov_h;
	int ov_dirty;         /* algo mudou: redesenhar a camada */
	Editor editor;        /* editor de macros (tela "Auto clique" das Configurações) */
	char editor_dir[1024]; /* macros/<jogo> do jogo selecionado na biblioteca */
	/* auto clique (macros do jogo atual) */
	MacroList macros;     /* lidas de macro_dir ao abrir o jogo */
	char macro_dir[1024]; /* macros/<jogo> ao lado do config.ini; vazio = sem pasta de configuração */
	Macro rec_macro;      /* a macro em gravação */
	MacroRecorder rec;
	MacroPlayer player;
	MacroUiState macro_ui;
	int ff_hold, ff_toggle, ff_was_active;
	/* aviso temporário na barra de status */
	char status_msg[256];
	Uint32 status_until;
} App;

static App app;

static const char *const SPEED_NAMES[FF_SPEED_COUNT] = {"1,5x", "2x", "3x", "4x", "6x", "8x"};

static void load_macros(const char *rom_utf8);
static void macro_abort(void);

static void set_macro_ui(MacroUiState state) {
	app.macro_ui = state;
	app.overlay.macro_state = state;
	app.ov_dirty = 1;
}

static void set_status(const char *msg, int ms) {
	snprintf(app.status_msg, sizeof(app.status_msg), "%s", msg);
	app.status_until = SDL_GetTicks() + (Uint32) ms;
}

static void save_config(void) {
	if (!app.config_path_ok || config_save(&app.config, app.config_path) != 0) {
		set_status("N\xC3\xA3o foi poss\xC3\xADvel gravar a configura\xC3\xA7\xC3\xA3o", 4000);
	}
}

/* Junta a pasta do config.ini com `name` (UTF-8). */
static int sibling_of_config(const char *name, char *out, size_t n) {
	const char *cut = strrchr(app.config_path, '\\');
	const char *cut2 = strrchr(app.config_path, '/');
	if (cut2 > cut) cut = cut2;
	if (!cut) return -1;
	int len = snprintf(out, n, "%.*s%s", (int) (cut - app.config_path + 1), app.config_path, name);
	return (len < 0 || (size_t) len >= n) ? -1 : 0;
}

static void save_meta(void) {
	if (app.meta_path[0] && !app.demo) library_meta_save(&app.lib, app.meta_path);
}

/* Varre a pasta configurada e atualiza a biblioteca; pasta sumida vira aviso, nunca erro fatal. */
static void refresh_roms(void) {
	if (app.demo) return;
	RomList list = {0};
	int rc = app.config.rom_dir[0] ? romscan_scan(app.config.rom_dir, &list) : 0;
	library_free(&app.lib);
	library_from_roms(&app.lib, &list);
	romscan_free(&list);
	if (app.meta_path[0]) library_meta_load(&app.lib, app.meta_path);
	if (app.thumbs_dir[0]) library_thumbs_load(&app.lib, app.thumbs_dir);
	view_set_library(&app.view, &app.lib, 1);
	app.lib_loaded = 1;
	if (rc != 0) set_status("Pasta n\xC3\xA3o encontrada. Pressione F1 para escolher outra.", 6000);
}

static void choose_folder(void) {
	if (app.demo) {
		set_status("Modo de demonstra\xC3\xA7\xC3\xA3o: somente visualiza\xC3\xA7\xC3\xA3o", 3000);
		return;
	}
	char picked[1024];
	int rc = folder_pick(picked, sizeof(picked));
	if (rc == 1) {
		snprintf(app.config.rom_dir, sizeof(app.config.rom_dir), "%s", picked);
		refresh_roms();
		save_config();
	} else if (rc < 0) {
		set_status("N\xC3\xA3o foi poss\xC3\xADvel abrir o seletor de pastas", 4000);
	} /* cancelou: mantém a pasta anterior */
}

static void show_library(void) {
	app.dirty = 1;
	video_set_mode(VIDEO_UI);
	video_set_title(MENU_TITLE);
	SDL_StartTextInput();
}

static void start_game(const char *rom_utf8) {
	char err[512];
	Game *g = game_open(rom_utf8, err, sizeof(err));
	if (!g) {
		set_status(err, 6000);
		return;
	}
	app.game = g;
	snprintf(app.game_path, sizeof(app.game_path), "%s", rom_utf8);
	SDL_StopTextInput();
	video_set_mode(VIDEO_GAME);
	video_set_scale_mode(app.config.settings.scale);
	audio_set_volume(app.config.settings.volume, app.config.settings.muted);
	if (app.config.settings.start_fullscreen) video_set_fullscreen(1);
	app.overlay.open = 0;
	video_overlay_clear();
	app.ff_hold = app.ff_was_active = 0;
	app.ff_toggle = app.config.settings.ff_always; /* "sempre ativo": já começa acelerado; F4 desliga na sessão */
	turbo_reset(&app.turbo);
	stick_reset(&app.stick);
	app.frames = 0;
	app.fps_since = SDL_GetTicks64();
	app.game_start_ms = app.fps_since;
	pacer_reset(&app.pacer, SDL_GetPerformanceCounter());
	set_macro_ui(MUI_IDLE);
	app.overlay.macro_sel = 0;
	load_macros(rom_utf8);
}

static LibEntry *find_entry(const char *path) {
	for (int i = 0; i < app.lib.count; i++) {
		if (strcmp(app.lib.items[i].path, path) == 0) return &app.lib.items[i];
	}
	return NULL;
}

static void stop_game(void) {
	macro_abort(); /* macro tocando para; gravação em andamento é descartada, com aviso na biblioteca */
	int64_t seconds = (int64_t) ((SDL_GetTicks64() - app.game_start_ms) / 1000);
	uint32_t thumb[LIB_THUMB * LIB_THUMB];
	library_thumb_make(game_pixels(app.game), thumb); /* último quadro, antes de fechar o jogo */
	game_close(app.game);
	app.game = NULL;
	video_osd_clear();
	LibEntry *entry = find_entry(app.game_path);
	if (entry && !app.demo) {
		library_add_session(entry, seconds, (int64_t) time(NULL));
		memcpy(entry->thumb, thumb, sizeof(thumb));
		entry->has_thumb = 1;
		if (app.thumbs_dir[0]) library_thumb_save(app.thumbs_dir, entry->path, thumb);
		save_meta();
	}
	show_library();
}

/* ---- eventos da biblioteca ---- */

static int is_repeatable_key(SDL_Scancode sc) {
	return sc == SDL_SCANCODE_UP || sc == SDL_SCANCODE_DOWN || sc == SDL_SCANCODE_LEFT || sc == SDL_SCANCODE_RIGHT ||
	       sc == SDL_SCANCODE_PAGEUP || sc == SDL_SCANCODE_PAGEDOWN || sc == SDL_SCANCODE_BACKSPACE;
}

/* O controle navega na biblioteca: direcional, A = Enter, B = ESC, ombros = PgUp/PgDn. */
static int pad_to_scancode(SDL_GameControllerButton b) {
	switch (b) {
	case SDL_CONTROLLER_BUTTON_DPAD_UP: return SDL_SCANCODE_UP;
	case SDL_CONTROLLER_BUTTON_DPAD_DOWN: return SDL_SCANCODE_DOWN;
	case SDL_CONTROLLER_BUTTON_DPAD_LEFT: return SDL_SCANCODE_LEFT;
	case SDL_CONTROLLER_BUTTON_DPAD_RIGHT: return SDL_SCANCODE_RIGHT;
	case SDL_CONTROLLER_BUTTON_LEFTSHOULDER: return SDL_SCANCODE_PAGEUP;
	case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER: return SDL_SCANCODE_PAGEDOWN;
	case SDL_CONTROLLER_BUTTON_A: return SDL_SCANCODE_RETURN;
	case SDL_CONTROLLER_BUTTON_B: return SDL_SCANCODE_ESCAPE;
	default: return SDL_SCANCODE_UNKNOWN;
	}
}

static void open_first_controller(void) {
	if (app.pad) return;
	for (int i = 0; i < SDL_NumJoysticks(); i++) {
		if (!SDL_IsGameController(i)) continue;
		app.pad = SDL_GameControllerOpen(i);
		if (app.pad) {
			printf("Controle conectado: %s\n", SDL_GameControllerName(app.pad));
			return;
		}
	}
}

/* Conexão e remoção de controle a qualquer momento; sem controle o jogo segue só com o teclado. */
static void handle_device_event(const SDL_Event *ev) {
	if (ev->type == SDL_CONTROLLERDEVICEADDED) {
		open_first_controller();
	} else if (ev->type == SDL_CONTROLLERDEVICEREMOVED && app.pad) {
		SDL_JoystickID id = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(app.pad));
		if (id == ev->cdevice.which) {
			SDL_GameControllerClose(app.pad);
			app.pad = NULL;
			printf("Controle removido\n");
			open_first_controller(); /* outro controle já conectado assume */
		}
	}
}

/* ---- editor de macros (tela "Auto clique") ---- */

/* Relê as macros do jogo do disco para o editor (depois de uma falha de gravação, por exemplo), mantendo a macro
 * escolhida pelo nome e o aviso que estava na tela. */
static void reload_editor_list(void) {
	char keep[MACRO_NAME_BYTES] = "", title[256], message[160];
	if (app.editor.sel >= 0 && app.editor.sel < app.editor.list.count) {
		snprintf(keep, sizeof(keep), "%s", app.editor.list.items[app.editor.sel].name);
	}
	snprintf(title, sizeof(title), "%s", app.editor.title);
	snprintf(message, sizeof(message), "%s", app.editor.message);
	MacroList list = {0};
	macro_list_dir(app.editor_dir, &list);
	editor_open(&app.editor, title, &list, app.editor_dir);
	for (int i = 0; i < app.editor.list.count; i++) {
		if (strcmp(app.editor.list.items[i].name, keep) == 0) editor_select(&app.editor, i);
	}
	editor_set_message(&app.editor, message);
}

static void open_editor(void) {
	const LibEntry *entry = view_selected(&app.view);
	if (!entry || app.demo) {
		set_status("Selecione um jogo na lista para editar as macros dele", 4000);
		return;
	}
	char base[1024];
	if (sibling_of_config("macros", base, sizeof(base)) != 0 ||
	    macro_dir_for_rom(base, entry->path, app.editor_dir, sizeof(app.editor_dir)) != 0) {
		set_status("Macros indispon\xC3\xADveis (sem pasta de configura\xC3\xA7\xC3\xA3o)", 4000);
		return;
	}
	MacroList list = {0};
	macro_list_dir(app.editor_dir, &list);
	int skipped = list.skipped;
	app.view.settings_open = 0;
	editor_open(&app.editor, entry->title, &list, app.editor_dir);
	if (skipped > 0) {
		char text[96];
		snprintf(text, sizeof(text), "%d arquivo(s) inv\xC3\xA1lido(s) ignorado(s)", skipped);
		editor_set_message(&app.editor, text);
	}
	app.dirty = 1;
}

/* Grava no disco o que o editor pediu. Falha: relê a lista (nada se perde nem se duplica) e avisa. */
static void apply_editor_action(EditorAction a) {
	const char *dir = app.editor_dir;
	Macro *m = (a.index >= 0 && a.index < app.editor.list.count) ? &app.editor.list.items[a.index] : NULL;
	const char *failure = NULL;
	switch (a.type) {
	case EA_SAVE:
		if (m && macro_save_file(m, dir) != 0) failure = "N\xC3\xA3o foi poss\xC3\xADvel salvar a macro";
		break;
	case EA_RENAME:
		if (m) {
			int rc = macro_rename_files(m, dir, a.old_name);
			if (rc == -2) failure = "J\xC3\xA1 existe uma macro com esse nome";
			else if (rc != 0) failure = "N\xC3\xA3o foi poss\xC3\xADvel renomear a macro";
		}
		break;
	case EA_DELETE:
		if (macro_delete_file(dir, a.old_name) != 0) failure = "N\xC3\xA3o foi poss\xC3\xADvel apagar os arquivos da macro";
		break;
	case EA_DUPLICATE:
		if (m) {
			if (macro_save_file(m, dir) != 0 || macro_copy_state(dir, a.old_name, m->name) != 0) {
				failure = "N\xC3\xA3o foi poss\xC3\xADvel duplicar a macro";
			} else {
				app.editor.has_state[a.index] = macro_has_state(dir, m->name);
			}
		}
		break;
	case EA_CLOSE: editor_close(&app.editor); break;
	case EA_NONE: break;
	}
	if (failure) {
		reload_editor_list();
		editor_set_message(&app.editor, failure);
	}
	app.dirty = 1;
}

static void handle_view_action(ViewAction action) {
	switch (action) {
	case VACT_PLAY: {
		const LibEntry *e = view_selected(&app.view);
		if (e) start_game(e->path);
		break;
	}
	case VACT_ADD_GAME: choose_folder(); break;
	case VACT_CONTROLS: menu_open(&app.menu); break;
	case VACT_SETTINGS_CHANGED:
		video_set_scale_mode(app.config.settings.scale);
		save_config();
		break;
	case VACT_WIN_MIN: video_minimize(); break;
	case VACT_WIN_MAX: video_toggle_maximize(); break;
	case VACT_AUTOCLICK: open_editor(); break;
	case VACT_QUIT: app.running = 0; break;
	case VACT_NONE: break;
	}
}

static void send_to_view(ViewEvent ev) {
	handle_view_action(view_handle(&app.view, ev));
}

static void send_key_to_view(SDL_Scancode sc, int mods) {
	if (sc == SDL_SCANCODE_F1) { choose_folder(); return; }                   /* atalhos da versão anterior */
	if (sc == SDL_SCANCODE_F2) { menu_open(&app.menu); return; }
	if (sc == SDL_SCANCODE_F5) {                                               /* "Atualizar lista" (antes no menu Visualizar) */
		if (app.demo) choose_folder(); else refresh_roms();
		return;
	}
	send_to_view((ViewEvent) {VEV_KEY, 0, 0, (int) sc, mods, 0, NULL});
}

/* Posição do mouse na imagem 1280x720; fora da imagem (barras pretas) vira (-1,-1). */
static void mouse_to_ui(int wx, int wy, int *ux, int *uy) {
	if (!video_window_to_ui(wx, wy, ux, uy)) *ux = *uy = -1;
}

static void handle_menu_action(MenuAction a) {
	if (a == ACT_CONFIG_CHANGED || a == ACT_CLOSE) save_config();
}

/* Tela Controles aberta: teclado e controle editam as teclas; o mouse é ignorado. */
static void run_keys_event(const SDL_Event *ev) {
	MenuEvent mev;
	if (ev->type == SDL_KEYDOWN) {
		SDL_Scancode sc = ev->key.keysym.scancode;
		if (app.menu.capturing) {
			if (ev->key.repeat) return;
			mev = (MenuEvent) {sc == SDL_SCANCODE_ESCAPE ? MEV_ESC : MEV_KEY_PRESSED, (int) sc};
		} else if (sc == SDL_SCANCODE_UP) {
			mev = (MenuEvent) {MEV_UP, 0};
		} else if (sc == SDL_SCANCODE_DOWN) {
			mev = (MenuEvent) {MEV_DOWN, 0};
		} else if (ev->key.repeat) {
			return;
		} else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER) {
			mev = (MenuEvent) {MEV_ENTER, 0};
		} else if (sc == SDL_SCANCODE_ESCAPE) {
			mev = (MenuEvent) {MEV_ESC, 0};
		} else {
			return;
		}
		handle_menu_action(menu_handle(&app.menu, mev));
	} else if (ev->type == SDL_CONTROLLERBUTTONDOWN) {
		SDL_GameControllerButton b = (SDL_GameControllerButton) ev->cbutton.button;
		if (app.menu.capturing) {
			mev = (MenuEvent) {MEV_PAD_PRESSED, (int) b};
		} else if (b == SDL_CONTROLLER_BUTTON_DPAD_UP) {
			mev = (MenuEvent) {MEV_UP, 0};
		} else if (b == SDL_CONTROLLER_BUTTON_DPAD_DOWN) {
			mev = (MenuEvent) {MEV_DOWN, 0};
		} else if (b == SDL_CONTROLLER_BUTTON_A) {
			mev = (MenuEvent) {MEV_ENTER, 0};
		} else if (b == SDL_CONTROLLER_BUTTON_B) {
			mev = (MenuEvent) {MEV_ESC, 0};
		} else {
			return;
		}
		handle_menu_action(menu_handle(&app.menu, mev));
	}
}

static void run_library_event(const SDL_Event *ev) {
	int ux, uy;
	switch (ev->type) {
	case SDL_MOUSEMOTION:
		mouse_to_ui(ev->motion.x, ev->motion.y, &ux, &uy);
		send_to_view((ViewEvent) {VEV_MOUSE_MOVE, ux, uy, 0, 0, 0, NULL});
		break;
	case SDL_MOUSEBUTTONDOWN:
		if (ev->button.button != SDL_BUTTON_LEFT) break;
		mouse_to_ui(ev->button.x, ev->button.y, &ux, &uy);
		send_to_view((ViewEvent) {ev->button.clicks >= 2 ? VEV_MOUSE_DBL : VEV_MOUSE_DOWN, ux, uy, 0, 0, 0, NULL});
		break;
	case SDL_MOUSEBUTTONUP:
		if (ev->button.button != SDL_BUTTON_LEFT) break;
		mouse_to_ui(ev->button.x, ev->button.y, &ux, &uy);
		send_to_view((ViewEvent) {VEV_MOUSE_UP, ux, uy, 0, 0, 0, NULL});
		break;
	case SDL_MOUSEWHEEL: {
		int mx, my;
		SDL_GetMouseState(&mx, &my);
		mouse_to_ui(mx, my, &ux, &uy);
		send_to_view((ViewEvent) {VEV_WHEEL, ux, uy, 0, 0, ev->wheel.y, NULL});
		break;
	}
	case SDL_KEYDOWN: {
		SDL_Scancode sc = ev->key.keysym.scancode;
		if (ev->key.repeat && !is_repeatable_key(sc)) break;
		send_key_to_view(sc, (int) SDL_GetModState());
		break;
	}
	case SDL_TEXTINPUT:
		send_to_view((ViewEvent) {VEV_TEXT, 0, 0, 0, 0, 0, ev->text.text});
		break;
	case SDL_CONTROLLERBUTTONDOWN: {
		int sc = pad_to_scancode((SDL_GameControllerButton) ev->cbutton.button);
		if (sc != SDL_SCANCODE_UNKNOWN) send_key_to_view((SDL_Scancode) sc, 0);
		break;
	}
	default: break;
	}
}

/* Editor de macros aberto: mouse, teclado, texto e controle (pelas mesmas teclas da biblioteca). */
static void send_to_editor(EditorEvent ev) {
	apply_editor_action(editor_handle(&app.editor, ev));
}

static void run_editor_event(const SDL_Event *ev) {
	int ux, uy;
	switch (ev->type) {
	case SDL_MOUSEMOTION:
		mouse_to_ui(ev->motion.x, ev->motion.y, &ux, &uy);
		send_to_editor((EditorEvent) {EEV_MOUSE_MOVE, ux, uy, 0, 0, 0, NULL, 0});
		break;
	case SDL_MOUSEBUTTONDOWN:
	case SDL_MOUSEBUTTONUP:
		if (ev->button.button != SDL_BUTTON_LEFT) break;
		mouse_to_ui(ev->button.x, ev->button.y, &ux, &uy);
		send_to_editor((EditorEvent) {ev->type == SDL_MOUSEBUTTONDOWN ? EEV_MOUSE_DOWN : EEV_MOUSE_UP, ux, uy, 0, 0, 0, NULL, ev->button.clicks});
		break;
	case SDL_MOUSEWHEEL: {
		int mx, my;
		SDL_GetMouseState(&mx, &my);
		mouse_to_ui(mx, my, &ux, &uy);
		send_to_editor((EditorEvent) {EEV_WHEEL, ux, uy, 0, 0, ev->wheel.y, NULL, 0});
		break;
	}
	case SDL_KEYDOWN: {
		SDL_Scancode sc = ev->key.keysym.scancode;
		if (ev->key.repeat && !is_repeatable_key(sc)) break;
		send_to_editor((EditorEvent) {EEV_KEY, 0, 0, (int) sc, (int) SDL_GetModState(), 0, NULL, 0});
		break;
	}
	case SDL_TEXTINPUT:
		send_to_editor((EditorEvent) {EEV_TEXT, 0, 0, 0, 0, 0, ev->text.text, 0});
		break;
	case SDL_CONTROLLERBUTTONDOWN: {
		int sc = pad_to_scancode((SDL_GameControllerButton) ev->cbutton.button);
		if (sc != SDL_SCANCODE_UNKNOWN) send_to_editor((EditorEvent) {EEV_KEY, 0, 0, sc, 0, 0, NULL, 0});
		break;
	}
	default: break;
	}
}

static void draw_screen(void) {
	StatusInfo info = {0};
	info.fps = 60;
	info.core_ready = 1;
	info.maximized = video_is_maximized();
	info.cursor_on = (int) ((SDL_GetTicks() / 500) % 2);
	info.now = (int64_t) time(NULL);
	info.message = SDL_TICKS_PASSED(SDL_GetTicks(), app.status_until) ? NULL : app.status_msg;
	/* Desenha direto no tamanho real da saída (pixels reais): nada é esticado depois. */
	int w, h;
	video_ui_size(&w, &h);
	if (w < 1 || h < 1) return;
	if (w != app.ui_fb_w || h != app.ui_fb_h || !app.ui_fb) {
		free(app.ui_fb);
		app.ui_fb = malloc((size_t) w * (size_t) h * sizeof(uint32_t));
		app.ui_fb_w = app.ui_fb ? w : 0;
		app.ui_fb_h = app.ui_fb ? h : 0;
		if (!app.ui_fb) return;
	}
	UiMetrics m = ui_metrics_fit(w, h);
	Canvas cv;
	canvas_init(&cv, app.ui_fb, w, h, m.scale, m.ox, m.oy);
	if (app.editor.open) ui_draw_editor(&cv, &app.view, &info, &app.editor);
	else if (app.menu.open) ui_draw_keys(&cv, &app.view, &info, &app.menu);
	else ui_draw_library(&cv, &app.view, &info);
	video_present_ui(app.ui_fb, w, h);
	app.dirty = 0;
}

static void run_menu_frame(void) {
	SDL_Event ev;
	while (SDL_PollEvent(&ev) && app.running && !app.game) {
		app.dirty = 1; /* qualquer evento (mouse, teclado, janela, controle) pode mudar o que se vê */
		handle_device_event(&ev);
		if (ev.type == SDL_QUIT) {
			app.running = 0;
		} else if (app.editor.open) {
			run_editor_event(&ev);
		} else if (app.menu.open) {
			run_keys_event(&ev);
		} else {
			run_library_event(&ev);
		}
	}
	if (!app.running || app.game) return;
	/* Redesenha só quando algo muda: evento, cursor da busca piscando ou aviso que aparece/expira. */
	int blink = (int) ((SDL_GetTicks() / 500) % 2);
	/* cursor piscante: o da busca, ou o do nome / da duração em edição no editor de macros */
	int blinking = app.editor.open ? (app.editor.name_editing || app.editor.dur_editing)
	                               : (app.view.focus == FOCUS_SEARCH && !app.menu.open);
	if (blinking && blink != app.last_blink) app.dirty = 1;
	app.last_blink = blink;
	int msg_active = app.status_msg[0] && !SDL_TICKS_PASSED(SDL_GetTicks(), app.status_until);
	if (msg_active != app.msg_shown) app.dirty = 1;
	app.msg_shown = msg_active;
	if (app.dirty || !app.ui_fb) draw_screen();
	SDL_Delay(8);
}

/* ---- jogo ---- */

static void osd_speed_indicator(void) {
	char text[32];
	snprintf(text, sizeof(text), ">> %s", SPEED_NAMES[app.config.settings.ff_index]);
	video_osd_show(text, 0);
}

static void do_save_state(void) {
	if (game_save_state(app.game)) {
		printf("Estado salvo em %s\n", game_state_path(app.game));
		video_osd_show("Estado salvo", 1500);
	} else {
		fprintf(stderr, "Erro: n\xC3\xA3o foi poss\xC3\xADvel salvar o estado em %s\n", game_state_path(app.game));
		video_osd_show("Falha ao salvar o estado", 2500);
	}
}

static void do_load_state(void) {
	if (game_load_state(app.game)) {
		printf("Estado carregado de %s\n", game_state_path(app.game));
		video_osd_show("Estado carregado", 1500);
	} else {
		fprintf(stderr, "N\xC3\xA3o foi poss\xC3\xADvel carregar o estado (inexistente ou inv\xC3\xA1lido): %s\n",
		        game_state_path(app.game));
		video_osd_show("Sem estado salvo", 2500);
	}
}

/* ---- menu do jogo (painel por cima do jogo pausado) ---- */

/* Direção segurada no direcional e no analógico esquerdo (sempre, mesmo com "Analógico como direcional" desligado). */
static NavDir held_nav(void) {
	if (!app.pad) return NAV_NONE;
	int ax = SDL_GameControllerGetAxis(app.pad, SDL_CONTROLLER_AXIS_LEFTX);
	int ay = SDL_GameControllerGetAxis(app.pad, SDL_CONTROLLER_AXIS_LEFTY);
	uint32_t bits = stick_step(&app.nav_stick, ax, ay, app.config.settings.stick_deadzone);
	static const struct { SDL_GameControllerButton button; GbaButton gba; } DPAD[] = {
		{SDL_CONTROLLER_BUTTON_DPAD_UP, BTN_UP}, {SDL_CONTROLLER_BUTTON_DPAD_DOWN, BTN_DOWN},
		{SDL_CONTROLLER_BUTTON_DPAD_LEFT, BTN_LEFT}, {SDL_CONTROLLER_BUTTON_DPAD_RIGHT, BTN_RIGHT}};
	for (size_t i = 0; i < sizeof(DPAD) / sizeof(DPAD[0]); i++) {
		if (SDL_GameControllerGetButton(app.pad, DPAD[i].button)) bits |= input_button_bit(DPAD[i].gba);
	}
	return nav_from_bits(bits);
}

static void open_overlay(void) {
	overlay_open(&app.overlay);
	audio_clear_queue();
	video_osd_clear();
	stick_reset(&app.nav_stick);
	navrepeat_reset(&app.nav, held_nav(), SDL_GetTicks()); /* o que já está segurado não dispara na hora */
	SDL_ShowCursor(SDL_ENABLE);
	app.ov_dirty = 1;
}

static void close_overlay(void) {
	app.overlay.open = 0;
	video_overlay_clear();
	pacer_reset(&app.pacer, SDL_GetPerformanceCounter()); /* sem "avanço em rajada" para recuperar o tempo parado */
	audio_clear_queue();
	turbo_reset(&app.turbo);
	stick_reset(&app.stick);
	app.ff_hold = SDL_GetKeyboardState(NULL)[SDL_SCANCODE_TAB] != 0; /* o Tab pode ter sido solto com o painel aberto */
	app.ff_was_active = 0; /* se o avanço ainda está ativo, o indicador volta sozinho */
	app.frames = 0;
	app.fps_since = SDL_GetTicks64();
	if (video_is_fullscreen()) SDL_ShowCursor(SDL_DISABLE);
}

/* Refaz a camada quando algo mudou ou quando a janela mudou de tamanho. */
static void refresh_overlay_layer(void) {
	int w, h;
	video_ui_size(&w, &h);
	if (w < 1 || h < 1) return;
	if (!app.ov_dirty && w == app.ov_w && h == app.ov_h) return;
	if (w != app.ov_w || h != app.ov_h || !app.ov_fb) {
		free(app.ov_fb);
		app.ov_fb = malloc((size_t) w * (size_t) h * sizeof(uint32_t));
		app.ov_w = app.ov_fb ? w : 0;
		app.ov_h = app.ov_fb ? h : 0;
		if (!app.ov_fb) return;
	}
	memset(app.ov_fb, 0, (size_t) w * (size_t) h * sizeof(uint32_t)); /* transparente fora do painel */
	UiMetrics m = ui_metrics_fit(w, h);
	Canvas cv;
	canvas_init(&cv, app.ov_fb, w, h, m.scale, m.ox, m.oy);
	ui_draw_overlay(&cv, &app.overlay);
	video_overlay_set(app.ov_fb, w, h);
	app.ov_dirty = 0;
}

/* ---- macros (auto clique) ---- */

/* Lê as macros do jogo (macros/<jogo> ao lado do config.ini). Arquivo inválido é ignorado e contado, nunca é erro. */
static void load_macros(const char *rom_utf8) {
	char base[1024];
	macrolist_free(&app.macros);
	macro_free(&app.rec_macro);
	app.macro_dir[0] = '\0';
	if (sibling_of_config("macros", base, sizeof(base)) != 0 ||
	    macro_dir_for_rom(base, rom_utf8, app.macro_dir, sizeof(app.macro_dir)) != 0) {
		app.macro_dir[0] = '\0';
		return;
	}
	macro_list_dir(app.macro_dir, &app.macros);
	if (app.macros.skipped > 0) {
		char text[96];
		snprintf(text, sizeof(text), "%d macro(s) inv\xC3\xA1lida(s) ignorada(s)", app.macros.skipped);
		video_osd_show(text, 3000);
	}
}

/* Termina a gravação em andamento: salva a macro se gravou algo; senão descarta (e apaga o estado salvo dela). */
static void finish_recording(int hit_limit) {
	macro_rec_end(&app.rec);
	Macro *m = &app.rec_macro;
	set_macro_ui(MUI_IDLE);
	if (m->count == 0) {
		macro_delete_file(app.macro_dir, m->name);
		macro_free(m);
		video_osd_show("Nada gravado", 2000);
		return;
	}
	if (macro_save_file(m, app.macro_dir) != 0) {
		macro_delete_file(app.macro_dir, m->name);
		macro_free(m);
		video_osd_show("N\xC3\xA3o foi poss\xC3\xADvel salvar a macro", 3000);
		return;
	}
	char name[MACRO_NAME_BYTES], text[200];
	snprintf(name, sizeof(name), "%s", m->name);
	int steps = m->count;
	macro_free(m);
	macrolist_free(&app.macros);
	macro_list_dir(app.macro_dir, &app.macros);
	app.overlay.macro_sel = 0;
	for (int i = 0; i < app.macros.count; i++) {
		if (strcmp(app.macros.items[i].name, name) == 0) app.overlay.macro_sel = i;
	}
	snprintf(text, sizeof(text), "Macro '%s' gravada (%d passos)%s", name, steps,
	         hit_limit ? ": limite da grava\xC3\xA7\xC3\xA3o atingido, salvei o que coube" : "");
	video_osd_show(text, 3500);
}

/* Sair do jogo: a macro que toca para e a gravação em andamento é descartada, com aviso na biblioteca. */
static void macro_abort(void) {
	if (app.macro_ui == MUI_RECORDING) {
		macro_delete_file(app.macro_dir, app.rec_macro.name);
		macro_free(&app.rec_macro);
		set_status("Grava\xC3\xA7\xC3\xA3o da macro descartada ao sair do jogo", 4000);
	}
	set_macro_ui(MUI_IDLE);
}

/* Chamada UMA vez por quadro emulado (também no avanço rápido): toca a macro no lugar das teclas do usuário, ou grava as
 * teclas do usuário (depois do analógico, antes do turbo). Devolve as teclas que valem neste quadro. */
static uint32_t macro_frame(uint32_t user_keys) {
	if (app.macro_ui == MUI_PLAYING) {
		uint32_t keys = user_keys;
		MacroPlayEvent ev = macro_play_step(&app.player, user_keys, &keys);
		if (ev == MP_RELOAD) { /* "do estado": recomeça do ponto onde a gravação começou */
			char state[1024];
			macro_state_path(app.macro_dir, app.player.macro->name, state, sizeof(state));
			if (!game_load_state_from(app.game, state)) {
				set_macro_ui(MUI_IDLE);
				video_osd_show("N\xC3\xA3o foi poss\xC3\xADvel recarregar o estado da macro", 3500);
				return user_keys;
			}
			ev = macro_play_step(&app.player, user_keys, &keys);
		}
		switch (ev) {
		case MP_RUNNING: return keys;
		case MP_DONE:
			set_macro_ui(MUI_IDLE);
			video_osd_show("Macro conclu\xC3\xAD""da", 1500);
			break;
		case MP_INTERRUPTED:
			set_macro_ui(MUI_IDLE);
			video_osd_show("Macro interrompida", 1500);
			break;
		case MP_RELOAD: /* nunca dois seguidos: trata como falha */
			set_macro_ui(MUI_IDLE);
			break;
		}
		return user_keys;
	}
	if (app.macro_ui == MUI_RECORDING && !macro_rec_frame(&app.rec, user_keys)) finish_recording(1);
	return user_keys;
}

static void start_macro_play(void) {
	if (app.macros.count == 0) return;
	int sel = app.overlay.macro_sel < app.macros.count ? app.overlay.macro_sel : 0;
	macro_play_start(&app.player, &app.macros.items[sel]);
	set_macro_ui(MUI_PLAYING);
	char text[160];
	snprintf(text, sizeof(text), "Tocando: %s", app.macros.items[sel].name);
	video_osd_show(text, 1500);
	close_overlay();
}

static void start_macro_record(void) {
	if (!app.macro_dir[0]) {
		video_osd_show("Macros indispon\xC3\xADveis (sem pasta de configura\xC3\xA7\xC3\xA3o)", 3000);
		return;
	}
	char name[MACRO_NAME_BYTES], state[1024];
	if (macro_unique_name(&app.macros, "Macro", name, sizeof(name)) != 0) return;
	macro_free(&app.rec_macro);
	macro_init(&app.rec_macro, name);
	macro_rec_begin(&app.rec, &app.rec_macro);
	/* O estado do início fica ao lado da macro, para "do_estado=1" recomeçar do mesmo ponto. */
	macro_ensure_dir(app.macro_dir);
	macro_state_path(app.macro_dir, name, state, sizeof(state));
	int has_state = game_save_state_to(app.game, state);
	if (!has_state) macro_delete_file(app.macro_dir, name);
	set_macro_ui(MUI_RECORDING);
	video_osd_show(has_state ? "Gravando: abra o menu (F12) para parar"
	                         : "Gravando (sem estado salvo): abra o menu (F12) para parar", 0);
	close_overlay();
}

static void apply_overlay_action(OverlayAction a) {
	Settings *s = &app.config.settings;
	switch (a) {
	case OACT_QUIT_GAME:
		close_overlay();
		stop_game(); /* como o ESC: grava o save, descarta gravação de macro em andamento e volta à biblioteca */
		break;
	case OACT_MACRO_PLAY: start_macro_play(); break;
	case OACT_MACRO_RECORD: start_macro_record(); break;
	case OACT_MACRO_STOP:
		if (app.macro_ui == MUI_PLAYING) {
			set_macro_ui(MUI_IDLE);
			video_osd_show("Macro interrompida", 1500);
		} else if (app.macro_ui == MUI_RECORDING) {
			finish_recording(0);
		}
		break;
	case OACT_SETTINGS_CHANGED:
		audio_set_volume(s->volume, s->muted);
		save_config();
		app.ov_dirty = 1;
		break;
	case OACT_SAVE_STATE: do_save_state(); break;
	case OACT_LOAD_STATE: do_load_state(); break;
	case OACT_CLOSE: close_overlay(); break;
	case OACT_NONE: break;
	}
}

static void send_overlay(OverlayEventType type, int x, int y) {
	if (!app.overlay.open) return;
	app.ov_dirty = 1;
	apply_overlay_action(overlay_handle(&app.overlay, (OverlayEvent) {type, x, y}));
}

static void fill_pad_pressed(uint8_t *pressed) {
	memset(pressed, 0, SDL_CONTROLLER_BUTTON_MAX);
	if (!app.pad) return;
	for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; b++) {
		pressed[b] = SDL_GameControllerGetButton(app.pad, (SDL_GameControllerButton) b);
	}
}

/* Jogo pausado com o painel aberto: só eventos, repetição da direção segurada e desenho. */
static void run_overlay_frame(void) {
	SDL_Event ev;
	while (app.overlay.open && app.running && SDL_PollEvent(&ev)) {
		handle_device_event(&ev);
		int ux, uy;
		switch (ev.type) {
		case SDL_QUIT: app.running = 0; break;
		case SDL_KEYDOWN: {
			SDL_Scancode sc = ev.key.keysym.scancode;
			if (sc == SDL_SCANCODE_UP) send_overlay(OEV_UP, 0, 0);
			else if (sc == SDL_SCANCODE_DOWN) send_overlay(OEV_DOWN, 0, 0);
			else if (sc == SDL_SCANCODE_LEFT) send_overlay(OEV_LEFT, 0, 0);
			else if (sc == SDL_SCANCODE_RIGHT) send_overlay(OEV_RIGHT, 0, 0);
			else if (ev.key.repeat) break;
			else if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER || sc == SDL_SCANCODE_SPACE) send_overlay(OEV_ACCEPT, 0, 0);
			else if (sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_F12) send_overlay(OEV_BACK, 0, 0);
			break;
		}
		case SDL_MOUSEMOTION:
			mouse_to_ui(ev.motion.x, ev.motion.y, &ux, &uy);
			send_overlay(OEV_MOUSE_MOVE, ux, uy);
			break;
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			if (ev.button.button != SDL_BUTTON_LEFT) break;
			mouse_to_ui(ev.button.x, ev.button.y, &ux, &uy);
			send_overlay(ev.type == SDL_MOUSEBUTTONDOWN ? OEV_MOUSE_DOWN : OEV_MOUSE_UP, ux, uy);
			break;
		case SDL_CONTROLLERBUTTONDOWN: {
			uint8_t pressed[SDL_CONTROLLER_BUTTON_MAX];
			fill_pad_pressed(pressed);
			SDL_GameControllerButton b = (SDL_GameControllerButton) ev.cbutton.button;
			if (hotkey_pad_opens_overlay(b, pressed) || b == SDL_CONTROLLER_BUTTON_B) send_overlay(OEV_BACK, 0, 0);
			else if (b == SDL_CONTROLLER_BUTTON_A) send_overlay(OEV_ACCEPT, 0, 0);
			break;
		}
		case SDL_WINDOWEVENT: app.ov_dirty = 1; break;
		default: break;
		}
	}
	if (!app.running || !app.overlay.open) return;

	switch (navrepeat_step(&app.nav, held_nav(), SDL_GetTicks())) {
	case NAV_UP: send_overlay(OEV_UP, 0, 0); break;
	case NAV_DOWN: send_overlay(OEV_DOWN, 0, 0); break;
	case NAV_LEFT: send_overlay(OEV_LEFT, 0, 0); break;
	case NAV_RIGHT: send_overlay(OEV_RIGHT, 0, 0); break;
	case NAV_NONE: break;
	}
	if (!app.overlay.open) return;
	refresh_overlay_layer();
	video_present(game_pixels(app.game)); /* o vsync dita o ritmo; o jogo não avança */
	SDL_Delay(1);                          /* evita girar à toa com a janela minimizada */
}

static void handle_hotkey(Hotkey hk, int is_down, int repeat) {
	Settings *s = &app.config.settings;
	char text[64];
	if (hk == HK_FF_HOLD) {
		app.ff_hold = is_down;
		return;
	}
	if (!is_down || repeat) return;
	switch (hk) {
	case HK_ESCAPE:
		if (video_is_fullscreen()) video_set_fullscreen(0); else stop_game();
		break;
	case HK_FF_TOGGLE: app.ff_toggle = !app.ff_toggle; break;
	case HK_SPEED_DOWN:
	case HK_SPEED_UP:
		if (settings_step_speed(s, hk == HK_SPEED_UP ? 1 : -1)) save_config();
		snprintf(text, sizeof(text), "Velocidade %s", SPEED_NAMES[s->ff_index]);
		video_osd_show(text, 1500);
		if (app.ff_was_active) osd_speed_indicator();
		break;
	case HK_VOL_DOWN:
	case HK_VOL_UP:
		if (settings_step_volume(s, hk == HK_VOL_UP ? 5 : -5)) save_config();
		audio_set_volume(s->volume, s->muted);
		snprintf(text, sizeof(text), "Volume %d%%", s->volume);
		video_osd_show(text, 1500);
		break;
	case HK_MUTE:
		video_osd_show(settings_toggle_mute(s) ? "Mudo" : "Som ligado", 1500);
		audio_set_volume(s->volume, s->muted);
		save_config();
		break;
	case HK_FULLSCREEN: video_set_fullscreen(!video_is_fullscreen()); break;
	case HK_SAVE: do_save_state(); break;
	case HK_LOAD: do_load_state(); break;
	case HK_OVERLAY: open_overlay(); break;
	default: break;
	}
}

static void count_fps(int emulated) {
	app.frames += emulated;
	Uint64 now = SDL_GetTicks64();
	if (now - app.fps_since >= 1000) {
		video_set_title_fps(app.frames * 1000.0 / (double) (now - app.fps_since));
		app.frames = 0;
		app.fps_since = now;
	}
}

static void run_game_frame(void) {
	if (app.overlay.open) {
		run_overlay_frame();
		return;
	}
	SDL_Event ev;
	while (SDL_PollEvent(&ev)) {
		handle_device_event(&ev);
		if (ev.type == SDL_QUIT) {
			app.running = 0;
		} else if (ev.type == SDL_KEYDOWN || ev.type == SDL_KEYUP) {
			Hotkey hk = hotkey_from_key(ev.key.keysym.scancode, ev.key.keysym.mod);
			if (hk != HK_NONE) handle_hotkey(hk, ev.type == SDL_KEYDOWN, ev.key.repeat);
			if (!app.game) return; /* ESC voltou para a biblioteca */
			if (app.overlay.open) return; /* F12 abriu o menu: o resto dos eventos é dele */
		} else if (ev.type == SDL_CONTROLLERBUTTONDOWN) {
			uint8_t pressed[SDL_CONTROLLER_BUTTON_MAX];
			fill_pad_pressed(pressed);
			if (hotkey_pad_opens_overlay((SDL_GameControllerButton) ev.cbutton.button, pressed)) {
				open_overlay();
				return;
			}
		}
	}
	if (!app.running) return;

	uint8_t pad_pressed[SDL_CONTROLLER_BUTTON_MAX];
	int rt = 0;
	if (app.pad) {
		for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; b++) {
			pad_pressed[b] = SDL_GameControllerGetButton(app.pad, (SDL_GameControllerButton) b);
		}
		rt = SDL_GameControllerGetAxis(app.pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT) > RT_THRESHOLD;
	}
	uint32_t pressed_keys = input_map(&app.config.bind, SDL_GetKeyboardState(NULL), app.pad ? pad_pressed : NULL);
	if (app.pad && app.config.settings.stick_on) {
		int ax = SDL_GameControllerGetAxis(app.pad, SDL_CONTROLLER_AXIS_LEFTX);
		int ay = SDL_GameControllerGetAxis(app.pad, SDL_CONTROLLER_AXIS_LEFTY);
		pressed_keys |= stick_step(&app.stick, ax, ay, app.config.settings.stick_deadzone);
	} else {
		stick_reset(&app.stick); /* sem controle ou analógico desligado: nenhuma direção fica presa */
	}

	int ff = app.ff_hold || app.ff_toggle || rt;
	if (ff != app.ff_was_active) { /* entrou ou saiu do avanço rápido */
		audio_clear_queue();
		pacer_reset(&app.pacer, SDL_GetPerformanceCounter());
		if (ff) osd_speed_indicator(); else video_osd_clear();
		app.ff_was_active = ff;
	}

	if (ff) {
		int n = pacer_frames_due(&app.pacer, SDL_GetPerformanceCounter(), SDL_GetPerformanceFrequency(),
		                         settings_ff_speed(&app.config.settings));
		for (int i = 0; i < n; i++) { /* o turbo conta quadros emulados, também no avanço rápido */
			game_set_keys(app.game, turbo_step(&app.turbo, &app.config.settings, macro_frame(pressed_keys)));
			game_run_frames(app.game, 1, 0);
		}
		video_present(game_pixels(app.game));
		count_fps(n);
		return;
	}
	/* O relógio dita o ritmo (59,7275 quadros/s). Esperar a fila de áudio e depois o vsync entregava os quadros em
	 * rajadas (1 ms, depois 20-30 ms), o que parece engasgo, principalmente em movimento rápido como a corrida. */
	int due = pacer_frames_due(&app.pacer, SDL_GetPerformanceCounter(), SDL_GetPerformanceFrequency(), 1.0);
	if (audio_queued_bytes() > AUDIO_QUEUE_MAX_BYTES) due = 0; /* áudio atrasado em relação ao relógio: deixa drenar */
	if (due == 0) {
		SDL_Delay(1);
		return;
	}
	for (int i = 0; i < due; i++) {
		game_set_keys(app.game, turbo_step(&app.turbo, &app.config.settings, macro_frame(pressed_keys)));
		game_run_frame(app.game);
	}
	video_present(game_pixels(app.game));
	count_fps(due);
}

int main(int argc, char **argv) {
	setvbuf(stdout, NULL, _IONBF, 0); /* mensagens aparecem na hora, mesmo redirecionadas */
	static struct mLogger logger = { .log = quiet_log };
	mLogSetDefaultLogger(&logger);

	const char *rom = NULL;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "--demo")) {
			app.demo = 1;
			continue;
		}
		if (rom) {
			fprintf(stderr, "Uso: %s [--demo] [rom.gba]\n", argv[0]);
			return 2;
		}
#ifdef _WIN32
		SetConsoleOutputCP(CP_UTF8);
		char *rom_utf8 = utf8_arg(i);
		if (!rom_utf8) rom_utf8 = _strdup(argv[i]);
		/* O mGBA só reconhece '/' como separador; com '\' ele não acha o diretório da ROM (.sav/.ss1). */
		for (char *c = rom_utf8; c && *c; c++) {
			if (*c == '\\') *c = '/';
		}
		rom = rom_utf8 ? rom_utf8 : argv[i];
#else
		rom = argv[i];
#endif
	}

	app.config_path_ok = config_default_path(app.config_path, sizeof(app.config_path)) == 0;
	if (!app.config_path_ok || config_load(&app.config, app.config_path) != 0) {
		config_defaults(&app.config); /* sem arquivo (ou sem APPDATA): usa os padrões */
	}
	if (app.config_path_ok) {
		sibling_of_config("library.ini", app.meta_path, sizeof(app.meta_path));
		sibling_of_config("thumbs", app.thumbs_dir, sizeof(app.thumbs_dir));
	}
	menu_init(&app.menu, &app.config.bind);
	view_init(&app.view, &app.config.settings);
	overlay_init(&app.overlay, &app.config.settings);
	app.overlay.macros = &app.macros;
	editor_init(&app.editor);
	app.running = 1;

	if (rom) {
		/* ROM por argumento: erro de abertura sai com código 1, como na linha de comando */
		if (video_init(UI_W, UI_H, 1) != 0) return 1;
		app.demo = 0;
		start_game(rom);
		if (!app.game) {
			fprintf(stderr, "%s\n", app.status_msg);
			video_shutdown();
			return 1;
		}
	} else {
		if (video_init(UI_W, UI_H, 1) != 0) return 1;
		show_library();
		if (app.demo) {
			library_demo(&app.lib);
			view_set_library(&app.view, &app.lib, 0);
			app.lib_loaded = 1;
		} else if (app.config.rom_dir[0] == '\0') {
			refresh_roms();
			draw_screen();
			choose_folder(); /* primeira execução: pede a pasta de ROMs */
		} else {
			refresh_roms();
		}
	}

	if (SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER) != 0) {
		fprintf(stderr, "Aviso: controle indispon\xC3\xADvel (%s); seguindo s\xC3\xB3 com o teclado.\n", SDL_GetError());
	} else {
		open_first_controller();
	}

	while (app.running) {
		if (app.game) {
			run_game_frame();
			if (!app.game && app.running && !app.lib_loaded && !app.demo) {
				refresh_roms(); /* voltou para a biblioteca após abrir por argumento */
			}
		} else {
			run_menu_frame();
		}
	}

	if (app.game) game_close(app.game);
	if (app.pad) SDL_GameControllerClose(app.pad);
	SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
	view_free(&app.view);
	library_free(&app.lib);
	free(app.ui_fb);
	macro_abort(); /* gravação em andamento é descartada (sem sobrar .state solto) */
	macrolist_free(&app.macros);
	macro_free(&app.rec_macro);
	editor_close(&app.editor);
	free(app.ov_fb);
	video_shutdown();
	return 0;
}
