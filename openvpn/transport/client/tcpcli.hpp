#ifndef OPENVPN_TRANSPORT_CLIENT_TCPCLI_H
#define OPENVPN_TRANSPORT_CLIENT_TCPCLI_H

#include <sstream>

#include <boost/asio.hpp>

#include <openvpn/transport/tcplink.hpp>
#include <openvpn/transport/endpoint_cache.hpp>
#include <openvpn/transport/client/transbase.hpp>

namespace openvpn {
  namespace TCPTransport {

    OPENVPN_EXCEPTION(tcp_transport_resolve_error);
    OPENVPN_EXCEPTION(tcp_transport_error);

    class ClientConfig : public TransportClientFactory
    {
    public:
      typedef boost::intrusive_ptr<ClientConfig> Ptr;

      std::string server_host;
      std::string server_port;
      size_t send_queue_max_size;
      size_t free_list_max_size;
      Frame::Ptr frame;
      SessionStats::Ptr stats;

      static Ptr new_obj()
      {
	return new ClientConfig;
      }

      virtual TransportClient::Ptr new_client_obj(boost::asio::io_service& io_service,
						  TransportClientParent& parent);

      EndpointCache<TCPTransport::Endpoint> endpoint_cache;

    private:
      ClientConfig()
	: send_queue_max_size(64),
	  free_list_max_size(8)
      {}
    };

    class Client : public TransportClient
    {
      friend class ClientConfig;  // calls constructor
      friend class Link<Client*>; // calls tcp_read_handler

      typedef Link<Client*> LinkImpl;

      typedef AsioDispatchResolve<Client,
				  void (Client::*)(const boost::system::error_code&, boost::asio::ip::tcp::resolver::iterator),
				  boost::asio::ip::tcp::resolver::iterator> AsioDispatchResolveTCP;

    public:
      virtual void start()
      {
	if (!impl)
	  {
	    halt = false;
	    if (config->endpoint_cache.defined())
	      {
		server_endpoint = config->endpoint_cache.endpoint();
		start_impl_();
	      }
	    else
	      {
		boost::asio::ip::tcp::resolver::query query(config->server_host,
							    config->server_port);
		parent.transport_pre_resolve();
		resolver.async_resolve(query, AsioDispatchResolveTCP(&Client::post_start_, this));
	      }
	  }
      }

      virtual bool transport_send_const(const Buffer& buf)
      {
	return send_const(buf);
      }

      virtual bool transport_send(BufferAllocated& buf)
      {
	return send(buf);
      }

      virtual void server_endpoint_info(std::string& host, std::string& port, std::string& proto, std::string& ip_addr) const
      {
	host = config->server_host;
	port = config->server_port;
	const IP::Addr addr = server_endpoint_addr();
	proto = "TCP";
	proto += addr.version_string();
	ip_addr = addr.to_string();
      }

      virtual IP::Addr server_endpoint_addr() const
      {
	return IP::Addr::from_asio(server_endpoint.address());
      }

      virtual void stop() { stop_(); }
      virtual ~Client() { stop_(); }

    private:
      Client(boost::asio::io_service& io_service_arg,
	     ClientConfig* config_arg,
	     TransportClientParent& parent_arg)
	:  io_service(io_service_arg),
	   config(config_arg),
	   parent(parent_arg),
	   resolver(io_service_arg),
	   halt(false)
      {
      }

      bool send_const(const Buffer& cbuf)
      {
	if (impl)
	  {
	    BufferAllocated buf(cbuf, 0);
	    return impl->send(buf);
	  }
	else
	  return false;
      }

      bool send(BufferAllocated& buf)
      {
	if (impl)
	  return impl->send(buf);
	else
	  return false;
      }

      void tcp_read_handler(BufferAllocated& buf) // called by LinkImpl
      {
	parent.transport_recv(buf);
      }

      void tcp_error_handler(const char *error)
      {
	std::ostringstream os;
	os << "Transport error on '" << config->server_host << ": " << error;
	stop();
	tcp_transport_error err(os.str());
	parent.transport_error(err);
      }

      void stop_()
      {
	if (impl)
	  {
	    impl->stop();
	    impl.reset();
	  }
	resolver.cancel();
	halt = true;
      }

      void post_start_(const boost::system::error_code& error,
		       boost::asio::ip::tcp::resolver::iterator endpoint_iterator)
      {
	if (!halt)
	  {
	    if (!error)
	      {
		// get resolved endpoint
		server_endpoint = *endpoint_iterator;
		start_impl_();
	      }
	    else
	      {
		std::ostringstream os;
		os << "DNS resolve error on '" << config->server_host << "' for TCP session: " << error;
		config->stats->error(Error::RESOLVE_ERROR);
		stop();
		tcp_transport_resolve_error err(os.str());
		parent.transport_error(err);
	      }
	  }
      }

      void start_impl_()
      {
	config->endpoint_cache.set_endpoint(server_endpoint);
	impl.reset(new LinkImpl(io_service,
				this,
				server_endpoint,
				REMOTE_CONNECT,
				false,
				config->send_queue_max_size,
				config->free_list_max_size,
				config->frame,
				config->stats));
	impl->start();
	parent.transport_connecting();
      }

      boost::asio::io_service& io_service;
      ClientConfig::Ptr config;
      TransportClientParent& parent;
      LinkImpl::Ptr impl;
      boost::asio::ip::tcp::resolver resolver;
      TCPTransport::Endpoint server_endpoint;
      bool halt;
    };

    inline TransportClient::Ptr ClientConfig::new_client_obj(boost::asio::io_service& io_service,
							     TransportClientParent& parent)
    {
      return TransportClient::Ptr(new Client(io_service, this, parent));
    }
  }
} // namespace openvpn

#endif
