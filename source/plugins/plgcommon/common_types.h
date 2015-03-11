#pragma once

#include "boost/boost_some.h"

enum proto_features_e
{
    PF_INVITE_NAME          = (1 << 0),  // invite send name
    PF_UNAUTHORIZED_CHAT    = (1 << 1),
    PF_AUDIO_CALLS          = (1 << 2),
    PF_VIDEO_CALLS          = (1 << 3),
    PF_SEND_FILE            = (1 << 4),
    PF_FILE_RESUME          = (1 << 5),
};

enum proxy_support_e
{
    PROXY_SUPPORT_HTTP = 1,
    PROXY_SUPPORT_SOCKS4 = 2,
    PROXY_SUPPORT_SOCKS5 = 4,
};

enum cd_mask_e
{
    // no CDM_ID because id always valid
    CDM_PUBID           = 1 << 0,
    CDM_NAME            = 1 << 1,
    CDM_STATUSMSG       = 1 << 2,
    CDM_STATE           = 1 << 3,
    CDM_ONLINE_STATE    = 1 << 4,
    CDM_GENDER          = 1 << 5,
    CDM_AVATAR_TAG      = 1 << 6,
};

enum cmd_result_e
{
    CR_OK,
    CR_MODULE_NOT_FOUND,
    CR_FUNCTION_NOT_FOUND,
    CR_INVALID_PUB_ID,
    CR_ALREADY_PRESENT,
    CR_MULTIPLE_CALL,
    CR_MEMORY_ERROR,
    CR_TIMEOUT,
    CR_OPERATION_IN_PROGRESS,
};

enum stop_call_e
{
    STOPCALL_REJECT, // reject incoming call
    STOPCALL_CANCEL, // cancel outgoing call
    STOPCALL_HANGUP, // break current call
};

enum file_control_e
{
    FIC_NONE,
    FIC_ACCEPT,
    FIC_REJECT,
    FIC_BREAK,
    FIC_DONE,
    FIC_PAUSE,
    FIC_UNPAUSE,
};

enum message_type_e : unsigned // hard order
{
    MT_MESSAGE,
    MT_ACTION,
    MT_FRIEND_REQUEST,
    MT_INCOMING_CALL,
    MT_CALL_STOP,
    MT_CALL_ACCEPTED,

    message_type_check,
    message_type_bits = 1 + (::boost::static_log2<(message_type_check - 1)>::value)

};

enum contact_state_e : unsigned // hard order
{
    CS_INVITE_SEND,     // friend invite was sent
    CS_INVITE_RECEIVE,
    CS_REJECTED,
    CS_ONLINE,
    CS_OFFLINE,
    CS_ROTTEN,
    CS_WAIT,

    contact_state_check,
    contact_state_bits = 1 + (::boost::static_log2<(contact_state_check - 1)>::value)
};

enum contact_online_state_e : unsigned
{
    COS_ONLINE,
    COS_AWAY,
    COS_DND,

    contact_online_state_check,
    contact_online_state_bits = 1 + (::boost::static_log2<(contact_online_state_check - 1)>::value)
};

enum contact_gender_e : unsigned
{
    CSEX_UNKNOWN,
    CSEX_MALE,
    CSEX_FEMALE,

    contact_gender_count,
    contact_gender_bits = 1 + (::boost::static_log2<(contact_gender_count - 1)>::value)
};

