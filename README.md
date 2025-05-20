# 4d-plugin-soci
SOCI for 4D

```
status:=SOCI(backend;connection;statements;binding;transaction)
```

|parameter|type|description|
|-|-|-|
|backend|Integer|see [SOCI backends](#soci-backends)|
|connection|Text|backend specific connection string|
|statements|Collection&lt;Text&gt;|lines of SQL|
|bindings|Collection&lt;Object&gt;|KVP|
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
|Is boolean|`bool`|
|Is date|`std::tm`|
|Is longint|`signed int`|
|Is null|`soci::i_null`|
|Is real|`double`|
|Is text|`std::string`|
|Is time|`signed int`|

### output binding

|SOCI|4D|
|-|-|
|`soci::i_null`|Is null|
|`soci::dt_string`|Is text|
|`soci::dt_xml`|Is text|
|`soci::dt_date`|Is date|
|`soci::dt_double`|Is real|
|`soci::dt_integer`|Is longint|
|`soci::dt_long_long`|Is text|
|`soci::dt_unsigned_long_long`|Is text|
|`soci::dt_blob`|Is BLOB|

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

references: https://github.com/SOCI/soci/tree/master/docs/backends
