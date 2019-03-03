# librpc
Component (module, plugin, process  (like dbus)) rpc server daemon and client library，
make component communicating easier,using epoll event with unix socket and internet tcp/udp socket and etc.

特性说明：
1. 支持模块间，进程间，跨网络的主机的远程调用，让通信方式更加简单，不用关注底层通信的实现
2. 支持客户端服务注册认证，目前暂时支持CHAP认证方式，保证远程调用的客户端是可信的一方
3. 支持HEAD + DATA协议方式保证数据边界，支持数据部分二进制格式，JSON格式，XML格式序列化
4. 支持Epoll事件驱动，支持unix，netlink，tcp，udp socket等多种通信方式，可根据场景灵活选取
5. 支持接收发送方向的多线程处理方案，可根据配置文件修改线程数，适配SMP主机多线程性能提升
6. 支持进程，多线程绑核方案，通过roundbin分配连接，充分利用CPU资源，让各个线程达到负载均衡 
   (其他：随机、一致性哈希、roundbin三种负载均衡算法)
7. 支持多IO聚合提升性能，支持大IO的半发送的处理
8. 客户端和服务端支持0-8K的预先分配+动态分配的内存缓存分配，有效提高分配效率和减少内存碎片
9. 客户端支持超时同步和异步两种远程调用方式

待实现：
1. 客户端支持限流和及时熔断
2. 支持事件消息订阅
3. 支持主从节点故障迁移（zookeeper，redis etc）
4. 支持客户端协议透明通信代理

简单测试：

