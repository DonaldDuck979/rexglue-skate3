/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2022 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * @modified    Tom Clay, 2026 - Adapted for ReXGlue runtime
 */

// Disable warnings about unused parameters for kernel functions
#pragma GCC diagnostic ignored "-Wunused-parameter"

#include <cstdio>
#include <cstring>

#include <rex/chrono/clock.h>
#include <rex/cvar.h>
#include <rex/kernel/xam/module.h>
#include <rex/kernel/xam/private.h>
#include <rex/kernel/xboxkrnl/error.h>
#include <rex/kernel/xboxkrnl/threading.h>
#include <rex/logging.h>
#include <rex/hook.h>
#include <rex/types.h>
#include <rex/string.h>
#include <rex/system/kernel_state.h>
#include <rex/system/xevent.h>
#include <rex/system/xsocket.h>
#include <rex/system/xthread.h>
#include <rex/system/xtypes.h>

// [skate3-online] Packet log: reveals every real network call the game makes.
// Default ON so the first session after this build produces a log we can read
// without having to enable a cvar; turn off with `skate3_net_packet_log 0`
// once we've captured the endpoints.
REXCVAR_DEFINE_BOOL(skate3_net_packet_log, false, "Net",
                    "Diagnostic: log every socket send/recv/connect/DNS lookup "
                    "the game makes (dest IP:port + len + first 16 bytes hex) "
                    "plus XLIVEBASE message codes. Default OFF (this was an "
                    "EA-Nation investigation aid; noisy). Enable with "
                    "`skate3_net_packet_log 1` only when debugging the game's "
                    "own network calls.")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);
// [skate3-online] The "online" IP reported to the game by XNetGetTitleXnAddr
// (the inaOnline field = this console's address as the outside world sees it).
// Reporting a non-zero online address + the ONLINE status flag is what makes
// the game believe it has an online presence and proceed to actually connect
// to EA Nation (instead of polling forever then "lost connection to EA
// Nation"). Defaults to James's known home PUBLIC IP; update it here if the
// ISP reassigns it (a public IP can be dynamic). Empty = fall back to the
// machine's local IP.
REXCVAR_DEFINE_STRING(skate3_xnet_online_ip, "74.221.197.102", "Net",
                      "Public IP reported to the game as its online address "
                      "(XNetGetTitleXnAddr inaOnline). Set to your current "
                      "home public IP; empty = use the local IP. Only used when "
                      "skate3_xnet_report_online is on.")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);
// [skate3-online] Master gate for the "report a real online presence" behavior
// in XNetGetTitleXnAddr. DEFAULT OFF: with it on, the game believes it's online
// and actively tries to connect to EA Nation -- which dead-ends at EA's
// console-certificate auth and shows the user "Connecting to EA Nation... /
// You have lost your connection to EA Nation" errors. For the SHIPPING fan-
// online build we want the game to sit quietly "not online" (our own netplay
// is the online path), so this stays off. Kept as a cvar so the EA-Nation /
// revival-server experiment can be resumed later with `skate3_xnet_report_online 1`.
// [skate3-online v2] DEFAULT TRUE on the online-v2-blaze branch: v2's whole
// purpose is to make the game go online for real and dial out to OUR revival
// Blaze server (not EA's). On the shipping `online-layer` branch this stays
// false. With it on, the game clears the sign-in / "need internet" gates and
// actively tries to connect -- which currently dead-ends until the v2 Blaze
// server + DNS redirect exist; that connection attempt is exactly what we want
// to observe (enable skate3_net_packet_log to see the EA endpoints it reaches).
REXCVAR_DEFINE_BOOL(skate3_xnet_report_online, true, "Net",
                    "EXPERIMENTAL: report a real online presence to the game so "
                    "its own EA Nation / Xbox LIVE client tries to connect. "
                    "v2 default ON (dials out to our own Blaze server; leave "
                    "off unless experimenting with real/revival servers).")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

// [skate3-online v2] The IP we hand the game's EA client when it asks the Xbox
// secure-server resolver (XNetServerToInAddr / XNetXnAddrToInAddr) "what's the
// real address of the EA server?". Redirecting this to our own machine is how we
// point Skate 3's own online client at our revival Blaze server instead of EA's.
// Default 127.0.0.1 = a local listener on this same PC. Set to a LAN/VPS IP once
// the Blaze server runs elsewhere. Only used when skate3_xnet_report_online is on.
REXCVAR_DEFINE_STRING(skate3_blaze_server_ip, "127.0.0.1", "Net",
                      "IPv4 address the game's EA/Blaze client is redirected to "
                      "when it resolves the EA server (XNetServerToInAddr / "
                      "XNetXnAddrToInAddr). Default 127.0.0.1 (local Blaze server).")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

// [skate3-online v2] The port our redirected Blaze/EA server listens on. Handed
// to the game inside XONLINE_SERVICE_INFO by the XLiveBase GetServiceInfo handler
// (xlivebase_app.cpp msg 0x58007) alongside skate3_blaze_server_ip. Default 3659
// = EA's classic Blaze redirector port; change to whatever our server binds.
REXCVAR_DEFINE_UINT32(skate3_blaze_server_port, 3659, "Net",
                      "Port the game's EA/Blaze client is redirected to (paired "
                      "with skate3_blaze_server_ip in XOnlineGetServiceInfo).")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

// [skate3-online v2] One-shot dump of committed guest memory to a file, triggered
// the first time the game does a post-redirector DnsLookup (Blaze schema fully
// loaded by then). Used to reverse Skate's TDF container byte-encoding offline by
// searching for the packed ADDR/VALU tags + their type tables. Default OFF.
REXCVAR_DEFINE_BOOL(skate3_dump_guestmem, false, "Net",
                    "Dump committed guest memory to Skate3-Blaze-Server/guestmem.bin "
                    "on the next DnsLookup (for offline Blaze protocol RE).")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

#if REX_PLATFORM_WIN32
// NOTE: must be included last as it expects windows.h to already be included.
#define _WINSOCK_DEPRECATED_NO_WARNINGS  // inet_addr
#include <winsock2.h>                    // NOLINT(build/include_order)
#elif REX_PLATFORM_LINUX || REX_PLATFORM_MAC
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <sys/select.h>
#include <sys/socket.h>
#endif

