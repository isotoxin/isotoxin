/*
  Copyright (c) 2006-2015 by Jakob Schröter <js@camaya.net>
  This file is part of the gloox library. http://camaya.net/gloox

  This software is distributed under a license. The full license
  agreement can be found in the file LICENSE in this distribution.
  This software may not be copied, modified, sold or distributed
  other than expressed in the named license agreement.

  This software is distributed without any warranty.
*/



#include "mucinvitationhandler.h"
#include "mucroom.h"

namespace gloox
{

  MUCInvitationHandler::MUCInvitationHandler( ClientBase* parent )
  {
    if( parent )
      parent->registerStanzaExtension( new MUCRoom::MUCUser() );
  }

}
