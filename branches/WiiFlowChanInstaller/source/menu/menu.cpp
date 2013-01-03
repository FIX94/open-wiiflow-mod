
#include <fstream>
#include <sys/stat.h>
#include <dirent.h>
#include <time.h>
#include <wchar.h>
#include <network.h>
#include <errno.h>

#include "menu.hpp"
#include "types.h"
#include "fonts.h"
#include "channel/nand.hpp"
#include "fileOps/fileOps.h"
#include "gui/Gekko.h"
#include "gui/GameTDB.hpp"
#include "loader/cios.h"
#include "loader/fs.h"
#include "loader/nk.h"
#include "loader/playlog.h"
#include "music/SoundHandler.hpp"
#include "memory/mem2.hpp"
#include "unzip/U8Archive.h"

// Sounds
extern const u8 click_wav[];
extern const u32 click_wav_size;
extern const u8 hover_wav[];
extern const u32 hover_wav_size;
extern const u8 camera_wav[];
extern const u32 camera_wav_size;
// Pics
extern const u8 btnplus_png[];
extern const u8 btnpluss_png[];
extern const u8 btnminus_png[];
extern const u8 btnminuss_png[];
extern const u8 background_jpg[];
extern const u32 background_jpg_size;
extern const u8 butleft_png[];
extern const u8 butcenter_png[];
extern const u8 butright_png[];
extern const u8 butsleft_png[];
extern const u8 butscenter_png[];
extern const u8 butsright_png[];
extern const u8 buthleft_png[];
extern const u8 buthcenter_png[];
extern const u8 buthright_png[];
extern const u8 buthsleft_png[];
extern const u8 buthscenter_png[];
extern const u8 buthsright_png[];
extern const u8 pbarleft_png[];
extern const u8 pbarcenter_png[];
extern const u8 pbarright_png[];
extern const u8 pbarlefts_png[];
extern const u8 pbarcenters_png[];
extern const u8 pbarrights_png[];
extern const u8 butauon_png[];
extern const u8 butauons_png[];
extern const u8 butauoff_png[];
extern const u8 butauoffs_png[];
extern const u8 butenon_png[];
extern const u8 butenons_png[];
extern const u8 butenoff_png[];
extern const u8 butenoffs_png[];
extern const u8 butjaon_png[];
extern const u8 butjaons_png[];
extern const u8 butjaoff_png[];
extern const u8 butjaoffs_png[];
extern const u8 butfron_png[];
extern const u8 butfrons_png[];
extern const u8 butfroff_png[];
extern const u8 butfroffs_png[];
extern const u8 butdeon_png[];
extern const u8 butdeons_png[];
extern const u8 butdeoff_png[];
extern const u8 butdeoffs_png[];
extern const u8 buteson_png[];
extern const u8 butesons_png[];
extern const u8 butesoff_png[];
extern const u8 butesoffs_png[];
extern const u8 butiton_png[];
extern const u8 butitons_png[];
extern const u8 butitoff_png[];
extern const u8 butitoffs_png[];
extern const u8 butnlon_png[];
extern const u8 butnlons_png[];
extern const u8 butnloff_png[];
extern const u8 butnloffs_png[];
extern const u8 butpton_png[];
extern const u8 butptons_png[];
extern const u8 butptoff_png[];
extern const u8 butptoffs_png[];
extern const u8 butruon_png[];
extern const u8 butruons_png[];
extern const u8 butruoff_png[];
extern const u8 butruoffs_png[];
extern const u8 butkoon_png[];
extern const u8 butkoons_png[];
extern const u8 butkooff_png[];
extern const u8 butkooffs_png[];
extern const u8 butzhcnon_png[];
extern const u8 butzhcnons_png[];
extern const u8 butzhcnoff_png[];
extern const u8 butzhcnoffs_png[];

CMenu::CMenu()
{
	m_aa = 0;
	m_thrdWorking = false;
	m_thrdStop = false;
	m_thrdProgress = 0.f;
	m_thrdStep = 0.f;
	m_thrdStepLen = 0.f;
	m_locked = false;
	m_favorites = false;
	m_thrdNetwork = false;
	m_mutex = 0;
	m_showtimer = 0;
	m_gameSoundThread = LWP_THREAD_NULL;
	m_soundThrdBusy = false;
	m_numCFVersions = 0;
	m_bgCrossFade = 0;
	m_bnrSndVol = 0;
	m_bnr_settings = true;
	m_directLaunch = false;
	m_exit = false;
	m_reload = false;
	m_gamesound_changed = false;
	m_video_playing = false;
	m_base_font = NULL;
	m_base_font_size = 0;
	m_wbf1_font = NULL;
	m_wbf2_font = NULL;
	m_current_view = COVERFLOW_USB;
	m_Emulator_boot = false;
	m_music_info = true;
	m_prevBg = NULL;
	m_nextBg = NULL;
	m_lqBg = NULL;
	m_use_sd_logging = false;
	m_use_wifi_gecko = false;
	init_network = false;
	vWii = false;
}

