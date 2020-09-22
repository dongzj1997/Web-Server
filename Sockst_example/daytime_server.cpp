// #define _CRT_SECURE_NO_WARNINGS
#include <ctime>
#include <iostream>
#include <string>
#include <boost/asio.hpp>

using boost::asio::ip::tcp;

std::string make_daytime_string()
{
    char s[50];
    using namespace std; // For time_t, time and ctime;
    time_t now = time(0);
    ctime_s(s, sizeof(s), &now);
    // return ctime(&now);
    return s;
}

int main()
{
    try
    {
        boost::asio::io_context io_context;

        // 创建一个acceptor对象来侦听新的连接。它初始化为侦听TCP端口13（用于IP版本4）
        tcp::acceptor acceptor(io_context, tcp::endpoint(tcp::v4(), 13));

        for (;;)
        {
            // 同步服务器，将一次处理一个连接。创建一个套接字，它将表示与客户端的连接，然后等待连接。
            tcp::socket socket(io_context);

            // 如果没有客户端创建socket，则程序一直阻塞在这一步等待
            acceptor.accept(socket); // 阻塞的方式

            std::string message = make_daytime_string();

            boost::system::error_code ignored_error; // 如果正常的话 ignored_error 的值是 system:0
            boost::asio::write(socket, boost::asio::buffer(message), ignored_error); 
            // 运行到这里，出了代码块以后socket会销毁，连接关闭
        }
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}