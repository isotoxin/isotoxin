#include "isotoxin.h"

av_contact_s::av_contact_s( const contact_key_s &avk, state_e st ) :c( avk.find_root_contact() ), avkey(avk), state( st )
{
    inactive = false;
    dirty_cam_size = true;
    starttime = now();
    prevtick = ts::Time::current() - 1;

    volume = cfg().vol_talk();
    dsp_flags = cfg().dsp_flags();
    cvt.volume = cfg().vol_mic();
    cvt.filter_options.init( fmt_converter_s::FO_NOISE_REDUCTION, FLAG( dsp_flags, DSP_MIC_NOISE ) );
    cvt.filter_options.init( fmt_converter_s::FO_GAINER, FLAG( dsp_flags, DSP_MIC_AGC ) );

    ap = prf().ap( avk.protoid );
}

av_contact_s::~av_contact_s()
{
    g_app->mediasystem().free_voice_channel( ts::ref_cast<uint64>( avkey ) );
}

ts::uint32 av_contact_s::gm_handler( gmsg<ISOGM_PEER_STREAM_OPTIONS> &rso )
{
    if ( avkey == rso.avkey && state == AV_INPROGRESS )
    {
        remote_so = rso.so;
        remote_sosz = rso.videosize;
        dirty_cam_size = true;
    }
    return 0;
}

