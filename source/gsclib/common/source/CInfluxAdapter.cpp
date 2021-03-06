
#include "../include/CInfluxAdapter.h"

#include <cpprest/http_client.h>

#include <log4cxx/logger.h>

#include <stdexcept>

namespace GSC { namespace Common {
   
struct CInfluxAdapter::Impl   
{
   static web::http::client::http_client_config doConfig()
   {
      web::http::client::http_client_config config;
      config.set_timeout(config.timeout()); ///< Keep default but maybe we should decrease it to a few s
      return config;
   }
   
   Impl(web::uri uri, std::string db) 
      :m_config(doConfig())
      ,m_client(uri, m_config) 
      ,m_queryURI(web::uri_builder("/query").append_query("db", db).to_uri())
      ,m_writeURI(web::uri_builder("/write").append_query("db", db).to_uri())
      ,m_pingURI(web::uri_builder("/ping").to_uri())
      ,m_logger(log4cxx::Logger::getLogger("GSC.Common.CInfluxAdapter"))
   {
      createDatabase(db).wait();
      LOG4CXX_INFO(m_logger, "Created database: " << db);
   }
   
   pplx::task<web::http::http_response> createDatabase(std::string name)
   {
      web::http::http_request request(web::http::methods::POST);
      std::ostringstream os;
      os << "CREATE DATABASE \"" << name << "\"";
      request.set_request_uri(web::uri_builder("/query").append_query("q", os.str()).to_uri());
      return doRequest(std::move(request), web::http::status_codes::OK);
   }
   
   pplx::task<web::http::http_response> write(std::string data)
   {
      web::http::http_request request(web::http::methods::POST);
      request.set_request_uri(m_writeURI);
      request.set_body(utility::string_t(data));
      return doRequest(std::move(request), web::http::status_codes::NoContent);
   }
   
   pplx::task<web::http::http_response> ping()
   {  
      web::http::http_request request(web::http::methods::GET);
      request.set_request_uri(m_pingURI);
      return doRequest(std::move(request), web::http::status_codes::NoContent);
   }
   
   pplx::task<web::http::http_response> doRequest(web::http::http_request&& request, web::http::status_code expect)
   {  
      LOG4CXX_INFO(m_logger, "HTTP request: " << request.to_string());
      return m_client.request(std::move(request)).then([this, expect](web::http::http_response response)
      {
         if (response.status_code() != expect)
         {  
            LOG4CXX_INFO(m_logger, "Request unsuccessful with code: " << response.status_code());
            throw std::runtime_error("Request unsuccessful"); 
         }
         return response;
      });
   }
   
private:
   web::http::client::http_client_config m_config;
   web::http::client::http_client m_client;
   web::uri m_queryURI;
   web::uri m_writeURI;
   web::uri m_pingURI;
   log4cxx::LoggerPtr m_logger;
};
   
CInfluxAdapter::CInfluxAdapter() : m_impl(std::make_unique<Impl>(web::uri_builder("http://localhost:8086").to_uri(), "gsc"))
{
    /**  \todo Create empty series for 'active' and 'exceeding'
    */
}

CInfluxAdapter::~CInfluxAdapter() = default;

void CInfluxAdapter::insert(API::IRatedMatch::SetType const& rated)
{
   std::ostringstream os;
   os << "active system=1.0\n";
   
   if (rated.empty())
   {
      m_impl->write(os.str()).wait();
      return;
   }  
   
   for (auto const& e : rated) 
   {  os << "active " << e->getName() << "=" << boost::rational_cast<float>(e->getRatio()) << "\n"; }
   
   m_impl->write(os.str()).wait();
}

void CInfluxAdapter::ping() 
{  m_impl->ping().wait(); }

}}
