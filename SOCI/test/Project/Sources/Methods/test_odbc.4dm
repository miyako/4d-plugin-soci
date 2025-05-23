//%attributes = {}
$connection:="DSN=4D"
$INSERT:="INSERT INTO users(name,email) VALUES(:name,:email);"
$SELECT:="SELECT name,email FROM users WHERE name = :name;"
$SQL:=[$INSERT; $SELECT]
$params:=[\
{name: "keisuke miyako"; email: "keisuke.miyako@4d.com"}; \
{name: "keisuke miyako"}]

$status:=SOCI(\
SOCI_ODBC; \
$connection; \
$SQL; \
$params; \
SOCI_NOT_IN_TRANSACTION; \
{odbc_option_driver_complete: "0"})