ts::uint32 av_contact_s::gm_handler( gmsg<ISOGM_CHANGED_SETTINGS> &ch )
{
    if ( ch.pass == 0 && ( ch.protoid == 0 || (unsigned)ch.protoid == avkey.protoid ) )
    {

        switch ( ch.sp )
        {
        case CFG_TALKVOLUME:
            volume = cfg().vol_talk();
            break;
        case CFG_MICVOLUME:
            cvt.volume = cfg().vol_mic();
            break;
        case CFG_DSPFLAGS:
            {
                int flags = cfg().dsp_flags();
                cvt.filter_options.init( fmt_converter_s::FO_NOISE_REDUCTION, FLAG( flags, DSP_MIC_NOISE ) );
                cvt.filter_options.init( fmt_converter_s::FO_GAINER, FLAG( flags, DSP_MIC_AGC ) );
                dsp_flags = flags;
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
    mic_switch();
    is_mic_off() ?
        ( (gui_button_c *)p )->set_face_getter( BUTTON_FACE( unmute_mic ) ) :
        ( (gui_button_c *)p )->set_face_getter( BUTTON_FACE( mute_mic ) );
    return true;
}

bool av_contact_s::b_speaker_switch( RID, GUIPARAM p )
{
    speaker_switch();
    is_speaker_off() ?
        ( (gui_button_c *)p )->set_face_getter( BUTTON_FACE( unmute_speaker ) ) :
        ( (gui_button_c *)p )->set_face_getter( BUTTON_FACE( mute_speaker ) );
    return true;
}

void av_contact_s::mic_off()
{
    int oso = cur_so();
    RESETFLAG( so, SO_SENDING_AUDIO );
    if ( cur_so() != oso )
    {
        send_so();
        c->redraw();
    }
}

void av_contact_s::camera_switch()
{
    int oso = cur_so();
    INVERTFLAG( so, SO_SENDING_VIDEO );
    if ( cur_so() != oso )
    {
        if ( !is_camera_on() )
            vsb.reset();

        send_so();
        c->redraw();
    }
}

void av_contact_s::mic_switch()
{
    int oso = cur_so();
    INVERTFLAG( so, SO_SENDING_AUDIO );
    if ( cur_so() != oso )
    {
        send_so();
        c->redraw();
    }
}

void av_contact_s::update_speaker()
{
    g_app->mediasystem().voice_mute( (uint64)avkey, is_speaker_off() );
}

void av_contact_s::speaker_switch()
{
    int oso = cur_so();
    INVERTFLAG( so, SO_RECEIVING_AUDIO );

    if ( cur_so() != oso )
    {
        update_speaker();
        send_so();
        c->redraw();
    }
}

void av_contact_s::camera( bool on )
{
    int oso = cur_so();
    INITFLAG( so, SO_SENDING_VIDEO, on );

    if ( cur_so() != oso )
    {
        send_so();
        c->redraw();
    }
}

void av_contact_s::set_recv_video( bool allow_recv )
{
    int oso = cur_so();
    INITFLAG( so, SO_RECEIVING_VIDEO, allow_recv );

    if ( cur_so() != oso )
    {
        send_so();
        c->redraw();
    }
}

void av_contact_s::set_inactive( bool inactive_ )
{
    int oso = cur_so();
    inactive = inactive_;

    if ( cur_so() != oso )
    {
        update_speaker();
        send_so();
        c->redraw();
    }
}

void av_contact_s::set_so_audio( bool inactive_, bool enable_mic, bool enable_speaker )
{
    int oso = cur_so();
    inactive = inactive_;

    INITFLAG( so, SO_SENDING_AUDIO, enable_mic );
    INITFLAG( so, SO_RECEIVING_AUDIO, enable_speaker );

    if ( cur_so() != oso )
    {
        update_speaker();
        send_so();
        c->redraw();
    }
}

void av_contact_s::set_video_res( const ts::ivec2 &vsz )
{
    if ( ts::tabs( sosz.x - vsz.x ) > 64 || ts::tabs( sosz.y - vsz.y ) > 64 )
    {
        sosz = vsz;
        send_so();
    }
}

void av_contact_s::send_so()
{
    if ( !ap )
        return;

    ap->set_stream_options( avkey.gidcid(), cur_so(), sosz );
}

vsb_c *av_contact_s::createcam()
{
    if ( currentvsb.id.is_empty() )
        return vsb_c::build();
    return vsb_c::build( currentvsb, cpar );
}

void av_contact_s::call_tick()
{
    ts::Time ct = ts::Time::current();
    int delta = ct - prevtick;
    prevtick = ct;
    mstime += delta;
}

void av_contact_s::camera_tick()
{
    if ( !vsb )
    {
        vsb.reset( createcam() );
        vsb->set_bmp_ready_handler( DELEGATE( this, on_frame_ready ) );
    }

    ts::ivec2 dsz = vsb->get_video_size();
    if ( dsz == ts::ivec2( 0 ) ) return;
    if ( dirty_cam_size || dsz != prev_video_size )
    {
        prev_video_size = dsz;
        if ( remote_sosz.x == 0 || ( remote_sosz >>= dsz ) )
        {
            vsb->set_desired_size( dsz );
        }
        else
        {
            dsz = vsb->fit_to_size( remote_sosz );
        }
        dirty_cam_size = false;
    }

    if ( vsb->updated() )
        gmsg<ISOGM_CAMERA_TICK>( c ).send();

}

void av_contact_s::on_frame_ready( const ts::bmpcore_exbody_s &ebm )
{
    if ( 0 == ( remote_so & SO_RECEIVING_VIDEO ) || ap == nullptr )
        return;

    ap->send_video_frame( avkey.contactid, ebm, mstime );
}



void av_contact_s::add_audio( uint64 timestamp, const s3::Format &fmt, const void *data, ts::aint size )
{
    if ( 0 == ( so & SO_RECEIVING_AUDIO ) )
        return;

    if ( timestamp == 0 || 0 == ( so & SO_RECEIVING_VIDEO ) || 0 == ( remote_so & SO_SENDING_VIDEO ) )
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
    if ( 0 != ( dsp_flags & DSP_SPEAKERS_NOISE ) ) dspf |= fmt_converter_s::FO_NOISE_REDUCTION;
    if ( 0 != ( dsp_flags & DSP_SPEAKERS_AGC ) ) dspf |= fmt_converter_s::FO_GAINER;

    g_app->mediasystem().play_voice( ts::ref_cast<int64>( avkey ), fmt, data, size, volume, dspf );
}


void av_contact_s::send_audio( const s3::Format& ifmt, const void *data, ts::aint size, bool clear_cvt_buffer )
{
    if ( clear_cvt_buffer || ap == nullptr )
        cvt.clear();

    if ( !ap )
        return;

    struct s
    {
        uint64 msmonotonic;
        active_protocol_c *ap;
        int cid;
        void send_audio( const s3::Format& ofmt, const void *data, ts::aint size )
        {
            ap->send_audio( cid, data, size, msmonotonic - ofmt.bytesToMSec(size) );
        }
    } ss = { mstime, ap, (int)avkey.gidcid() };

    cvt.ofmt = ap->defaudio();
    cvt.acceptor = DELEGATE( &ss, send_audio );
    cvt.cvt( ifmt, data, size );

}




int av_contacts_c::get_avinprogresscount() const
{
    int cnt = 0;
    for ( const av_contact_s *avc : m_contacts )
        if ( av_contact_s::AV_INPROGRESS == avc->state )
            ++cnt;
    return cnt;
}

int av_contacts_c::get_avringcount() const
{
    int cnt = 0;
    for ( const av_contact_s *avc : m_contacts )
        if ( av_contact_s::AV_RINGING == avc->state )
            ++cnt;
    return cnt;
}

av_contact_s * av_contacts_c::find_inprogress( const contact_key_s &avk )
{
    SIMPLELOCK( sync );

    for ( av_contact_s *avc : m_contacts )
        if ( av_contact_s::AV_INPROGRESS == avc->state && avc->avkey == avk )
            return avc;
    return nullptr;

}

av_contact_s & av_contacts_c::get( const contact_key_s &avk, av_contact_s::state_e st )
{
    ASSERT( spinlock::pthread_self() == g_app->base_tid() );

    for ( av_contact_s *avc : m_contacts )
        if ( avc->avkey == avk )
        {
            if ( av_contact_s::AV_NONE != st )
                avc->state = st;
            return *avc;
        }

    av_contact_s *avc = TSNEW( av_contact_s, avk, st );

    spinlock::simple_lock( sync );
    m_contacts.add( avc );
    spinlock::simple_unlock( sync );

    avc->send_so(); // update stream options now
    return *avc;
}

void av_contacts_c::del( contact_root_c *c )
{
    ASSERT( spinlock::pthread_self() == g_app->base_tid() );

    for ( ts::aint i = m_contacts.size() - 1; i >= 0; --i )
    {
        if ( m_contacts.get( i )->c == c )
        {
            SIMPLELOCK( sync );
            m_contacts.remove_fast( i );
        }
    }
}

void av_contacts_c::stop_all_av()
{
    gmsg<ISOGM_NOTICE>( nullptr, nullptr, NOTICE_KILL_CALL_INPROGRESS ).send();

    while ( m_contacts.size() )
        m_contacts.get( 0 )->c->stop_av();
}

void av_contacts_c::clear()
{
    SIMPLELOCK( sync );
    m_contacts.clear();
}


bool av_contacts_c::camera_tick()
{
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

