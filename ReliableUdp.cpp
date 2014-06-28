#include "ReliableUdp.h"
#include "Log.h"
#include "Util.h"

#include <cassert>
#include <typeinfo>

using namespace std;

void ReliableUdp::ProxyOwner::sendGoBackN ( GoBackN *gbn, const MsgPtr& msg )
{
    LOG ( "parent=%08x; proxy=%08x; owner=%08x; proxy->gbn=%08x", parent, this, owner, &gbn );

    assert ( parent != 0 );
    assert ( gbn == &this->gbn );
    assert ( gbn->owner == this );

    parent->Socket::send ( msg, address );
}

void ReliableUdp::ProxyOwner::recvGoBackN ( GoBackN *gbn, const MsgPtr& msg )
{
    LOG ( "parent=%08x; proxy=%08x; owner=%08x; proxy->gbn=%08x", parent, this, owner, &gbn );

    assert ( parent != 0 );
    assert ( parent->proxy.get() != 0 );
    assert ( gbn == &this->gbn );
    assert ( gbn->owner == this );

    if ( parent->proxy.get() == this )
    {
        assert ( parent->owner == this );
        assert ( owner != 0 );

        switch ( msg->getType() )
        {
            case MsgType::ReliableUdpConnected:
                parent->state = State::Connected;
                LOG ( "Accepted: parent=%08x; proxy=%08x; owner=%08x; proxy->gbn=%08x", parent, this, owner, &gbn );
                owner->connectEvent ( parent );
                break;

            default:
                owner->readEvent ( parent, msg, address );
                break;
        }
    }
    else
    {
        Socket::Owner *owner = ( this->owner ? this->owner : parent->proxy->owner );

        assert ( parent->proxies.find ( address ) != parent->proxies.end() );
        assert ( parent->proxies[address].get() == this );
        assert ( owner != 0 );

        switch ( msg->getType() )
        {
            case MsgType::ReliableUdpConnect:
                parent->acceptedSocket.reset ( new ReliableUdp ( parent->proxies[address] ) );
                parent->acceptedSocket->send ( new ReliableUdpConnected() );
                LOG ( "Accepted: parent=%08x; proxy=%08x; owner=%08x; proxy->gbn=%08x", parent, this, owner, &gbn );
                owner->acceptEvent ( parent );
                parent->acceptedSocket.reset();
                break;

            default:
                owner->readEvent ( parent, msg, address );
                break;
        }
    }
}

void ReliableUdp::ProxyOwner::readEvent ( Socket *socket, const MsgPtr& msg, const IpAddrPort& address )
{
    assert ( parent->owner == this );
    assert ( parent == socket );

    if ( parent->isClient() )
        parent->proxy->gbn.recv ( msg );
    else
        parent->recvGbnAddressed ( msg, address );
}

void ReliableUdp::sendGbnAddressed ( const MsgPtr& msg, const IpAddrPort& address )
{
    shared_ptr<ProxyOwner> proxy;

    auto it = proxies.find ( address );
    if ( it != proxies.end() )
    {
        proxy = it->second;
    }
    else
    {
        proxy.reset ( new ProxyOwner ( this, 0, address ) );
        proxies.insert ( make_pair ( address, proxy ) );
    }

    proxy->gbn.send ( msg );
}

void ReliableUdp::recvGbnAddressed ( const MsgPtr& msg, const IpAddrPort& address )
{
    shared_ptr<ProxyOwner> proxy;

    auto it = proxies.find ( address );
    if ( it != proxies.end() )
    {
        proxy = it->second;
    }
    else
    {
        proxy.reset ( new ProxyOwner ( this, 0, address ) );
        proxies.insert ( make_pair ( address, proxy ) );
    }

    proxy->gbn.recv ( msg );
}

ReliableUdp::ReliableUdp ( const shared_ptr<ProxyOwner>& proxy )
    : Socket ( proxy.get(), proxy->address.addr, proxy->address.port, Protocol::UDP ), state ( State::Connected )
    , proxy ( proxy )
{
    LOG ( "parent=%08x; proxy=%08x; owner=%08x; proxy->gbn=%08x", this, proxy.get(), owner, &proxy->gbn );
}

ReliableUdp::ReliableUdp ( Socket::Owner *owner, unsigned port )
    : Socket ( 0, port, Protocol::UDP ), state ( State::Listening ), proxy ( new ProxyOwner ( this, owner ) )
{
    LOG ( "parent=%08x; proxy=%08x; owner=%08x; proxy->gbn=%08x", this, proxy.get(), owner, &proxy->gbn );
    this->owner = proxy.get();
}

ReliableUdp::ReliableUdp ( Socket::Owner *owner, const string& address, unsigned port )
    : Socket ( 0, address, port, Protocol::UDP ), state ( State::Connecting )
    , proxy ( new ProxyOwner ( this, owner, IpAddrPort ( address, port ) ) )
{
    LOG ( "parent=%08x; proxy=%08x; owner=%08x; proxy->gbn=%08x", this, proxy.get(), owner, &proxy->gbn );
    this->owner = proxy.get();
    send ( new ReliableUdpConnect() );
}

ReliableUdp::~ReliableUdp()
{
    disconnect();
}

void ReliableUdp::disconnect()
{
    Socket::disconnect();
    state = State::Disconnected;
}

shared_ptr<Socket> ReliableUdp::listen ( Socket::Owner *owner, unsigned port )
{
    return shared_ptr<Socket> ( new ReliableUdp ( owner, port ) );
}

shared_ptr<Socket> ReliableUdp::connect ( Socket::Owner *owner, const string& address, unsigned port )
{
    return shared_ptr<Socket> ( new ReliableUdp ( owner, address, port ) );
}

shared_ptr<Socket> ReliableUdp::accept ( Socket::Owner *owner )
{
    if ( !acceptedSocket.get() )
        return 0;

    assert ( typeid ( *acceptedSocket ) == typeid ( ReliableUdp ) );
    static_cast<ReliableUdp *> ( acceptedSocket.get() )->proxy->owner = owner;
    return acceptedSocket;
}

void ReliableUdp::send ( Serializable *message, const IpAddrPort& address )
{
    MsgPtr msg ( message );
    send ( msg, address );
}

void ReliableUdp::send ( const MsgPtr& msg, const IpAddrPort& address )
{
    switch ( msg->getBaseType() )
    {
        case BaseType::SerializableMessage:
            Socket::send ( msg, address );
            break;

        case BaseType::SerializableSequence:
            if ( address.empty() || address == getRemoteAddress() )
                proxy->gbn.send ( msg );
            else
                sendGbnAddressed ( msg, address );
            break;

        default:
            LOG ( "Unhandled BaseType '%s'!", TO_C_STR ( msg->getBaseType() ) );
            break;
    }
}