namespace rex {
namespace kernel {
namespace xam {
using namespace rex::system;
using namespace rex::system::xam;

// [skate3-online] Packet-log helpers. Format sockaddr fields safely regardless
// of rex::be's implicit-conversion behavior by extracting raw bytes (sin_addr
// and sin_port are always stored network byte order per XSOCKADDR_IN's comment).
static void FormatSockaddr(const N_XSOCKADDR_IN* sa, char* out, size_t out_sz) {
  if (!sa) { std::snprintf(out, out_sz, "?"); return; }
  const uint8_t* ab = reinterpret_cast<const uint8_t*>(&sa->sin_addr);
  const uint8_t* pb = reinterpret_cast<const uint8_t*>(&sa->sin_port);
  const uint16_t port = (uint16_t(pb[0]) << 8) | uint16_t(pb[1]);
  std::snprintf(out, out_sz, "%u.%u.%u.%u:%u",
                ab[0], ab[1], ab[2], ab[3], port);
}
// First 16 bytes of a payload as hex, for quick fingerprinting of packets.
static void FormatHexPeek(const void* data, size_t len, char* out, size_t out_sz) {
  const uint8_t* p = static_cast<const uint8_t*>(data);
  const size_t n = std::min<size_t>(len, 16);
  size_t off = 0;
  for (size_t i = 0; i < n && off + 3 < out_sz; ++i) {
    off += std::snprintf(out + off, out_sz - off, "%02x ", p[i]);
  }
  if (off > 0 && out[off - 1] == ' ') out[off - 1] = 0;
  else if (off < out_sz) out[off] = 0;
}
static inline bool PacketLogEnabled() {
  return REXCVAR_GET(skate3_net_packet_log);
}

// [skate3-online] Best-effort primary outbound local IPv4, in network byte
// order. Opens a UDP socket and "connects" it toward a public address (UDP
// connect sends nothing -- it just makes the OS pick the default-route
// interface), then reads back which local address that is. Falls back to
// loopback. Windows-only (this file's winsock path is Win32); other platforms
// return loopback.
static uint32_t PrimaryLocalIPv4NBO() {
#if REX_PLATFORM_WIN32
  SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s == INVALID_SOCKET) {
    return htonl(INADDR_LOOPBACK);
  }
  sockaddr_in probe = {};
  probe.sin_family = AF_INET;
  probe.sin_port = htons(53);
  probe.sin_addr.s_addr = inet_addr("8.8.8.8");
  uint32_t ip = htonl(INADDR_LOOPBACK);
  if (::connect(s, reinterpret_cast<sockaddr*>(&probe), sizeof(probe)) == 0) {
    sockaddr_in local = {};
    int len = static_cast<int>(sizeof(local));
    if (::getsockname(s, reinterpret_cast<sockaddr*>(&local), &len) == 0 &&
        local.sin_addr.s_addr != 0) {
      ip = local.sin_addr.s_addr;  // already network byte order.
    }
  }
  ::closesocket(s);
  return ip;
#else
  return htonl(INADDR_LOOPBACK);
#endif
}

// https://github.com/G91/TitanOffLine/blob/1e692d9bb9dfac386d08045ccdadf4ae3227bb5e/xkelib/xam/xamNet.h
enum {
  XNCALLER_INVALID = 0x0,
  XNCALLER_TITLE = 0x1,
  XNCALLER_SYSAPP = 0x2,
  XNCALLER_XBDM = 0x3,
  XNCALLER_TEST = 0x4,
  NUM_XNCALLER_TYPES = 0x4,
};

// https://github.com/pmrowla/hl2sdk-csgo/blob/master/common/xbox/xboxstubs.h
typedef struct {
  // FYI: IN_ADDR should be in network-byte order.
  in_addr ina;                    // IP address (zero if not static/DHCP)
  in_addr inaOnline;              // Online IP address (zero if not online)
  rex::be<uint16_t> wPortOnline;  // Online port
  uint8_t abEnet[6];              // Ethernet MAC address
  uint8_t abOnline[20];           // Online identification
} XNADDR;

typedef struct {
  rex::be<int32_t> status;
  rex::be<uint32_t> cina;
  in_addr aina[8];
} XNDNS;

typedef struct {
  uint8_t flags;
  uint8_t reserved;
  rex::be<uint16_t> probes_xmit;
  rex::be<uint16_t> probes_recv;
  rex::be<uint16_t> data_len;
  rex::be<uint32_t> data_ptr;
  rex::be<uint16_t> rtt_min_in_msecs;
  rex::be<uint16_t> rtt_med_in_msecs;
  rex::be<uint32_t> up_bits_per_sec;
  rex::be<uint32_t> down_bits_per_sec;
} XNQOSINFO;

typedef struct {
  rex::be<uint32_t> count;
  rex::be<uint32_t> count_pending;
  XNQOSINFO info[1];
} XNQOS;

struct Xsockaddr_t {
  rex::be<uint16_t> sa_family;
  char sa_data[14];
};

struct X_WSADATA {
  rex::be<uint16_t> version;
  rex::be<uint16_t> version_high;
  char description[256 + 1];
  char system_status[128 + 1];
  rex::be<uint16_t> max_sockets;
  rex::be<uint16_t> max_udpdg;
  rex::be<uint32_t> vendor_info_ptr;
};

struct XWSABUF {
  rex::be<uint32_t> len;
  rex::be<uint32_t> buf_ptr;
};

struct XWSAOVERLAPPED {
  rex::be<uint32_t> internal;
  rex::be<uint32_t> internal_high;
  union {
    struct {
      rex::be<uint32_t> low;
      rex::be<uint32_t> high;
    } offset;  // must be named to avoid GCC error
    rex::be<uint32_t> pointer;
  };
  rex::be<uint32_t> event_handle;
};

void LoadSockaddr(const uint8_t* ptr, sockaddr* out_addr) {
  out_addr->sa_family = memory::load_and_swap<uint16_t>(ptr + 0);
  switch (out_addr->sa_family) {
    case AF_INET: {
      auto in_addr = reinterpret_cast<sockaddr_in*>(out_addr);
      in_addr->sin_port = memory::load_and_swap<uint16_t>(ptr + 2);
      // Maybe? Depends on type.
      in_addr->sin_addr.s_addr = *(uint32_t*)(ptr + 4);
      break;
    }
    default:
      assert_unhandled_case(out_addr->sa_family);
      break;
  }
}

void StoreSockaddr(const sockaddr& addr, uint8_t* ptr) {
  switch (addr.sa_family) {
    case AF_UNSPEC:
      std::memset(ptr, 0, sizeof(addr));
      break;
    case AF_INET: {
      auto& in_addr = reinterpret_cast<const sockaddr_in&>(addr);
      memory::store_and_swap<uint16_t>(ptr + 0, in_addr.sin_family);
      memory::store_and_swap<uint16_t>(ptr + 2, in_addr.sin_port);
      // Maybe? Depends on type.
      memory::store_and_swap<uint32_t>(ptr + 4, in_addr.sin_addr.s_addr);
      break;
    }
    default:
      assert_unhandled_case(addr.sa_family);
      break;
  }
}

// https://github.com/joolswills/mameox/blob/master/MAMEoX/Sources/xbox_Network.cpp#L136
struct XNetStartupParams {
  uint8_t cfgSizeOfStruct;
  uint8_t cfgFlags;
  uint8_t cfgSockMaxDgramSockets;
  uint8_t cfgSockMaxStreamSockets;
  uint8_t cfgSockDefaultRecvBufsizeInK;
  uint8_t cfgSockDefaultSendBufsizeInK;
  uint8_t cfgKeyRegMax;
  uint8_t cfgSecRegMax;
  uint8_t cfgQosDataLimitDiv4;
  uint8_t cfgQosProbeTimeoutInSeconds;
  uint8_t cfgQosProbeRetries;
  uint8_t cfgQosSrvMaxSimultaneousResponses;
  uint8_t cfgQosPairWaitTimeInSeconds;
};

XNetStartupParams xnet_startup_params = {0};

u32 NetDll_XNetStartup_entry(u32 caller, ppc_ptr_t<XNetStartupParams> params) {
  if (PacketLogEnabled()) {
    REXKRNL_INFO("[net-pkt] XNetStartup caller={}", caller);
  }
  if (params) {
    assert_true(params->cfgSizeOfStruct == sizeof(XNetStartupParams));
    std::memcpy(&xnet_startup_params, params, sizeof(XNetStartupParams));
  }

  auto xam = REX_KERNEL_STATE()->GetKernelModule<XamModule>("xam.xex");

  /*
  if (!xam->xnet()) {
    auto xnet = new XNet(REX_KERNEL_STATE());
    xnet->Initialize();

    xam->set_xnet(xnet);
  }
  */

  return 0;
}

u32 NetDll_XNetCleanup_entry(u32 caller, mapped_void params) {
  auto xam = REX_KERNEL_STATE()->GetKernelModule<XamModule>("xam.xex");
  // auto xnet = xam->xnet();
  // xam->set_xnet(nullptr);

  // TODO: Shut down and delete.
  // delete xnet;

  return 0;
}

u32 NetDll_XNetGetOpt_entry(u32 one, u32 option_id, mapped_void buffer_ptr,
                            mapped_u32 buffer_size) {
  assert_true(one == 1);
  switch (option_id) {
    case 1:
      if (*buffer_size < sizeof(XNetStartupParams)) {
        *buffer_size = sizeof(XNetStartupParams);
        return 0x2738;  // WSAEMSGSIZE
      }
      std::memcpy(buffer_ptr, &xnet_startup_params, sizeof(XNetStartupParams));
      return 0;
    default:
      REXKRNL_ERROR("NetDll_XNetGetOpt: option {} unimplemented", option_id);
      return 0x2726;  // WSAEINVAL
  }
}

u32 NetDll_XNetRandom_entry(u32 caller, mapped_void buffer_ptr, u32 length) {
  // For now, constant values.
  // This makes replicating things easier.
  std::memset(buffer_ptr, 0xBB, length);

  return 0;
}

u32 NetDll_WSAStartup_entry(u32 caller, u16 version, ppc_ptr_t<X_WSADATA> data_ptr) {
// TODO(benvanik): abstraction layer needed.
#if REX_PLATFORM_WIN32
  WSADATA wsaData;
  ZeroMemory(&wsaData, sizeof(WSADATA));
  int ret = WSAStartup(version, &wsaData);

  auto data_out = REX_KERNEL_MEMORY()->TranslateVirtual(data_ptr.guest_address());

  if (data_ptr) {
    data_ptr->version = wsaData.wVersion;
    data_ptr->version_high = wsaData.wHighVersion;
    std::memcpy(&data_ptr->description, wsaData.szDescription, 0x100);
    std::memcpy(&data_ptr->system_status, wsaData.szSystemStatus, 0x80);
    data_ptr->max_sockets = wsaData.iMaxSockets;
    data_ptr->max_udpdg = wsaData.iMaxUdpDg;

    // Some games (5841099F) want this value round-tripped - they'll compare if
    // it changes and bugcheck if it does.
    uint32_t vendor_ptr = memory::load_and_swap<uint32_t>(data_out + 0x190);
    memory::store_and_swap<uint32_t>(data_out + 0x190, vendor_ptr);
  }
#else
  int ret = 0;
  if (data_ptr) {
    // Guess these values!
    data_ptr->version = version;
    data_ptr->description[0] = '\0';
    data_ptr->system_status[0] = '\0';
    data_ptr->max_sockets = 100;
    data_ptr->max_udpdg = 1024;
  }
#endif

  // DEBUG
  /*
  auto xam = REX_KERNEL_STATE()->GetKernelModule<XamModule>("xam.xex");
  if (!xam->xnet()) {
    auto xnet = new XNet(REX_KERNEL_STATE());
    xnet->Initialize();

    xam->set_xnet(xnet);
  }
  */

  return ret;
}

u32 NetDll_WSACleanup_entry(u32 caller) {
  // This does nothing. Xenia needs WSA running.
  return 0;
}

u32 NetDll_WSAGetLastError_entry() {
  uint32_t e = XThread::GetLastError();
  if (PacketLogEnabled() && e != 0) {
    REXKRNL_INFO("[net-pkt] WSAGetLastError -> {:#x}", e);
  }
  return e;
}

u32 NetDll_WSARecvFrom_entry(u32 caller, u32 socket, ppc_ptr_t<XWSABUF> buffers_ptr,
                             u32 buffer_count, mapped_u32 num_bytes_recv, mapped_u32 flags_ptr,
                             ppc_ptr_t<XSOCKADDR_IN> from_addr,
                             ppc_ptr_t<XWSAOVERLAPPED> overlapped_ptr,
                             mapped_void completion_routine_ptr) {
  if (overlapped_ptr) {
    // auto evt = REX_KERNEL_OBJECTS()->LookupObject<XEvent>(
    //    overlapped_ptr->event_handle);

    // if (evt) {
    //  //evt->Set(0, false);
    //}
  }

  // we're not going to be receiving packets any time soon
  // return error so we don't wait on that - Cancerous
  return -1;
}

// If the socket is a VDP socket, buffer 0 is the game data length, and buffer 1
// is the unencrypted game data.
u32 NetDll_WSASendTo_entry(u32 caller, u32 socket_handle, ppc_ptr_t<XWSABUF> buffers,
                           u32 num_buffers, mapped_u32 num_bytes_sent, u32 flags,
                           ppc_ptr_t<XSOCKADDR_IN> to_ptr, u32 to_len,
                           ppc_ptr_t<XWSAOVERLAPPED> overlapped, mapped_void completion_routine) {
  assert(!overlapped);
  assert(!completion_routine);

  auto socket = REX_KERNEL_OBJECTS()->LookupObject<XSocket>(socket_handle);
  if (!socket) {
    // WSAENOTSOCK
    XThread::SetLastError(0x2736);
    return -1;
  }

  // Our sockets implementation doesn't support multiple buffers, so we need
  // to combine the buffers the game has given us!
  std::vector<uint8_t> combined_buffer_mem;
  uint32_t combined_buffer_size = 0;
  uint32_t combined_buffer_offset = 0;
  for (uint32_t i = 0; i < num_buffers; i++) {
    combined_buffer_size += buffers[i].len;
    combined_buffer_mem.resize(combined_buffer_size);
    uint8_t* combined_buffer = combined_buffer_mem.data();

    std::memcpy(combined_buffer + combined_buffer_offset,
                REX_KERNEL_MEMORY()->TranslateVirtual(buffers[i].buf_ptr), buffers[i].len);
    combined_buffer_offset += buffers[i].len;
  }

  N_XSOCKADDR_IN native_to(to_ptr);
  socket->SendTo(combined_buffer_mem.data(), combined_buffer_size, flags, &native_to, to_len);

  // TODO: Instantly complete overlapped

  return 0;
}

u32 NetDll_WSAWaitForMultipleEvents_entry(u32 num_events, mapped_u32 events, u32 wait_all,
                                          u32 timeout, u32 alertable) {
  if (num_events > 64) {
    XThread::SetLastError(87);  // ERROR_INVALID_PARAMETER
    return ~0u;
  }

  uint64_t timeout_wait = (uint64_t)timeout;

  X_STATUS result = 0;
  do {
    result = xboxkrnl::xeNtWaitForMultipleObjectsEx(num_events, events, wait_all, 1, alertable,
                                                    timeout != -1 ? &timeout_wait : nullptr);
  } while (result == X_STATUS_ALERTED);

  if (XFAILED(result)) {
    uint32_t error = xboxkrnl::xeRtlNtStatusToDosError(result);
    XThread::SetLastError(error);
    return ~0u;
  }
  return 0;
}

u32 NetDll_WSACreateEvent_entry() {
  XEvent* ev = new XEvent(REX_KERNEL_STATE());
  ev->Initialize(true, false);
  return ev->handle();
}

u32 NetDll_WSACloseEvent_entry(u32 event_handle) {
  X_STATUS result = REX_KERNEL_OBJECTS()->ReleaseHandle(event_handle);
  if (XFAILED(result)) {
    uint32_t error = xboxkrnl::xeRtlNtStatusToDosError(result);
    XThread::SetLastError(error);
    return 0;
  }
  return 1;
}

u32 NetDll_WSAResetEvent_entry(u32 event_handle) {
  X_STATUS result = xboxkrnl::xeNtClearEvent(event_handle);
  if (XFAILED(result)) {
    uint32_t error = xboxkrnl::xeRtlNtStatusToDosError(result);
    XThread::SetLastError(error);
    return 0;
  }
  return 1;
}

u32 NetDll_WSASetEvent_entry(u32 event_handle) {
  X_STATUS result = xboxkrnl::xeNtSetEvent(event_handle, nullptr);
  if (XFAILED(result)) {
    uint32_t error = xboxkrnl::xeRtlNtStatusToDosError(result);
    XThread::SetLastError(error);
    return 0;
  }
  return 1;
}

struct XnAddrStatus {
  // Address acquisition is not yet complete
  static const uint32_t XNET_GET_XNADDR_PENDING = 0x00000000;
  // XNet is uninitialized or no debugger found
  static const uint32_t XNET_GET_XNADDR_NONE = 0x00000001;
  // Host has ethernet address (no IP address)
  static const uint32_t XNET_GET_XNADDR_ETHERNET = 0x00000002;
  // Host has statically assigned IP address
  static const uint32_t XNET_GET_XNADDR_STATIC = 0x00000004;
  // Host has DHCP assigned IP address
  static const uint32_t XNET_GET_XNADDR_DHCP = 0x00000008;
  // Host has PPPoE assigned IP address
  static const uint32_t XNET_GET_XNADDR_PPPOE = 0x00000010;
  // Host has one or more gateways configured
  static const uint32_t XNET_GET_XNADDR_GATEWAY = 0x00000020;
  // Host has one or more DNS servers configured
  static const uint32_t XNET_GET_XNADDR_DNS = 0x00000040;
  // Host is currently connected to online service
  static const uint32_t XNET_GET_XNADDR_ONLINE = 0x00000080;
  // Network configuration requires troubleshooting
  static const uint32_t XNET_GET_XNADDR_TROUBLESHOOT = 0x00008000;
};

u32 NetDll_XNetGetTitleXnAddr_entry(u32 caller, ppc_ptr_t<XNADDR> addr_ptr) {
  // Default (shipping) behavior: report loopback with NO online presence, so
  // the game stays quietly offline and doesn't try to reach EA Nation (which
  // dead-ends at EA's console-cert auth). Our own netplay is the online path.
  // The EA-Nation experiment path below only runs under skate3_xnet_report_online.
  if (!REXCVAR_GET(skate3_xnet_report_online)) {
    addr_ptr->ina.s_addr = htonl(INADDR_LOOPBACK);
    addr_ptr->inaOnline.s_addr = 0;
    addr_ptr->wPortOnline = 0;
    std::memset(addr_ptr->abEnet, 0xCC, 6);   // non-zero MAC (RakNet needs it).
    std::memset(addr_ptr->abOnline, 0, 20);
    return XnAddrStatus::XNET_GET_XNADDR_STATIC;
  }

  // [skate3-online] EXPERIMENTAL: report a REAL online presence so the game
  // stops polling "am I online?" and proceeds to actually connect to EA Nation.
  //   ina         = this machine's real local IPv4 (a bindable local interface;
  //                 must NOT be a remote/VPS address or the game may fail to
  //                 bind a socket to it).
  //   inaOnline   = our home PUBLIC IP (skate3_xnet_online_ip cvar) = this
  //                 console's address as the outside world sees it. Falls back
  //                 to the local IP when the cvar is empty/unparseable.
  //   wPortOnline = a non-zero online port (assigned as a plain host value; the
  //                 guest reads XNADDR big-endian, so a small value is fine).
  //   status now includes XNET_GET_XNADDR_ONLINE so the poll resolves "online".
  const uint32_t local_ip = PrimaryLocalIPv4NBO();
  uint32_t online_ip = local_ip;
  const std::string online_ip_str =
      rex::cvar::Query<std::string>("skate3_xnet_online_ip");
  if (!online_ip_str.empty()) {
    const uint32_t parsed = inet_addr(online_ip_str.c_str());
    if (parsed != INADDR_NONE) {
      online_ip = parsed;  // network byte order.
    }
  }
  addr_ptr->ina.s_addr = local_ip;
  addr_ptr->inaOnline.s_addr = online_ip;
  addr_ptr->wPortOnline = 34643;  // plain value; guest reads big-endian.

  // TODO(gibbed): A proper mac address.
  // RakNet's 360 version appears to depend on abEnet to create "random" 64-bit
  // numbers. A zero value will cause RakPeer::Startup to fail.
  std::memset(addr_ptr->abEnet, 0xCC, 6);
  std::memset(addr_ptr->abOnline, 0, 20);

  // Rate-limited: the game may poll this many times; log the first call and
  // then sparsely, so the EA-connection packets we care about aren't buried.
  static uint32_t s_xnaddr_calls = 0;
  if (PacketLogEnabled() && (s_xnaddr_calls++ % 500) == 0) {
    const uint8_t* lb = reinterpret_cast<const uint8_t*>(&local_ip);
    const uint8_t* ob = reinterpret_cast<const uint8_t*>(&online_ip);
    REXKRNL_INFO(
        "[net-pkt] XNetGetTitleXnAddr caller={} #{} local={}.{}.{}.{} "
        "online={}.{}.{}.{}:34643 status=STATIC|GATEWAY|DNS|ONLINE",
        caller, s_xnaddr_calls, lb[0], lb[1], lb[2], lb[3],
        ob[0], ob[1], ob[2], ob[3]);
  }

  return XnAddrStatus::XNET_GET_XNADDR_STATIC |
         XnAddrStatus::XNET_GET_XNADDR_GATEWAY |
         XnAddrStatus::XNET_GET_XNADDR_DNS |
         XnAddrStatus::XNET_GET_XNADDR_ONLINE;
}

u32 NetDll_XNetGetDebugXnAddr_entry(u32 caller, ppc_ptr_t<XNADDR> addr_ptr) {
  addr_ptr.Zero();

  // XNET_GET_XNADDR_NONE causes caller to gracefully return.
  return XnAddrStatus::XNET_GET_XNADDR_NONE;
}

u32 NetDll_XNetXnAddrToMachineId_entry(u32 caller, ppc_ptr_t<XNADDR> addr_ptr, mapped_u32 id_ptr) {
  // Tell the caller we're not signed in to live (non-zero ret)
  return 1;
}

void NetDll_XNetInAddrToString_entry(u32 caller, u32 in_addr, mapped_string string_out,
                                     u32 string_size) {
  rex::string::rex_strcpy(string_out, string_size, "666.666.666.666");
}

// [skate3-online v2] Parse skate3_blaze_server_ip to a network-byte-order IPv4;
// fall back to loopback. inet_addr already returns network byte order.
static uint32_t BlazeRedirectNBO() {
  const std::string ip = rex::cvar::Query<std::string>("skate3_blaze_server_ip");
  if (!ip.empty()) {
    const uint32_t parsed = inet_addr(ip.c_str());
    if (parsed != INADDR_NONE) {
      return parsed;
    }
  }
  return htonl(INADDR_LOOPBACK);
}

// This converts a XNet address to an IN_ADDR. The IN_ADDR is used for
// subsequent socket calls (like a handle to a XNet address)
u32 NetDll_XNetXnAddrToInAddr_entry(u32 caller, ppc_ptr_t<XNADDR> xn_addr, mapped_void xid,
                                    mapped_void in_addr) {
  // [skate3-online v2] REDIRECT: when we're online-for-real, resolve every XNet
  // address to our own Blaze server IP so the game connects to US, not EA. The
  // original stub returned 1 = failure, which is what made the EA client give up
  // with "EA server is not available" before ever opening a socket.
  if (REXCVAR_GET(skate3_xnet_report_online)) {
    const uint32_t redirect = BlazeRedirectNBO();
    if (in_addr) {
      std::memcpy(in_addr.host_address(), &redirect, sizeof(redirect));
    }
    if (PacketLogEnabled()) {
      const uint8_t* rb = reinterpret_cast<const uint8_t*>(&redirect);
      REXKRNL_INFO("[net-pkt] XNetXnAddrToInAddr caller={} -> redirect={}.{}.{}.{} (ret=0)",
                   caller, rb[0], rb[1], rb[2], rb[3]);
    }
    return 0;  // success
  }
  return 1;
}

// [skate3-online v2] EA's Blaze client resolves the EA server address through
// this Xbox secure-server resolver -- NOT plain DNS (which is why the recon run
// logged zero DnsLookup calls). It was a bare stub, so the game got no address
// and showed "The EA server is not available" before opening any socket.
// Redirect it to our own Blaze server IP so the game proceeds to actually
// connect to US; our socket/connect logging then reveals the port + protocol.
// Args are logged RAW (hex) because XNetServerToInAddr has no prototype in-tree;
// the documented 360 ABI is (const IN_ADDR ina, DWORD dwServiceId, IN_ADDR* pina)
// under this SDK's leading-`caller` convention -- the log confirms/corrects it.
u32 NetDll_XNetServerToInAddr_entry(u32 caller, u32 server_ina, u32 service_id,
                                    mapped_void in_addr) {
  if (!REXCVAR_GET(skate3_xnet_report_online)) {
    return 1;  // offline: behave like the old failing stub.
  }
  const uint32_t redirect = BlazeRedirectNBO();
  if (in_addr) {
    std::memcpy(in_addr.host_address(), &redirect, sizeof(redirect));
  }
  if (PacketLogEnabled()) {
    const uint8_t* rb = reinterpret_cast<const uint8_t*>(&redirect);
    REXKRNL_INFO(
        "[net-pkt] XNetServerToInAddr caller={} arg_ina={:#x} service_id={:#x} "
        "out_ptr={:#x} -> redirect={}.{}.{}.{} (ret=0)",
        caller, server_ina, service_id, in_addr.guest_address(),
        rb[0], rb[1], rb[2], rb[3]);
  }
  return 0;  // success
}

// Does the reverse of the above.
// FIXME: Arguments may not be correct.
u32 NetDll_XNetInAddrToXnAddr_entry(u32 caller, mapped_void in_addr, ppc_ptr_t<XNADDR> xn_addr,
                                    mapped_void xid) {
  return 1;
}

// https://www.google.com/patents/WO2008112448A1?cl=en
// Reserves a port for use by system link
u32 NetDll_XNetSetSystemLinkPort_entry(u32 caller, u32 port) {
  return 1;
}

// [skate3-online v2] XNet secure-connection status values (XNetGetConnectStatus).
struct XConnectStatus {
  static const uint32_t XNET_CONNECT_STATUS_IDLE = 0;
  static const uint32_t XNET_CONNECT_STATUS_PENDING = 1;
  static const uint32_t XNET_CONNECT_STATUS_CONNECTED = 2;
  static const uint32_t XNET_CONNECT_STATUS_LOST = 3;
};

// [skate3-online v2] After GetServiceInfo (xlivebase_app.cpp msg 0x58007) hands
// the game our server address, the EA client calls XNetConnect(ina) to bring up
// a "secure" link, then polls XNetGetConnectStatus until CONNECTED. Both were
// stubs, so the game spun on XNetGetConnectStatus forever (6000+ polls) and never
// opened a socket. We don't implement real XNet secure networking -- just report
// the link established so the game proceeds to open its game socket and speak
// Blaze to our server (which our socket/connect/sendto logging then reveals).
u32 NetDll_XNetConnect_entry(u32 caller, u32 ina) {
  if (PacketLogEnabled()) {
    const uint8_t* ab = reinterpret_cast<const uint8_t*>(&ina);
    REXKRNL_INFO("[net-pkt] XNetConnect caller={} ina={}.{}.{}.{} -> ok",
                 caller, ab[0], ab[1], ab[2], ab[3]);
  }
  return 0;  // success -- connection initiated.
}

u32 NetDll_XNetGetConnectStatus_entry(u32 caller, u32 ina) {
  if (REXCVAR_GET(skate3_xnet_report_online)) {
    static uint32_t s_calls = 0;
    if (PacketLogEnabled() && (s_calls++ % 500) == 0) {
      REXKRNL_INFO("[net-pkt] XNetGetConnectStatus caller={} #{} -> CONNECTED", caller, s_calls);
    }
    return XConnectStatus::XNET_CONNECT_STATUS_CONNECTED;
  }
  return XConnectStatus::XNET_CONNECT_STATUS_IDLE;
}

// https://github.com/ILOVEPIE/Cxbx-Reloaded/blob/master/src/CxbxKrnl/EmuXOnline.h#L39
struct XEthernetStatus {
  static const uint32_t XNET_ETHERNET_LINK_ACTIVE = 0x01;
  static const uint32_t XNET_ETHERNET_LINK_100MBPS = 0x02;
  static const uint32_t XNET_ETHERNET_LINK_10MBPS = 0x04;
  static const uint32_t XNET_ETHERNET_LINK_FULL_DUPLEX = 0x08;
  static const uint32_t XNET_ETHERNET_LINK_HALF_DUPLEX = 0x10;
};

u32 NetDll_XNetGetEthernetLinkStatus_entry(u32 caller) {
  // [skate3-online v2] When we're reporting a real online presence (Blaze
  // revival path), the game's "do I have an internet connection?" gate checks
  // this. Returning 0 (no link) makes it show "you need an internet connection"
  // even though XNetGetTitleXnAddr says ONLINE. Report an ACTIVE 100Mbps
  // full-duplex link so that gate passes. Shipping behavior (cvar OFF) is
  // unchanged: report no link so the game stays quietly offline on our relay.
  if (REXCVAR_GET(skate3_xnet_report_online)) {
    return XEthernetStatus::XNET_ETHERNET_LINK_ACTIVE |
           XEthernetStatus::XNET_ETHERNET_LINK_100MBPS |
           XEthernetStatus::XNET_ETHERNET_LINK_FULL_DUPLEX;
  }
  return 0;
}

#if REX_PLATFORM_WIN32
// [skate3-online v2] Dump every committed guest-memory region to a file, once.
// Records are [u32 guest_addr][u32 size][size bytes]. Uses VirtualQuery so
// unmapped/guard pages are skipped (no crash). For offline Blaze protocol RE.
static void DumpGuestMemoryOnce() {
  static bool done = false;
  if (done) return;
  done = true;
  uint8_t* base = REX_KERNEL_MEMORY()->virtual_membase();
  const char* path = "C:\\Users\\James\\Desktop\\Skate3-Blaze-Server\\guestmem.bin";
  FILE* f = std::fopen(path, "wb");
  if (!f) {
    REXKRNL_ERROR("[memdump] cannot open {}", path);
    return;
  }
  const uint64_t END = 0x100000000ULL;  // 4 GiB guest address space
  const DWORD readmask = PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY |
                         PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
  uint64_t addr = 0, total = 0;
  uint32_t regions = 0;
  while (addr < END) {
    MEMORY_BASIC_INFORMATION mbi;
    if (VirtualQuery(base + addr, &mbi, sizeof(mbi)) == 0) break;
    uint64_t region_off = static_cast<uint64_t>(reinterpret_cast<uint8_t*>(mbi.BaseAddress) - base);
    uint64_t region_size = mbi.RegionSize;
    if (region_size == 0) break;
    bool readable = (mbi.State == MEM_COMMIT) && (mbi.Protect & readmask) &&
                    !(mbi.Protect & PAGE_GUARD) && region_off < END;
    if (readable) {
      uint64_t avail = END - region_off;
      uint32_t ga = static_cast<uint32_t>(region_off);
      uint32_t sz = static_cast<uint32_t>(region_size < avail ? region_size : avail);
      std::fwrite(&ga, 4, 1, f);
      std::fwrite(&sz, 4, 1, f);
      std::fwrite(base + region_off, 1, sz, f);
      total += sz;
      regions++;
    }
    addr = region_off + region_size;
  }
  std::fclose(f);
  REXKRNL_INFO("[memdump] wrote {} committed guest bytes in {} regions to {}", total, regions,
               path);
}
#endif

u32 NetDll_XNetDnsLookup_entry(u32 caller, mapped_string host, u32 event_handle, mapped_u32 pdns) {
#if REX_PLATFORM_WIN32
  if (REXCVAR_GET(skate3_dump_guestmem)) {
    DumpGuestMemoryOnce();
  }
#endif
  if (PacketLogEnabled()) {
    // `host` is a guest string; log whatever hostname the game asked for --
    // often EA / Xbox LIVE FQDNs (e.g. easo.ea.com, xboxlive.com, xhttp.msft.net).
    // Even though the lookup is stubbed, this reveals the game's INTENT.
    const char* h = host ? host.host_address() : "(null)";
    REXKRNL_INFO("[net-pkt] DnsLookup host='{}' event={}", h, event_handle);
  }
  if (pdns) {
    auto dns_guest = REX_KERNEL_MEMORY()->SystemHeapAlloc(sizeof(XNDNS));
    auto dns = REX_KERNEL_MEMORY()->TranslateVirtual<XNDNS*>(dns_guest);
    // [skate3-online v2] Resolve EVERY hostname to our own revival server IP so
    // the game's web/feed/EA services (e.g. 'skate3_web', downloads.skate.online.
    // ea.com) reach our server on the same box instead of failing (a failed
    // lookup made the game connect to 255.255.255.255 and show "EA server not
    // available"). Only when online-for-real; otherwise report a lookup failure.
    if (REXCVAR_GET(skate3_xnet_report_online)) {
      dns->status = 0;  // success
      dns->cina = 1;    // one address
      dns->aina[0].s_addr = BlazeRedirectNBO();  // our server IP (network order)
    } else {
      dns->status = 1;  // non-zero = error
      dns->cina = 0;
    }
    *pdns = dns_guest;
  }
  if (event_handle) {
    auto ev = REX_KERNEL_OBJECTS()->LookupObject<XEvent>(event_handle);
    assert_not_null(ev);
    ev->Set(0, false);
  }
  return 0;
}

u32 NetDll_XNetDnsRelease_entry(u32 caller, ppc_ptr_t<XNDNS> dns) {
  if (!dns) {
    return X_STATUS_INVALID_PARAMETER;
  }
  REX_KERNEL_MEMORY()->SystemHeapFree(dns.guest_address());
  return 0;
}

u32 NetDll_XNetQosServiceLookup_entry(u32 caller, u32 flags, u32 event_handle, mapped_u32 pqos) {
  // [skate3-online v2] Report a HEALTHY, COMPLETE QoS probe so the game believes
  // it has a good internet connection (open NAT, low latency, high bandwidth).
  // The old stub returned 0 probes -> the game concluded it had no usable internet
  // and fell back to "you have lost your connection to EA Nation".
  if (pqos) {
    auto qos_guest = REX_KERNEL_MEMORY()->SystemHeapAlloc(sizeof(XNQOS));
    auto qos = REX_KERNEL_MEMORY()->TranslateVirtual<XNQOS*>(qos_guest);
    qos->count = 1;
    qos->count_pending = 0;  // 0 pending == fully complete
    XNQOSINFO& q = qos->info[0];
    q.flags = 0x0B;  // COMPLETE | TARGET_CONTACTED | DATA_RECEIVED
    q.reserved = 0;
    q.probes_xmit = 4;
    q.probes_recv = 4;  // all probes answered
    q.data_len = 0;
    q.data_ptr = 0;
    q.rtt_min_in_msecs = 10;
    q.rtt_med_in_msecs = 15;
    q.up_bits_per_sec = 10000000;    // 10 Mbps up
    q.down_bits_per_sec = 10000000;  // 10 Mbps down
    *pqos = qos_guest;
  }
  if (event_handle) {
    auto ev = REX_KERNEL_OBJECTS()->LookupObject<XEvent>(event_handle);
    assert_not_null(ev);
    ev->Set(0, false);
  }
  return 0;
}

u32 NetDll_XNetQosRelease_entry(u32 caller, ppc_ptr_t<XNQOS> qos) {
  if (!qos) {
    return X_STATUS_INVALID_PARAMETER;
  }
  REX_KERNEL_MEMORY()->SystemHeapFree(qos.guest_address());
  return 0;
}

u32 NetDll_XNetQosListen_entry(u32 caller, mapped_void id, mapped_void data, u32 data_size, u32 r7,
                               u32 flags) {
  // [skate3-online v2] Report success so the game can register as a QoS host
  // (was FUNCTION_FAILED, which could make the game consider itself unable to
  // host / verify connectivity). No real UDP QoS listener is set up yet.
  return 0;
}

u32 NetDll_inet_addr_entry(mapped_string addr_ptr) {
  if (!addr_ptr) {
    return -1;
  }

  uint32_t addr = inet_addr(addr_ptr);
  // https://docs.microsoft.com/en-us/windows/win32/api/winsock2/nf-winsock2-inet_addr#return-value
  // Based on console research it seems like x360 uses old version of inet_addr
  // In case of empty string it return 0 instead of -1
  if (addr == -1 && !addr_ptr.value().length()) {
    return 0;
  }

  return rex::byte_swap(addr);
}

u32 NetDll_socket_entry(u32 caller, u32 af, u32 type, u32 protocol) {
  XSocket* socket = new XSocket(REX_KERNEL_STATE());
  X_STATUS result =
      socket->Initialize(XSocket::AddressFamily((uint32_t)af), XSocket::Type((uint32_t)type),
                         XSocket::Protocol((uint32_t)protocol));

  if (XFAILED(result)) {
    socket->Release();

    uint32_t error = xboxkrnl::xeRtlNtStatusToDosError(result);
    XThread::SetLastError(error);
    if (PacketLogEnabled()) {
      REXKRNL_INFO("[net-pkt] socket af={} type={} proto={} -> FAILED err={:#x}",
                   af, type, protocol, error);
    }
    return -1;
  }

  if (PacketLogEnabled()) {
    REXKRNL_INFO("[net-pkt] socket af={} type={} proto={} -> handle={}",
                 af, type, protocol, socket->handle());
  }
  return socket->handle();
}

u32 NetDll_closesocket_entry(u32 caller, u32 socket_handle) {
  auto socket = REX_KERNEL_OBJECTS()->LookupObject<XSocket>(socket_handle);
  if (!socket) {
    // WSAENOTSOCK
    XThread::SetLastError(0x2736);
    return -1;
  }

  if (PacketLogEnabled()) {
    REXKRNL_INFO("[net-pkt] closesocket sock={}", socket_handle);
  }
  // TODO: Absolutely delete this object. It is no longer valid after calling
  // closesocket.
  socket->Close();
  socket->ReleaseHandle();
  return 0;
}

i32 NetDll_shutdown_entry(u32 caller, u32 socket_handle, i32 how) {
  auto socket = REX_KERNEL_OBJECTS()->LookupObject<XSocket>(socket_handle);
  if (!socket) {
    // WSAENOTSOCK
    XThread::SetLastError(0x2736);
    return -1;
  }

  if (PacketLogEnabled()) {
    REXKRNL_INFO("[net-pkt] shutdown sock={} how={}", socket_handle, how);
  }
  auto ret = socket->Shutdown(how);
  if (ret == -1) {
#if REX_PLATFORM_WIN32
    uint32_t error_code = WSAGetLastError();
    XThread::SetLastError(error_code);
#else
    XThread::SetLastError(0x0);
#endif
  }
  return ret;
}

u32 NetDll_setsockopt_entry(u32 caller, u32 socket_handle, u32 level, u32 optname,
                            mapped_void optval_ptr, u32 optlen) {
  auto socket = REX_KERNEL_OBJECTS()->LookupObject<XSocket>(socket_handle);
  if (!socket) {
    // WSAENOTSOCK
    XThread::SetLastError(0x2736);
    return -1;
  }

  if (PacketLogEnabled()) {
    REXKRNL_INFO("[net-pkt] setsockopt sock={} level={:#x} optname={:#x} optlen={}",
                 socket_handle, level, optname, optlen);
  }
  X_STATUS status = socket->SetOption(level, optname, optval_ptr, optlen);
  return XSUCCEEDED(status) ? 0 : -1;
}

u32 NetDll_ioctlsocket_entry(u32 caller, u32 socket_handle, u32 cmd, mapped_void arg_ptr) {
  auto socket = REX_KERNEL_OBJECTS()->LookupObject<XSocket>(socket_handle);
  if (!socket) {
    // WSAENOTSOCK
    XThread::SetLastError(0x2736);
    return -1;
  }

  if (PacketLogEnabled()) {
    REXKRNL_INFO("[net-pkt] ioctlsocket sock={} cmd={:#x}", socket_handle, cmd);
  }
  X_STATUS status = socket->IOControl(cmd, arg_ptr);
  if (XFAILED(status)) {
    XThread::SetLastError(xboxkrnl::xeRtlNtStatusToDosError(status));
    return -1;
  }

  // TODO
  return 0;
}

u32 NetDll_bind_entry(u32 caller, u32 socket_handle, ppc_ptr_t<XSOCKADDR_IN> name, u32 namelen) {
  auto socket = REX_KERNEL_OBJECTS()->LookupObject<XSocket>(socket_handle);
  if (!socket) {
    // WSAENOTSOCK
    XThread::SetLastError(0x2736);
    return -1;
  }

  N_XSOCKADDR_IN native_name(name);
  if (PacketLogEnabled()) {
    char addr[48];
    FormatSockaddr(&native_name, addr, sizeof(addr));
    REXKRNL_INFO("[net-pkt] bind sock={} local={}", socket_handle, addr);
  }
  X_STATUS status = socket->Bind(&native_name, namelen);
  if (XFAILED(status)) {
    XThread::SetLastError(xboxkrnl::xeRtlNtStatusToDosError(status));
    return -1;
  }

  return 0;
}

u32 NetDll_connect_entry(u32 caller, u32 socket_handle, ppc_ptr_t<XSOCKADDR> name, u32 namelen) {
  auto socket = REX_KERNEL_OBJECTS()->LookupObject<XSocket>(socket_handle);
  if (!socket) {
    // WSAENOTSOCK
    XThread::SetLastError(0x2736);
    return -1;
  }

  N_XSOCKADDR native_name(name);
  if (PacketLogEnabled()) {
    // XSOCKADDR is generic; cast to sockaddr_in-shaped for logging (family+port+addr layout).
    char dst[48];
    FormatSockaddr(reinterpret_cast<const N_XSOCKADDR_IN*>(&native_name), dst, sizeof(dst));
    REXKRNL_INFO("[net-pkt] connect sock={} dst={}", socket_handle, dst);
  }
  X_STATUS status = socket->Connect(&native_name, namelen);
  if (XFAILED(status)) {
    // [skate3-online v2] The game sets the socket non-blocking (FIONBIO) before
    // connect, so a native connect returns WSAEWOULDBLOCK/EINPROGRESS = "in
    // progress, poll via select()". The default X_STATUS mapping turned that
    // into generic error 0x1f (ERROR_GEN_FAILURE), which DirtySDK read as a HARD
    // failure and aborted -- even though select() then reported the socket
    // writable and getpeername succeeded. Report the guest's WSAEWOULDBLOCK
    // (0x2733 = 10035) for the in-progress case so the client polls + proceeds.
#if REX_PLATFORM_WIN32
    int nerr = WSAGetLastError();
    if (nerr == WSAEWOULDBLOCK || nerr == WSAEINPROGRESS || nerr == WSAEALREADY) {
      XThread::SetLastError(0x2733);  // WSAEWOULDBLOCK
      if (PacketLogEnabled()) {
        REXKRNL_INFO("[net-pkt] connect sock={} -> in-progress (WSAEWOULDBLOCK)", socket_handle);
      }
      return -1;
    }
#endif
    XThread::SetLastError(xboxkrnl::xeRtlNtStatusToDosError(status));
    return -1;
  }

  return 0;
}

// [skate3-online v2] getpeername/getsockname were stubs. EA's client calls
// getpeername right after connect() to validate the connection; a stubbed
// failure made it give up and close the socket having sent 0 bytes. Implement
// both against the real underlying host socket so the connected peer/local
// address come back correctly and the client proceeds to send its Blaze data.
u32 NetDll_getpeername_entry(u32 caller, u32 socket_handle, ppc_ptr_t<XSOCKADDR_IN> name,
                             mapped_u32 namelen) {
  auto socket = REX_KERNEL_OBJECTS()->LookupObject<XSocket>(socket_handle);
  if (!socket) {
    XThread::SetLastError(0x2736);  // WSAENOTSOCK
    return -1;
  }
  sockaddr_in peer = {};
  peer.sin_family = AF_INET;
#if REX_PLATFORM_WIN32
  int plen = static_cast<int>(sizeof(peer));
  int ret = ::getpeername(static_cast<SOCKET>(socket->native_handle()),
                          reinterpret_cast<sockaddr*>(&peer), &plen);
#else
  socklen_t plen = sizeof(peer);
  int ret = ::getpeername(static_cast<int>(socket->native_handle()),
                          reinterpret_cast<sockaddr*>(&peer), &plen);
#endif
  if (ret != 0) {
    XThread::SetLastError(0x2749);  // WSAENOTCONN
    return -1;
  }
  if (name) {
    // Guest XSOCKADDR_IN = [be16 family][be16 port net-order][be32 addr net-order]
    // (see xsocket.h: sin_port/sin_addr are "Always big-endian!"). winsock's
    // sin_port/sin_addr are ALREADY network order -> copy raw. (StoreSockaddr
    // store_and_swaps them, which double-reverses network-order bytes and made
    // the guest see the peer as 1.0.0.127:<reversed> -> EA client rejected it.)
    uint8_t* p = reinterpret_cast<uint8_t*>(name.host_address());
    memory::store_and_swap<uint16_t>(p + 0, 2 /* AF_INET */);
    std::memcpy(p + 2, &peer.sin_port, 2);
    std::memcpy(p + 4, &peer.sin_addr.s_addr, 4);
  }
  if (namelen) {
    *namelen = 16u;  // guest sockaddr_in size
  }
  if (PacketLogEnabled()) {
    const uint8_t* ab = reinterpret_cast<const uint8_t*>(&peer.sin_addr);
    REXKRNL_INFO("[net-pkt] getpeername sock={} peer={}.{}.{}.{}:{}", socket_handle, ab[0], ab[1],
                 ab[2], ab[3], static_cast<uint32_t>(ntohs(peer.sin_port)));
  }
  return 0;
}

u32 NetDll_getsockname_entry(u32 caller, u32 socket_handle, ppc_ptr_t<XSOCKADDR_IN> name,
                             mapped_u32 namelen) {
  auto socket = REX_KERNEL_OBJECTS()->LookupObject<XSocket>(socket_handle);
  if (!socket) {
    XThread::SetLastError(0x2736);  // WSAENOTSOCK
    return -1;
  }
  sockaddr_in local = {};
  local.sin_family = AF_INET;
#if REX_PLATFORM_WIN32
  int llen = static_cast<int>(sizeof(local));
  int ret = ::getsockname(static_cast<SOCKET>(socket->native_handle()),
                          reinterpret_cast<sockaddr*>(&local), &llen);
#else
  socklen_t llen = sizeof(local);
  int ret = ::getsockname(static_cast<int>(socket->native_handle()),
                          reinterpret_cast<sockaddr*>(&local), &llen);
#endif
  if (ret != 0) {
    XThread::SetLastError(0x2749);  // WSAENOTCONN
    return -1;
  }
  if (name) {
    // Same guest XSOCKADDR_IN layout as getpeername -- raw-copy the already-
    // network-order port/addr; only the family needs host->big-endian.
    uint8_t* p = reinterpret_cast<uint8_t*>(name.host_address());
    memory::store_and_swap<uint16_t>(p + 0, 2 /* AF_INET */);
    std::memcpy(p + 2, &local.sin_port, 2);
    std::memcpy(p + 4, &local.sin_addr.s_addr, 4);
  }
  if (namelen) {
    *namelen = 16u;
  }
  if (PacketLogEnabled()) {
    const uint8_t* ab = reinterpret_cast<const uint8_t*>(&local.sin_addr);
    REXKRNL_INFO("[net-pkt] getsockname sock={} local={}.{}.{}.{}:{}", socket_handle, ab[0], ab[1],
                 ab[2], ab[3], static_cast<uint32_t>(ntohs(local.sin_port)));
  }
  return 0;
}

// [skate3-online v2] getsockopt was a stub. After connect, DirtySDK typically
// calls getsockopt(SOL_SOCKET, SO_ERROR) to check the connect result; a stubbed
// garbage return would make it treat the (successful) connection as failed.
// Report success with a zeroed value (SO_ERROR = 0 = "no error"). Logged so we
// can see exactly which option is queried and extend if a real value is needed.
u32 NetDll_getsockopt_entry(u32 caller, u32 socket_handle, u32 level, u32 optname,
                            mapped_void optval, mapped_u32 optlen) {
  auto socket = REX_KERNEL_OBJECTS()->LookupObject<XSocket>(socket_handle);
  if (!socket) {
    XThread::SetLastError(0x2736);  // WSAENOTSOCK
    return -1;
  }
  if (optval) {
    memory::store_and_swap<uint32_t>(reinterpret_cast<uint8_t*>(optval.host_address()), 0);
  }
  if (optlen) {
    *optlen = 4u;
  }
  if (PacketLogEnabled()) {
    REXKRNL_INFO("[net-pkt] getsockopt sock={} level={:#x} optname={:#x} -> 0 (success)",
                 socket_handle, level, optname);
  }
  return 0;
}

u32 NetDll_listen_entry(u32 caller, u32 socket_handle, i32 backlog) {
  auto socket = REX_KERNEL_OBJECTS()->LookupObject<XSocket>(socket_handle);
  if (!socket) {
    // WSAENOTSOCK
    XThread::SetLastError(0x2736);
    return -1;
  }

  X_STATUS status = socket->Listen(backlog);
  if (XFAILED(status)) {
    XThread::SetLastError(xboxkrnl::xeRtlNtStatusToDosError(status));
    return -1;
  }

  return 0;
}

u32 NetDll_accept_entry(u32 caller, u32 socket_handle, ppc_ptr_t<XSOCKADDR> addr_ptr,
                        mapped_u32 addrlen_ptr) {
  if (!addr_ptr) {
    // WSAEFAULT
    XThread::SetLastError(0x271E);
    return -1;
  }

  auto socket = REX_KERNEL_OBJECTS()->LookupObject<XSocket>(socket_handle);
  if (!socket) {
    // WSAENOTSOCK
    XThread::SetLastError(0x2736);
    return -1;
  }

  N_XSOCKADDR native_addr(addr_ptr);
  int native_len = *addrlen_ptr;
  auto new_socket = socket->Accept(&native_addr, &native_len);
  if (new_socket) {
    addr_ptr->address_family = native_addr.address_family;
    std::memcpy(addr_ptr->sa_data, native_addr.sa_data, *addrlen_ptr - 2);
    *addrlen_ptr = native_len;

    return new_socket->handle();
  } else {
    return -1;
  }
}

struct x_fd_set {
  rex::be<uint32_t> fd_count;
  rex::be<uint32_t> fd_array[64];
};

struct host_set {
  uint32_t count;
  object_ref<XSocket> sockets[64];

