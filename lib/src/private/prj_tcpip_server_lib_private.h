/*!
 * \file
 * \brief file prj_tcpip_server_lib_private.h
 *
 * private functionality of the library
 *
 * Copyright of Timo Hannukkala. All rights reserved.
 *
 * \author Timo Hannukkala <timohannukkala@hotmail.com>
 */

#ifndef PRJ_TCPIP_SERVER_LIB_PRIVATE_H
#define PRJ_TCPIP_SERVER_LIB_PRIVATE_H

#include <cstdint>
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <memory>

/*!
 * \brief PrjTcpipServerLibPrivate
 *
 */
class PrjTcpipServerLibPrivate
{
public:
    using ReceiveCallback = std::function<void(int clientId, std::string incomingText)>;
    using DisconnectCallback = std::function<void(int clientId)>;

    explicit PrjTcpipServerLibPrivate();
    ~PrjTcpipServerLibPrivate();

    bool init(uint16_t port, ReceiveCallback callback, DisconnectCallback disconnectCallback,
              const std::string& listenAddress);
    bool sendTextToClient(int clientId, const std::string& text, bool addLineBreakIfMissing);
    bool sendTextToAllClients(const std::string& text, bool addLineBreakIfMissing);
private:
    /*!
     * \brief Client owns the socket. The socket is closed when the last reference
     * is gone, so it can't be reused while a send to it is still running.
     */
    struct Client {
        Client(int clientId, int fd) : id(clientId), socketFd(fd) {}
        ~Client();
        Client(const Client&) = delete;
        Client& operator=(const Client&) = delete;

        const int id;
        const int socketFd;
        std::mutex sendMutex;
    };

    std::vector<std::shared_ptr<Client>> m_clients;
    std::mutex m_mutex;
    std::condition_variable m_threadsCv;
    int m_activeThreads = 0;
    int m_serverSocket = -1;
    int m_nextClientId = 1;
    bool m_stopping = false;

    std::shared_ptr<Client> findClient(int clientId);
    int nextClientId();
    void removeClient(const std::shared_ptr<Client>& client);
    void threadFinished();
    static bool sendToClient(Client& client, const std::string& text);
    static bool sendAll(int socketFd, const std::string& text);
    static bool processLines(int clientId, const char *receivedData, size_t receivedSize, std::string& incompleteLine,
                             const ReceiveCallback& callback);
    static void handleClient(std::shared_ptr<Client> client, PrjTcpipServerLibPrivate *priv,
                             ReceiveCallback callback, DisconnectCallback disconnectCallback);

};

#endif // PRJ_TCPIP_SERVER_LIB_PRIVATE_H
