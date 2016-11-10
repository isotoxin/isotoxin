#include "isotoxin.h"

av_contact_s::av_contact_s( contact_root_c *c, contact_c *sub, state_e st ):sub( sub )
{
    tag = gui->get_free_tag();

    if ( av_contact_s * same = g_app->avcontacts().find_inprogress_any( c ) )
    {
        core = same->core;
        core->state = st;
    }
    else
        core = TSNEW( ccore_s, c, st );

    core->inactive = false;
    core->dirty_cam_size = true;
    core->starttime = now();
    core->prevtick = ts::Time::current() - 1;

    volume = cfg().vol_talk();
    speaker_dsp_flags = cfg().dsp_flags();
    core->mic_dsp_flags = speaker_dsp_flags & DSP_MIC;
    core->cvt.volume = cfg().vol_mic();
    core->cvt.filter_options.init( fmt_converter_s::FO_NOISE_REDUCTION, FLAG( speaker_dsp_flags, DSP_MIC_NOISE ) );
    core->cvt.filter_options.init( fmt_converter_s::FO_GAINER, FLAG( speaker_dsp_flags, DSP_MIC_AGC ) );
    speaker_dsp_flags = speaker_dsp_flags & DSP_SPEAKER;

    avkey = c->getkey() | sub->getkey();

    ASSERT( contact_key_s(avkey).protoid == sub->getkey().protoid );
    ASSERT( !c->getkey().is_conference() || c->getkey().protoid == sub->getkey().protoid );

    core->ap = prf().ap( sub->getkey().protoid );
}

av_contact_s::~av_contact_s()
{
    g_app->mediasystem().free_voice_channel( avkey );
}

ts::uint32 av_contact_s::gm_handler( gmsg<ISOGM_CHANGED_SETTINGS> &ch )
{
    if ( ch.pass == 0 && ( ch.protoid == 0 || (unsigned)ch.protoid == contact_key_s( avkey ).protoid ) )
    {

        switch ( ch.sp )
        {
        case CFG_TALKVOLUME:
            volume = cfg().vol_talk();
            break;
        case CFG_MICVOLUME:
            core->cvt.volume = cfg().vol_mic();
            break;
        case CFG_DSPFLAGS:
            {
                int flags = cfg().dsp_flags();
                core->cvt.filter_options.init( fmt_converter_s::FO_NOISE_REDUCTION, FLAG( flags, DSP_MIC_NOISE ) );
                core->cvt.filter_options.init( fmt_converter_s::FO_GAINER, FLAG( flags, DSP_MIC_AGC ) );
                core->mic_dsp_flags = flags & DSP_MIC;
                speaker_dsp_flags = flags & DSP_SPEAKER;
            }
            break;
        }
    }

    return 0;
}


void av_contact_s::update_btn_face_camera( gui_button_c &btn )
{
    is_camera_on() ?
        btn.set_face_getter( BUTTON_FACE( on_camera ) ) :
        btn.set_face_getter( BUTTON_FACE( off_camera ) );
}

bool av_contact_s::b_mic_switch( RID, GUIPARAM p )
{
    mic_switch( p != nullptr );
    return true;
}

bool av_contact_s::b_speaker_switch( RID, GUIPARAM p )
{
    speaker_switch( p != nullptr );
    return true;
}

void av_contact_s::mic_off()
{
    int oso = cur_so();
    RESETFLAG( core->so, SO_SENDING_AUDIO );
    if ( cur_so() != oso )
    {
        send_so();
        core->c->redraw();
    }
}

void av_contact_s::camera_switch()
{
    int oso = cur_so();
    INVERTFLAG( core->so, SO_SENDING_VIDEO );
    if ( cur_so() != oso )
    {
        if ( !is_camera_on() )
            core->vsb.reset();

        send_so();
        core->c->redraw();
    }
}

void av_contact_s::mic_switch(bool enable)
{
    int oso = cur_so();
    INITFLAG( core->so, SO_SENDING_AUDIO, enable );
    if ( cur_so() != oso )
    {
        send_so();
        core->c->redraw();

        if (conference_s *c = core->c->find_conference())
            c->change_flag( conference_s::F_MIC_ENABLED, is_mic_on() );
    }
}

void av_contact_s::update_speaker()
{
    g_app->mediasystem().voice_mute( avkey, is_speaker_off() );
}

void av_contact_s::speaker_switch( bool enable )
{
    int oso = cur_so();
    INITFLAG( core->so, SO_RECEIVING_AUDIO, enable );

    if ( cur_so() != oso )
    {
        update_speaker();
        send_so();
        core->c->redraw();

        if (conference_s *c = core->c->find_conference())
            c->change_flag( conference_s::F_SPEAKER_ENABLED, is_speaker_on() );
    }
}

