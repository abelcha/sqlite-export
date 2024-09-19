function sqlite2_std -a db_file -a dest_file
    echo .quit |sqlite3 "$db_file"  -json -cmd "select * from c2" > "$dest_file"
end

function duckdb_std -a db_file -a dest_file
     duckdb -json cargo.sqlite3 -c "FROM c2" > $dest_file
end

function sqlite_utils -a db_file -a dest_file
     sqlite-utils query cargo.sqlite3 "SELECT * FROM c2" > "$dest_file"
end

function sqlite2dir -a db_file -a dest_dir
   sqlite2dir "$db_file" "$dest_dir"
end

function sqlite_export -a db_file -a dest_dir
    sqlite_export "$db_file" "$dest_dir"
end

set -l sqlite2_std     '"echo .quit |sqlite3 "$db_file"  -json -cmd "select * from c2" > "$dest_file"'
set -l duckdb_std      'duckdb -json cargo.sqlite3 -c "FROM c2" > $dest_file'
set -l sqlite_utils      'sqlite-utils query cargo.sqlite3 "SELECT * FROM c2" > "$dest_file"'
set -l sqlite2dir    'sqlite2dir "$db_file" "$dest_dir"'
set -l sqlite_export     'sqlite_export "$db_file" "$dest_dir"'

# hyperdryve
# echo $h_sqlite2_std
hyperfine commads -L compiler sqlite2_std duckdb_stdsqlite_utils,sqlite2dir,sqlite_export '{compiler} cargo.sqlite3 cargo.json'