  void Load(const x_fd_set* guest_set) {
    assert_true(guest_set->fd_count < 64);
    this->count = guest_set->fd_count;
    for (uint32_t i = 0; i < this->count; ++i) {
      auto socket_handle = static_cast<X_HANDLE>(guest_set->fd_array[i]);
      if (socket_handle == -1) {
        this->count = i;
        break;
      }
      // Convert from Xenia -> native
      auto socket = REX_KERNEL_OBJECTS()->LookupObject<XSocket>(socket_handle);
      assert_not_null(socket);
      this->sockets[i] = socket;
    }
  }

  void Store(x_fd_set* guest_set) {
    guest_set->fd_count = 0;
    for (uint32_t i = 0; i < this->count; ++i) {
      auto socket = this->sockets[i];
      guest_set->fd_array[guest_set->fd_count++] = socket->handle();
    }
  }

  void Store(fd_set* native_set) {
    FD_ZERO(native_set);
    for (uint32_t i = 0; i < this->count; ++i) {
      FD_SET(this->sockets[i]->native_handle(), native_set);
    }
  }

  void UpdateFrom(fd_set* native_set) {
    uint32_t new_count = 0;
    for (uint32_t i = 0; i < this->count; ++i) {
      auto socket = this->sockets[i];
      if (FD_ISSET(socket->native_handle(), native_set)) {
        this->sockets[new_count++] = socket;
      }
    }
    this->count = new_count;
  }
};

i32 NetDll_select_entry(i32 caller, i32 nfds, ppc_ptr_t<x_fd_set> readfds,
                        ppc_ptr_t<x_fd_set> writefds, ppc_ptr_t<x_fd_set> exceptfds,
                        mapped_void timeout_ptr) {
  host_set host_readfds = {0};
  fd_set native_readfds = {0};
  if (readfds) {
    host_readfds.Load(readfds);
    host_readfds.Store(&native_readfds);
  }
  host_set host_writefds = {0};
  fd_set native_writefds = {0};
  if (writefds) {
    host_writefds.Load(writefds);
    host_writefds.Store(&native_writefds);
  }
  host_set host_exceptfds = {0};
  fd_set native_exceptfds = {0};
  if (exceptfds) {
    host_exceptfds.Load(exceptfds);
    host_exceptfds.Store(&native_exceptfds);
  }
  timeval* timeout_in = nullptr;
  timeval timeout;
  if (timeout_ptr) {
    timeout = {static_cast<int32_t>(timeout_ptr.as_array<int32_t>()[0]),
               static_cast<int32_t>(timeout_ptr.as_array<int32_t>()[1])};
    chrono::Clock::ScaleGuestDurationTimeval(reinterpret_cast<int32_t*>(&timeout.tv_sec),
                                             reinterpret_cast<int32_t*>(&timeout.tv_usec));
    timeout_in = &timeout;
  }
  int ret = select(nfds, readfds ? &native_readfds : nullptr, writefds ? &native_writefds : nullptr,
                   exceptfds ? &native_exceptfds : nullptr, timeout_in);
  if (readfds) {
    host_readfds.UpdateFrom(&native_readfds);
    host_readfds.Store(readfds);
  }
  if (writefds) {
    host_writefds.UpdateFrom(&native_writefds);
    host_writefds.Store(writefds);
  }
  if (exceptfds) {
    host_exceptfds.UpdateFrom(&native_exceptfds);
    host_exceptfds.Store(exceptfds);
  }

  if (PacketLogEnabled()) {
    REXKRNL_INFO("[net-pkt] select nfds={} r={} w={} e={} -> ret={}", nfds, readfds ? 1 : 0,
                 writefds ? 1 : 0, exceptfds ? 1 : 0, ret);
  }
  // TODO(gibbed): modify ret to be what's actually copied to the guest fd_sets?
  return ret;
}

u32 NetDll_recv_entry(u32 caller, u32 socket_handle, mapped_void buf_ptr, u32 buf_len, u32 flags) {
  auto socket = REX_KERNEL_OBJECTS()->LookupObject<XSocket>(socket_handle);
  if (!socket) {
    // WSAENOTSOCK
    XThread::SetLastError(0x2736);
    return -1;
  }

  int ret = socket->Recv(buf_ptr, buf_len, flags);
  if (PacketLogEnabled() && ret > 0) {
    char peek[64];
    FormatHexPeek(buf_ptr, static_cast<size_t>(ret), peek, sizeof(peek));
    REXKRNL_INFO("[net-pkt] recv sock={} len={} data=[{}]",
                 socket_handle, ret, peek);
  }
  // Propagate the socket error the same way recvfrom does. The Blaze / EA
  // Nation TCP read loop drains the socket until a recv "would block", then
  // checks WSAGetLastError expecting WSAEWOULDBLOCK (0x2733). Without this the
  // guest reads a stale last-error (observed: 0x80004005 E_FAIL) and treats the
  // connection as failed, tearing down the EA Nation session right after login.
  if (ret == -1) {
#if REX_PLATFORM_WIN32
    XThread::SetLastError(WSAGetLastError());
#else
    XThread::SetLastError(0x0);
#endif
  }
  return ret;
}

u32 NetDll_recvfrom_entry(u32 caller, u32 socket_handle, mapped_void buf_ptr, u32 buf_len,
                          u32 flags, ppc_ptr_t<XSOCKADDR_IN> from_ptr, mapped_u32 fromlen_ptr) {
  auto socket = REX_KERNEL_OBJECTS()->LookupObject<XSocket>(socket_handle);
  if (!socket) {
    // WSAENOTSOCK
    XThread::SetLastError(0x2736);
    return -1;
  }

  N_XSOCKADDR_IN native_from;
  if (from_ptr) {
    native_from = *from_ptr;
  }
  uint32_t native_fromlen = fromlen_ptr ? fromlen_ptr.value() : 0;
  int ret =
      socket->RecvFrom(buf_ptr, buf_len, flags, &native_from, fromlen_ptr ? &native_fromlen : 0);

  if (from_ptr) {
    from_ptr->sin_family = native_from.sin_family;
    from_ptr->sin_port = native_from.sin_port;
    from_ptr->sin_addr = native_from.sin_addr;
    std::memset(from_ptr->x_sin_zero, 0, sizeof(from_ptr->x_sin_zero));
  }
  if (fromlen_ptr) {
    *fromlen_ptr = native_fromlen;
  }

  if (PacketLogEnabled() && ret > 0) {
    char src[48], peek[64];
    FormatSockaddr(&native_from, src, sizeof(src));
    FormatHexPeek(buf_ptr, static_cast<size_t>(ret), peek, sizeof(peek));
    REXKRNL_INFO("[net-pkt] recvfrom sock={} src={} len={} data=[{}]",
                 socket_handle, src, ret, peek);
  }
  if (ret == -1) {
// TODO: Better way of getting the error code
#if REX_PLATFORM_WIN32
    uint32_t error_code = WSAGetLastError();
    XThread::SetLastError(error_code);
#else
    XThread::SetLastError(0x0);
#endif
  }

  return ret;
}

u32 NetDll_send_entry(u32 caller, u32 socket_handle, mapped_void buf_ptr, u32 buf_len, u32 flags) {
  auto socket = REX_KERNEL_OBJECTS()->LookupObject<XSocket>(socket_handle);
  if (!socket) {
    // WSAENOTSOCK
    XThread::SetLastError(0x2736);
    return -1;
  }

  if (PacketLogEnabled()) {
    char peek[64];
    FormatHexPeek(buf_ptr, static_cast<size_t>(buf_len), peek, sizeof(peek));
    REXKRNL_INFO("[net-pkt] send sock={} len={} data=[{}]",
                 socket_handle, buf_len, peek);
  }
  return socket->Send(buf_ptr, buf_len, flags);
}

u32 NetDll_sendto_entry(u32 caller, u32 socket_handle, mapped_void buf_ptr, u32 buf_len, u32 flags,
                        ppc_ptr_t<XSOCKADDR_IN> to_ptr, u32 to_len) {
  auto socket = REX_KERNEL_OBJECTS()->LookupObject<XSocket>(socket_handle);
  if (!socket) {
    // WSAENOTSOCK
    XThread::SetLastError(0x2736);
    return -1;
  }

  N_XSOCKADDR_IN native_to(to_ptr);
  if (PacketLogEnabled()) {
    char dst[48], peek[64];
    FormatSockaddr(&native_to, dst, sizeof(dst));
    FormatHexPeek(buf_ptr, static_cast<size_t>(buf_len), peek, sizeof(peek));
    REXKRNL_INFO("[net-pkt] sendto sock={} dst={} len={} data=[{}]",
                 socket_handle, dst, buf_len, peek);
  }
  return socket->SendTo(buf_ptr, buf_len, flags, &native_to, to_len);
}

// [skate3-online] XNetLogonGetTitleID: was a stub returning 0, causing the
// game to abort its XLive activation before any packets were built. Returns
// Skate 3's Xbox LIVE title ID = 0x454108E6 (same value seen in the save
// folder path <XUID>/454108E6/... produced by the game itself). No args; the
// title ID is a compile-time property of the xex.
u32 XNetLogonGetTitleID_entry() {
  return 0x454108E6u;
}

u32 NetDll___WSAFDIsSet_entry(u32 socket_handle, ppc_ptr_t<x_fd_set> fd_set) {
  const uint8_t max_fd_count = std::min((uint32_t)fd_set->fd_count, uint32_t(64));
  for (uint8_t i = 0; i < max_fd_count; i++) {
    if (fd_set->fd_array[i] == socket_handle) {
      return 1;
    }
  }
  return 0;
}

void NetDll_WSASetLastError_entry(u32 error_code) {
  XThread::SetLastError(error_code);
}

}  // namespace xam
}  // namespace kernel
}  // namespace rex

