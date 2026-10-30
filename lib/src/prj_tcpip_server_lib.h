/*!
 * \file
 * \brief file prj_tcpip_server_lib.h
 *
 * public functionality of the TCP/IP server library
 *
 * Copyright of Timo Hannukkala. All rights reserved.
 *
 * \author Timo Hannukkala <timohannukkala@hotmail.com>
 */

#ifndef PRJ_TCPIP_SERVER_LIB_H
#define PRJ_TCPIP_SERVER_LIB_H

#include <cstdint>
#include <string>
#include <functional>

class PrjTcpipServerLibPrivate;

#ifndef PRJ_TCPIP_SERVER_LIBRARY_EXPORT
#define PRJ_TCPIP_SERVER_LIBRARY_EXPORT
#endif

/**
 * @brief Line-based TCP/IP server with one thread per client
 *
 * Clients are identified by a client ID. An ID is never reused while its client
 * is connected, and IDs are reused only after the ID counter wraps around.
 */
class PRJ_TCPIP_SERVER_LIBRARY_EXPORT PrjTcpipServerLib
{
public:
    explicit PrjTcpipServerLib();
    /**
     * Stops the server, disconnects all clients and waits until all running
     * callbacks have returned. Must not be called from a callback; doing so aborts.
     */
    ~PrjTcpipServerLib();
    PrjTcpipServerLib(const PrjTcpipServerLib&) = delete;
    PrjTcpipServerLib& operator=(const PrjTcpipServerLib&) = delete;

    /**
     * Starts listening on listenAddress (IPv4) and port, and blocks until this
     * object is destroyed, then returns true. Returns false if the server can't
     * be started or if init() is already running.
     *
     * callback is called once per received line ('\n'-terminated, with the
     * newline and a trailing '\r' removed) from the client's own thread, so it
     * must be thread-safe. Data after the last newline is dropped on disconnect.
     * A line longer than 64 KiB, or an exception thrown from callback,
     * disconnects the client.
     * disconnectCallback, if set, is called from the client's thread after the
     * client has been removed.
     * At most 1000 clients can be connected; further connections are closed.
     */
    bool init(uint16_t port, std::function<void(int clientId, std::string incomingText)> callback,
              std::function<void(int clientId)> disconnectCallback = nullptr,
              const std::string& listenAddress = "0.0.0.0");
    /**
     * Sends text to the client. If addLineBreakIfMissing is set, '\n' is appended
     * when text doesn't already end with one, so empty text is sent as an empty line.
     * Returns false if text is empty and addLineBreakIfMissing isn't set, if the
     * client is not connected, or if sending fails or takes over 10 seconds. In the
     * last two cases the client is disconnected, because the text may have been
     * only partially sent.
     */
    bool send(int clientId, const std::string& text, bool addLineBreakIfMissing);
    /**
     * Sends text to all clients one by one, handling text and addLineBreakIfMissing
     * like send(). Returns true if any send succeeded.
     */
    bool sendToAllClients(const std::string& text, bool addLineBreakIfMissing);
private:
    PrjTcpipServerLibPrivate *m_private;
};

#endif // PRJ_TCPIP_SERVER_LIB_H
