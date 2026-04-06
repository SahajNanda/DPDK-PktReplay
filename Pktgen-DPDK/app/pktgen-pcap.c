/*-
 * Copyright(c) <2010-2026>, Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/* Created 2010 by Keith Wiles @ intel.com */

#include <lua_config.h>
#include <errno.h>

#include "pktgen-display.h"
#include "pktgen-log.h"

#include "pktgen.h"

#ifndef MBUF_INVALID_PORT
#define MBUF_INVALID_PORT UINT16_MAX
#endif

static pcap_info_t *pcap_info_list[RTE_MAX_ETHPORTS];

void
pktgen_pcap_info(pcap_info_t *pcap, uint16_t port, int flag)
{
    printf("PCAP file for port %d: %s\n", port, pcap->filename);
    printf("  magic: %08x,", pcap->info.magic_number);
    printf(" Version: %d.%d,", pcap->info.version_major, pcap->info.version_minor);
    printf(" Zone: %d,", pcap->info.thiszone);
    printf(" snaplen: %d,", pcap->info.snaplen);
    printf(" sigfigs: %d,", pcap->info.sigfigs);
    printf(" network: %d", pcap->info.network);
    printf(" Convert Endian: %s\n", pcap->convert ? "Yes" : "No");
    if (flag)
        printf("  Packet count: %d, max size %d\n", pcap->pkt_count, pcap->max_pkt_size);
    fflush(stdout);
}

static __inline__ void
pcap_convert(pcap_info_t *pcap, pcap_record_hdr_t *pHdr)
{
    if (pcap->convert) {
        pHdr->incl_len = ntohl(pHdr->incl_len);
        pHdr->orig_len = ntohl(pHdr->orig_len);
        pHdr->ts_sec   = ntohl(pHdr->ts_sec);
        pHdr->ts_usec  = ntohl(pHdr->ts_usec);
    }
}

static void
pcap_rewind(pcap_info_t *pcap)
{
    /* Rewind to the beginning */
    rewind(pcap->fp);

    /* Seek past the pcap header */
    (void)fseek(pcap->fp, sizeof(pcap_hdr_t), SEEK_SET);
}

static int
pcap_skip_packets(pcap_info_t *pcap, uint32_t start_pkt)
{
    pcap_record_hdr_t hdr;

    for (uint32_t i = 0; i < start_pkt; i++) {
        if (fread(&hdr, 1, sizeof(hdr), pcap->fp) != sizeof(hdr))
            return -1;

        pcap_convert(pcap, &hdr);
        if (fseek(pcap->fp, hdr.incl_len, SEEK_CUR) < 0)
            return -1;
    }

    return 0;
}

static void
pcap_get_info(pcap_info_t *pcap)
{
    pcap_record_hdr_t hdr;

    if (fread(&pcap->info, 1, sizeof(pcap_hdr_t), pcap->fp) != sizeof(pcap_hdr_t))
        rte_exit(EXIT_FAILURE, "%s: failed to read pcap header\n", __func__);

    /* Make sure we have a valid PCAP file for Big or Little Endian formats. */
    if (pcap->info.magic_number == PCAP_MAGIC_NUMBER)
        pcap->convert = 0;
    else if (pcap->info.magic_number == ntohl(PCAP_MAGIC_NUMBER))
        pcap->convert = 1;
    else
        rte_exit(EXIT_FAILURE, "%s: invalid magic number 0x%08x\n", __func__,
                 pcap->info.magic_number);

    if (pcap->convert) {
        pcap->info.magic_number  = ntohl(pcap->info.magic_number);
        pcap->info.version_major = ntohs(pcap->info.version_major);
        pcap->info.version_minor = ntohs(pcap->info.version_minor);
        pcap->info.thiszone      = ntohl(pcap->info.thiszone);
        pcap->info.sigfigs       = ntohl(pcap->info.sigfigs);
        pcap->info.snaplen       = ntohl(pcap->info.snaplen);
        pcap->info.network       = ntohl(pcap->info.network);
    }

    pcap->max_pkt_size  = 0;
    pcap->avg_pkt_size  = 0;
    uint64_t total_size = 0;
    /* count the number of packets and get the largest size packet */
    for (;;) {
        if (fread(&hdr, 1, sizeof(pcap_record_hdr_t), pcap->fp) != sizeof(hdr))
            break;

        /* Convert the packet header to the correct format if needed */
        pcap_convert(pcap, &hdr);

        if (fseek(pcap->fp, hdr.incl_len, SEEK_CUR) < 0)
            break;

        pcap->pkt_count++;
        if (hdr.incl_len > pcap->max_pkt_size)
            pcap->max_pkt_size = hdr.incl_len;

        total_size += hdr.incl_len;
    }
    printf("PCAP: Max Packet Size: %d\n", pcap->max_pkt_size);

    pcap->avg_pkt_size = total_size / pcap->pkt_count;

    printf("PCAP: Avg Packet Size: %d\n", pcap->avg_pkt_size);

    pcap_rewind(pcap);
}

