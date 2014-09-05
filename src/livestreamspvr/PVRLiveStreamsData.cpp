/*
 *      Copyright (C) 2014 Zoltan Csizmadia
 *      http://github.com/zcsizmadia/xbmc-pvr-livestreams/
 *
 *      Copyright (C) 2013 Anton Fedchin
 *      http://github.com/afedchin/xbmc-addon-iptvsimple/
 *
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
 *  the Free Software Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.
 *  http://www.gnu.org/copyleft/gpl.html
 *
 */

#include <sstream>
#include <string>
#include <fstream>
#include <map>
#include "slre.h"
#include "zlib.h"
#include "rapidxml/rapidxml.hpp"
#include "PVRLiveStreamsData.h"
#include "platform/sockets/tcp.h"

#define SECONDS_IN_DAY          (60*60*24)

using namespace std;
using namespace ADDON;
using namespace rapidxml;

template<class Ch>
inline bool GetNodeValue(const xml_node<Ch> * pRootNode, const char* strTag, CStdString& strStringValue)
{
  xml_node<Ch> *pChildNode = pRootNode->first_node(strTag);
  if (pChildNode == NULL)
  {
    return false;
  }
  strStringValue = pChildNode->value();
  return true;
}

template<class Ch>
inline bool GetAttributeValue(const xml_node<Ch> * pNode, const char* strAttributeName, CStdString& strStringValue)
{
  xml_attribute<Ch> *pAttribute = pNode->first_attribute(strAttributeName);
  if (pAttribute == NULL)
  {
    return false;
  }
  strStringValue = pAttribute->value();
  return true;
}

PVRLiveStreamsData::PVRLiveStreamsData(void)
{
  m_strXMLTVUrl = g_strTvgPath;
  m_strXMLUrl = g_strXMLPath;
  m_iEPGTimeShift = g_iEPGTimeShift;
  m_bTSOverride = g_bTSOverride;
  m_iLastStart = 0;
  m_iLastEnd = 0;

  m_bEGPLoaded = false;

  if (LoadPlayList())
  {
    XBMC->QueueNotification(QUEUE_INFO, "%d channels loaded.", m_channels.size());
  }
}

void *PVRLiveStreamsData::Process(void)
{
  return NULL;
}

PVRLiveStreamsData::~PVRLiveStreamsData(void)
{
  m_channels.clear();
  m_groups.clear();
  m_epg.clear();
}

