#define PTP_TESTER_HOST_TEST
#define NOMINMAX
#define FAR

#include <winsock2.h>
#include <ws2tcpip.h>
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <deque>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "ptp_auth.cpp"
#include "ptp_tester.cpp"

#undef FAR
#define FAR
#undef interface

using socklen_t = int;
using ssize_t = std::ptrdiff_t;
struct esp_netif_t {};
struct ifreq { char ifr_name[16]; };
struct esp_netif_ip_info_t { struct { uint32_t addr; } ip; };
struct esp_ip6_addr_t { uint32_t addr[4]; };
constexpr int ESP_OK = 0;
constexpr int SO_BINDTODEVICE = 0x7f01;

namespace
{
    using namespace ptp_tester;
    constexpr int64_t Never = (std::numeric_limits<int64_t>::max)();
    constexpr uint8_t Domain = 7;
    constexpr char Target4[] = "192.0.2.20";
    constexpr char Target6[] = "fe80::20";
    constexpr char Local4[] = "192.0.2.10";
    constexpr int InterfaceIndex = 3;
    int assertions = 0;
    std::string last_evidence;
    esp_netif_t netif;
    std::atomic<bool> link_up{true}, stopped{false};
    std::atomic<uint32_t> generation{1};

    void check(bool condition, const char *description)
    {
        ++assertions;
        if (!condition)
            throw std::runtime_error(std::string(description) + "\n" + last_evidence);
    }

    struct SimPacket
    {
        std::vector<uint8_t> bytes;
        sockaddr_storage source{};
    };

    struct FakeSocket
    {
        int family = 0;
        uint16_t port = 0;
        bool open = true;
        bool interface_bound = false;
        bool group_bound = false;
        bool joined = false;
        bool outgoing_interface = false;
        bool ttl_one = false;
        bool loop_disabled = false;
        bool v6_only = false;
        std::deque<SimPacket> incoming;
    };

    struct Pending
    {
        int fd = 0;
        int64_t when = 0;
        SimPacket packet;
    };

    struct FakeNetwork
    {
        std::map<int, FakeSocket> sockets;
        std::vector<Pending> pending;
        std::vector<std::vector<uint8_t>> sent;
        int next_fd = 4;
        int64_t now = 1000000;
        int64_t next_announce[2] = {1010000, 1010000};
        int64_t next_sync[2] = {1020000, 1020000};
        uint16_t announce_sequence[2] = {0, 0};
        uint16_t sync_sequence[2] = {0, 0};
        bool service = true;
        bool fail_bind = false;
        bool fail_join = false;
        bool fail_select = false;
        bool fail_send = false;
        bool accept_invalid = false;
        bool mismatched_grandmaster = false;
        bool keep_service_on_fault = false;
        bool foreign_announce = false;
        bool wrong_response_sequence = false;
        bool wrong_response_requester = false;
        bool omit_follow_up = false;
        uint16_t response_port = 320;
        int64_t response_delay = 10000;
        int64_t identity_change_at = Never;
        int valid_requests = 0;
        int ignore_valid_from = 0;
        int start_callbacks = 0;
        int end_callbacks = 0;
        int fault_callbacks = 0;
        int restore_callbacks = 0;
        Test assisted_action = Test::Count;
        int64_t cancel_at = Never;
        int64_t link_change_at = Never;
    } sim;

    void reset()
    {
        sim = FakeNetwork{};
        link_up.store(true);
        stopped.store(false);
        generation.store(1);
    }

    int slot(int family) { return family == AF_INET ? 0 : 1; }

    void put16(std::vector<uint8_t> &bytes, size_t offset, uint16_t value)
    {
        bytes[offset] = static_cast<uint8_t>(value >> 8);
        bytes[offset + 1] = static_cast<uint8_t>(value);
    }

    void put_timestamp(std::vector<uint8_t> &bytes, size_t offset)
    {
        const uint64_t seconds = 1700000000;
        for (size_t index = 0; index < 6; ++index)
            bytes[offset + index] = static_cast<uint8_t>(seconds >> ((5 - index) * 8));
        bytes[offset + 6] = 0;
        bytes[offset + 7] = 0;
        bytes[offset + 8] = 0;
        bytes[offset + 9] = 5;
    }

