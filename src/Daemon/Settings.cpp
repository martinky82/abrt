#include "Settings.h"
#include <fstream>
#include <stdlib.h>

#define SECTION_COMMON      "Common"
#define SECTION_ANALYZER_ACTIONS_AND_REPORTERS   "AnalyzerActionsAndReporters"
#define SECTION_CRON        "Cron"


set_strings_t g_settings_setOpenGPGPublicKeys;
set_strings_t g_settings_mapSettingsBlackList;
set_strings_t g_settings_setEnabledPlugins;
unsigned int  g_settings_nMaxCrashReportsSize = 1000;
bool          g_settings_bOpenGPGCheck = false;
std::string   g_settings_sDatabase;
map_cron_t    g_settings_mapCron;
vector_pair_string_string_t g_settings_vectorActionsAndReporters;
map_analyzer_actions_and_reporters_t g_settings_mapAnalyzerActionsAndReporters;


typedef std::map<std::string, std::string> map_settings_t;
static map_settings_t s_mapSettingsCommon;
static map_settings_t s_mapSettingsAnalyzerActionsAndReporters;
static map_settings_t s_mapSettingsCron;

static set_strings_t ParseList(const std::string& pList)
{
   unsigned int ii;
   std::string item  = "";
   set_strings_t set;
   for(ii = 0; ii < pList.size(); ii++)
   {
       if (pList[ii] == ',')
       {
           set.insert(item);
           item = "";
       }
       else
       {
          item += pList[ii];
       }
   }
   if (item != "")
   {
       set.insert(item);
   }
   return set;
}

static vector_pair_string_string_t ParseListWithArgs(const std::string& pValue)
{
    vector_pair_string_string_t pluginsWithArgs;
    unsigned int ii;
    std::string item = "";
    std::string action = "";
    bool is_quote = false;
    bool is_arg = false;
    for (ii = 0; ii < pValue.size(); ii++)
    {
        if (pValue[ii] == '\"')
        {
            is_quote = is_quote == true ? false : true;
            item += pValue[ii];
        }
        else if (pValue[ii] == '(' && !is_quote)
        {
            action = item;
            item = "";
            is_arg = true;
        }
        else if (pValue[ii] == ')' && is_arg && !is_quote)
        {
            pluginsWithArgs.push_back(make_pair(action, item));
            item = "";
            is_arg = false;
            action = "";
        }
        else if (pValue[ii] == ',' && !is_quote && !is_arg)
        {
            if (item != "")
            {
                pluginsWithArgs.push_back(make_pair(item, ""));
                item = "";
            }
        }
        else
        {
            item += pValue[ii];
        }
    }
    if (item != "")
    {
        pluginsWithArgs.push_back(make_pair(item, ""));
    }
    return pluginsWithArgs;
}

static void ParseCommon()
{
    map_settings_t::const_iterator it = s_mapSettingsCommon.find("OpenGPGCheck");
    map_settings_t::const_iterator end = s_mapSettingsCommon.end();
    if (it != end)
    {
        g_settings_bOpenGPGCheck = it->second == "yes";
    }
    it = s_mapSettingsCommon.find("OpenGPGPublicKeys");
    if (it != end)
    {
        g_settings_setOpenGPGPublicKeys = ParseList(it->second);
    }
    it = s_mapSettingsCommon.find("BlackList");
    if (it != end)
    {
        g_settings_mapSettingsBlackList = ParseList(it->second);
    }
    it = s_mapSettingsCommon.find("Database");
    if (it != end)
    {
        g_settings_sDatabase = it->second;
    }
    it = s_mapSettingsCommon.find("EnabledPlugins");
    if (it != end)
    {
        g_settings_setEnabledPlugins = ParseList(it->second);
    }
    it = s_mapSettingsCommon.find("MaxCrashReportsSize");
    if (it != end)
    {
        g_settings_nMaxCrashReportsSize = atoi(it->second.c_str());
    }
    it = s_mapSettingsCommon.find("ActionsAndReporters");
    if (it != end)
    {
        g_settings_vectorActionsAndReporters = ParseListWithArgs(it->second);
    }
}

