/*************************************************************************
    > File Name: config.cpp
    > Author: hsz
    > Brief:
    > Created Time: 2022-03-27 18:58:51 Sunday
 ************************************************************************/

#include "config.h"
#include <log/log.h>
#include <iostream>
#include <sstream>
#include <string>

#define LOG_TAG "config"

namespace eular {

namespace TypeCast {
// 模板全特化
template<>
short Chars2Other<short>(const char *src)
{
    return atoi(src);
}

template<>
unsigned short Chars2Other<unsigned short>(const char *src)
{
    return static_cast<unsigned short>(atoi(src));
}

template<>
int Chars2Other<int>(const char *src)
{
    return atoi(src);
}

template<>
unsigned int Chars2Other<unsigned int>(const char *src)
{
    return static_cast<unsigned int>(atoi(src));
}

template<>
float Chars2Other<float>(const char *src)
{
    return atof(src);
}

template<>
double Chars2Other<double>(const char *src)
{
    return static_cast<double>(atof(src));
}

template<>
long Chars2Other<long>(const char *src)
{
    return atol(src);
}

template<>
unsigned long Chars2Other<unsigned long>(const char *src)
{
    return static_cast<unsigned long>(atol(src));
}

template<>
bool Chars2Other<bool>(const char *src)
{
    bool flag;
    flag = strcasecmp(src, "true") == 0;
    return flag;
}

template<>
const char *Chars2Other<const char *>(const char *src)
{
    return src;
}

template<>
eular::String8 Chars2Other<eular::String8>(const char *src)
{
    return String8(src);
}

} // namespace TypeCast

std::map<String8, YAML::Node> Config::mConfigMap;
RWMutex Config::mMutex;

Config::Config() :
    mValid(false)
{

}

Config::Config(const String8 &path) :
    mYamlConfigPath(path),
    mValid(false)
{
    Init(mYamlConfigPath, false);
}

Config::~Config()
{

}

void Config::Init(const String8 &path, bool usePath)
{
    if (usePath) {
        mYamlConfigPath = path;
    }

    mValid = false;
    mConfigMap.clear();
    try {
        mYamlRoot = YAML::LoadFile(mYamlConfigPath.c_str());
        WRAutoLock<RWMutex> wrlock(mMutex);
        LoadYaml("", mYamlRoot);
        mValid = true;
    } catch (const std::exception &e){
        mErrorMsg = e.what();
    }
}

void Config::LoadYaml(const String8& prefix, const YAML::Node& node)
{
    if (prefix.toStdString().find_first_not_of("abcdefghijklmnopqrstuvwxyz._1234567890")
        != std::string::npos) {
        LOGE("Config invalid prefix: %s", prefix.c_str());
        return;
    }
    mConfigMap.insert(std::make_pair(prefix, node));
    if (node.IsMap()) {
        for (auto it = node.begin(); it != node.end(); ++it) {
            LoadYaml(prefix.isEmpty() ? it->first.Scalar() :
                prefix + "." + it->first.Scalar(), it->second);
        }
    }
}

void Config::foreach()
{
    std::stringstream strstream;
    for (auto it = mConfigMap.begin(); it != mConfigMap.end(); ++it) {
        // std::cout << it->first << ": " << it->second << std::endl;
        strstream << it->first.c_str() << it->second << std::endl;
    }
    LOGD("%s", strstream.str().c_str());
}

} // namespace eular
