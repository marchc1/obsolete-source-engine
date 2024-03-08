// Copyright Valve Corporation, All rights reserved.

#ifndef SRC_LAUNCHER_SCOPED_WINSOCK_H
#define SRC_LAUNCHER_SCOPED_WINSOCK_H

#include <sal.h>
#include <winsock.h>

#include <system_error>

#include "tier0/dbg.h"

namespace src::launcher {

// Scoped Windows sockets initializer.
class ScopedWinsock {
 public:
  ScopedWinsock(unsigned short version)
      : errc_{::WSAStartup(version, &wsa_data_)}, version_{version} {}

  [[nodiscard]] int errc() const noexcept { return errc_; }

  ScopedWinsock(const ScopedWinsock &) = delete;
  ScopedWinsock(ScopedWinsock &&) = delete;
  ScopedWinsock &operator=(const ScopedWinsock &) = delete;
  ScopedWinsock &operator=(ScopedWinsock &&) = delete;

  ~ScopedWinsock() noexcept {
    if (!errc_) {
      const int rc{::WSACleanup()};
      if (rc) {
        Warning("Windows sockets shutdown failure (%d): %s.\n", rc,
                std::system_category().message(rc).c_str());
      }
    }
  }

 private:
  WSAData wsa_data_;
  const int errc_;
  const unsigned short version_;
};

}  // namespace src::launcher

#endif  // SRC_LAUNCHER_SCOPED_WINSOCK_H