static void ParseCron()
{
    map_settings_t::iterator it = s_mapSettingsCron.begin();
    for (; it != s_mapSettingsCron.end(); it++)
    {
        vector_pair_string_string_t actionsAndReporters = ParseListWithArgs(it->second);
        g_settings_mapCron[it->first] = actionsAndReporters;
    }
}

static set_strings_t ParseKey(const std::string& Key)
{
   unsigned int ii;
   std::string item  = "";
   std::string key = "";
   set_strings_t set;
   bool is_quote = false;
   for(ii = 0; ii < Key.size(); ii++)
   {
       if (Key[ii] == '\"')
       {
          is_quote = is_quote == true ? false : true;
       }
       else if (Key[ii] == ':' && !is_quote)
       {
           key = item;
           item = "";
       }
       else if ((Key[ii] == ',') && !is_quote)
       {
           set.insert(key + ":" + item);
           item = "";
       }
       else
       {
           item += Key[ii];
       }
    }
    if (item != "" && !is_quote)
    {
        if (key == "")
        {
            set.insert(item);
        }
        else
        {
            set.insert(key + ":" + item);
        }
    }
    return set;
}

static void ParseAnalyzerActionsAndReporters()
{
    map_settings_t::iterator it = s_mapSettingsAnalyzerActionsAndReporters.begin();
    for (; it != s_mapSettingsAnalyzerActionsAndReporters.end(); it++)
    {
        set_strings_t keys = ParseKey(it->first);
        vector_pair_string_string_t actionsAndReporters = ParseListWithArgs(it->second);
        set_strings_t::iterator it_keys = keys.begin();
        for (; it_keys != keys.end(); it_keys++)
        {
            g_settings_mapAnalyzerActionsAndReporters[*it_keys] = actionsAndReporters;
        }
    }
}

void LoadSettings(const char* pPath)
{
    std::ifstream fIn;
    fIn.open(pPath);
    if (fIn.is_open())
    {
        std::string line;
        std::string section = "";
        while (!fIn.eof())
        {
            getline(fIn, line);

            unsigned int ii;
            bool is_key = true;
            bool is_section = false;
            bool is_quote = false;
            std::string key = "";
            std::string value = "";
            for (ii = 0; ii < line.length(); ii++)
            {
                if (isspace(line[ii]) && !is_quote)
                {
                    continue;
                }
                else if (line[ii] == '#' && !is_quote && key == "")
                {
                    break;
                }
                else if (line[ii] == '[' && !is_quote)
                {
                    is_section = true;
                    section = "";
                }
                else if (line[ii] == '\"')
                {
                    is_quote = is_quote == true ? false : true;
                    value += line[ii];
                }
                else if (is_section)
                {
                    if (line[ii] == ']')
                    {
                        break;
                    }
                    section += line[ii];
                }
                else if (line[ii] == '=' && !is_quote)
                {
                    is_key = false;
                    key = value;
                    value = "";
                }
                else
                {
                    value += line[ii];
                }
            }
            if (!is_key && !is_quote)
            {
                if (section == SECTION_COMMON)
                {
                    if (s_mapSettingsCommon[key] != "")
                    {
                        s_mapSettingsCommon[key] += ",";
                    }
                    s_mapSettingsCommon[key] += value;
                }
                else if (section == SECTION_ANALYZER_ACTIONS_AND_REPORTERS)
                {
                    if (s_mapSettingsAnalyzerActionsAndReporters[key] != "")
                    {
                        s_mapSettingsAnalyzerActionsAndReporters[key] += ",";
                    }
                    s_mapSettingsAnalyzerActionsAndReporters[key] += value;
                }
                else if (section == SECTION_CRON)
                {
                    if (s_mapSettingsCron[key] != "")
                    {
                        s_mapSettingsCron[key] += ",";
                    }
                    s_mapSettingsCron[key] += value;
                }
            }
        }
        fIn.close();
    }
    ParseCommon();
    ParseAnalyzerActionsAndReporters();
    ParseCron();
}

static void SaveSetString(const std::string& pKey, const set_strings_t& pSet, std::ofstream& pFOut, bool pNewLine = true)
{
    if (pKey != "")
    {
        pFOut << pKey << " = ";
    }
    int ii = 0;
    set_strings_t::const_iterator it_set = pSet.begin();
    for (; it_set != pSet.end(); it_set++)
    {
        pFOut << (*it_set);
        ii++;
        if (ii < pSet.size())
        {
            pFOut << ",";
        }
    }
    if (pNewLine)
    {
        pFOut << std::endl;
    }
}

