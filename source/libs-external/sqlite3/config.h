#pragma once

int isotoxin_compare( const unsigned char *zA, const unsigned char *zB );

static void likeFunc( sqlite3_context *context, int argc, sqlite3_value **argv )
{
    const unsigned char *zA, *zB;

    zB = sqlite3_value_text(argv[0]);
    zA = sqlite3_value_text(argv[1]);

    sqlite3_result_int(context, isotoxin_compare(zA, zB));
}