-- Add current script directory and common Pktgen paths to Lua module search path
local script_dir = debug.getinfo(1).source:match("@?(.*/)")
if script_dir then
    package.path = package.path .. ";" .. script_dir .. "?.lua"
end
package.path = package.path .. ";?.lua;test/?.lua;app/?.lua;"

require "Pktgen"

--
-- Replay a large PCAP in fixed windows by repeatedly reloading from a new start index.
--
-- How it works:
-- 1) Pktgen can only load a bounded packet window into memory at one time.
-- 2) Set total_pkts and loaded_pkts from "pcap show".
-- 3) Each cycle advances start_pkt by loaded_pkts and wraps by total_pkts.
-- 4) Reload from the new start index and send exactly loaded_pkts packets.
--
-- Run with:
--   pktgen ... -f scripts/pcap-window-loop.lua
--

local CFG = {
    -- Port to replay on (string form matches pktgen Lua API conventions).
    port = "0",

    -- PCAP path as seen by pktgen process.
    pcap_file = "../data/chunk_20_0.pcap",

    -- REQUIRED: total packets in the source file (pcap show total packet count).
    total_pkts = 1048576,

    -- REQUIRED: packets currently loaded in memory (pcap show loaded packet count).
    loaded_pkts = 500000,

    -- Max wait time for one loaded window to finish transmitting.
    max_wait_ms_per_window = 120000,

    -- Optional delay between windows.
    pause_ms_between_windows = 100,

    -- 0 means infinite loop, otherwise run exactly this many windows.
    windows_to_run = 0,

    -- Where to emit one-line command files used by pktgen.load().
    tmp_cmd_file = "/tmp/pktgen_pcap_reload.cmd",
}

local function cli_quote(s)
    local v = tostring(s or "")

    -- Escape characters that can break quoted CLI token parsing.
    v = string.gsub(v, "\\", "\\\\")
    v = string.gsub(v, '"', '\\"')

    return string.format('"%s"', v)
end

local function write_cmd_file(path, line)
    local f, err = io.open(path, "w")
    if not f then
        error("Failed to open command file: " .. tostring(path) .. " error=" .. tostring(err))
    end
    f:write(line)
    f:write("\n")
    f:close()
end

local function run_cmd(line)
    write_cmd_file(CFG.tmp_cmd_file, line)
    pktgen.load(CFG.tmp_cmd_file)
end

local function reload_window(start_pkt)
    -- pcap reload syntax:
    --   pcap reload <portlist> <filename> <start_pkt>
    --   pcap reload <portlist> <filename> <start_pkt> add <count>
    local cmd = string.format("pcap reload %s %s %d add %d", CFG.port, cli_quote(CFG.pcap_file),
                              start_pkt, CFG.loaded_pkts)
    run_cmd(cmd)
end

local function wait_window_done(max_wait_ms)
    local waited_ms = 0

    while true do
        local sending = pktgen.isSending(CFG.port)
        if sending[tonumber(CFG.port)] == "n" then
            return true
        end

        if waited_ms >= max_wait_ms then
            return false
        end

        pktgen.delay(100)
        waited_ms = waited_ms + 100
    end
end

local function run_one_window(start_pkt, index)
    pktgen.stop(CFG.port)
    reload_window(start_pkt)
    pktgen.set(CFG.port, "count", CFG.loaded_pkts)
    pktgen.pcap(CFG.port, "enable")
    pktgen.start(CFG.port)

    printf("Window %d: start_pkt=%d, send_count=%d\n", index, start_pkt, CFG.loaded_pkts)

    if not wait_window_done(CFG.max_wait_ms_per_window) then
        pktgen.stop(CFG.port)
        error(string.format("Window %d timed out after %d ms", index, CFG.max_wait_ms_per_window))
    end

    pktgen.stop(CFG.port)
    if CFG.pause_ms_between_windows > 0 then
        pktgen.delay(CFG.pause_ms_between_windows)
    end
end

local function main()
    if CFG.total_pkts == nil or CFG.total_pkts <= 0 then
        errmsg("CFG.total_pkts must be > 0\n")
    end
    if CFG.loaded_pkts == nil or CFG.loaded_pkts <= 0 then
        errmsg("CFG.loaded_pkts must be > 0\n")
    end
    if CFG.max_wait_ms_per_window == nil or CFG.max_wait_ms_per_window <= 0 then
        errmsg("CFG.max_wait_ms_per_window must be > 0\n")
    end
    if CFG.windows_to_run == nil or CFG.windows_to_run < 0 then
        errmsg("CFG.windows_to_run must be >= 0\n")
    end
    if CFG.pause_ms_between_windows == nil or CFG.pause_ms_between_windows < 0 then
        errmsg("CFG.pause_ms_between_windows must be >= 0\n")
    end

    -- Each cycle sends exactly loaded_pkts packets before advancing start index.
    pktgen.stop(CFG.port)
    pktgen.set(CFG.port, "count", CFG.loaded_pkts)

    local start_pkt = 0
    local i = 0

    while true do
        i = i + 1
        run_one_window(start_pkt, i)

        -- Advance by loaded packets and wrap around total packets in file.
        start_pkt = (start_pkt + CFG.loaded_pkts) % CFG.total_pkts

        if CFG.windows_to_run > 0 and i >= CFG.windows_to_run then
            break
        end
    end

    printf("Done after %d window(s).\n", i)
end

main()