static void SaveVectorPairStrings(const std::string& pKey, const vector_pair_string_string_t& pVector, std::ofstream& pFOut, bool pNewLine = true)
{
    int ii;
    if (pKey != "")
    {
        pFOut << pKey << " = ";
    }
    for (ii = 0; ii < pVector.size(); ii++)
    {
        pFOut << pVector[ii].first;
        if (pVector[ii].second != "")
        {
            pFOut << "(" << pVector[ii].second << ")";
        }
        if ((ii + 1) < pVector.size())
        {
            pFOut << ",";
        }
    }
    if (pNewLine)
    {
        pFOut << std::endl;
    }
}

static void SaveMapVectorPairStrings(const map_vector_pair_string_string_t& pMap, std::ofstream& pFOut, bool pNewLine = true)
{
    map_vector_pair_string_string_t::const_iterator it = pMap.begin();
    for (; it != pMap.end(); it++)
    {
        pFOut << it->first << " = ";
        SaveVectorPairStrings("", it->second, pFOut, false);
        pFOut << std::endl;
    }
    if (pNewLine)
    {
        pFOut << std::endl;
    }

}

static void SaveSection(const std::string& pSection, std::ofstream& pFOut)
{
    pFOut << std::endl << "[" << pSection << "]" << std::endl << std::endl;
}

static void SaveBool(const std::string& pKey, const bool pBool, std::ofstream& pFOut, bool pNewLine = true)
{
    if (pKey != "")
    {
        pFOut << pKey << " = ";
    }
    pFOut << (pBool ? "yes" : "no");
    if (pNewLine)
    {
        pFOut << std::endl;
    }
}

void SaveSettings(const char* pPath)
{
    std::ofstream fOut;
    fOut.open(pPath);

    if (fOut.is_open())
    {
        SaveSection(SECTION_COMMON, fOut);
        SaveBool("OpenGPGCheck", g_settings_bOpenGPGCheck, fOut);
        SaveSetString("OpenGPGPublicKeys", g_settings_setOpenGPGPublicKeys, fOut);
        SaveSetString("BlackList", g_settings_mapSettingsBlackList, fOut);
        SaveSetString("EnabledPlugins", g_settings_setEnabledPlugins, fOut);
        fOut << "Database = " << g_settings_sDatabase << std::endl;
        fOut << "MaxCrashReportsSize = " << g_settings_nMaxCrashReportsSize << std::endl;
        SaveVectorPairStrings("ActionsAndReporters", g_settings_vectorActionsAndReporters, fOut);
        SaveSection(SECTION_ANALYZER_ACTIONS_AND_REPORTERS, fOut);
        SaveMapVectorPairStrings(g_settings_mapAnalyzerActionsAndReporters, fOut);
        SaveSection(SECTION_CRON, fOut);
        SaveMapVectorPairStrings(g_settings_mapCron, fOut);
        fOut.close();
    }
}

void SetSettings(const map_abrt_settings_t& pSettings)
{
    map_abrt_settings_t::const_iterator it = pSettings.find(SECTION_COMMON);
    if (it != pSettings.end())
    {
        s_mapSettingsCommon = it->second;
        ParseCommon();
    }
    it = pSettings.find(SECTION_ANALYZER_ACTIONS_AND_REPORTERS);
    if (it != pSettings.end())
    {
        s_mapSettingsAnalyzerActionsAndReporters = it->second;
        ParseAnalyzerActionsAndReporters();
    }
    it = pSettings.find(SECTION_CRON);
    if (it != pSettings.end())
    {
        s_mapSettingsCron = it->second;
        ParseCron();
    }
}

map_abrt_settings_t GetSettings()
{
    map_abrt_settings_t ABRTSettings;

    ABRTSettings[SECTION_COMMON] = s_mapSettingsCommon;
    ABRTSettings[SECTION_ANALYZER_ACTIONS_AND_REPORTERS] = s_mapSettingsAnalyzerActionsAndReporters;
    ABRTSettings[SECTION_CRON] = s_mapSettingsCron;

    return ABRTSettings;
}

