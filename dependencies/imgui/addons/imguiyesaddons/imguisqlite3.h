/* 
 CppSQLite was originally developed by Rob Groves on CodeProject:
 <http://www.codeproject.com/KB/database/CppSQLite.aspx>
 
 Maintenance and updates are Copyright (C) 2011 NeoSmart Technologies
 <http://neosmart.net/>
 
 Original copyright information:
 Copyright (C) 2004 Rob Groves. All Rights Reserved.
 <rob.groves@btinternet.com>
 
 Permission to use, copy, modify, and distribute this software and its
 documentation for any purpose, without fee, and without a written
 agreement, is hereby granted, provided that the above copyright notice, 
 this paragraph and the following two paragraphs appear in all copies, 
 modifications, and distributions.

 IN NO EVENT SHALL THE AUTHOR BE LIABLE TO ANY PARTY FOR DIRECT,
 INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
 PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
 EVEN IF THE AUTHOR HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

 THE AUTHOR SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
 LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION, IF
 ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". THE AUTHOR HAS NO OBLIGATION
 TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
*/
////////////////////////////////////////////////////////////////////////////////
// Modifications to the original CppSQLite3 library:
// -> added CppSQLite3 namespace
// -> removed prefix "CppSQLite3" to all classes
// -> removed #include <sqlite3.h> from the .h file
////////////////////////////////////////////////////////////////////////////////
// USAGE:
// -> needs -lsqlite3
// -> or the "plain C" amalgamated file "sqlite.c" compiled with a "plain C"
//    compiler linked to the executable. (Not included because it's too big)
////////////////////////////////////////////////////////////////////////////////
#ifndef CppSQLite3_H
#define CppSQLite3_H
#define IMGUISQLITE3_H_

/*#ifndef IMGUI_API
#include <imgui.h>
#endif //IMGUI_API*/


//#include <sqlite3.h>	// see if we can remove it
// Attempt to move "sqlite3.h" to imguisqlite3.cpp:----
struct sqlite3;
struct sqlite3_stmt;
#ifndef SQLITE_INT64_TYPE
#if defined(_MSC_VER) || defined(__BORLANDC__)
  #define SQLITE_INT64_TYPE __int64
#else
  #define SQLITE_INT64_TYPE long long int
#endif
#endif //SQLITE_INT64_TYPE
typedef SQLITE_INT64_TYPE sqlite_int64;
typedef unsigned SQLITE_INT64_TYPE sqlite_uint64;
typedef sqlite_int64 sqlite3_int64;
typedef sqlite_uint64 sqlite3_uint64;
// End Attempt ------------------------------------------


#define CPPSQLITE_ERROR 1000

namespace CppSQLite3 {

class Exception
{
public:

    Exception(const int nErrCode,
                    const char* szErrMess,
                    bool bDeleteMsg=true);

    Exception(const Exception&  e);

    virtual ~Exception();

    int errorCode() const { return mnErrCode; }

    const char* errorMessage() const { return mpszErrMess; }

    static const char* errorCodeAsString(int nErrCode);

private:

    int mnErrCode;
    char* mpszErrMess;
};


class Buffer
{
public:

    Buffer();

    ~Buffer();

    const char* format(const char* szFormat, ...);

    operator const char*() const { return mpBuf; }

    void clear();

private:

    char* mpBuf;
};


class Binary
{
public:

    Binary();

    ~Binary();

    void setBinary(const unsigned char* pBuf, int nLen);
    void setEncoded(const unsigned char* pBuf);

    const unsigned char* getEncoded();
    const unsigned char* getBinary();

    int getBinaryLength();

    unsigned char* allocBuffer(int nLen);

    void clear();

private:

    unsigned char* mpBuf;
    int mnBinaryLen;
    int mnBufferLen;
    int mnEncodedLen;
    bool mbEncoded;
};


class Query
{
public:

    Query();

    Query(const Query& rQuery);

    Query(sqlite3* pDB,
                sqlite3_stmt* pVM,
                bool bEof,
                bool bOwnVM=true);