void CMenu::init()
{
	SoundHandle.Init();
	loadDefaultFont();

	m_aa = 3;
	CColor pShadowColor = CColor(0x3F000000);
	float pShadowX = 3.f;
	float pShadowY = 3.f;
	bool pShadowBlur = false;
	for(int chan = WPAD_MAX_WIIMOTES-2; chan >= 0; chan--)
	{
		m_cursor[chan].init(fmt("%s/%s", m_themeDataDir.c_str(), m_theme.getString("GENERAL", fmt("pointer%i", chan+1)).c_str()),
			m_vid.wide(), pShadowColor, pShadowX, pShadowY, pShadowBlur, chan);
		WPAD_SetVRes(chan, m_vid.width() + m_cursor[chan].width(), m_vid.height() + m_cursor[chan].height());
	}
	m_btnMgr.init();
	_buildMenus();

	LWP_MutexInit(&m_mutex, 0);
	m_btnMgr.setSoundVolume(255);
}

bool cleaned_up = false;

void CMenu::cleanup()
{
	if(cleaned_up)
		return;
	//gprintf("MEM1_freesize(): %i\nMEM2_freesize(): %i\n", MEM1_freesize(), MEM2_freesize());
	_cleanupDefaultFont();

	_stopSounds();
	_Theme_Cleanup();
	SoundHandle.Cleanup();
	soundDeinit();
	m_vid.cleanup();

	wiiLightOff();
	LWP_MutexDestroy(m_mutex);
	m_mutex = 0;

	cleaned_up = true;
	//gprintf(" \nMemory cleaned up\n");
	gprintf("MEM1_freesize(): %i\nMEM2_freesize(): %i\n", MEM1_freesize(), MEM2_freesize());
}

void CMenu::_Theme_Cleanup(void)
{
	for(TexSet::iterator texture = theme.texSet.begin(); texture != theme.texSet.end(); ++texture)
		TexHandle.Cleanup(texture->second);
	for(FontSet::iterator font = theme.fontSet.begin(); font != theme.fontSet.end(); ++font)
		font->second.ClearData();
	for(SoundSet::iterator sound = theme.soundSet.begin(); sound != theme.soundSet.end(); ++sound)
		sound->second->FreeMemory();
}

