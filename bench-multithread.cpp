#include <fstream>
#include <iostream>
#include <mutex>
#include <sqlite3.h>
#include <string>
#include <thread>
#include <vector>

std::mutex g_mutex;

static int callback(void *data, int argc, char **argv, char **azColName) {
  std::ofstream *file = static_cast<std::ofstream *>(data);
  std::lock_guard<std::mutex> lock(g_mutex);

  *file << "    {" << std::endl;
  for (int i = 0; i < argc; i++) {
    *file << "        \"" << azColName[i] << "\": \""
          << (argv[i] ? argv[i] : "NULL") << "\"";
    if (i < argc - 1) {
      *file << ",";
    }
    *file << std::endl;
  }
  *file << "    }," << std::endl;
  return 0;
}

void exportTable(const std::string &dbPath, const std::string &tableName) {
  sqlite3 *db;
  char *zErrMsg = 0;
  int rc = sqlite3_open(dbPath.c_str(), &db);
  if (rc) {
    std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
    return;
  }

  std::string sql = "SELECT * FROM `" + tableName + "`";
  std::ofstream file("nout/" + tableName + ".json");
  file << "[" << std::endl;

  rc = sqlite3_exec(db, sql.c_str(), callback, &file, &zErrMsg);
  if (rc != SQLITE_OK) {
    std::cerr << "SQL error: " << zErrMsg << std::endl;
    sqlite3_free(zErrMsg);
  } else {
    file.seekp(-3, std::ios_base::end);
    file << std::endl << "]" << std::endl;
  }
  file.close();
  sqlite3_close(db);
}

int main(int argc, char *argv[]) {
  if (argc <= 2) {
    std::cerr << "Usage: " << argv[0] << " <database_file>" << std::endl;
    return 1;
  }

  std::string dbPath = argv[1];
  sqlite3 *db;
  char *zErrMsg = 0;
  int rc = sqlite3_open(dbPath.c_str(), &db);
  if (rc) {
    std::cerr << "Can't open database: " << sqlite3_errmsg(db) << std::endl;
    return 1;
  }

  std::string sql = "SELECT name FROM sqlite_master WHERE type='table'";
  sqlite3_stmt *stmt;
  rc = sqlite3_prepare_v2(db, sql.c_str(), -1, &stmt, nullptr);
  if (rc != SQLITE_OK) {
    std::cerr << "Failed to fetch tables: " << sqlite3_errmsg(db) << std::endl;
    sqlite3_close(db);
    return 1;
  }
  std::printf("Found %d tables\n", sqlite3_column_count(stmt));

  std::vector<std::thread> threads;
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    std::string tableName =
        reinterpret_cast<const char *>(sqlite3_column_text(stmt, 0));
    threads.emplace_back(exportTable, dbPath, tableName);
  }

  for (auto &thread : threads) {
    thread.join();
  }

  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return 0;
}
