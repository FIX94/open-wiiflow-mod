#ifndef __MENU_HPP
#define __MENU_HPP
//#define SHOWMEM
//#define SHOWMEMGECKO

#include <ogc/pad.h>
#include <vector>
#include <map>

#include "btnmap.h"
#include "gecko/gecko.hpp"
#include "gecko/wifi_gecko.hpp"
#include "gui/cursor.hpp"
#include "gui/fanart.hpp"
#include "gui/gui.hpp"
#include "loader/sys.h"
#include "music/gui_sound.h"
#include "wiiuse/wpad.h"

using namespace std;

class CMenu
{
public:
	CMenu();
	void init();
	void error(const wstringEx &msg);
	void terror(const char *key, const wchar_t *msg) { error(_fmt(key, msg)); }
	int main(void);
	void cleanup(void);
	void loadDefaultFont(void);
	void TempLoadIOS(int IOS = 0);
	u8 m_current_view;
	u8 enabledPluginPos;
	u8 enabledPluginsCount;
	char PluginMagicWord[9];
	static void addDiscProgress(int status, int total, void *user_data);
private:
	bool vWii;
	struct SZone
	{
		int x;
		int y;
		int w;
		int h;
		bool hide;
	};
	CCursor m_cursor[WPAD_MAX_WIIMOTES];
	CFanart m_fa;
	Config m_cfg;
	Config m_loc;
	Config m_cat;
	Config m_gcfg1;
	Config m_gcfg2;
	Config m_theme;
	Config m_titles;
	Config m_version;
	vector<string> m_homebrewArgs;
	u8 *m_base_font;
	u32 m_base_font_size;
	u8 *m_wbf1_font;
	u8 *m_wbf2_font;
	u8 m_aa;
	bool m_bnr_settings;
	bool m_directLaunch;
	bool m_locked;
	bool m_favorites;
	bool m_music_info;
	s16 m_showtimer;
	string m_curLanguage;

	u8 m_numCFVersions;

	string m_themeDataDir;
	string m_appDir;
	string m_dataDir;
	string m_pluginsDir;
	string m_customBnrDir;
	string m_picDir;
	string m_boxPicDir;
	string m_boxcPicDir;
	string m_cacheDir;
	string m_listCacheDir;
	string m_bnrCacheDir;
	string m_themeDir;
	string m_musicDir;
	string m_txtCheatDir;
	string m_cheatDir;
	string m_wipDir;
	string m_videoDir;
	string m_fanartDir;
	string m_screenshotDir;
	string m_settingsDir;
	string m_languagesDir;
	string m_DMLgameDir;
	string m_helpDir;
	
	/* Updates */
	char m_app_update_drive[6];
	const char* m_app_update_url;
	const char* m_data_update_url;
	string m_dol;
	string m_app_update_zip;
	u32 m_app_update_size;
	string m_data_update_zip;
	u32 m_data_update_size;
	string m_ver;
	/* End Updates */
	// 
	TexData m_curBg;
	const TexData *m_prevBg;
	const TexData *m_nextBg;
	const TexData *m_lqBg;
	u8 m_bgCrossFade;
	//
	TexData m_errorBg;
	TexData m_mainBg;
	TexData m_configBg;
	TexData m_config3Bg;
	TexData m_configScreenBg;
	TexData m_config4Bg;
	TexData m_configAdvBg;
	TexData m_configSndBg;
	TexData m_downloadBg;
	TexData m_gameBg;
	TexData m_codeBg;
	TexData m_aboutBg;
	TexData m_systemBg;
	TexData m_wbfsBg;
	TexData m_gameSettingsBg;
	TexData m_gameBgLQ;
	TexData m_mainBgLQ;
//Main Coverflow
	s16 m_mainBtnInstall;
	s16 m_mainBtnUninstall;
	s16 m_mainBtnQuit;
	s16 m_mainLblInstall;
	s16 m_mainBar;
	s16 m_mainTitle;
//
	static s8 _versionDownloaderInit(CMenu *m);
	static s8 _versionTxtDownloaderInit(CMenu *m);
	s8 _versionDownloader();
	s8 _versionTxtDownloader();

// NandEmulation
	string m_saveExtGameId;
	bool m_forceext;
	bool m_tempView;
	s32 m_partRequest;
// Zones
	SZone m_mainPrevZone;
	SZone m_mainNextZone;
	SZone m_mainButtonsZone;
	SZone m_mainButtonsZone2;
	SZone m_mainButtonsZone3;
	SZone m_gameButtonsZone;
	bool m_reload;

