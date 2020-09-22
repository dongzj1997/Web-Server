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