bool PVRLiveStreamsData::LoadEPG(time_t iStart, time_t iEnd)
{
  if (m_strXMLTVUrl.IsEmpty())
  {
    XBMC->Log(LOG_NOTICE, "EPG file path is not configured. EPG not loaded.");
    m_bEGPLoaded = true;
    return false;
  }

  std::string data;
  std::string decompressed;
  int iReaded = 0;

  int iCount = 0;
  while (iCount < 3) // max 3 tries
  {
    if ((iReaded = GetCachedFileContents(TVG_FILE_NAME, m_strXMLTVUrl, data, g_bCacheEPG)) != 0)
    {
      break;
    }
    XBMC->Log(LOG_ERROR, "Unable to load EPG file '%s':  file is missing or empty. :%dth try.", m_strXMLTVUrl.c_str(), ++iCount);
    if (iCount < 3)
    {
      usleep(2 * 1000 * 1000); // sleep 2 sec before next try.
    }
  }

  if (iReaded == 0)
  {
    XBMC->Log(LOG_ERROR, "Unable to load EPG file '%s':  file is missing or empty. After %d tries.", m_strXMLTVUrl.c_str(), iCount);
    m_bEGPLoaded = true;
    m_iLastStart = iStart;
    m_iLastEnd = iEnd;
    return false;
  }

  char * buffer;

  // gzip packed
  if (data[0] == '\x1F' && data[1] == '\x8B' && data[2] == '\x08')
  {
    if (!GzipInflate(data, decompressed))
    {
      XBMC->Log(LOG_ERROR, "Invalid EPG file '%s': unable to decompress file.", m_strXMLTVUrl.c_str());
      m_bEGPLoaded = true;
      return false;
    }
    buffer = &(decompressed[0]);
  }
  else
  {
    buffer = &(data[0]);
  }

  // xml should starts with '<?xml'
  if (buffer[0] != '\x3C' || buffer[1] != '\x3F' || buffer[2] != '\x78' ||
    buffer[3] != '\x6D' || buffer[4] != '\x6C')
  {
    // check for BOM
    if (buffer[0] != '\xEF' || buffer[1] != '\xBB' || buffer[2] != '\xBF')
    {
      // check for tar archive
      if (strcmp(buffer + 0x101, "ustar") ||
        strcmp(buffer + 0x101, "GNUtar"))
      {
        buffer += 0x200; // RECORDSIZE = 512
      }
      else
      {
        m_bEGPLoaded = true;
        return false;
      }
    }
  }

  xml_document<> xmlDoc;
  try
  {
    xmlDoc.parse<0>(buffer);
  }
  catch (parse_error p)
  {
    XBMC->Log(LOG_ERROR, "Unable parse EPG XML: %s", p.what());
    m_bEGPLoaded = true;
    return false;
  }

  xml_node<> *pRootElement = xmlDoc.first_node("tv");
  if (!pRootElement)
  {
    XBMC->Log(LOG_ERROR, "Invalid EPG XML: no <tv> tag found");
    m_bEGPLoaded = true;
    return false;
  }

  // clear previously loaded epg
  if (m_epg.size() > 0)
  {
    m_epg.clear();
  }

  int iBroadCastId = 0;
  xml_node<> *pChannelNode = NULL;
  for (pChannelNode = pRootElement->first_node("channel"); pChannelNode; pChannelNode = pChannelNode->next_sibling("channel"))
  {
    CStdString strName;
    CStdString strId;
    if (!GetAttributeValue(pChannelNode, "id", strId))
    {
      continue;
    }
    GetNodeValue(pChannelNode, "display-name", strName);

    if (FindChannel(strId, strName) == NULL)
    {
      continue;
    }

    PVRLiveStreamsEpgChannel epgChannel;
    epgChannel.strId = strId;
    epgChannel.strName = strName;

    m_epg.push_back(epgChannel);
  }

  if (m_epg.size() == 0)
  {
    XBMC->Log(LOG_ERROR, "EPG channels not found.");
    return false;
  }

  int iMinShiftTime = m_iEPGTimeShift;
  int iMaxShiftTime = m_iEPGTimeShift;
  if (!m_bTSOverride)
  {
    iMinShiftTime = SECONDS_IN_DAY;
    iMaxShiftTime = -SECONDS_IN_DAY;

    vector<PVRLiveStreamsChannel>::iterator it;
    for (it = m_channels.begin(); it < m_channels.end(); it++)
    {
      if (it->iTvgShift + m_iEPGTimeShift < iMinShiftTime)
        iMinShiftTime = it->iTvgShift + m_iEPGTimeShift;
      if (it->iTvgShift + m_iEPGTimeShift > iMaxShiftTime)
        iMaxShiftTime = it->iTvgShift + m_iEPGTimeShift;
    }
  }

  CStdString strEmpty = "";
  PVRLiveStreamsEpgChannel *epg = NULL;
  for (pChannelNode = pRootElement->first_node("programme"); pChannelNode; pChannelNode = pChannelNode->next_sibling("programme"))
  {
    CStdString strId;
    if (!GetAttributeValue(pChannelNode, "channel", strId))
      continue;

    if (epg == NULL || epg->strId != strId)
    {
      if ((epg = FindEpg(strId)) == NULL)
        continue;
    }

    CStdString strStart;
    CStdString strStop;

    if (!GetAttributeValue(pChannelNode, "start", strStart) || !GetAttributeValue(pChannelNode, "stop", strStop))
    {
      continue;
    }

    int iTmpStart = ParseDateTime(strStart);
    int iTmpEnd = ParseDateTime(strStop);

    if ((iTmpEnd + iMaxShiftTime < iStart) || (iTmpStart + iMinShiftTime > iEnd))
    {
      continue;
    }

    CStdString strTitle;
    CStdString strCategory;
    CStdString strDesc;

    GetNodeValue(pChannelNode, "title", strTitle);
    GetNodeValue(pChannelNode, "category", strCategory);
    GetNodeValue(pChannelNode, "desc", strDesc);

    CStdString strIconPath;
    xml_node<> *pIconNode = pChannelNode->first_node("icon");
    if (pIconNode != NULL)
    {
      if (!GetAttributeValue(pIconNode, "src", strIconPath))
      {
        strIconPath = "";
      }
    }

    PVRLiveStreamsEpgEntry entry;
    entry.iBroadcastId = ++iBroadCastId;
    entry.iGenreType = 0;
    entry.iGenreSubType = 0;
    entry.strTitle = strTitle;
    entry.strPlot = strDesc;
    entry.strPlotOutline = "";
    entry.strIconPath = strIconPath;
    entry.startTime = iTmpStart;
    entry.endTime = iTmpEnd;
    entry.strGenreString = strCategory;

    epg->epg.push_back(entry);
  }

  xmlDoc.clear();
  m_bEGPLoaded = true;

  XBMC->Log(LOG_NOTICE, "EPG Loaded.");

  return true;
}

