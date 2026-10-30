#include "prj_tcpip_server_lib_private.h"
#include "../tg_global_log.h"
#include <algorithm>
#include <chrono>
#include <climits>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <system_error>
#include <thread>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

const int BUFFER_SIZE = 16 * 1024;
const size_t MAX_LINE_LENGTH = 64 * 1024;
const size_t MAX_CLIENTS = 1000;
const int SEND_TIMEOUT_SECONDS = 10;

// set in client threads; used to detect the server being destroyed from its own callback
static thread_local const PrjTcpipServerLibPrivate *t_clientThreadServer = nullptr;

/**
 * runs a user callback; exceptions are logged instead of terminating the process
 * @return false if the callback threw
 */
template <typename F>
static bool runCallback(int clientId, F&& f)
{
    TG_FUNCTION_BEGIN();
    (void)clientId; // only used in logging
    try {
        f();
        TG_FUNCTION_END();
        return true;
    } catch (const std::exception& e) {
        TG_ERROR_LOG("Callback threw an exception", clientId, e.what());
        (void)e;
    } catch (...) {
        TG_ERROR_LOG("Callback threw an unknown exception", clientId);
    }
    TG_FUNCTION_END();
    return false;
}

/**
 * sends the whole text, because send() may write only part of it in one call
 * @return true if all bytes were sent within SEND_TIMEOUT_SECONDS
 */
bool PrjTcpipServerLibPrivate::sendAll(int socketFd, const std::string& text)
{
    TG_FUNCTION_BEGIN();
    // one deadline for the whole text, so that a slowly reading client can't block senders forever
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(SEND_TIMEOUT_SECONDS);
    size_t totalSent = 0;
    while (totalSent < text.size()) {
        ssize_t bytesSent = send(socketFd, text.data() + totalSent, text.size() - totalSent,
                                 MSG_NOSIGNAL | MSG_DONTWAIT);
        if (bytesSent >= 0) {
            totalSent += bytesSent;
            continue;
        }
        if (errno == EINTR) {
            continue;
        }
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            TG_FUNCTION_END();
            return false;
        }
        auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
                    deadline - std::chrono::steady_clock::now()).count();
        if (remaining <= 0) {
            TG_FUNCTION_END();
            return false;
        }
        pollfd pfd{socketFd, POLLOUT, 0};
        if (poll(&pfd, 1, static_cast<int>(remaining)) < 0 && errno != EINTR) {
            TG_FUNCTION_END();
            return false;
        }
    }
    TG_FUNCTION_END();
    return true;
}

PrjTcpipServerLibPrivate::Client::~Client()
{
    TG_FUNCTION_BEGIN();
    close(socketFd);
    TG_FUNCTION_END();
}

PrjTcpipServerLibPrivate::PrjTcpipServerLibPrivate()
{
    TG_FUNCTION_BEGIN();
    TG_FUNCTION_END();
}
/**
 * stops the accept loop and all client threads and waits for them to finish,
 * so that no thread uses this object after it has been destroyed
 */
PrjTcpipServerLibPrivate::~PrjTcpipServerLibPrivate()
{
    TG_FUNCTION_BEGIN();
    if (t_clientThreadServer == this) {
        // waiting for our own thread would deadlock
        TG_ERROR_LOG("Server destroyed from its own callback");
        std::abort();
    }
    std::unique_lock<std::mutex> lock(m_mutex);
    m_stopping = true;
    if (m_serverSocket >= 0) {
        shutdown(m_serverSocket, SHUT_RDWR);
    }
    for (const auto& client : m_clients) {
        shutdown(client->socketFd, SHUT_RDWR);
    }
    m_threadsCv.wait(lock, [this] { return m_activeThreads == 0; });
    TG_FUNCTION_END();
}

/**
 * finds a connected client; m_mutex must be locked
 */
std::shared_ptr<PrjTcpipServerLibPrivate::Client> PrjTcpipServerLibPrivate::findClient(int clientId)
{
    TG_FUNCTION_BEGIN();
    for (size_t i=0;i<m_clients.size();i++) {
        if (m_clients[i]->id == clientId) {
            TG_FUNCTION_END();
            return m_clients[i];
        }
    }
    TG_FUNCTION_END();
    return nullptr;
}

/**
 * returns the next free client ID, skipping IDs still in use after the counter
 * wraps around; m_mutex must be locked
 */
int PrjTcpipServerLibPrivate::nextClientId()
{
    TG_FUNCTION_BEGIN();
    while (true) {
        int clientId = m_nextClientId;
        m_nextClientId = (m_nextClientId == INT_MAX) ? 1 : m_nextClientId + 1;
        if (!findClient(clientId)) {
            TG_FUNCTION_END();
            return clientId;
        }
    }
}

/**
 * sends the whole text to one client; on failure the client is disconnected,
 * because part of the text may already have been sent
 */
