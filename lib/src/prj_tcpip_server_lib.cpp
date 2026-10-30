#include "prj_tcpip_server_lib.h"
#include "private/prj_tcpip_server_lib_private.h"
#include "tg_global_log.h"


PrjTcpipServerLib::PrjTcpipServerLib()
    : m_private(new PrjTcpipServerLibPrivate())
{
    TG_FUNCTION_BEGIN();
    TG_FUNCTION_END();
}

PrjTcpipServerLib::~PrjTcpipServerLib()
{
    TG_FUNCTION_BEGIN();
    delete m_private;
    TG_FUNCTION_END();
}

bool PrjTcpipServerLib::init(uint16_t port, std::function<void(int clientId, std::string incomingText)> callback,
                             std::function<void(int clientId)> disconnectCallback,
                             const std::string& listenAddress)
{
    TG_FUNCTION_BEGIN();
    const bool ret = m_private->init(port, callback, disconnectCallback, listenAddress);
    TG_FUNCTION_END();
    return ret;
}

bool PrjTcpipServerLib::send(int clientId, const std::string& text, bool addLineBreakIfMissing)
{
    TG_FUNCTION_BEGIN();
    const bool ret = m_private->sendTextToClient(clientId, text, addLineBreakIfMissing);
    TG_FUNCTION_END();
    return ret;
}

bool PrjTcpipServerLib::sendToAllClients(const std::string& text, bool addLineBreakIfMissing)
{
    TG_FUNCTION_BEGIN();
    const bool ret = m_private->sendTextToAllClients(text, addLineBreakIfMissing);
    TG_FUNCTION_END();
    return ret;
}