void CMenu::_buildMenus(void)
{
	// Default fonts
	theme.btnFont = _font(theme.fontSet, BUTTONFONT);
	theme.btnFontColor = m_theme.getColor("GENERAL", "button_font_color", 0xD0BFDFFF);

	theme.lblFont = _font(theme.fontSet, LABELFONT);
	theme.lblFontColor = m_theme.getColor("GENERAL", "label_font_color", 0xD0BFDFFF);

	theme.titleFont = _font(theme.fontSet, TITLEFONT);
	theme.titleFontColor = m_theme.getColor("GENERAL", "title_font_color", 0xFFFFFFFF);

	theme.txtFont = _font(theme.fontSet, TEXTFONT);
	theme.txtFontColor = m_theme.getColor("GENERAL", "text_font_color", 0xFFFFFFFF);

	theme.selsbtnFontColor = m_theme.getColor("GENERAL", "selsbtn_font_color", 0xFA5882FF);
	theme.selubtnFontColor = m_theme.getColor("GENERAL", "selubtn_font_color", 0xD0BFDFFF);

	// Default Sounds
	theme.clickSound	= _sound(theme.soundSet, click_wav, click_wav_size, "default_btn_click", false);
	theme.hoverSound	= _sound(theme.soundSet, hover_wav, hover_wav_size, "default_btn_hover", false);
	theme.cameraSound	= _sound(theme.soundSet, camera_wav, camera_wav_size, "default_camera", false);

	// Default textures
	TexHandle.fromPNG(theme.btnTexL, butleft_png);
	theme.btnTexL = _texture("GENERAL", "button_texture_left", theme.btnTexL); 
	TexHandle.fromPNG(theme.btnTexR, butright_png);
	theme.btnTexR = _texture("GENERAL", "button_texture_right", theme.btnTexR); 
	TexHandle.fromPNG(theme.btnTexC, butcenter_png);
	theme.btnTexC = _texture("GENERAL", "button_texture_center", theme.btnTexC); 
	TexHandle.fromPNG(theme.btnTexLS, butsleft_png);
	theme.btnTexLS = _texture("GENERAL", "button_texture_left_selected", theme.btnTexLS); 
	TexHandle.fromPNG(theme.btnTexRS, butsright_png);
	theme.btnTexRS = _texture("GENERAL", "button_texture_right_selected", theme.btnTexRS); 
	TexHandle.fromPNG(theme.btnTexCS, butscenter_png);
	theme.btnTexCS = _texture("GENERAL", "button_texture_center_selected", theme.btnTexCS); 

	TexHandle.fromPNG(theme.btnTexLH, buthleft_png);
	theme.btnTexLH = _texture("GENERAL", "button_texture_hlleft", theme.btnTexLH); 
	TexHandle.fromPNG(theme.btnTexRH, buthright_png);
	theme.btnTexRH = _texture("GENERAL", "button_texture_hlright", theme.btnTexRH); 
	TexHandle.fromPNG(theme.btnTexCH, buthcenter_png);
	theme.btnTexCH = _texture("GENERAL", "button_texture_hlcenter", theme.btnTexCH); 
	TexHandle.fromPNG(theme.btnTexLSH, buthsleft_png);
	theme.btnTexLSH = _texture("GENERAL", "button_texture_hlleft_selected", theme.btnTexLSH); 
	TexHandle.fromPNG(theme.btnTexRSH, buthsright_png);
	theme.btnTexRSH = _texture("GENERAL", "button_texture_hlright_selected", theme.btnTexRSH); 
	TexHandle.fromPNG(theme.btnTexCSH, buthscenter_png);
	theme.btnTexCSH = _texture("GENERAL", "button_texture_hlcenter_selected", theme.btnTexCSH); 

	TexHandle.fromPNG(theme.pbarTexL, pbarleft_png);
	theme.pbarTexL = _texture("GENERAL", "progressbar_texture_left", theme.pbarTexL);
	TexHandle.fromPNG(theme.pbarTexR, pbarright_png);
	theme.pbarTexR = _texture("GENERAL", "progressbar_texture_right", theme.pbarTexR);
	TexHandle.fromPNG(theme.pbarTexC, pbarcenter_png);
	theme.pbarTexC = _texture("GENERAL", "progressbar_texture_center", theme.pbarTexC);
	TexHandle.fromPNG(theme.pbarTexLS, pbarlefts_png);
	theme.pbarTexLS = _texture("GENERAL", "progressbar_texture_left_selected", theme.pbarTexLS);
	TexHandle.fromPNG(theme.pbarTexRS, pbarrights_png);
	theme.pbarTexRS = _texture("GENERAL", "progressbar_texture_right_selected", theme.pbarTexRS);
	TexHandle.fromPNG(theme.pbarTexCS, pbarcenters_png);
	theme.pbarTexCS = _texture("GENERAL", "progressbar_texture_center_selected", theme.pbarTexCS);
	TexHandle.fromPNG(theme.btnTexPlus, btnplus_png);
	theme.btnTexPlus = _texture("GENERAL", "plus_button_texture", theme.btnTexPlus);
	TexHandle.fromPNG(theme.btnTexPlusS, btnpluss_png);
	theme.btnTexPlusS = _texture("GENERAL", "plus_button_texture_selected", theme.btnTexPlusS);
	TexHandle.fromPNG(theme.btnTexMinus, btnminus_png);
	theme.btnTexMinus = _texture("GENERAL", "minus_button_texture", theme.btnTexMinus);
	TexHandle.fromPNG(theme.btnTexMinusS, btnminuss_png);
	theme.btnTexMinusS = _texture("GENERAL", "minus_button_texture_selected", theme.btnTexMinusS);

	// Default background
	TexHandle.fromJPG(theme.bg, background_jpg, background_jpg_size);
	TexHandle.fromJPG(m_mainBgLQ, background_jpg, background_jpg_size, GX_TF_CMPR, 64, 64);
	m_gameBg = theme.bg;
	m_gameBgLQ = m_mainBgLQ;

	if(IOS_GetType(512) != IOS_TYPE_STUB && IOS_GetType(513) != IOS_TYPE_STUB)
		vWii = true;
	_initMainMenu();
	_initChannelSettings();
}

void CMenu::_netInit(void)
{
	_initAsyncNetwork();
	while(net_get_status() == -EBUSY)
		usleep(100);
}

typedef struct
{
	string ext;
	u32 min;
	u32 max;
	u32 def;
	u32 res;
} FontHolder;

SFont CMenu::_font(CMenu::FontSet &fontSet, u32 fontSize, u32 lineSpacing, u32 weight, u32 index, const char *genKey)
{
	FontHolder fonts[3] = {{ "_size", 6u, 300u, fontSize, 0 }, { "_line_height", 6u, 300u, lineSpacing, 0 }, { "_weight", 1u, 32u, weight, 0 }};
	for(u32 i = 0; i < 3; i++)
	{
		string defValue = genKey;
		defValue += fonts[i].ext;
		if(fonts[i].res <= 0)
			fonts[i].res = (u32)m_theme.getInt("GENERAL", defValue);
		fonts[i].res = min(max(fonts[i].min, fonts[i].res <= 0 ? fonts[i].def : fonts[i].res), fonts[i].max);
	}

	// Try to find the same font with the same size
	CMenu::FontSet::iterator i = fontSet.find(CMenu::FontDesc(upperCase(genKey), fonts[0].res));
	if (i != fontSet.end()) return i->second;

	// TTF not found in memory, load it to create a new font
	SFont retFont;
	/* Fallback to default font */
	if(retFont.fromBuffer(m_base_font, m_base_font_size, fonts[0].res, fonts[1].res, fonts[2].res, index))
	{
		// Default font
		fontSet[CMenu::FontDesc(upperCase(genKey), fonts[0].res)] = retFont;
		return retFont;
	}
	return retFont;
}