void av_contact_s::camera( bool on )
{
    int oso = cur_so();
    INITFLAG( core->so, SO_SENDING_VIDEO, on );

    if ( cur_so() != oso )
    {
        send_so();
        core->c->redraw();
    }
}

void av_contact_s::set_recv_video( bool allow_recv )
{
    int oso = cur_so();
    INITFLAG( core->so, SO_RECEIVING_VIDEO, allow_recv );

    if ( cur_so() != oso )
    {
        send_so();
        core->c->redraw();
    }
}

void av_contact_s::set_inactive( bool inactive_ )
{
    int oso = cur_so();
    core->inactive = inactive_;

    if ( cur_so() != oso )
    {
        update_speaker();
        send_so();
        core->c->redraw();
    }
}

void av_contact_s::set_so_audio( bool inactive_, bool enable_mic, bool enable_speaker )
{
    int oso = cur_so();
    core->inactive = inactive_;

    INITFLAG( core->so, SO_SENDING_AUDIO, enable_mic );
    INITFLAG( core->so, SO_RECEIVING_AUDIO, enable_speaker );

    if ( cur_so() != oso )
    {
        update_speaker();
        send_so();
        core->c->redraw();
    }
}

void av_contact_s::set_video_res( const ts::ivec2 &vsz )
{
    if ( ts::tabs( core->sosz.x - vsz.x ) > 64 || ts::tabs( core->sosz.y - vsz.y ) > 64 )
    {
        core->sosz = vsz;
        send_so();
    }
}

void av_contact_s::send_so()
{
    if ( !core->ap )
        return;

    core->ap->set_stream_options( contact_key_s(avkey).gidcid(), cur_so(), core->sosz );
}

vsb_c *av_contact_s::createcam()
{
    if ( core->currentvsb.id.is_empty() )
        return vsb_c::build();
    return vsb_c::build( core->currentvsb, core->cpar );
}

void av_contact_s::call_tick()
{
    ts::Time ct = ts::Time::current();
    int delta = ct - core->prevtick;
    core->prevtick = ct;
    core->mstime += delta;
}

void av_contact_s::camera_tick()
{
    if ( !core->vsb )
    {
        core->vsb.reset( createcam() );
        core->vsb->set_bmp_ready_handler( DELEGATE( this, on_frame_ready ) );
    }

    ts::ivec2 dsz = core->vsb->get_video_size();
    if ( dsz == ts::ivec2( 0 ) ) return;
    if ( core->dirty_cam_size || dsz != core->prev_video_size )
    {
        core->prev_video_size = dsz;
        if ( remote_sosz.x == 0 || ( remote_sosz >>= dsz ) )
        {
            core->vsb->set_desired_size( dsz );
        }
        else
        {
            dsz = core->vsb->fit_to_size( remote_sosz );
        }
        core->dirty_cam_size = false;
    }

    if ( core->vsb->updated() )
        gmsg<ISOGM_CAMERA_TICK>( core->c ).send();

}

void av_contact_s::on_frame_ready( const ts::bmpcore_exbody_s &ebm )
{
    if ( 0 == ( remote_so & SO_RECEIVING_VIDEO ) || core->ap == nullptr )
        return;

    core->ap->send_video_frame( contact_key_s(avkey).contactid, ebm, core->mstime );
}



void av_contact_s::add_audio( uint64 timestamp, const s3::Format &fmt, const void *data, ts::aint size )
{
    // ACHTUNG! *NOT* MAIN THREAD

    if ( 0 == ( core->so & SO_RECEIVING_AUDIO ) )
        return;

    g_app->avcontacts().set_indicator( avkey, lround( find_max_sample( fmt, data, size ) * 100.0f ) );

    if ( timestamp == 0 || 0 == ( core->so & SO_RECEIVING_VIDEO ) || 0 == ( remote_so & SO_SENDING_VIDEO ) )
    {
        // no video, no need to synchronize
        // play now
        play_audio( fmt, data, size );
        return;
    }

    play_audio( fmt, data, size );
}

void av_contact_s::play_audio( const s3::Format &fmt, const void *data, ts::aint size )
{
    int dspf = 0;
    if ( 0 != ( speaker_dsp_flags & DSP_SPEAKERS_NOISE ) ) dspf |= fmt_converter_s::FO_NOISE_REDUCTION;
    if ( 0 != ( speaker_dsp_flags & DSP_SPEAKERS_AGC ) ) dspf |= fmt_converter_s::FO_GAINER;

    g_app->mediasystem().play_voice( avkey, fmt, data, size, volume, dspf );
}


