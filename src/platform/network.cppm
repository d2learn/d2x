// Small book-preview primitives. No server or port-selection policy lives here.
module;

#include <cstdlib>
#include <cerrno>
#include <cstring>
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  include <iphlpapi.h>
#else
#  include <sys/socket.h>
#  include <sys/wait.h>
#  include <netdb.h>
#  include <unistd.h>
#endif

export module d2x.platform:network;
import std;

namespace d2x::platform_impl {
namespace {
#ifdef _WIN32
    using Socket = SOCKET;
    constexpr Socket invalid_socket = INVALID_SOCKET;
    int socket_error() { return ::WSAGetLastError(); }
    void close_socket(Socket s) { ::closesocket(s); }
    bool address_in_use(int error) { return error == WSAEADDRINUSE; }
    bool unsupported_family(int error) { return error == WSAEAFNOSUPPORT; }
    struct Winsock {
        WSADATA data{};
        int error = ::WSAStartup(MAKEWORD(2, 2), &data);
        ~Winsock() { if (!error) ::WSACleanup(); }
    };

    // An exclusive wildcard listener can make bind(localhost) report ACCESS
    // rather than INUSE. Confirm an overlapping listener before classifying
    // that error as a conflict; excluded ports and other permission errors
    // must remain errors. Reading the table does not connect to the service.
    bool has_overlapping_listener(const addrinfo& address) {
        DWORD size = 0;
        auto result = ::GetExtendedTcpTable(nullptr, &size, FALSE,
            address.ai_family, TCP_TABLE_OWNER_PID_LISTENER, 0);
        if (result != ERROR_INSUFFICIENT_BUFFER) return false;
        std::vector<unsigned char> buffer(size);
        result = ::GetExtendedTcpTable(buffer.data(), &size, FALSE,
            address.ai_family, TCP_TABLE_OWNER_PID_LISTENER, 0);
        if (result != NO_ERROR) return false;
        if (address.ai_family == AF_INET) {
            auto* target = reinterpret_cast<const sockaddr_in*>(address.ai_addr);
            auto* table = reinterpret_cast<const MIB_TCPTABLE_OWNER_PID*>(buffer.data());
            for (DWORD i = 0; i < table->dwNumEntries; ++i) {
                const auto& row = table->table[i];
                if (static_cast<unsigned short>(row.dwLocalPort) == target->sin_port
                    && (row.dwLocalAddr == INADDR_ANY
                        || row.dwLocalAddr == target->sin_addr.s_addr)) return true;
            }
        } else if (address.ai_family == AF_INET6) {
            auto* target = reinterpret_cast<const sockaddr_in6*>(address.ai_addr);
            auto* table = reinterpret_cast<const MIB_TCP6TABLE_OWNER_PID*>(buffer.data());
            for (DWORD i = 0; i < table->dwNumEntries; ++i) {
                const auto& row = table->table[i];
                if (static_cast<unsigned short>(row.dwLocalPort) != target->sin6_port) continue;
                bool wildcard = std::ranges::all_of(row.ucLocalAddr,
                    [](unsigned char c) { return c == 0; });
                if (wildcard || (row.dwLocalScopeId == target->sin6_scope_id
                    && std::memcmp(row.ucLocalAddr, &target->sin6_addr, 16) == 0)) return true;
            }
        }
        return false;
    }
#else
    using Socket = int;
    constexpr Socket invalid_socket = -1;
    int socket_error() { return errno; }
    void close_socket(Socket s) { ::close(s); }
    bool address_in_use(int error) { return error == EADDRINUSE; }
    bool unsupported_family(int error) { return error == EAFNOSUPPORT; }
#endif
    std::string describe_socket_error(int error) {
        return std::error_code(error, std::system_category()).message()
            + " (" + std::to_string(error) + ")";
    }
}

// true = all usable localhost addresses bind; false = address in use.
// Other errors are not interchangeable with a port conflict.
export std::expected<bool, std::string> probe_tcp_port(int port) {
#ifdef _WIN32
    Winsock winsock;
    if (winsock.error) return std::unexpected(describe_socket_error(winsock.error));
#endif
    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    addrinfo* addresses = nullptr;
    auto service = std::to_string(port);
    int error = ::getaddrinfo("localhost", service.c_str(), &hints, &addresses);
    if (error) return std::unexpected("Cannot resolve localhost (getaddrinfo "
        + std::to_string(error) + ")");
    struct AddressGuard {
        addrinfo* value;
        ~AddressGuard() { ::freeaddrinfo(value); }
    } address_guard{addresses};
    struct SocketGuard {
        std::vector<Socket> values;
        ~SocketGuard() { for (auto s : values) close_socket(s); }
    } sockets;
    std::set<std::string> checked;
    for (auto* address = addresses; address; address = address->ai_next) {
        char host[NI_MAXHOST]{};
        error = ::getnameinfo(address->ai_addr, static_cast<int>(address->ai_addrlen),
            host, sizeof(host), nullptr, 0, NI_NUMERICHOST);
        if (error) return std::unexpected("Cannot format localhost address");
        if (!checked.insert(host).second) continue;
        auto socket = ::socket(address->ai_family, SOCK_STREAM, IPPROTO_TCP);
        if (socket == invalid_socket) {
            error = socket_error();
            if (unsupported_family(error)) continue;
            return std::unexpected(describe_socket_error(error));
        }
        sockets.values.push_back(socket);
#ifdef _WIN32
        BOOL exclusive = TRUE;
        if (::setsockopt(socket, SOL_SOCKET, SO_EXCLUSIVEADDRUSE,
                reinterpret_cast<const char*>(&exclusive), sizeof(exclusive)) != 0)
            return std::unexpected(describe_socket_error(socket_error()));
#endif
        if (::bind(socket, address->ai_addr, static_cast<int>(address->ai_addrlen)) != 0) {
            error = socket_error();
            if (address_in_use(error)) return false;
#ifdef _WIN32
            if (error == WSAEACCES && has_overlapping_listener(*address)) return false;
#endif
            return std::unexpected(describe_socket_error(error));
        }
    }
    if (sockets.values.empty()) return std::unexpected("No usable localhost TCP address");
    return true;
}

export std::optional<std::string> quote_argument(std::string_view value) {
#ifdef _WIN32
    // cmd.exe expands percent/delayed-expansion variables even inside quotes.
    if (value.find_first_of("\"%!\r\n") != std::string_view::npos) return std::nullopt;
    return "\"" + std::string(value) + "\"";
#else
    std::string quoted = "'";
    for (char c : value) {
        if (c == '\'') quoted += "'\\''";
        else quoted += c;
    }
    return quoted + "'";
#endif
}

export int normalize_exit_status(int status) {
    if (status == -1) return 127;
#ifdef _WIN32
    return status;
#else
    if (WIFEXITED(status)) return WEXITSTATUS(status);
    if (WIFSIGNALED(status)) return 128 + WTERMSIG(status);
    return 1;
#endif
}
}
