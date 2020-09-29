#include "httpserver.hpp"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

int main() {

    unsigned short port = 8080;
    size_t num_threads = 1;
    size_t request_timeout = 5;
    size_t content_timeout = 300;

    string config_path = "./config.json";
    ifstream config(config_path, ios::in);
    if (config) {
        std::stringstream ss;
        ss << config.rdbuf();
        config.close();
        try {
            boost::property_tree::ptree pt;
            read_json(ss, pt);

            port =            pt.get<unsigned short>("port");
            num_threads =     pt.get<size_t>("num_threads");
            request_timeout = pt.get<size_t>("request_timeout");
            content_timeout = pt.get<size_t>("content_timeout");
        }
        catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
        }
    }

    boost::asio::io_context io;

    std::cout << "httpserver in port: " << port << std::endl;
    std::cout << "request_timeout is : " << request_timeout;
    std::cout << ", content_timeout is : " << content_timeout << std::endl;
    HTTPServer httpserver(io, port, num_threads, request_timeout, content_timeout);
    httpserver.start();
    
    return 0;
}