REX_EXPORT(__imp__NetDll_XNetStartup, rex::kernel::xam::NetDll_XNetStartup_entry)
REX_EXPORT(__imp__NetDll_XNetCleanup, rex::kernel::xam::NetDll_XNetCleanup_entry)
REX_EXPORT(__imp__NetDll_XNetGetOpt, rex::kernel::xam::NetDll_XNetGetOpt_entry)
REX_EXPORT(__imp__NetDll_XNetRandom, rex::kernel::xam::NetDll_XNetRandom_entry)
REX_EXPORT(__imp__NetDll_WSAStartup, rex::kernel::xam::NetDll_WSAStartup_entry)
REX_EXPORT(__imp__NetDll_WSACleanup, rex::kernel::xam::NetDll_WSACleanup_entry)
REX_EXPORT(__imp__NetDll_WSAGetLastError, rex::kernel::xam::NetDll_WSAGetLastError_entry)
REX_EXPORT(__imp__NetDll_WSARecvFrom, rex::kernel::xam::NetDll_WSARecvFrom_entry)
REX_EXPORT(__imp__NetDll_WSASendTo, rex::kernel::xam::NetDll_WSASendTo_entry)
REX_EXPORT(__imp__NetDll_WSAWaitForMultipleEvents,
           rex::kernel::xam::NetDll_WSAWaitForMultipleEvents_entry)
