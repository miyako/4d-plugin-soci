# 4d-plugin-soci
SOCI for 4D

```
status:=SOCI(backend;connection;statements;binding;transaction)
```

|parameter|type|description|
|-|-|-|
|backend|Integer|see [SOCI backends](#soci-backends)|
|connection|Text|see [SOCI documentation](https://github.com/SOCI/soci/tree/master/docs/backends)|
|statements|Collection&lt;Text&gt;|lines of SQL|
|bindings|Collection&lt;Object&gt;|KVP for [input binding](#input-binding)|
|transaction|Integer|see [SOCI modes](#soci-modes)|
|status|Object||

### SOCI backends

* `0`: `SOCI_ODBC`
* `1`: `SOCI_MYSQL`
* `2`: `SOCI_POSTGRESQL`
* `3`: `SOCI_SQLITE3`

### SOCI modes

* `0`: `SOCI_NOT_IN_TRANSACTION`
* `1`: `SOCI_IN_TRANSACTION`

### input binding

|4D|SOCI|
|-|-|
|Is BLOB|`soci::blob`|
|Is boolean|`signed int`|
|Is date|`std::string`|
|Is longint|`signed int`|
|Is null|`soci::i_null`|
|Is real|`double`|
|Is text|`std::string`|
|Is time|`signed int`|

### output binding

|SOCI|4D|
|-|-|
|`soci::i_null`|Is null|
|`soci::dt_blob`|Is BLOB|
|`soci::dt_string`|Is text|
|`soci::dt_xml`|Is text|
|`soci::dt_date`|Is date|
|`soci::dt_double`|Is real|
|`soci::dt_integer`|Is longint|
|`soci::dt_long_long`|Is text|
|`soci::dt_unsigned_long_long`|Is text|

> [!NOTE]
> boolean is returned as number (`1` or `0`)
> date is converted to `YYYY-MM-DD`

> [!WARNING]
> if a `DATETIME` contains a value that can't be parsed as ISO 8601, you may get the error "Cannot parse date/time field component"
> 

## SQLite3 example

```4d
$connection:=File(File("/RESOURCES/test.db").platformPath; fk platform path).path
$INSERT:="INSERT INTO users(name,email) VALUES(:name,:email);"
$SELECT:="SELECT name,email FROM users WHERE name = :name;"
$SQL:=[$INSERT; $SELECT]
$params:=[\
{name: "keisuke miyako"; email: "keisuke.miyako@4d.com"}; \
{name: "keisuke miyako"}]

$status:=SOCI(SOCI_SQLITE3; $connection; $SQL; $params; SOCI_IN_TRANSACTION)
```

```4d
$connection:=File(File("/RESOURCES/test.db").platformPath; fk platform path).path

$DROP:="DROP TABLE IF EXISTS sample;"

$CREATE:="CREATE TABLE sample ("+\
"id INTEGER PRIMARY KEY,"+\
"name TEXT, flag BOOLEAN, score REAL, data BLOB, created_at DATETIME);"

$INSERT:="INSERT INTO sample (id, name, flag, score, data, created_at) "+\
"VALUES(:id, :name, :flag, :score, :data, :created_at);"

$SELECT:="SELECT * FROM sample;"

var $data : Blob
TEXT TO BLOB("hello!"; $data; UTF8 text without length)

$params:=[Null; Null; {id: 1; \
name: "keisuke miyako"; \
flag: True; \
score: 123456789.1234; \
data: $data; \
created_at: Current date}; Null]

$SQL:=[$DROP; $CREATE; $INSERT; $SELECT]

$status:=SOCI(SOCI_SQLITE3; $connection; $SQL; $params; SOCI_NOT_IN_TRANSACTION)
```

## ODBC example

```4d
$connection:="DSN=4Dv20"
$INSERT:="INSERT INTO users(name,email) VALUES(:name,:email);"
$SELECT:="SELECT name,email FROM users WHERE name = :name;"
$SQL:=[$INSERT; $SELECT]
$params:=[\
{name: "keisuke miyako"; email: "keisuke.miyako@4d.com"}; \
{name: "keisuke miyako"}]

$status:=SOCI(SOCI_ODBC; $connection; $SQL; $params; SOCI_IN_TRANSACTION)
```

## PostgreSQL example

```4d
$connection:="host=localhost port=5432 dbname=mydb user=myuser password=mypassword"
$INSERT:="INSERT INTO users(name,email) VALUES(:name,:email);"
$SELECT:="SELECT name,email FROM users WHERE name = :name;"
$SQL:=[$INSERT; $SELECT]
$params:=[\
{name: "keisuke miyako"; email: "keisuke.miyako@4d.com"}; \
{name: "keisuke miyako"}]

$status:=SOCI(SOCI_POSTGRESQL; $connection; $SQL; $params; SOCI_IN_TRANSACTION)
```

## MySQL example

```4d
$connection:="db=mydb user=myuser password=mypassword host=localhost"
$INSERT:="INSERT INTO users(name,email) VALUES(:name,:email);"
$SELECT:="SELECT name,email FROM users WHERE name = :name;"
$SQL:=[$INSERT; $SELECT]
$params:=[\
{name: "keisuke miyako"; email: "keisuke.miyako@4d.com"}; \
{name: "keisuke miyako"}]

$status:=SOCI(SOCI_MYSQL; $connection; $SQL; $params; SOCI_IN_TRANSACTION)
```

## SQL used in examples

```sql
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    name VARCHAR(50) NOT NULL,
    email VARCHAR(100) UNIQUE NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
);
```