void av_contact_s::send_audio( const s3::Format& ifmt, const void *data, ts::aint size, bool clear_cvt_buffer )
{
    if ( clear_cvt_buffer || core->ap == nullptr )
        core->cvt.clear();

    if ( 0 == ( remote_so & SO_RECEIVING_AUDIO ) || core->ap == nullptr )
        return;

    struct s
    {
        uint64 msmonotonic;
        active_protocol_c *ap;
        int cid;
        void send_audio( const s3::Format& ofmt, const void *data, ts::aint size )
        {
            ap->send_audio( cid, data, static_cast<int>(size), msmonotonic - ofmt.bytesToMSec( static_cast<int>( size ) ) );
        }
    } ss = { core->mstime, core->ap, contact_key_s(avkey).gidcid() };

    core->cvt.ofmt = core->ap->defaudio();
    core->cvt.acceptor = DELEGATE( &ss, send_audio );
    core->cvt.cvt( ifmt, data, size );

}


av_contacts_c::~av_contacts_c()
{
    if (gui)
        gui->delete_event( DELEGATE( this, clean_indicators ) );
}


int av_contacts_c::get_avinprogresscount() const
{
    int cnt = 0;
    for ( const av_contact_s *avc : m_contacts )
        if ( av_contact_s::AV_INPROGRESS == avc->core->state )
            ++cnt;
    return cnt;
}

int av_contacts_c::get_avringcount() const
{
    int cnt = 0;
    for ( const av_contact_s *avc : m_contacts )
        if ( av_contact_s::AV_RINGING == avc->core->state )
            ++cnt;
    return cnt;
}

av_contact_s * av_contacts_c::find_inprogress( uint64 avk )
{
    SIMPLELOCK( sync );

    for ( av_contact_s *avc : m_contacts )
        if ( av_contact_s::AV_INPROGRESS == avc->core->state && avc->avkey == avk )
            return avc;
    return nullptr;
}

av_contact_s * av_contacts_c::find_inprogress_any( contact_root_c *c )
{
    SIMPLELOCK( sync );

    for ( av_contact_s *avc : m_contacts )
        if ( av_contact_s::AV_INPROGRESS == avc->core->state && avc->core->c == c )
            return avc;
    return nullptr;
}

bool av_contacts_c::is_any_inprogress( contact_root_c *cr )
{
    SIMPLELOCK( sync );

    for ( av_contact_s *avc : m_contacts )
        if ( av_contact_s::AV_INPROGRESS == avc->core->state && avc->core->c == cr )
            return true;
    return false;
}

void av_contacts_c::set_tag( contact_root_c *cr, int tag )
{
    for ( av_contact_s *avc : m_contacts )
        if ( avc->core->c == cr )
            avc->tag = tag;
}

av_contact_s & av_contacts_c::get( uint64 avkey, av_contact_s::state_e st )
{
    ASSERT( spinlock::pthread_self() == g_app->base_tid() );

    for ( av_contact_s *avc : m_contacts )
        if ( avc->avkey == avkey )
        {
            if ( av_contact_s::AV_NONE != st )
                avc->core->state = st;
            return *avc;
        }

    contact_root_c *root = contact_key_s( avkey ).find_root_contact();
    contact_c *sub = contact_key_s( avkey ).find_sub_contact();
    ASSERT( root != nullptr && sub != nullptr );
    ASSERT( avkey == ( root->getkey() | sub->getkey() ) );

    av_contact_s *avc = TSNEW( av_contact_s, root, sub, st );

    if ( root->getkey().is_conference() )
        if ( auto * x = m_preso.find( contact_key_s() | root->getkey() ) )
            avc->remote_so = x->value.so, avc->remote_sosz = x->value.videosize;

    if ( auto * x = m_preso.find( avkey ) )
    {
        avc->remote_so = x->value.so;
        avc->remote_sosz = x->value.videosize;
        m_preso.remove( avkey );
    }

    spinlock::simple_lock( sync );
    m_contacts.add( avc );
    spinlock::simple_unlock( sync );

    avc->send_so(); // update stream options now
    return *avc;
}

void av_contacts_c::del( active_protocol_c *ap )
{
    ASSERT( spinlock::pthread_self() == g_app->base_tid() );

    for ( ts::aint i = m_contacts.size() - 1; i >= 0; --i )
    {
        if ( m_contacts.get( i )->core->ap == ap )
        {
            SIMPLELOCK( sync );
            m_contacts.remove_fast( i );
        }
    }
}

