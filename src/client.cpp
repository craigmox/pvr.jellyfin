/*
 *      Copyright (C) 2011 Pulse-Eight
 *      http://www.pulse-eight.com/
 *
 *  This Program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This Program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with XBMC; see the file COPYING.  If not, write to
 *  the Free Software Foundation, 51 Franklin Street, Fifth Floor, Boston,
 *  MA 02110-1301  USA
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include "client.h"
#include "xbmc_pvr_dll.h"
#include <stdlib.h>
#include "JellyfinData.h"
#include "p8-platform/util/util.h"
#include <p8-platform/util/StringUtils.h>

#if defined(TARGET_WINDOWS) && defined(CreateDirectory)
#undef CreateDirectory
#endif

using namespace std;
using namespace ADDON;

bool           m_bCreated         = false;
ADDON_STATUS   m_CurStatus        = ADDON_STATUS_UNKNOWN;

JellyfinChannel    m_currentChannel;

/* User adjustable settings are saved here.
 * Default values are defined inside client.h
 * and exported to the other source files.
 */
std::string g_strHostname         = DEFAULT_HOST;
int         g_iPortHTTP           = DEFAULT_HTTP_PORT;
int         g_iPortHTTPS          = DEFAULT_HTTPS_PORT;
std::string g_strAuth             = DEFAULT_AUTH;
std::string g_strUsername         = "";
std::string g_strPassword         = "";
std::string g_strPath             = "";

std::string g_strBaseUrl          = "";
int         g_iStartNumber        = 1;
std::string g_strUserPath         = "";
std::string g_strClientPath       = "";
int         g_iEPGTimeShift       = 0;

std::string g_strLiveTVParameters = "";
std::string g_strRecordingParameters = "";

CHelper_libXBMC_addon *XBMC       = NULL;
CHelper_libXBMC_pvr   *PVR        = NULL;
Jellyfin                  *JellyfinData   = NULL;


extern std::string PathCombine(const std::string &strPath, const std::string &strFileName)
{
  std::string strResult = strPath;
  if (strResult.at(strResult.size() - 1) == '\\' ||
    strResult.at(strResult.size() - 1) == '/')
  {
    strResult.append(strFileName);
  }
  else
  {
    strResult.append("/");
    strResult.append(strFileName);
  }
   
  return strResult;
}

extern std::string GetClientFilePath(const std::string &strFileName)
{
  return PathCombine(g_strClientPath, strFileName);
}

extern std::string GetUserFilePath(const std::string &strFileName)
{
  return PathCombine(g_strUserPath, strFileName);
}

