#include "httpserver.hpp"

HTTPServer::HTTPServer(boost::asio::io_context& io, unsigned short port = 80, size_t num_threads = 1) 
    : io_(io), endpoint_(ip::tcp::v4(), port),
    acceptor_(io_, endpoint_), num_threads_(num_threads) {

    // ��������ķ�ʽ

    //ֱ�ӷ���Host,���� 127.0.0.1:8080 ,û�о����·��,�������������.
    this->resources_["^/$"]["GET"] = [](ostream& response, const Request& request, const smatch& path_match) {
        std::stringstream content_stream;
        content_stream << "<h1>Request:</h1>";
        content_stream << request.method << " " << request.path << " HTTP/" << request.http_version << "<br>";
        for (auto& header : request.header) {
            content_stream << header.first << ": " << header.second << "<br>";
        }

        //��stringstreamָ���Ƶ�ĩβ, �����Ժ���tellp��ȡ���ĳ���.
        content_stream.seekp(0, std::ios::end);

        response << "HTTP/1.1 200 OK\r\nContent-Length: " << content_stream.tellp() << "\r\n\r\n" << content_stream.rdbuf();
    };

    // ���ʾ����ļ�, ���� http://127.0.0.1:8080/test.html
    this->resources_["^/.+$"]["GET"] = [](ostream& response, const Request& request, const smatch& path_match) {
        
        try {
            boost::filesystem::path web_root_path = boost::filesystem::canonical("web");
            boost::filesystem::path path = boost::filesystem::canonical(web_root_path / request.path);
            // ȷ��Ŀ¼���� web_root_path ��
            if (std::distance(web_root_path.begin(), web_root_path.end()) > std::distance(path.begin(), path.end()) ||
                !std::equal(web_root_path.begin(), web_root_path.end(), path.begin()))
                throw std::invalid_argument("path must be within root path");

            // ��������path Ϊ/ ��ȫΪ/index.html
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
            std::cerr << ": " << e.what() << std::endl;
            content_stream << "Could not open path " + request.path << std::endl;
            content_stream.seekp(0, ios::end);

            response << "HTTP/1.1 404 NOT FOUND\r\nContent-Length: " << content_stream.tellp() << "\r\n\r\n" << content_stream.rdbuf();
        }
    };
}

void HTTPServer::start() {
    accept();

    // ���� num_threads ������� run �߳�
    for(size_t c=1;c<num_threads_;c++) {
        threads_.emplace_back([this](){
            io_.run();
        });
    }

    io_.run();

    // ���������߳�
    for(std::thread& t: threads_) {
        t.join(); // -> io_.run();
    }
}

void HTTPServer::accept() {
    
    // ������ָ�����socket ����
    shared_ptr<ip::tcp::socket> socket(new ip::tcp::socket(io_));

    acceptor_.async_accept(*socket, [this, socket](const boost::system::error_code& ec) {
        
        //�������ȴ������½�һ��socket�� ���������½�������
        accept();

        if(!ec) {
            process_request_and_respond(socket);
        }
    });
}

void HTTPServer::process_request_and_respond(shared_ptr<ip::tcp::socket> socket) {
    // ����http�����Ժ� ��ʼ��������
    // �� shared_ptr ������ read_buffer ����
    shared_ptr<boost::asio::streambuf> read_buffer(new boost::asio::streambuf);

    async_read_until(*socket, *read_buffer, "\r\n\r\n",               // bytes_transferred �ǵ�ָ�������������ָ����������ֽ�����
    [this, socket, read_buffer](const boost::system::error_code& ec, size_t bytes_transferred) {
        if(!ec) {
            //read_buffer->size() ���ܺ� bytes_transferred ��ͬ
            //ִ��async_read_until֮��streambuf ���ܰ���delimiter֮����������ݡ�
            //ѡ��Ľ���������ڽ�����ͷʱֱ�Ӵ�stream�а��н����� 
            //read_bufferʣ������� �ں���async_read�д������ڼ������ݣ�

            size_t total = read_buffer->size();

            istream stream(read_buffer.get());

            shared_ptr<Request> request(new Request());
            *request=parse_request(stream);

            size_t num_additional_bytes = total - bytes_transferred;

            // �������ͷ֮��, ����Content 
            if(request->header.count("Content-Length")>0) {
                // transfer_exactly ��ʾ��ָ��Ҫ read ���ֽ���
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
    //HTTP��һ��Ϊ��
    //GET /index.html HTTP/1.1   ƥ��ʱ�ո�Ϊ�ֽ�㣬sm[1] sm[2] sm[2]
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
        //�Ժ����ÿ����ֵ�����������ӵ�request.header�ֵ���
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

                // �����Ժ�response���Ѿ�������Ҫ���ص���Ϣ
                methods[request->method](response, *request, sm_res);

                //��lambda�в���write_buffer��ȷ����async_write���֮ǰ����������
                async_write(*socket, *write_buffer, [this, socket, request, write_buffer](const boost::system::error_code& ec, size_t bytes_transferred) {
                    //���ʱHTTP1.1�������ϵİ汾��ʹ�ó־����ӣ�����������socket
                    if(!ec && stof(request->http_version) > 1.05)
                        // ʹ�� async_read_until �����ȴ������������� 
                        process_request_and_respond(socket);
                });
                break;
            }
        }
    }
    return;
}