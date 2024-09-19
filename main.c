#include <libgen.h>
#include <sqlite3.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define MAX_PATH 1024
#define MAX_COLUMNS 1024

// typedef enum { false, true } bool;

static void ajson_pstr(const char *string, FILE *pfs) {
  while (*string) {
    fputc(*string, pfs);

    string++;
  }
}

/**
 * Quote and write string using JSON output function
 * @param string string to be written
 * @param pfs JSON output function
 */

static void ajson_pstrq(const char *string, FILE *pfs) {
  char buf[64];

  if (!string) {
    ajson_pstr("null", pfs);
    return;
  }
  fputc('"', pfs);
  while (*string) {
    switch (*string) {
    case '"':
    case '\\':
      fputc('\\', pfs);
      fputc(*string, pfs);
      break;
    case '\b':
      fputc('\\', pfs);
      fputc('b', pfs);
      break;
    case '\f':
      fputc('\\', pfs);
      fputc('f', pfs);
      break;
    case '\n':
      fputc('\\', pfs);
      fputc('n', pfs);
      break;
    case '\r':
      fputc('\\', pfs);
      fputc('r', pfs);
      break;
    case '\t':
      fputc('\\', pfs);
      fputc('t', pfs);
      break;
    default:
      if (((*string < ' ') && (*string > 0)) || (*string == 0x7f)) {
        sprintf(buf, "\\u%04x", *string);
        ajson_pstr(buf, pfs);
      } else if (*string < 0) {
        unsigned char c = string[0];
        unsigned long uc = 0;

        if (c < 0xc0) {
          uc = c;
        } else if (c < 0xe0) {
          if ((string[1] & 0xc0) == 0x80) {
            uc = ((c & 0x1f) << 6) | (string[1] & 0x3f);
            ++string;
          } else {
            uc = c;
          }
        } else if (c < 0xf0) {
          if (((string[1] & 0xc0) == 0x80) && ((string[2] & 0xc0) == 0x80)) {
            uc = ((c & 0x0f) << 12) | ((string[1] & 0x3f) << 6) |
                 (string[2] & 0x3f);
            string += 2;
          } else {
            uc = c;
          }
        } else if (c < 0xf8) {
          if (((string[1] & 0xc0) == 0x80) && ((string[2] & 0xc0) == 0x80) &&
              ((string[3] & 0xc0) == 0x80)) {
            uc = ((c & 0x03) << 18) | ((string[1] & 0x3f) << 12) |
                 ((string[2] & 0x3f) << 6) | (string[4] & 0x3f);
            string += 3;
          } else {
            uc = c;
          }
        } else if (c < 0xfc) {
          if (((string[1] & 0xc0) == 0x80) && ((string[2] & 0xc0) == 0x80) &&
              ((string[3] & 0xc0) == 0x80) && ((string[4] & 0xc0) == 0x80)) {
            uc = ((c & 0x01) << 24) | ((string[1] & 0x3f) << 18) |
                 ((string[2] & 0x3f) << 12) | ((string[4] & 0x3f) << 6) |
                 (string[5] & 0x3f);
            string += 4;
          } else {
            uc = c;
          }
        } else {
          /* ignore */
          ++string;
        }
        if (uc < 0x10000) {
          sprintf(buf, "\\u%04lx", uc);
        } else if (uc < 0x100000) {
          uc -= 0x10000;

          sprintf(buf, "\\u%04lx", 0xd800 | ((uc >> 10) & 0x3ff));
          ajson_pstr(buf, pfs);
          sprintf(buf, "\\u%04lx", 0xdc00 | (uc & 0x3ff));
        } else {
          strcpy(buf, "\\ufffd");
        }
        ajson_pstr(buf, pfs);
      } else {
        fputc(*string, pfs);
      }
      break;
    }
    ++string;
  }
  fputc('"', pfs);
}

/**
 * Conditionally quote and write string using JSON output function
 * @param string string to be written
 * @param pfs JSON output function
 */

static void ajson_pstrc(const char *string, FILE *pfs) {
  if (*string && strchr(".0123456789-+", *string)) {
    ajson_pstr(string, pfs);
  } else {
    ajson_pstrq(string, pfs);
  }
}

/**
 * Write a blob as base64 string using JSON output function
 * @param blk pointer to blob
 * @param len length of blob
 * @param pfs JSON output function
 */