    SimPacket message(int family, uint8_t type, uint16_t sequence)
    {
        const size_t size = type == 11 ? 64 : type == 9 ? 54 : 44;
        SimPacket packet;
        packet.bytes.resize(size);
        auto &bytes = packet.bytes;
        bytes[0] = type;
        bytes[1] = 2;
        put16(bytes, 2, static_cast<uint16_t>(size));
        bytes[4] = Domain;
        if (type == 0)
            bytes[6] = 2;
        for (size_t index = 0; index < 8; ++index)
            bytes[20 + index] = static_cast<uint8_t>(0x10 + index);
        if (sim.now >= sim.identity_change_at)
            bytes[20] ^= 0x40;
        bytes[29] = family == AF_INET ? 2 : 3;
        put16(bytes, 30, sequence);
        bytes[32] = type == 0 ? 0 : type == 8 ? 2 : type == 9 ? 3 : 5;
        if (type == 8 || type == 9)
            put_timestamp(bytes, 34);
        if (type == 11)
        {
            bytes[47] = 128;
            bytes[48] = 248;
            bytes[49] = 0xfe;
            bytes[50] = 0xff;
            bytes[51] = 0xff;
            bytes[52] = 128;
            for (size_t index = 0; index < 8; ++index)
                bytes[53 + index] = static_cast<uint8_t>(0x10 + index);
            if (family == AF_INET6 && sim.mismatched_grandmaster)
                bytes[53] ^= 0x40;
            bytes[63] = 0xa0;
        }
        if (family == AF_INET)
        {
            auto *address = reinterpret_cast<sockaddr_in *>(&packet.source);
            address->sin_family = AF_INET;
            address->sin_port = htons(type == 0 ? 319 : 320);
            InetPtonA(AF_INET, Target4, &address->sin_addr);
        }
        else
        {
            auto *address = reinterpret_cast<sockaddr_in6 *>(&packet.source);
            address->sin6_family = AF_INET6;
            address->sin6_port = htons(type == 0 ? 319 : 320);
            address->sin6_scope_id = InterfaceIndex;
            InetPtonA(AF_INET6, Target6, &address->sin6_addr);
        }
        return packet;
    }

    int find_socket(int family, uint16_t port)
    {
        for (const auto &entry : sim.sockets)
            if (entry.second.open && entry.second.family == family && entry.second.port == port && entry.second.joined)
                return entry.first;
        return -1;
    }

    void generate_due()
    {
        for (int family : {AF_INET, AF_INET6})
        {
            const int index = slot(family);
            const int event_fd = find_socket(family, 319);
            const int general_fd = find_socket(family, 320);
            if (!sim.service || event_fd < 0 || general_fd < 0)
                continue;
            if (sim.now >= sim.next_announce[index])
            {
                SimPacket announce = message(family, 11, ++sim.announce_sequence[index]);
                if (sim.foreign_announce)
                {
                    SimPacket foreign = announce;
                    foreign.bytes[20] ^= 0x20;
                    if (family == AF_INET)
                        InetPtonA(AF_INET, "192.0.2.21", &reinterpret_cast<sockaddr_in *>(&foreign.source)->sin_addr);
                    else
                        InetPtonA(AF_INET6, "fe80::21", &reinterpret_cast<sockaddr_in6 *>(&foreign.source)->sin6_addr);
                    sim.sockets[general_fd].incoming.push_back(foreign);
                }
                sim.sockets[general_fd].incoming.push_back(announce);
                sim.next_announce[index] = sim.now + 400000;
            }
            if (sim.now >= sim.next_sync[index])
            {
                const uint16_t sequence = ++sim.sync_sequence[index];
                sim.sockets[event_fd].incoming.push_back(message(family, 0, sequence));
                if (!sim.omit_follow_up)
                    sim.sockets[general_fd].incoming.push_back(message(family, 8, sequence));
                sim.next_sync[index] = sim.now + 200000;
            }
        }
        for (auto item = sim.pending.begin(); item != sim.pending.end();)
        {
            if (item->when <= sim.now)
            {
                if (sim.sockets[item->fd].open)
                    sim.sockets[item->fd].incoming.push_back(item->packet);
                item = sim.pending.erase(item);
            }
            else
                ++item;
        }
        if (sim.now >= sim.cancel_at)
            stopped.store(true);
        if (sim.now >= sim.link_change_at)
            generation.store(2);
    }