REX_EXPORT(__imp__NetDll_WSACreateEvent, rex::kernel::xam::NetDll_WSACreateEvent_entry)
REX_EXPORT(__imp__NetDll_WSACloseEvent, rex::kernel::xam::NetDll_WSACloseEvent_entry)
REX_EXPORT(__imp__NetDll_WSAResetEvent, rex::kernel::xam::NetDll_WSAResetEvent_entry)
REX_EXPORT(__imp__NetDll_WSASetEvent, rex::kernel::xam::NetDll_WSASetEvent_entry)
REX_EXPORT(__imp__NetDll_XNetGetTitleXnAddr, rex::kernel::xam::NetDll_XNetGetTitleXnAddr_entry)
REX_EXPORT(__imp__NetDll_XNetGetDebugXnAddr, rex::kernel::xam::NetDll_XNetGetDebugXnAddr_entry)
REX_EXPORT(__imp__NetDll_XNetXnAddrToMachineId,
           rex::kernel::xam::NetDll_XNetXnAddrToMachineId_entry)
REX_EXPORT(__imp__NetDll_XNetInAddrToString, rex::kernel::xam::NetDll_XNetInAddrToString_entry)
REX_EXPORT(__imp__NetDll_XNetXnAddrToInAddr, rex::kernel::xam::NetDll_XNetXnAddrToInAddr_entry)
REX_EXPORT(__imp__NetDll_XNetInAddrToXnAddr, rex::kernel::xam::NetDll_XNetInAddrToXnAddr_entry)
REX_EXPORT(__imp__NetDll_XNetSetSystemLinkPort,
           rex::kernel::xam::NetDll_XNetSetSystemLinkPort_entry)
