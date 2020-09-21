#include <ctime>
#include <iostream>
#include <string>
#include <boost/bind/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
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


// 继承enable_shared_from_this和使用shared_ptr的作用是：只要有引用它的操作，我们就希望tcp_connection对象保持活动状态。
class tcp_connection
    : public boost::enable_shared_from_this<tcp_connection>
{
public:
    typedef boost::shared_ptr<tcp_connection> pointer;

    static pointer create(boost::asio::io_context& io_context)
    {
        return pointer(new tcp_connection(io_context));
    }

    tcp::socket& socket()
    {
        return socket_;
    }

    // 在函数start中,调用async_write将数据提供给客户端。不调用async_write_some以确保发送了整个数据块。
    void start()
    {
        // 要发送的数据存储在类成员message_中，因为我们需要保持数据有效，直到异步操作完成。
        message_ = make_daytime_string();

        boost::asio::async_write(socket_, boost::asio::buffer(message_),
            boost::bind(&tcp_connection::handle_write, shared_from_this(), // shared_from_this() 相当于this指针
                boost::asio::placeholders::error,
                boost::asio::placeholders::bytes_transferred));
    }

private:
    // 构造函数声明为私有，所以只能通过create创建tcp_connection对象
    tcp_connection(boost::asio::io_context& io_context)
        : socket_(io_context)
    {
    }

    // 此客户端连接的所有其他操作都由handle_write 负责。这个程序中不需要其他操作
    void handle_write(const boost::system::error_code& /*error*/,
        size_t /*bytes_transferred*/)
    {
    }

    tcp::socket socket_;
    std::string message_;
};

class tcp_server
{
public:
    tcp_server(boost::asio::io_context& io_context) : io_context_(io_context),
        acceptor_(io_context, tcp::endpoint(tcp::v4(), 13))
    {
        start_accept();
    }

private:
    // 函数start_accept() 创建一个socket并启动异步接受操作以等待新的连接。
    void start_accept()
    {
        tcp_connection::pointer new_connection = tcp_connection::create(io_context_);
        // handle_accept 为类成员函数，需要传入一个this指针当第一个参数
        acceptor_.async_accept(new_connection->socket(),
            boost::bind(&tcp_server::handle_accept, this, new_connection,
                boost::asio::placeholders::error));
    }

    // async_accept的回调函数，如果等到有请求，调用此函数
    // async_accept为客户端请求提供服务，处理完毕后再次调用start_accept（）发起下一个接受操作。
    void handle_accept(tcp_connection::pointer new_connection,
        const boost::system::error_code& error)
    {
        if (!error)
        {
            new_connection->start();
        }

        start_accept();
    }

    boost::asio::io_context& io_context_;
    tcp::acceptor acceptor_;
};

int main()
{
    try
    {
        boost::asio::io_context io_context;
        tcp_server server(io_context);
        // 进行异步操作
        io_context.run();
    }
    catch (std::exception& e)
    {
        std::cerr << e.what() << std::endl;
    }

    return 0;
}