    int fake_socket(int family, int type, int protocol)
    {
        check((family == AF_INET || family == AF_INET6) && type == SOCK_DGRAM && protocol == IPPROTO_UDP,
              "only IPv4/IPv6 UDP sockets are created");
        const int fd = sim.next_fd++;
        sim.sockets[fd].family = family;
        return fd;
    }

    int fake_bind(int fd, const sockaddr *address, int)
    {
        if (sim.fail_bind)
        {
            errno = EADDRINUSE;
            return -1;
        }
        FakeSocket &socket = sim.sockets.at(fd);
        if (socket.family == AF_INET)
        {
            const auto &actual = *reinterpret_cast<const sockaddr_in *>(address);
            in_addr expected{};
            InetPtonA(AF_INET, "224.0.1.129", &expected);
            socket.group_bound = actual.sin_family == AF_INET &&
                                 std::memcmp(&actual.sin_addr, &expected, sizeof(expected)) == 0;
            socket.port = ntohs(actual.sin_port);
        }
        else
        {
            const auto &actual = *reinterpret_cast<const sockaddr_in6 *>(address);
            in6_addr expected{};
            InetPtonA(AF_INET6, "ff02::181", &expected);
            socket.group_bound = actual.sin6_family == AF_INET6 && actual.sin6_scope_id == InterfaceIndex &&
                                 std::memcmp(&actual.sin6_addr, &expected, sizeof(expected)) == 0;
            socket.port = ntohs(actual.sin6_port);
        }
        return 0;
    }

    int fake_setsockopt(int fd, int level, int option, const void *value, int length)
    {
        FakeSocket &socket = sim.sockets.at(fd);
        if (level == SOL_SOCKET && option == SO_BINDTODEVICE)
        {
            socket.interface_bound = length == sizeof(ifreq) &&
                                     std::strcmp(static_cast<const ifreq *>(value)->ifr_name, "en3") == 0;
            return 0;
        }
        if (level == IPPROTO_IPV6 && option == IPV6_V6ONLY)
        {
            socket.v6_only = length == sizeof(int) && *static_cast<const int *>(value) == 1;
            return 0;
        }
        const bool joining = (level == IPPROTO_IP && option == IP_ADD_MEMBERSHIP) ||
                             (level == IPPROTO_IPV6 && option == IPV6_JOIN_GROUP);
        if (joining && sim.fail_join)
        {
            errno = EADDRNOTAVAIL;
            return -1;
        }
        if (joining)
        {
            if (socket.family == AF_INET)
            {
                const auto &membership = *static_cast<const ip_mreq *>(value);
                in_addr group{}, local{};
                InetPtonA(AF_INET, "224.0.1.129", &group);
                InetPtonA(AF_INET, Local4, &local);
                socket.joined = length == sizeof(membership) &&
                                std::memcmp(&membership.imr_multiaddr, &group, sizeof(group)) == 0 &&
                                std::memcmp(&membership.imr_interface, &local, sizeof(local)) == 0;
            }
            else
            {
                const auto &membership = *static_cast<const ipv6_mreq *>(value);
                in6_addr group{};
                InetPtonA(AF_INET6, "ff02::181", &group);
                socket.joined = length == sizeof(membership) && membership.ipv6mr_interface == InterfaceIndex &&
                                std::memcmp(&membership.ipv6mr_multiaddr, &group, sizeof(group)) == 0;
            }
        }
        else if (level == IPPROTO_IP && option == IP_MULTICAST_IF)
        {
            in_addr local{};
            InetPtonA(AF_INET, Local4, &local);
            socket.outgoing_interface = length == sizeof(local) && std::memcmp(value, &local, sizeof(local)) == 0;
        }
        else if (level == IPPROTO_IPV6 && option == IPV6_MULTICAST_IF)
            socket.outgoing_interface = length == 1 && *static_cast<const uint8_t *>(value) == InterfaceIndex;
        else if ((level == IPPROTO_IP && option == IP_MULTICAST_TTL) ||
                 (level == IPPROTO_IPV6 && option == IPV6_MULTICAST_HOPS))
            socket.ttl_one = length == 1 && *static_cast<const uint8_t *>(value) == 1;
        else if ((level == IPPROTO_IP && option == IP_MULTICAST_LOOP) ||
                 (level == IPPROTO_IPV6 && option == IPV6_MULTICAST_LOOP))
            socket.loop_disabled = length == 1 && *static_cast<const uint8_t *>(value) == 0;
        else
            throw std::runtime_error("unexpected socket option");
        return 0;
    }

