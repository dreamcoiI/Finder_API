#include "finder.h"

int main() {
  web::http::experimental::listener::http_listener listener(
      "http://localhost:5050");

  listener.support(finder::methods::POST, finder::handlePostRequest);

  listener.open()
      .then([]() { std::cout << "API server started." << std::endl; })
      .wait();
  std::cout << "Please specify no more than "
            << std::thread::hardware_concurrency() << " directories"
            << std::endl;
  std::cout << "Press Enter to exit." << std::endl;
  std::cin.get();

  listener.close()
      .then([]() { std::cout << "API server stopped." << std::endl; })
      .wait();
}

void finder::searchfile(const fs::path& directory, const std::string& extension) {
  std::vector<std::string> localNames;
  for (const auto& entry : fs::recursive_directory_iterator(directory)) {
    if (fs::is_regular_file(entry) && entry.path().extension() == extension &&
        fs::exists(entry)) {
      localNames.push_back(entry.path().string());
      count.fetch_add(1, std::memory_order_relaxed);
    }
  }
  std::cout << "you're response" << std::endl;
  fileNames.insert(fileNames.end(), localNames.begin(), localNames.end());
  threadsCount.fetch_add(1, std::memory_order_relaxed);
}

void finder::handlePostRequest(const http_request& req) {
  req.extract_json().then([req](const pplx::task<json::value>& task) {
    try {
      json::value requestBody = task.get();
      if (requestBody.has_field("directories") &&
          requestBody.has_field("extensions")) {
        json::array directories = requestBody["directories"].as_array();
        json::array extensions = requestBody["extensions"].as_array();

        if (directories.size() != extensions.size()) {
          json::value response;
          response["status"] = json::value::string("Error");
          response["message"] = json::value::string(
              "Mismatch between directories and extensions");

          req.reply(status_codes::BadRequest, response);
          return;
        }

        if (directories.size() > std::thread::hardware_concurrency()) {
          json::value response;
          response["status"] = json::value::string("Error");
          response["message"] =
              json::value::string("You have specified too many directories");

          req.reply(status_codes::BadRequest, response);
          return;
        }

        auto start = std::chrono::high_resolution_clock::now();

        std::vector<std::thread> threads;
        for (size_t i = 0; i < directories.size(); ++i) {
          fs::path dir(directories[i].as_string());
          if(fs::exists(dir) && fs::is_directory(dir))
            threads.emplace_back(searchfile, dir, extensions[i].as_string());
          else {
              web::json::value response;
              response["message"] = json::value::string("Directory "+dir.string()+" not found");
              std::cout<<"Directory " << dir.string()<<" not found" <<std::endl;
              continue;
          }
        }

        for (auto& thread : threads) thread.join();

        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> time = end - start;
        web::json::value response;
        response["status"] = json::value::string("success");
        response["fileNames"] = web::json::value::array();
        int index = 0;
        std::cout << "pls write you response." << std::endl;

        for (const auto& name : fileNames)
          response["fileNames"][index++] = web::json::value::string(name);

        response["count"] = json::value::number(count);
        response["time"] = json::value::number(time.count());
        response["count threads"] = json::value::number(threadsCount);
        req.reply(status_codes::OK, response);

        fileNames.clear();
        count = 0;

      } else {
        json::value response;
        response["status"] = json::value::string("error");
        response["message"] = json::value::string("Invalid parameters");

        req.reply(status_codes::BadRequest, response);
      }
    } catch (const std::exception& e) {
      json::value response;
      response["status"] = json::value::string("error");
      response["message"] = json::value::string("Internal server error.");

      req.reply(status_codes::InternalError, response);
    }
  });
}