void av_contacts_c::del( contact_root_c *c )
{
    ASSERT( spinlock::pthread_self() == g_app->base_tid() );

    for ( ts::aint i = m_contacts.size() - 1; i >= 0; --i )
    {
        if ( m_contacts.get( i )->core->c == c )
        {
            SIMPLELOCK( sync );
            m_contacts.remove_fast( i );
        }
    }
}

void av_contacts_c::del( int tag )
{
    ASSERT( spinlock::pthread_self() == g_app->base_tid() );

    for ( ts::aint i = m_contacts.size() - 1; i >= 0; --i )
    {
        if ( m_contacts.get( i )->tag == tag )
        {
            SIMPLELOCK( sync );
            m_contacts.remove_fast( i );
        }
    }
}

void av_contacts_c::stop_all_av()
{
    gmsg<ISOGM_NOTICE>( nullptr, nullptr, NOTICE_KILL_CALL_INPROGRESS ).send();

    ts::tmp_pointers_t< contact_root_c, 0 > tmp;
    for ( av_contact_s *av : m_contacts )
        tmp.set( av->core->c.get() );

    for ( contact_root_c *c : tmp )
        c->stop_av();

    m_contacts.clear();
}

void av_contacts_c::clear()
{
    SIMPLELOCK( sync );
    m_contacts.clear();
}


bool av_contacts_c::tick()
{
    if (g_app->F_INDICATOR_CHANGED && !clean_started)
    {
        if (m_indicators.lock_read()().count())
        {
            clean_started = true;
            DEFERRED_UNIQUE_CALL( 0.1, DELEGATE( this, clean_indicators ), 0 );
        }  else
            g_app->F_INDICATOR_CHANGED = false;
    }

    bool c = false;
    for ( av_contact_s *avc : m_contacts )
    {
        if ( avc->is_camera_on() )
        {
            avc->camera_tick();
            c = true;
        }
    }
    return c;
}

ts::uint32 av_contacts_c::gm_handler( gmsg<ISOGM_PEER_STREAM_OPTIONS> &rso )
{
    contact_root_c *root = contact_key_s( rso.avkey ).find_root_contact();
    if ( !root ) return 0;

    contact_c *sub = contact_key_s( rso.avkey ).find_sub_contact();

    bool processed = false;
    for( av_contact_s *avc : m_contacts )
    {
        if ( avc->core->c == root && ( !sub || sub == avc->sub ) && avc->core->state == av_contact_s::AV_INPROGRESS )
        {
            avc->remote_so = rso.so;
            avc->remote_sosz = rso.videosize;
            avc->core->dirty_cam_size = true;
            m_preso.remove( rso.avkey );
            processed = true;
        }
    }

    if ( processed )
    {
        if ( root->getkey().is_conference() )
            m_preso.remove( contact_key_s() | root->getkey() );

        return 0;
    }

    so_s &so = m_preso[ rso.avkey ];

    so.so = rso.so;
    so.videosize = rso.videosize;
    return 0;
}


bool av_contacts_c::clean_indicators( RID, GUIPARAM )
{
    auto w = m_indicators.lock_write();

    for (ts::aint i= w().count()-1;i>=0;--i)
    {
        ind_s &p = w().get( i );
        if ( p.indicator == 0 )
        {
            --p.cooldown;
            if (p.cooldown < 0)
                w().remove_fast( i );
        } else
        {
            p.cooldown = 5;
            p.indicator -= 10;
            if (p.indicator < 0)
                p.indicator = 0;
        }
    }

    if (w().count())
        DEFERRED_UNIQUE_CALL( 0.1, DELEGATE( this, clean_indicators ), 0 );
    else
        clean_started = false;

    g_app->F_INDICATOR_CHANGED = true;

    return true;
}

void av_contacts_c::set_indicator( uint64 avk, int iv )
{
    auto w = m_indicators.lock_write();

    for(ind_s &p : w())
        if ( p.avk == avk )
        {
            g_app->F_INDICATOR_CHANGED = true;
            p.indicator = iv;
            p.cooldown = 5;
            return;
        }
    auto &p = w().add();
    p.avk = avk;
    p.indicator = iv;
    p.cooldown = 5;
    g_app->F_INDICATOR_CHANGED = true;
}

int av_contacts_c::get_indicator( uint64 avk )
{
    int rv = -1;
    int some = false;
    auto r = m_indicators.lock_read();
    some = r().count() != 0;
    for (const ind_s &p : r())
    {
        if (p.avk == avk)
        {
            rv = p.indicator;
            r.unlock();
            break;
        }
    }
    if (some && !clean_started)
    {
        clean_started = true;
        DEFERRED_UNIQUE_CALL( 0.1, DELEGATE( this, clean_indicators ), 0 );
    }
    return rv;
}
