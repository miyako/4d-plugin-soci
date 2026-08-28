![version](https://img.shields.io/badge/version-20%2B-E23089)
![platform](https://img.shields.io/static/v1?label=platform&message=mac-intel%20|%20mac-arm%20|%20win-64&color=blue)
[![license](https://img.shields.io/github/license/miyako/4d-plugin-soci)](LICENSE)
![downloads](https://img.shields.io/github/downloads/miyako/4d-plugin-soci/total)

# 4d-plugin-soci

This guide documents the **SOCI** plugin from the point of view of a 4D developer calling it from 4D code. It does not assume any knowledge of the plugin's C++ implementation.

---

## 1. Overview

SOCI is a single 4D command, `SOCI`, that lets you connect to a database and run one or more SQL statements against it in one call, using named (`:paramName`) bind parameters for input and returning results as native 4D collections/objects.

It wraps the [SOCI C++ database library](https://github.com/SOCI/soci) and currently supports four backends:

| Backend | Constant | Notes |
|---|---|---|
| ODBC | `SOCI_ODBC` | Any database reachable through an ODBC DSN/driver |
| MySQL | `SOCI_MYSQL` | Direct MySQL client protocol |
| PostgreSQL | `SOCI_POSTGRESQL` | Direct PostgreSQL client protocol |
| SQLite3 | `SOCI_SQLITE3` | Local file-based database, no server required |

**Platforms:** macOS (Intel & Apple Silicon), Windows 64-bit. **4D version:** 20 and later. **Licensing:** the plugin's own source is MIT-licensed; SOCI itself is Boost Software License 1.0. The licensing of the compiled binary is subject to the license terms of whichever backend client libraries it's built against (e.g. the MySQL client library) — check those before redistributing a build.

---

## 2. Installing the plugin

Copy the plugin bundle into your project's `Plugins` folder (or the machine-wide 4D `Plugins` folder) and restart 4D. Once loaded, `SOCI` and its associated constants appear under the **SOCI** theme in the 4D language reference and autocomplete.

---

## 3. The `SOCI` command

### Syntax

```4d
status := SOCI(backend; connection; statements; bindings; transaction{; options})
```

### Parameters

| # | Name | Type | Required | Description |
|---|---|---|---|---|
| 1 | `backend` | Integer | Yes | Which database driver to use. One of the `SOCI_*` backend constants (§5.1). |
| 2 | `connection` | Text | Yes | The connection string. Format is backend-specific — see §9. |
| 3 | `statements` | Collection\<Text\> | Yes | One or more SQL statements to execute, in order. Each element must be text; non-text elements are silently skipped (see §7). |
| 4 | `bindings` | Collection\<Object\> | Yes | Parallel to `statements`. Element *i* is the object of `:name` → value pairs bound into statement *i*. Pass `Null` for a statement that needs no parameters (e.g. `DROP TABLE`, `CREATE TABLE`). If this collection is shorter than `statements`, the remaining statements simply run unbound. |
| 5 | `transaction` | Integer | Yes | Whether to wrap the whole batch in a transaction. One of the `SOCI_*` mode constants (§5.2). |
| 6 | `options` | Object | No | Backend-specific options. Currently only meaningful for `SOCI_ODBC` — see §5.3. |

### Return value — the `status` object

| Property | Type | Always present? | Description |
|---|---|---|---|
| `success` | Boolean | Yes | `True` if every statement executed and (if `SOCI_IN_TRANSACTION`) the transaction committed. `False` on any error. |
| `results` | Collection | Yes | Parallel to `statements`. Element *i* is itself a **Collection of Objects** — one object per row returned by statement *i*, keyed by column name. For `INSERT`/`UPDATE`/`DELETE`/DDL statements (no rows returned) this is an empty collection, not `Null`. **On error, `results` still contains the statements that completed successfully before the failing one** — see §11. |
| `errorMessage` | Text | Only when `success` is `False` | The underlying SOCI/driver error message. |

```4d
// shape of a successful status object, e.g. after INSERT + SELECT:
{
  success: true,
  results: [
    [],                                     // INSERT — no rows
    [{name: "keisuke miyako"; email: "keisuke.miyako@4d.com"}]   // SELECT — 1 row
  ]
}
```

---

## 4. How statements, bindings, and results line up

- `results[i]` always corresponds to `statements[i]` — same index, same order.
- A statement that returns no rows (INSERT/UPDATE/DELETE/DDL) still gets an entry in `results`; it's just an empty collection.
- `bindings[i]` may be `Null` (or simply absent, if the `bindings` collection is shorter than `statements`) for a statement that has no `:name` placeholders.
- Every named placeholder in a statement (`:name`) **must** have a matching key in that statement's binding object, or the statement will fail — see §11.
- If an element of `statements` is not text (e.g. `Null`, a number), that entry is skipped entirely and does not produce a `results` element for that position — avoid mixing types in the `statements` collection.

---

## 5. Constants reference

### 5.1 SOCI backends

| Constant | Value |
|---|---|
| `SOCI_ODBC` | `0` |
| `SOCI_MYSQL` | `1` |
| `SOCI_POSTGRESQL` | `2` |
| `SOCI_SQLITE3` | `3` |

### 5.2 SOCI modes (transaction parameter)

| Constant | Value | Behavior |
|---|---|---|
| `SOCI_NOT_IN_TRANSACTION` | `0` | Each statement is executed and committed by the driver's default (usually auto-commit) behavior. |
| `SOCI_IN_TRANSACTION` | `1` | All statements run inside a single transaction. If every statement succeeds, the transaction is committed. If **any** statement throws, the transaction is automatically rolled back and nothing in the batch is persisted. |

### 5.3 ODBC options (6th parameter, ODBC backend only)

| Key | Type | Description |
|---|---|---|
| `odbc_option_driver_complete` | Text | Passed through to the ODBC driver manager's `SQLDriverConnect` completion mode. **Must be passed as text**, e.g. `"0"`, not the number `0`. |

```4d
$status := SOCI(SOCI_ODBC; $connection; $SQL; $params; SOCI_NOT_IN_TRANSACTION; {odbc_option_driver_complete: "0"})
```

---

## 6. Data type binding

### 6.1 Input binding — 4D value → SQL parameter

This is how a value in your `bindings` object gets sent to the database, based on its **4D type**:

| 4D type | Sent to SOCI as |
|---|---|
| Boolean | signed int (`1`/`0`) |
| Longint | signed int |
| Real | `double` |
| Text | `std::string` |
| Date | `std::string`, formatted `YYYY-MM-DD` |
| Time | signed int |
| Null | `soci::i_null` (SQL `NULL`) |
| BLOB (`4D.Blob`) | `soci::blob` |

> **Note:** dates are converted to the ISO `YYYY-MM-DD` string form *before* being sent — this is an input-side conversion, independent of how dates come back on output (§6.2).

### 6.2 Output binding — SQL column → 4D value

This is how a returned column's **SQL data type** maps to the property set on each row object:

| SQL/SOCI type | 4D value |
|---|---|
| `NULL` | `Null` |
| string / xml | Text |
| date | **Date** (native 4D date, not text) |
| double | Real |
| integer | Longint |
| long long | Text |
| unsigned long long | Text |
| blob | BLOB |

> **Booleans** come back as `0`/`1` via the integer path — there is no native SQL boolean in this mapping, so treat the returned Longint as a boolean yourself if needed.
>
> **64-bit integers are returned as Text**, not Longint. A 4D `Longint` is 32-bit, so values that don't fit are deliberately returned as text to avoid silent truncation/precision loss. Convert with `Num()` or into a `Long64`/`8-byte` variable as needed in your own code.
>
> **Date parsing:** if a `DATETIME`/`TIMESTAMP` column contains a value the driver can't parse as ISO 8601, you'll get an error like *"Cannot parse date/time field component"* instead of a row. This is a driver-level limitation, not something the plugin's binding rules control — see §13.

---

## 7. Transactions

```4d
$status := SOCI(SOCI_SQLITE3; $connection; $SQL; $params; SOCI_IN_TRANSACTION)
```

With `SOCI_IN_TRANSACTION`, the whole `statements` batch is one atomic unit: either all statements commit together, or (if any statement fails) none of them are persisted and the transaction is rolled back automatically — you don't need to issue your own `ROLLBACK` statement or handle cleanup. Use `SOCI_NOT_IN_TRANSACTION` for read-only batches or when you deliberately want each statement to commit independently.

---

## 8. Backend-specific connection examples

### SQLite3

```4d
$connection := File(File("/RESOURCES/test.db").platformPath; fk platform path).path
$INSERT := "INSERT INTO users(name,email) VALUES(:name,:email);"
$SELECT := "SELECT name,email FROM users WHERE name = :name;"
$SQL := [$INSERT; $SELECT]
$params := [\
{name: "keisuke miyako"; email: "keisuke.miyako@4d.com"}; \
{name: "keisuke miyako"}]

$status := SOCI(SOCI_SQLITE3; $connection; $SQL; $params; SOCI_IN_TRANSACTION)
```

### ODBC

```4d
$connection := "DSN=4Dv20;UID=myuser;PWD=mypassword;"
$INSERT := "INSERT INTO users(name,email) VALUES(:name,:email);"
$SELECT := "SELECT name,email FROM users WHERE name = :name;"
$SQL := [$INSERT; $SELECT]
$params := [\
{name: "keisuke miyako"; email: "keisuke.miyako@4d.com"}; \
{name: "keisuke miyako"}]

$status := SOCI(SOCI_ODBC; $connection; $SQL; $params; SOCI_IN_TRANSACTION; {odbc_option_driver_complete: "0"})
```

### PostgreSQL

```4d
$connection := "host=localhost port=5432 dbname=mydb user=myuser password=mypassword"
$INSERT := "INSERT INTO users(name,email) VALUES(:name,:email);"
$SELECT := "SELECT name,email FROM users WHERE name = :name;"
$SQL := [$INSERT; $SELECT]
$params := [\
{name: "keisuke miyako"; email: "keisuke.miyako@4d.com"}; \
{name: "keisuke miyako"}]

$status := SOCI(SOCI_POSTGRESQL; $connection; $SQL; $params; SOCI_IN_TRANSACTION)
```

### MySQL

```4d
$connection := "db=mydb user=myuser password=mypassword host=localhost"
$INSERT := "INSERT INTO users(name,email) VALUES(:name,:email);"
$SELECT := "SELECT name,email FROM users WHERE name = :name;"
$SQL := [$INSERT; $SELECT]
$params := [\
{name: "keisuke miyako"; email: "keisuke.miyako@4d.com"}; \
{name: "keisuke miyako"}]

$status := SOCI(SOCI_MYSQL; $connection; $SQL; $params; SOCI_IN_TRANSACTION)
```

---

## 9. Full example: mixed types + BLOB

This example creates a table, inserts one row exercising every supported input type (including a BLOB), and reads it back:

```4d
$connection := File(File("/RESOURCES/test.db").platformPath; fk platform path).path

$DROP := "DROP TABLE IF EXISTS sample;"

$CREATE := "CREATE TABLE sample ("+\
"id INTEGER PRIMARY KEY,"+\
"name TEXT, flag BOOLEAN, score REAL, data BLOB, created_at DATETIME);"

$INSERT := "INSERT INTO sample (id, name, flag, score, data, created_at) "+\
"VALUES(:id, :name, :flag, :score, :data, :created_at);"

$SELECT := "SELECT * FROM sample;"

var $data : Blob
TEXT TO BLOB("hello!"; $data; UTF8 text without length)

$params := [Null; Null; {id: 1; \
name: "keisuke miyako"; \
flag: True; \
score: 123456789.1234; \
data: $data; \
created_at: Current date}; Null]

$SQL := [$DROP; $CREATE; $INSERT; $SELECT]

$status := SOCI(SOCI_SQLITE3; $connection; $SQL; $params; SOCI_NOT_IN_TRANSACTION)

If ($status.success)
    $row := $status.results[3][0]  // 4th statement (SELECT), 1st row
    // $row.flag is 1 (Longint), $row.created_at is a real Date, $row.data is a Blob
Else
    ALERT($status.errorMessage)
End if
```

---

## 10. Working with BLOBs

- Bind a value of 4D type **BLOB** (a variable declared `var $x : Blob`, or a `4D.Blob` object) exactly like any other binding — put it in the parameter object under its `:name` key.
- A **zero-byte BLOB** is a valid, supported input — it's bound as an empty `soci::blob` rather than being rejected or causing undefined behavior (this was hardened in the current build).
- On output, BLOB columns come back as native 4D BLOB values, not text — no conversion needed on your side.
- Binding a non-BLOB object (an object that isn't a `4D.Blob`) into a slot where the plugin expects a blob is treated as a zero-length blob rather than throwing — if a BLOB parameter silently comes back empty, double-check the value you passed is actually a `4D.Blob`.

---

## 11. Error handling

Every `SOCI` call returns a status object — it never raises a 4D error/exception directly. Always check `.success` before touching `.results`.

```4d
$status := SOCI(SOCI_POSTGRESQL; $connection; $SQL; $params; SOCI_IN_TRANSACTION)

If (Not($status.success))
    ALERT("Database error: "+$status.errorMessage)
Else
    // ... use $status.results
End if
```

Common causes of `success = False`:

| Situation | What you'll see |
|---|---|
| Bad connection string / server unreachable | Driver-specific connection error in `errorMessage` |
| SQL syntax error | Backend's parser error text |
| A `:name` placeholder with no matching key in that statement's binding object | Binding/prepare error — check every placeholder has a corresponding key |
| Constraint violation (unique/foreign key/etc.) | Backend's constraint error text |
| `DATETIME`/`TIMESTAMP` value that isn't clean ISO 8601 | *"Cannot parse date/time field component"* — see §13 |
| Invalid `backend` constant | Generic "session not opened" style error |

**On failure partway through a batch**, `$status.results` still contains the results of every statement that completed *before* the one that failed — useful for diagnosing exactly which statement in a multi-line batch caused the problem. If you used `SOCI_IN_TRANSACTION`, none of those earlier statements were actually committed even though they appear in `results` — the whole batch was rolled back.

---

## 12. Best practices

- **Always use named bind parameters (`:name`) for values — never concatenate user input directly into the SQL text.** This plugin's binding mechanism exists specifically to avoid SQL injection; use it for anything that comes from outside your code (form input, imported data, etc.).
- **Use `SOCI_IN_TRANSACTION`** for any batch where partial application would leave your data in an inconsistent state (e.g. a multi-table insert).
- **Keep batches focused.** A single `SOCI` call blocks the calling 4D process until every statement finishes. For long-running or large-result queries, call `SOCI` from a preemptive process/worker rather than the main process to avoid freezing the interface.
- **Check `results[i]` shape, not just `success`.** An empty collection at a given index is normal for non-`SELECT` statements — don't treat it as an error.
- **For SQLite3**, remember it's a single file — concurrent writers from multiple processes/threads can see "database is locked" style errors under contention; serialize heavy write workloads if you're hitting this.

---

## 13. Troubleshooting / FAQ

**"Cannot parse date/time field component"**
The database returned a `DATETIME`/`TIMESTAMP` value in a format the driver can't parse as ISO 8601. Check how the column was populated — values written by tools other than this plugin (or by SQL literals not in ISO form) are the usual cause.

**A `:name` placeholder isn't getting a value**
Every named placeholder in a statement must have a matching key in that statement's `bindings[i]` object. If `bindings` is shorter than `statements`, the missing entries run fully unbound — fine for DDL, but any `:name` in those statements will fail to bind.

**`odbc_option_driver_complete` doesn't seem to take effect**
It must be passed as **Text**, not a number — `{odbc_option_driver_complete: "0"}`, not `{odbc_option_driver_complete: 0}`.

**A 64-bit integer column looks like text in my results**
Expected — see §6.2. 4D's `Longint` is 32-bit, so `long long`/`unsigned long long` columns are returned as Text to avoid silent precision loss. Convert on your side if you need it numeric.

**A BLOB parameter came back empty / wasn't inserted**
Confirm the value you bound is a genuine `4D.Blob` (e.g. from `TEXT TO BLOB`, `BLOB`, or a `var ... : Blob`) — see §10.

---

## 14. Quick reference

```4d
status := SOCI(backend; connection; statements; bindings; transaction{; options})

// backend:      SOCI_ODBC (0) | SOCI_MYSQL (1) | SOCI_POSTGRESQL (2) | SOCI_SQLITE3 (3)
// transaction:  SOCI_NOT_IN_TRANSACTION (0) | SOCI_IN_TRANSACTION (1)
// options:      {odbc_option_driver_complete: "<text>"}   -- ODBC only

// status.success      : Boolean
// status.results      : Collection, parallel to `statements`; each element is a
//                        Collection of row-Objects (empty for non-SELECT statements)
// status.errorMessage : Text, only when status.success = False
```