bool PrjTcpipServerLibPrivate::sendToClient(Client& client, const std::string& text)
{
    TG_FUNCTION_BEGIN();
    // only this client's lock is held, so a slow client doesn't block others
    std::lock_guard<std::mutex> sendLock(client.sendMutex);
    if (sendAll(client.socketFd, text)) {
        TG_DEBUG_LOG("Sent data to client", client.id);
        TG_FUNCTION_END();
        return true;
    }
    TG_ERROR_LOG("Failed to send data, disconnecting client", client.id);
    shutdown(client.socketFd, SHUT_RDWR);
    TG_FUNCTION_END();
    return false;
}

/**
 * returns true if a line break must be appended to text before sending it
 */
static bool needsLineBreak(const std::string& text, bool addLineBreakIfMissing)
{
    TG_FUNCTION_BEGIN();
    TG_FUNCTION_END();
    return addLineBreakIfMissing && (text.empty() || text.back() != '\n');
}

/**
 * sends text to a client
 * @param clientId client to send to
 * @param text text to send
 * @param addLineBreakIfMissing append '\n' if text doesn't end with one
 */
bool PrjTcpipServerLibPrivate::sendTextToClient(int clientId, const std::string& text, bool addLineBreakIfMissing)
{
    TG_FUNCTION_BEGIN();
    // empty text is only sent as an empty line
    if (text.empty() && !addLineBreakIfMissing) {
        TG_FUNCTION_END();
        return false;
    }
    m_mutex.lock();
    std::shared_ptr<Client> client = findClient(clientId);
    m_mutex.unlock();
    if (!client) {
        TG_FUNCTION_END();
        return false;
    }
    const bool ret = needsLineBreak(text, addLineBreakIfMissing)
            ? sendToClient(*client, text + "\n") : sendToClient(*client, text);
    TG_FUNCTION_END();
    return ret;
}

/**
 * sends text to all clients
 * @param text text to send
 * @param addLineBreakIfMissing append '\n' if text doesn't end with one
 */
bool PrjTcpipServerLibPrivate::sendTextToAllClients(const std::string& text, bool addLineBreakIfMissing)
{
    TG_FUNCTION_BEGIN();
    // empty text is only sent as an empty line
    if (text.empty() && !addLineBreakIfMissing) {
        TG_FUNCTION_END();
        return false;
    }
    // build the text with the line break only once, not separately for every client
    std::string textWithLineBreak;
    if (needsLineBreak(text, addLineBreakIfMissing)) {
        textWithLineBreak = text + "\n";
    }
    const std::string& textToSend = textWithLineBreak.empty() ? text : textWithLineBreak;

    m_mutex.lock();
    const std::vector<std::shared_ptr<Client>> clients = m_clients;
    m_mutex.unlock();
    bool ret = false;

    for (size_t i=0;i<clients.size();i++) {
        if (sendToClient(*clients[i], textToSend)) {
            ret = true;
        }
    }
    TG_FUNCTION_END();
    return ret;
}

void PrjTcpipServerLibPrivate::removeClient(const std::shared_ptr<Client>& client)
{
    TG_FUNCTION_BEGIN();
    m_mutex.lock();
    TG_INFO_LOG("Closing the client", client->id);
    // wakes up sends blocked on this client; the socket itself is closed by ~Client
    shutdown(client->socketFd, SHUT_RDWR);
    auto it = std::find(m_clients.begin(), m_clients.end(), client);
    if (it != m_clients.end()) {
        m_clients.erase(it);
        TG_INFO_LOG("Removed the client", client->id);
    }
    m_mutex.unlock();
    TG_FUNCTION_END();
}

void PrjTcpipServerLibPrivate::threadFinished()
{
    TG_FUNCTION_BEGIN();
    std::lock_guard<std::mutex> lock(m_mutex);
    m_activeThreads--;
    m_threadsCv.notify_all();
    TG_FUNCTION_END();
}

/**
 * splits the complete lines from the received data and passes each one to the callback;
 * lines are read directly from receivedData, and only a line that continues in the next read
 * is stored in incompleteLine, so each byte is searched only once and copied at most once
 * @param receivedData data from the latest read
 * @param receivedSize size of receivedData
 * @param incompleteLine start of a line received in earlier reads, updated with the new incomplete line
 * @return false if the callback threw, in which case the client should be disconnected
 */
bool PrjTcpipServerLibPrivate::processLines(int clientId, const char *receivedData, size_t receivedSize, std::string& incompleteLine,
        const ReceiveCallback& callback)
{
    TG_FUNCTION_BEGIN();
    const char *receivedEnd = receivedData + receivedSize;
    const char *lineStart = receivedData;
    const char *lineEnd;
    while ((lineEnd = static_cast<const char*>(std::memchr(lineStart, '\n', receivedEnd - lineStart))) != nullptr) {
        std::string incomingText;
        if (incompleteLine.empty()) {
            incomingText.assign(lineStart, lineEnd);
        } else {
            // the line started in an earlier read
            incompleteLine.append(lineStart, lineEnd);
            incomingText = std::move(incompleteLine);
            incompleteLine.clear();
        }
        if (!incomingText.empty() && incomingText.back() == '\r') {
            incomingText.pop_back();
        }
        TG_DEBUG_LOG("Read from client", clientId, incomingText);
        // moved, so the callback gets the text without another copy
        if (!runCallback(clientId, [&] { callback(clientId, std::move(incomingText)); })) {
            TG_FUNCTION_END();
            return false;
        }
        lineStart = lineEnd + 1;
    }
    incompleteLine.append(lineStart, receivedEnd);
    TG_FUNCTION_END();
    return true;
}

