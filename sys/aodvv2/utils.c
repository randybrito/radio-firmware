/*
 * Copyright (C) 2014 Freie Universität Berlin
 * Copyright (C) 2014 Lotte Steenbrink <lotte.steenbrink@fu-berlin.de>
 * Copyright (C) 2020 Locha Inc
 *
 * This file is subject to the terms and conditions of the GNU Lesser
 * General Public License v2.1. See the file LICENSE in the top level
 * directory for more details.
 */

/**
 * @ingroup     aodvv2
 * @{
 *
 * @file
 * @brief       AODVv2 routing protocol
 *
 * @author      Lotte Steenbrink <lotte.steenbrink@fu-berlin.de>
 * @author      Gustavo Grisales <gustavosinbandera1@hotmail.com>
 * @author      Jean Pierre Dudey <jeandudey@hotmail.com>
 * @}
 */

#include <string.h>

#include "utils.h"

#define ENABLE_DEBUG (1)
#include "debug.h"

#include "constants.h"
#include "seqnum.h"

/* Some aodvv2 utilities (mostly tables) */
static mutex_t rreqt_mutex;

static aodvv2_rreq_entry_t *_get_comparable_rreq(aodvv2_packet_data_t *packet_data);
static void _add_rreq(aodvv2_packet_data_t *packet_data);
static void _reset_entry_if_stale(uint8_t i);

static aodvv2_rreq_entry_t rreq_table[AODVV2_RREQ_BUF];

static struct netaddr_str nbuf;
static timex_t null_time, now, _max_idletime;


void ipv6_addr_to_netaddr(ipv6_addr_t *src, struct netaddr *dst)
{
    dst->_type = AF_INET6;
    dst->_prefix_len = AODVV2_RIOT_PREFIXLEN;
    memcpy(dst->_addr, src, sizeof(dst->_addr));
}

void netaddr_to_ipv6_addr(struct netaddr *src, ipv6_addr_t *dst)
{
    memcpy(dst, src->_addr, sizeof(uint8_t) * NETADDR_MAX_LENGTH);
}

void aodvv2_rreqtable_init(void)
{
    mutex_lock(&rreqt_mutex);
    null_time = timex_set(0, 0);
    _max_idletime = timex_set(AODVV2_MAX_IDLETIME, 0);

    memset(&rreq_table, 0, sizeof(rreq_table));
    mutex_unlock(&rreqt_mutex);
    DEBUG("RREQ table initialized.\n");
}

bool aodvv2_rreqtable_is_redundant(aodvv2_packet_data_t *packet_data)
{
    aodvv2_rreq_entry_t *comparable_rreq;
    timex_t now;
    bool result;

    mutex_lock(&rreqt_mutex);
    comparable_rreq = _get_comparable_rreq(packet_data);

    /* if there is no comparable rreq stored, add one and return false */
    if (comparable_rreq == NULL) {
        _add_rreq(packet_data);
        result = false;
    }
    else {
        int seqnum_comparison = aodvv2_seqnum_cmp(packet_data->orig_node.seqnum,
                                                  comparable_rreq->seqnum);

        /*
         * If two RREQs have the same
         * metric type and OrigNode and Targnode addresses, the information from
         * the one with the older Sequence Number is not needed in the table
         */
        if (seqnum_comparison == -1) {
            result = true;
        }

        if (seqnum_comparison == 1) {
            /* Update RREQ table entry with new seqnum value */
            comparable_rreq->seqnum = packet_data->orig_node.seqnum;
        }

        /*
         * in case they have the same Sequence Number, the one with the greater
         * Metric value is not needed
         */
        if (seqnum_comparison == 0) {
            if (comparable_rreq->metric <= packet_data->orig_node.metric) {
                result = true;
            }
            /* Update RREQ table entry with new metric value */
            comparable_rreq->metric = packet_data->orig_node.metric;
        }

        /* Since we've changed RREQ info, update the timestamp */
        xtimer_now_timex(&now);
        comparable_rreq->timestamp = now;
        result = true;
    }

    mutex_unlock(&rreqt_mutex);
    return result;
}

/*
 * retrieve pointer to a comparable (according to Section 6.7.)
 * RREQ table entry if it exists and NULL otherwise.
 * Two AODVv2 RREQ messages are comparable if:
 * - they have the same metric type
 * - they have the same OrigNode and TargNode addresses
 */
static aodvv2_rreq_entry_t *_get_comparable_rreq(aodvv2_packet_data_t *packet_data)
{
    for (unsigned i = 0; i < AODVV2_RREQ_BUF; i++) {
        _reset_entry_if_stale(i);

        if (!netaddr_cmp(&rreq_table[i].origNode, &packet_data->orig_node.addr) &&
            !netaddr_cmp(&rreq_table[i].targNode, &packet_data->targ_node.addr) &&
            rreq_table[i].metricType == packet_data->metric_type) {
            return &rreq_table[i];
        }
    }

    return NULL;
}


static void _add_rreq(aodvv2_packet_data_t *packet_data)
{
    if (_get_comparable_rreq(packet_data)) {
        return;
    }
    /*find empty rreq and fill it with packet_data */

    for (unsigned i = 0; i < AODVV2_RREQ_BUF; i++) {
        if (!rreq_table[i].timestamp.seconds &&
            !rreq_table[i].timestamp.microseconds) {
            rreq_table[i].origNode = packet_data->orig_node.addr;
            rreq_table[i].targNode = packet_data->targ_node.addr;
            rreq_table[i].metricType = packet_data->metric_type;
            rreq_table[i].metric = packet_data->orig_node.metric;
            rreq_table[i].seqnum = packet_data->orig_node.seqnum;
            rreq_table[i].timestamp = packet_data->timestamp;
            return;
        }
    }
}

/*
 * Check if entry at index i is stale and clear the struct it fills if it is
 */
static void _reset_entry_if_stale(uint8_t i)
{
    xtimer_now_timex(&now);

    if (timex_cmp(rreq_table[i].timestamp, null_time) == 0) {
        return;
    }
    timex_t expiration_time = timex_add(rreq_table[i].timestamp, _max_idletime);
    if (timex_cmp(expiration_time, now) < 0) {
        /* timestamp+expiration time is in the past: this entry is stale */
        DEBUG("\treset rreq table entry %s\n",
              netaddr_to_string(&nbuf, &rreq_table[i].origNode));
        memset(&rreq_table[i], 0, sizeof(rreq_table[i]));
    }
}