bool PVRLiveStreamsData::LoadPlayList(void)
{
  if (m_strXMLUrl.IsEmpty())
  {
    XBMC->Log(LOG_NOTICE, "Playlist file path is not configured. Channels not loaded.");
    return false;
  }

  CStdString strPlaylistContent;
  if (!GetCachedFileContents(XML_FILE_NAME, m_strXMLUrl, strPlaylistContent, g_bCacheXML))
  {
    XBMC->Log(LOG_ERROR, "Unable to load playlist file '%s':  file is missing or empty.", m_strXMLUrl.c_str());
    return false;
  }

  xml_document<> xmlDoc;
  try
  {
    XBMC->Log(LOG_NOTICE, "Unable to parse playlist file: %s", strPlaylistContent.c_str());
    xmlDoc.parse<0>((char*)strPlaylistContent.c_str());
  }
  catch (parse_error p)
  {
    XBMC->Log(LOG_ERROR, "Unable to parse playlist file: %s", p.what());
    m_bEGPLoaded = true;
    return false;
  }

  xml_node<> *pRootElement = &xmlDoc;
  xml_node<> *pChannelNode = NULL;
  xml_node<> *pRegexNode = NULL;
  xml_node<> *pNode = NULL;
  int iChannelNum = g_iStartNumber;
  for (pChannelNode = pRootElement->first_node("item"); pChannelNode; pChannelNode = pChannelNode->next_sibling("item"))
  {
    PVRLiveStreamsChannel channel;
    CStdString str;

    channel.bRadio = false;
    channel.iUniqueId = 0;
    channel.iChannelNumber = 0;
    channel.iEncryptionSystem = 0;
    channel.iTvgShift = 0;

    GetNodeValue(pChannelNode, "title", str);
    channel.strChannelName = XBMC->UnknownToUTF8(str);

    GetNodeValue(pChannelNode, "link", str);
    channel.strLink = str;

    GetNodeValue(pChannelNode, "thumbnail", str);
    channel.strLogoPath = str;

    GetNodeValue(pChannelNode, "tvg-id", str);
    channel.strTvgId = XBMC->UnknownToUTF8(str);

    GetNodeValue(pChannelNode, "tvg-name", str);
    channel.strTvgName = XBMC->UnknownToUTF8(str);
    if (channel.strTvgName.length() == 0)
      channel.strTvgName = channel.strChannelName;

    GetNodeValue(pChannelNode, "tvg-shift", str);
    channel.iTvgShift *= atoi(str) * 3600;

    GetNodeValue(pChannelNode, "radio", str);
    channel.bRadio = strcmp(str, "true") == 0;

    if (GetNodeValue(pChannelNode, "number", str))
      iChannelNum = atoi(str);

    channel.iChannelNumber = iChannelNum++;
    channel.iEncryptionSystem = 0;
    channel.strStreamURL = "";

    if (GetAttributeValue(pChannelNode, "id", str))
      channel.iUniqueId = atoi(str);
    else
      channel.iUniqueId = GetChannelId(channel.strChannelName.c_str(), channel.strLink.c_str());

    for (pRegexNode = pChannelNode->first_node("regex"); pRegexNode; pRegexNode = pRegexNode->next_sibling("regex"))
    {
      RegexParams regexParams;

      for (pNode = pRegexNode->first_node(); pNode; pNode = pNode->next_sibling())
        regexParams[pNode->name()] = pNode->value();

      if (regexParams.find("name") == regexParams.end())
        continue;

      channel.regexs[regexParams["name"]] = regexParams;

      XBMC->Log(LOG_NOTICE, "Regex name: %s", regexParams["name"].c_str());
      XBMC->Log(LOG_NOTICE, "Regex page: %s", regexParams["page"].c_str());
      XBMC->Log(LOG_NOTICE, "Regex expres: %s", regexParams["expres"].c_str());
    }

    m_channels.push_back(channel);
  }

  xmlDoc.clear();

  if (m_channels.size() == 0)
  {
    XBMC->Log(LOG_ERROR, "Unable to load channels from file '%s':  file is corrupted.", m_strXMLUrl.c_str());
    return false;
  }

  //ApplyChannelsLogos();

  XBMC->Log(LOG_NOTICE, "Loaded %d channels.", m_channels.size());
  return true;
}

