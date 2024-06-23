#include "apf.h"
#include "die.h"

#include <string>
#include <unordered_set>

#include <absl/flags/flag.h>
#include <absl/flags/parse.h>
#include <absl/flags/usage.h>
#include <absl/strings/numbers.h>
#include <absl/strings/str_format.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/epoll.h>
#include <sys/socket.h>

ABSL_FLAG(std::string, mei_device, "/dev/mei0", "Path to the MEI chardev");
ABSL_FLAG(std::vector<std::string>, allowed_ports,
          (std::vector<std::string>{"16992", "16993"}), "Which ports to forward");
ABSL_FLAG(std::string, listen_addr, "127.0.0.1", "Address to listen on");

namespace amt {
namespace {

sockaddr *sa_ptr(sockaddr_in &sa) { return reinterpret_cast<sockaddr *>(&sa); }
sockaddr *sa_ptr(sockaddr_storage &sa) { return reinterpret_cast<sockaddr *>(&sa); }

void epoll_ctl_add(int epfd, int fd, uint32_t events) {
  struct epoll_event ev;
  ev.events = events;
  ev.data.fd = fd;
  int err = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
  die_if(err == -1, "epoll_ctl_add errno=%d", errno);
}

void epoll_ctl_del(int epfd, int fd) {
  int err = epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
  die_if(err == -1, "epoll_ctl_DEL errno=%d", errno);
}

class Apfd {
public:
  Apfd() : apf_(absl::GetFlag(FLAGS_mei_device)) {
    for (const auto &p : absl::GetFlag(FLAGS_allowed_ports)) {
      uint32_t port = 0;
      bool success = absl::SimpleAtoi(p, &port);
      if (!success || port > 65535) {
        die("invalid port %s", p);
      }
      allowed_ports_.insert(port);
    }
  }

  int Run() {
    epoll_fd_ = epoll_create(1);
    die_if(epoll_fd_ < 0, "epoll_create errno=%d", errno);
    epoll_ctl_add(epoll_fd_, apf_.fd(), EPOLLIN);

    while (true) {
      epoll_event events[1024];
      int event_count = epoll_wait(epoll_fd_, events, 1024, -1);
      die_if(event_count == -1, "epoll_wait errno=%d", errno);
      for (int i = 0; i < event_count; i++) {
        int fd = events[i].data.fd;

        if (fd == apf_.fd()) {
          HandleMeRequest(apf_.ProcessOneMessage());
        } else if (auto it = listen_fd_port_.find(fd); it != listen_fd_port_.end()) {
          HandleIncomingConnection(fd);
        } else if (auto it = channel_fd_id_.find(fd); it != channel_fd_id_.end()) {
          auto it2 = channels_.find(it->second);
          die_if(it2 == channels_.end(), "inconsistent state");
          if (events[i].events & EPOLLIN) {
            HandleFdToApfData(/*is_fd=*/true, it2->second);
          }
          if (events[i].events & EPOLLOUT) {
            HandleApfToFdData(/*is_fd=*/true, it2->second);
          }
          if (events[i].events & (EPOLLHUP | EPOLLRDHUP)) {
            HandleChannelClosure(/*is_fd=*/true, it2->second);
          }
        } else {
          die("impossible");
        }
      }
    }

    return 0;
  }

private:
  struct ChannelInfo {
    int fd;
    uint32_t channel_id;
    // Waiting for SendDataCompletion
    bool apf_blocked;
    // Has incoming data from APF.
    bool apf_incoming;
  };

  void HandleIncomingConnection(int listen_fd) {
    sockaddr_storage ss{};
    socklen_t sslen = sizeof(ss);
    char buf[4096];

    int client_fd = accept4(listen_fd, sa_ptr(ss), &sslen, SOCK_NONBLOCK);
    die_if(client_fd < 0, "accept errno=%d", errno);
    die_if(ss.ss_family != AF_INET, "bad family");

    // get peer ip and port
    auto *sa = reinterpret_cast<const sockaddr_in *>(&ss);
    const char *ret = inet_ntop(ss.ss_family, &sa->sin_addr, buf, 4096);
    die_if(ret == nullptr, "inet_ntop");
    std::string peer_ip = buf;
    uint32_t peer_port = ntohs(sa->sin_port);

    uint32_t listen_port = listen_fd_port_[listen_fd];
    uint32_t channel_id = apf_.OpenChannel(peer_port, listen_port);
    channel_fd_id_[client_fd] = channel_id;
    channels_[channel_id] = ChannelInfo{
        .fd = client_fd,
        .channel_id = channel_id,
    };

    absl::PrintF("Incoming %s:%u fd=%d\n", peer_ip, peer_port, client_fd);
    // Don't start poll the fd, wait for OpenChannelResult
  }

  void BeginListen(uint32_t port) {
    sockaddr_in listen_sa{};
    int err;

    int fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    die_if(fd < 0, "socket creation fail");

    listen_sa.sin_family = AF_INET;
    listen_sa.sin_port = htons(port);
    err = inet_aton(absl::GetFlag(FLAGS_listen_addr).c_str(), &listen_sa.sin_addr);
    die_if(err == 0, "inet_aton");

    err = bind(fd, sa_ptr(listen_sa), sizeof(listen_sa));
    die_if(err == -1, "bind");

    err = listen(fd, 4096);
    die_if(err == -1, "listen");

    listen_fd_port_[fd] = port;
    epoll_ctl_add(epoll_fd_, fd, EPOLLIN);
  }