vector<TexData> CMenu::_textures(const char *domain, const char *key)
{
	vector<TexData> textures;

	if (m_theme.loaded())
	{
		vector<string> filenames = m_theme.getStrings(domain, key);
		if (filenames.size() > 0)
		{
			for (vector<string>::iterator itr = filenames.begin(); itr != filenames.end(); itr++)
			{
				const string &filename = *itr;
				TexSet::iterator i = theme.texSet.find(filename);
				if (i != theme.texSet.end())
					textures.push_back(i->second);
				TexData tex;
				if(TexHandle.fromImageFile(tex, fmt("%s/%s", m_themeDataDir.c_str(), filename.c_str())) == TE_OK)
				{
					theme.texSet[filename] = tex;
					textures.push_back(tex);
				}
			}
		}
	}
	return textures;
}

TexData CMenu::_texture(const char *domain, const char *key, TexData &def, bool freeDef)
{
	string filename;

	if(m_theme.loaded())
	{
		/* Load from theme */
		filename = m_theme.getString(domain, key);
		if(!filename.empty())
		{
			TexSet::iterator i = theme.texSet.find(filename);
			if(i != theme.texSet.end())
				return i->second;
			/* Load from image file */
			TexData tex;
			if(TexHandle.fromImageFile(tex, fmt("%s/%s", m_themeDataDir.c_str(), filename.c_str())) == TE_OK)
			{
				if(freeDef && def.data != NULL)
				{
					free(def.data);
					def.data = NULL;
				}
				theme.texSet[filename] = tex;
				return tex;
			}
		}
	}
	/* Fallback to default */
	theme.texSet[filename] = def;
	return def;
}

// Only for loading defaults and GENERAL domains!!
GuiSound *CMenu::_sound(CMenu::SoundSet &soundSet, const u8 * snd, u32 len, const char *name, bool isAllocated)
{
	CMenu::SoundSet::iterator i = soundSet.find(upperCase(name));
	if(i == soundSet.end())
	{
		soundSet[upperCase(name)] = new GuiSound(snd, len, name, isAllocated);
		return soundSet[upperCase(name)];
	}
	return i->second;
}

//For buttons and labels only!!
GuiSound *CMenu::_sound(CMenu::SoundSet &soundSet, const char *name)
{
	if(strrchr(name, '/') != NULL)
		name = strrchr(name, '/') + 1;
	return soundSet[upperCase(name)];  // General/Default are already cached!
}

u16 CMenu::_textStyle(const char *domain, const char *key, u16 def)
{
	u16 textStyle = 0;
	string style(m_theme.getString(domain, key));
	if (style.empty()) return def;

	if (style.find_first_of("Cc") != string::npos)
		textStyle |= FTGX_JUSTIFY_CENTER;
	else if (style.find_first_of("Rr") != string::npos)
		textStyle |= FTGX_JUSTIFY_RIGHT;
	else
		textStyle |= FTGX_JUSTIFY_LEFT;
	if (style.find_first_of("Mm") != string::npos)
		textStyle |= FTGX_ALIGN_MIDDLE;
	else if (style.find_first_of("Bb") != string::npos)
		textStyle |= FTGX_ALIGN_BOTTOM;
	else
		textStyle |= FTGX_ALIGN_TOP;
	return textStyle;
}

s16 CMenu::_addButton(SFont font, const wstringEx &text, int x, int y, u32 width, u32 height, const CColor &color)
{
	SButtonTextureSet btnTexSet;
	btnTexSet.left = theme.btnTexL;
	btnTexSet.right = theme.btnTexR;
	btnTexSet.center = theme.btnTexC;
	btnTexSet.leftSel = theme.btnTexLS;
	btnTexSet.rightSel = theme.btnTexRS;
	btnTexSet.centerSel = theme.btnTexCS;

	CColor c(color);
	font = _font(theme.fontSet, BUTTONFONT);

	GuiSound *clickSound = _sound(theme.soundSet, theme.clickSound->GetName());
	GuiSound *hoverSound = _sound(theme.soundSet, theme.hoverSound->GetName());

	u16 btnPos = FTGX_JUSTIFY_LEFT | FTGX_ALIGN_TOP;
	if (btnPos & FTGX_JUSTIFY_RIGHT)
		x = m_vid.width() - x - width;
	if (btnPos & FTGX_ALIGN_BOTTOM)
		y = m_vid.height() - y - height;

	return m_btnMgr.addButton(font, text, x, y, width, height, c, btnTexSet, clickSound, hoverSound);
}