int PVRLiveStreamsData::GetChannelsAmount(void)
{
  return m_channels.size();
}

PVR_ERROR PVRLiveStreamsData::GetChannels(ADDON_HANDLE handle, bool bRadio)
{
  for (size_t i = 0; i < m_channels.size(); i++)
  {
    PVRLiveStreamsChannel& channel = m_channels[i];

    if (channel.bRadio == bRadio)
    {
      PVR_CHANNEL xbmcChannel;
      memset(&xbmcChannel, 0, sizeof(PVR_CHANNEL));

      xbmcChannel.iUniqueId = channel.iUniqueId;
      xbmcChannel.bIsRadio = channel.bRadio;
      xbmcChannel.iChannelNumber = channel.iChannelNumber;
      PVR_STRCPY(xbmcChannel.strChannelName, channel.strChannelName.c_str());
      PVR_STRCPY(xbmcChannel.strIconPath, channel.strLogoPath.c_str());
      xbmcChannel.iEncryptionSystem = channel.iEncryptionSystem;
      xbmcChannel.bIsHidden = false;
      snprintf(xbmcChannel.strStreamURL, sizeof(xbmcChannel.strStreamURL), "pvr://stream/%s/%x", channel.bRadio ? "radio" : "tv", channel.iUniqueId);

      PVR->TransferChannelEntry(handle, &xbmcChannel);
    }
  }

  return PVR_ERROR_NO_ERROR;
}

bool PVRLiveStreamsData::GetChannel(const PVR_CHANNEL &channel, PVRLiveStreamsChannel &myChannel)
{
  for (size_t i = 0; i < m_channels.size(); i++)
  {
    PVRLiveStreamsChannel& thisChannel = m_channels[i];
    if (thisChannel.iUniqueId == (int)channel.iUniqueId)
    {
      myChannel = thisChannel;
      return true;
    }
  }

  return false;
}

const char* PVRLiveStreamsData::GetChannelURL(const PVR_CHANNEL &channel)
{
  for (size_t i = 0; i < m_channels.size(); i++)
  {
    PVRLiveStreamsChannel& thisChannel = m_channels[i];
    if (thisChannel.iUniqueId == (int)channel.iUniqueId)
    {
      thisChannel.strStreamURL = thisChannel.strLink;
      if (!GetRegexParsed(thisChannel.strStreamURL, thisChannel.regexs))
        return "";
      XBMC->Log(LOG_NOTICE, "GetChannelURL: %s", thisChannel.strStreamURL.c_str());
      return thisChannel.strStreamURL.c_str();
    }
  }

  return NULL;
}

int PVRLiveStreamsData::GetChannelGroupsAmount(void)
{
  return m_groups.size();
}

