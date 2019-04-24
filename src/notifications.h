
#include <set>
#include <string>
#include <vector>
#include <sstream>
#include <unordered_map>
#include <functional>

#include <stdlib.h>

#include <ifaddrs.h>
#include <arpa/inet.h>

#include "jsons.h"
#include "url_decode.h"
#include "http_request.h"

namespace apollo {
  namespace client {

static std::string getLocalIP(const std::set<std::string>& subnetMask) {
    struct ifaddrs* ifAddrStruct = NULL;
    struct ifaddrs* ifa = NULL;
    void* tmpAddrPtr = NULL;

    std::vector<std::string> ips;
    char addressBuffer[INET6_ADDRSTRLEN] = {0};

    getifaddrs(&ifAddrStruct);

    for (ifa = ifAddrStruct; ifa != NULL; ifa = ifa->ifa_next) {
        if (!ifa->ifa_addr) {
            continue;
        }
        if (ifa->ifa_addr->sa_family == AF_INET) { // IP4 Address
            tmpAddrPtr=&((struct sockaddr_in *)ifa->ifa_addr)->sin_addr;
            inet_ntop(AF_INET, tmpAddrPtr, addressBuffer, INET_ADDRSTRLEN);
            std::string ip(addressBuffer);
            std::size_t pos = ip.rfind(".");
            if(pos == std::string::npos)
                continue;
            if(!subnetMask.empty() &&
                subnetMask.find(ip.substr(0, pos)) == subnetMask.end())    // 子网掩码非空，且不存在则过滤
                continue;
            ips.emplace_back(std::move(ip));
        } else if (ifa->ifa_addr->sa_family == AF_INET6) { // IP6 Address
            tmpAddrPtr=&((struct sockaddr_in6 *)ifa->ifa_addr)->sin6_addr;
            inet_ntop(AF_INET6, tmpAddrPtr, addressBuffer, INET6_ADDRSTRLEN);
            std::string ip(addressBuffer);
            std::size_t pos = ip.rfind(".");
            if(pos == std::string::npos)
                continue;
            if(!subnetMask.empty() && subnetMask.find(ip.substr(0, pos)) == subnetMask.end())    // 子网掩码非空，且不存在则过滤
                continue;
            ips.emplace_back(std::move(ip));
        }
    }
    if (ifAddrStruct != NULL)
        freeifaddrs(ifAddrStruct);

    std::string clientIp{""};
    if(!ips.empty())
        clientIp = ips[0];
    return clientIp;
}

class notification {
public:

    notification(const std::string& ns="application", int nid=-1)
      : namespaceName_(ns), notificationId_(nid) {
    }

    std::string toJSON() const {
        std::stringstream ss;
        ss<<"{\"namespaceName\":\""<<namespaceName_<<"\",\"notificationId\":"<<notificationId_<<"}";
        return ss.str();
    }
    void set(const std::string& ns, int nid) {
        namespaceName_ = ns;
        notificationId_ = nid;
    }
private:
    std::string namespaceName_{"application"};
    int notificationId_{-1};
};

class notifications {
public:

    void add(const std::string& ns, int nid=-1) {
        auto it = notifications_.find(ns);
        if(it == notifications_.end()) {
            notification no{ns, nid};
            notifications_.emplace(ns, std::move(no));
            //std::cout<<"NOTIFICATION ADD "<<ns<<" "<<nid<<std::endl;
        } else {
            notifications_[ns].set(ns, nid);
            //std::cout<<"NOTIFICATION UPDATE "<<ns<<" "<<nid<<std::endl;
        }
    }

    std::string toJSON() const {
        if(notifications_.empty()) return "[]";

        std::stringstream ss;
        for(auto& p : notifications_) {
            ss<<p.second.toJSON()<<",";
        }
        std::string s = ss.str();
        s.resize(s.size()-1);
        s = "[" + s + "]";
        return s;
    }

private:
    std::unordered_map<std::string, notification> notifications_;
};


class ApolloNotification {
public:

    ApolloNotification() {}
    ApolloNotification(const std::string& configServerUrl, const std::string& appId, const std::string& clusterName) {
        set(configServerUrl, appId, clusterName);
    }

    void set(const std::string& configServerUrl, const std::string& appId, const std::string& clusterName) {
        configServerUrl_ = configServerUrl;
        appId_ = appId;
        clusterName_ = clusterName;
    }
    void add(const std::string& ns, int nid=-1) {
        notifications_.add(ns, nid);
    }

    static void ParseKey(const std::string& key, std::string& appId, std::string& cluster, std::string& ns) {
        // "10000+diamond+application":10
        std::size_t pos1 = key.find("+");
        if(pos1 != std::string::npos) {
            appId = key.substr(0, pos1);
        }
        std::size_t pos2 = key.find("+", pos1+1);
        if(pos2 != std::string::npos) {
            cluster = key.substr(pos1+1, pos2-pos1-1);
        }
        ns = key.substr(pos2+1);
    }

