/*************************************************************************
    > File Name: config.h
    > Author: hsz
    > Brief:
    > Created Time: 2022-03-27 18:58:20 Sunday
 ************************************************************************/

#ifndef __EULAR_P2P_CONFIG_H__
#define __EULAR_P2P_CONFIG_H__

#include <utils/exception.h>
#include <utils/singleton.h>
#include <utils/string8.h>
#include <utils/mutex.h>
#include <yaml-cpp/yaml.h>

namespace eular {

namespace TypeCast {
template<typename T>
T Chars2Other(const char *src)
{
    T t = T();
    eular::String8 str;
    str.appendFormat("invalid type. %s", typeid(T).name());
    throw eular::Exception(str);
    return t;
}
}

class Config
{
public:
    void Init(const String8 &path, bool usePath = true);
    bool isVaild() const { return mValid; }
    const String8& getErrorMsg() const { return mErrorMsg; }
    void foreach();

    template<typename T>
    static T Lookup(const String8& key, const T& default_val)
    {
        RDAutoLock<RWMutex> rdlock(mMutex);
        auto it = mConfigMap.find(key);
        if (it != mConfigMap.end()) {
            return TypeCast::Chars2Other<T>(it->second.Scalar().c_str());
        }

        return default_val;
    }

private:
    Config();
    Config(const String8 &path);
    ~Config();
    void LoadYaml(const String8& prefix, const YAML::Node& node);

private:
    String8                                 mYamlConfigPath;       // yaml配置文件绝对路径
    YAML::Node                              mYamlRoot;
    static std::map<String8, YAML::Node>    mConfigMap;

    bool            mValid;
    String8         mErrorMsg;
    static RWMutex  mMutex;
    friend class Singleton<Config>;
};


typedef Singleton<Config> ConfigManager;

} // namespace eular

#endif // __EULAR_P2P_CONFIG_H__