s16 CMenu::_addSelButton(const char *domain, SFont font, const wstringEx &text, int x, int y, u32 width, u32 height, const CColor &color)
{
	SButtonTextureSet btnTexSet;
	CColor c(color);

	c = m_theme.getColor(domain, "color", c);
	x = m_theme.getInt(domain, "x", x);
	y = m_theme.getInt(domain, "y", y);
	width = m_theme.getInt(domain, "width", width);
	height = m_theme.getInt(domain, "height", height);
	btnTexSet.left = _texture(domain, "texture_left", theme.btnTexLH, false);
	btnTexSet.right = _texture(domain, "texture_right", theme.btnTexRH, false);
	btnTexSet.center = _texture(domain, "texture_center", theme.btnTexCH, false);
	btnTexSet.leftSel = _texture(domain, "texture_left_selected", theme.btnTexLSH, false);
	btnTexSet.rightSel = _texture(domain, "texture_right_selected", theme.btnTexRSH, false);
	btnTexSet.centerSel = _texture(domain, "texture_center_selected", theme.btnTexCSH, false);

	font = _font(theme.fontSet, BUTTONFONT);

	GuiSound *clickSound = _sound(theme.soundSet, theme.clickSound->GetName());
	GuiSound *hoverSound = _sound(theme.soundSet, theme.hoverSound->GetName());

	u16 btnPos = _textStyle(domain, "elmstyle", FTGX_JUSTIFY_LEFT | FTGX_ALIGN_TOP);
	if (btnPos & FTGX_JUSTIFY_RIGHT)
		x = m_vid.width() - x - width;
	if (btnPos & FTGX_ALIGN_BOTTOM)
		y = m_vid.height() - y - height;

	return m_btnMgr.addButton(font, text, x, y, width, height, c, btnTexSet, clickSound, hoverSound);
}

s16 CMenu::_addPicButton(const char *domain, TexData &texNormal, TexData &texSelected, int x, int y, u32 width, u32 height)
{
	x = m_theme.getInt(domain, "x", x);
	y = m_theme.getInt(domain, "y", y);
	width = m_theme.getInt(domain, "width", width);
	height = m_theme.getInt(domain, "height", height);
	TexData tex1 = _texture(domain, "texture_normal", texNormal, false);
	TexData tex2 = _texture(domain, "texture_selected", texSelected, false);
	GuiSound *clickSound = _sound(theme.soundSet, theme.clickSound->GetName());
	GuiSound *hoverSound = _sound(theme.soundSet, theme.hoverSound->GetName());

	u16 btnPos = _textStyle(domain, "elmstyle", FTGX_JUSTIFY_LEFT | FTGX_ALIGN_TOP);
	if (btnPos & FTGX_JUSTIFY_RIGHT)
		x = m_vid.width() - x - width;
	if (btnPos & FTGX_ALIGN_BOTTOM)
		y = m_vid.height() - y - height;

	return m_btnMgr.addPicButton(tex1, tex2, x, y, width, height, clickSound, hoverSound);
}

s16 CMenu::_addTitle(SFont font, const wstringEx &text, int x, int y, u32 width, u32 height, const CColor &color, s16 style)
{
	CColor c(color);
	font = _font(theme.fontSet, TITLEFONT);

	u16 btnPos = FTGX_JUSTIFY_LEFT | FTGX_ALIGN_TOP;
	if (btnPos & FTGX_JUSTIFY_RIGHT)
		x = m_vid.width() - x - width;
	if (btnPos & FTGX_ALIGN_BOTTOM)
		y = m_vid.height() - y - height;

	return m_btnMgr.addLabel(font, text, x, y, width, height, c, style);
}

s16 CMenu::_addText(const char *domain, SFont font, const wstringEx &text, int x, int y, u32 width, u32 height, const CColor &color, s16 style)
{
	CColor c(color);

	c = m_theme.getColor(domain, "color", c);
	x = m_theme.getInt(domain, "x", x);
	y = m_theme.getInt(domain, "y", y);
	width = m_theme.getInt(domain, "width", width);
	height = m_theme.getInt(domain, "height", height);
	font = _font(theme.fontSet, TEXTFONT);
	style = _textStyle(domain, "style", style);

	u16 btnPos = _textStyle(domain, "elmstyle", FTGX_JUSTIFY_LEFT | FTGX_ALIGN_TOP);
	if (btnPos & FTGX_JUSTIFY_RIGHT)
		x = m_vid.width() - x - width;
	if (btnPos & FTGX_ALIGN_BOTTOM)
		y = m_vid.height() - y - height;

	return m_btnMgr.addLabel(font, text, x, y, width, height, c, style);
}

