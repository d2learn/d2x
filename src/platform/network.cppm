// Small book-preview primitives. No server or port-selection policy lives here.
module;

#include <cstdlib>
#include <cerrno>
#ifdef _WIN32
#  define WIN32_LEAN_AND_MEAN
#  define NOMINMAX
#  include <winsock2.h>
#  include <ws2tcpip.h>
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
