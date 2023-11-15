/*
 *  Copyright (c) 2019-2022, Peter Haag
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *   * Redistributions of source code must retain the above copyright notice,
 *     this list of conditions and the following disclaimer.
 *   * Redistributions in binary form must reproduce the above copyright notice,
 *     this list of conditions and the following disclaimer in the documentation
 *     and/or other materials provided with the distribution.
 *   * Neither the name of the author nor the names of its contributors may be
 *     used to endorse or promote products derived from this software without
 *     specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include "output_csv.h"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>

#include "config.h"
#include "nfdump.h"
#include "nffile.h"
#include "output_csv.h"
#include "output_util.h"
#include "util.h"

#define IP_STRING_LEN (INET6_ADDRSTRLEN)

// record counter
static uint32_t recordCount;

void csv_prolog(void) {
    recordCount = 0;
    /*
    printf(
        "ts,te,td,sa,da,sp,dp,pr,flg,fwd,stos,ipkt,ibyt,opkt,obyt,in,out,sas,das,smk,dmk,dtos,dir,nh,nhb,svln,dvln,ismc,odmc,idmc,osmc,mpls1,mpls2,"
        "mpls3,mpls4,mpls5,mpls6,mpls7,mpls8,mpls9,mpls10,cl,sl,al,ra,eng,exid,tr\n");
    */
    printf("sa,da,ibyt,sas,das,ra,tr\n");

}  // End of csv_prolog

void csv_epilog(void) {}  // End of csv_epilog