	WPADData *wd[WPAD_MAX_WIIMOTES];
	void LeftStick();
	u8 pointerhidedelay[WPAD_MAX_WIIMOTES];
	u16 stickPointer_x[WPAD_MAX_WIIMOTES];
	u16 stickPointer_y[WPAD_MAX_WIIMOTES];
	
	u8 m_wpadLeftDelay;
	u8 m_wpadDownDelay;
	u8 m_wpadRightDelay;
	u8 m_wpadUpDelay;
	u8 m_wpadADelay;
	//u8 m_wpadBDelay;

	u8 m_padLeftDelay;
	u8 m_padDownDelay;
	u8 m_padRightDelay;
	u8 m_padUpDelay;
	u8 m_padADelay;
	//u8 m_padBDelay;
	
	u32 wii_btnsPressed;
	u32 wii_btnsHeld;
	u32 gc_btnsPressed;
	u32 gc_btnsHeld;

	bool m_show_pointer[WPAD_MAX_WIIMOTES];
	float left_stick_angle[WPAD_MAX_WIIMOTES];
	float left_stick_mag[WPAD_MAX_WIIMOTES];
	float right_stick_angle[WPAD_MAX_WIIMOTES];
	float right_stick_mag[WPAD_MAX_WIIMOTES];
	float wmote_roll[WPAD_MAX_WIIMOTES];
	s32   right_stick_skip[WPAD_MAX_WIIMOTES];
	s32	  wmote_roll_skip[WPAD_MAX_WIIMOTES];
	bool  enable_wmote_roll;

	bool m_cfNeedsUpdate;

	void _mainLoopCommon();
	void SetupInput(bool reset_pos = false);
	void ScanInput(void);

	void ButtonsPressed(void);
	void ButtonsHeld(void);

	bool lStick_Up(void);
	bool lStick_Right(void);
	bool lStick_Down(void);
	bool lStick_Left(void);

	bool rStick_Up(void);
	bool rStick_Right(void);
	bool rStick_Down(void);
	bool rStick_Left(void);

	bool wRoll_Left(void);
	bool wRoll_Right(void);

	bool wii_btnRepeat(s64 btn);
	bool gc_btnRepeat(s64 btn);

	bool WPadIR_Valid(int chan);
	bool WPadIR_ANY(void);

	void ShowZone(SZone zone, bool &showZone);
	void ShowMainZone(void);
	void ShowMainZone2(void);
	void ShowMainZone3(void);
	void ShowPrevZone(void);
	void ShowNextZone(void);
	void ShowGameZone(void);
	bool ShowPointer(void);
	bool m_show_zone_main;
	bool m_show_zone_main2;
	bool m_show_zone_main3;
	bool m_show_zone_prev;
	bool m_show_zone_next;
	bool m_show_zone_game;

