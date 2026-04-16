/*-
 * Copyright(c) <2010-2026>, Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */
/* Created 2010 by Keith Wiles @ intel.com */

#include <lua_config.h>

#include "pktgen-display.h"
#include "pktgen-log.h"

#include "pktgen.h"

#ifndef MBUF_INVALID_PORT
#define MBUF_INVALID_PORT UINT16_MAX
#endif

static pcap_info_t *pcap_info_list[RTE_MAX_ETHPORTS];

#define DEFAULT_PKTGEN_BASELINE_HUGEPAGES 100

static int
get_total_hugepage_bytes(uint64_t *bytes, uint64_t *hugepage_size_bytes)
{
    FILE *fp;
    char line[256];
    uint64_t hugepages_total = 0;
    uint64_t hugepage_size_kb = 0;

    if (bytes == NULL)
        return -1;

    fp = fopen("/proc/meminfo", "r");
    if (fp == NULL)
        return -1;

    while (fgets(line, sizeof(line), fp) != NULL) {
        if (sscanf(line, "HugePages_Total: %lu", &hugepages_total) == 1)
            continue;
        if (sscanf(line, "Hugepagesize: %lu kB", &hugepage_size_kb) == 1)
            continue;
    }

    fclose(fp);

    if (hugepages_total == 0 || hugepage_size_kb == 0)
        return -1;

    *bytes = hugepages_total * hugepage_size_kb * 1024ULL;
    if (hugepage_size_bytes != NULL)
        *hugepage_size_bytes = hugepage_size_kb * 1024ULL;

    pktgen_log_info("HugePages_Total bytes calculated: %lu", (unsigned long)*bytes); // remove

    return 0;
}

static uint64_t
pcap_split_section_budgets(uint64_t total_hugepage_bytes, uint64_t reserve_hugepage_bytes,
                           uint64_t *section0_bytes, uint64_t *section1_bytes)
{
    uint64_t usable_hugepage_bytes = 0;

    if (total_hugepage_bytes > reserve_hugepage_bytes)
        usable_hugepage_bytes = total_hugepage_bytes - reserve_hugepage_bytes;

    *section0_bytes = usable_hugepage_bytes / 2;
    *section1_bytes = usable_hugepage_bytes - *section0_bytes;

    return usable_hugepage_bytes;
} // splite hugepage budget across two sections

static __inline__ void
pcap_section_reset(pcap_section_t *section)
{
    if (section != NULL)
        section->pkt_loaded = 0;
} // reset the loaded packet count for a section

static __inline__ pcap_section_t *
pcap_section_from_mp(pcap_info_t *pcap, struct rte_mempool *mp)
{
    if (pcap == NULL || mp == NULL)
        return NULL;

    if (mp == pcap->sections[0].mp)
        return &pcap->sections[0];
    if (mp == pcap->sections[1].mp)
        return &pcap->sections[1];

    return NULL;
} // get the section pointer for a mempool, or NULL if not found

static __inline__ void mbuf_iterate_cb(struct rte_mempool *mp, void *opaque, void *obj,
                                       unsigned obj_idx __rte_unused);


static void
pcap_load_section(pcap_info_t *pcap, pcap_section_t *section)
{
    if (pcap == NULL || section == NULL || section->mp == NULL)
        return;

    section->file_offset_begin = ftell(pcap->fp); // record the beginning offset for this section load
    section->chunk_id          = pcap->next_chunk_id++; // assign a chunk ID for this section
    pcap_section_reset(section); // reset the loaded packet count for this section before loading
    rte_mempool_obj_iter(section->mp, mbuf_iterate_cb, pcap); // iterate over all mbufs in the mempool and load packets from the pcap file into them
    section->file_offset_end = ftell(pcap->fp); // record the end offset for this section load
} // load packets into a section

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
    if (flag) {
        printf("  Packet count: %d, max size %d\n", pcap->pkt_count, pcap->max_pkt_size);
        // print section information
        printf("  Section 0: loaded %u / %u, budget %lu bytes\n",
               pcap->sections[0].pkt_loaded, pcap->sections[0].pkt_count,
               (unsigned long)pcap->sections[0].budget_bytes);
        printf("             chunk: %lu\n", (unsigned long)pcap->sections[0].chunk_id);
         printf("             offsets: %ld -> %ld\n", pcap->sections[0].file_offset_begin,
             pcap->sections[0].file_offset_end);
        printf("  Section 1: loaded %u / %u, budget %lu bytes\n",
               pcap->sections[1].pkt_loaded, pcap->sections[1].pkt_count,
               (unsigned long)pcap->sections[1].budget_bytes);
        printf("             chunk: %lu\n", (unsigned long)pcap->sections[1].chunk_id);
        printf("             offsets: %ld -> %ld\n", pcap->sections[1].file_offset_begin,
            pcap->sections[1].file_offset_end);
        printf("TEST\n");
    }
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
    pcap_section_t *section = pcap_section_from_mp(pcap, mp); // get the section pointer for this mempool

    if (section != NULL && section->pkt_loaded >= section->pkt_count)
        return;

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

    if (section != NULL)
        section->pkt_loaded++;
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
    pcap->active_section_idx = 0; // start with section 0 as the active section
    pcap->next_chunk_id = 1; // initialize the next chunk ID to 1

    pcap_info_list[pid] = pcap;

    return 0;
}

