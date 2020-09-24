#ifndef HTTPSERVER_HPP
#define	HTTPSERVER_HPP

#include <iostream>
#include <regex>
#include <unordered_map>
#include <thread>

#include <boost/asio.hpp>
#include <boost/filesystem.hpp>


using std::string;
using std::shared_ptr;
using std::unordered_map;
using std::function;
using std::istream;
using std::ostream;
using std::smatch;
using std::ifstream;
using std::ios;

using namespace boost::asio;


struct Request {
    string method, path, http_version;
    shared_ptr<istream> content;
    unordered_map<string, string> header;
};

class HTTPServer {
public:
    unordered_map<string, unordered_map<string, function<void(ostream&, const Request&, const smatch&)>>> resources_;
    
    HTTPServer(boost::asio::io_context&, unsigned short, size_t);
    
    void start();
            
private:
    io_context &io_;
    ip::tcp::endpoint endpoint_;
    ip::tcp::acceptor acceptor_;
    size_t num_threads_;
    std::vector<std::thread> threads_;

    void accept();

    void process_request_and_respond(shared_ptr<ip::tcp::socket> socket);
    
    void respond(shared_ptr<ip::tcp::socket> socket, shared_ptr<Request> request);

    Request parse_request(istream& stream);

};

#endif	/* HTTPSERVER_HPP */