    static std::string sReplace(const std::string& s, const std::string& sub, const std::string& rep) {
        std::string cp{s};
        size_t pos = cp.find(sub);
        if(pos != std::string::npos) {
            cp.replace(pos, sub.size(), rep);
        }
        return cp;
    }

    int parse(const std::string& resp, std::unordered_map<std::string, std::string>& updated_uris) {
        rapidjson::Document doc;
        if(!JSON::Parse(doc, resp) || !doc.IsArray()) {
            return -1;
        }
        for (rapidjson::SizeType i = doc.Size(); i>0; i--) {
            rapidjson::Value& jitem = doc[i-1];

            //std::string namespaceName = JSON::GetString(jitem, "namespaceName", "");
            //long notificationId = JSON::GetInt(jitem, "notificationId", -1);
            rapidjson::Value& jmessages = jitem["messages"];
            rapidjson::Value& jdetails = jmessages["details"];

            rapidjson::Value::ConstMemberIterator itr = jdetails.MemberBegin();
            for (; itr != jdetails.MemberEnd(); ++itr) {
                std::string key = itr->name.GetString();
                long notificationId = itr->value.GetInt();

                std::string appId, cluster, ns;
                ParseKey(key, appId, cluster, ns);
                if(appId == appId_ && clusterName_ == cluster && !ns.empty()) {
                    add(ns, notificationId);

                    std::string updated_uri = appId + "/" + cluster + "/" + ns;
                    updated_uris[updated_uri] = ns;
                }
            }
        }
        return 0;
    }

    std::string toUrl() const {
        std::stringstream ss;
        ss  << configServerUrl_<<"/notifications/v2"
            << "?appId="<<appId_
            << "&cluster="<<clusterName_
            << "&notifications="<<UrlEncode(notifications_.toJSON());
        return ss.str();
    }

    int requestServer(const std::string& url, std::string& resp, std::string& err, int to_seconds=65) {
        return utils::HttpRequest::Get(url, resp, &err, to_seconds);
    }

private:
    std::string configServerUrl_{"<configServerUrl>"};
    std::string appId_{"<appId>"};
    std::string clusterName_{"default"};
    apollo::client::notifications notifications_;
};

class ApolloServicesConfig {
public:

    ApolloServicesConfig() {
    }

    int requestServer(const std::string& servicesConfigUrl, std::string& resp, std::string& err, int to_seconds=5) {
        // [{"appName":"APOLLO-CONFIGSERVICE","instanceId":"xxx","homepageUrl":"http://xx.xx.xx.xx/"}]
        return utils::HttpRequest::Get(servicesConfigUrl, resp, &err, to_seconds);
    }

    int parse(const std::string& resp, std::string& err) {
        rapidjson::Document doc;
        if(!JSON::Parse(doc, resp) || !doc.IsArray()) {
            return -1;
        }
        std::vector<std::string> homePageUrls;
        for (rapidjson::SizeType i = doc.Size(); i>0; i--) {
            rapidjson::Value& jitem = doc[i-1];

            std::string appName = JSON::GetString(jitem, "appName", "");
            if(appName == "APOLLO-CONFIGSERVICE") {
                std::string homepageUrl = JSON::GetString(jitem, "homepageUrl", "");
                if(!homepageUrl.empty() && homepageUrl[homepageUrl.size()-1] == '/') {
                    homepageUrl.resize(homepageUrl.size()-1);
                }
                homePageUrls.push_back(homepageUrl);
            }
        }
        homePageUrls_.swap(homePageUrls);
        return 0;
    }
    
    std::string getUrl() const {
        if(homePageUrls_.empty()) return "";
        srandom(time(NULL));
        long int rnd = random() % homePageUrls_.size();
        return homePageUrls_[rnd];
    }

private:
    std::vector<std::string> homePageUrls_;
};

class ApolloConfigfiles {
public:
    ApolloConfigfiles() {
    }


    static void url_add_param(std::string& url, const std::string& key, const std::string& val) {
        size_t pos = url.find(key+"=");
        if(pos == std::string::npos) {
            size_t found = url.find('?');
            if(found == std::string::npos) {
                url = url + "?" + key + "=" + val; 
            } else {
                url = url + "&" + key + "=" + val; 
            }
            return;
        }
        size_t i = pos;
        for(; i<url.size(); i++) {
            if(url[i] == '&') {
                break;
            }
        }
        url.replace(pos, i-pos, key+"="+val);
    }