	volatile bool m_exit;
	volatile bool m_thrdStop;
	volatile bool m_thrdWorking;
	volatile bool m_thrdNetwork;
	float m_thrdStep;
	float m_thrdStepLen;
	string m_coverDLGameId;
	mutex_t m_mutex;
	wstringEx m_thrdMessage;
	volatile float m_thrdProgress;
	volatile float m_fileProgress;
	volatile bool m_thrdMessageAdded;
	volatile bool m_gameSelected;
	GuiSound m_gameSound;
	volatile bool m_soundThrdBusy;
	lwp_t m_gameSoundThread;
	bool m_gamesound_changed;
	u8 m_bnrSndVol;
	u8 m_max_categories;
	bool m_video_playing;

private:
	enum WBFS_OP
	{
		WO_ADD_GAME,
		WO_REMOVE_GAME,
		WO_FORMAT,
		WO_COPY_GAME,
	};
	typedef pair<string, u32> FontDesc;
	typedef map<FontDesc, SFont> FontSet;
	typedef map<string, TexData> TexSet;
	typedef map<string, GuiSound*> SoundSet;
	struct SThemeData
	{
		TexSet texSet;
		FontSet fontSet;
		SoundSet soundSet;
		SFont btnFont;
		SFont lblFont;
		SFont titleFont;
		SFont txtFont;
		CColor btnFontColor;
		CColor lblFontColor;
		CColor txtFontColor;
		CColor titleFontColor;
		CColor selubtnFontColor;
		CColor selsbtnFontColor;
		TexData bg;
		TexData btnTexL;
		TexData btnTexR;
		TexData btnTexC;
		TexData btnTexLS;
		TexData btnTexRS;
		TexData btnTexCS;
		TexData btnTexLH;
		TexData btnTexRH;
		TexData btnTexCH;
		TexData btnTexLSH;
		TexData btnTexRSH;
		TexData btnTexCSH;
		TexData btnAUOn;
		TexData btnAUOns;
		TexData btnAUOff;
		TexData btnAUOffs;
		TexData btnENOn;
		TexData btnENOns;
		TexData btnENOff;
		TexData btnENOffs;
		TexData btnJAOn;
		TexData btnJAOns;
		TexData btnJAOff;
		TexData btnJAOffs;
		TexData btnFROn;
		TexData btnFROns;
		TexData btnFROff;
		TexData btnFROffs;
		TexData btnDEOn;
		TexData btnDEOns;
		TexData btnDEOff;
		TexData btnDEOffs;
		TexData btnESOn;
		TexData btnESOns;
		TexData btnESOff;
		TexData btnESOffs;
		TexData btnITOn;
		TexData btnITOns;
		TexData btnITOff;
		TexData btnITOffs;
		TexData btnNLOn;
		TexData btnNLOns;
		TexData btnNLOff;
		TexData btnNLOffs;
		TexData btnPTOn;
		TexData btnPTOns;
		TexData btnPTOff;
		TexData btnPTOffs;
		TexData btnRUOn;
		TexData btnRUOns;
		TexData btnRUOff;
		TexData btnRUOffs;
		TexData btnKOOn;
		TexData btnKOOns;
		TexData btnKOOff;
		TexData btnKOOffs;
		TexData btnZHCNOn;
		TexData btnZHCNOns;
		TexData btnZHCNOff;
		TexData btnZHCNOffs;
		TexData checkboxoff;
		TexData checkboxoffs;
		TexData checkboxon;
		TexData checkboxons;
		TexData checkboxHid;
		TexData checkboxHids;
		TexData checkboxReq;
		TexData checkboxReqs;
		TexData pbarTexL;
		TexData pbarTexR;
		TexData pbarTexC;
		TexData pbarTexLS;
		TexData pbarTexRS;
		TexData pbarTexCS;
		TexData btnTexPlus;
		TexData btnTexPlusS;
		TexData btnTexMinus;
		TexData btnTexMinusS;
		GuiSound *clickSound;
		GuiSound *hoverSound;
		GuiSound *cameraSound;
	};
	SThemeData theme;
	struct SCFParamDesc
	{
		enum
		{
			PDT_EMPTY,
			PDT_FLOAT,
			PDT_V3D,
			PDT_COLOR,
			PDT_BOOL,
			PDT_INT, 
			PDT_TXTSTYLE,
		} paramType[4];
		enum
		{
			PDD_BOTH,
			PDD_NORMAL, 
			PDD_SELECTED,
		} domain;
		bool scrnFmt;
		const char name[32];
		const char valName[4][64];
		const char key[4][48];
		float step[4];
		float minMaxVal[4][2];
	};
	// 
	static void _InstallChannels(void *menu);
	static void _UninstallChannels(void *menu);
	//
	void _initMainMenu(void);
	void _initChannelSettings(void);
	//
	void _textMain(void);
	//
	void _refreshBoot(void);
	//
	void _hideMain(bool instant = false);
	void _hideProgress(bool instant = false);
	//
	void _showMain(void);
	void _showProgress(void);
	//
	void _updateSourceBtns(void);
	void _updatePluginText(void);
	void _updatePluginCheckboxes(void);
	void _updateCheckboxes(void);
	void _checkForSinglePlugin(void);
	void _getIDCats(void);
	void _setIDCats(void);
	void _setBg(const TexData &tex, const TexData &lqTex);
	void _updateBg(void);
	void _drawBg(void);
	void _showNandEmu(void);
	//
	void _config(int page);
	int _configCommon(void);
	int _config1(void);
	int _config3(void);
	int _configScreen(void);
	int _config4(void);
	int _configAdv(void);
	int _configSnd(void);
	int _NandEmuCfg(void);
	int _AutoCreateNand(void);
	int _AutoExtractSave(string gameId);
	int _FlashSave(string gameId);
	enum configPageChanges
	{
		CONFIG_PAGE_DEC = -1,
		CONFIG_PAGE_NO_CHANGE = 0,
		CONFIG_PAGE_INC = 1,
		CONFIG_PAGE_BACK,
	};
	void _cfNeedsUpdate(void);
	void _game(bool launch = false);
	void _download(string gameId = string());
	void _code(void);
	void _about(bool help = false);
	bool _wbfsOp(WBFS_OP op);
	void _cfTheme(void);
	void _system(void);
	void _gameinfo(void);
	void _gameSettings(void);
	void _CheatSettings();
	void _PluginSettings();
	void _CategorySettings(bool fromGameSet = false);
	void _Progress(bool install);
public:
	void directlaunch(const char *GameID);
private:
	bool m_use_wifi_gecko;
	bool m_use_sd_logging;
	bool init_network;
	void _netInit();
	bool _loadFile(u8 * &buffer, u32 &size, const char *path, const char *file);
	int _loadIOS(u8 ios, int userIOS, string id, bool RealNAND_Channels = false);
	void _setAA(int aa);
	void _loadCFCfg();
	void _loadCFLayout(int version, bool forceAA = false, bool otherScrnFmt = false);
	Vector3D _getCFV3D(const string &domain, const string &key, const Vector3D &def, bool otherScrnFmt = false);
	int _getCFInt(const string &domain, const string &key, int def, bool otherScrnFmt = false);
	float _getCFFloat(const string &domain, const string &key, float def, bool otherScrnFmt = false);
	void _cfParam(bool inc, int i, const SCFParamDesc &p, int cfVersion, bool wide);
	void _buildMenus(void);
	void _cleanupDefaultFont();
	void _Theme_Cleanup();
	const char *_getId(void);
	const char *_domainFromView(void);
	const char *_cfDomain(bool selected = false);
	int MIOSisDML();
	void RemoveCover(const char *id);
	SFont _font(CMenu::FontSet &fontSet, u32 fontSize, u32 lineSpacing, u32 weight, u32 index, const char *genKey);
	TexData _texture(const char *domain, const char *key, TexData &def, bool freeDef = true);
	vector<TexData> _textures(const char *domain, const char *key);
	void _showWaitMessage();
public:
	void _hideWaitMessage();
	bool m_Emulator_boot;
private:
	GuiSound *_sound(CMenu::SoundSet &soundSet, const u8 * snd, u32 len, const char *name, bool isAllocated);
	GuiSound *_sound(CMenu::SoundSet &soundSet, const char *name);
	u16 _textStyle(const char *domain, const char *key, u16 def);
	s16 _addButton(SFont font, const wstringEx &text, int x, int y, u32 width, u32 height, const CColor &color);
	s16 _addSelButton(const char *domain, SFont font, const wstringEx &text, int x, int y, u32 width, u32 height, const CColor &color);
	s16 _addPicButton(const char *domain, TexData &texNormal, TexData &texSelected, int x, int y, u32 width, u32 height);
	s16 _addTitle(SFont font, const wstringEx &text, int x, int y, u32 width, u32 height, const CColor &color, s16 style);
	s16 _addText(const char *domain, SFont font, const wstringEx &text, int x, int y, u32 width, u32 height, const CColor &color, s16 style);
	s16 _addLabel(SFont font, const wstringEx &text, int x, int y, u32 width, u32 height, const CColor &color, s16 style);
	s16 _addLabel(SFont font, const wstringEx &text, int x, int y, u32 width, u32 height, const CColor &color, s16 style, TexData &bg);
	s16 _addProgressBar(int x, int y, u32 width, u32 height);
	void _setHideAnim(s16 id, const char *domain, int dx, int dy, float scaleX, float scaleY);
	// 
	const wstringEx _t(const char *key, const wchar_t *def = L"") { return m_loc.getWString(m_curLanguage, key, def); }
	const wstringEx _fmt(const char *key, const wchar_t *def);
	wstringEx _getNoticeTranslation(int sorting, wstringEx curLetter);
	// 
	void _setThrdMsg(const wstringEx &msg, float progress);
	void _setDumpMsg(const wstringEx &msg, float progress, float fileprog);
	int _coverDownloader(bool missingOnly);
	static int _coverDownloaderAll(CMenu *m);
	static int _coverDownloaderMissing(CMenu *m);
	static bool _downloadProgress(void *obj, int size, int position);
	static int _gametdbDownloader(CMenu *m);
	int _gametdbDownloaderAsync();

