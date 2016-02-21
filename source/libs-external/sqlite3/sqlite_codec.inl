#include <sodium.h>

#define CODEC_FLAG_HAS_READ_KEY 1
#define CODEC_FLAG_HAS_WRITE_KEY 2

typedef struct _ChaChaKey
{
    unsigned char salt[16];
    unsigned char key[crypto_stream_chacha20_KEYBYTES];

} ChaChaKey;

typedef struct _Codec
{
    unsigned char temp[65536];
    int           flags;
    int           pagesize;
    ChaChaKey     read_key;
    ChaChaKey     write_key;
} Codec;

// zMagicHeader

void sqlite3_activate_see(const char *info)
{
    sodium_init();
}

/*
// Free the encryption data structure associated with a pager instance.
// (called from the modified code in pager.c) 
*/
static void codec_free(void *pCodecArg)
{
    if (pCodecArg)
    {
        sqlite3_free(pCodecArg);
    }
}

static void codec_size_changed(void *pArg, int pageSize, int reservedSize)
{
    ((Codec *)pArg)->pagesize = pageSize;
}

static void init_nonce( unsigned char *nonce, int pagenum )
{
    nonce[0] = pagenum & 0xff;
    nonce[1] = 1 ^ ((pagenum >> 8) & 0xff);
    nonce[2] = 48 ^ ((pagenum >> 16) & 0xff);
    nonce[3] = 161 ^ ((pagenum >> 24) & 0xff);
    for (int i = 4; i < crypto_stream_chacha20_NONCEBYTES; ++i)
        nonce[i] = nonce[i & 3] ^ (i + 1);
}

/*
// Encrypt/Decrypt functionality, called by pager.c
*/
void* codec_do_job(void* pCodecArg, void* data, Pgno nPageNum, int nMode)
{
    if (pCodecArg == NULL)
        return data;
  
    Codec* codec = (Codec*) pCodecArg;

    int pageSize = codec->pagesize;

    void* data_tgt = data;

    switch(nMode)
    {
    case 7: /* encrypt with read key; ChaCha20 xor is symetic, so encode and decode - same */

        data_tgt = codec->temp; // only difference - use temp buffer
        // no break here

    case 0: /* Undo a "case 7" journal file encryption */
    case 2: /* Reload a page */
    case 3: /* Load a page */
        if (0 != (codec->flags & CODEC_FLAG_HAS_READ_KEY))
        {
            // create nonce from pagenum
            unsigned char nonce[crypto_stream_chacha20_NONCEBYTES];
            init_nonce(nonce, nPageNum);
            crypto_stream_chacha20_xor(data_tgt, data, pageSize, nonce, codec->read_key.key );
            if (nPageNum == 1)
            {
                if (nMode == 7)
                    memcpy(data_tgt, codec->read_key.salt, 16);
                else
                    memcpy(data_tgt, zMagicHeader, 16);
            }
            return data_tgt;
        }
        break;

    case 6: /* Encrypt a page for the main database file */
        if (0 != (codec->flags & CODEC_FLAG_HAS_WRITE_KEY))
        {
            unsigned char nonce[crypto_stream_chacha20_NONCEBYTES];
            init_nonce(nonce, nPageNum);
            crypto_stream_chacha20_xor(codec->temp, data, pageSize, nonce, codec->write_key.key);
            if (nPageNum == 1)
                memcpy(codec->temp, codec->write_key.salt, 16 );
            return codec->temp;

        }
        break;
    }
    return data;
}

static void copy_key(ChaChaKey *k, const void* zKey, int nKey)
{
    memcpy(k->salt, zKey, 16);
    crypto_generichash( k->key, crypto_stream_chacha20_KEYBYTES, zKey, nKey, NULL, 0 );
}

static Codec *new_codec( const void* zKey, int nKey )
{
    Codec * pCodec = sqlite3_malloc(sizeof(Codec));
    copy_key( &pCodec->read_key, zKey, nKey );
    memcpy(&pCodec->write_key, &pCodec->read_key, sizeof(ChaChaKey));
    pCodec->flags = CODEC_FLAG_HAS_READ_KEY | CODEC_FLAG_HAS_WRITE_KEY;
    return pCodec;
}

int sqlite3CodecAttach(sqlite3* db, int nDb, const void* zKey, int nKey)
{
    Codec *pCodec;

    if (zKey == NULL || nKey <= 0)
    {
        // No key specified, could mean either use the main db's encryption or no encryption
        if (nDb != 0 && nKey < 0)
        {
            //Is an attached database, therefore use the key of main database, if main database is encrypted
            void *pMainCodec = sqlite3PagerGetCodec(sqlite3BtreePager(db->aDb[0].pBt));
            if (pMainCodec != NULL)
            {
                pCodec = sqlite3_malloc( sizeof(Codec) );
                memcpy( pCodec, pMainCodec, sizeof(Codec) );

                sqlite3PagerSetCodec(sqlite3BtreePager(db->aDb[nDb].pBt),
                                     codec_do_job,
                                     codec_size_changed,
                                     codec_free, pCodec);
            }
        }
    }
    else
    {
        // Key specified, setup encryption key for database
        pCodec = new_codec(zKey, nKey);
        pCodec->pagesize = sqlite3BtreeGetPageSize(db->aDb[nDb].pBt);

        sqlite3PagerSetCodec(sqlite3BtreePager(db->aDb[nDb].pBt),
                             codec_do_job,
                             codec_size_changed,
                             codec_free, pCodec);
    }

    return SQLITE_OK;
}

