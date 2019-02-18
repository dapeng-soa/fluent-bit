![](fluentbit_logo.png)

[Fluent Bit](http://fluentbit.io) is a Data Forwarder for Linux, Embedded Linux, OSX and BSD family operating systems. It's part of the [Fluentd](http://fluentd.org) Ecosystem.  Fluent Bit allows collection of information from different sources, buffering and dispatching them to different outputs such as [Fluentd](http://fluentd.org), Elasticsearch, Nats or any HTTP end-point within others. It's fully supported on x86_64, x86 and ARM architectures.

For more details about it capabilities and general features please visit the official documentation:

http://fluentbit.io/documentation

## 说明

1.在原[fluent-bit](https://github.com/fluent/fluent-bit.git)的基础上，主要对其修改使其实现主从host的转发（forward）功能。原fluent-bit只支持向一个host进行转发，进行修改后，添加了一个从host（hoststandby)，当主host宕掉以后会通过hoststandby建立连接进行转发，当主host恢复后再建立主连接进行转发。

2.修改日志转发逻辑：原有修改后的转发逻辑是当主host宕掉以后，通过hoststandby建立连接转发，当主host恢复后便立即使用主host建立连接进行转发。现在更改为当主host宕掉后，通过hoststandby建立连接进行转发，即使主host重新恢复也不会立即使用host进行转发，只有当hoststandby宕掉后才会再次使用主host建立连接进行转发。

3.对input插件中的tail进行了改造：在没有配置DB的情况下，修改为默认从文件末尾进行读取；在配置了DB的情况下，对配置文件添加db_count字段，通过该字段可以设置将文件偏移量offset写入DB的频率。原生将offset写入DB的策略为：读取一次缓存块就进行一次写入，在这种策略下，会引起高频率的IO操作。

## 环境配置
在linux下编译构建fluent-bit
>如果开发环境是在Windows下,那么可以通过docker启动一个centos容器，在centos容器中进行编译执行，并实现本地文件与容器文件的共享，便于本地对容器中fluent-bit源代码的修改
```
//将本地文件夹/c:/Users/Administrator/Desktop/share映射到centos容器中/root/share的文件夹下，并将容器centos另命名为fbit

    $ winpty docker run -v /c:/Users/Administrator/Desktop/share:/root/share -it --name=fbit centos bash
```
1.安装git gcc gcc-c++ cmake make，用于对项目进行相关的git操作和编译执行操作。

    `$ yum install -y git gcc gcc-c++ cmake make`
 
2.下载源码git clone fluent-bit(在实现与本地进行共享的文件夹/root/share中进行操作，便于本地对源代码的修改)

    `$ git clone http://pms.today36524.com.cn:8083/basic-services/fluent-bit`
    
## 编译与执行
```
$ cd build
$ cmake ..
$ make
$ bin/fluent-bit -i cpu -o stdout
```
上述例子将CPU信息标准输出

## Features

[Fluent Bit](http://fluentbit.io) support the following features through plugins:

### Input plugins

| name               | option  | description  |
|--------------------|---------|---------------------------------------------------------------------------------|
| CPU                | cpu     | gather CPU usage between snapshots of one second. It support multiple cores     |
| Disk               | disk    | usage of block device |
| Dummy              | dummy   | generates dummy event |
| Exec               | exec    | executes external program and collects event logs |
| Forward            | forward | [Fluentd](http://fluentd.org) forward protocol |
| Memory             | mem     | usage of system memory |
| MQTT               | mqtt    | start a MQTT server and receive publish messages |
| Netif              | netif   | usage of network interface |
| Kernel Ring Buffer | kmsg    | read Linux Kernel messages, same behavior as the __dmesg__ command line program |
| Syslog             | syslog  | read messages from a syslog daemon | 
| Systemd/Journald   | systemd | read messages from journald, part of the systemd suite |
| Serial Port        | serial  | read from serial port |
| Standard Input     | stdin   | read from the standard input |
| Head               | head    | read first part of files |
| Health             | health  | check health of TCP services|
| Process            | proc    | check health of Process |
| Random             | random  | generate random numbers |
| Tail               | tail    | tail log files |
| TCP                | tcp     | listen for raw JSON map messages over TCP |

### Filter Plugins

| name               | option     | description  |
|--------------------|------------|---------------------------------------------------------------------------------|
| Record Modifier    | record_modifier | Append/Remove key-value pair |
| Grep               | grep       | Match or exclude specific records by patterns |
| Kubernetes         | kubernetes | Enrich logs with Kubernetes Metadata |
| Stdout             | stdout     | Print records to the standard output interface |
| Parser             | parser     | Parse records |


### Output Plugins

| name               | option                  | description  |
|--------------------|-------------------------|---------------------------------------------------------------------------------|
| Counter            | counter | count records |
| Elasticsearch      | es | flush records to a Elasticsearch server |
| File               | file | flush records to a file |
| FlowCounter        | flowcounter| count records and its size |
| Forward            | forward  | flush records to a [Fluentd](http://fluentd.org) service. On the [Fluentd](http://fluentd.org) side, it requires an __in_forward__.|
| NATS               | nats | flush records to a NATS server |
| HTTP               | http | flush records to a HTTP end point |
| InfluxDB           | influxdb | flush records to InfluxDB time series database |
| Plot               | plot | generate a file for gnuplot |
| Standard Output    | stdout                  | prints the records to the standard output stream |
| Treasure Data      | td                      | flush records to [Treasure Data](http://treasuredata.com) service (cloud analytics)|



## 主要修改

1.原fluent-bit以插件的形式支持许多特性。为了实现主从host的转发功能，主要对Output Plugins中的forward进行了修改。

1)、首先重新定义了forward的输出参数配置。原有的forward输出只定义了一个host和port，现在添加了hoststandby与portstandby：

    [OUTPUT]
       Name        forward
       Host        192.168.4.107
       Port        24223
       HostStandby        192.168.4.107
       PortStandby        60000

2).对源代码的修改

 - flb_output.h ---->添加备用的host   :   struct flb_net_host host_standby;
 
 - flb_output.c ---->根据config文件为hoststandby赋值：flb_output_set_property()
 
 - forward.h ---->添加备用的upstream  :	struct flb_upstream *u_standby;
 
 - forward.c -------> 创建备用的upstream hander :   cb_forward_init()
 
             -------> 当主host不可用时，建立hoststandby连接 ： cb_forward_flush()

2.对首次修改后的转发机制的修改。

 - forward.c -------> 添加标志flag ：cb_forward_flush()
 
             -------> 修改转发逻辑 ：cb_forward_flush()
			 
3.对Input Plugins中的tail进行了修改：在没有配置DB的情况下，从文件末尾读

 - tail_file.c ------> 修改文件读取位置和偏移量 ： flb_tail_file_append()
 
4.在配置了DB的情况下，添加db_count字段

 - tail_config.h ------> 添加db_count字段 ：struct flb_tail_config
 
 - tail.h ------> 添加默认写入频率 ： FLB_TAIL_DB_COUNT
 
 - tail_config.c -------> 根据config文件为db_count赋值 ：flb_tail_config_create()
 
 - tail_file.c ------> 添加写入次数变量 count ： flb_tail_file_chunk()
    
	           ------> 修改写入逻辑 ： flb_tail_file_chunk()

               ------> 当fluent-bit宕掉时，将未写入db的offset写入db : flb_tail_file_remove()
			   
修改后的配置文件格式如下：

    [INPUT]
       Name tail
       path ./logs/*
       db ./log.db
       db_count 40
	   
    [OUTPUT]
       Name        stdout
       match       *

5.修改两次读chunk的时间差大于60s时，将offset写入到db中，与db_count字段同时限制db中offset的写入

 - tail_file.c ------> 添加写入时间变量 timer ：flb_tail_file_chunk（）
              
               ------> 修改写入逻辑 ： flb_tail_file_chunk（）

6.修改解析文件（Parsers_File）支持从环境变量里面取值

 - flb_parser.c ------> 添加 flb_env.h 头文件
 
                ------> 当读入regex字段时，对该值进行translate ：flb_parser_conf_file（）

## 实例

列举一个简单的forward实例，该例子功能实现将cpu信息进行转发到定义的端口

1.定义配置文件flb.cong,该例子中只定义部分参数，其余参数详情可以查看原[fluent-bit](https://fluentbit.io/documentation/0.12/output/forward.html)相关文档

    [SERVICE]
       Flush           5
        Daemon          off
        Log_Level       debug

    [INPUT]
        Name cpu
        Tag  my_cpu

    [OUTPUT]
        Name        forward
        Host        172.0.0.1
        Port        24224
        HostStandby        192.168.4.36
        Portstandby        60000

2.执行（进入到fluent-bit/build/bin下，该命令需保证fluent-bit与flb.conf都在fluent-bit/build/bin目录下)

    $ fluent-bit -c flb.conf

3.结果 （在相应的host端口接收到的数据如下所示）

2018-04-27 10:45:01.000494600 +0800 my_cpu: {"cpu_p":0.5,"user_p":0.5,"system_p":0.0,"cpu0.p_cpu":0.0,"cpu0.p_user":0.0,"cpu0.p_system":0.0,"cpu1.p_cpu":1.0,"cpu1.p_user":1.0,"cpu1.p_system":0.0}
2018-04-27 10:45:02.000199800 +0800 my_cpu: {"cpu_p":2.0,"user_p":1.0,"system_p":1.0,"cpu0.p_cpu":3.0,"cpu0.p_user":1.0,"cpu0.p_system":2.0,"cpu1.p_cpu":1.0,"cpu1.p_user":1.0,"cpu1.p_system":0.0}
2018-04-27 10:45:03.000663600 +0800 my_cpu: {"cpu_p":0.5,"user_p":0.0,"system_p":0.5,"cpu0.p_cpu":1.0,"cpu0.p_user":0.0,"cpu0.p_system":1.0,"cpu1.p_cpu":0.0,"cpu1.p_user":0.0,"cpu1.p_system":0.0}
2018-04-27 10:45:04.000657400 +0800 my_cpu: {"cpu_p":0.5,"user_p":0.0,"system_p":0.5,"cpu0.p_cpu":1.0,"cpu0.p_user":0.0,"cpu0.p_system":1.0,"cpu1.p_cpu":0.0,"cpu1.p_user":0.0,"cpu1.p_system":0.0}
2018-04-27 10:45:05.000551800 +0800 my_cpu: {"cpu_p":1.0,"user_p":0.5,"system_p":0.5,"cpu0.p_cpu":2.0,"cpu0.p_user":1.0,"cpu0.p_system":1.0,"cpu1.p_cpu":0.0,"cpu1.p_user":0.0,"cpu1.p_system":0.0}
2018-04-27 10:45:06.000429100 +0800 my_cpu: {"cpu_p":1.0,"user_p":0.0,"system_p":1.0,"cpu0.p_cpu":2.0,"cpu0.p_user":0.0,"cpu0.p_system":2.0,"cpu1.p_cpu":0.0,"cpu1.p_user":0.0,"cpu1.p_system":0.0}
2018-04-27 10:45:07.000670700 +0800 my_cpu: {"cpu_p":0.0,"user_p":0.0,"system_p":0.0,"cpu0.p_cpu":0.0,"cpu0.p_user":0.0,"cpu0.p_system":0.0,"cpu1.p_cpu":0.0,"cpu1.p_user":0.0,"cpu1.p_system":0.0}
2018-04-27 10:45:08.000444200 +0800 my_cpu: {"cpu_p":1.5,"user_p":0.5,"system_p":1.0,"cpu0.p_cpu":3.0,"cpu0.p_user":1.0,"cpu0.p_system":2.0,"cpu1.p_cpu":0.0,"cpu1.p_user":0.0,"cpu1.p_system":0.0}
2018-04-27 10:45:09.000470000 +0800 my_cpu: {"cpu_p":1.0,"user_p":0.5,"system_p":0.5,"cpu0.p_cpu":1.0,"cpu0.p_user":0.0,"cpu0.p_system":1.0,"cpu1.p_cpu":1.0,"cpu1.p_user":1.0,"cpu1.p_system":0.0}
2018-04-27 10:45:10.000667000 +0800 my_cpu: {"cpu_p":0.5,"user_p":0.5,"system_p":0.0,"cpu0.p_cpu":1.0,"cpu0.p_user":1.0,"cpu0.p_system":0.0,"cpu1.p_cpu":0.0,"cpu1.p_user":0.0,"cpu1.p_system":0.0}

## Official Documentation

The official documentation of [Fluent Bit](http://fluentbit.io) can be found in the following site:

http://fluentbit.io/documentation/

## Contributing

In order to contribute to the project please refer to the [CONTRIBUTING](CONTRIBUTING.md) guidelines.

## Contact

Feel free to join us on our Slack channel, Mailing List or IRC:

 - Slack: http://slack.fluentd.org (#fluent-bit channel)
 - Mailing List: https://groups.google.com/forum/#!forum/fluent-bit
 - IRC: irc.freenode.net #fluent-bit
 - Twitter: http://twitter.com/fluentbit

## License

This program is under the terms of the [Apache License v2.0](http://www.apache.org/licenses/LICENSE-2.0).

## Authors

[Fluent Bit](http://fluentbit.io) is made and sponsored by [Treasure Data](http://treasuredata.com) among
other [contributors](https://github.com/fluent/fluent-bit/graphs/contributors).
