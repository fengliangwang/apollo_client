
#include <string>
#include <iostream>
#include <unordered_map>

#include <stdlib.h>
#include <time.h>

#include "notifications.h"

// TODO: 根据业务情况对配置项进行更新
static void config_change_notify(const std::string& appId, const std::string& cluster, const std::string& ns, const std::unordered_map<std::string, std::string>& kvs) {
    for(auto& p : kvs) {
        std::cout<<"APP "<<appId<<" "<<cluster<<" "<<ns<<" "<<p.first<<" -> "<<p.second<<std::endl;
    }
}

int main() {

    int res;
    std::string err;

    std::set<std::string> subnetMaskPrefix{"xx.xx.xx"};  // 根据IP前缀控制IP（这里取了前3个段）, xx.xx.xx.xx, 也可以静态指定(不推荐)
    std::string clientIp = apollo::client::getLocalIP(subnetMaskPrefix);

    std::string servicesConfigUrl = "http://xx.xx.xx.xx:80/services/config";
    std::string appId{"news"};
    std::string cluster{"newsapi"};
    std::vector<std::string> namespaces{"application", "news.channels"};

    apollo::client::ApolloConfigServer::run(
            servicesConfigUrl,
            appId,
            cluster,
            namespaces,
            clientIp,
            std::bind(config_change_notify, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3, std::placeholders::_4));

    return 0;
}