PVR_ERROR PVRLiveStreamsData::GetChannelGroups(ADDON_HANDLE handle, bool bRadio)
{
  for (unsigned int iGroupPtr = 0; iGroupPtr < m_groups.size(); iGroupPtr++)
  {
    PVRLiveStreamsChannelGroup &group = m_groups.at(iGroupPtr);
    if (group.bRadio == bRadio)
    {
      PVR_CHANNEL_GROUP xbmcGroup;
      memset(&xbmcGroup, 0, sizeof(PVR_CHANNEL_GROUP));

      xbmcGroup.bIsRadio = bRadio;
      strncpy(xbmcGroup.strGroupName, group.strGroupName.c_str(), sizeof(xbmcGroup.strGroupName) - 1);

      PVR->TransferChannelGroup(handle, &xbmcGroup);
    }
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR PVRLiveStreamsData::GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group)
{
  PVRLiveStreamsChannelGroup *myGroup;
  if ((myGroup = FindGroup(group.strGroupName)) != NULL)
  {
    for (unsigned int iPtr = 0; iPtr < myGroup->members.size(); iPtr++)
    {
      int iIndex = myGroup->members.at(iPtr);
      if (iIndex < 0 || iIndex >= (int)m_channels.size())
        continue;

      PVRLiveStreamsChannel &channel = m_channels.at(iIndex);
      PVR_CHANNEL_GROUP_MEMBER xbmcGroupMember;
      memset(&xbmcGroupMember, 0, sizeof(PVR_CHANNEL_GROUP_MEMBER));

      strncpy(xbmcGroupMember.strGroupName, group.strGroupName, sizeof(xbmcGroupMember.strGroupName) - 1);
      xbmcGroupMember.iChannelUniqueId = channel.iUniqueId;
      xbmcGroupMember.iChannelNumber = channel.iChannelNumber;

      PVR->TransferChannelGroupMember(handle, &xbmcGroupMember);
    }
  }

  return PVR_ERROR_NO_ERROR;
}

PVR_ERROR PVRLiveStreamsData::GetEPGForChannel(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd)
{
  vector<PVRLiveStreamsChannel>::iterator myChannel;
  for (myChannel = m_channels.begin(); myChannel < m_channels.end(); myChannel++)
  {
    if (myChannel->iUniqueId != (int)channel.iUniqueId)
      continue;

    if (!m_bEGPLoaded || iStart > m_iLastStart || iEnd > m_iLastEnd)
    {
      if (LoadEPG(iStart, iEnd))
      {
        m_iLastStart = iStart;
        m_iLastEnd = iEnd;
      }
    }

    PVRLiveStreamsEpgChannel *epg;
    if ((epg = FindEpgForChannel(*myChannel)) == NULL || epg->epg.size() == 0)
    {
      return PVR_ERROR_NO_ERROR;
    }

    int iShift = m_bTSOverride ? m_iEPGTimeShift : myChannel->iTvgShift + m_iEPGTimeShift;

    vector<PVRLiveStreamsEpgEntry>::iterator myTag;
    for (myTag = epg->epg.begin(); myTag < epg->epg.end(); myTag++)
    {
      if ((myTag->endTime + iShift) < iStart)
        continue;

      EPG_TAG tag;
      memset(&tag, 0, sizeof(EPG_TAG));

      tag.iUniqueBroadcastId = myTag->iBroadcastId;
      tag.strTitle = myTag->strTitle.c_str();
      tag.iChannelNumber = myTag->iChannelId;
      tag.startTime = myTag->startTime + iShift;
      tag.endTime = myTag->endTime + iShift;
      tag.strPlotOutline = myTag->strPlotOutline.c_str();
      tag.strPlot = myTag->strPlot.c_str();
      tag.strIconPath = myTag->strIconPath.c_str();
      tag.iGenreType = EPG_GENRE_USE_STRING;        //myTag.iGenreType;
      tag.iGenreSubType = 0;                           //myTag.iGenreSubType;
      tag.strGenreDescription = myTag->strGenreString.c_str();

      PVR->TransferEpgEntry(handle, &tag);

      if ((myTag->startTime + iShift) > iEnd)
        break;
    }

    return PVR_ERROR_NO_ERROR;
  }

  return PVR_ERROR_NO_ERROR;
}

int PVRLiveStreamsData::GetFileContents(CStdString& url, std::string &strContent)
{
  strContent.clear();
  void* fileHandle = XBMC->OpenFile(url.c_str(), 0);
  if (fileHandle)
  {
    char buffer[1024];
    while (int bytesRead = XBMC->ReadFile(fileHandle, buffer, 1024))
      strContent.append(buffer, bytesRead);
    XBMC->CloseFile(fileHandle);
  }

  return strContent.length();
}

int PVRLiveStreamsData::ParseDateTime(CStdString strDate, bool iDateFormat)
{
  struct tm timeinfo;
  memset(&timeinfo, 0, sizeof(tm));

  if (iDateFormat)
  {
    sscanf(strDate, "%04d%02d%02d%02d%02d%02d", &timeinfo.tm_year, &timeinfo.tm_mon, &timeinfo.tm_mday, &timeinfo.tm_hour, &timeinfo.tm_min, &timeinfo.tm_sec);
  }
  else
  {
    sscanf(strDate, "%02d.%02d.%04d%02d:%02d:%02d", &timeinfo.tm_mday, &timeinfo.tm_mon, &timeinfo.tm_year, &timeinfo.tm_hour, &timeinfo.tm_min, &timeinfo.tm_sec);
  }

  timeinfo.tm_mon -= 1;
  timeinfo.tm_year -= 1900;
  timeinfo.tm_isdst = -1;

  return mktime(&timeinfo);
}

PVRLiveStreamsChannel * PVRLiveStreamsData::FindChannel(const std::string &strId, const std::string &strName)
{
  CStdString strTvgName = strName;
  strTvgName.Replace(' ', '_');

  vector<PVRLiveStreamsChannel>::iterator it;
  for (it = m_channels.begin(); it < m_channels.end(); it++)
  {
    if (it->strTvgId == strId)
    {
      return &*it;
    }
    if (strTvgName == "")
    {
      continue;
    }
    if (it->strTvgName == strTvgName)
    {
      return &*it;
    }
    if (it->strChannelName == strName)
    {
      return &*it;
    }
  }

  return NULL;
}

PVRLiveStreamsChannelGroup * PVRLiveStreamsData::FindGroup(const std::string &strName)
{
  vector<PVRLiveStreamsChannelGroup>::iterator it;
  for (it = m_groups.begin(); it < m_groups.end(); it++)
  {
    if (it->strGroupName == strName)
    {
      return &*it;
    }
  }

  return NULL;
}

PVRLiveStreamsEpgChannel * PVRLiveStreamsData::FindEpg(const std::string &strId)
{
  vector<PVRLiveStreamsEpgChannel>::iterator it;
  for (it = m_epg.begin(); it < m_epg.end(); it++)
  {
    if (it->strId == strId)
    {
      return &*it;
    }
  }

  return NULL;
}

PVRLiveStreamsEpgChannel * PVRLiveStreamsData::FindEpgForChannel(PVRLiveStreamsChannel &channel)
{
  vector<PVRLiveStreamsEpgChannel>::iterator it;
  for (it = m_epg.begin(); it < m_epg.end(); it++)
  {
    if (it->strId == channel.strTvgId)
    {
      return &*it;
    }
    CStdString strName = it->strName;
    strName.Replace(' ', '_');
    if (strName == channel.strTvgName
      || it->strName == channel.strTvgName)
    {
      return &*it;
    }
    if (it->strName == channel.strChannelName)
    {
      return &*it;
    }
  }

  return NULL;
}

/*
 * This method uses zlib to decompress a gzipped file in memory.
 * Author: Andrew Lim Chong Liang
 * http://windrealm.org
 */
bool PVRLiveStreamsData::GzipInflate(const std::string& compressedBytes, std::string& uncompressedBytes) {

#define HANDLE_CALL_ZLIB(status) {  \
  if(status != Z_OK) {              \
    free(uncomp);                   \
    return false;                   \
    }                               \
}

  if (compressedBytes.size() == 0)
  {
    uncompressedBytes = compressedBytes;
    return true;
  }

  uncompressedBytes.clear();

  unsigned full_length = compressedBytes.size();
  unsigned half_length = compressedBytes.size() / 2;

  unsigned uncompLength = full_length;
  char* uncomp = (char*)calloc(sizeof(char), uncompLength);

  z_stream strm;
  strm.next_in = (Bytef *)compressedBytes.c_str();
  strm.avail_in = compressedBytes.size();
  strm.total_out = 0;
  strm.zalloc = Z_NULL;
  strm.zfree = Z_NULL;

  bool done = false;

  HANDLE_CALL_ZLIB(inflateInit2(&strm, (16 + MAX_WBITS)));

  while (!done)
  {
    // If our output buffer is too small  
    if (strm.total_out >= uncompLength)
    {
      // Increase size of output buffer  
      uncomp = (char *)realloc(uncomp, uncompLength + half_length);
      if (uncomp == NULL)
        return false;
      uncompLength += half_length;
    }

    strm.next_out = (Bytef *)(uncomp + strm.total_out);
    strm.avail_out = uncompLength - strm.total_out;

    // Inflate another chunk.  
    int err = inflate(&strm, Z_SYNC_FLUSH);
    if (err == Z_STREAM_END)
      done = true;
    else if (err != Z_OK)
    {
      break;
    }
  }

  HANDLE_CALL_ZLIB(inflateEnd(&strm));

  for (size_t i = 0; i < strm.total_out; ++i)
  {
    uncompressedBytes += uncomp[i];
  }

  free(uncomp);
  return true;
}

