#include "httpserver.hpp"

#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>

int main() {

    unsigned short port = 8080;
    size_t num_threads = 1;

    string config_path = "./config.json";
    ifstream config(config_path, ios::in);
    if (config) {
        std::stringstream ss;
        ss << config.rdbuf();
        config.close();
        try {
            boost::property_tree::ptree pt;
            read_json(ss, pt);

            port = pt.get<unsigned short>("port");
            num_threads = pt.get<size_t>("num_threads");
        }
        catch (const std::exception& e) {
            std::cerr << e.what() << std::endl;
        }
    }

    boost::asio::io_context io;

    std::cout << "httpserver in port: " << port << std::endl;
    HTTPServer httpserver(io, port, num_threads);
    httpserver.start();
    
    return 0;
}