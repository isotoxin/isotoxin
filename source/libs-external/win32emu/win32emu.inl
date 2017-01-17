#ifdef WIN32EMU

// please, include this file into any cpp file of your project


unsigned int WaitForSingleObject( HANDLE evt, int timeout )
{
    handle_t t;
    t.hptr = evt;
    switch (t.ht)
    {
    case HT_EVENT: {

            if ( INFINITE != timeout )
            {
                pollfd fd;
                fd.fd = t.h;
                fd.events = POLLIN;
                fd.revents = 0;
                int r = poll(&fd, 1, timeout);
                if ( r == 0 )
                    return WAIT_TIMEOUT;
                if ( r == -1 )
                    return WAIT_FAILED;
            }

            int64 evv1 = 1;
            int r = read( t.h, &evv1, 8 );
            if (r != 8)
                return WAIT_FAILED;
            if ( t.manualreset )
                SetEvent( evt );
            return WAIT_OBJECT_0;
        }
        break;
    }
    return WAIT_FAILED;
}


w32e_consts CompareStringW( w32e_consts l, w32e_consts o, const char16_t *s1, int len1, const char16_t *s2, int len2 )
{
    if ( len1 != len2 )
        return CSTR_NOTEQUAL;

    wchar_t *buf = (wchar_t *)alloca( (len1*2) * sizeof(wchar_t) );
    for(int i=0;i<len1;++i)
    {
        buf[i] = s1[i];
        buf[i+len1] = s2[i];
    }

    return 0 == wcsncasecmp( buf, buf+len1, len1 ) ? CSTR_EQUAL : CSTR_NOTEQUAL;
}

w32e_consts CompareStringA( w32e_consts l, w32e_consts o, const char *s1, int len1, const char *s2, int len2 )
{
    if ( len1 != len2 )
        return CSTR_NOTEQUAL;
    return 0 == strncasecmp( s1, s2, len1 ) ? CSTR_EQUAL : CSTR_NOTEQUAL;
}

void CharLowerBuffW( char16_t *s1, int len )
{
    for( int i=0; i<len; ++i )
        *s1 = towlower( *s1 );
}

void CharLowerBuffA( char *s1, int len )
{
    for( int i=0; i<len; ++i )
        *s1 = tolower( *s1 );
}
void CharUpperBuffW( char16_t *s1, int len )
{
    for( int i=0; i<len; ++i )
        *s1 = toupper( *s1 );
}

void CharUpperBuffA( char *s1, int len )
{
    for( int i=0; i<len; ++i )
        *s1 = towupper( *s1 );
}


#endif