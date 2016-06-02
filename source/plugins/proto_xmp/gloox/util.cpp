/*
  Copyright (c) 2006-2015 by Jakob Schröter <js@camaya.net>
  This file is part of the gloox library. http://camaya.net/gloox

  This software is distributed under a license. The full license
  agreement can be found in the file LICENSE in this distribution.
  This software may not be copied, modified, sold or distributed
  other than expressed in the named license agreement.

  This software is distributed without any warranty.
*/

#include "util.h"
#include "gloox.h"

#include <cstdio>

namespace gloox
{

  namespace util
  {

    int internalLog2( unsigned int n )
    {
      int pos = 0;
      if ( n >= 1<<16 ) { n >>= 16; pos += 16; }
      if ( n >= 1<< 8 ) { n >>=  8; pos +=  8; }
      if ( n >= 1<< 4 ) { n >>=  4; pos +=  4; }
      if ( n >= 1<< 2 ) { n >>=  2; pos +=  2; }
      if ( n >= 1<< 1 ) {           pos +=  1; }
      return ( (n == 0) ? (-1) : pos );
    }

    unsigned _lookup( const std::string& str, const char* values[], unsigned size, int def )
    {
      unsigned i = 0;
      for( ; i < size && str != values[i]; ++i )
        ;
      return ( i == size && def >= 0 ) ? (unsigned)def : i;
    }

    const std::string _lookup( unsigned code, const char* values[], unsigned size, const std::string& def )
    {
      return code < size ? std::string( values[code] ) : def;
    }

    unsigned _lookup2( const std::string& str, const char* values[],
                       unsigned size, int def )
    {
      return 1 << _lookup( str, values, size, def <= 0 ? def : (int)internalLog2( def ) );
    }

    const std::string _lookup2( unsigned code, const char* values[], unsigned size, const std::string& def )
    {
      const unsigned i = (unsigned)internalLog2( code );
      return i < size ? std::string( values[i] ) : def;
    }

    std::string hex( const std::string& input )
    {
      const char* H = input.c_str();
      char* buf = new char[input.length() * 2 + 1];
      for( unsigned int i = 0; i < input.length(); ++i )
        sprintf( buf + i * 2, "%02x", (unsigned char)( H[i] ) );
      return std::string( buf, 40 );
    }

    static const char escape_chars[] = { '&', '<', '>', '\'', '"' };

    static const std::string escape_seqs[] = { "amp;", "lt;", "gt;", "apos;", "quot;" };

    static const std::string escape_seqs_full[] = { "&amp;", "&lt;", "&gt;", "&apos;", "&quot;" };

    static const unsigned escape_size = 5;

    const std::string escape( std::string what )
    {
      for( size_t val, i = 0; i < what.length(); ++i )
      {
        for( val = 0; val < escape_size; ++val )
        {
          if( what[i] == escape_chars[val] )
          {
            what[i] = '&';
            what.insert( i+1, escape_seqs[val] );
            i += escape_seqs[val].length();
            break;
          }
        }
      }
      return what;
    }

    void appendEscaped( std::string& target, const std::string& data )
    {
      size_t rangeStart = 0, rangeCount = 0;
      size_t length = data.length();
      const char* dataPtr = data.data();
      for( size_t val, i = 0; i < length; ++i )
      {
        const char current = dataPtr[i];
        for( val = 0; val < escape_size; ++val )
        {
          if( current == escape_chars[val] )
          {
            // We have a character that needs to be escaped.
            if( rangeCount > 0 )
            {
              // We have a range of the data that needs to be appended
              // before we escape the current character.
              // NOTE: Use "data" (std::string) here not dataPtr (const char*).
              //  Both have the same content, but there isn't
              //  an append override that takes const char*, pos, n
              //  (so a temporary std::string would be created)
              target.append( data, rangeStart, rangeCount );
            }
            target.append( escape_seqs_full[val] );
            rangeStart = i + 1;
            rangeCount = 0;
            break;
          }
        }

        if( rangeStart <= i )
        {
          // current did not need to be escaped
          ++rangeCount;
        }
      }

      if( rangeCount > 0 )
      {
        // Append the remaining pending range of data that does
        // not need to be escaped.
        // NOTE: See previous comment on using data not dataPtr for append.
        target.append( data, rangeStart, rangeCount );
      }
    }

    bool checkValidXMLChars( const std::string& data )
    {
      if( data.empty() )
        return true;

      const char* dataPtr = data.data();
      const char* end = dataPtr + data.length();
      for( ; dataPtr != end; ++dataPtr )
      {
        unsigned char current = (unsigned char) *dataPtr;
        if( current < 0x20 )
        {
          if( current == 0x09
              || current == 0x0a
              || current == 0x0d )
            // Valid character
            continue;
          else
            // Invalid character
            break;
        }
        else if( current >= 0xf5 )
          // Invalid character
          break;
        else if( current == 0xc0
                 || current == 0xc1 )
          // Invalid character
          break;
        else
          // Valid character
          continue;
      }

      return ( dataPtr == end );
    }

    void replaceAll( std::string& target, const std::string& find, const std::string& replace )
    {
      std::string::size_type findSize = find.size();
      std::string::size_type replaceSize = replace.size();

      if( findSize == 0 )
        return;

      std::string::size_type index = target.find( find, 0 );

      while( index != std::string::npos )
      {
        target.replace( index, findSize, replace );
        index = target.find( find, index+replaceSize );
      }
    }

  }

}