s16 CMenu::_addLabel(SFont font, const wstringEx &text, int x, int y, u32 width, u32 height, const CColor &color, s16 style)
{
	CColor c(color);
	font = _font(theme.fontSet, LABELFONT);

	u16 btnPos = FTGX_JUSTIFY_LEFT | FTGX_ALIGN_TOP;
	if (btnPos & FTGX_JUSTIFY_RIGHT)
		x = m_vid.width() - x - width;
	if (btnPos & FTGX_ALIGN_BOTTOM)
		y = m_vid.height() - y - height;

	return m_btnMgr.addLabel(font, text, x, y, width, height, c, style);
}

s16 CMenu::_addLabel(SFont font, const wstringEx &text, int x, int y, u32 width, u32 height, const CColor &color, s16 style, TexData &bg)
{
	CColor c(color);
	font = _font(theme.fontSet, BUTTONFONT);

	u16 btnPos = FTGX_JUSTIFY_LEFT | FTGX_ALIGN_TOP;
	if (btnPos & FTGX_JUSTIFY_RIGHT)
		x = m_vid.width() - x - width;
	if (btnPos & FTGX_ALIGN_BOTTOM)
		y = m_vid.height() - y - height;

	return m_btnMgr.addLabel(font, text, x, y, width, height, c, style, bg);
}

s16 CMenu::_addProgressBar(int x, int y, u32 width, u32 height)
{
	SButtonTextureSet btnTexSet;
	btnTexSet.left = theme.pbarTexL;
	btnTexSet.right = theme.pbarTexR;
	btnTexSet.center = theme.pbarTexC;
	btnTexSet.leftSel = theme.pbarTexLS;
	btnTexSet.rightSel = theme.pbarTexRS;
	btnTexSet.centerSel = theme.pbarTexCS;

	u16 btnPos = FTGX_JUSTIFY_LEFT | FTGX_ALIGN_TOP;
	if (btnPos & FTGX_JUSTIFY_RIGHT)
		x = m_vid.width() - x - width;
	if (btnPos & FTGX_ALIGN_BOTTOM)
		y = m_vid.height() - y - height;

	return m_btnMgr.addProgressBar(x, y, width, height, btnTexSet);
}

void CMenu::_setHideAnim(s16 id, const char *domain, int dx, int dy, float scaleX, float scaleY)
{
	dx = m_theme.getInt(domain, "effect_x", dx);
	dy = m_theme.getInt(domain, "effect_y", dy);
	scaleX = m_theme.getFloat(domain, "effect_scale_x", scaleX);
	scaleY = m_theme.getFloat(domain, "effect_scale_y", scaleY);

	int x, y;
	u32 width, height;
	m_btnMgr.getDimensions(id, x, y, width, height);

	u16 btnPos = _textStyle(domain, "elmstyle", FTGX_JUSTIFY_LEFT | FTGX_ALIGN_TOP);
	if (btnPos & FTGX_JUSTIFY_RIGHT)
	{
		dx = m_vid.width() - dx - width;
		scaleX = m_vid.width() - scaleX - width;
	}
	if (btnPos & FTGX_ALIGN_BOTTOM)
	{
		dy = m_vid.height() - dy - height;
		scaleY = m_vid.height() - scaleY - height;
	}

	m_btnMgr.hide(id, dx, dy, scaleX, scaleY, true);
}

void CMenu::_mainLoopCommon()
{
	m_btnMgr.tick();
	m_vid.prepare();
	m_vid.setup2DProjection(false, true);
	_updateBg();

	m_vid.setup2DProjection();
	_drawBg();
	m_btnMgr.draw();
	ScanInput();
	m_vid.render();

	if(Sys_Exiting())
		m_exit = true;
}

void CMenu::_setBg(const TexData &tex, const TexData &lqTex)
{
	/* Not setting same bg again */
	if(m_nextBg == &tex)
		return;
	m_lqBg = &lqTex;
	/* before setting new next bg set previous */
	if(m_nextBg != NULL)
		m_prevBg = m_nextBg;
	m_nextBg = &tex;
	m_bgCrossFade = 0xFF;
}