    int getConfigfiles(const std::string& configServer, const std::string& uri, std::unordered_map<std::string, std::string>& kvs, std::string& err, const std::string& ip="", int to_seconds=5) {
        // http://xx.xx.xx.xx/configfiles/json/10000/diamond/application
        std::stringstream ss;
        ss<<configServer<<"/configfiles/json/"<<uri;
        std::string url = ss.str();

        if(!ip.empty())
            url_add_param(url, "ip", ip);

        std::string resp;
        int res = utils::HttpRequest::Get(url, resp, &err, to_seconds);
        if(res != 0) {
            std::cerr<<"CONFIGS failed "<<res<<" "<<resp<<" "<<err;
            return res;
        }
        if(resp.empty()) {
            err = "empty";
            return -1;
        }
        res = parse(resp, kvs);

        return res;
    }

    int getConfigs(const std::string& configServer, const std::string& uri, std::unordered_map<std::string, std::string>& kvs, std::string& err, const std::string& ip="", int to_seconds=5) {
        // http://xx.xx.xx.xx/configs/10000/diamond/application
        std::stringstream ss;
        ss<<configServer<<"/configs/"<<uri;
        std::string url = ss.str();

        if(!releaseKey_.empty())
            url_add_param(url, "releaseKey", releaseKey_);
        if(!ip.empty())
            url_add_param(url, "ip", ip);

        std::string resp;
        int res = utils::HttpRequest::Get(url, resp, &err, to_seconds); // TODO: check http code: 304
        if(res != 0) {
            std::cerr<<"CONFIGS failed "<<res<<" "<<resp<<" "<<err;
            return res;
        }
        if(resp.empty()) {
            err = "empty";
            return -1;
        }
        res = parse(resp, kvs);

        return res;
    }

    int parse(const std::string& resp, std::unordered_map<std::string, std::string>& kvs) {
        rapidjson::Document doc;
        if(!JSON::Parse(doc, resp)) {
            return -1;
        }
        if(JSON::Exist(doc, "releaseKey")) {
            releaseKey_ = JSON::GetString(doc, "releaseKey", "");
        }
        if(JSON::Exist(doc, "configurations")) {
            rapidjson::Value& jconfigurations = doc["configurations"];
            return parse(jconfigurations, kvs);
        }
        return parse(doc, kvs);
    }

    int parse(rapidjson::Value& jconfigurations, std::unordered_map<std::string, std::string>& kvs) {
        rapidjson::Value::ConstMemberIterator itr = jconfigurations.MemberBegin();
        for (; itr != jconfigurations.MemberEnd(); ++itr) {
            std::string key = itr->name.GetString();
            std::string value = itr->value.GetString();
            kvs[key] = value;
        }
        return 0;
    }

private:
    std::string releaseKey_{""}; // 仅适用于/configs/10000/diamond/application接口

};

class ApolloConfigServer {
public:

    static void run(const std::string& servicesConfigUrl,
                    const std::string& appId,
                    const std::string& clusterName,
                    const std::vector<std::string>& namespaces,
                    const std::string& clientIp,
                    std::function<void (const std::string& appId, const std::string& cluster, const std::string& ns, const std::unordered_map<std::string, std::string>& priorities)> configsChangeNotifyCallback) {
        int res;
        std::string err;

        std::string _clusterName = clusterName;
        if(_clusterName.empty())
            _clusterName = "default";

        long update_sevices_config = 0;
        std::string configServerUrl;

        ApolloServicesConfig servicesConfig;
        ApolloNotification nos{configServerUrl, appId, _clusterName};
        ApolloConfigfiles configs;

        if(namespaces.empty()) {
            nos.add("application", -1);
        } else {
            for(auto& ns : namespaces) {
                nos.add(ns, -1);
            }
        }

        while(true) {
            long now = time(NULL);
            if(now - update_sevices_config > 600) {
                std::string servicesConfigResp;
                res = servicesConfig.requestServer(servicesConfigUrl, servicesConfigResp, err);
                res = servicesConfig.parse(servicesConfigResp, err);
                configServerUrl = servicesConfig.getUrl();
                update_sevices_config = now;
            }
            nos.set(configServerUrl, appId, _clusterName);

            std::string notification_resp;
            res = nos.requestServer(nos.toUrl(), notification_resp, err);

            std::unordered_map<std::string, std::string> updated_uris;
            if(res == 0) {
                nos.parse(notification_resp, updated_uris);
                for(auto& p : updated_uris) {
                    const std::string& updated_uri = p.first;
                    const std::string& ns = p.second;
                    std::unordered_map<std::string, std::string> kvs;
                    configs.getConfigfiles(configServerUrl, updated_uri, kvs, err, clientIp);
                    //configs.getConfigs(configServerUrl, updated_uri, kvs, err, clientIp);

                    // callback(appId, cluster, kvs)
                    if(configsChangeNotifyCallback)
                        configsChangeNotifyCallback(appId, _clusterName, ns, kvs);
                }
            } else {
                std::cerr<<"nos.requestServer failed"<<std::endl;
            }

            

        }
    }
};

}
}
