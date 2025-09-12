#pragma once
#include <functional>
#include <vector>
#include <span>
#include <cstddef>

namespace arkan::relay::application::ports {

struct IHook {
  using Bytes = std::vector<std::byte>;
  using Callback = std::function<void(const Bytes&)>;

  virtual ~IHook() = default;
  virtual bool install() = 0;      // install send/recv hooks
  virtual void uninstall() = 0;    // remove hooks

  // Register callbacks triggered on client's send/recv
  virtual void on_send(Callback cb) = 0;
  virtual void on_recv(Callback cb) = 0;
};

} // namespace arkan::relay::application::ports
