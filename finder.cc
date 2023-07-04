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

void finder::searchfile(const fs::path& directory,
                        const std::string& extension) {
  std::vector<std::string> localNames;
  for (const auto& entry : fs::recursive_directory_iterator(directory)) {
    if (fs::is_regular_file(entry) && entry.path().extension() == extension &&
        fs::exists(entry)) {
      localNames.push_back(entry.path().string());
      lastModify.push_back(fs::last_write_time(entry));
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
        std::vector<std::string> wrong_dir;
        for (size_t i = 0; i < directories.size(); ++i) {
          fs::path dir(directories[i].as_string());
          if (fs::exists(dir) && fs::is_directory(dir))
            threads.emplace_back(searchfile, dir, extensions[i].as_string());
          else {
            wrong_dir.push_back(dir);
            std::cout << "Directory " << dir.string() << " not found"
                      << std::endl;
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

        for (const auto& name : fileNames) {
          response["fileNames"][index]["fileName"] =
              web::json::value::string(name);
          time_t timeUpdate =
              std::chrono::system_clock::to_time_t(lastModify[index]);
          std::tm* localTime = std::localtime(&timeUpdate);
          char buffer[80];
          std::strftime(buffer, sizeof(buffer), "%d-%m-%Y %H:%M:%S", localTime);
          std::uintmax_t fileSize = fs::file_size(name);
          std::string formattedSize = formatFileSize(fileSize);
          response["fileNames"][index]["lastModify"] =
              web::json::value::string(buffer);
          response["fileNames"][index]["fileSize"] =
              web::json::value::string(formattedSize);
          ++index;
        }
        if (!wrong_dir.empty()) {
          int ind = 0;
          for (const auto& name : wrong_dir)
            response["directory not found"][ind++] =
                web::json::value::string(name);
        }
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

std::string finder::formatFileSize(std::uintmax_t sizeInBytes) {
  static const std::string suffixes[] = {"B", "KB", "MB", "GB", "TB"};
  std::uintmax_t index = 0;
  auto size = static_cast<double>(sizeInBytes);

  while (size >= 1024 && index < sizeof(suffixes) / sizeof(suffixes[0]) - 1) {
    size /= 1024;
    ++index;
  }

  return std::to_string(size) + " " + suffixes[index];
}