REX_EXPORT(__imp__NetDll_XNetGetEthernetLinkStatus,
           rex::kernel::xam::NetDll_XNetGetEthernetLinkStatus_entry)
REX_EXPORT(__imp__NetDll_XNetDnsLookup, rex::kernel::xam::NetDll_XNetDnsLookup_entry)
REX_EXPORT(__imp__NetDll_XNetDnsRelease, rex::kernel::xam::NetDll_XNetDnsRelease_entry)
REX_EXPORT(__imp__NetDll_XNetQosServiceLookup, rex::kernel::xam::NetDll_XNetQosServiceLookup_entry)
REX_EXPORT(__imp__NetDll_XNetQosRelease, rex::kernel::xam::NetDll_XNetQosRelease_entry)
REX_EXPORT(__imp__NetDll_XNetQosListen, rex::kernel::xam::NetDll_XNetQosListen_entry)
REX_EXPORT(__imp__NetDll_inet_addr, rex::kernel::xam::NetDll_inet_addr_entry)
REX_EXPORT(__imp__NetDll_socket, rex::kernel::xam::NetDll_socket_entry)
REX_EXPORT(__imp__NetDll_closesocket, rex::kernel::xam::NetDll_closesocket_entry)
REX_EXPORT(__imp__NetDll_shutdown, rex::kernel::xam::NetDll_shutdown_entry)
REX_EXPORT(__imp__NetDll_setsockopt, rex::kernel::xam::NetDll_setsockopt_entry)
REX_EXPORT(__imp__NetDll_ioctlsocket, rex::kernel::xam::NetDll_ioctlsocket_entry)
REX_EXPORT(__imp__NetDll_bind, rex::kernel::xam::NetDll_bind_entry)
REX_EXPORT(__imp__NetDll_connect, rex::kernel::xam::NetDll_connect_entry)
REX_EXPORT(__imp__NetDll_listen, rex::kernel::xam::NetDll_listen_entry)
REX_EXPORT(__imp__NetDll_accept, rex::kernel::xam::NetDll_accept_entry)
REX_EXPORT(__imp__NetDll_select, rex::kernel::xam::NetDll_select_entry)
REX_EXPORT(__imp__NetDll_recv, rex::kernel::xam::NetDll_recv_entry)
REX_EXPORT(__imp__NetDll_recvfrom, rex::kernel::xam::NetDll_recvfrom_entry)
REX_EXPORT(__imp__NetDll_send, rex::kernel::xam::NetDll_send_entry)
REX_EXPORT(__imp__NetDll_sendto, rex::kernel::xam::NetDll_sendto_entry)
REX_EXPORT(__imp__NetDll___WSAFDIsSet, rex::kernel::xam::NetDll___WSAFDIsSet_entry)
REX_EXPORT(__imp__NetDll_WSASetLastError, rex::kernel::xam::NetDll_WSASetLastError_entry)
// [skate3-online] Real XNetLogonGetTitleID (was REX_EXPORT_STUB in xam_misc.cpp).
REX_EXPORT(__imp__XNetLogonGetTitleID, rex::kernel::xam::XNetLogonGetTitleID_entry)

