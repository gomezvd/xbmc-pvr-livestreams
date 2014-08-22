#pragma once
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

#include <vector>
#include <map>
#include "platform/os.h"
#include "platform/util/StdString.h"
#include "client.h"
#include "platform/threads/threads.h"

typedef std::map<std::string, std::string> RegexParams;
typedef std::map<std::string, RegexParams> RegexParamsTable;

struct PVRLiveStreamsEpgEntry
{
  int iBroadcastId;
  int iChannelId;
  int iGenreType;
  int iGenreSubType;
  time_t startTime;
  time_t endTime;
  std::string strTitle;
  std::string strPlotOutline;
  std::string strPlot;
  std::string strIconPath;
  std::string strGenreString;
};

struct PVRLiveStreamsEpgChannel
{
  std::string strId;
  std::string strName;
  std::vector<PVRLiveStreamsEpgEntry> epg;
};

struct PVRLiveStreamsChannel
{
  bool bRadio;
  int iUniqueId;
  int iChannelNumber;
  int iEncryptionSystem;
  int iTvgShift;
  std::string strChannelName;
  std::string strLogoPath;
  std::string strLink;
  std::string strStreamURL;
  std::string strTvgId;
  std::string strTvgName;

  RegexParamsTable regexs;
};

struct PVRLiveStreamsChannelGroup
{
  bool bRadio;
  int iGroupId;
  std::string strGroupName;
  std::vector<int> members;
};

class PVRLiveStreamsData : public PLATFORM::CThread
{
public:
  PVRLiveStreamsData(void);
  virtual ~PVRLiveStreamsData(void);

  virtual int GetChannelsAmount(void);
  virtual PVR_ERROR GetChannels(ADDON_HANDLE handle, bool bRadio);
  virtual bool GetChannel(const PVR_CHANNEL &channel, PVRLiveStreamsChannel &myChannel);
  virtual int GetChannelGroupsAmount(void);
  virtual PVR_ERROR GetChannelGroups(ADDON_HANDLE handle, bool bRadio);
  virtual PVR_ERROR GetChannelGroupMembers(ADDON_HANDLE handle, const PVR_CHANNEL_GROUP &group);
  virtual PVR_ERROR GetEPGForChannel(ADDON_HANDLE handle, const PVR_CHANNEL &channel, time_t iStart, time_t iEnd);
  virtual const char* GetChannelURL(const PVR_CHANNEL &channel);

protected:
  virtual bool LoadPlayList(void);
  virtual bool LoadEPG(time_t iStart, time_t iEnd);
  virtual int  GetFileContents(CStdString& url, std::string &strContent);
  virtual PVRLiveStreamsChannel *FindChannel(const std::string &strId, const std::string &strName);
  virtual PVRLiveStreamsChannelGroup *FindGroup(const std::string &strName);
  virtual PVRLiveStreamsEpgChannel *FindEpg(const std::string &strId);
  virtual PVRLiveStreamsEpgChannel *FindEpgForChannel(PVRLiveStreamsChannel &channel);
  virtual int ParseDateTime(CStdString strDate, bool iDateFormat = true);
  virtual bool GzipInflate( const std::string &compressedBytes, std::string &uncompressedBytes);
  virtual int GetCachedFileContents(const std::string &strCachedName, const std::string &strFilePath, std::string &strContent, const bool bUseCache = false);

protected:
  virtual void* Process(void);

public:
  bool SplitURL(const std::string& uri, std::string& protocol, std::string& host, std::string& port, std::string& path);
  bool DownloadURL(const std::string& host, unsigned short port, const std::string& path, const std::vector<std::pair<std::string, std::string> >& headers, const std::string& data, std::string& response);
  bool GetRegexParsed(std::string& url, RegexParamsTable& regexs);

private:
  bool m_bTSOverride;
  bool m_bEGPLoaded;
  int m_iEPGTimeShift;
  int m_iLastStart;
  int m_iLastEnd;
  CStdString m_strXMLTVUrl;
  CStdString m_strXMLUrl;
  std::vector<PVRLiveStreamsChannelGroup> m_groups;
  std::vector<PVRLiveStreamsChannel> m_channels;
  std::vector<PVRLiveStreamsEpgChannel> m_epg;
};