static void ajson_pb64(const unsigned char *blk, int len, FILE *pfs) {
  // impexp_putc fputc = pfs->fputc;
  // void *parg = pfs->parg;
  int i, reg[5];
  char buf[16];
  static const char *b64 =
      "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";

  if (!blk) {
    ajson_pstr("null", pfs);
    return;
  }
  buf[4] = '\0';
  fputc('"', pfs);
  for (i = 0; i < len; i += 3) {
    reg[1] = reg[2] = reg[3] = reg[4] = 0;
    reg[0] = blk[i];
    if (i + 1 < len) {
      reg[1] = blk[i + 1];
      reg[3] = 1;
    }
    if (i + 2 < len) {
      reg[2] = blk[i + 2];
      reg[4] = 1;
    }
    buf[0] = b64[reg[0] >> 2];
    buf[1] = b64[((reg[0] << 4) & 0x30) | (reg[1] >> 4)];
    if (reg[3]) {
      buf[2] = b64[((reg[1] << 2) & 0x3c) | (reg[2] >> 6)];
    } else {
      buf[2] = '=';
    }
    if (reg[4]) {
      buf[3] = b64[reg[2] & 0x3f];
    } else {
      buf[3] = '=';
    }
    ajson_pstr(buf, pfs);
  }
  fputc('"', pfs);
}

// t_format extensions[] = {"json", "csv", "tsv"};

// typedef struct {
//   t_format format;
//   char end_wrap[2];
//   char *start_wrap[2];
// } t_formats;

// t_formats formats[] = {
//     {json, "[", "]", '\n', ':'},
//     // {csv, ',', ',', '\n', ','},
//     // {tsv, 0, '	', '\n', '\t'},
// };
// {
//   {json, "[", "]", '\n', ':'},
//    {csv, ',', ',', '\n', ','},
//    {tsv, 0, '	', '\n', '\t'},
// };

typedef enum { json, csv, jsonl, tsv, ndjson } t_format;

static const char *extensions[] = {"json", "csv", "jsonl", "tsv", "ndjson"};
typedef int (*t_array_map)(sqlite3 *db, const char *table_name, FILE *pfs,
                           t_format format);

void print_value(sqlite3_stmt *stmt, int i, FILE *pfs) {
  switch (sqlite3_column_type(stmt, i)) {
  case SQLITE_INTEGER:
    ajson_pstr((char *)sqlite3_column_text(stmt, i), pfs);
    break;
  case SQLITE_FLOAT:
    ajson_pstrc((char *)sqlite3_column_text(stmt, i), pfs);
    break;
  case SQLITE_BLOB:
    ajson_pb64((unsigned char *)sqlite3_column_blob(stmt, i),
               sqlite3_column_bytes(stmt, i), pfs);
    break;
  case SQLITE_TEXT:
    ajson_pstrq((char *)sqlite3_column_text(stmt, i), pfs);
    break;

  case SQLITE_NULL:
  default:
    ajson_pstr("null", pfs);
    break;
  }
}

static char separator_map[5] = {',', ',', '\n', '\t', '\n'};

int dump_tabular(sqlite3 *db, const char *table_name, FILE *pfs,
                 t_format format) {
  char SEPARATOR = ',';
  char query[MAX_PATH];
  sqlite3_stmt *stmt;

  snprintf(query, MAX_PATH, "SELECT * FROM `%s`", table_name);

  if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Error preparing statement for table %s: %s\n", table_name,
            sqlite3_errmsg(db));
    return 0;
  }
  // typedef enum { json, csv, jsonl, tsv, ndjson } t_format;
  int num_cols = sqlite3_column_count(stmt);
  int row_count = 0;
  // print headers:
  for (int i = 0; i < num_cols; i++) {
    i &&fputc(separator_map[format], pfs); // cell_sep
    i &&fputc(' ', pfs);
    fprintf(pfs, "%s", sqlite3_column_name(stmt, i));
  }
  fputc('\n', pfs);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    for (int i = 0; i < num_cols; i++) {
      if (i != 0) {
        fputc(separator_map[format], pfs); // cell_sep
        fputc(' ', pfs);
      }
      print_value(stmt, i, pfs);
    }
    fputc('\n', pfs); // row_sep
    if (row_count % 499 == 0) {
      fprintf(stderr, "\r%d", row_count);
    }
  }
  return row_count;
}

