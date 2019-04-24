
# apollo C++ client


根据 Apollo官方git上面的[其它语言客户端接入指南](https://github.com/ctripcorp/apollo/wiki/%E5%85%B6%E5%AE%83%E8%AF%AD%E8%A8%80%E5%AE%A2%E6%88%B7%E7%AB%AF%E6%8E%A5%E5%85%A5%E6%8C%87%E5%8D%97#13-%E9%80%9A%E8%BF%87%E4%B8%8D%E5%B8%A6%E7%BC%93%E5%AD%98%E7%9A%84http%E6%8E%A5%E5%8F%A3%E4%BB%8Eapollo%E8%AF%BB%E5%8F%96%E9%85%8D%E7%BD%AE)官方文档实现了：

> 1.4 应用感知配置更新

> 1.3 通过不带缓存的Http接口从Apollo读取配置

> 1.2 通过带缓存的Http接口从Apollo读取配置

具体如何接入应用，需要参考config_change_notify函数，当配置发生更新时，通知到应用（这里没有实现，不同应用实现的方式可能不太一样，个人倾向于类似ZK+QConf的方案）。

应用参考test.cpp

## 流程
1. 启动时判断本机IP地址，如果要实现灰度发布，这个IP地址需要跟apollo server获取到的IP地址一致，否则灰度不可用。
2. 注册appId、cluster，同时可以注册多个namespace
3. 运行run循环，一直监听配置更新，在更新后调用注册的回调函数更新到应用（自己实现）


未实现：
1. 由于等待apollo配置更新是long polling机制会阻塞，所以需要根据应用场景开启线程; 如果要同时监听多个appId,cluster也需要多线程（apollo接口限制）
2. 通知到应用，应用通常都是多线程/线程池，如何保证一个处理过程中配置数据的一致性
