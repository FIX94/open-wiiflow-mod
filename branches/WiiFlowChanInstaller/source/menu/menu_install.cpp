/****************************************************************************
 * Copyright (C) 2013 FIX94
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/
#include <gccore.h>
#include "menu.hpp"
#include "loader/fs.h"
#include "rijndael.h"
#include "loader/sha1.h"
#include "memory/mem2.hpp"

u32 InstallStatus;
u32 UninstallStatus;

/* Normal FW */
extern const u8 normal_bnr_bin[];
extern const u32 normal_bnr_bin_size;
extern const u8 normal_fw_bin[];
extern const u32 normal_fw_bin_size;
/* Plugin Return To */
extern const u8 plugin_bnr_bin[];
extern const u32 plugin_bnr_bin_size;
extern const u8 plugin_fw_bin[];
extern const u32 plugin_fw_bin_size;
/* NAND Loaders */
extern const u8 nand_loader_wii_bin[];
extern const u32 nand_loader_wii_bin_size;
extern const u8 nand_loader_vwii_bin[];
extern const u32 nand_loader_vwii_bin_size;
/* Tickets and TMDs vWii */
extern const u8 normal_tik_vwii_bin[];
extern const u8 normal_tmd_vwii_bin[];
extern const u8 plugin_tik_vwii_bin[];
extern const u8 plugin_tmd_vwii_bin[];
/* Tickets and TMDs Wii */
extern const u8 normal_tik_wii_bin[];
extern const u8 normal_tmd_wii_bin[];
extern const u8 plugin_tik_wii_bin[];
extern const u8 plugin_tmd_wii_bin[];

u8 aesCommonKey[16] = {0xeb, 0xe4, 0x2a, 0x22, 0x5e, 0x85, 0x93, 0xe4,
						0x48, 0xd9, 0xc5, 0x45, 0x73, 0x81, 0xaa, 0xf7};

struct InstallableChannelData {
	InstallableChannelData() : App(NULL), AppSize(0) { };
	const u8 *App;
	u64 AppSize;
} ATTRIBUTE_PACKED;

struct InstallableChannel {
	InstallableChannel() : Tik(NULL), TikSize(0), TMD(NULL), TMDSize(0) { };
	const signed_blob *Tik;
	u32 TikSize;
	const signed_blob *TMD;
	u32 TMDSize;
	InstallableChannelData Data[3]; /* Our forwarders have 3 */
} ATTRIBUTE_PACKED;

InstallableChannel vWii_NormalForwarder;
InstallableChannel Wii_NormalForwarder;
InstallableChannel vWii_PluginForwarder;
InstallableChannel Wii_PluginForwarder;

void getTitleKey(const signed_blob *sTik, u8 *dst)
{
	u8 chTitleId[16] ATTRIBUTE_ALIGN(32);
	u8 chCryptedKey[16] ATTRIBUTE_ALIGN(32);
	u8 chDecryptedkey[16] ATTRIBUTE_ALIGN(32);
	tik *pTik=(tik*)SIGNATURE_PAYLOAD(sTik);
	memcpy(chCryptedKey,(u8*)&pTik->cipher_title_key,sizeof(chCryptedKey));
	memset(chDecryptedkey,0, sizeof(chDecryptedkey));
	memset(chTitleId, 0, sizeof(chTitleId));
	memcpy(chTitleId, &pTik->titleid, sizeof(pTik->titleid));
	aes_set_key(aesCommonKey);
	aes_decrypt(chTitleId,chCryptedKey,chDecryptedkey,sizeof(chCryptedKey));
	memcpy(dst, chDecryptedkey, sizeof(chDecryptedkey));
}

static void IncreaseStatus(u32 &Current, void *menu)
{
	Current++;
	CMenu &m = *(CMenu*)menu;
	m.addDiscProgress(min(Current, 10u), 10u, menu);
}

