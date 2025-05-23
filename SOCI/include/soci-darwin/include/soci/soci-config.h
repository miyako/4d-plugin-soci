//
// Copyright (C) 2004-2008 Maciej Sobczak, Stephen Hutton
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//

#ifndef SOCICONFIG_H_INCLUDED
#define SOCICONFIG_H_INCLUDED

//
// SOCI has been build with support for:
//

// Boost library
#define SOCI_HAVE_BOOST

// Boost date_time library
#define SOCI_HAVE_BOOST_DATE_TIME

// Enables C++11 support
#define SOCI_HAVE_CXX11

// DB2 backend
/* #undef SOCI_HAVE_DB2 */

// FIREBIRD backend
/* #undef SOCI_HAVE_FIREBIRD */

// MYSQL backend
#define SOCI_HAVE_MYSQL

// ODBC backend
#define SOCI_HAVE_ODBC

// ORACLE backend
/* #undef SOCI_HAVE_ORACLE */

// POSTGRESQL backend
#define SOCI_HAVE_POSTGRESQL

// SQLITE3 backend
#define SOCI_HAVE_SQLITE3

// Test SOCI_HAVE_VISIBILITY_SUPPORT
#define SOCI_HAVE_VISIBILITY_SUPPORT


#endif // SOCICONFIG_H_INCLUDED