static __inline__ void
mbuf_iterate_cb(struct rte_mempool *mp, void *opaque, void *obj, unsigned obj_idx __rte_unused)
{
    pcap_info_t *pcap     = (pcap_info_t *)opaque;
    struct rte_mbuf *m    = (struct rte_mbuf *)obj;
    pcap_record_hdr_t hdr = {0};

    if (fread(&hdr, 1, sizeof(pcap_record_hdr_t), pcap->fp) != sizeof(hdr)) {
        pcap_rewind(pcap);
        if (fread(&hdr, 1, sizeof(pcap_record_hdr_t), pcap->fp) != sizeof(hdr))
            rte_exit(EXIT_FAILURE, "%s: failed to read pcap header\n", __func__);
    }

    pcap_convert(pcap, &hdr); /* Convert the packet header to the correct format. */

    if (fread(rte_pktmbuf_mtod(m, char *), 1, hdr.incl_len, pcap->fp) == 0)
        rte_exit(EXIT_FAILURE, "%s: failed to read packet data from PCAP file\n", __func__);

    m->pool     = mp;
    m->next     = NULL;
    m->data_len = hdr.incl_len;
    m->pkt_len  = hdr.incl_len;
    m->port     = 0;
    m->ol_flags = 0;
}

/*
 * Create a mempool for the given parameters.
 * First tries to allocate the requested count directly.
 * If that fails, performs up to 8 binary-search attempts between requested and min.
 * If binary search also fails, falls back to minimum count.
 */
static struct rte_mempool *
pcap_create_best_effort_pool(const char *name, uint16_t pid, uint16_t sid, uint32_t dataroom,
                             uint32_t requested_count, uint32_t min_count,
                             uint32_t *loaded_count)
{
    struct rte_mempool *mp = NULL;
    uint32_t low, high, best;
    uint32_t attempt = 0;
    const uint32_t max_attempts = 8;

    if (loaded_count)
        *loaded_count = 0;

    if (requested_count < min_count)
        requested_count = min_count;

    /* Try to allocate the full requested count first */
    mp = rte_pktmbuf_pool_create(name, requested_count, 0, DEFAULT_PRIV_SIZE, dataroom, sid);
    if (mp != NULL) {
        if (loaded_count)
            *loaded_count = requested_count;
        return mp;
    }

    /* Full request failed, start binary search between requested and min */
    low  = min_count;
    high = requested_count;
    best = 0;
    attempt = 0;

    while (low <= high && attempt < max_attempts) {
        uint32_t count = low + ((high - low) / 2);
        char try_name[64] = {0};

        snprintf(try_name, sizeof(try_name), "pcap-bin-%u-%u-%u", pid, attempt, count);
        mp = rte_pktmbuf_pool_create(try_name, count, 0, DEFAULT_PRIV_SIZE, dataroom, sid);

        if (mp != NULL) {
            rte_mempool_free(mp);
            mp   = NULL;
            best = count;
            low  = count + 1;
        } else {
            if (count == 0)
                break;
            high = count - 1;
        }

        attempt++;
    }

    /* Allocate the best successful size found by binary search. */
    if (best > 0) {
        mp = rte_pktmbuf_pool_create(name, best, 0, DEFAULT_PRIV_SIZE, dataroom, sid);
        if (mp != NULL) {
            if (loaded_count)
                *loaded_count = best;
            return mp;
        }
    }

    /* Fallback: try to allocate minimum count as last resort. */
    if (best != min_count) {
        mp = rte_pktmbuf_pool_create(name, min_count, 0, DEFAULT_PRIV_SIZE, dataroom, sid);
        if (mp != NULL) {
            if (loaded_count)
                *loaded_count = min_count;
            return mp;
        }
    }

    pktgen_log_warning("PCAP port %u mbuf pool allocation failed down to minimum %u", pid,
                       min_count);
    if (loaded_count)
        *loaded_count = 0;
    return NULL;
}

int
pktgen_pcap_add(char *filename, uint16_t pid)
{
    pcap_info_t *pcap = NULL;
    char name[64]     = {0};
    uint16_t sid;

    if (filename == NULL)
        rte_exit(EXIT_FAILURE, "%s: PCAP filename is NULL\n", __func__);

    sid = pg_eth_dev_socket_id(pid);

    snprintf(name, sizeof(name), "PCAP-Info-%d", pid);
    pcap = (pcap_info_t *)rte_zmalloc_socket(name, sizeof(pcap_info_t), RTE_CACHE_LINE_SIZE, sid);
    if (pcap == NULL)
        rte_exit(EXIT_FAILURE, "%s: rte_zmalloc_socket() failed for pcap_info_t structure\n",
                 __func__);

    /* Default to little endian format. */
    pcap->filename = strdup(filename);

    pcap_info_list[pid] = pcap;

    return 0;
}

