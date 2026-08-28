//%attributes = {}
$connection:=File:C1566(File:C1566("/RESOURCES/test.db").platformPath; fk platform path:K87:2).path

$DROP:="DROP TABLE IF EXISTS sample;"

$CREATE:="CREATE TABLE sample ("+\
"id INTEGER PRIMARY KEY,"+\
"name TEXT, flag BOOLEAN, score REAL, data BLOB, created_at DATETIME);"

$INSERT:="INSERT INTO sample (id, name, flag, score, data, created_at) "+\
"VALUES(:id, :name, :flag, :score, :data, :created_at);"

$SELECT:="SELECT * FROM sample;"

var $data : Blob
TEXT TO BLOB:C554("hello!"; $data; UTF8 text without length:K22:17)

$params:=[Null:C1517; Null:C1517; {id: 1; \
name: "keisuke miyako"; \
flag: True:C214; \
score: 123456789.1234; \
data: $data; \
created_at: Current date:C33}; Null:C1517]

$SQL:=[$DROP; $CREATE; $INSERT; $SELECT]

$status:=SOCI(SOCI_SQLITE3; $connection; $SQL; $params; SOCI_NOT_IN_TRANSACTION)