#include "httpserver.hpp"

HTTPServer::HTTPServer(boost::asio::io_context& io, unsigned short port = 80, size_t num_threads = 1) 
    : io_(io), endpoint_(ip::tcp::v4(), port),
    acceptor_(io_, endpoint_), num_threads_(num_threads) {

    // 定义请求的方式

    //直接访问Host,比如 127.0.0.1:8080 ,没有具体的路径,返回请求的详情.
    this->resources_["^/$"]["GET"] = [](ostream& response, const Request& request, const smatch& path_match) {
        std::stringstream content_stream;
        content_stream << "<h1>Request:</h1>";
        content_stream << request.method << " " << request.path << " HTTP/" << request.http_version << "<br>";
        for (auto& header : request.header) {
            content_stream << header.first << ": " << header.second << "<br>";
        }

        //将stringstream指针移到末尾, 便于以后用tellp获取流的长度.
        content_stream.seekp(0, std::ios::end);

        response << "HTTP/1.1 200 OK\r\nContent-Length: " << content_stream.tellp() << "\r\n\r\n" << content_stream.rdbuf();
    };

    // 访问具体文件, 比如 http://127.0.0.1:8080/test.html
    this->resources_["^/.+$"]["GET"] = [](ostream& response, const Request& request, const smatch& path_match) {
        
        try {
            boost::filesystem::path web_root_path = boost::filesystem::canonical("web");
            boost::filesystem::path path = boost::filesystem::canonical(web_root_path / request.path);
            // 确保目录还在 web_root_path 下
            if (std::distance(web_root_path.begin(), web_root_path.end()) > std::distance(path.begin(), path.end()) ||
                !std::equal(web_root_path.begin(), web_root_path.end(), path.begin()))
                throw std::invalid_argument("path must be within root path");

            // 如果请求的path 为/ 则补全为/index.html
            // if (boost::filesystem::is_directory(path))
            //     path /= "index.html";
            auto ifs = std::make_shared<ifstream>();
            ifs->open(path.string(), ifstream::in | ios::binary | ios::ate);
            ifs->seekg(0, std::ios::beg);
            if (*ifs) {
                std::stringstream content_stream;
                content_stream << ifs->rdbuf();
                content_stream.seekp(0, ios::end);
                response << "HTTP/1.1 200 OK\r\nContent-Length: " << content_stream.tellp() << "\r\n\r\n" << content_stream.rdbuf();
            }

            else
                throw std::invalid_argument("could not read file");
            ifs->close();
        }
        catch (const std::exception& e) {
            std::stringstream content_stream;
            std::cerr << ": " << request.path << std::endl;
            content_stream << "Could not open path " << e.what() << std::endl;
            content_stream.seekp(0, ios::end);

            response << "HTTP/1.1 404 NOT FOUND\r\nContent-Length: " << content_stream.tellp() << "\r\n\r\n" << content_stream.rdbuf();
        }
    };
}

void HTTPServer::start() {
    accept();

    // 根据 num_threads 创建多个 run 线程
    for(size_t c = 1;c < num_threads_; c++) {
        threads_.emplace_back([this](){
            io_.run();
        });
    }

    io_.run();

    // 启动其他线程
    for(std::thread& t: threads_) {
        t.join(); // -> io_.run();
    }
}

void HTTPServer::accept() {
    
    // 用智能指针管理socket 对象
    shared_ptr<ip::tcp::socket> socket(new ip::tcp::socket(io_));

    acceptor_.async_accept(*socket, [this, socket](const boost::system::error_code& ec) {
        
        //区别于先处理，再新建一个socket， 这里是先新建，后处理
        accept();

        if(!ec) {
            process_request_and_respond(socket);
        }
    });
}

void HTTPServer::process_request_and_respond(shared_ptr<ip::tcp::socket> socket) {
    // 建立http连接以后 开始发送数据
    // 用 shared_ptr 来管理 read_buffer 对象
    shared_ptr<boost::asio::streambuf> read_buffer(new boost::asio::streambuf);

    async_read_until(*socket, *read_buffer, "\r\n\r\n",               // bytes_transferred 是到指定定界符并包括指定定界符的字节数。
    [this, socket, read_buffer](const boost::system::error_code& ec, size_t bytes_transferred) {
        if(!ec) {
            //read_buffer->size() 可能和 bytes_transferred 不同
            //执行async_read_until之后，streambuf 可能包含delimiter之外的其他数据”
            //选择的解决方案是在解析标头时直接从stream中按行解析。 
            //read_buffer剩余的内容 在函数async_read中处理（用于检索内容）

            size_t total = read_buffer->size();

            istream stream(read_buffer.get());

            shared_ptr<Request> request(new Request());
            *request=parse_request(stream);

            size_t num_additional_bytes = total - bytes_transferred;

            // 请求除了头之外, 还有Content 
            if(request->header.count("Content-Length")>0) {
                // transfer_exactly 显示的指定要 read 的字节数
                async_read(*socket, *read_buffer, transfer_exactly(stoull(request->header["Content-Length"]) - num_additional_bytes), 
                [this, socket, read_buffer, request](const boost::system::error_code& ec, size_t bytes_transferred) {
                    if(!ec) {
                        // Store pointer to read_buffer as istream object
                        // shared_ptr<istream> content;
                        request->content = shared_ptr<istream>(new istream(read_buffer.get()));

                        respond(socket, request);
                    }
                });
            }
            else {                   
                respond(socket, request);
            }
        }
    });
}

Request HTTPServer::parse_request(istream& stream) {
    Request request;
    //HTTP第一行为：
    //GET /index.html HTTP/1.1   匹配时空格为分界点，sm[1] sm[2] sm[2]
    std::regex e("^([^ ]*) ([^ ]*) HTTP/([^ ]*)$");

    smatch sm;
    string line;
    getline(stream, line);
    line.pop_back();
    if(regex_match(line, sm, e)) {        
        request.method=sm[1];
        request.path=sm[2];
        request.http_version=sm[3];

        bool matched;

        e = "^([^:]*): ?(.*)$";
        //对后面的每个键值对做分析，加到request.header字典中
        do {
            getline(stream, line);
            line.pop_back();
            matched=regex_match(line, sm, e);
            if(matched) {
                request.header[sm[1]]=sm[2];
            }

        } while(matched==true);
    }

    return request;
}

void HTTPServer::respond(shared_ptr<ip::tcp::socket> socket, shared_ptr<Request> request) {

    // std::cout << " request->path : "<<request->path << endl;
    for(auto& res: resources_) {
        std::regex e(res.first);
        auto& methods = res.second;
        smatch sm_res;
        if(regex_match(request->path, sm_res, e)) {
            if(methods.count(request->method)>0) {
                shared_ptr<boost::asio::streambuf> write_buffer(new boost::asio::streambuf);
                ostream response(write_buffer.get());

                // 调用以后，response中已经填充好了要返回的信息
                methods[request->method](response, *request, sm_res);

                //在lambda中捕获write_buffer，确保在async_write完成之前不会销毁它
                async_write(*socket, *write_buffer, [this, socket, request, write_buffer](const boost::system::error_code& ec, size_t bytes_transferred) {
                    //如果时HTTP1.1或者以上的版本，使用持久连接，不立即销毁socket
                    if(!ec && stof(request->http_version) > 1.05)
                        // 使用 async_read_until 继续等待其他数据请求 
                        process_request_and_respond(socket);
                });
                break;
            }
        }
    }
    return;
}