static void InstallChannel(InstallableChannel &CurrentChan, void *menu)
{
	const tmd *CurrentTMD = (const tmd*)SIGNATURE_PAYLOAD(CurrentChan.TMD);
	/* Too much variables */
	s32 fd = -1;
	s32 ret = 0;
	u64 read_content = 0;
	u64 written_content = 0;
	char ISFS_Path[ISFS_MAXPATH];
	u8 chTitleKey[16];
	u8 chCryptParameter[16];
	u8 chEncryptedBuffer[1024] ATTRIBUTE_ALIGN(0x20);
	u8 chDecryptedBuffer[1024] ATTRIBUTE_ALIGN(0x20);
	/* grab cert.sys */
	u32 certSize = 0;
	memset(&ISFS_Path, 0, ISFS_MAXPATH);
	strcpy(ISFS_Path, "/sys/cert.sys");
	signed_blob *certBuffer = (signed_blob*)ISFS_GetFile(ISFS_Path, &certSize, -1);
	if(certBuffer == NULL || certSize == 0)
		goto error;
	IncreaseStatus(InstallStatus, menu);
	/* Install Ticket and TMD */
	ret = ES_AddTicket(CurrentChan.Tik, CurrentChan.TikSize, certBuffer, certSize, NULL, 0);
	if(ret < 0)
		goto error;
	ret = ES_AddTitleStart(CurrentChan.TMD, CurrentChan.TMDSize, certBuffer, certSize, NULL, 0);
	if(ret < 0)
		goto error;
	IncreaseStatus(InstallStatus, menu);
	/* Begin our real App Install */
	getTitleKey(CurrentChan.Tik, chTitleKey);
	aes_set_key(chTitleKey);
	for(u16 i = 0; i < CurrentTMD->num_contents; i++)
	{
		if(CurrentChan.Data[i].AppSize == CurrentTMD->contents[i].size)
			ret = ES_AddContentStart(CurrentTMD->title_id, CurrentTMD->contents[i].cid);
		else
			ret = -1;
		/* Content should be alright, lets go */
		if(ret >= 0)
		{
			fd = ret;
			written_content = 0;
			memset(&chCryptParameter, 0, 16);
			memcpy(chCryptParameter, &CurrentTMD->contents[i].index, 2);
			while(written_content < CurrentTMD->contents[i].size)
			{
				read_content = min(CurrentTMD->contents[i].size - written_content, 0x400ull);
				memset(chDecryptedBuffer, 0, ALIGN(16, read_content));
				memcpy(chDecryptedBuffer, CurrentChan.Data[i].App + written_content, read_content);
				aes_encrypt(chCryptParameter, chDecryptedBuffer, chEncryptedBuffer, sizeof(chDecryptedBuffer));
				ret = ES_AddContentData(fd, chEncryptedBuffer, ALIGN(16, read_content));
				if(ret < 0)
					break;
				else
					written_content += ALIGN(16, read_content);
			}
			ret = ES_AddContentFinish(fd);
		}
		if(ret < 0)
			goto error;
		IncreaseStatus(InstallStatus, menu);
	}
	ret = ES_AddTitleFinish();
	if(ret < 0)
		goto error;
	IncreaseStatus(InstallStatus, menu);
	gprintf("Channel installed!\n");
	goto done;
error:
	gprintf("ERROR %i while installing channel!\n", ret);
	ES_AddTitleCancel();
done:
	if(certBuffer != NULL)
		free(certBuffer);
}

static void UninstallChannel(InstallableChannel &CurrentChan, void *menu)
{
	const tmd *CurrentTMD = (const tmd*)SIGNATURE_PAYLOAD(CurrentChan.TMD);
	tikview *views = NULL;
	u32 viewCnt = 0;
	s32 ret = ES_GetNumTicketViews(CurrentTMD->title_id, &viewCnt);
	if(ret < 0)
		goto error;
	views = (tikview*)MEM2_memalign(32, sizeof(tikview) * viewCnt);
	ret = ES_GetTicketViews(CurrentTMD->title_id, views, viewCnt);
	if(ret < 0)
		goto error;
	IncreaseStatus(UninstallStatus, menu);
	for(u32 cnt = 0; cnt < viewCnt; cnt++)
	{
		ret = ES_DeleteTicket(&views[cnt]);
		if(ret < 0)
			goto error;
		IncreaseStatus(UninstallStatus, menu);
	}
	ret = ES_DeleteTitleContent(CurrentTMD->title_id);
	if(ret < 0)
		goto error;
	IncreaseStatus(UninstallStatus, menu);
	ret = ES_DeleteTitle(CurrentTMD->title_id);
	if(ret < 0)
		goto error;
	IncreaseStatus(UninstallStatus, menu);
	gprintf("Channel uninstalled!\n");
	goto done;
error:
	gprintf("ERROR %i while uninstalling channel!\n", ret);
done:
	if(views != NULL)
		free(views);
}

void CMenu::_InstallChannels(void *menu)
{
	CMenu &m = *(CMenu*)menu;
	InstallStatus = 0;
	m.addDiscProgress(InstallStatus, 10, menu);
	if(m.vWii == true)
	{
		InstallChannel(vWii_NormalForwarder, menu);
		InstallChannel(vWii_PluginForwarder, menu);
	}
	else
	{
		InstallChannel(Wii_NormalForwarder, menu);
		InstallChannel(Wii_PluginForwarder, menu);
	}
	m.m_thrdWorking = false;
}

