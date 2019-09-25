/*
 * @CopyRight:
 * FISCO-BCOS is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * FISCO-BCOS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with FISCO-BCOS.  If not, see <http://www.gnu.org/licenses/>
 * (c) 2016-2018 fisco-dev contributors.
 */
/**
 * @file: ChannelRPCServer.h
 * @author: monan
 * @date: 2017
 *
 * @author xingqiangbai
 * @date 2018.11.5
 * @modify use libp2p to send message
 */

#pragma once

#include "ChannelMessage.h"  // for TopicChannelM...
#include "ChannelSession.h"  // for ChannelSessio...
#include "Message.h"         // for Message, Mess...
#include "libethcore/Common.h"
#include "libp2p/P2PMessage.h"
#include <jsonrpccpp/server/abstractserverconnector.h>
#include <boost/asio/io_service.hpp>  // for io_service
#include <atomic>                     // for atomic
#include <map>                        // for map
#include <mutex>                      // for mutex
#include <set>                        // for set
#include <string>                     // for string
#include <thread>
#include <utility>  // for swap, move
#include <vector>   // for vector
namespace boost
{
namespace asio
{
namespace ssl
{
class context;
}
}  // namespace asio
}  // namespace boost

namespace Json
{
class Value;
}  // namespace Json

namespace dev
{
namespace channel
{
class ChannelException;
class ChannelServer;
}  // namespace channel
namespace network
{
class NetworkException;
}
namespace p2p
{
class P2PInterface;
class P2PMessage;
class P2PSession;
}  // namespace p2p

class ChannelRPCServer : public jsonrpc::AbstractServerConnector,
                         public std::enable_shared_from_this<ChannelRPCServer>
{
public:
    enum ChannelERRORCODE
    {
        REMOTE_PEER_UNAVAILABLE = 100,
        REMOTE_CLIENT_PEER_UNAVAILABLE = 101,
        TIMEOUT = 102
    };

    typedef std::shared_ptr<ChannelRPCServer> Ptr;

    ChannelRPCServer(std::string listenAddr = "", int listenPort = 0)
      : jsonrpc::AbstractServerConnector(), _listenAddr(listenAddr), _listenPort(listenPort){};
    virtual ~ChannelRPCServer();
    virtual bool StartListening() override;
    virtual bool StopListening() override;
    virtual bool SendResponse(std::string const& _response, void* _addInfo = nullptr) override;


    virtual void onConnect(
        dev::channel::ChannelException e, dev::channel::ChannelSession::Ptr session);


    virtual void onDisconnect(
        dev::channel::ChannelException e, dev::channel::ChannelSession::Ptr session);


    virtual void onClientRequest(dev::channel::ChannelSession::Ptr session,
        dev::channel::ChannelException e, dev::channel::Message::Ptr message);
    virtual void blockNotify(int16_t _groupID, int64_t _blockNumber);

    void setListenAddr(const std::string& listenAddr);

    void setListenPort(int listenPort);

    void removeSession(int sessionID);

    void CloseConnection(int _socket);

    void onNodeChannelRequest(dev::network::NetworkException, std::shared_ptr<p2p::P2PSession>,
        std::shared_ptr<p2p::P2PMessage>);

    void setService(std::shared_ptr<dev::p2p::P2PInterface> _service);

    void setSSLContext(std::shared_ptr<boost::asio::ssl::context> sslContext);

    std::shared_ptr<dev::channel::ChannelServer> channelServer() { return _server; }
    void setChannelServer(std::shared_ptr<dev::channel::ChannelServer> server);

    void asyncPushChannelMessage(std::string topic, dev::channel::Message::Ptr message,
        std::function<void(dev::channel::ChannelException, dev::channel::Message::Ptr,
            dev::channel::ChannelSession::Ptr)>
            callback);

    void asyncPushChannelMessageHandler(const std::string& toTopic, const std::string& content);

    void asyncBroadcastChannelMessage(std::string topic, dev::channel::Message::Ptr message);

    virtual dev::channel::TopicChannelMessage::Ptr pushChannelMessage(
        dev::channel::TopicChannelMessage::Ptr message, size_t timeout);

    void setCallbackSetter(std::function<void(
            std::function<void(const std::string& receiptContext)>*, std::function<uint32_t()>*)>
            callbackSetter)
    {
        m_callbackSetter = callbackSetter;
    };

    void setEventFilterCallback(std::function<int32_t(const std::string&, uint32_t,
            std::function<bool(
                const std::string& _filterID, int32_t _result, const Json::Value& _logs)>,
            std::function<bool()>)>
            _callback)
    {
        m_eventFilterCallBack = _callback;
    };

    void addHandler(const dev::eth::Handler<int64_t>& handler) { m_handlers.push_back(handler); }

private:
    virtual void onClientRPCRequest(
        dev::channel::ChannelSession::Ptr session, dev::channel::Message::Ptr message);

    virtual void onClientTopicRequest(
        dev::channel::ChannelSession::Ptr session, dev::channel::Message::Ptr message);

    virtual void onClientChannelRequest(
        dev::channel::ChannelSession::Ptr session, dev::channel::Message::Ptr message);

    virtual void onClientEventLogRequest(
        dev::channel::ChannelSession::Ptr session, dev::channel::Message::Ptr message);

    virtual void onClientHandshake(
        dev::channel::ChannelSession::Ptr session, dev::channel::Message::Ptr message);

    virtual void onClientHeartbeat(
        dev::channel::ChannelSession::Ptr session, dev::channel::Message::Ptr message);

    dev::channel::ChannelSession::Ptr sendChannelMessageToSession(std::string topic,
        dev::channel::Message::Ptr message,
        const std::set<dev::channel::ChannelSession::Ptr>& exclude);

    void updateHostTopics();

    std::vector<dev::channel::ChannelSession::Ptr> getSessionByTopic(const std::string& topic);

    void onClientUpdateTopicStatusRequest(dev::channel::Message::Ptr message);

    bool _running = false;

    std::string _listenAddr;
    int _listenPort;
    std::shared_ptr<boost::asio::io_service> _ioService;

    std::shared_ptr<boost::asio::ssl::context> _sslContext;
    std::shared_ptr<dev::channel::ChannelServer> _server;

    std::map<int, dev::channel::ChannelSession::Ptr> _sessions;
    std::mutex _sessionMutex;

    std::map<std::string, dev::channel::ChannelSession::Ptr> _seq2session;
    std::mutex _seqMutex;

    int _sessionCount = 1;

    std::shared_ptr<dev::p2p::P2PInterface> m_service;

    std::function<void(
        std::function<void(const std::string& receiptContext)>*, std::function<uint32_t()>*)>
        m_callbackSetter;

    std::function<int32_t(const std::string&, uint32_t,
        std::function<bool(
            const std::string& _filterID, int32_t _result, const Json::Value& _logs)>,
        std::function<bool()>)>
        m_eventFilterCallBack;

    std::vector<dev::eth::Handler<int64_t>> m_handlers;
};

}  // namespace dev