    Query& operator=(const Query& rQuery);

    virtual ~Query();

    int numFields() const;

    int fieldIndex(const char* szField) const;
    const char* fieldName(int nCol) const;

    const char* fieldDeclType(int nCol) const;
    int fieldDataType(int nCol) const;

    const char* fieldValue(int nField) const;
    const char* fieldValue(const char* szField) const;

    int getIntField(int nField, int nNullValue=0) const;
    int getIntField(const char* szField, int nNullValue=0) const;

    long long getInt64Field(int nField, long long nNullValue=0) const;
    long long getInt64Field(const char* szField, long long nNullValue=0) const;

    double getFloatField(int nField, double fNullValue=0.0) const;
    double getFloatField(const char* szField, double fNullValue=0.0) const;

    const char* getStringField(int nField, const char* szNullValue="") const;
    const char* getStringField(const char* szField, const char* szNullValue="") const;

    const unsigned char* getBlobField(int nField, int& nLen) const;
    const unsigned char* getBlobField(const char* szField, int& nLen) const;

    bool fieldIsNull(int nField) const;
    bool fieldIsNull(const char* szField) const;

    bool eof() const;

    void nextRow();

    void finalize();

private:

    void checkVM() const;

    sqlite3* mpDB;
    sqlite3_stmt* mpVM;
    bool mbEof;
    int mnCols;
    bool mbOwnVM;
};


class Table
{
public:

    Table();

    Table(const Table& rTable);

    Table(char** paszResults, int nRows, int nCols);

    virtual ~Table();

    Table& operator=(const Table& rTable);

    int numFields() const;

    int numRows() const;

    const char* fieldName(int nCol) const;

    const char* fieldValue(int nField) const;
    const char* fieldValue(const char* szField) const;

    int getIntField(int nField, int nNullValue=0) const;
    int getIntField(const char* szField, int nNullValue=0) const;

    double getFloatField(int nField, double fNullValue=0.0) const;
    double getFloatField(const char* szField, double fNullValue=0.0) const;

    const char* getStringField(int nField, const char* szNullValue="") const;
    const char* getStringField(const char* szField, const char* szNullValue="") const;

    bool fieldIsNull(int nField) const;
    bool fieldIsNull(const char* szField) const;

    void setRow(int nRow);

    void finalize();

private:

    void checkResults() const;

    int mnCols;
    int mnRows;
    int mnCurrentRow;
    char** mpaszResults;
};


class Statement
{
public:

    Statement();

    Statement(const Statement& rStatement);

    Statement(sqlite3* pDB, sqlite3_stmt* pVM);

    virtual ~Statement();

    Statement& operator=(const Statement& rStatement);

    int execDML();

    Query execQuery();

    void bind(int nParam, const char* szValue);
    void bind(int nParam, const int nValue);
    void bind(int nParam, const long long nValue);
    void bind(int nParam, const double dwValue);
    void bind(int nParam, const unsigned char* blobValue, int nLen);
    void bindNull(int nParam);

    void reset();

    void finalize();

private:

    void checkDB() const;
    void checkVM() const;

    sqlite3* mpDB;
    sqlite3_stmt* mpVM;
};


class DB
{
public:

    DB();

    virtual ~DB();

    void open(const char* szFile);

    void close();

    bool tableExists(const char* szTable);

    int execDML(const char* szSQL);

    Query execQuery(const char* szSQL);

    int execScalar(const char* szSQL);

    Table getTable(const char* szSQL);

    Statement compileStatement(const char* szSQL);

    sqlite_int64 lastRowId() const;

    void interrupt();

    void setBusyTimeout(int nMillisecs);

    static const char* SQLiteVersion();

private:

    DB(const DB& db);
    DB& operator=(const DB& db);

    sqlite3_stmt* compile(const char* szSQL);

    void checkDB() const;

    sqlite3* mpDB;
    int mnBusyTimeoutMs;
};
} // namespace CppSQLite3

#endif //IMGUISQLITE3_H_