    int fake_select(int, fd_set *read, fd_set *, fd_set *, const timeval *timeout)
    {
        if (sim.fail_select)
        {
            errno = EIO;
            return -1;
        }
        std::vector<int> requested;
        for (u_int index = 0; index < read->fd_count; ++index)
            requested.push_back(static_cast<int>(read->fd_array[index]));
        const int64_t end = sim.now + static_cast<int64_t>(timeout->tv_sec) * 1000000 + timeout->tv_usec;
        while (true)
        {
            generate_due();
            FD_ZERO(read);
            int ready = 0;
            for (int fd : requested)
                if (!sim.sockets.at(fd).incoming.empty())
                {
                    FD_SET(static_cast<SOCKET>(fd), read);
                    ++ready;
                }
            if (ready || sim.now >= end)
                return ready;
            int64_t next = (std::min)({end, sim.cancel_at, sim.link_change_at});
            for (const Pending &pending : sim.pending)
                next = (std::min)(next, pending.when);
            if (sim.service)
                for (int family : {AF_INET, AF_INET6})
                    if (find_socket(family, 319) >= 0 && find_socket(family, 320) >= 0)
                    {
                        next = (std::min)(next, sim.next_announce[slot(family)]);
                        next = (std::min)(next, sim.next_sync[slot(family)]);
                    }
            sim.now = next > sim.now ? next : sim.now + 1;
        }
    }

    ssize_t fake_recvfrom(int fd, void *buffer, size_t capacity, int, sockaddr *source, socklen_t *source_length)
    {
        auto &queue = sim.sockets.at(fd).incoming;
        if (queue.empty())
        {
            errno = EAGAIN;
            return -1;
        }
        SimPacket packet = queue.front();
        queue.pop_front();
        const size_t length = (std::min)(capacity, packet.bytes.size());
        std::memcpy(buffer, packet.bytes.data(), length);
        if (source && source_length)
        {
            const size_t address_length = packet.source.ss_family == AF_INET ? sizeof(sockaddr_in) : sizeof(sockaddr_in6);
            std::memcpy(source, &packet.source, address_length);
            *source_length = static_cast<socklen_t>(address_length);
        }
        return static_cast<ssize_t>(length);
    }

