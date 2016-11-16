#pragma once

namespace ts
{

enum class data_type_e
{
    t_int, t_int64, t_float, t_str, t_blob, t_null
};

INLINE bool same_sqlite_type( data_type_e t1, data_type_e t2 )
{
    if (t1 == t2) return true;
    if (t1 == data_type_e::t_int && t2 == data_type_e::t_int64) return true;
    if (t1 == data_type_e::t_int64 && t2 == data_type_e::t_int) return true;
    return false;
}

struct column_desc_s
{
    MOVABLE( true );
    asptr name_;
    asptr default_;
    data_type_e type_ = data_type_e::t_int;
    flags32_s options;

    static const flags32_s::BITS f_primary = SETBIT(0);
    static const flags32_s::BITS f_autoincrement = SETBIT(1);
    static const flags32_s::BITS f_nullable = SETBIT(2);
    static const flags32_s::BITS f_unique_index = SETBIT(3);
    static const flags32_s::BITS f_non_unique_index = SETBIT(4);

    bool has_default() const
    {
        if (default_.s) return true;
        return type_ != data_type_e::t_blob && !options.is(f_nullable) && !options.is(f_autoincrement) && !options.is(f_primary);
    }

    str_c get_default() const
    {
        if (default_.s) return str_c(default_);
        if (type_ == data_type_e::t_int || type_ == data_type_e::t_int64 || type_ == data_type_e::t_float) return str_c(CONSTASTR("0")); 
        return str_c(CONSTASTR("\'\'"));
    }
};

struct data_value_s
{
    MOVABLE( true );
    str_c text;
    blob_c blob;
    union 
    {
        int64 i;
        double f;
    };
};

struct data_pair_s : public data_value_s
{
    DUMMY(data_pair_s);
    data_pair_s() {}
    str_c name;
    data_type_e type_ = data_type_e::t_int;

};

typedef fastdelegate::FastDelegate<data_type_e (int, data_value_s &)> SQLITE_DATAGETTER;
typedef fastdelegate::FastDelegate<bool (int, SQLITE_DATAGETTER)> SQLITE_TABLEREADER;
typedef fastdelegate::FastDelegate<void(int, int)> SQLITE_ENCRYPT_PROCESS_CALLBACK;
typedef fastdelegate::FastDelegate<void( const data_value_s &v )> SQLITE_UNIQUE_VALUES;

class sqlitedb_c
{
    DECLARE_EYELET( sqlitedb_c );
protected:
    sqlitedb_c() {}
    ~sqlitedb_c() {}
public:

    virtual void begin_transaction() = 0;
    virtual void end_transaction() = 0;

    virtual bool is_correct() const = 0; // should be called just after connect to check that encrypted db was opened with correct key

    virtual void close() = 0; // no need to call due no resource leak - all databases will be destroyed automatically at end of program
    virtual bool is_table_exist( const asptr& tablename ) = 0;
    virtual void create_table( const asptr& tablename, array_wrapper_c<const column_desc_s> columns, bool norowid ) = 0;
    virtual int  update_table_struct( const asptr& tablename, array_wrapper_c<const column_desc_s> columns, bool norowid ) = 0; // create or add/remove/update columns
    virtual bool unique_values( const asptr& tablename, SQLITE_UNIQUE_VALUES reader, const asptr& column ) = 0;
    virtual bool read_table( const asptr& tablename, SQLITE_TABLEREADER reader, const asptr& where_items = asptr() ) = 0;
    virtual int  insert( const asptr& tablename, array_wrapper_c<const data_pair_s> fields ) = 0;
    virtual void delrow( const asptr& tablename, int id ) = 0;
    virtual void delrows(const asptr& tablename, const asptr& where_items) = 0;
    virtual int  count( const asptr& tablename, const asptr& where_items ) = 0;
    virtual void update( const asptr& tablename, array_wrapper_c<const data_pair_s> fields, const asptr& where_items ) = 0;
    virtual int  find_free( const asptr& tablename, const asptr& id ) = 0;
    
    virtual void rekey( const uint8 *k, SQLITE_ENCRYPT_PROCESS_CALLBACK cb ) = 0; // k must be 48 bytes length (16 salt + 32 password hash, salt will be saved into file as header)

    static sqlitedb_c *connect( const wsptr &fn, const uint8 *k /* 48 bytes; see rekey; can be null - mean unencrypted */, bool readonly ); // will create file if not exist / returns already connected db
};

class db_transaction_c
{
    iweak_ptr<sqlitedb_c> db;
public:
    db_transaction_c( sqlitedb_c *db ):db(db)
    {
        if (db) db->begin_transaction();
    }
    ~db_transaction_c()
    {
        end();
    }

    void end()
    {
        if (db)
        {
            db->end_transaction();
            db.unconnect();
        }
    }
};

}