/**
 * Open a single port's PCAP file and load packets into mempool.
 *
 * @param pid Port ID to open PCAP for.
 * @param start_pkt Packet index to start from.
 * @return 0 on success, negative on error.
 */
static int
pktgen_pcap_open_port(uint16_t pid, uint32_t start_pkt, uint32_t add_pkt_count)
{
    pcap_info_t *pcap = NULL;
    struct rte_mempool *mp;
    char name[64] = {0};
    uint16_t sid;
    uint32_t requested_count;
    uint32_t loaded_count;
    uint32_t file_pkt_count;
    uint32_t min_count;
    uint32_t dataroom;

    if ((pcap = pcap_info_list[pid]) == NULL)
        return -ENOENT;

    sid = pg_eth_dev_socket_id(pid);

    pcap->fp = fopen((const char *)pcap->filename, "r");
    if (pcap->fp == NULL) {
        pktgen_log_error("%s: failed to open file (%s)", __func__, pcap->filename);
        return -ENOENT;
    }

    pcap->pkt_count = 0;
    pcap_get_info(pcap);

    file_pkt_count  = pcap->pkt_count;
    requested_count = file_pkt_count;
    loaded_count    = 0;
    min_count       = (DEFAULT_TX_DESC * 4);

    if (requested_count == 0) {
        fclose(pcap->fp);
        pcap->fp = NULL;
        pktgen_log_error("%s: PCAP file is empty: %s", __func__, pcap->filename);
        return -ENODATA;
    }

    if (start_pkt >= file_pkt_count) {
        start_pkt %= file_pkt_count;
        pktgen_log_warning("PCAP start packet adjusted to %u on port %u", start_pkt, pid);
    }

    pcap_rewind(pcap);
    if (start_pkt > 0 && pcap_skip_packets(pcap, start_pkt) < 0) {
        fclose(pcap->fp);
        pcap->fp = NULL;
        pktgen_log_error("%s: failed to seek to packet %u in %s", __func__, start_pkt,
                         pcap->filename);
        return -EINVAL;
    }

    pcap->pkt_index = start_pkt;

    snprintf(name, sizeof(name), "pcap-%d", pid);
    dataroom = RTE_ALIGN_CEIL(pcap->max_pkt_size + RTE_PKTMBUF_HEADROOM, RTE_CACHE_LINE_SIZE);

    if (add_pkt_count > 0) {
        /* Explicit mode: caller provides exact target packet count. */
        requested_count = add_pkt_count;
        mp = rte_pktmbuf_pool_create(name, requested_count, 0, DEFAULT_PRIV_SIZE, dataroom, sid);
        if (mp != NULL)
            loaded_count = requested_count;
    } else {
        mp = pcap_create_best_effort_pool(name, pid, sid, dataroom, requested_count, min_count,
                                          &loaded_count);
    }

    if (mp == NULL) {
        fclose(pcap->fp);
        pcap->fp = NULL;
        if (add_pkt_count > 0)
            pktgen_log_error("Cannot create mbuf pool (%s) port %d, exact request %u, socket %d: %s",
                             name, pid, requested_count, sid, rte_strerror(rte_errno));
        else
            pktgen_log_error(
                "Cannot create mbuf pool (%s) port %d, requested up to %u, min %u, socket %d: %s",
                name, pid, requested_count, min_count, sid, rte_strerror(rte_errno));
        return -rte_errno;
    }

    if ((add_pkt_count == 0) && (loaded_count < file_pkt_count))
        pktgen_log_warning("PCAP port %d limited by memory: requested %u packets, loaded %u", pid,
                           file_pkt_count, loaded_count);

    pcap->pkt_count = loaded_count;
    pcap->mp        = mp;

    rte_mempool_obj_iter(mp, mbuf_iterate_cb, pcap);

    if (l2p_set_pcap_info(pid, pcap) < 0) {
        rte_mempool_free(mp);
        pcap->mp = NULL;
        fclose(pcap->fp);
        pcap->fp = NULL;
        pktgen_log_error("Error opening PCAP file: %s", pcap->filename);
        return -1;
    }

    return 0;
}

int
pktgen_pcap_open(void)
{
    int ret;

    for (int pid = 0; pid < RTE_MAX_ETHPORTS; pid++) {
        if (pcap_info_list[pid] == NULL)
            continue;

        ret = pktgen_pcap_open_port(pid, 0, 0);
        if (ret < 0)
            rte_exit(EXIT_FAILURE, "Failed to open PCAP on port %d\n", pid);
    }
    return 0;
}