void PrjTcpipServerLibPrivate::handleClient(std::shared_ptr<Client> client, PrjTcpipServerLibPrivate *priv,
        ReceiveCallback callback, DisconnectCallback disconnectCallback)
{
    TG_FUNCTION_BEGIN();
    t_clientThreadServer = priv;
    const int clientId = client->id;
    char buffer[BUFFER_SIZE];
    std::string incompleteLine;
    bool connected = true;
    while (connected) {
        ssize_t bytesRead = recv(client->socketFd, buffer, BUFFER_SIZE, 0);

        if (bytesRead < 0) {
            if (errno == EINTR) {
                continue;
            }
            TG_ERROR_LOG("Failed to read from client", clientId);
            break;
        }
        else if (bytesRead == 0) {
            TG_INFO_LOG("Client disconnected", clientId);
            break;
        }

        // TCP is a byte stream, so collect the data and pass complete lines to the callback
        connected = processLines(clientId, buffer, bytesRead, incompleteLine, callback);

        if (incompleteLine.size() > MAX_LINE_LENGTH) {
            TG_ERROR_LOG("Line too long, disconnecting", clientId);
            break;
        }
    }
    priv->removeClient(client);
    client.reset();
    if (disconnectCallback) {
        runCallback(clientId, [&] { disconnectCallback(clientId); });
    }
    t_clientThreadServer = nullptr;
    TG_FUNCTION_END();
    priv->threadFinished();
}

static bool isOutOfResources(int error)
{
    TG_FUNCTION_BEGIN();
    TG_FUNCTION_END();
    return error == EMFILE || error == ENFILE || error == ENOBUFS || error == ENOMEM;
}

bool PrjTcpipServerLibPrivate::init(uint16_t port, ReceiveCallback callback,
        DisconnectCallback disconnectCallback, const std::string& listenAddress)
{
    TG_FUNCTION_BEGIN();
    sockaddr_in serverAddr{};
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, listenAddress.c_str(), &serverAddr.sin_addr) != 1) {
        TG_ERROR_LOG("Invalid listen address", listenAddress);
        TG_FUNCTION_END();
        return false;
    }

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (serverSocket < 0) {
        TG_FUNCTION_END();
        return false;
    }

    int opt = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        TG_WARNING_LOG("Failed to set SO_REUSEADDR", errno);
    }

    if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        close(serverSocket);
        TG_FUNCTION_END();
        return false;
    }

    if (listen(serverSocket, SOMAXCONN) < 0) {
        close(serverSocket);
        TG_FUNCTION_END();
        return false;
    }

    m_mutex.lock();
    if (m_stopping || m_serverSocket >= 0) {
        m_mutex.unlock();
        TG_ERROR_LOG("Server is already running or has been stopped");
        close(serverSocket);
        TG_FUNCTION_END();
        return false;
    }
    m_serverSocket = serverSocket;
    m_activeThreads++;
    m_mutex.unlock();

    TG_INFO_LOG("[Server] Listening on", listenAddress, port);

    while (true) {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);

        int socketFd = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        int acceptError = errno;
        m_mutex.lock();
        if (m_stopping) {
            m_mutex.unlock();
            if (socketFd >= 0) {
                close(socketFd);
            }
            break;
        }
        if (socketFd < 0) {
            m_mutex.unlock();
            TG_WARNING_LOG("Failed to accept a client", acceptError);
            if (isOutOfResources(acceptError)) {
                // retrying immediately would busy-loop until resources are freed
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
            }
            continue;
        }
        if (m_clients.size() >= MAX_CLIENTS) {
            m_mutex.unlock();
            TG_WARNING_LOG("Too many clients, refusing connection", socketFd);
            close(socketFd);
            continue;
        }

        auto client = std::make_shared<Client>(nextClientId(), socketFd);
        m_clients.push_back(client);
        m_activeThreads++;
        TG_INFO_LOG("Client connected", client->id, m_clients.size());
        m_mutex.unlock();
        try {
            std::thread t(handleClient, client, this, callback, disconnectCallback);
            t.detach();
        } catch (const std::system_error& e) {
            TG_ERROR_LOG("Failed to create client thread", client->id, e.what());
            (void)e;
            removeClient(client);
            threadFinished();
        }
    }

    TG_INFO_LOG("[Server] Stopped listening on port", port);
    close(serverSocket);
    m_mutex.lock();
    m_serverSocket = -1;
    m_mutex.unlock();
    threadFinished();
    TG_FUNCTION_END();
    return true;
}
