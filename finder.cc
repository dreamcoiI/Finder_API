#include <iostream>
#include <experimental/filesystem>
#include <thread>
#include <atomic>
#include <mutex>
#include <vector>
#include <chrono>
#include <string>
#include <cpprest/http_listener.h>
#include <cpprest/json.h>
//#pragma comment(lib, "cpprest_2_10")

using namespace web;
using namespace http;
using namespace http::experimental::listener;
namespace fs = std::experimental::filesystem;
std::atomic<int> count(0);
std::vector<std::string> fileNames;
std::mutex m;

void searchfile(const fs::path& directory, const std::string& extension) {
    std::vector<std::string> localNames;
    for (const auto& entry : fs::recursive_directory_iterator(directory))
    {
        if (fs::is_regular_file(entry) && entry.path().extension() == extension && fs::exists(entry)) {
            localNames.push_back(entry.path().string());
            count.fetch_add(1, std::memory_order_relaxed);
        }
    }
    std::cout<<"you're response" << std::endl;
    std::lock_guard<std::mutex> lock(m);
    fileNames.insert(fileNames.end(),localNames.begin(),localNames.end());
}

void handlePostRequest(const http_request& req) {
    req.extract_json().then([req](pplx::task<json::value> task)
      {
        try {
            json::value requestBody = task.get();
            if( requestBody.has_field("directory") && requestBody.has_field("extension")) {
               std::string directory = requestBody["directory"].as_string();
               std::string extension = requestBody["extension"].as_string();
               std::string directory2 = requestBody["directory_2"].as_string();
               std::string ext2 = requestBody["extension_2"].as_string();

               fs::path dir(directory);
               fs::path dir2(directory2);

               auto start = std::chrono::high_resolution_clock::now();
               std::thread t1(searchfile, dir, extension);
               std::thread t2(searchfile, dir2, ext2);
               t1.join();
               t2.join();
               auto end = std::chrono::high_resolution_clock::now();
               std::chrono::duration<double>time = end -start;
               web::json::value response;
               response["status"] = json::value::string("success");
                response["fileNames"] = web::json::value::array();
                int index =0;
                std::cout << "pls write you response." << std::endl;
                for (const auto& name : fileNames)
                {
                    response["fileNames"][index++] = web::json::value::string(name);
                }
                response["count"] =json::value::number(count);
                response["time"] = json::value::number(time.count());
                req.reply(status_codes::OK, response);

                fileNames.clear();
                count = 0;
            } else {
                json::value response;
                response["status"] = json::value::string("error");
                response["message"] = json::value::string("Invalid parameters");

                req.reply(status_codes::BadRequest,response);
            }
        } catch (const std::exception &e){
            json::value response;
            response["status"] = json::value::string("error");
            response["message"] = json::value::string("Internal server error.");

             req.reply(status_codes::InternalError,response);  
        }

      });
}

int main()
{
    web::http::experimental::listener::http_listener listener("http://localhost:5050");

    listener.support(methods::POST, handlePostRequest   );

    listener.open().then([]() {
        std::cout << "API server started." << std::endl;
    }).wait();

    std::cout << "Press Enter to exit." << std::endl;
    std::cin.get();

    listener.close().then([]() {
        std::cout << "API server stopped." << std::endl;
    }).wait();
}