REX_EXPORT_STUB(__imp__NetDll_UpnpActionCalculateWorkBufferSize);
REX_EXPORT_STUB(__imp__NetDll_UpnpActionCreate);
REX_EXPORT_STUB(__imp__NetDll_UpnpActionGetResults);
REX_EXPORT_STUB(__imp__NetDll_UpnpCleanup);
REX_EXPORT_STUB(__imp__NetDll_UpnpCloseHandle);
REX_EXPORT_STUB(__imp__NetDll_UpnpDescribeCreate);
REX_EXPORT_STUB(__imp__NetDll_UpnpDescribeGetResults);
REX_EXPORT_STUB(__imp__NetDll_UpnpDoWork);
REX_EXPORT_STUB(__imp__NetDll_UpnpEventCreate);
REX_EXPORT_STUB(__imp__NetDll_UpnpEventGetCurrentState);
REX_EXPORT_STUB(__imp__NetDll_UpnpEventUnsubscribe);
REX_EXPORT_STUB(__imp__NetDll_UpnpSearchCreate);
REX_EXPORT_STUB(__imp__NetDll_UpnpSearchGetDevices);
REX_EXPORT_STUB(__imp__NetDll_UpnpStartup);
REX_EXPORT_STUB(__imp__NetDll_WSACancelOverlappedIO);
REX_EXPORT_STUB(__imp__NetDll_WSAEventSelect);
REX_EXPORT_STUB(__imp__NetDll_WSAGetOverlappedResult);
REX_EXPORT_STUB(__imp__NetDll_WSARecv);
REX_EXPORT_STUB(__imp__NetDll_WSASend);
REX_EXPORT_STUB(__imp__NetDll_WSAStartupEx);
REX_EXPORT_STUB(__imp__NetDll_XHttpCloseHandle);
REX_EXPORT_STUB(__imp__NetDll_XHttpConnect);
REX_EXPORT_STUB(__imp__NetDll_XHttpCrackUrl);
REX_EXPORT_STUB(__imp__NetDll_XHttpCrackUrlW);
REX_EXPORT_STUB(__imp__NetDll_XHttpCreateUrl);
REX_EXPORT_STUB(__imp__NetDll_XHttpCreateUrlW);
REX_EXPORT_STUB(__imp__NetDll_XHttpDoWork);
REX_EXPORT_STUB(__imp__NetDll_XHttpGetPerfCounters);
REX_EXPORT_STUB(__imp__NetDll_XHttpOpen);
REX_EXPORT_STUB(__imp__NetDll_XHttpOpenRequest);
REX_EXPORT_STUB(__imp__NetDll_XHttpOpenRequestUsingMemory);
REX_EXPORT_STUB(__imp__NetDll_XHttpQueryAuthSchemes);
REX_EXPORT_STUB(__imp__NetDll_XHttpQueryHeaders);
REX_EXPORT_STUB(__imp__NetDll_XHttpQueryOption);
REX_EXPORT_STUB(__imp__NetDll_XHttpReadData);
REX_EXPORT_STUB(__imp__NetDll_XHttpReceiveResponse);
REX_EXPORT_STUB(__imp__NetDll_XHttpResetPerfCounters);
REX_EXPORT_STUB(__imp__NetDll_XHttpSendRequest);
REX_EXPORT_STUB(__imp__NetDll_XHttpSetCredentials);
REX_EXPORT_STUB(__imp__NetDll_XHttpSetOption);
REX_EXPORT_STUB(__imp__NetDll_XHttpSetStatusCallback);
REX_EXPORT_STUB(__imp__NetDll_XHttpShutdown);
REX_EXPORT_STUB(__imp__NetDll_XHttpStartup);
REX_EXPORT_STUB(__imp__NetDll_XHttpWriteData);
REX_EXPORT(__imp__NetDll_XNetConnect, rex::kernel::xam::NetDll_XNetConnect_entry)
REX_EXPORT_STUB(__imp__NetDll_XNetCreateKey);
REX_EXPORT_STUB(__imp__NetDll_XNetDnsReverseLookup);
REX_EXPORT_STUB(__imp__NetDll_XNetDnsReverseRelease);
REX_EXPORT_STUB(__imp__NetDll_XNetGetBroadcastVersionStatus);
REX_EXPORT(__imp__NetDll_XNetGetConnectStatus, rex::kernel::xam::NetDll_XNetGetConnectStatus_entry)
REX_EXPORT_STUB(__imp__NetDll_XNetGetSystemLinkPort);
REX_EXPORT_STUB(__imp__NetDll_XNetGetXnAddrPlatform);
REX_EXPORT_STUB(__imp__NetDll_XNetInAddrToServer);
REX_EXPORT_STUB(__imp__NetDll_XNetQosGetListenStats);
REX_EXPORT_STUB(__imp__NetDll_XNetQosLookup);
REX_EXPORT_STUB(__imp__NetDll_XNetRegisterKey);
REX_EXPORT_STUB(__imp__NetDll_XNetReplaceKey);
REX_EXPORT(__imp__NetDll_XNetServerToInAddr, rex::kernel::xam::NetDll_XNetServerToInAddr_entry)
REX_EXPORT_STUB(__imp__NetDll_XNetSetOpt);
REX_EXPORT_STUB(__imp__NetDll_XNetStartupEx);
REX_EXPORT_STUB(__imp__NetDll_XNetTsAddrToInAddr);
REX_EXPORT_STUB(__imp__NetDll_XNetUnregisterInAddr);
REX_EXPORT_STUB(__imp__NetDll_XNetUnregisterKey);
REX_EXPORT_STUB(__imp__NetDll_XmlDownloadContinue);
REX_EXPORT_STUB(__imp__NetDll_XmlDownloadGetParseTime);
REX_EXPORT_STUB(__imp__NetDll_XmlDownloadGetReceivedDataSize);
REX_EXPORT_STUB(__imp__NetDll_XmlDownloadStart);
REX_EXPORT_STUB(__imp__NetDll_XmlDownloadStop);
REX_EXPORT_STUB(__imp__NetDll_XnpCapture);
REX_EXPORT_STUB(__imp__NetDll_XnpConfig);
REX_EXPORT_STUB(__imp__NetDll_XnpConfigUPnP);
REX_EXPORT_STUB(__imp__NetDll_XnpConfigUPnPPortAndExternalAddr);
REX_EXPORT_STUB(__imp__NetDll_XnpEthernetInterceptRecv);
REX_EXPORT_STUB(__imp__NetDll_XnpEthernetInterceptSetCallbacks);
REX_EXPORT_STUB(__imp__NetDll_XnpEthernetInterceptSetExtendedReceiveCallback);
REX_EXPORT_STUB(__imp__NetDll_XnpEthernetInterceptXmit);
REX_EXPORT_STUB(__imp__NetDll_XnpEthernetInterceptXmitAsIp);
REX_EXPORT_STUB(__imp__NetDll_XnpGetActiveSocketList);
REX_EXPORT_STUB(__imp__NetDll_XnpGetConfigStatus);
REX_EXPORT_STUB(__imp__NetDll_XnpGetKeyList);
REX_EXPORT_STUB(__imp__NetDll_XnpGetQosLookupList);
REX_EXPORT_STUB(__imp__NetDll_XnpGetSecAssocList);
REX_EXPORT_STUB(__imp__NetDll_XnpGetVlanXboxName);
REX_EXPORT_STUB(__imp__NetDll_XnpLoadConfigParams);
REX_EXPORT_STUB(__imp__NetDll_XnpLoadMachineAccount);
REX_EXPORT_STUB(__imp__NetDll_XnpLogonClearChallenge);
REX_EXPORT_STUB(__imp__NetDll_XnpLogonClearQEvent);
REX_EXPORT_STUB(__imp__NetDll_XnpLogonGetChallenge);
REX_EXPORT_STUB(__imp__NetDll_XnpLogonGetQFlags);
REX_EXPORT_STUB(__imp__NetDll_XnpLogonGetQVals);
REX_EXPORT_STUB(__imp__NetDll_XnpLogonGetStatus);
REX_EXPORT_STUB(__imp__NetDll_XnpLogonSetChallengeResponse);
REX_EXPORT_STUB(__imp__NetDll_XnpLogonSetPState);
REX_EXPORT_STUB(__imp__NetDll_XnpLogonSetQEvent);
REX_EXPORT_STUB(__imp__NetDll_XnpLogonSetQFlags);
REX_EXPORT_STUB(__imp__NetDll_XnpLogonSetQVals);
REX_EXPORT_STUB(__imp__NetDll_XnpNoteSystemTime);
REX_EXPORT_STUB(__imp__NetDll_XnpPersistTitleState);
REX_EXPORT_STUB(__imp__NetDll_XnpQosHistoryGetAggregateMeasurement);
REX_EXPORT_STUB(__imp__NetDll_XnpQosHistoryGetEntries);
REX_EXPORT_STUB(__imp__NetDll_XnpQosHistoryLoad);
REX_EXPORT_STUB(__imp__NetDll_XnpQosHistorySaveMeasurements);
REX_EXPORT_STUB(__imp__NetDll_XnpRegisterKeyForCallerType);
REX_EXPORT_STUB(__imp__NetDll_XnpReplaceKeyForCallerType);
REX_EXPORT_STUB(__imp__NetDll_XnpSaveConfigParams);
REX_EXPORT_STUB(__imp__NetDll_XnpSaveMachineAccount);
REX_EXPORT_STUB(__imp__NetDll_XnpSetVlanXboxName);
REX_EXPORT_STUB(__imp__NetDll_XnpToolIpProxyInject);
REX_EXPORT_STUB(__imp__NetDll_XnpToolSetCallbacks);
REX_EXPORT_STUB(__imp__NetDll_XnpUnregisterKeyForCallerType);
REX_EXPORT_STUB(__imp__NetDll_XnpUpdateConfigParams);
REX_EXPORT(__imp__NetDll_getpeername, rex::kernel::xam::NetDll_getpeername_entry)
REX_EXPORT(__imp__NetDll_getsockname, rex::kernel::xam::NetDll_getsockname_entry)
REX_EXPORT(__imp__NetDll_getsockopt, rex::kernel::xam::NetDll_getsockopt_entry)
