
#include <unistd.h>
#include <fstream>
#include <sys/stat.h>

#include "menu.hpp"
#include "channel/nand.hpp"
#include "gui/GameTDB.hpp"
#include "loader/cios.h"
#include "loader/nk.h"
#include "loader/wdvd.h"
#include "memory/mem2.hpp"

extern const u8 btnquit_png[];
extern const u8 btnquits_png[];

static inline int loopNum(int i, int s)
{
	return i < 0 ? (s - (-i % s)) % s : i % s;
}

void CMenu::_hideMain(bool instant)
{
	m_btnMgr.hide(m_mainBtnInstall, instant);
	m_btnMgr.hide(m_mainBtnUninstall, instant);
	m_btnMgr.hide(m_mainBtnQuit, instant);
	m_btnMgr.hide(m_mainLblInstall, instant);
}

void CMenu::_showMain(void)
{
	_hideWaitMessage();
	m_vid.set2DViewport(m_cfg.getInt("GENERAL", "tv_width", 640), m_cfg.getInt("GENERAL", "tv_height", 480),
	m_cfg.getInt("GENERAL", "tv_x", 0), m_cfg.getInt("GENERAL", "tv_y", 0));
	_setBg(m_gameBg, m_gameBgLQ);

	m_btnMgr.setText(m_mainTitle, fmt("WiiFlow Channel Installer [%s]", vWii ? "vWii" : "Wii"));
	m_btnMgr.show(m_mainBtnInstall);
	m_btnMgr.show(m_mainBtnUninstall);
	m_btnMgr.show(m_mainBtnQuit);
	m_btnMgr.show(m_mainLblInstall);
}

int CMenu::main(void)
{
	wstringEx curLetter;
	SetupInput(true);
	_showMain();
	m_btnMgr.show(m_mainTitle);
	while(!m_exit)
	{
		_mainLoopCommon();
		if(BTN_HOME_PRESSED)
			break;
		else if(BTN_A_PRESSED)
		{
			if(m_btnMgr.selected(m_mainBtnQuit))
				break;
			else if(m_btnMgr.selected(m_mainBtnInstall))
			{
				_hideMain(true);
				_Progress(true);
				_showMain();
			}
			else if(m_btnMgr.selected(m_mainBtnUninstall))
			{
				_hideMain(true);
				_Progress(false);
				_showMain();
			}
		}
	}
	_hideMain();
	m_btnMgr.hide(m_mainTitle, true);
	return 0;
}

void CMenu::_initMainMenu()
{
	TexData texQuit;
	TexData texQuitS;
	TexData emptyTex;

	m_mainBg = _texture("MAIN/BG", "texture", theme.bg, false);
	m_mainTitle = _addTitle(theme.titleFont, L"", 20, 30, 600, 60, theme.titleFontColor, FTGX_JUSTIFY_CENTER | FTGX_ALIGN_MIDDLE);
	m_mainBtnInstall = _addButton(theme.titleFont, L"", 72, 190, 496, 56, theme.titleFontColor);
	m_mainBtnUninstall = _addButton(theme.titleFont, L"", 72, 290, 496, 56, theme.titleFontColor);
	m_mainBtnQuit = _addButton(theme.titleFont, L"", 72, 390, 496, 56, theme.titleFontColor);
	m_mainLblInstall = _addLabel(theme.lblFont, L"", 40, 60, 560, 140, theme.lblFontColor, FTGX_JUSTIFY_LEFT | FTGX_ALIGN_MIDDLE);
	m_mainBar = _addProgressBar(40, 270, 560, 20);
	//
	_hideMain(true);
	_hideProgress(true);
	_textMain();
}

void CMenu::_textMain(void)
{
	m_btnMgr.setText(m_mainBtnInstall, "Install Channels");
	m_btnMgr.setText(m_mainBtnUninstall, "Uninstall Channels");
	m_btnMgr.setText(m_mainBtnQuit, "Quit Installer");
	m_btnMgr.setText(m_mainLblInstall, "Welcome to the WiiFlow Channel Installer!\nPlease select your options.");
}

void CMenu::_showProgress(void)
{
	m_btnMgr.setText(m_mainTitle, "Please wait...");
	m_btnMgr.show(m_mainBar);
}

void CMenu::_hideProgress(bool instant)
{
	m_btnMgr.hide(m_mainBar, instant);
}

void CMenu::_Progress(bool install)
{
	_showProgress();
	lwp_t thread = LWP_THREAD_NULL;
	u32 stack_size = 8192;
	u8 *stack = (u8*)MEM2_alloc(stack_size);
	m_thrdWorking = true;
	if(install)
		LWP_CreateThread(&thread, (void *(*)(void *))_InstallChannels, (void *)this, stack, stack_size, 64);
	else
		LWP_CreateThread(&thread, (void *(*)(void *))_UninstallChannels, (void *)this, stack, stack_size, 64);

	while(m_thrdWorking)
	{
		_mainLoopCommon();
		if(m_thrdMessageAdded)
		{
			LWP_MutexLock(m_mutex);
			m_thrdMessageAdded = false;
			m_btnMgr.setProgress(m_mainBar, m_thrdProgress);
			LWP_MutexUnlock(m_mutex);
		}
	}
	LWP_JoinThread(thread, NULL);
	thread = LWP_THREAD_NULL;
	_hideProgress();
}

void CMenu::addDiscProgress(int status, int total, void *user_data)
{
	CMenu &m = *(CMenu *)user_data;
	m.m_progress = total == 0 ? 0.f : (float)status / (float)total;
	// Don't synchronize too often
	if(m.m_progress - m.m_thrdProgress >= 0.01f)
	{
		LWP_MutexLock(m.m_mutex);
		m.m_thrdMessageAdded = true;
		m.m_thrdProgress = m.m_progress;
		LWP_MutexUnlock(m.m_mutex);
	}
}
