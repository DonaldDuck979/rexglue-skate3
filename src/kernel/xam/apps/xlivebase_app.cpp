/**
 ******************************************************************************
 * Xenia : Xbox 360 Emulator Research Project                                 *
 ******************************************************************************
 * Copyright 2021 Ben Vanik. All rights reserved.                             *
 * Released under the BSD license - see LICENSE in the root for more details. *
 ******************************************************************************
 *
 * @modified    Tom Clay, 2026 - Adapted for ReXGlue runtime
 */

#include <cstdio>
#include <cstring>

#include <rex/cvar.h>
#include <rex/kernel/xam/apps/xlivebase_app.h>
#include <rex/logging.h>
#include <rex/thread.h>

namespace rex {
namespace kernel {
namespace xam {
using namespace rex::system;
using namespace rex::system::xam;
namespace apps {
using namespace rex::system;

XLiveBaseApp::XLiveBaseApp(KernelState* kernel_state) : App(kernel_state, 0xFC) {}

// http://mb.mirage.org/bugzilla/xliveless/main.c

X_HRESULT XLiveBaseApp::DispatchMessageSync(uint32_t message, uint32_t buffer_ptr,
                                            uint32_t buffer_length) {
  // NOTE: buffer_length may be zero or valid.
  auto buffer = memory_->TranslateVirtual(buffer_ptr);
  // [skate3-online] Trace EVERY XLIVEBASE message the game sends, gated behind
  // the packet-log cvar (off by default; this was an EA-Nation investigation
  // aid). Enable with `skate3_net_packet_log 1` to see the full sequence.
  if (rex::cvar::Query<bool>("skate3_net_packet_log")) {
    REXKRNL_INFO("[xlive-trace] XLIVEBASE msg={:08X} buf={:08X} len={:08X}",
                 message, buffer_ptr, buffer_length);
  }
  switch (message) {
    case 0x00058004: {
      // Called on startup, seems to just return a bool in the buffer.
      assert_true(!buffer_length || buffer_length == 4);
      REXKRNL_DEBUG("XLiveBaseGetLogonId({:08X})", buffer_ptr);
      memory::store_and_swap<uint32_t>(buffer + 0, 1);  // ?
      return X_E_SUCCESS;
    }
    case 0x00058006: {
      assert_true(!buffer_length || buffer_length == 4);
      REXKRNL_DEBUG("XLiveBaseGetNatType({:08X})", buffer_ptr);
      memory::store_and_swap<uint32_t>(buffer + 0, 1);  // XONLINE_NAT_OPEN
      return X_E_SUCCESS;
    }
    case 0x00058007: {
      // [skate3-online v2] XOnlineGetServiceInfo -- the EA client's "where is the
      // EA Nation server?" call, and THE source of the "EA server is not
      // available" message (it used to hardcode 0x80151802 ERROR_CONNECTION_INVALID).
      // Observed args: arg1 (buffer_ptr) = dwServiceId VALUE (e.g. 0x45410004,
      // 0x4541 = 'EA'); arg2 (buffer_length) = guest pointer to the output
      // XONLINE_SERVICE_INFO { DWORD dwServiceId; IN_ADDR inaServer; WORD wPort;
      // WORD wReserved; } (12 bytes; big-endian, inaServer in network order).
      // Fill it with OUR redirect server (skate3_blaze_server_ip:_port) and
      // return SUCCESS so the game proceeds to connect to us. The output pointer
      // is range-guarded so a wrong-layout guess cannot corrupt guest memory.
      const uint32_t service_id = buffer_ptr;
      const uint32_t out_guest = buffer_length;

      unsigned a = 127, b = 0, c = 0, d = 1;
      const std::string ip = rex::cvar::Query<std::string>("skate3_blaze_server_ip");
      std::sscanf(ip.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d);
      const uint16_t port =
          static_cast<uint16_t>(rex::cvar::Query<uint32_t>("skate3_blaze_server_port"));

      bool wrote = false;
      if (out_guest >= 0x1000u && out_guest < 0xC0000000u) {
        auto out = memory_->TranslateVirtual(out_guest);
        if (out) {
          const uint8_t ipbytes[4] = {static_cast<uint8_t>(a), static_cast<uint8_t>(b),
                                      static_cast<uint8_t>(c), static_cast<uint8_t>(d)};
          memory::store_and_swap<uint32_t>(out + 0, service_id);  // dwServiceId
          std::memcpy(out + 4, ipbytes, 4);                       // inaServer (network order)
          memory::store_and_swap<uint16_t>(out + 8, port);        // wPort
          memory::store_and_swap<uint16_t>(out + 10, 0);          // wReserved
          wrote = true;
        }
      }
      if (rex::cvar::Query<bool>("skate3_net_packet_log")) {
        REXKRNL_INFO(
            "[xlive-trace] GetServiceInfo service={:08X} out_ptr={:08X} -> "
            "server={}.{}.{}.{}:{} wrote={} (SUCCESS)",
            service_id, out_guest, a, b, c, d, port, wrote ? 1 : 0);
      }
      return X_E_SUCCESS;
    }
    case 0x00058009: {
      // [skate3-online] Was returning "Unimplemented" and blocking Skate 3's
      // XLive activation upstream of the socket layer. Neighboring messages
      // in the 0x58000-0x58020 range are XLive Logon service selectors
      // (GetLogonId=0x58004, GetNatType=0x58006, GetServiceInfo=0x58007,
      // Enumerate=0x58020). 0x58009 hasn't been publicly identified but
      // the game supplies a 16-byte output buffer -- zero-fill it and return
      // SUCCESS so the game proceeds; if it later checks specific fields
      // we'll see that as the next stub firing and iterate. Matches the
      // permissive-success pattern of 0x58046 already in this file.
      REXKRNL_DEBUG("XLiveBase58009({:08X}, {:08X}) -> SUCCESS/empty",
                    buffer_ptr, buffer_length);
      if (buffer && buffer_length > 0) {
        std::memset(buffer, 0, buffer_length);
      }
      return X_E_SUCCESS;
    }
    case 0x00058020: {
      // 0x00058004 is called right before this.
      // We should create a XamEnumerate-able empty list here, but I'm not
      // sure of the format.
      // buffer_length seems to be the same ptr sent to 0x00058004.
      REXKRNL_DEBUG("CXLiveFriends::Enumerate({:08X}, {:08X}) unimplemented", buffer_ptr,
                    buffer_length);
      return X_E_FAIL;
    }
    case 0x00058023: {
      REXKRNL_DEBUG(
          "CXLiveMessaging::XMessageGameInviteGetAcceptedInfo({:08X}, {:08X}) "
          "unimplemented",
          buffer_ptr, buffer_length);
      return X_E_FAIL;
    }
    case 0x00058046: {
      // Required to be successful for 4D530910 to detect signed-in profile
      // Doesn't seem to set anything in the given buffer, probably only takes
      // input
      REXKRNL_DEBUG("XLiveBaseUnk58046({:08X}, {:08X}) unimplemented", buffer_ptr, buffer_length);
      return X_E_SUCCESS;
    }
  }
  REXKRNL_ERROR(
      "Unimplemented XLIVEBASE message app={:08X}, msg={:08X}, arg1={:08X}, "
      "arg2={:08X}",
      app_id(), message, buffer_ptr, buffer_length);
  return X_E_FAIL;
}

}  // namespace apps
}  // namespace xam
}  // namespace kernel
}  // namespace rex