int PVRLiveStreamsData::GetCachedFileContents(const std::string &strCachedName, const std::string &filePath,
  std::string &strContents, const bool bUseCache /* false */)
{
  bool bNeedReload = false;
  CStdString strCachedPath = GetUserFilePath(strCachedName);
  CStdString strFilePath = filePath;

  // check cached file is exists
  if (bUseCache && XBMC->FileExists(strCachedPath, false))
  {
    struct __stat64 statCached;
    struct __stat64 statOrig;

    XBMC->StatFile(strCachedPath, &statCached);
    XBMC->StatFile(strFilePath, &statOrig);

    bNeedReload = statCached.st_mtime < statOrig.st_mtime || statOrig.st_mtime == 0;
  }
  else
  {
    bNeedReload = true;
  }

  if (bNeedReload)
  {
    GetFileContents(strFilePath, strContents);

    // write to cache
    if (bUseCache && strContents.length() > 0)
    {
      void* fileHandle = XBMC->OpenFileForWrite(strCachedPath, true);
      if (fileHandle)
      {
        XBMC->WriteFile(fileHandle, strContents.c_str(), strContents.length());
        XBMC->CloseFile(fileHandle);
      }
    }
    return strContents.length();
  }

  return GetFileContents(strCachedPath, strContents);
}

