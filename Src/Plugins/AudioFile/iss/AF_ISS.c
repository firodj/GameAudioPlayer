/*
 * ISS plug-in source code
 * (FunCom music/sfx/speech: The Longest Journey)
 *
 * Copyright (C) 2001 ANX Software
 * E-mail: son_of_the_north@mail.ru
 *
 * Author: Valery V. Anisimovsky (son_of_the_north@mail.ru)
 *
 * IMA ADPCM decoder corrections were pointed out by Peter Pawlowski:
 * E-mail: peterpw666@hotmail.com, piotrpw@polbox.com
 * http://www.geocities.com/pp_666/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <stdlib.h>

#include <windows.h>

#include "..\..\..\GAP\Messages.h"
#include "..\AFPlugin.h"
#include "..\..\ResourceFile\RFPlugin.h"

#include "AF_ISS.h"
#include "resource.h"

typedef struct tagData
{
	WORD   channels,bits,align;
	DWORD  rate,blocksize,headersize;
	LONG   Index1,Index2;
	LONG   Cur_Sample1,Cur_Sample2;
	char  *buffer;
} Data;

#define LPData(f) ((Data*)((f)->afData))

AFPlugin Plugin;

BOOL opCheckBlockSize,
	 opCheckChannels,
	 opCheckUnknown1,
	 opCheckRate,
	 opCheckUnknown2,
	 opCheckVersion,
	 opInitName,
	 opUseClipping,
	 opUseEnhancement;

short Index_Adjust[]=
{
    -1,
    -1,
    -1,
    -1,
     2,
     4,
     6,
     8,
	-1,
    -1,
    -1,
    -1,
     2,
     4,
     6,
     8
};

short Step_Table[]=
{
    7,	   8,	  9,	 10,	11,    12,     13,    14,    16,
    17,    19,	  21,	 23,	25,    28,     31,    34,    37,
    41,    45,	  50,	 55,	60,    66,     73,    80,    88,
    97,    107,   118,	 130,	143,   157,    173,   190,   209,
    230,   253,   279,	 307,	337,   371,    408,   449,   494,
    544,   598,   658,	 724,	796,   876,    963,   1060,  1166,
    1282,  1411,  1552,  1707,	1878,  2066,   2272,  2499,  2749,
    3024,  3327,  3660,  4026,	4428,  4871,   5358,  5894,  6484,
    7132,  7845,  8630,  9493,	10442, 11487,  12635, 13899, 15289,
    16818, 18500, 20350, 22385, 24623, 27086,  29794, 32767
};

#define NIBBLE(b,n) ( (n) ? (((b)&0xF0)>>4) : ((b)&0x0F) )

#define LoadOptionBool(str) ((BOOL)lstrcmpi(str,"off"))

void __stdcall Init(void)
{
	char str[40];

	DisableThreadLibraryCalls(Plugin.hDllInst);

    if (Plugin.szINIFileName==NULL)
    {
		opCheckBlockSize=FALSE;
		opCheckChannels=TRUE;
		opCheckUnknown1=FALSE;
		opCheckRate=TRUE;
		opCheckUnknown2=FALSE;
		opCheckVersion=TRUE;
		opInitName=TRUE;
		opUseClipping=TRUE;
		opUseEnhancement=FALSE;
		return;
    }
	GetPrivateProfileString(Plugin.Description,"CheckBlockSize","off",str,40,Plugin.szINIFileName);
    opCheckBlockSize=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"CheckChannels","on",str,40,Plugin.szINIFileName);
    opCheckChannels=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"CheckUnknown1","off",str,40,Plugin.szINIFileName);
    opCheckUnknown1=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"CheckRate","on",str,40,Plugin.szINIFileName);
    opCheckRate=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"CheckUnknown2","off",str,40,Plugin.szINIFileName);
    opCheckUnknown2=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"CheckVersion","on",str,40,Plugin.szINIFileName);
    opCheckVersion=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"InitName","on",str,40,Plugin.szINIFileName);
    opInitName=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"UseClipping","on",str,40,Plugin.szINIFileName);
    opUseClipping=LoadOptionBool(str);
	GetPrivateProfileString(Plugin.Description,"UseEnhancement","off",str,40,Plugin.szINIFileName);
    opUseEnhancement=LoadOptionBool(str);
}

#define SaveOptionBool(b) ((b)?"on":"off")

void __stdcall Quit(void)
{
    if (Plugin.szINIFileName==NULL)
		return;

	WritePrivateProfileString(Plugin.Description,"CheckBlockSize",SaveOptionBool(opCheckBlockSize),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"CheckChannels",SaveOptionBool(opCheckChannels),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"CheckUnknown1",SaveOptionBool(opCheckUnknown1),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"CheckRate",SaveOptionBool(opCheckRate),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"CheckUnknown2",SaveOptionBool(opCheckUnknown2),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"CheckVersion",SaveOptionBool(opCheckVersion),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"InitName",SaveOptionBool(opInitName),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"UseClipping",SaveOptionBool(opUseClipping),Plugin.szINIFileName);
	WritePrivateProfileString(Plugin.Description,"UseEnhancement",SaveOptionBool(opUseEnhancement),Plugin.szINIFileName);
}

void ReadUntilSpace(FSHandle* file, char *str, DWORD size)
{
	char  ch;
	DWORD i=0,read;

	while (
			(!(Plugin.fsh->EndOfFile(file))) &&
			(i<size-1)
		  )
	{
		Plugin.fsh->ReadFile(file,&ch,sizeof(ch),&read);
		if (
			(read<sizeof(ch)) ||
			(ch==' ')
		   )
			break;
		str[i++]=ch;
	}

	str[i]=0;
}

BOOL ReadISSHeader
(
	FSHandle* file,
	DWORD* blocksize,
	char*  id,
	DWORD* rate,
	WORD*  channels,
	WORD*  bits,
	char*  version,
	DWORD* outsize,
	DWORD* unknown1,
	DWORD* unknown2,
	DWORD* size
)
{
    DWORD tmp;
	char  str[256],*end;

    if (file==NULL)
		return FALSE;

    Plugin.fsh->SetFilePointer(file,0,FILE_BEGIN);

	ReadUntilSpace(file,str,sizeof(str));
	if (lstrcmp(str,IDSTR_ISS))
		return FALSE;

	ReadUntilSpace(file,str,sizeof(str));
	*blocksize=strtoul(str,&end,10);
	if (
		(opCheckBlockSize) &&
		(*blocksize!=512) &&
		(*blocksize!=2048)
	   )
		return FALSE;

	ReadUntilSpace(file,id,sizeof(str));

	ReadUntilSpace(file,str,sizeof(str));
	*outsize=strtoul(str,&end,10);

	ReadUntilSpace(file,str,sizeof(str));
	tmp=strtoul(str,&end,10);
	if (
		(opCheckChannels) &&
		(tmp>1)
	   )
		return FALSE;
	*channels=(tmp)?2:1;

	ReadUntilSpace(file,str,sizeof(str));
	*unknown1=strtoul(str,&end,10);
	if (
		(opCheckUnknown1) &&
		((*unknown1)!=1)
	   )
		return FALSE;

	ReadUntilSpace(file,str,sizeof(str));
	tmp=strtoul(str,&end,10);
	if (tmp==0) // ???
		return FALSE;
	*rate=44100/tmp;
	if (
		(opCheckRate) &&
		(
		 (*rate<4000) ||
		 (*rate>96000)
		)
	   )
		return FALSE;

	ReadUntilSpace(file,str,sizeof(str));
	*unknown2=strtoul(str,&end,10);
	if (
		(opCheckUnknown2) &&
		((*unknown2)!=0)
	   )
		return FALSE;

	ReadUntilSpace(file,version,sizeof(str));
	if (
		(opCheckVersion) &&
		(lstrcmp(version,IDSTR_VER1000))
	   )
		return FALSE;

	ReadUntilSpace(file,str,sizeof(str));
	*size=strtoul(str,&end,10);

	*bits=16; // ???

	return TRUE;
}

BOOL __stdcall InitPlayback(FSHandle* f, DWORD* rate, WORD* channels, WORD* bits)
{
	char  version[16],id[256];
	DWORD size,outsize,blocksize,unknown1,unknown2;

    if (f==NULL)
		return FALSE;

    if (!ReadISSHeader(f,&blocksize,id,rate,channels,bits,version,&outsize,&unknown1,&unknown2,&size))
    {
		SetLastError(PRIVEC(IDS_NOTOURFILE));
		return FALSE;
    }
	f->afData=GlobalAlloc(GPTR,sizeof(Data));
	if (f->afData==NULL)
	{
		SetLastError(PRIVEC(IDS_NOMEM));
		return FALSE;
	}
	blocksize-=(*channels)*sizeof(ISSBlockHeader);
	LPData(f)->buffer=(char*)GlobalAlloc(GPTR,blocksize);
	if (LPData(f)->buffer==NULL)
	{
		GlobalFree(f->afData);
		SetLastError(PRIVEC(IDS_NOBUFFER));
		return FALSE;
	}
	LPData(f)->Index1=0L;
	LPData(f)->Index2=0L;
	LPData(f)->Cur_Sample1=0L;
	LPData(f)->Cur_Sample2=0L;
	LPData(f)->align=(*channels)*((*bits)/8);
	LPData(f)->rate=*rate;
	LPData(f)->channels=*channels;
	LPData(f)->bits=*bits;
	LPData(f)->blocksize=blocksize;
	LPData(f)->headersize=Plugin.fsh->GetFilePointer(f);

    return TRUE;
}

BOOL __stdcall ShutdownPlayback(FSHandle* f)
{
    if (f==NULL)
		return TRUE;
    if (LPData(f)==NULL)
		return TRUE;

    return (
			(GlobalFree(LPData(f)->buffer)==NULL) &&
			(GlobalFree(f->afData)==NULL)
		   );
}

void Step_IMA_ADPCM(BYTE code, long *pindex, long *pcur_sample)
{
	long delta;

	delta=Step_Table[*pindex]>>3;
	if (code & 4)
		delta+=Step_Table[*pindex];
	if (code & 2)
		delta+=Step_Table[*pindex]>>1;
	if (code & 1)
		delta+=Step_Table[*pindex]>>2;
	if (code & 8)
		(*pcur_sample)-=delta;
	else
		(*pcur_sample)+=delta;
	(*pindex)+=Index_Adjust[code];
	if ((*pindex)<0)
		*pindex=0;
    else if ((*pindex)>88)
	    *pindex=88;
}

void Clip16BitSample(long *sample)
{
	if ((*sample)>32767)
		(*sample)=32767;
    else if ((*sample)<-32768)
		(*sample)=-32768;
}

void Enhance16BitSample(long* sample)
{
	if (*sample>0)
		(*sample)--;
	else if (*sample)
		(*sample)++;
}

AFile* __stdcall CreateNodeForID(HWND hwnd, RFHandle* f, DWORD ipattern, DWORD pos, DWORD *newpos)
{
    AFile   *node;
	FSHandle fs;
	char	 version[16],id[256];
	WORD	 channels,bits;
	DWORD	 size,outsize,blocksize,unknown1,unknown2,rate;

    if (f->rfPlugin==NULL)
		return NULL;

	switch (ipattern)
	{
		case 0: // IMA_ADPCM_Sound
			SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"Verifying ISS file...");
			RFPLUGIN(f)->SetFilePointer(f,pos,FILE_BEGIN);
			fs.rf=f;
			fs.start=pos;
			fs.length=1000;
			fs.node=NULL;
			if (!ReadISSHeader(&fs,&blocksize,id,&rate,&channels,&bits,version,&outsize,&unknown1,&unknown2,&size))
				return NULL;
			SendMessage(hwnd,WM_GAP_SHOWPROGRESSSTATE,0,(LPARAM)"ISS file correct.");
			break;
		default: // ???
			return NULL;
	}
    node=(AFile*)GlobalAlloc(GPTR,sizeof(AFile));
    node->fsStart=pos;
    node->fsLength=(RFPLUGIN(f)->GetFilePointer(f)-pos)+size;
	*newpos=node->fsStart+node->fsLength;
    lstrcpy(node->afDataString,"");
	lstrcpy(node->afName,(opInitName)?id:"");

    return node;
}

AFile* __stdcall CreateNodeForFile(FSHandle* f, const char* rfName, const char* rfDataString)
{
    AFile *node;
	char   version[16],id[256];
	WORD   channels,bits;
	DWORD  size,outsize,blocksize,unknown1,unknown2,rate;

    if (f==NULL)
		return NULL;

    if (!ReadISSHeader(f,&blocksize,id,&rate,&channels,&bits,version,&outsize,&unknown1,&unknown2,&size))
		return NULL;
    node=(AFile*)GlobalAlloc(GPTR,sizeof(AFile));
    node->fsStart=0;
    node->fsLength=Plugin.fsh->GetFileSize(f);
    lstrcpy(node->afDataString,"");
	lstrcpy(node->afName,(opInitName)?id:"");

    return node;
}

DWORD __stdcall GetTime(AFile* node)
{
    FSHandle* f;
	char   version[16],id[256];
	WORD   channels,bits;
	DWORD  size,outsize,blocksize,unknown1,unknown2,rate;

    if (node==NULL)
		return -1;

    if ((f=Plugin.fsh->OpenFile(node))==NULL)
		return -1;
    if (!ReadISSHeader(f,&blocksize,id,&rate,&channels,&bits,version,&outsize,&unknown1,&unknown2,&size))
	{
		Plugin.fsh->CloseFile(f);
		return -1;
    }
	Plugin.fsh->CloseFile(f);

	return MulDiv(1000,outsize,rate);
}

DWORD Decode_IMA_ADPCM(FSHandle* f, char* chunk, DWORD size, short* buff)
{
	DWORD i;
	BYTE  Code;

	if (LPData(f)->channels==2)
	{
		for (i=0;i<size;i++)
		{
			Code=NIBBLE(chunk[i],1);
			Step_IMA_ADPCM(Code,&(LPData(f)->Index1),&(LPData(f)->Cur_Sample1));
			if (opUseClipping)
				Clip16BitSample(&(LPData(f)->Cur_Sample1));
			if (opUseEnhancement)
				Enhance16BitSample(&(LPData(f)->Cur_Sample1));
			buff[2*i]=(short)(LPData(f)->Cur_Sample1);
			Code=NIBBLE(chunk[i],0);
			Step_IMA_ADPCM(Code,&(LPData(f)->Index2),&(LPData(f)->Cur_Sample2));
			if (opUseClipping)
				Clip16BitSample(&(LPData(f)->Cur_Sample2));
			if (opUseEnhancement)
				Enhance16BitSample(&(LPData(f)->Cur_Sample2));
			buff[2*i+1]=(short)(LPData(f)->Cur_Sample2);
		}
	}
	else
	{
		for (i=0;i<size;i++)
		{
			Code=NIBBLE(chunk[i],0);
			Step_IMA_ADPCM(Code,&(LPData(f)->Index1),&(LPData(f)->Cur_Sample1));
			if (opUseClipping)
				Clip16BitSample(&(LPData(f)->Cur_Sample1));
			if (opUseEnhancement)
				Enhance16BitSample(&(LPData(f)->Cur_Sample1));
			buff[2*i]=(short)(LPData(f)->Cur_Sample1);
			Code=NIBBLE(chunk[i],1);
			Step_IMA_ADPCM(Code,&(LPData(f)->Index1),&(LPData(f)->Cur_Sample1));
			if (opUseClipping)
				Clip16BitSample(&(LPData(f)->Cur_Sample1));
			if (opUseEnhancement)
				Enhance16BitSample(&(LPData(f)->Cur_Sample1));
			buff[2*i+1]=(short)(LPData(f)->Cur_Sample1);
		}
	}

	return 4*size;
}

DWORD __stdcall FillPCMBuffer(FSHandle* file, char* buffer, DWORD buffsize, DWORD* buffpos)
{
    DWORD read,pcmSize,decoded;
	ISSBlockHeader blockhdr;

    if (file==NULL)
		return 0;
    if (LPData(file)==NULL)
		return 0;
    if (Plugin.fsh->EndOfFile(file))
		return 0;

	pcmSize=0;
	while (
			(!(Plugin.fsh->EndOfFile(file))) &&
			(buffsize>=4*LPData(file)->blocksize)
		  )
	{
		Plugin.fsh->ReadFile(file,&blockhdr,sizeof(ISSBlockHeader),&read);
		LPData(file)->Cur_Sample1=(LONG)blockhdr.sample;
		LPData(file)->Index1=(LONG)blockhdr.index;
		if (LPData(file)->channels==2)
		{
			Plugin.fsh->ReadFile(file,&blockhdr,sizeof(ISSBlockHeader),&read);
			LPData(file)->Cur_Sample2=(LONG)blockhdr.sample;
			LPData(file)->Index2=(LONG)blockhdr.index;
		}
		Plugin.fsh->ReadFile(file,LPData(file)->buffer,LPData(file)->blocksize,&read);
		if (read>0)
			decoded=Decode_IMA_ADPCM(file,LPData(file)->buffer,read,(short*)(buffer+pcmSize));
		else
			break; // ???
		pcmSize+=decoded;
		buffsize-=decoded;
	}
	*buffpos=-1;

    return pcmSize;
}

BOOL CenterWindow(HWND hwndChild, HWND hwndParent)
{
    RECT    rcWorkArea,rcChild,rcParent;
    int     cxChild,cyChild,cxParent,cyParent;
    int     xNew,yNew;

    GetWindowRect(hwndChild,&rcChild);
    cxChild=rcChild.right-rcChild.left;
    cyChild=rcChild.bottom-rcChild.top;
    SystemParametersInfo(SPI_GETWORKAREA,0,&rcWorkArea,0);
    if (hwndParent!=NULL)
    {
		GetWindowRect(hwndParent,&rcParent);
		cxParent=rcParent.right-rcParent.left;
		cyParent=rcParent.bottom-rcParent.top;
    }
    else
    {
		rcParent.left=rcWorkArea.left;
		rcParent.top=rcWorkArea.top;
		cxParent=rcWorkArea.right-rcWorkArea.left;
		cyParent=rcWorkArea.bottom-rcWorkArea.top;
    }

    xNew=rcParent.left+((cxParent-cxChild)/2);
    if (xNew<rcWorkArea.left)
		xNew=rcWorkArea.left;
    else if ((xNew+cxChild)>rcWorkArea.right)
		xNew=rcWorkArea.right-cxChild;

    yNew=rcParent.top+((cyParent-cyChild)/2);
    if (yNew<rcWorkArea.top)
		yNew=rcWorkArea.top;
    else if ((yNew+cyChild)>rcWorkArea.bottom)
		yNew=rcWorkArea.bottom-cyChild;

    return SetWindowPos(hwndChild,
						NULL,
						xNew, yNew,
						0,0,
						SWP_NOSIZE | SWP_NOZORDER);
}

LRESULT CALLBACK AboutDlgProc(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm)
{
    switch (umsg)
    {
		case WM_INITDIALOG:
			CenterWindow(hwnd,GetWindow(hwnd,GW_OWNER));
			return TRUE;
		case WM_CLOSE:
			EndDialog(hwnd,TRUE);
			break;
		case WM_COMMAND:
			switch (LOWORD(wparm))
			{
				case IDOK:
					EndDialog(hwnd,TRUE);
					break;
				default:
					break;
			}
			break;
		default:
			break;
    }
    return FALSE;
}

void __stdcall About(HWND hwnd)
{
    DialogBox(Plugin.hDllInst,"About",hwnd,AboutDlgProc);
}

char* GetShortTimeString(DWORD time, char* str)
{
    DWORD sec,min,hour;

    sec=time%60;
    min=(time/60)%60;
    hour=time/3600;
    if (hour!=0)
		wsprintf(str,"%lu:%02lu:%02lu",hour,min,sec);
    else
		wsprintf(str,"%lu:%02lu",min,sec);

    return str;
}

AFile* ibNode;
DWORD  ibSize,ibRate,ibTime;
char   ibVersion[16],ibID[256];
WORD   ibChannels,ibBits;
DWORD  ibUnknown1,ibUnknown2,ibBlockSize;

LRESULT CALLBACK InfoBoxDlgProc(HWND hwnd, UINT umsg, WPARAM wparm, LPARAM lparm)
{
    char str[512];

    switch (umsg)
    {
		case WM_INITDIALOG:
			CenterWindow(hwnd,GetWindow(hwnd,GW_OWNER));
			SetDlgItemText(hwnd,ID_RESNAME,ibNode->rfName);
			SetDlgItemText(hwnd,ID_DATASTR,ibNode->rfDataString);
			SetDlgItemText(hwnd,ID_INTERNALNAME,ibID);
			wsprintf(str,"%lu",ibSize);
			SetDlgItemText(hwnd,ID_FILESIZE,str);
			if (ibTime!=-1)
				GetShortTimeString(ibTime/1000,str);
			else
				lstrcpy(str,"N/A");
			SetDlgItemText(hwnd,ID_LENGTH,str);
			wsprintf(str,"Compression: IMA ADPCM\r\n"
						 "Version: %s\r\n"
						 "Block Size: %lu\r\n"
						 "Channels: %u %s\r\n"
						 "Sample Rate: %lu Hz\r\n"
						 "Resolution: %u-bit\r\n"
						 "Unknown1: %lu\r\n"
						 "Unknown2: %lu",
						 ibVersion,
						 ibBlockSize,
						 ibChannels,
						 (ibChannels==4)?"(quadro)":((ibChannels==2)?"(stereo)":((ibChannels==1)?"(mono)":"")),
						 ibRate,
						 ibBits,
						 ibUnknown1,
						 ibUnknown2
					);
			SetDlgItemText(hwnd,ID_DATAFMT,str);
			return TRUE;
		case WM_CLOSE:
			EndDialog(hwnd,TRUE);
			break;
		case WM_COMMAND:
			switch (LOWORD(wparm))
			{
				case IDOK:
					EndDialog(hwnd,TRUE);
					break;
				default:
					break;
			}
			break;
		default:
			break;
    }
    return FALSE;
}

void SetCheckBox(HWND hwnd, int cbId, BOOL cbVal)
{
    CheckDlgButton(hwnd,cbId,(cbVal)?BST_CHECKED:BST_UNCHECKED);
}

BOOL GetCheckBox(HWND hwnd, int cbId)
{
    return (BOOL)(IsDlgButtonChecked(hwnd,cbId)==BST_CHECKED);
}

LRESULT CALLBACK ConfigDlgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
		case WM_INITDIALOG:
			CenterWindow(hwnd,GetWindow(hwnd,GW_OWNER));
			SetCheckBox(hwnd,IDC_CHECKBLOCKSIZE,opCheckBlockSize);
			SetCheckBox(hwnd,IDC_CHECKCHANNELS,opCheckChannels);
			SetCheckBox(hwnd,IDC_CHECKUNKNOWN1,opCheckUnknown1);
			SetCheckBox(hwnd,IDC_CHECKRATE,opCheckRate);
			SetCheckBox(hwnd,IDC_CHECKUNKNOWN2,opCheckUnknown2);
			SetCheckBox(hwnd,IDC_CHECKVERSION,opCheckVersion);
			SetCheckBox(hwnd,IDC_INITNAME,opInitName);
			SetCheckBox(hwnd,ID_USECLIPPING,opUseClipping);
			SetCheckBox(hwnd,ID_USEENHANCEMENT,opUseEnhancement);
			return TRUE;
		case WM_CLOSE:
			EndDialog(hwnd,FALSE);
			break;
		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case ID_DEFAULTS:
					SetCheckBox(hwnd,IDC_CHECKBLOCKSIZE,FALSE);
					SetCheckBox(hwnd,IDC_CHECKCHANNELS,TRUE);
					SetCheckBox(hwnd,IDC_CHECKUNKNOWN1,FALSE);
					SetCheckBox(hwnd,IDC_CHECKRATE,TRUE);
					SetCheckBox(hwnd,IDC_CHECKUNKNOWN2,FALSE);
					SetCheckBox(hwnd,IDC_CHECKVERSION,TRUE);
					SetCheckBox(hwnd,IDC_INITNAME,TRUE);
					SetCheckBox(hwnd,ID_USECLIPPING,TRUE);
					SetCheckBox(hwnd,ID_USEENHANCEMENT,FALSE);
					break;
				case IDCANCEL:
					EndDialog(hwnd,FALSE);
					break;
				case IDOK:
					opCheckBlockSize=GetCheckBox(hwnd,IDC_CHECKBLOCKSIZE);
					opCheckChannels=GetCheckBox(hwnd,IDC_CHECKCHANNELS);
					opCheckUnknown1=GetCheckBox(hwnd,IDC_CHECKUNKNOWN1);
					opCheckRate=GetCheckBox(hwnd,IDC_CHECKRATE);
					opCheckUnknown2=GetCheckBox(hwnd,IDC_CHECKUNKNOWN2);
					opCheckVersion=GetCheckBox(hwnd,IDC_CHECKVERSION);
					opInitName=GetCheckBox(hwnd,IDC_INITNAME);
					opUseClipping=GetCheckBox(hwnd,ID_USECLIPPING);
					opUseEnhancement=GetCheckBox(hwnd,ID_USEENHANCEMENT);
					EndDialog(hwnd,TRUE);
					break;
				default:
					break;
			}
			break;
		default:
			break;
    }
    return FALSE;
}

void __stdcall Config(HWND hwnd)
{
    DialogBox(Plugin.hDllInst,"Config",hwnd,ConfigDlgProc);
}

void __stdcall InfoBox(AFile* node, HWND hwnd)
{
	FSHandle* f;
    char  str[256];
	DWORD size,outsize;

    if (node==NULL)
		return;
    if ((f=Plugin.fsh->OpenFile(node))==NULL)
    {
		MessageBox(hwnd,"Cannot open file.",Plugin.Description,MB_OK | MB_ICONEXCLAMATION);
		return;
    }
    if (!ReadISSHeader(f,&ibBlockSize,ibID,&ibRate,&ibChannels,&ibBits,ibVersion,&outsize,&ibUnknown1,&ibUnknown2,&size))
    {
		wsprintf(str,"This is not %s file.",Plugin.afID);
		MessageBox(hwnd,str,Plugin.Description,MB_OK | MB_ICONEXCLAMATION);
		Plugin.fsh->CloseFile(f);
		return;
    }
    ibSize=Plugin.fsh->GetFileSize(f);
    Plugin.fsh->CloseFile(f);
    ibNode=node;
    if (node->afTime!=-1)
		ibTime=node->afTime;
    else
		ibTime=GetTime(node);
    DialogBox(Plugin.hDllInst,"InfoBox",hwnd,InfoBoxDlgProc);
}

void __stdcall Seek(FSHandle* f, DWORD pos)
{
    DWORD filepos;

    if (f==NULL)
		return;
    if (LPData(f)==NULL)
		return;

    filepos=MulDiv(pos,(LPData(f)->rate)*(LPData(f)->align),1000);
	filepos/=4*(LPData(f)->blocksize);
	filepos*=(LPData(f)->blocksize)+(LPData(f)->channels)*sizeof(ISSBlockHeader);
    Plugin.fsh->SetFilePointer(f,LPData(f)->headersize+filepos,FILE_BEGIN);
}

SearchPattern patterns[]=
{
	{15,IDSTR_ISS}
};

AFPlugin Plugin=
{
	AFP_VER,
    AFF_VERIFIABLE,
    0,
	0,
    NULL,
    "ANX ISS Audio File Decoder",
    "v0.80",
	"FunCom Audio Files (*.ISS)\0*.iss\0",
    "ISS",
    1,
    patterns,
    NULL,
    Config,
    About,
    Init,
    Quit,
    InfoBox,
    InitPlayback,
    ShutdownPlayback,
    FillPCMBuffer,
    CreateNodeForID,
    CreateNodeForFile,
    GetTime,
    Seek
};

__declspec(dllexport) AFPlugin* __stdcall GetAudioFilePlugin(void)
{
    return &Plugin;
}
