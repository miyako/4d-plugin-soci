# 4d-plugin-soci
SOCI for 4D

```
status:=SOCI(backend;connection;statements;binding;transaction)
```

|parameter|type|description|
|-|-|-|
|backend|Integer|see below|
|connection|Text|backend specific connection string|
|statements|Collection&lt;Text&gt;|lines of SQL|
|binding|Object|KVP|
|transaction|Integer|`0` (default) or `1` use transaction|
|status|Object||