bool PVRLiveStreamsData::SplitURL(const std::string& uri, std::string& protocol, std::string& host, std::string& port, std::string& path)
{
  typedef std::string::const_iterator iterator_t;

  if (uri.length() == 0)
    return false;

  iterator_t uriEnd = uri.end();

  // protocol
  iterator_t protocolStart = uri.begin();
  iterator_t protocolEnd = std::find(protocolStart, uriEnd, ':'); //"://");

  if (protocolEnd != uriEnd)
  {
    std::string prot = &*(protocolEnd);
    if ((prot.length() > 3) && (prot.substr(0, 3) == "://"))
    {
      protocol = std::string(protocolStart, protocolEnd);
      protocolEnd += 3;   //      ://
    }
    else
    {
      protocol = "http";
      protocolEnd = uri.begin();  // no protocol
    }
  }
  else
    protocolEnd = uri.begin();  // no protocol

  // host
  iterator_t hostStart = protocolEnd;
  iterator_t pathStart = std::find(hostStart, uriEnd, '/');  // get pathStart

  iterator_t hostEnd = std::find(protocolEnd, pathStart, ':'); // check for port

  if (hostStart == hostEnd)
    return false;

  host = std::string(hostStart, hostEnd);

  // port
  if ((hostEnd != uriEnd) && ((&*(hostEnd))[0] == ':'))  // we have a port
  {
    hostEnd++;
    port = std::string(hostEnd, pathStart);
  }
  else
    port = "80";

  // path
  if (pathStart != uriEnd)
    path = std::string(pathStart, uri.end());
  else
    path = "/";

  return true;
}   // Parse

