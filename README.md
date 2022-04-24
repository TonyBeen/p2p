# p2p
p2p server &amp; client

#### 依赖
> git clone git@github.com:TonyBeen/third_party.git
> git clone git@github.com:TonyBeen/eular.git

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
      host: 172.25.12.215
      port: 12000
      send_timeout: 1000
      recv_timeout: 500
    udp:
      host: 172.25.12.215
      port: 12500
      disconnection_timeout_ms: 3000
    redis:
      redis_amount: 6
      redis_host: 172.25.12.215
      redis_port: 6379
      redis_auth: 123456
    worker:
      io_worker_num: 4
      process_worker_num: 4

#### 编译
    make
    make clean
