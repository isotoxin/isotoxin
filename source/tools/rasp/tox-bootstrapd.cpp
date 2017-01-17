#include "stdafx.h"
#include <vector>

#pragma warning (disable : 4200) // nonstandard extension used : zero-sized array in struct/union
#pragma warning (disable : 4324) // structure was padded due to __declspec(align())
#pragma warning (disable : 4244) // 'argument' : conversion from 'int' to 'uint8_t', possible loss of data

/* tox-bootstrapd.c
 *
 * Tox DHT bootstrap daemon.
 *
 *  Copyright (C) 2014 Tox project All Rights Reserved.
 *
 *  This file is part of Tox.
 *
 *  Tox is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  Tox is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with Tox.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

// system provided

// C
#ifdef _WIN32
#include <WinSock2.h>
#include <ws2tcpip.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// 3rd party
//#include <libconfig.h>

// ./configure
//#ifdef HAVE_CONFIG_H
//#include "config.h"
//#endif

typedef bool _Bool;

// toxcore
extern "C"
{
#include "toxcore/toxcore/LAN_discovery.h"
#include "toxcore/toxcore/onion_announce.h"
#include "toxcore/toxcore/TCP_server.h"
#include "toxcore/toxcore/util.h"

// misc
#include "bootstrap_node_packets.c"
//#include "../../testing/misc_tools.c"
}


#define DAEMON_NAME "tox-bootstrapd"
#define DAEMON_VERSION_NUMBER 2014101200UL // yyyymmmddvv format: yyyy year, mm month, dd day, vv version change count for that day

#define SLEEP_TIME_MILLISECONDS 30
#define sleep usleep(1000*SLEEP_TIME_MILLISECONDS)

#define DEFAULT_KEYS_FILE_PATH        "tox-bootstrapd.keys"
#define DEFAULT_PORT                  33445
#define DEFAULT_ENABLE_IPV6           1 // 1 - true, 0 - false
#define DEFAULT_ENABLE_IPV4_FALLBACK  1 // 1 - true, 0 - false
#define DEFAULT_ENABLE_LAN_DISCOVERY  1 // 1 - true, 0 - false
#define DEFAULT_ENABLE_TCP_RELAY      1 // 1 - true, 0 - false
#define DEFAULT_TCP_RELAY_PORTS       443, 3389, 33445 // comma-separated list of ports. make sure to adjust DEFAULT_TCP_RELAY_PORTS_COUNT accordingly
#define DEFAULT_TCP_RELAY_PORTS_COUNT 3
#define DEFAULT_ENABLE_MOTD           1 // 1 - true, 0 - false
#define DEFAULT_MOTD                  DAEMON_NAME

#define MIN_ALLOWED_PORT 1
#define MAX_ALLOWED_PORT 65535


// Uses the already existing key or creates one if it didn't exist
//
// retirns 1 on success
//         0 on failure - no keys were read or stored

int manage_keys(DHT *dht, char *keys_file_path)
{
    const uint32_t KEYS_SIZE = crypto_box_PUBLICKEYBYTES + crypto_box_SECRETKEYBYTES;
    uint8_t keys[KEYS_SIZE];
    FILE *keys_file;

    // Check if file exits, proceed to open and load keys
    keys_file = fopen(keys_file_path, "r");

    if (keys_file != NULL) {
        const size_t read_size = fread(keys, sizeof(uint8_t), KEYS_SIZE, keys_file);

        if (read_size != KEYS_SIZE) {
            fclose(keys_file);
            return 0;
        }

        memcpy(dht->self_public_key, keys, crypto_box_PUBLICKEYBYTES);
        memcpy(dht->self_secret_key, keys + crypto_box_PUBLICKEYBYTES, crypto_box_SECRETKEYBYTES);
    } else {
        // Otherwise save new keys
        memcpy(keys, dht->self_public_key, crypto_box_PUBLICKEYBYTES);
        memcpy(keys + crypto_box_PUBLICKEYBYTES, dht->self_secret_key, crypto_box_SECRETKEYBYTES);

        keys_file = fopen(keys_file_path, "w");

        const size_t write_size = fwrite(keys, sizeof(uint8_t), KEYS_SIZE, keys_file);

        if (write_size != KEYS_SIZE) {
            fclose(keys_file);
            return 0;
        }
    }

    fclose(keys_file);

    return 1;
}


// Gets general config options
//
// Important: you are responsible for freeing `pid_file_path` and `keys_file_path`
//            also, iff `tcp_relay_ports_count` > 0, then you are responsible for freeing `tcp_relay_ports`
//            and also `motd` iff `enable_motd` is set
//
// returns 1 on success
//         0 on failure, doesn't modify any data pointed by arguments

int get_general_config(char **keys_file_path, int *port,
                       int *enable_ipv6,
                       int *enable_ipv4_fallback, int *enable_lan_discovery, int *enable_tcp_relay, uint16_t **tcp_relay_ports,
                       int *tcp_relay_port_count, int *enable_motd, char **motd)
{
    // Get port
    *port = DEFAULT_PORT;

    // Get keys file location
    const char *tmp_keys_file;
    tmp_keys_file = DEFAULT_KEYS_FILE_PATH;
    *keys_file_path = (char *)malloc(strlen(tmp_keys_file) + 1);
    strcpy(*keys_file_path, tmp_keys_file);

    // Get IPv6 option
    *enable_ipv6 = DEFAULT_ENABLE_IPV6;

    // Get IPv4 fallback option
    *enable_ipv4_fallback = DEFAULT_ENABLE_IPV4_FALLBACK;

    // Get LAN discovery option
    *enable_lan_discovery = DEFAULT_ENABLE_LAN_DISCOVERY;

    // Get TCP relay option
    *enable_tcp_relay = DEFAULT_ENABLE_TCP_RELAY;

    *tcp_relay_ports = (uint16_t *)malloc( 3 * sizeof(uint16_t) );
    (*tcp_relay_ports)[0] = 443;
    (*tcp_relay_ports)[1] = 3389;
    (*tcp_relay_ports)[2] = 33445;

    *tcp_relay_port_count = 3;

    // Get MOTD option
    *enable_motd = DEFAULT_ENABLE_MOTD;

    if (*enable_motd) {
        // Get MOTD
        const char *tmp_motd;

        tmp_motd = DEFAULT_MOTD;

        size_t tmp_motd_length = strlen(tmp_motd) + 1;
        size_t motd_length = tmp_motd_length > MAX_MOTD_LENGTH ? MAX_MOTD_LENGTH : tmp_motd_length;
        *motd = (char *)malloc(motd_length);
        strncpy(*motd, tmp_motd, motd_length);
        (*motd)[motd_length - 1] = '\0';
    }

    return 1;
}


// Prints public key

void print_public_key(const uint8_t *public_key)
{
    char buffer[2 * crypto_box_PUBLICKEYBYTES + 1];
    int index = 0;

    size_t i;

    for (i = 0; i < crypto_box_PUBLICKEYBYTES; i++) {
        index += sprintf(buffer + index, "%02hhX", public_key[i]);
    }

    Print(FOREGROUND_GREEN, "Public Key: %s\n", buffer);

    return;
}

int proc_toxrelay(const ts::wstrings_c & pars)
{
    Print(FOREGROUND_GREEN, "Running \"%s\" version %lu.\n", DAEMON_NAME, DAEMON_VERSION_NUMBER);

    char *keys_file_path;
    int port;
    int enable_ipv6;
    int enable_ipv4_fallback;
    int enable_lan_discovery;
    int enable_tcp_relay;
    uint16_t *tcp_relay_ports;
    int tcp_relay_port_count;
    int enable_motd;
    char *motd;

    if (get_general_config(&keys_file_path, &port, &enable_ipv6, &enable_ipv4_fallback,
                           &enable_lan_discovery, &enable_tcp_relay, &tcp_relay_ports, &tcp_relay_port_count, &enable_motd, &motd)) {
        //Print(FOREGROUND_BLUE, "General config read successfully\n");
    } else {
        //Print(FOREGROUND_RED, "Couldn't read config file: %s. Exiting.\n", cfg_file_path);
        return 1;
    }

    if (port < MIN_ALLOWED_PORT || port > MAX_ALLOWED_PORT) {
        Print(FOREGROUND_RED, "Invalid port: %d, should be in [%d, %d]. Exiting.\n", port, MIN_ALLOWED_PORT, MAX_ALLOWED_PORT);
        return 1;
    }

    IP ip;
    ip_init(&ip, enable_ipv6);

    Networking_Core *net = new_networking(nullptr, ip, port);

    if (net == NULL) {
        if (enable_ipv6 && enable_ipv4_fallback) {
            Print(FOREGROUND_BLUE, "Couldn't initialize IPv6 networking. Falling back to using IPv4.\n");
            enable_ipv6 = 0;
            ip_init(&ip, enable_ipv6);
            net = new_networking(nullptr, ip, port);

            if (net == NULL) {
                Print(FOREGROUND_BLUE, "Couldn't fallback to IPv4. Exiting.\n");
                return 1;
            }
        } else {
            Print(FOREGROUND_BLUE, "Couldn't initialize networking. Exiting.\n");
            return 1;
        }
    }


    DHT *dht = new_DHT(nullptr,net,true);

    if (dht == NULL) {
        Print(FOREGROUND_RED, "Couldn't initialize Tox DHT instance. Exiting.\n");
        return 1;
    }

    Onion *onion = new_onion(dht);
    Onion_Announce *onion_a = new_onion_announce(dht);

    if (!(onion && onion_a)) {
        Print(FOREGROUND_RED, "Couldn't initialize Tox Onion. Exiting.\n");
        return 1;
    }

    if (enable_motd) {
        if (bootstrap_set_callbacks(dht->net, DAEMON_VERSION_NUMBER, (uint8_t *)motd, (uint16_t)strlen(motd) + 1) == 0) {
            Print(FOREGROUND_BLUE, "Set MOTD successfully.\n");
        } else {
            Print(FOREGROUND_RED, "Couldn't set MOTD: %s. Exiting.\n", motd);
            return 1;
        }

        free(motd);
    }

    if (manage_keys(dht, keys_file_path)) {
        Print(FOREGROUND_BLUE, "Keys are managed successfully.\n");
    } else {
        Print(FOREGROUND_RED, "Couldn't read/write: %s. Exiting.\n", keys_file_path);
        return 1;
    }

    TCP_Server *tcp_server = NULL;

    if (enable_tcp_relay) {
        if (tcp_relay_port_count == 0) {
            Print(FOREGROUND_RED, "No TCP relay ports read. Exiting.\n");
            return 1;
        }

        tcp_server = new_TCP_server(enable_ipv6, tcp_relay_port_count, tcp_relay_ports, dht->self_secret_key, onion);

        // tcp_relay_port_count != 0 at this point
        free(tcp_relay_ports);

        if (tcp_server != NULL) {
            Print(FOREGROUND_BLUE, "Initialized Tox TCP server successfully.\n");
        } else {
            Print(FOREGROUND_RED, "Couldn't initialize Tox TCP server. Exiting.\n");
            return 1;
        }
    }

    struct dht_node_s
    {
        ts::str_c addr4;
        ts::str_c addr6;
        uint8_t pubid[32];
        int port;
        int used = 0;
        int random = 0;

        dht_node_s(const ts::asptr& addr4, const ts::asptr& addr6, int port, const ts::asptr& pubid_) :addr4(addr4), addr6(addr6), port(port)
        {
            ts::pstr_c(pubid_).hex2buf<32>(pubid);
        }
    };

    std::vector<dht_node_s> nodes;

#include "../../plugins/proto_tox/dht_nodes.inl"

    for( const dht_node_s &n : nodes )
    {
        DHT_bootstrap_from_address(dht, n.addr4.cstr(), 1, htons((uint16_t)n.port), n.pubid);
    }

    /*
    if (bootstrap_from_config(cfg_file_path, dht, enable_ipv6)) {
        Print(FOREGROUND_BLUE, "List of bootstrap nodes read successfully.\n");
    } else {
        Print(FOREGROUND_RED, "Couldn't read list of bootstrap nodes in %s. Exiting.\n", cfg_file_path);
        return 1;
    }
    */

    print_public_key(dht->self_public_key);

    free(keys_file_path);

    uint64_t last_LANdiscovery = 0;
    const uint16_t htons_port = htons(port);

    int waiting_for_dht_connection = 1;

    if (enable_lan_discovery) {
        LANdiscovery_init(dht);
        Print(FOREGROUND_BLUE, "Initialized LAN discovery.\n");
    }

    while (1) {
        do_DHT(dht);

        if (enable_lan_discovery && is_timeout(last_LANdiscovery, LAN_DISCOVERY_INTERVAL)) {
            send_LANdiscovery(htons_port, dht);
            last_LANdiscovery = unix_time();
        }

        if (enable_tcp_relay) {
            do_TCP_server(tcp_server);
        }

        networking_poll(dht->net, nullptr);

        if (waiting_for_dht_connection && DHT_isconnected(dht)) {
            Print(FOREGROUND_BLUE, "Connected to other bootstrap node successfully.\n");
            waiting_for_dht_connection = 0;
        }

        ts::sys_sleep(1);
    }

    return 1;
}
