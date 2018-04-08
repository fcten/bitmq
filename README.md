# BitMQ

BitMQ 是一款开源消息队列与消息代理服务端程序。

BitMQ 支持在 GNU/Linux 以及 Windows 上运行。BitMQ 目前在 Windows 下使用 select 模型，不建议用于高负载环境。

## 编译 BitMQ [![Build Status](https://travis-ci.org/fcten/webit.svg?branch=master)](https://travis-ci.org/fcten/webit)

编译 BitMQ 需要安装 openssl (>=1.0.2，用以支持 HTTPS)，zlib (>=1.2.0.4，用以支持 GZIP)。您可以使用包管理工具安装或者下载源代码编译安装

    centos/redhat# yum install openssl-devel openssl zlib-devel zlib
    debian/ubuntu# apt-get install libssl-dev openssl zlib1g-dev zlib1g

BitMQ 使用 CMake (>=2.8) 进行构建。您可以在源码目录下使用以下命令构建

    mkdir build
    cd build
    cmake ..
    make

您也可以在 Winodws 下使用 CMake 以及 Visual Studio 进行构建，或者直接下载已经编译好的可执行文件。

## 使用 BitMQ

### 运行

使用当前工作路径下的配置文件 (bmq.conf) 启动

    ./bmq

使用指定的配置文件启动

    ./bmq -c /opt/bmq/1039.conf

注意，您可以使用多个配置文件在同一台服务器上启动多个 BitMQ 实例。但这些配置文件中设定的端口必须各不相同。

以后台模式启动

    ./bmq -d

### 停止

首先找到 master 进程的 pid，例如

    root@localhost:~# ps -ef | grep bmq
    root      118675   1553  0 14:36 ?        00:00:00 bmq: master process (/opt/bmq/bmq.conf)
    www-data  118676 118675  0 14:36 ?        00:00:00 bmq: worker process


然后执行

    kill 118675

注意，此时 worker 进程并不会立刻退出，而是停止 accept 新连接。直到所有已建立的连接处理完毕并正常关闭后， worker 进程才会退出。这最长需要等待 keep-alive-timeout 的时间。

注意，直接对 worker 进程执行 kill 并不能关闭 BitMQ。master 进程会在 worker 进程退出后立即重新 fork 出一个 worker 进程。

### 重启

首先找到 master 进程的 pid，然后执行

    kill 118675
    ./bmq -d

执行该命令后会产生新的 master 进程和 worker 进程。旧进程会在所有已建立的连接处理完毕并正常关闭后退出。

### 平滑重启

首先找到 master 进程的 pid，然后执行

    kill -USR2 118675 && kill 118675

注意：为了在不关闭程序的情况下更新二进制文件，您总是应当使用符号链接来启动 BitMQ。
注意：如果启用了数据持久化功能，请不要使用该方式重启程序，否则可能会造成数据丢失。

## 性能测试

以下测试结果在虚拟机环境下取得。