int dump_json(sqlite3 *db, const char *table_name, FILE *pfs, t_format format) {
  // fprintf(stderr, "JSON FORMAT\n");
  // FILE *pfs = malloc(sizeof(pfs));
  // pfs->fputc = fputc;
  // bool JSONLINES = false;

  char query[MAX_PATH];
  sqlite3_stmt *stmt;

  snprintf(query, MAX_PATH, "SELECT * FROM `%s`", table_name);

  if (sqlite3_prepare_v2(db, query, -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Error preparing statement for table %s: %s\n", table_name,
            sqlite3_errmsg(db));
    return 0;
  }

  int num_cols = sqlite3_column_count(stmt);
  int row_count = 0;
  // print headers:
  char headerKeys[num_cols][256];
  // printf("Setting Headervalues:");
  for (int i = 0; i < num_cols; i++) {
    // headerKeys[i] = malloc(sizeof(char) * 256);
    snprintf(headerKeys[i], 255, "\"%s\":", sqlite3_column_name(stmt, i));
    // sprintf(headerKeys, "\"%s\":", sqlite3_column_name(stmt, i));
  }
  // print headers:
  if (format != ndjson) {
    fputc('[', pfs);
    fputc('\n', pfs);
  }
  int rate = 99;
  // fputc((i == 0) ? '[' : ',', parg);
  while (sqlite3_step(stmt) == SQLITE_ROW) {
    if (row_count++ && format != ndjson) {
      fputc(',', pfs);
    }
    fputc('{', pfs); // row_start
    for (int i = 0; i < num_cols; i++) {
      if (i != 0) {
        fputc(',', pfs); // cell_sep
      }
      fprintf(pfs, "\"%s\":", sqlite3_column_name(stmt, i)); // key_sep
      print_value(stmt, i, pfs);
    }
    fputc('}', pfs);  // row_end
    fputc('\n', pfs); // row_sep
    if (row_count % 9 == 0) {
      fprintf(stderr, "\r%d", row_count);
      if ((row_count * 1000) > rate) {
        rate = rate * 10;
      }
    }
    if (format != ndjson) {
      fputc(']', pfs); // end
    }
  }
  return row_count;
}

t_array_map array_map[] = {dump_json, dump_tabular, dump_json, dump_tabular,
                           dump_json};
int main(int argc, char *argv[]) {
  sqlite3 *db;
  sqlite3_stmt *stmt;
  t_format format = json;
  char separator = ',';
  char *source_path = NULL;
  char *output_dir = NULL;

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--json") == 0) {
      format = json;
    } else if (strcmp(argv[i], "--ndjson") == 0) {
      format = json;
    } else if (strcmp(argv[i], "--tsv") == 0) {
      format = tsv;
    } else if (strcmp(argv[i], "--csv") == 0) {
      format = csv;
    } else if (source_path == NULL && argv[i][0] != '-') {
      source_path = argv[i];
    } else if (output_dir == NULL && argv[i][0] != '-') {
      output_dir = argv[i];
    } else {
      printf("Unknown argument: %s\n", argv[i]);
      printf("Usage: %s db.sqlite3 export-dir [--json|--jsonl|--tsv|--csv]\n",
             argv[0]);
      exit(1);
    }
  }
  if (!output_dir) {
    output_dir = malloc(1024);
    memset(output_dir, 0, 1024);
    snprintf(output_dir, 1024, "ex_%s", basename(source_path));
    // strcat(strdup("export_"), basename(source_path));
  }
  fprintf(stderr, "source: %s\n", source_path);
  fprintf(stderr, "target: %s\n", output_dir);

  if (sqlite3_open(argv[1], &db) != SQLITE_OK) {
    printf("Failed to open DB: %s\n", sqlite3_errmsg(db));
    return 1;
  }

  if (sqlite3_prepare_v2(db,
                         "WITH tables as (SELECT name FROM sqlite_master WHERE "
                         "type='table' AND name LIKE '%%') "
                         "SELECT name,  (select count(*) from tables as cnt )"
                         "FROM tables GROUP BY name",
                         -1, &stmt, NULL) != SQLITE_OK) {
    fprintf(stderr, "Error preparing statement: %s\n", sqlite3_errmsg(db));
    sqlite3_close(db);
    return 1;
  }

  mkdir(output_dir, 0755);
  int i = 0;

  while (sqlite3_step(stmt) == SQLITE_ROW) {
    const char *table_name = (const char *)sqlite3_column_text(stmt, 0);
    int table_count = sqlite3_column_int(stmt, 1);

    FILE *output = stdout;
    char path[1024 * 4] = {0};
    if (output_dir != NULL) {
      snprintf(path, sizeof(path), "%s/%s.%s", output_dir, table_name,
               extensions[format]);
      output = fopen(path, "w");
    }
    fprintf(stderr, "\n\t\t%s\t\t%d/%d ...", path, ++i, table_count);
    int total_rows = array_map[format](db, table_name, output, format);
    if (total_rows) {
      // fprintf(stderr, "\r%d", total_rows);
    }
    // fprintf(stderr, "\t\t%s\t\t ...", path);
  }
  fprintf(stderr, "\n");
  sqlite3_finalize(stmt);
  sqlite3_close(db);

  return 0;
}