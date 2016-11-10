#pragma once

/* must be include in c/cpp file */

static bool htl_initialized = false;

typedef struct
{
    char name[128];
    uint32_t id;

} chnl_s;

static chnl_s *htl_chnls = NULL;
static int htl_chnls_count = 0;
static char htl_fn[512] = {0};
static int htl_part = 0;
static HANDLE htl_mutex = NULL;

static void htl_gen_name( char *outb, int part )
{
    if (part == 0)
    {
        strcpy(outb, htl_fn );
        return;
    }

    char *dot = strstr( htl_fn, ".html" );
    int l = dot - htl_fn;
    strncpy( outb, htl_fn, l );
    outb[l] = 0;
    sprintf_s( outb + l, 512-l, "%i%s", part, dot );
}

static FILE *htl_check( FILE *fp )
{
    fseek( fp, 0, SEEK_END );

    size_t sz = ftell( fp );

    if (sz > 250000)
    {
        char n[512], ss[1024];
        htl_gen_name(n, htl_part  + 1);

        FILE *nfp;
        for (;;)
        {
            nfp = fopen( n, "ab" );
            if (nfp) break;
        }

        if (ftell( nfp ) == 0)
        {
            int l = sprintf_s( ss, 1024, "</table><br><br><a href='%s'>next part</a>", n );
            fwrite( ss, l, 1, fp );

            htl_gen_name( n, htl_part );
            l = sprintf_s( ss, 1024, "<a href='%s'>prev part</a><br>", n );
            fwrite( ss, l, 1, nfp );
            sz = 0;
        }

        ++htl_part;
        fclose( fp );
        fp = nfp;

    }


    if (0 == sz)
    {
        char *h = "<style> table, th, td { border: 1px solid; border-color: #dddddd; border-collapse: collapse; font-family: 'Arial'; font-size: 10px; } p { padding:0; margin:0 } p:before {content: '- '} </style> <table style='width:100 % '>";
        int l = strlen( h );
        fwrite( h, l, 1, fp );

        char w[4000];
        strcpy( w, "<tr>" );
        int n = 4;

        for (int i = 0; i < htl_chnls_count; ++i)
        {
            n += sprintf_s( w + n, sizeof( w ) - n, "<th>%s</th>", htl_chnls[i].name );
        }
        strcpy( w + n, "</tr>\r\n" );
        n += 7;

        fwrite( w, n, 1, fp );
    }
    return fp;
}

static void htl_sys( FILE *fp, const char *f, ... )
{
    char w[4000];

    va_list args;
    va_start( args, f );
    vsprintf_s( w, 2000, f, args );
    va_end( args );

    int n = sprintf_s( w + 2000, 2000, "<tr style='background-color: #aaffaa'><td colspan='%i'>%s</td></tr>", htl_chnls_count, w );

    fwrite( w + 2000, n, 1, fp );

}
static void htl_log_file_name( const char * fn )
{
    strcpy( htl_fn, fn );

    FILE *fp = fopen( htl_fn, "ab" );
    if (!fp)
        return;

    fp = htl_check( fp );
    htl_sys( fp, "started pid %u, tid %u", GetCurrentProcessId(), GetCurrentThreadId() );

    fclose(fp);
}

static void htl_channel( uint32_t id, const char *name )
{
    for(int i=0;i<htl_chnls_count;++i)
    {
        if (htl_chnls[i].id == id)
        {
            strcpy( htl_chnls[i].name, name );
            return;
        }
    }

    htl_chnls = (chnl_s *)realloc( htl_chnls, (htl_chnls_count + 1) * sizeof( chnl_s ) );
    htl_chnls[htl_chnls_count].id = id;
    strcpy( htl_chnls[htl_chnls_count].name, name );

    ++htl_chnls_count;
}

static const char *htl_chname(uint32_t id)
{
    for (int i = 0; i < htl_chnls_count; ++i)
    {
        if (htl_chnls[i].id == id)
        {
            return htl_chnls[i].name;
        }
    }

    static char b[128];
    sprintf_s( b, sizeof( b ), "[unknown %u]", id );
    return b;
}

static void htl_shutdown()
{
    free( htl_chnls );
    htl_chnls = NULL;
    htl_chnls_count = 0;
    htl_part = 0;
    htl_fn[0] = 0;
    CloseHandle( htl_mutex );
    htl_mutex = NULL;
}

static bool htl_tick()
{
    bool t = !htl_initialized;

    if (t) {
        htl_mutex = CreateMutexA( NULL, FALSE, "vknsdfnvoeirvo" );
    }

    htl_initialized = true;
    return t;
}


static void htl_log( uint32_t id, const char *f, ... )
{
    if (!htl_fn[0])
        return;

    char str[2000], w[4000];

    va_list args;
    va_start( args, f );
    vsprintf_s( str, 2000, f, args );
    va_end( args );

    int n = 4;
    if (id)
    {

        //sprintf_s( w, sizeof(w), "<tr>" );
        strcpy( w, "<tr>" );
        bool ok = false;
        for (int i = 0; i < htl_chnls_count; ++i)
        {
            if (htl_chnls[i].id == id)
            {
                n += sprintf_s( w + n, sizeof( w ) - n, "<td>%s</td>", str );
                ok = true;
            } else
            {
                strcpy( w + n, "<td></td>" );
                n += 9;
            }
        }
        strcpy( w + n, "</tr>\r\n" );
        n += 7;

        if (!ok) {
            sprintf_s( w, sizeof( w ), "unknown channel (%u): %s", id, str);
            id = 0;
            strcpy(str,w);
        }
    }

    WaitForSingleObject( htl_mutex, INFINITE );

    for(;;)
    {
        char name[512];
        htl_gen_name( name, htl_part );

        FILE *fp = fopen( name, "ab" );
        if (!fp)
        {
            continue;
        }
        fp = htl_check( fp );

        if (!id)
            htl_sys( fp, "%s", str );
        else
            fwrite( w, n, 1, fp );
        fclose( fp );
        break;
    }

    ReleaseMutex( htl_mutex );

}