    ssize_t fake_sendto(int fd, const void *buffer, size_t length, int, const sockaddr *destination, socklen_t)
    {
        if (sim.fail_send)
        {
            errno = EIO;
            return -1;
        }
        const FakeSocket &socket = sim.sockets.at(fd);
        check(socket.port == 319 && socket.group_bound && socket.interface_bound && socket.joined,
              "Delay_Req uses the multicast-bound event socket");
        if (socket.family == AF_INET)
        {
            const auto &address = *reinterpret_cast<const sockaddr_in *>(destination);
            in_addr expected{};
            InetPtonA(AF_INET, "224.0.1.129", &expected);
            check(address.sin_family == AF_INET && ntohs(address.sin_port) == 319 &&
                  std::memcmp(&address.sin_addr, &expected, sizeof(expected)) == 0,
                  "IPv4 Delay_Req destination is event multicast");
        }
        else
        {
            const auto &address = *reinterpret_cast<const sockaddr_in6 *>(destination);
            in6_addr expected{};
            InetPtonA(AF_INET6, "ff02::181", &expected);
            check(address.sin6_family == AF_INET6 && ntohs(address.sin6_port) == 319 &&
                  address.sin6_scope_id == InterfaceIndex &&
                  std::memcmp(&address.sin6_addr, &expected, sizeof(expected)) == 0,
                  "IPv6 Delay_Req destination is scoped event multicast");
        }
        const auto *bytes = static_cast<const uint8_t *>(buffer);
        sim.sent.emplace_back(bytes, bytes + length);
        const bool valid = length == 44 && bytes[0] == 1 && bytes[1] == 2 &&
                           bytes[2] == 0 && bytes[3] == 44 && bytes[4] == Domain && bytes[32] == 1 &&
                           std::any_of(bytes + 20, bytes + 28, [](uint8_t octet) { return octet != 0; }) &&
                           (bytes[28] != 0 || bytes[29] != 0);
        if (valid)
            ++sim.valid_requests;
        const bool reply = sim.service && ((valid && (!sim.ignore_valid_from || sim.valid_requests < sim.ignore_valid_from)) ||
                                           (!valid && sim.accept_invalid));
        if (reply && length >= 34)
        {
            const uint16_t sequence = static_cast<uint16_t>((bytes[30] << 8) | bytes[31]);
            SimPacket response = message(socket.family, 9, sequence);
            response.bytes[4] = bytes[4];
            std::memcpy(response.bytes.data() + 44, bytes + 20, 10);
            if (sim.wrong_response_sequence)
                response.bytes[31] ^= 1;
            if (sim.wrong_response_requester)
                response.bytes[44] ^= 1;
            if (socket.family == AF_INET)
                reinterpret_cast<sockaddr_in *>(&response.source)->sin_port = htons(sim.response_port);
            else
                reinterpret_cast<sockaddr_in6 *>(&response.source)->sin6_port = htons(sim.response_port);
            sim.pending.push_back({find_socket(socket.family, sim.response_port), sim.now + sim.response_delay,
                                   std::move(response)});
        }
        return static_cast<ssize_t>(length);
    }

    int fake_close(int fd)
    {
        sim.sockets.at(fd).open = false;
        return 0;
    }

    int esp_netif_get_netif_impl_name(esp_netif_t *, char *name)
    {
        std::memcpy(name, "en3", 4);
        return ESP_OK;
    }
    int esp_netif_get_netif_impl_index(esp_netif_t *) { return InterfaceIndex; }
    int esp_netif_get_ip_info(esp_netif_t *, esp_netif_ip_info_t *info)
    {
        in_addr address{};
        InetPtonA(AF_INET, Local4, &address);
        info->ip.addr = address.s_addr;
        return ESP_OK;
    }
    int esp_netif_get_ip6_linklocal(esp_netif_t *, esp_ip6_addr_t *) { return ESP_OK; }
    int esp_netif_get_mac(esp_netif_t *, uint8_t *mac)
    {
        const uint8_t local[6] = {2, 0x11, 0x22, 0x33, 0x44, 0x55};
        std::memcpy(mac, local, sizeof(local));
        return ESP_OK;
    }
    int64_t esp_timer_get_time() { return sim.now; }
    template <typename... Args> void fake_log(Args...) {}

    bool is_self_target(const char *text)
    {
        return std::strcmp(text, Local4) == 0 || std::strcmp(text, "fe80::10") == 0;
    }
#include "ptp_udp_validation.inc"
}

#define socket fake_socket
#define bind fake_bind
#define setsockopt fake_setsockopt
#define select fake_select
#define recvfrom fake_recvfrom
#define sendto fake_sendto
#define close fake_close
#define ESP_LOGE(...) fake_log(__VA_ARGS__)
#define ESP_LOGW(...) fake_log(__VA_ARGS__)

namespace ptp_tester
{
    namespace
    {
        constexpr uint32_t MaxTimeoutMs = 120000;
        uint64_t s_request_nonce = 0;
#include "ptp_udp_production.inc"
    }
}