void CMenu::_updateBg(void)
{
	if(m_bgCrossFade == 0)
		return;
	m_bgCrossFade = max(0, (int)m_bgCrossFade - 14);

	Mtx modelViewMtx;
	GXTexObj texObj;
	GXTexObj texObj2;

	/* last pass so remove previous bg */
	if(m_bgCrossFade == 0)
		m_prevBg = NULL;

	GX_ClearVtxDesc();
	GX_SetNumTevStages(m_prevBg == NULL ? 1 : 2);
	GX_SetNumChans(0);
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GX_SetNumTexGens(m_prevBg == NULL ? 1 : 2);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_SetTexCoordGen(GX_TEXCOORD1, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_SetTevKColor(GX_KCOLOR0, CColor(m_bgCrossFade, 0xFF - m_bgCrossFade, 0, 0));
	GX_SetTevKColorSel(GX_TEVSTAGE0, GX_TEV_KCSEL_K0_R);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_TEXC, GX_CC_ZERO, GX_CC_KONST, GX_CC_ZERO);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
	GX_SetTevKColorSel(GX_TEVSTAGE1, GX_TEV_KCSEL_K0_G);
	GX_SetTevColorIn(GX_TEVSTAGE1, GX_CC_TEXC, GX_CC_ZERO, GX_CC_KONST, GX_CC_CPREV);
	GX_SetTevAlphaIn(GX_TEVSTAGE1, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
	GX_SetTevColorOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaOp(GX_TEVSTAGE1, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevOrder(GX_TEVSTAGE1, GX_TEXCOORD1, GX_TEXMAP1, GX_COLORNULL);
	GX_SetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	GX_SetAlphaUpdate(GX_FALSE);
	GX_SetCullMode(GX_CULL_NONE);
	GX_SetZMode(GX_DISABLE, GX_ALWAYS, GX_FALSE);
	guMtxIdentity(modelViewMtx);
	GX_LoadPosMtxImm(modelViewMtx, GX_PNMTX0);
	if(m_nextBg != NULL && m_nextBg->data != NULL)
	{
		GX_InitTexObj(&texObj, m_nextBg->data, m_nextBg->width, m_nextBg->height, m_nextBg->format, GX_CLAMP, GX_CLAMP, GX_FALSE);
		GX_LoadTexObj(&texObj, GX_TEXMAP0);
	}
	if(m_prevBg != NULL && m_prevBg->data != NULL)
	{
		GX_InitTexObj(&texObj2, m_prevBg->data, m_prevBg->width, m_prevBg->height, m_prevBg->format, GX_CLAMP, GX_CLAMP, GX_FALSE);
		GX_LoadTexObj(&texObj2, GX_TEXMAP1);
	}
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
	GX_Position3f32(0.f, 0.f, 0.f);
	GX_TexCoord2f32(0.f, 0.f);
	GX_Position3f32(640.f, 0.f, 0.f);
	GX_TexCoord2f32(1.f, 0.f);
	GX_Position3f32(640.f, 480.f, 0.f);
	GX_TexCoord2f32(1.f, 1.f);
	GX_Position3f32(0.f, 480.f, 0.f);
	GX_TexCoord2f32(0.f, 1.f);
	GX_End();
	GX_SetNumTevStages(1);
	m_curBg.width = 640;
	m_curBg.height = 480;
	m_curBg.format = GX_TF_RGBA8;
	m_curBg.maxLOD = 0;
	m_vid.renderToTexture(m_curBg, true);
}

void CMenu::_drawBg(void)
{
	Mtx modelViewMtx;
	GXTexObj texObj;

	GX_ClearVtxDesc();
	GX_SetNumTevStages(1);
	GX_SetNumChans(0);
	GX_SetVtxDesc(GX_VA_POS, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_POS, GX_POS_XYZ, GX_F32, 0);
	GX_SetVtxDesc(GX_VA_TEX0, GX_DIRECT);
	GX_SetVtxAttrFmt(GX_VTXFMT0, GX_VA_TEX0, GX_TEX_ST, GX_F32, 0);
	GX_SetNumTexGens(1);
	GX_SetTevColorIn(GX_TEVSTAGE0, GX_CC_ZERO, GX_CC_ZERO, GX_CC_ZERO, GX_CC_TEXC);
	GX_SetTevAlphaIn(GX_TEVSTAGE0, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO, GX_CA_ZERO);
	GX_SetTevColorOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTevAlphaOp(GX_TEVSTAGE0, GX_TEV_ADD, GX_TB_ZERO, GX_CS_SCALE_1, GX_TRUE, GX_TEVPREV);
	GX_SetTexCoordGen(GX_TEXCOORD0, GX_TG_MTX2x4, GX_TG_TEX0, GX_IDENTITY);
	GX_SetTevOrder(GX_TEVSTAGE0, GX_TEXCOORD0, GX_TEXMAP0, GX_COLORNULL);
	GX_SetBlendMode(GX_BM_NONE, GX_BL_SRCALPHA, GX_BL_INVSRCALPHA, GX_LO_CLEAR);
	GX_SetAlphaUpdate(GX_FALSE);
	GX_SetCullMode(GX_CULL_NONE);
	GX_SetZMode(GX_DISABLE, GX_ALWAYS, GX_FALSE);
	guMtxIdentity(modelViewMtx);
	GX_LoadPosMtxImm(modelViewMtx, GX_PNMTX0);
	GX_InitTexObj(&texObj, m_curBg.data, m_curBg.width, m_curBg.height, m_curBg.format, GX_CLAMP, GX_CLAMP, GX_FALSE);
	GX_LoadTexObj(&texObj, GX_TEXMAP0);
	GX_Begin(GX_QUADS, GX_VTXFMT0, 4);
	GX_Position3f32(0.f, 0.f, 0.f);
	GX_TexCoord2f32(0.f, 0.f);
	GX_Position3f32(640.f, 0.f, 0.f);
	GX_TexCoord2f32(1.f, 0.f);
	GX_Position3f32(640.f, 480.f, 0.f);
	GX_TexCoord2f32(1.f, 1.f);
	GX_Position3f32(0.f, 480.f, 0.f);
	GX_TexCoord2f32(0.f, 1.f);
	GX_End();
}

const wstringEx CMenu::_fmt(const char *key, const wchar_t *def)
{
	wstringEx ws = m_loc.getWString(m_curLanguage, key, def);
	if (checkFmt(def, ws)) return ws;
	return def;
}

void CMenu::_stopSounds(void)
{
	m_btnMgr.stopSounds();
}

bool CMenu::_loadFile(u8 * &buffer, u32 &size, const char *path, const char *file)
{
	size = 0;
	FILE *fp = fopen(file == NULL ? path : fmt("%s/%s", path, file), "rb");
	if (fp == NULL)
		return false;

	fseek(fp, 0, SEEK_END);
	u32 fileSize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	u8 *fileBuf = (u8*)MEM2_alloc(fileSize);
	if (fileBuf == NULL)
	{
		fclose(fp);
		return false;
	}
	if (fread(fileBuf, 1, fileSize, fp) != fileSize)
	{
		fclose(fp);
		return false;
	}
	fclose(fp);

	if(buffer != NULL)
		free(buffer);
	buffer = fileBuf;

	size = fileSize;
	return true;
}

void CMenu::_load_installed_cioses()
{
	gprintf("Loading cIOS map\n");
	_installed_cios[0] = 1;

	for(u8 slot = 200; slot < 254; slot++)
	{
		u8 base = 1;
		if(IOS_D2X(slot, &base))
		{
			gprintf("Found d2x base %u in slot %u\n", base, slot);
			_installed_cios[slot] = base;
		}
		else if(CustomIOS(IOS_GetType(slot)))
		{
			gprintf("Found cIOS in slot %u\n", slot);
			_installed_cios[slot] = slot;
		}
	}
}

void CMenu::_hideWaitMessage()
{
	m_vid.hideWaitMessage();
}

void CMenu::_showWaitMessage()
{
	m_vid.waitMessage(_textures("GENERAL", "waitmessage"), m_theme.getFloat("GENERAL", "waitmessage_delay", 0.f));
}

typedef struct map_entry
{
	char filename[8];
	u8 sha1[20];
} ATTRIBUTE_PACKED map_entry_t;

void CMenu::loadDefaultFont(void)
{
	if(m_base_font != NULL)
		return;

	u32 size = 0;
	bool retry = false;
	bool korean = (CONF_GetLanguage() == CONF_LANG_KOREAN);
	char ISFS_Filename[32] ATTRIBUTE_ALIGN(32);

	// Read content.map from ISFS
	strcpy(ISFS_Filename, "/shared1/content.map");
	u8 *content = ISFS_GetFile(ISFS_Filename, &size, -1);
	if(content == NULL)
		return;

	u32 items = size / sizeof(map_entry_t);
	//gprintf("Open content.map, size %d, items %d\n", size, items);
	map_entry_t *cm = (map_entry_t *)content;

retry:
	bool kor_font = (korean && !retry) || (!korean && retry);
	for(u32 i = 0; i < items; i++)
	{
		if(m_base_font != NULL)
			break;
		if(memcmp(cm[i].sha1, kor_font ? WIIFONT_HASH_KOR : WIIFONT_HASH, 20) == 0 && m_base_font == NULL)
		{
			sprintf(ISFS_Filename, "/shared1/%.8s.app", cm[i].filename);  //who cares about the few ticks more?
			u8 *u8_font_archive = ISFS_GetFile(ISFS_Filename, &size, -1);
			if(u8_font_archive != NULL)
			{
				const u8 *font_file = u8_get_file_by_index(u8_font_archive, 1, &size); // There is only one file in that app
				//gprintf("Extracted font: %d\n", size);
				m_base_font = (u8*)MEM1_lo_alloc(size);
				memcpy(m_base_font, font_file, size);
				DCFlushRange(m_base_font, size);
				m_base_font_size = size;
				free(u8_font_archive);
			}
		}
	}
	if(!retry && m_base_font == NULL)
	{
		retry = true;
		goto retry;
	}
	free(content);
}

void CMenu::_cleanupDefaultFont()
{
	MEM1_lo_free(m_base_font);
	m_base_font = NULL;
	m_base_font_size = 0;
}