void sqlite3CodecGetKey(sqlite3* db, int nDb, void** zKey, int* nKey)
{
    *zKey = NULL;
    *nKey = -1;
}

int sqlite3_key(sqlite3 *db, const void *zKey, int nKey)
{
    return sqlite3CodecAttach(db, 0, zKey, nKey);
}

int sqlite3_key_v2(sqlite3 *db, const char * zDbName, const void *zKey, int nKey) // UNSUPPORTED
{
    /* The key is only set for the main database, not the temp database  */
    return sqlite3_key( db, zKey, nKey );
}

extern void rekey_process_callback(const void *zKey, int nKey, int i, int n);

int sqlite3_rekey(sqlite3 *db, const void *zKey, int nKey)
{
    // Changes the encryption key for an existing database.
    int rc = SQLITE_ERROR;
    Btree *pbt = db->aDb[0].pBt;
    Pager *pPager = sqlite3BtreePager(pbt);
    Codec *pCodec = sqlite3PagerGetCodec(pPager);

    if ((zKey == NULL || nKey == 0) && pCodec == NULL)
    {
        // Database not encrypted and key not specified. Do nothing
        return SQLITE_OK;
    }

    if (pCodec == NULL)
    {
        // Database not encrypted, but key specified. Encrypt database
        pCodec = new_codec(zKey, nKey);
        pCodec->flags &= ~CODEC_FLAG_HAS_READ_KEY; // set no read key
        sqlite3PagerSetCodec(pPager, codec_do_job,
                             codec_size_changed,
                             codec_free, pCodec);
    }
    else if (zKey == NULL || nKey == 0)
    {
        // Database encrypted, but key not specified. Decrypt database
        // Keep read key, drop write key
        pCodec->flags &= ~CODEC_FLAG_HAS_WRITE_KEY;
    }
    else
    {
        // Database encrypted and key specified. Re-encrypt database with new key
        // Keep read key, change write key to new key
        pCodec->flags |= CODEC_FLAG_HAS_WRITE_KEY;
        copy_key( &pCodec->write_key, zKey, nKey );
    }

    sqlite3_mutex_enter(db->mutex);

    // Start transaction
    rc = sqlite3BtreeBeginTrans(pbt, 1);
    if (rc == SQLITE_OK)
    {
        // Rewrite all pages using the new encryption key (if specified)
        int nPageCount = -1;
        sqlite3PagerPagecount(pPager, &nPageCount);
        Pgno nPage = (Pgno)nPageCount;

        Pgno nSkip = PAGER_MJ_PGNO(pPager);
        DbPage *pPage;

        Pgno n;
        for (n = 1; rc == SQLITE_OK && n <= nPage; n++)
        {
            if (n == nSkip)
                continue;

            rc = sqlite3PagerGet(pPager, n, &pPage, 0);

            if (!rc)
            {
                rc = sqlite3PagerWrite(pPage);
                sqlite3PagerUnref(pPage);
            }
            else
                sqlite3ErrorWithMsg(db, SQLITE_ERROR, "%s", "Error while rekeying database page. Transaction Canceled.");

            rekey_process_callback(zKey, nKey, n-1, nPage);
        }
    }
    else
        sqlite3ErrorWithMsg(db, SQLITE_ERROR, "%s", "Error beginning rekey transaction. Make sure that the current encryption key is correct.");

    if (rc == SQLITE_OK)
    {
        // All good, commit
        rc = sqlite3BtreeCommit(pbt);

        if (rc == SQLITE_OK)
        {
            //Database rekeyed and committed successfully, update read key
            if (0 != (pCodec->flags & CODEC_FLAG_HAS_WRITE_KEY))
            {
                copy_key(&pCodec->read_key, zKey, nKey);
                pCodec->flags |= CODEC_FLAG_HAS_READ_KEY;

            } else //No write key == no longer encrypted
                sqlite3PagerSetCodec(pPager, NULL, NULL, NULL, NULL);
        }
        else
        {
            //FIXME: can't trigger this, not sure if rollback is needed, reference implementation didn't rollback
            sqlite3ErrorWithMsg(db, SQLITE_ERROR, "%s", "Could not commit rekey transaction.");
        }
    }
    else
    {
        // Rollback, rekey failed
        sqlite3BtreeRollback(pbt, SQLITE_ERROR, 0);

        // go back to read key
        if (0 != (pCodec->flags & CODEC_FLAG_HAS_READ_KEY))
        {
            copy_key(&pCodec->write_key, zKey, nKey);
            pCodec->flags |= CODEC_FLAG_HAS_WRITE_KEY;
        }
        else //Database wasn't encrypted to start with
            sqlite3PagerSetCodec(pPager, NULL, NULL, NULL, NULL);
    }

    sqlite3_mutex_leave(db->mutex);

    rekey_process_callback(zKey, nKey, rc, 0);

    return rc;
}

int sqlite3_rekey_v2(sqlite3 *db, const char * zDbName, const void *zKey, int nKey)
{
    return sqlite3_rekey(db, zKey, nKey);
}

