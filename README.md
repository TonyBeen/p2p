# p2p
p2p server

#### 依赖
> `git clone git@github.com:TonyBeen/third_party.git`
> `git clone git@github.com:TonyBeen/eular.git`

#### 库依赖说明
`openssl libunwind hiredis yaml-cpp`

#### 配置

> `config.yaml`

    log:
      level: debug
      target: stdout|fileout
      sync: true
    epoll:
      event_size: 4096
    tcp:
      host: 127.0.0.1
      port: 12000
      send_timeout: 1000
      recv_timeout: 500
    udp:
      host: 127.0.0.1
      port: 12500
      disconnection_timeout_ms: 3000
    redis:
      redis_amount: 4         # redis实例数量，与io_worker_num数量保持一致即可
      redis_host: 127.0.0.1   # redis服务IP
      redis_port: 6379        # redis监听端口
      redis_auth: xxxxxx      # 密码
    worker:
      io_worker_num: 4        # IO事件处理线程数量
      process_worker_num: 4   # 一般事务处理线程数量

#### 编译
    make
    make clean

#### 引用
> 架构方面参考 https://github.com/sylar-yin/sylar
