#include "sylar/http/http_server.h"
#include "sylar/address.h"

void run() {
    sylar::Address::ptr addr = sylar::Address::LookupAnyIPAddress("0.0.0.0:8020");
    if (!addr) {
        perror("unable to resolve addr");
        return;
    }
    sylar::http::HttpServer::ptr http_server(new sylar::http::HttpServer(true));

    while (!http_server->bind(addr)) {
        perror("failed to bind");
        sleep(1);
    }

    auto sd = http_server->getServletDispatch();
    sd->addServlet("/procmon/xx", [](sylar::http::HttpRequest::ptr req
                                    , sylar::http::HttpResponse::ptr rsp
                                    , sylar::http::HttpSession::ptr session){
        rsp->setBody(req->toString());
        return 0;
    });

    http_server->start();
}

int main(int argc, char const *argv[]) {
    sylar::IOManager iom;
    iom.schedule(run);
    return 0;
}