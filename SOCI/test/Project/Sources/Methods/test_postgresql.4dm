//%attributes = {}
$connection:="host=localhost port=5432 dbname=mydb user=myuser password=mypass"
$INSERT:="INSERT INTO users(name,email) VALUES(:name,:email);"
$SELECT:="SELECT name,email FROM users WHERE name = :name;"
$SQL:=[$INSERT; $SELECT]
$params:=[\
{name: "keisuke miyako"; email: "keisuke.miyako@4d.com"}; \
{name: "keisuke miyako"}]

$status:=SOCI(SOCI_POSTGRESQL; $connection; $SQL; $params; SOCI_IN_TRANSACTION)