int
pktgen_pcap_open(void)
{
    pcap_info_t *pcap = NULL;
    struct rte_mempool *mp;
    char name[64] = {0};
    uint16_t sid;
    uint32_t pkt_count;
    uint64_t total_hugepage_bytes = 0;
    uint64_t hugepage_size_bytes = 0;
    int have_hugepage_info;

    have_hugepage_info = get_total_hugepage_bytes(&total_hugepage_bytes, &hugepage_size_bytes);

    for (int pid = 0; pid < RTE_MAX_ETHPORTS; pid++) {
        if ((pcap = pcap_info_list[pid]) == NULL)
            continue;

        pcap = pcap_info_list[pid];

        sid = pg_eth_dev_socket_id(pid);

        /* Read the pcap file trailer. */
        pcap->fp = fopen((const char *)pcap->filename, "r");
        if (pcap->fp == NULL)
            rte_exit(EXIT_FAILURE, "%s: failed for (%s)\n", __func__, pcap->filename);

        pcap_get_info(pcap);

        pkt_count = pcap->pkt_count;
        if (pkt_count == 0) {
            fclose(pcap->fp);
            rte_exit(EXIT_FAILURE, "%s: PCAP file is empty: %s\n", __func__, pcap->filename);
        }
        if (pkt_count < (DEFAULT_TX_DESC * 4))
            pkt_count = (DEFAULT_TX_DESC * 4);

        snprintf(name, sizeof(name), "pcap-%d", pid);
        uint32_t dataroom =
            RTE_ALIGN_CEIL(pcap->max_pkt_size + RTE_PKTMBUF_HEADROOM, RTE_CACHE_LINE_SIZE);
        pktgen_log_info("PCAP port %d: dataroom bytes calculated: %u", pid, dataroom); // remove

        if (have_hugepage_info == 0) {
            uint64_t reserve_hugepage_bytes =
                (uint64_t)DEFAULT_PKTGEN_BASELINE_HUGEPAGES * hugepage_size_bytes;
            uint64_t available_hugepage_bytes = 0;
            uint64_t section0_budget_bytes = 0; // initial hugepage budget for section 0, will be updated by pcap_split_section_budgets
            uint64_t section1_budget_bytes = 0; // ^^
            uint64_t per_pkt_bytes =
                RTE_ALIGN_CEIL(sizeof(struct rte_mbuf) + DEFAULT_PRIV_SIZE + dataroom,
                               RTE_CACHE_LINE_SIZE) + 32; // align to cache line size and account for potential mbuf overhead
            uint32_t section0_pkt_cap = 0; // initial packet capacity for section 0, will be updated based on the section 0 budget and per-packet bytes
            uint32_t section1_pkt_cap = 0; // ^^
            pktgen_log_info("PCAP port %d per_pkt_bytes calc: sizeof(rte_mbuf)=%zu "
                            "priv=%u dataroom=%u cache_line=%u result=%lu",
                            pid, sizeof(struct rte_mbuf), (unsigned)DEFAULT_PRIV_SIZE,
                            dataroom, (unsigned)RTE_CACHE_LINE_SIZE, per_pkt_bytes); // remove
            uint32_t max_pkts_fit = 0;

            if (total_hugepage_bytes > reserve_hugepage_bytes)
                available_hugepage_bytes = total_hugepage_bytes - reserve_hugepage_bytes;

            available_hugepage_bytes = pcap_split_section_budgets(
                total_hugepage_bytes, reserve_hugepage_bytes, &section0_budget_bytes,
                &section1_budget_bytes); // split the available hugepage bytes across the two sections and get the section budgets

            pcap->sections[0].budget_bytes = section0_budget_bytes; // set the section 0 budget in the pcap info structure
            pcap->sections[1].budget_bytes = section1_budget_bytes; // ^^

            if (per_pkt_bytes > 0) {
                section0_pkt_cap = (uint32_t)(section0_budget_bytes / per_pkt_bytes);
                section1_pkt_cap = (uint32_t)(section1_budget_bytes / per_pkt_bytes);
            } // calculate the packet capacity for each section based on the section budgets and per-packet bytes

            pcap->sections[0].pkt_count = section0_pkt_cap; // set the section 0 packet count cap in the pcap info structure
            pcap->sections[1].pkt_count = section1_pkt_cap; // ^^

            if (per_pkt_bytes > 0)
                max_pkts_fit = (uint32_t)(available_hugepage_bytes / per_pkt_bytes);

            if (max_pkts_fit == 0)
                rte_exit(EXIT_FAILURE,
                         "%s: not enough hugepage memory for PCAP port %d "
                         "(total=%lu bytes, reserved baseline=%lu pages)",
                         __func__, pid, total_hugepage_bytes,
                         (uint64_t)DEFAULT_PKTGEN_BASELINE_HUGEPAGES);

            if (pkt_count > max_pkts_fit) {
                pktgen_log_info("PCAP port %d: reducing pkt_count from %u to %u to fit total "
                                "hugepages",
                                pid, pkt_count, max_pkts_fit);
                pkt_count = max_pkts_fit;
            }
        } else {
            pktgen_log_info("PCAP port %d: unable to read total hugepage info, keeping pkt_count "
                            "at %u",
                            pid, pkt_count);
        }

        uint32_t section0_create_pkts = pkt_count; // initial packet count for section 0
        uint32_t section1_create_pkts = pkt_count; // ^^
        if (have_hugepage_info == 0 && pcap->sections[0].pkt_count > 0)
            section0_create_pkts = pcap->sections[0].pkt_count; // if we don't have hugepage info and the section 0 packet cap is set, use that as the packet count for section 0
        if (have_hugepage_info == 0 && pcap->sections[1].pkt_count > 0)
            section1_create_pkts = pcap->sections[1].pkt_count; // ^^

        /* Ensure section caps are valid in all startup paths before loading. */
        pcap->sections[0].pkt_count = section0_create_pkts;
        pcap->sections[1].pkt_count = section1_create_pkts;

        pktgen_log_info("PCAP port %d: section packet caps section0=%u section1=%u", pid,
                        section0_create_pkts, section1_create_pkts); // log the final section packet caps that will be used for creating the mempools

        mp = rte_pktmbuf_pool_create(name, section0_create_pkts, 0, DEFAULT_PRIV_SIZE, dataroom,
                                     sid); // create the mempool for section 0 with the calculated packet count and dataroom
        if (mp == NULL)
            rte_exit(EXIT_FAILURE,
                     "Cannot create mbuf pool (%s) port %d, nb_mbufs %d, socket_id %d: %s", name,
                     pid, section0_create_pkts, sid, rte_strerror(rte_errno)); // if section 0 mempool creation fails, exit with an error

        pcap->mp = mp; // set the main mempool pointer in the pcap info structure to the section 0 mempool
        pcap->sections[0].mp = mp; // set the section 0 mempool pointer in the pcap info structure
        pcap_load_section(pcap, &pcap->sections[0]); // load packets into section 0

        snprintf(name, sizeof(name), "pcap-%d-sec1", pid); // create a name for the section 1 mempool based on the port ID
        mp = rte_pktmbuf_pool_create(name, section1_create_pkts, 0, DEFAULT_PRIV_SIZE, dataroom,
                                     sid); // create the mempool for section 1 with the calculated packet count and dataroom
        if (mp == NULL)
            rte_exit(EXIT_FAILURE,
                     "Cannot create mbuf pool (%s) port %d, nb_mbufs %d, socket_id %d: %s", name,
                     pid, section1_create_pkts, sid, rte_strerror(rte_errno)); // if section 1 mempool creation fails, exit with an error

        pcap->sections[1].mp = mp; // set the section 1 mempool pointer in the pcap info structure
        pcap_load_section(pcap, &pcap->sections[1]); // load packets into section 1

        if (l2p_set_pcap_info(pid, pcap) < 0)
            pktgen_log_error("Error opening PCAP file: %s", pcap->filename);
    }
    return 0;
}

void
pktgen_pcap_close(void)
{
    pcap_info_t *pcap = NULL;

    for (int pid = 0; pid < RTE_MAX_ETHPORTS; pid++) {
        pcap = pcap_info_list[pid];
        if (pcap == NULL)
            continue; // changed from ret to continue since we want to attempt to close all pcaps even if one is NULL

        if (pcap->filename)
            free(pcap->filename);
        if (pcap->fp)
            fclose(pcap->fp);
        if (pcap->sections[0].mp)
            rte_mempool_free(pcap->sections[0].mp); // free the section 0 mempool
        if (pcap->sections[1].mp)
            rte_mempool_free(pcap->sections[1].mp); // free the section 1 mempool
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
