#ifndef FINDER_API_FINDER_H
#define FINDER_API_FINDER_H

#include <atomic>
#include <experimental/filesystem>
#include <iostream>
#include <thread>
#include <cpprest/http_listener.h>
#include <cpprest/json.h>
#include <chrono>
#include <string>
#include <vector>

namespace finder {
    using namespace web;
    using namespace http;
    using namespace http::experimental::listener;
    namespace fs = std::experimental::filesystem;
    void searchfile(const fs::path &directory, const std::string &extension);
    void handlePostRequest(const http_request &req);
    std::atomic<int> count(0);
    std::vector<std::string> fileNames;
    std::atomic<int> threadsCount(0);
};

#endif //FINDER_API_FINDER_H