int
pktgen_pcap_reload(uint16_t pid, const char *filename)
{
    return pktgen_pcap_reload_with_opts(pid, filename, 0, 0);
}

int
pktgen_pcap_reload_from(uint16_t pid, const char *filename, uint32_t start_pkt)
{
    return pktgen_pcap_reload_with_opts(pid, filename, start_pkt, 0);
}

int
pktgen_pcap_reload_with_opts(uint16_t pid, const char *filename, uint32_t start_pkt,
                             uint32_t add_pkt_count)
{
    pcap_info_t *pcap;
    port_info_t *pinfo;
    int ret;

    if (pid >= RTE_MAX_ETHPORTS) {
        pktgen_log_error("Invalid port ID %u", pid);
        return -EINVAL;
    }

    pcap = pcap_info_list[pid];
    if (pcap == NULL) {
        pktgen_log_error("No PCAP loaded on port %u", pid);
        return -ENOENT;
    }

    if (filename == NULL) {
        pktgen_log_error("Filename is NULL");
        return -EINVAL;
    }

    pinfo = l2p_get_port_pinfo(pid);
    if (pinfo && pktgen_tst_port_flags(pinfo, SENDING_PACKETS)) {
        pktgen_log_error("Cannot reload PCAP on port %u while transmitting", pid);
        return -EBUSY;
    }

    if (pcap->fp) {
        fclose(pcap->fp);
        pcap->fp = NULL;
    }
    if (pcap->mp) {
        rte_mempool_free(pcap->mp);
        pcap->mp = NULL;
    }
    if (pcap->filename)
        free(pcap->filename);

    pcap->filename = strdup(filename);
    if (pcap->filename == NULL) {
        pktgen_log_error("Failed to allocate memory for filename");
        return -ENOMEM;
    }

    pcap->pkt_index = 0;
    pcap->pkt_count = 0;

    ret = pktgen_pcap_open_port(pid, start_pkt, add_pkt_count);
    if (ret < 0) {
        pktgen_log_error("Failed to open new PCAP file on port %u: %s", pid, filename);
        return ret;
    }

    if (add_pkt_count > 0)
        pktgen_log_info("PCAP reloaded on port %u: %s (start packet %u, target packets %u)", pid,
                        filename, start_pkt, add_pkt_count);
    else
        pktgen_log_info("PCAP reloaded on port %u: %s (start packet %u)", pid, filename,
                        start_pkt);
    return 0;
}

void
pktgen_pcap_close(void)
{
    pcap_info_t *pcap = NULL;

    for (int pid = 0; pid < RTE_MAX_ETHPORTS; pid++) {
        pcap = pcap_info_list[pid];
        if (pcap == NULL)
            return;

        if (pcap->filename)
            free(pcap->filename);
        if (pcap->fp)
            fclose(pcap->fp);
        if (pcap->mp)
            rte_mempool_free(pcap->mp);
        rte_free(pcap);
    }
}

FILE *
pktgen_create_pcap_file(char *filename)
{
    struct pcap_file_header file_header;
    file_header.magic         = 0xa1b2c3d4;
    file_header.version_major = 2;
    file_header.version_minor = 4;
    file_header.thiszone      = 0;
    file_header.sigfigs       = 0;
    file_header.snaplen       = 65535;
    file_header.linktype      = 1;        // LINKTYPE_ETHERNET

    printf("Creating PCAP file: %s, %lu, %lu\n", filename, sizeof(struct pcap_file_header),
           sizeof(struct pcap_pkthdr));

    // Open the output file
    FILE *file = fopen(filename, "wb");
    if (!file)
        return NULL;

    // Write the file header
    fwrite(&file_header, sizeof(uint8_t), sizeof(file_header), file);
    fflush(file);

    return file;
}

void
pktgen_close_pcap_file(FILE *fp)
{
    if (fp)
        fclose(fp);
}

int
pktgen_write_mbuf_to_pcap_file(FILE *fp, struct rte_mbuf *mbuf)
{
    // Packet header
    pcap_record_hdr_t packet_header;
    size_t size;

    if (fp == NULL)
        return 0;

    packet_header.ts_sec   = 0;
    packet_header.ts_usec  = 0;
    packet_header.incl_len = rte_pktmbuf_pkt_len(mbuf);
    packet_header.orig_len = rte_pktmbuf_pkt_len(mbuf);

    // Write the packet header
    if ((size = fwrite(&packet_header, sizeof(uint8_t), sizeof(packet_header), fp)) != 16)
        printf("Error writing packet header %ld\n", size);

    // Write the packet data
    fwrite(rte_pktmbuf_mtod(mbuf, char *), sizeof(uint8_t), rte_pktmbuf_pkt_len(mbuf), fp);
    fflush(fp);

    return 0;
}