#undef socket
#undef bind
#undef setsockopt
#undef select
#undef recvfrom
#undef sendto
#undef close

namespace
{
    void observation(bool starting, void *)
    {
        if (starting)
            ++sim.start_callbacks;
        else
            ++sim.end_callbacks;
    }

    bool assist(Test action, bool restoration, const std::atomic<bool> &, void *, AssistPoll, void *)
    {
        sim.assisted_action = action;
        if (restoration)
        {
            ++sim.restore_callbacks;
            sim.service = true;
            sim.next_announce[0] = sim.next_announce[1] = sim.now + 10000;
            sim.next_sync[0] = sim.next_sync[1] = sim.now + 20000;
        }
        else
        {
            ++sim.fault_callbacks;
            if (!sim.keep_service_on_fault)
                sim.service = false;
        }
        return true;
    }

    UdpConfig configuration(bool ipv4, bool ipv6)
    {
        UdpConfig config{};
        config.ipv4_target = Target4;
        config.ipv6_target = Target6;
        config.ipv4 = ipv4;
        config.ipv6 = ipv6;
        config.domain = Domain;
        config.timeout_ms = 1500;
        config.observation_ms = 1200;
        config.samples = 2;
        config.response_window_ms = 100;
        config.request_interval_ms = 100;
        config.withdrawal_ms = 1000;
        config.recovery_ms = 1500;
        config.netif = &netif;
        config.link_up = &link_up;
        config.link_generation = &generation;
        config.assist = assist;
        config.observation = observation;
        return config;
    }

    Result execute(UdpTest test, UdpConfig config, std::string &evidence)
    {
        std::array<char, 16384> output{};
        const Result result = run_udp_test(test, config, stopped, output.data(), output.size());
        evidence = output.data();
        last_evidence = evidence;
        for (const auto &socket : sim.sockets)
            check(!socket.second.open, "every opened multicast socket is closed");
        return result;
    }

    void check_socket_configuration(size_t expected)
    {
        check(sim.sockets.size() == expected, "expected event and general sockets opened");
        for (const auto &entry : sim.sockets)
        {
            const FakeSocket &socket = entry.second;
            check((socket.port == 319 || socket.port == 320) && socket.group_bound && socket.interface_bound &&
                      socket.joined && socket.outgoing_interface && socket.ttl_one && socket.loop_disabled &&
                      (socket.family == AF_INET || socket.v6_only),
                  "socket joins and binds multicast on Ethernet with one-hop scope and loopback disabled");
        }
    }

