#include <ctime>
#include <iostream>
#include <string>
#include <boost/array.hpp>
#include <boost/bind/bind.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>


using namespace boost;
class HttpConnection : public boost::enable_shared_from_this<HttpConnection>
{
public:
    typedef boost::shared_ptr<HttpConnection> pointer;

    static pointer create(boost::asio::io_context& io_context)
    {
        return pointer(new HttpConnection(io_context));
    }

    asio::ip::tcp::socket& socket() { return socket_; }

    void start()
    {
        asio::async_read_until(socket_, asio::dynamic_buffer(request_), "\r\n\r\n",
            boost::bind(&HttpConnection::handle_read_until, shared_from_this(), 
                boost::asio::placeholders::error));
    }

    

private:
    HttpConnection(boost::asio::io_context& io) : socket_(io) {}

    void handle_read_until(const boost::system::error_code& err) {
        if (err)
        {
            std::cerr << "err in handle_read_until:" << err.message() << std::endl;
            return;
        }
        std::string first_line = request_.substr(0, request_.find("\r\n")); // should be like: GET / HTTP/1.0
        std::cout << first_line << std::endl;

        char str[] = "HTTP/1.0 200 OK\r\n\r\n<html> Hello World ! </html>";
        asio::async_write(socket_, asio::buffer(str), boost::bind(&HttpConnection::handle_write, 
            shared_from_this(), 
                boost::asio::placeholders::error,
                    boost::asio::placeholders::bytes_transferred));
    }

    void handle_write(const boost::system::error_code& /*error*/, size_t /*bytes_transferred*/)
    {
        socket_.close();
    }

    asio::ip::tcp::socket socket_;
    std::string request_;
};

class HttpServer
{
public:
    HttpServer(asio::io_context& io, asio::ip::tcp::endpoint ep) : io_(io), acceptor_(io, ep) {
        start_accept();
    }

private:
    void start_accept()
    {
        // 用智能指针管理对象
        HttpConnection::pointer new_connection = HttpConnection::create(io_);
        // 这里的bind形式也可以改为lambda表达式
        acceptor_.async_accept(new_connection->socket(), boost::bind(&HttpServer::handle_accept, this, new_connection,
            boost::asio::placeholders::error
        ));
    }
    void handle_accept(HttpConnection::pointer new_connection,
        const boost::system::error_code& error) {
        if (!error)
        {
            new_connection->start();
        }
        else {
            std::cerr << "err in handle_accept:" << error.message() << std::endl;
        }

        start_accept();
    }

    asio::io_context& io_;  //引用类型
    asio::ip::tcp::acceptor acceptor_;
};

int main()
{
    using asio::ip::tcp;
    try {
        asio::io_context io;
        auto t = tcp::v4();
        HttpServer hs(io, tcp::endpoint(tcp::v4(), 8080));
        io.run();
    }
    catch(std::exception& e){
        std::cerr << e.what() << std::endl;
    }
    return 0;
}