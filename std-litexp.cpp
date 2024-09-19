#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <ostream>
#include <sqlite3.h>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace fs = std::filesystem;
using json = nlohmann::json;

class SQLiteToJSON {
public:
  SQLiteToJSON(const std::string &db_path) : db_path_(db_path) {}

  bool convert(const std::string &table_name, const fs::path file_path) {

    sqlite3 *db;
    if (sqlite3_open(db_path_.c_str(), &db) != SQLITE_OK) {
      std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
      return false;
    }

    std::string query = "SELECT * FROM `" + table_name + "`";
    sqlite3_stmt *stmt;
    if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) !=
        SQLITE_OK) {
      std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db)
                << std::endl;
      sqlite3_close(db);
      return false;
    }

    std::vector<std::string> colnames(sqlite3_column_count(stmt));
    for (int i = 0; i < sqlite3_column_count(stmt); i++) {
      const char *column_name =
          reinterpret_cast<const char *>(sqlite3_column_name(stmt, i));
      colnames[i] = column_name;
    }
    int row_count = 0;
    json result;
    std::ofstream file(file_path);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
      json row;
      for (int i = 0; i < sqlite3_column_count(stmt); i++) {
        const char *value =
            reinterpret_cast<const char *>(sqlite3_column_text(stmt, i));
        // column_name = colnames[i];
        row[colnames[i]] = value ? std::string(value) : "";
      }
      result.push_back(std::move(row));
      // if (result.size() % 1000 == 0) {
      //   printf("Processed %ld rows\n", result.size());
      // }
      if (row_count++ % 9999 == 0) {
        fprintf(stderr, "\r%d", (int)result.size());
        
        // result.begin();
      }
      if (row_count % 9999 == 0) {
        fprintf(stderr, "\r%d", (int)result.size());
      }
    }

    file << result.dump(2);
    // std::cout << "Done converting table: " << table_name << std::endl;
    // printf("Result: %s\n", result.dump(4).c_str());

    // std::string filename = table_name + ".json";
    // std::ofstream file(filename);
    // file << result.dump(4); // Pretty print with 4-space indent

    // printf("Writing to file: %s\n", file_path.c_str());
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    return true;
  }

private:
  std::string db_path_;
};

int main(int argc, char *argv[]) {

  if (argc <= 2) {
    std::cerr << "Usage: " << argv[0] << " <database_file>" << std::endl;
    return 1;
  }

  std::string db_path = argv[1];
  SQLiteToJSON converter(db_path);

  sqlite3 *db;
  if (sqlite3_open(db_path.c_str(), &db) != SQLITE_OK) {
    std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
    return 1;
  }

  sqlite3_stmt *stmt;
  std::string query = "SELECT name FROM sqlite_master WHERE type='table';";
  if (sqlite3_prepare_v2(db, query.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
    std::cerr << "Failed to prepare statement: " << sqlite3_errmsg(db)
              << std::endl;
    sqlite3_close(db);
    return 1;
  }
  fs::create_directory(argv[2]);
  int i = 0;

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const char *table_name =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    fs::path file_path =
        fs::path(argv[2]) / (std::string(table_name) + ".json");

    fprintf(stderr, "\n    \t\t%s\t\t %d/%d ...", file_path.c_str(), ++i, 42);

    // std::cout << "Converting table: " << table_name << std::endl;
    converter.convert(table_name, file_path);
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return 0;
}