    void multicast_service()
    {
        std::string evidence;
        reset();
        check(execute(UdpTest::Announce, configuration(true, false), evidence) == Result::Pass,
              "IPv4 multicast Announce is observed");
        check_socket_configuration(2);
        check(evidence.find("Hardware timestamps") != std::string::npos, "measurement limitations appear in evidence");
        check(evidence.find("L3 UDP observer") != std::string::npos && evidence.find("L2 observer") == std::string::npos,
              "UDP evidence never mislabels its transport as L2");

        reset();
        sim.foreign_announce = true;
        check(execute(UdpTest::Announce, configuration(true, false), evidence) == Result::Pass,
              "multicast messages from other IP sources do not replace the configured master");
        check(evidence.find("selected multicast packets=1") != std::string::npos, "foreign-source traffic is excluded");

        reset();
        check(execute(UdpTest::SyncFollowUp, configuration(false, true), evidence) == Result::Pass,
              "IPv6 scoped Sync and Follow_Up pair successfully");
        check_socket_configuration(2);

        reset();
        check(execute(UdpTest::DelayExchange, configuration(false, true), evidence) == Result::Pass,
              "IPv6 general-port Delay_Resp matches requester and sequence");
        check(!sim.sent.empty() && sim.sent.front().size() == 44, "valid unsigned E2E Delay_Req was sent");
        reset();
        sim.response_port = 319;
        check(execute(UdpTest::DelayExchange, configuration(true, false), evidence) == Result::Pass,
              "event-port Delay_Resp also matches requester and sequence");
        for (int mismatch = 0; mismatch < 2; ++mismatch)
        {
            reset();
            sim.wrong_response_sequence = mismatch == 0;
            sim.wrong_response_requester = mismatch == 1;
            check(execute(UdpTest::DelayExchange, configuration(true, false), evidence) == Result::Fail,
                  "Delay_Resp requires both the request sequence and requester identity");
        }
        reset();
        sim.response_delay = 120000;
        check(execute(UdpTest::DelayExchange, configuration(true, false), evidence) == Result::Fail,
              "a response arriving after its deadline cannot satisfy the exchange");

        reset();
        check(execute(UdpTest::Simultaneous, configuration(true, true), evidence) == Result::Pass,
              "both families provide overlapping sustained service");
        check_socket_configuration(4);
        check(evidence.find("IPv4") != std::string::npos && evidence.find("IPv6") != std::string::npos,
              "dual-stack evidence identifies both families");

        reset();
        check(execute(UdpTest::CombinedLoad, configuration(true, true), evidence) == Result::Pass,
              "combined-load UDP observation completes");
        check(sim.start_callbacks == 1 && sim.end_callbacks == 1, "combined-load callback brackets one active window");

        reset();
        sim.mismatched_grandmaster = true;
        check(execute(UdpTest::Simultaneous, configuration(true, true), evidence) == Result::Fail,
              "mismatched simultaneous grandmasters are not reported as a pass");
        reset();
        sim.omit_follow_up = true;
        check(execute(UdpTest::Sustained, configuration(true, false), evidence) == Result::Fail,
              "missing Follow_Up messages prevent a sustained-service pass");
        reset();
        sim.identity_change_at = sim.now + 500000;
        check(execute(UdpTest::Sustained, configuration(false, true), evidence) == Result::CouldNotRun,
              "a changed master identity makes the observation inconclusive");
    }

    void negative_probes()
    {
        std::string evidence;
        reset();
        check(execute(UdpTest::NegativeDelay, configuration(true, false), evidence) == Result::Pass,
              "invalid multicast requests are rejected while healthy controls succeed");
        check(evidence.find("packet loss cannot be excluded") != std::string::npos,
              "negative-probe evidence discloses unobservable packet loss");
        check(sim.sent.size() >= 16, "negative probes are bracketed by repeated valid controls");
        bool wrong_domain = false, wrong_version = false, wrong_length = false;
        for (const auto &bytes : sim.sent)
        {
            wrong_domain |= bytes.size() >= 5 && bytes[4] != Domain;
            wrong_version |= bytes.size() >= 2 && bytes[1] != 2;
            wrong_length |= bytes.size() != 44 || (bytes.size() > 3 && bytes[3] != 44);
        }
        check(wrong_domain && wrong_version && wrong_length, "mutated request bytes exercise domain, version and length");

        reset();
        sim.accept_invalid = true;
        check(execute(UdpTest::NegativeDelay, configuration(false, true), evidence) == Result::Fail,
              "a response to a malformed request is detected");

        reset();
        sim.ignore_valid_from = 2;
        check(execute(UdpTest::NegativeDelay, configuration(true, false), evidence) == Result::CouldNotRun,
              "missing healthy controls cannot produce a negative-test pass");
    }

    void target_validation()
    {
        for (const char *text : {"", "192.0.2.20", "169.254.1.1"})
            check(valid_ptp_udp_target(text, AF_INET), "empty or unicast IPv4 master is valid");
        for (const char *text : {"0.0.0.0", "0.1.2.3", "127.0.0.1", "224.0.1.129", "255.255.255.255",
                                 "192.0.2.10", "192.0.2.999", "host.example"})
            check(!valid_ptp_udp_target(text, AF_INET), "invalid, multicast, local, or reserved IPv4 master is rejected");
        for (const char *text : {"", "fe80::20", "2001:db8::20"})
            check(valid_ptp_udp_target(text, AF_INET6), "empty or unicast IPv6 master is valid");
        for (const char *text : {"::", "::1", "ff02::181", "::ffff:c000:214", "fe80::10", "fe80::20%3", "bad::address"})
            check(!valid_ptp_udp_target(text, AF_INET6), "invalid, multicast, local, or reserved IPv6 master is rejected");
    }