void CMenu::_UninstallChannels(void *menu)
{
	CMenu &m = *(CMenu*)menu;
	UninstallStatus = 0;
	m.addDiscProgress(UninstallStatus, 10, menu);
	if(m.vWii == true)
	{
		UninstallChannel(vWii_NormalForwarder, menu);
		UninstallChannel(vWii_PluginForwarder, menu);
	}
	else
	{
		UninstallChannel(Wii_NormalForwarder, menu);
		UninstallChannel(Wii_PluginForwarder, menu);
	}
	m.m_thrdWorking = false;
}

void CMenu::_initChannelSettings(void)
{
	/* Normal FW vWii */
	vWii_NormalForwarder.Tik = (const signed_blob*)normal_tik_vwii_bin;
	vWii_NormalForwarder.TikSize = SIGNED_TIK_SIZE(vWii_NormalForwarder.Tik);
	vWii_NormalForwarder.TMD = (const signed_blob*)normal_tmd_vwii_bin;
	vWii_NormalForwarder.TMDSize = SIGNED_TMD_SIZE(vWii_NormalForwarder.TMD);
	vWii_NormalForwarder.Data[0].App = normal_bnr_bin;
	vWii_NormalForwarder.Data[0].AppSize = normal_bnr_bin_size;
	vWii_NormalForwarder.Data[1].App = nand_loader_vwii_bin;
	vWii_NormalForwarder.Data[1].AppSize = nand_loader_vwii_bin_size;
	vWii_NormalForwarder.Data[2].App = normal_fw_bin;
	vWii_NormalForwarder.Data[2].AppSize = normal_fw_bin_size;
	/* Plugin FW vWii */
	vWii_PluginForwarder.Tik = (const signed_blob*)plugin_tik_vwii_bin;
	vWii_PluginForwarder.TikSize = SIGNED_TIK_SIZE(vWii_PluginForwarder.Tik);
	vWii_PluginForwarder.TMD = (const signed_blob*)plugin_tmd_vwii_bin;
	vWii_PluginForwarder.TMDSize = SIGNED_TMD_SIZE(vWii_PluginForwarder.TMD);
	vWii_PluginForwarder.Data[0].App = plugin_bnr_bin;
	vWii_PluginForwarder.Data[0].AppSize = plugin_bnr_bin_size;
	vWii_PluginForwarder.Data[1].App = nand_loader_vwii_bin;
	vWii_PluginForwarder.Data[1].AppSize = nand_loader_vwii_bin_size;
	vWii_PluginForwarder.Data[2].App = plugin_fw_bin;
	vWii_PluginForwarder.Data[2].AppSize = plugin_fw_bin_size;
	/* Normal FW Wii */
	Wii_NormalForwarder.Tik = (const signed_blob*)normal_tik_wii_bin;
	Wii_NormalForwarder.TikSize = SIGNED_TIK_SIZE(Wii_NormalForwarder.Tik);
	Wii_NormalForwarder.TMD = (const signed_blob*)normal_tmd_wii_bin;
	Wii_NormalForwarder.TMDSize = SIGNED_TMD_SIZE(Wii_NormalForwarder.TMD);
	Wii_NormalForwarder.Data[0].App = normal_bnr_bin;
	Wii_NormalForwarder.Data[0].AppSize = normal_bnr_bin_size;
	Wii_NormalForwarder.Data[1].App = nand_loader_wii_bin;
	Wii_NormalForwarder.Data[1].AppSize = nand_loader_wii_bin_size;
	Wii_NormalForwarder.Data[2].App = normal_fw_bin;
	Wii_NormalForwarder.Data[2].AppSize = normal_fw_bin_size;
	/* Plugin FW Wii */
	Wii_PluginForwarder.Tik = (const signed_blob*)plugin_tik_wii_bin;
	Wii_PluginForwarder.TikSize = SIGNED_TIK_SIZE(Wii_PluginForwarder.Tik);
	Wii_PluginForwarder.TMD = (const signed_blob*)plugin_tmd_wii_bin;
	Wii_PluginForwarder.TMDSize = SIGNED_TMD_SIZE(Wii_PluginForwarder.TMD);
	Wii_PluginForwarder.Data[0].App = plugin_bnr_bin;
	Wii_PluginForwarder.Data[0].AppSize = plugin_bnr_bin_size;
	Wii_PluginForwarder.Data[1].App = nand_loader_wii_bin;
	Wii_PluginForwarder.Data[1].AppSize = nand_loader_wii_bin_size;
	Wii_PluginForwarder.Data[2].App = plugin_fw_bin;
	Wii_PluginForwarder.Data[2].AppSize = plugin_fw_bin_size;
}
