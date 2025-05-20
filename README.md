# 4d-plugin-soci
SOCI for 4D

```
status:=SOCI(backend;connection;statements;binding;transaction)
```

|parameter|type|description|
|-|-|-|
|backend|Integer|[SOCI Backends](#soci-backends)|
|connection|Text|backend specific connection string|
|statements|Collection&lt;Text&gt;|lines of SQL|
|binding|Object|KVP|
|transaction|Integer|`0` (default) or `1` use transaction|
|status|Object||

### SOCI Backends

* `0`: `SOCI_ODBC`
* `1`: `SOCI_MYSQL`
* `2`: `SOCI_POSTGRESQL`
* `3`: `SOCI_SQLITE3`