	static s32 _networkComplete(s32 result, void *usrData);
	void _initAsyncNetwork();
	bool _isNetworkAvailable();
	int _initNetwork();
	void LoadView(void);
	void _getGrabStatus(void);
	static void _Messenger(int message, int info, char *cinfo, void *user_data);
	static void _ShowProgress(int dumpstat, int dumpprog, int filestat, int fileprog, int files, int folders, const char *tmess, void *user_data);
	static int _gameInstaller(void *obj);	
	static int _GCgameInstaller(void *obj);
	static int _GCcopyGame(void *obj);
	float m_progress;
	float m_fprogress;
	int m_fileprog;
	int m_filesize;
	int m_dumpsize;
	int m_filesdone;
	int m_foldersdone;
	int m_nandexentry;
	wstringEx _optBoolToString(int b);
	void _stopSounds(void);
	static int _NandDumper(void *obj);
	static int _NandFlasher(void *obj);
	int _FindEmuPart(string &emuPath, bool searchvalid);
	bool _checkSave(string id, bool nand);
	bool _TestEmuNand(int epart, const char *path, bool indept);	

	static u32 _downloadCheatFileAsync(void *obj);

	void _playGameSound(void);
	void CheckGameSoundThread(void);
	static void _gameSoundThread(CMenu *m);

	static void _load_installed_cioses();

	struct SOption { const char id[10]; const wchar_t text[16]; };
	static const string _translations[23];
	static const SOption _languages[11];

	static const SOption _GlobalVideoModes[6];
	static const SOption _VideoModes[7];
	
	static const SOption _GlobalDMLvideoModes[6];
	static const SOption _GlobalGClanguages[7];
	static const SOption _DMLvideoModes[7];
	static const SOption _GClanguages[8];

	static const SOption _NandEmu[2];
	static const SOption _SaveEmu[5];
	static const SOption _GlobalSaveEmu[4];
	static const SOption _AspectRatio[3];
	static const SOption _NMM[4];
	static const SOption _NoDVD[3];
	static const SOption _GCLoader[3];
	static const SOption _vidModePatch[4];
	static const SOption _hooktype[8];
	static const SOption _exitTo[5];
	static map<u8, u8> _installed_cios;
	typedef map<u8, u8>::iterator CIOSItr;
	static int _version[9];
	static const SCFParamDesc _cfParams[];
	static const int _nbCfgPages;
	static const u32 SVN_REV_NUM;
};

#define ARRAY_SIZE(a)		(sizeof a / sizeof a[0])

#endif // !defined(__MENU_HPP)