HTTP 短连接 ( ab -n 500000 -c 10 http://127.0.0.1/ )：

    Server Software:        BitMQ
    Server Hostname:        127.0.0.1
    Server Port:            1039

    Document Path:          /
    Document Length:        2308 bytes

    Concurrency Level:      10
    Time taken for tests:   17.853 seconds
    Complete requests:      500000
    Failed requests:        0
    Total transferred:      1280000000 bytes
    HTML transferred:       1154000000 bytes
    Requests per second:    28007.07 [#/sec] (mean)
    Time per request:       0.357 [ms] (mean)
    Time per request:       0.036 [ms] (mean, across all concurrent requests)
    Transfer rate:          70017.68 [Kbytes/sec] received

    Connection Times (ms)
                  min  mean[+/-sd] median   max
    Connect:        0    0   0.0      0       7
    Processing:     0    0   0.2      0      11
    Waiting:        0    0   0.2      0      11
    Total:          0    0   0.2      0      11

    Percentage of the requests served within a certain time (ms)
      50%      0
      66%      0
      75%      0
      80%      0
      90%      0
      95%      0
      98%      1
      99%      1
     100%     11 (longest request)


    Server Software:        nginx/1.10.3
    Server Hostname:        127.0.0.1
    Server Port:            80

    Document Path:          /
    Document Length:        2308 bytes

    Concurrency Level:      10
    Time taken for tests:   17.813 seconds
    Complete requests:      500000
    Failed requests:        0
    Total transferred:      1275500000 bytes
    HTML transferred:       1154000000 bytes
    Requests per second:    28069.69 [#/sec] (mean)
    Time per request:       0.356 [ms] (mean)
    Time per request:       0.036 [ms] (mean, across all concurrent requests)
    Transfer rate:          69927.52 [Kbytes/sec] received

    Connection Times (ms)
                  min  mean[+/-sd] median   max
    Connect:        0    0   0.1      0       8
    Processing:     0    0   0.1      0       9
    Waiting:        0    0   0.1      0       9
    Total:          0    0   0.1      0       9

    Percentage of the requests served within a certain time (ms)
      50%      0
      66%      0
      75%      0
      80%      0
      90%      0
      95%      0
      98%      1
      99%      1
     100%      9 (longest request)

HTTP 长连接 ( ab -k -n 500000 -c 10 http://127.0.0.1/ )：

    Server Software:        BitMQ
    Server Hostname:        127.0.0.1
    Server Port:            1039

    Document Path:          /
    Document Length:        2308 bytes

    Concurrency Level:      10
    Time taken for tests:   10.062 seconds
    Complete requests:      500000
    Failed requests:        0
    Keep-Alive requests:    500000
    Total transferred:      1282500000 bytes
    HTML transferred:       1154000000 bytes
    Requests per second:    49692.49 [#/sec] (mean)
    Time per request:       0.201 [ms] (mean)
    Time per request:       0.020 [ms] (mean, across all concurrent requests)
    Transfer rate:          124473.86 [Kbytes/sec] received

    Connection Times (ms)
                  min  mean[+/-sd] median   max
    Connect:        0    0   0.0      0       1
    Processing:     0    0   0.1      0      10
    Waiting:        0    0   0.1      0      10
    Total:          0    0   0.1      0      10

    Percentage of the requests served within a certain time (ms)
      50%      0
      66%      0
      75%      0
      80%      0
      90%      0
      95%      0
      98%      0
      99%      0
     100%     10 (longest request)


    Server Software:        nginx/1.10.3
    Server Hostname:        127.0.0.1
    Server Port:            80

    Document Path:          /
    Document Length:        2308 bytes

    Concurrency Level:      10
    Time taken for tests:   19.471 seconds
    Complete requests:      500000
    Failed requests:        0
    Keep-Alive requests:    495003
    Total transferred:      1277975015 bytes
    HTML transferred:       1154000000 bytes
    Requests per second:    25679.27 [#/sec] (mean)
    Time per request:       0.389 [ms] (mean)
    Time per request:       0.039 [ms] (mean, across all concurrent requests)
    Transfer rate:          64096.61 [Kbytes/sec] received

    Connection Times (ms)
                  min  mean[+/-sd] median   max
    Connect:        0    0   0.0      0       0
    Processing:     0    0   1.3      0      28
    Waiting:        0    0   1.3      0      28
    Total:          0    0   1.3      0      28

    Percentage of the requests served within a certain time (ms)
      50%      0
      66%      0
      75%      0
      80%      0
      90%      0
      95%      0
      98%      1
      99%      7
     100%     28 (longest request)

注意：在以上测试中提供 nginx 的数据只是为硬件性能提供一个参考。BitMQ 并不是一个完善的 HTTP 服务端。

消息发送 ( 关闭持久化与日志 )：

    Time taken for tests:   3.21 seconds
    Complete publish:       1000000
    Failed publish:         0
    Total transferred:      89.65 Mbytes
    Message transferred:    86.78 Mbytes
    Publish per second:     311817.91 [#/sec]
    Time per publish:       0.00 [ms]
    Transfer rate:          27.95 [Mbytes/sec]

以上测试使用 JSON 格式发送消息，如果使用二进制格式，性能可进一步提升。
当开启日志或持久化功能时，消息吞吐量将主要受限于磁盘写入性能。

## 更新日志

### V0.6.0

 * Feature: 添加 WebSocket 协议支持
 * Feature: 添加授权认证和权限控制

### V0.5.0

 * Feature: 添加 BMTP 协议支持
 * Feature: 添加对 Windows 的支持

### V0.4.0

 * Feature: 新增 status 统计与运维接口
 * Feature: 优化了红黑树和定时器相关操作
 * Feature: 新增持久化支持
 * Feature: 新增 ACK 消息类型

## 授权协议

BitMQ 遵循 BSD 许可证。

## 获取 BitMQ

* http://www.bitmq.com/