extern "C" {  

void ADDON_ReadSettings(void)
{
  g_iStartNumber = 1;
    
  /* Read setting "host" from settings.xml */
  int iValue;
  char * buffer;
  buffer = (char*) malloc (1024);
  buffer[0] = 0; /* Set the end of string */

  if (XBMC->GetSetting("host", buffer))
  {
	  g_strHostname = buffer;
  }   
  else 
  {
	  g_strHostname = DEFAULT_HOST;
  }  
  buffer[0] = 0; /* Set the end of string */

  /* Read setting "webport" from settings.xml */
  if (!XBMC->GetSetting("httpport", &g_iPortHTTP)) 
  {
	  g_iPortHTTP = DEFAULT_HTTP_PORT;
  }  

  /* Read setting "webport" from settings.xml */
  if (!XBMC->GetSetting("httpsport", &g_iPortHTTPS)) 
  {
	  g_iPortHTTPS = DEFAULT_HTTPS_PORT;
  }    

  if (XBMC->GetSetting("username", buffer))
  {
	  g_strUsername = buffer;
  }   
  else 
  {
	  g_strUsername = "";
  }  
  buffer[0] = 0; /* Set the end of string */

  if (XBMC->GetSetting("password", buffer))
  {
	  g_strPassword = buffer;
  }   
  else 
  {
	  g_strPassword = "";
  }  
  buffer[0] = 0; /* Set the end of string */

  if (XBMC->GetSetting("path", buffer))
  {
	  g_strPath = buffer;
  }   
  else 
  {
	  g_strPath = "";
  }  
  buffer[0] = 0; /* Set the end of string */

  if (XBMC->GetSetting("livetvparameters", buffer))
  {
	  g_strLiveTVParameters = buffer;
  }   
  else 
  {
	  g_strLiveTVParameters = "";
  }  
  buffer[0] = 0; /* Set the end of string */

  if (XBMC->GetSetting("recordingparameters", buffer))
  {
	  g_strRecordingParameters = buffer;
  }   
  else 
  {
	  g_strRecordingParameters = "";
  }  
  buffer[0] = 0; /* Set the end of string */  

  free (buffer);
}

ADDON_STATUS ADDON_Create(void* hdl, void* props)
{
  if (!hdl || !props)
    return ADDON_STATUS_UNKNOWN;

  PVR_PROPERTIES* pvrprops = (PVR_PROPERTIES*)props;

  XBMC = new CHelper_libXBMC_addon;
  if (!XBMC->RegisterMe(hdl))
  {
    SAFE_DELETE(XBMC);
    return ADDON_STATUS_PERMANENT_FAILURE;
  }

  XBMC->Log(LOG_DEBUG, "%s - Creating Jellyfin Systems PVR-Client", __FUNCTION__);

  PVR = new CHelper_libXBMC_pvr;
  if (!PVR->RegisterMe(hdl))
  {
    SAFE_DELETE(PVR);
    SAFE_DELETE(XBMC);
    return ADDON_STATUS_PERMANENT_FAILURE;
  }

  m_CurStatus = ADDON_STATUS_UNKNOWN;
  g_strUserPath = pvrprops->strUserPath;
  g_strClientPath = pvrprops->strClientPath;

  if (!XBMC->DirectoryExists(g_strUserPath.c_str()))
  {
    XBMC->CreateDirectory(g_strUserPath.c_str());
  }
  
  ADDON_ReadSettings();

  JellyfinData = new Jellyfin;

  if (!JellyfinData->Open())
  {
    SAFE_DELETE(JellyfinData);
    SAFE_DELETE(PVR);
    SAFE_DELETE(XBMC);
    m_CurStatus = ADDON_STATUS_LOST_CONNECTION;
    return m_CurStatus;
  }

  m_CurStatus = ADDON_STATUS_OK;
  m_bCreated = true;
  return m_CurStatus;
}

ADDON_STATUS ADDON_GetStatus()
{
  /* check whether we're still connected */
  if (m_CurStatus == ADDON_STATUS_OK && !JellyfinData->IsConnected())
    m_CurStatus = ADDON_STATUS_LOST_CONNECTION;

  return m_CurStatus;
}

void ADDON_Destroy()
{
  if (m_bCreated)
  {
    m_bCreated = false;
  }
    
  SAFE_DELETE(JellyfinData);
  SAFE_DELETE(PVR);
  SAFE_DELETE(XBMC);

  m_CurStatus = ADDON_STATUS_UNKNOWN;
}

ADDON_STATUS ADDON_SetSetting(const char *settingName, const void *settingValue)
{
  string str = settingName;
  if (str == "host")
  {
    string strNewHostname = (const char*)settingValue;
    if (strNewHostname != g_strHostname)
    {
      g_strHostname = strNewHostname;
      XBMC->Log(LOG_INFO, "%s - Changed Setting 'host' from %s to %s", __FUNCTION__, g_strHostname.c_str(), (const char*)settingValue);
      return ADDON_STATUS_NEED_RESTART;
    }    
  }
  else if (str == "httpport")
  {
    int iNewValue = *(int*) settingValue;
    if (g_iPortHTTP != iNewValue)
    {
      g_iPortHTTP = iNewValue;
      XBMC->Log(LOG_INFO, "%s - Changed Setting 'httpport' from %u to %u", __FUNCTION__, g_iPortHTTP, iNewValue);      
      return ADDON_STATUS_NEED_RESTART;
    }
  }
    else if (str == "httpsport")
  {
    int iNewValue = *(int*) settingValue;
    if (g_iPortHTTPS != iNewValue)
    {
      g_iPortHTTPS = iNewValue;
      XBMC->Log(LOG_INFO, "%s - Changed Setting 'httpsport' from %u to %u", __FUNCTION__, g_iPortHTTPS, iNewValue);      
      return ADDON_STATUS_NEED_RESTART;
    }
  }
  else if (str == "username")
  {
    string strNewUsername = (const char*)settingValue;
    if (strNewUsername != g_strUsername)
    {
      g_strUsername = strNewUsername;
      XBMC->Log(LOG_INFO, "%s - Changed Setting 'username' from %s to %s", __FUNCTION__, g_strUsername.c_str(), (const char*)settingValue);
      return ADDON_STATUS_NEED_RESTART;
    }    
  }
  else if (str == "password")
  {
    string strNewPassword = (const char*)settingValue;
    if (strNewPassword != g_strPassword)
    {
      g_strPassword = strNewPassword;
      XBMC->Log(LOG_INFO, "%s - Changed Setting 'passwordm_strBaseUrl' from %s to %s", __FUNCTION__, g_strPassword.c_str(), (const char*)settingValue);
      return ADDON_STATUS_NEED_RESTART;
    }    
  }
  else if (str == "path")
  {
    string strNewPath = (const char*)settingValue;
    if (strNewPath != g_strPath)
    {
      g_strPath = strNewPath;
      XBMC->Log(LOG_INFO, "%s - Changed Setting 'path' from %s to %s", __FUNCTION__, g_strPath.c_str(), (const char*)settingValue);
      return ADDON_STATUS_NEED_RESTART;
    }    
  }  
  else if (str == "livetvparameters")
  {
    string strNew = (const char*)settingValue;
    if (strNew != g_strLiveTVParameters)
    {
      g_strLiveTVParameters = strNew;
      XBMC->Log(LOG_INFO, "%s - Changed Setting 'passwordm_strBaseUrl' from %s to %s", __FUNCTION__, g_strLiveTVParameters.c_str(), (const char*)settingValue);
      return ADDON_STATUS_NEED_RESTART;
    }    
  }
  else if (str == "recordingparameters")
  {
    string strNew = (const char*)settingValue;
    if (strNew != g_strRecordingParameters)
    {
      g_strRecordingParameters = strNew;
      XBMC->Log(LOG_INFO, "%s - Changed Setting 'passwordm_strBaseUrl' from %s to %s", __FUNCTION__, g_strRecordingParameters.c_str(), (const char*)settingValue);
      return ADDON_STATUS_NEED_RESTART;
    }    
  }
  
  return ADDON_STATUS_OK;
}

/***********************************************************
 * PVR Client AddOn specific public library functions
 ***********************************************************/

void OnSystemSleep()
{
}

void OnSystemWake()
{
}

void OnPowerSavingActivated()
{
}

void OnPowerSavingDeactivated()
{
}

PVR_ERROR GetAddonCapabilities(PVR_ADDON_CAPABILITIES* pCapabilities)
{
  pCapabilities->bSupportsEPG                 = true;
  pCapabilities->bSupportsTV                  = true;
  pCapabilities->bSupportsRadio               = false;
  pCapabilities->bSupportsChannelGroups       = false;
  pCapabilities->bSupportsRecordings          = true;
  pCapabilities->bSupportsRecordingsUndelete  = false;
  pCapabilities->bSupportsTimers              = true;
  pCapabilities->bSupportsChannelScan         = false;
  pCapabilities->bHandlesInputStream          = false;
  pCapabilities->bHandlesDemuxing             = false;
  pCapabilities->bSupportsLastPlayedPosition  = false;
  pCapabilities->bSupportsRecordingsRename    = false;
  pCapabilities->bSupportsRecordingsLifetimeChange = false;
  pCapabilities->bSupportsDescrambleInfo = false;

  return PVR_ERROR_NO_ERROR;
}

const char *GetBackendName(void)
{  
  static const char *strBackendName = JellyfinData ? JellyfinData->GetBackendName() : "unknown";
  return strBackendName;
}

const char *GetBackendVersion(void)
{
  static const char *strBackendVersion = JellyfinData ? JellyfinData->GetBackendVersion() : "unknown";  
  return strBackendVersion;
}

const char *GetBackendHostname(void)
{
    return g_strHostname.c_str();
}

const char *GetConnectionString(void)
{
  //static std::string strConnectionString = "connected";
  static std::string strConnectionString;
  if (JellyfinData)
    strConnectionString= StringUtils::Format("%s%s", g_strHostname.c_str(), JellyfinData->IsConnected() ? "" : " (Not connected!)");
  else
    strConnectionString= StringUtils::Format("%s (addon error!)", g_strHostname.c_str());
  return strConnectionString.c_str();
}

PVR_ERROR GetDriveSpace(long long *iTotal, long long *iUsed)
{   
	return PVR_ERROR_NOT_IMPLEMENTED;	
}

int GetChannelsAmount(void)
{
  if (!JellyfinData || !JellyfinData->IsConnected())
    return -1;

  return JellyfinData->GetChannelsAmount();
}

PVR_ERROR GetChannels(ADDON_HANDLE handle, bool bRadio)
{
  if (!JellyfinData || !JellyfinData->IsConnected())
    return PVR_ERROR_SERVER_ERROR;

  return JellyfinData->GetChannels(handle, bRadio);  
}

bool OpenLiveStream(const PVR_CHANNEL &channel)
{
  if (!JellyfinData || !JellyfinData->IsConnected())
    return false;
    
  CloseLiveStream();

  if (JellyfinData->GetChannel(channel, m_currentChannel))
  {    
    return true;
  }  

  return false;
}

void CloseLiveStream(void)
{  
  if (JellyfinData) {
    JellyfinData->CloseLiveStream();
  }  
}

PVR_ERROR GetEPGForChannel(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd)
{
  if (!JellyfinData || !JellyfinData->IsConnected())
    return PVR_ERROR_SERVER_ERROR;
  
  return JellyfinData->GetEPGForChannel(handle, channel, iStart, iEnd);
}

int GetRecordingsAmount(bool deleted)
{
  if (!JellyfinData || !JellyfinData->IsConnected())
    return PVR_ERROR_SERVER_ERROR;

  return JellyfinData->GetRecordingsAmount();
}

PVR_ERROR GetRecordings(ADDON_HANDLE handle, bool deleted)
{
  if (!JellyfinData || !JellyfinData->IsConnected())
    return PVR_ERROR_SERVER_ERROR;

  return JellyfinData->GetRecordings(handle);
}

PVR_ERROR GetTimerTypes(PVR_TIMER_TYPE types[], int *size)
{
  memset(types,0,sizeof(PVR_TIMER_TYPE));
  types[0].iId = PVR_TIMER_TYPE_NONE + 1;
  types[0].iAttributes = PVR_TIMER_TYPE_REQUIRES_EPG_TAG_ON_CREATE;
  strcpy(types[0].strDescription,"PVR Jellyfin Add-on only suports recordings based on EPG.");
  
  *size = 1;
  return PVR_ERROR_NO_ERROR;
}

int GetTimersAmount(void)
{
  if (!JellyfinData || !JellyfinData->IsConnected())
    return 0;

  return JellyfinData->GetTimersAmount();
}

PVR_ERROR GetTimers(ADDON_HANDLE handle)
{
  if (!JellyfinData || !JellyfinData->IsConnected())
    return PVR_ERROR_SERVER_ERROR;

  return JellyfinData->GetTimers(handle);
}

PVR_ERROR AddTimer(const PVR_TIMER &timer) { 
  
  if (!JellyfinData || !JellyfinData->IsConnected())
    return PVR_ERROR_SERVER_ERROR;

  return JellyfinData->AddTimer(timer);

}

PVR_ERROR DeleteTimer(const PVR_TIMER &timer, bool bForceDelete)  { 
  
  if (!JellyfinData || !JellyfinData->IsConnected())
    return PVR_ERROR_SERVER_ERROR;

  return JellyfinData->DeleteTimer(timer);

}


PVR_ERROR GetChannelStreamProperties(const PVR_CHANNEL* channel, PVR_NAMED_VALUE* properties, unsigned int* iPropertiesCount) {
  if (!JellyfinData || !JellyfinData->IsConnected())
    return PVR_ERROR_SERVER_ERROR;

  return JellyfinData->GetChannelStreamProperties(channel, properties, iPropertiesCount);
}

PVR_ERROR GetRecordingStreamProperties(const PVR_RECORDING* recording, PVR_NAMED_VALUE* properties, unsigned int* iPropertiesCount) {
  if (!JellyfinData || !JellyfinData->IsConnected())
    return PVR_ERROR_SERVER_ERROR;

  return JellyfinData->GetRecordingStreamProperties(recording, properties, iPropertiesCount);
}

/** UNUSED API FUNCTIONS */
int GetChannelGroupsAmount(void) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetChannelGroups(ADDON_HANDLE handle, bool bRadio) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SignalStatus(PVR_SIGNAL_STATUS &signalStatus) { return PVR_ERROR_NO_ERROR; }
PVR_ERROR GetStreamProperties(PVR_STREAM_PROPERTIES* pProperties) { return PVR_ERROR_NOT_IMPLEMENTED; }
void DemuxAbort(void) { return; }
DemuxPacket* DemuxRead(void) { return NULL; }
PVR_ERROR CallMenuHook(const PVR_MENUHOOK &menuhook, const PVR_MENUHOOK_DATA &item) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteChannel(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR RenameChannel(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR UpdateTimer(const PVR_TIMER &timer) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SetRecordingLastPlayedPosition(const PVR_RECORDING &recording, int lastplayedposition) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR RenameRecording(const PVR_RECORDING &recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteRecording(const PVR_RECORDING &recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
int GetRecordingLastPlayedPosition(const PVR_RECORDING &recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
bool OpenRecordedStream(const PVR_RECORDING &recording) { return false; }
void CloseRecordedStream(void) {}
int ReadRecordedStream(unsigned char *pBuffer, unsigned int iBufferSize) { return 0; }
long long SeekRecordedStream(long long iPosition, int iWhence /* = SEEK_SET */) { return 0; }
long long LengthRecordedStream(void) { return 0; }
void DemuxReset(void) {}
void DemuxFlush(void) {}
int ReadLiveStream(unsigned char *pBuffer, unsigned int iBufferSize) { return 0; }
long long SeekLiveStream(long long iPosition, int iWhence /* = SEEK_SET */) { return -1; }
long long LengthLiveStream(void) { return -1; }
PVR_ERROR SetRecordingPlayCount(const PVR_RECORDING &recording, int count) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetRecordingEdl(const PVR_RECORDING&, PVR_EDL_ENTRY[], int*) { return PVR_ERROR_NOT_IMPLEMENTED; };
void PauseStream(bool bPaused) {}
bool CanPauseStream(void) { return false; }
bool CanSeekStream(void) { return false; }
bool SeekTime(double, bool, double*) { return false; }
void SetSpeed(int) {};
bool IsTimeshifting(void) { return false; }
bool IsRealTimeStream() { return true; }
PVR_ERROR UndeleteRecording(const PVR_RECORDING& recording) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR DeleteAllRecordingsFromTrash() { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR OpenDialogChannelScan(void) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR OpenDialogChannelSettings(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR OpenDialogChannelAdd(const PVR_CHANNEL &channel) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SetEPGTimeFrame(int) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR SetRecordingLifetime(const PVR_RECORDING*) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetDescrambleInfo(PVR_DESCRAMBLE_INFO*) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetStreamTimes(PVR_STREAM_TIMES*) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR IsEPGTagPlayable(const EPG_TAG*, bool*) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR IsEPGTagRecordable(const EPG_TAG*, bool*) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetEPGTagStreamProperties(const EPG_TAG*, PVR_NAMED_VALUE*, unsigned int*) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetEPGTagEdl(const EPG_TAG* epgTag, PVR_EDL_ENTRY edl[], int *size) { return PVR_ERROR_NOT_IMPLEMENTED; }
PVR_ERROR GetStreamReadChunkSize(int* chunksize) { return PVR_ERROR_NOT_IMPLEMENTED; }
}