  void HandleMeRequest(AmtPortForwarding::MeRequest req) {
    if (!req.has_value()) {
      return;
    }
    if (const auto *fwd_req = std::get_if<AmtPortForwarding::RequestTcpForward>(&*req)) {
      if (allowed_ports_.find(fwd_req->port) == allowed_ports_.end()) {
        absl::PrintF("Rejected: %s:%u\n", fwd_req->addr, fwd_req->port);
        fwd_req->reject();
        return;
      }

      for (auto it : listen_fd_port_) {
        if (it.second == fwd_req->port) {
          absl::PrintF("Already listening on port %u\n", fwd_req->port);
          fwd_req->reject();
          return;
        }
      }

      BeginListen(fwd_req->port);
      fwd_req->accept();
      absl::PrintF("Accept: %s:%u\n", fwd_req->addr, fwd_req->port);
    } else if (const auto *open_result =
                   std::get_if<AmtPortForwarding::OpenChannelResult>(&*req)) {
      auto it = channels_.find(open_result->channel_id);
      if (it == channels_.end()) {
        absl::PrintF("unexpected OpenChannelResult channel=%u\n",
                     open_result->channel_id);
        return;
      }

      if (!open_result->success) {
        absl::PrintF("OpenChannel failed channel=%u\n", open_result->channel_id);
        channel_fd_id_.erase(it->second.fd);
        close(it->second.fd);
        channels_.erase(it);
        return;
      }

      epoll_ctl_add(epoll_fd_, it->second.fd,
                    EPOLLIN | EPOLLOUT | EPOLLHUP | EPOLLRDHUP | EPOLLET);
      absl::PrintF("Accepting data on channel %u\n", open_result->channel_id);
    } else if (const auto *apf_data =
                   std::get_if<AmtPortForwarding::IncomingData>(&*req)) {
      auto it = channels_.find(apf_data->channel_id);
      if (it == channels_.end()) {
        absl::PrintF("unexpected data on channel=%u\n", apf_data->channel_id);
        return;
      }
      HandleApfToFdData(/*is_fd=*/false, it->second);
    } else if (const auto *comp =
                   std::get_if<AmtPortForwarding::SendDataCompletion>(&*req)) {
      auto it = channels_.find(comp->channel_id);
      if (it == channels_.end()) {
        absl::PrintF("unexpected completion on channel=%u\n", comp->channel_id);
        return;
      }
      HandleFdToApfData(/*is_fd=*/false, it->second);
    } else if (const auto *closure =
                   std::get_if<AmtPortForwarding::ChannelClosed>(&*req)) {
      auto it = channels_.find(closure->channel_id);
      if (it == channels_.end()) {
        // absl::PrintF("unexpected closure on channel=%u\n", closure->channel_id);
        return;
      }
      HandleChannelClosure(/*is_fd=*/false, it->second);
    } else if (std::get_if<AmtPortForwarding::MeDisconnect>(&*req)) {
      die("ME disconnects");
    } else {
      die("Unexpected variant");
    }
  }

  void HandleFdToApfData(bool is_fd, ChannelInfo &channel) {
    if (is_fd && channel.apf_blocked) {
      // Can't do much
      return;
    }

    // Either APF is unblocked or new data arrives.
    uint8_t buf[4096];
    int r = read(channel.fd, buf, 4096);
    if (r < 0 && errno == EAGAIN) {
      if (!is_fd)
        channel.apf_blocked = false;
      return;
    }
    if (r == 0) {
      if (!is_fd)
        channel.apf_blocked = false;
      absl::PrintF("EOF fd=%d\n", channel.fd);
      return;
    }
    die_if(r < 0, "read err r=%d errno=%d", r, errno);

    apf_.SendData(channel.channel_id, absl::MakeConstSpan(buf, r));
    channel.apf_blocked = true;
  }

  void HandleApfToFdData(bool is_fd, ChannelInfo &channel) {
    if (is_fd && !channel.apf_incoming) {
      // nothing to do.
      return;
    }

    // is_fd && apf_incoming || IncomingData event received.
    const std::string *data = apf_.PeekData(channel.channel_id);
    size_t off = 0;
    size_t rem = data->size();
    while (rem > 0) {
      ssize_t written = write(channel.fd, data->data() + off, rem);
      if (written < 0 && errno == EAGAIN) { // fd blocked.
        break;
      }
      die_if(written <= 0, "write");
      off += written;
      rem -= written;
    }
    channel.apf_incoming = rem > 0;
    apf_.PopData(channel.channel_id, off);
  }

  // If the closure is initiated by FD, we'll receive another request later by APF.
  // But if it's initiated by APF, it will only be called once.
  // TODO graceful shutdown
  void HandleChannelClosure(bool is_fd, ChannelInfo &channel) {
    epoll_ctl_del(epoll_fd_, channel.fd);
    close(channel.fd);
    apf_.CloseChannel(channel.channel_id);
    channel_fd_id_.erase(channel.fd);
    channels_.erase(channel.channel_id);
    return;
  }

  AmtPortForwarding apf_;
  std::unordered_set<uint32_t> allowed_ports_;
  // listen fd to listen port mapping
  std::unordered_map<int, uint32_t> listen_fd_port_;
  // key is channel id
  std::unordered_map<uint32_t, ChannelInfo> channels_;
  // map fd to channel id
  std::unordered_map<int, uint32_t> channel_fd_id_;

  int epoll_fd_;
};

} // namespace
} // namespace amt

int main(int argc, char *argv[]) {
  absl::SetProgramUsageMessage("Forwards TCP port via MEI");
  absl::ParseCommandLine(argc, argv);

  amt::Apfd apfd;
  return apfd.Run();
}