void csv_record(FILE *stream, void *record, int tag) {
    char as[IP_STRING_LEN], ds[IP_STRING_LEN];
    char datestr1[64], datestr2[64], datestr3[64];
    char s_snet[IP_STRING_LEN], s_dnet[IP_STRING_LEN];
    time_t when;
    struct tm *ts;
    master_record_t *r = (master_record_t *)record;

    // if this flow is a tunnel, add a flow line with the tunnel IPs
    if (r->tun_ip_version) {
        master_record_t _r = {0};
        _r.proto = r->tun_proto;
        memcpy((void *)_r.tun_src_ip.V6, r->tun_src_ip.V6, 16);
        memcpy((void *)_r.tun_dst_ip.V6, r->tun_dst_ip.V6, 16);
        _r.msecFirst = r->msecFirst;
        _r.msecLast = r->msecLast;
        if (r->tun_ip_version == 6) _r.mflags = V3_FLAG_IPV6_ADDR;
        csv_record(stream, (void *)&_r, tag);
    }

    as[0] = 0;
    ds[0] = 0;
    if (TestFlag(r->mflags, V3_FLAG_IPV6_ADDR) != 0) {
        uint64_t snet[2];
        uint64_t dnet[2];

        snet[0] = htonll(r->V6.srcaddr[0]);
        snet[1] = htonll(r->V6.srcaddr[1]);
        dnet[0] = htonll(r->V6.dstaddr[0]);
        dnet[1] = htonll(r->V6.dstaddr[1]);
        inet_ntop(AF_INET6, snet, as, sizeof(as));
        inet_ntop(AF_INET6, dnet, ds, sizeof(ds));

        inet6_ntop_mask(r->V6.srcaddr, r->src_mask, s_snet, sizeof(s_snet));
        inet6_ntop_mask(r->V6.dstaddr, r->dst_mask, s_dnet, sizeof(s_dnet));

    } else {  // IPv4
        uint32_t snet, dnet;
        snet = htonl(r->V4.srcaddr);
        dnet = htonl(r->V4.dstaddr);
        inet_ntop(AF_INET, &snet, as, sizeof(as));
        inet_ntop(AF_INET, &dnet, ds, sizeof(ds));

        inet_ntop_mask(r->V4.srcaddr, r->src_mask, s_snet, sizeof(s_snet));
        inet_ntop_mask(r->V4.dstaddr, r->dst_mask, s_dnet, sizeof(s_dnet));
    }
    as[IP_STRING_LEN - 1] = 0;
    ds[IP_STRING_LEN - 1] = 0;

    when = r->msecFirst / 1000LL;
    ts = localtime(&when);
    strftime(datestr1, 63, "%Y-%m-%d %H:%M:%S", ts);

    when = r->msecLast / 1000LL;
    ts = localtime(&when);
    strftime(datestr2, 63, "%Y-%m-%d %H:%M:%S", ts);

    double duration = (double)(r->msecLast - r->msecFirst) / 1000.0;

    /*
    fprintf(stream, "%s,%s,%.3f,%s,%s,%u,%u,%s,%s,%u,%u,%llu,%llu,%llu,%llu", datestr1, datestr2, duration, as, ds, r->srcPort, r->dstPort,
            ProtoString(r->proto, 0), FlagsString(r->tcp_flags), r->fwd_status, r->tos, (unsigned long long)r->inPackets,
            (unsigned long long)r->inBytes, (long long unsigned)r->out_pkts, (long long unsigned)r->out_bytes);
    */
    fprintf(stream, "%s,%s,%llu", as, ds, (unsigned long long)r->inBytes, (long long unsigned)r->out_pkts, (long long unsigned)r->out_bytes);

    /*
    // EX_IO_SNMP_2:
    // EX_IO_SNMP_4:
    fprintf(stream, ",%u,%u", r->input, r->output);
    */

    // EX_AS_2:
    // EX_AS_4:
    fprintf(stream, ",%u,%u", r->srcas, r->dstas);

    /*
    // EX_MULIPLE:
    fprintf(stream, ",%u,%u,%u,%u", r->src_mask, r->dst_mask, r->dst_tos, r->dir);
    if (TestFlag(r->mflags, V3_FLAG_IPV6_NH) != 0) {  // IPv6
        // EX_NEXT_HOP_v6:
        as[0] = 0;
        r->ip_nexthop.V6[0] = htonll(r->ip_nexthop.V6[0]);
        r->ip_nexthop.V6[1] = htonll(r->ip_nexthop.V6[1]);
        inet_ntop(AF_INET6, r->ip_nexthop.V6, as, sizeof(as));
        as[IP_STRING_LEN - 1] = 0;
        fprintf(stream, ",%s", as);
    } else {
        // EX_NEXT_HOP_v4:
        as[0] = 0;
        r->ip_nexthop.V4 = htonl(r->ip_nexthop.V4);
        inet_ntop(AF_INET, &r->ip_nexthop.V4, as, sizeof(as));
        as[IP_STRING_LEN - 1] = 0;
        fprintf(stream, ",%s", as);
    }

    if (TestFlag(r->mflags, V3_FLAG_IPV6_NHB) != 0) {  // IPv6
        // EX_NEXT_HOP_BGP_v6:
        as[0] = 0;
        r->bgp_nexthop.V6[0] = htonll(r->bgp_nexthop.V6[0]);
        r->bgp_nexthop.V6[1] = htonll(r->bgp_nexthop.V6[1]);
        inet_ntop(AF_INET6, r->ip_nexthop.V6, as, sizeof(as));
        as[IP_STRING_LEN - 1] = 0;
        fprintf(stream, ",%s", as);
    } else {
        // 	EX_NEXT_HOP_BGP_v4:
        as[0] = 0;
        r->bgp_nexthop.V4 = htonl(r->bgp_nexthop.V4);
        inet_ntop(AF_INET, &r->bgp_nexthop.V4, as, sizeof(as));
        as[IP_STRING_LEN - 1] = 0;
        fprintf(stream, ",%s", as);
    }

    // EX_VLAN:
    fprintf(stream, ",%u,%u", r->src_vlan, r->dst_vlan);

    // case EX_MAC_1:

    uint8_t mac1[6], mac2[6];
    for (int i = 0; i < 6; i++) {
        mac1[i] = (r->in_src_mac >> (i * 8)) & 0xFF;
    }
    for (int i = 0; i < 6; i++) {
        mac2[i] = (r->out_dst_mac >> (i * 8)) & 0xFF;
    }

    fprintf(stream, ",%.2x:%.2x:%.2x:%.2x:%.2x:%.2x,%.2x:%.2x:%.2x:%.2x:%.2x:%.2x", mac1[5], mac1[4], mac1[3], mac1[2], mac1[1], mac1[0], mac2[5],
            mac2[4], mac2[3], mac2[2], mac2[1], mac2[0]);

    // EX_MAC_2:
    for (int i = 0; i < 6; i++) {
        mac1[i] = (r->in_dst_mac >> (i * 8)) & 0xFF;
    }
    for (int i = 0; i < 6; i++) {
        mac2[i] = (r->out_src_mac >> (i * 8)) & 0xFF;
    }

    fprintf(stream, ",%.2x:%.2x:%.2x:%.2x:%.2x:%.2x,%.2x:%.2x:%.2x:%.2x:%.2x:%.2x", mac1[5], mac1[4], mac1[3], mac1[2], mac1[1], mac1[0], mac2[5],
            mac2[4], mac2[3], mac2[2], mac2[1], mac2[0]);

    // EX_MPLS:
    for (int i = 0; i < 10; i++) {
        fprintf(stream, ",%u-%1u-%1u", r->mpls_label[i] >> 4, (r->mpls_label[i] & 0xF) >> 1, r->mpls_label[i] & 1);
    }

    double f1, f2, f3;
    f1 = (double)r->client_nw_delay_usec / 1000.0;
    f2 = (double)r->server_nw_delay_usec / 1000.0;
    f3 = (double)r->appl_latency_usec / 1000.0;

    fprintf(stream, ",%9.3f,%9.3f,%9.3f", f1, f2, f3);
    */

    // EX_ROUTER_IP_v4:
    if (TestFlag(r->mflags, V3_FLAG_IPV6_EXP) != 0) {
        // EX_NEXT_HOP_v6:
        as[0] = 0;
        r->ip_router.V6[0] = htonll(r->ip_router.V6[0]);
        r->ip_router.V6[1] = htonll(r->ip_router.V6[1]);
        inet_ntop(AF_INET6, r->ip_router.V6, as, sizeof(as));
        as[IP_STRING_LEN - 1] = 0;
        fprintf(stream, ",%s", as);
    } else {
        // EX_NEXT_HOP_v4:
        as[0] = 0;
        r->ip_router.V4 = htonl(r->ip_router.V4);
        inet_ntop(AF_INET, &r->ip_router.V4, as, sizeof(as));
        as[IP_STRING_LEN - 1] = 0;
        fprintf(stream, ",%s", as);
    }

    /*
    // EX_ROUTER_ID
    fprintf(stream, ",%u/%u", r->engine_type, r->engine_id);

    // Exporter SysID
    fprintf(stream, ",%u", r->exporter_sysid);
    */

    // Date flow received
    when = r->msecReceived / 1000LL;
    ts = localtime(&when);
    strftime(datestr3, 63, ",%Y-%m-%d %H:%M:%S", ts);

    fprintf(stream, "%s.%03llu\n", datestr3, (long long unsigned)r->msecReceived % 1000LL);

}  // End of csv_record