bool PVRLiveStreamsData::DownloadURL(const std::string& host, unsigned short port, const std::string& path, const std::vector<std::pair<std::string, std::string> >& headers, const std::string& data, std::string& response)
{
  char buffer[8 * 1024];
  int nDataLength;
  size_t i;
  std::string request;

  PLATFORM::CTcpSocket conn(host, port);

  if (!conn.Open(10000))
    return false;
  
  request = "GET " + path + " HTTP/1.1\r\n";
  for (i = 0; i < headers.size(); i++)
    request += headers[i].first + ": " + headers[i].second + "\r\n";
  request += "\r\n";

  if ((size_t)conn.Write((void*)request.c_str(), request.length()) != request.length())
    return false;
  
  if (data.length())
    if ((size_t)conn.Write((void*)data.c_str(), data.length()) != data.length())
      return false;
  
  for (;;)
  {
    nDataLength = conn.ReadSome(buffer, sizeof(buffer) - 1, 5000);
    if (nDataLength <= 0)
      break;
    
    buffer[nDataLength] = 0;
    response += buffer;
  }

  conn.Close();

  return true;
}

bool PVRLiveStreamsData::GetRegexParsed(std::string& url, RegexParamsTable& regexs)
{
  std::map<std::string, std::string> cachedPages;
  struct slre_cap cap;
  std::string link;

  while (slre_match("\\$doregex\\[([^\\]]*)\\]", url.c_str(), url.length(), &cap, 1, 0) >= 0)
  {
    std::string strMatch(cap.ptr, cap.len);
    std::string strMatchFull = "$doregex[" + strMatch + "]";

    RegexParamsTable::iterator regexParamsIter = regexs.find(strMatch);

    if (regexParamsIter == regexs.end())
      return false;

    RegexParams& regexParams = regexParamsIter->second;

    if (regexParams.find("page") == regexParams.end() || regexParams.find("expres") == regexParams.end())
      return false;

    if (cachedPages.find(regexParams["page"]) == cachedPages.end())
    {
      std::vector<std::pair<std::string, std::string> > headers;
      std::string response;

      std::string protocol, host_name, port, path, request;

      if (!SplitURL(regexParams["page"], protocol, host_name, port, path))
      {
        XBMC->Log(LOG_ERROR, "SplitURL failed!");
        return false;
      }

      if (regexParams.find("host") != regexParams.end())
        headers.push_back(std::make_pair("Host", regexParams["host"]));
      else
        headers.push_back(std::make_pair("Host", host_name));

      if (regexParams.find("agent") != regexParams.end())
        headers.push_back(std::make_pair("Agent", regexParams["agent"]));

      if (regexParams.find("referer") != regexParams.end())
        headers.push_back(std::make_pair("Referer", regexParams["referer"]));

      if (regexParams.find("connection") != regexParams.end())
        headers.push_back(std::make_pair("Connection", regexParams["connection"]));
      else
        headers.push_back(std::make_pair("Connection", "close"));

      if (!DownloadURL(host_name, (unsigned short)atoi(port.c_str()), path, headers, regexParams["data"], response))
      {
        XBMC->Log(LOG_ERROR, "DownloadURL failed!");
        return false;
      }

      size_t pos = response.find("\r\n\r\n");
      if (pos == std::string::npos)
        return false;

      link = response.c_str() + pos + 4;
      cachedPages[regexParams["page"]] = link;
    }
    else
      link = cachedPages[regexParams["page"]];

    XBMC->Log(LOG_NOTICE, "Searching for %s", regexParams["expres"].c_str());

    memset(&cap, 0, sizeof(cap));
    if (slre_match(regexParams["expres"].c_str(), link.c_str(), link.length(), &cap, 1, 0) < 0)
      return false;

    if (cap.ptr == NULL)
      return false;

    std::string data(cap.ptr, cap.len);

    //if (regexParams.find("function") != regexParams.end() && regexParams["function"] == "unquote")
    //    data = urllib.unquote(data)

    size_t pos = url.find(strMatchFull);
    if (pos == std::string::npos)
      return false;

    url.replace(pos, strMatchFull.length(), data);
  }

  return true;
}

int PVRLiveStreamsData::GetChannelId(const char * strChannelName, const char * strStreamUrl)
{
  std::string concat(strChannelName);
  concat.append(strStreamUrl);

  const char* strString = concat.c_str();
  int iId = 0;
  while (*strString)
    iId = ((iId << 5) + iId) + (*strString++); /* iId * 33 + c */

  return abs(iId);
}