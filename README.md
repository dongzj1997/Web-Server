# Web-Server (2) ---- Sockets 编程初步

A simple and fast HTTP server implemented using C++17 and Boost.Asio.

从零开始实现一个基于 `C++17` 和 `Boost.Asio` 并且简单快速的HTTP服务器。

---

**这篇文章用一个使用`asio`开发的简单`daytime`客户端和服务器程序来让我们快速了解`Sockets`编程的相关知识和`Asio`中相关库的使用。**

---

## 关于daytime服务

### 什么是daytime服务

维基百科中对daytime服务的描述是：

> The Daytime Protocol is a service in the Internet Protocol Suite, defined in 1983 in RFC 867. It is intended for testing and measurement purposes in computer networks.
>
>A host may connect to a server that supports the Daytime Protocol on either Transmission Control Protocol (TCP) or User Datagram Protocol (UDP) port 13. The server returns an ASCII character string of the current date and time in an unspecified format.

简单来说，daytime基于TCP或者UDP的一个应用，端口都是13，一般用于网络的调试，它的作用是返回当前时间和日期的字符串。

格式为：dd mmm yy hh:mm:ss zzz

比如：20 SEP 2020 21:54:44 CST 

## 如何启动daytime服务

在linux系统中，可以方便的启动该服务，可以使用以下命令

```sh
# 安装xinetd，如果为Contos 或者红帽系系列，可以使用yum
# 如果提示xinetd找不到，可以先用sudo apt update  更新一波
sudo apt install xinetd

# 执行该命令后会在/etc/xinetd.d/目录下生成一些配置文件
# 用vim打开，前面的一行的 “disable=yes”改为“disable=no”，如有需要，对UDP版本的也做同样的修改。
vim ./daytime

# 重启该服务
service xinetd restart 
```

效果如下图所示：

![img](./pic/xinetd.jpg)

![img](./pic/daytime.jpg)

![img](./pic/restart.jpg)


## 用Asio编写一个synchronous 的 TCP daytime 客户端

代码如下：

```c++
#include <iostream>
#include <boost/array.hpp>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

int main(int argc, char* argv[])
{
    try
    {
        // 参数只有一个，即为daytime服务器的ip地址，对应argv[1]
        // argv[0] 指向程序运行的全路径名
        if (argc != 2)
        {
            std::cerr << "Usage: client <host>" << std::endl;
            return 1;
        }

        boost::asio::io_context io_context;

        // 使用resolver将 参数指定的服务器名称转换为TCP端点，参数为  host名 + 服务协议名
        tcp::resolver resolver(io_context);
        // results_type类型其实是一个类似vector的东西，因为一个host可能对应多个ip（v4，v6）
        tcp::resolver::results_type endpoints = resolver.resolve(argv[1], "daytime");

        // 创建并连接socket。上面获得的端点列表可能同时包含IPv4和IPv6端点，因此我们需要尝试每个端点，直到找到可行的端点为止。
        // connect函数自动为我们执行此操作。
        tcp::socket socket(io_context);
        boost::asio::connect(socket, endpoints);

        //连接已打开。读取daytime服务的响应即可
        for (;;)
        {
            // 使用boost::array来保存接收到的数据。 char []或vector也可
            // buffer函数可以自动确定数组的大小，防止缓冲区溢出。
            boost::array<char, 128> buf;
            boost::system::error_code error;
            // read_some的返回值为读取数据的大小
            size_t len = socket.read_some(boost::asio::buffer(buf), error);

            if (error == boost::asio::error::eof)
                break; // 数据读完，跳出循环
            else if (error)
                throw boost::system::system_error(error); // Some other error.

            std::cout.write(buf.data(), len);
        }
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}
```

现在客户端的程序已经写好了，我们需要找一个daytime服务器来测试一下

在win10中，我们可以使用WSL2很容易的完成，安装方法这里不写了，比较简单。

在wsl中按照文章最开始的步骤启动daytime服务器，获得WSL的ip地址，这个就是我们需要填入的参数（注意一般是一个B类的内网保留地址，`172.16.0.0`-`172.31.255.255`，子网掩码是`255.240.0.0`）

配置好后，编译运行，在shell中或者在IDE中都可以，需要注意的是，如果在IDE中，需要人为的加入参数，按照下图的方式：

![img](./pic/cfg.jpg)

运行的结果如图(分别为IDE中和shell中)：

![img](./pic/result1.jpg)

![img](./pic/result2.jpg)

## 用Asio编写一个synchronous 的 TCP daytime 服务器

待续
