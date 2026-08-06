// Démo web — serveur : voir docs/superpowers/specs/2026-08-06-web-audio-demo-design.md.
#include "httplib.h"

#include <cstdio>

int main() {
    httplib::Server svr;
    svr.set_default_headers({{"Access-Control-Allow-Origin", "*"}});

    svr.Get("/api/ping", [](const httplib::Request&, httplib::Response& res) {
        res.set_content(R"({"ok":true})", "application/json");
    });

    std::printf("web_demo_server : ecoute sur http://127.0.0.1:8787\n");
    std::fflush(stdout);
    svr.listen("127.0.0.1", 8787);
    return 0;
}