    void response_deadline()
    {
        for (int64_t received : {int64_t{99}, int64_t{100}, int64_t{101}})
        {
            UdpObservation observation{};
            observation.master_locked = true;
            observation.master.source[0] = 0x10;
            observation.requester[0] = 0x20;
            observation.sequence = 42;
            observation.outstanding = true;
            observation.request_deadline_us = 100;
            Message response{};
            response.type = 9;
            response.sequence = 42;
            response.source[0] = 0x10;
            response.requester[0] = 0x20;
            response.timestamp.seconds = 1700000000;
            Packet packet{};
            packet.received_us = received;
            observation.accept(response, packet);
            check(observation.responses == (received <= 100 ? 1U : 0U) &&
                      observation.response_timeouts == (received > 100 ? 1U : 0U) && !observation.outstanding,
                  "matching responses are accepted only through the exact deadline");
        }
    }

    void interruptions_and_recovery()
    {
        std::string evidence;
        reset();
        sim.fail_bind = true;
        check(execute(UdpTest::Announce, configuration(true, false), evidence) == Result::CouldNotRun,
              "bind failure is inconclusive");
        check(evidence.find("errno") != std::string::npos, "setup failure reports socket error");

        reset();
        sim.fail_join = true;
        check(execute(UdpTest::Announce, configuration(true, true), evidence) == Result::CouldNotRun,
              "membership failure is inconclusive");
        reset();
        sim.fail_select = true;
        check(execute(UdpTest::Announce, configuration(true, false), evidence) == Result::CouldNotRun,
              "select failure is inconclusive");
        reset();
        sim.fail_send = true;
        check(execute(UdpTest::DelayExchange, configuration(false, true), evidence) == Result::CouldNotRun,
              "send failure is inconclusive");
        reset();
        sim.cancel_at = sim.now + 150000;
        check(execute(UdpTest::Sustained, configuration(true, true), evidence) == Result::CouldNotRun,
              "cancellation cannot report success");
        reset();
        sim.link_change_at = sim.now + 150000;
        check(execute(UdpTest::Sustained, configuration(true, false), evidence) == Result::CouldNotRun,
              "Ethernet generation change invalidates observation");

        reset();
        check(execute(UdpTest::RestartRecovery, configuration(false, true), evidence) == Result::Pass,
              "withdrawal and restored fresh service pass assisted recovery");
        check(sim.fault_callbacks == 1 && sim.restore_callbacks == 1 && sim.assisted_action == Test::RestartRecovery,
              "recovery prompts map to the appropriate fault and restoration");
        reset();
        sim.keep_service_on_fault = true;
        check(execute(UdpTest::PpsRecovery, configuration(true, false), evidence) == Result::Fail,
              "service continuing past the withdrawal bound fails despite successful restoration");
        check(sim.restore_callbacks == 1, "failed withdrawal still requests restoration");

        reset();
        UdpConfig invalid = configuration(true, false);
        invalid.ipv4_target = nullptr;
        check(execute(UdpTest::Announce, invalid, evidence) == Result::CouldNotRun,
              "missing target is rejected before use");
        reset();
        check(execute(UdpTest::Simultaneous, configuration(true, false), evidence) == Result::CouldNotRun,
              "simultaneous test requires both families");
    }
}

int main()
{
    try
    {
        WSADATA data{};
        check(WSAStartup(MAKEWORD(2, 2), &data) == 0, "WinSock initializes");
        multicast_service();
        negative_probes();
        target_validation();
        response_deadline();
        interruptions_and_recovery();
        WSACleanup();
        std::printf("PASS: PTP L3 UDP runtime: 5 groups, %d assertions, 0 failures\n", assertions);
        return 0;
    }
    catch (const std::exception &error)
    {
        std::fprintf(stderr, "FAIL after %d assertions: %s\n", assertions, error.what());
        WSACleanup();
        return 1;
    }
}
