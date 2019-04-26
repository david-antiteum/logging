
#pragma once

#include <string>
#include <optional>

#include <cpprest/http_client.h>
#include <cpprest/json.h>

#include <spdlog/fmt/fmt.h>

namespace consul
{

class Service
{
public:
	std::string		mId;
	int				mPort = 0;
};

class Services
{
private:
	const std::string mConsulAddress{ "http://127.0.0.1:8500/v1" };

public:
	std::optional<Service> get( const std::string & id )
	{
		std::optional<Service>			res;
		const std::string 				query = fmt::format( "{}/agent/service/{}", mConsulAddress, id );
		web::http::client::http_client 	client( utility::conversions::to_string_t( query ));
		web::http::http_request			req( web::http::methods::GET );

		req.headers().set_content_type( U("application/json; charset=utf-8") );
		client.request( req ).then([]( web::http::http_response response ){
			if( response.status_code() == web::http::status_codes::OK ){
				return response.extract_json();
			}
			return pplx::task_from_result( web::json::value() );
		}).then([ &res ](pplx::task<web::json::value> previousTask){
			try{
				const auto jsonRes = previousTask.get();

				if( jsonRes.has_field( utility::conversions::to_string_t( "ID" )) ){
					Service	service;

					service.mId = jsonRes.at( utility::conversions::to_string_t( "ID" )).as_string();
					service.mPort = jsonRes.at( utility::conversions::to_string_t( "Port" )).as_number().to_int32();

					res = service;
				}
			}catch( web::http::http_exception const & e ){
				std::wcout << e.what() << std::endl;
			}
		})
		.wait();
		return res;
	}

	void add( const std::string & name, int port, const std::string & group )
	{
		const std::string 				query = fmt::format( "{}/agent/service/register", mConsulAddress );
		web::http::client::http_client 	client( utility::conversions::to_string_t( query ));
		web::http::http_request			req( web::http::methods::PUT );
		web::json::value				payload;
		web::json::value				payloadCheck;
		web::json::value				tags = web::json::value::array();

		payload[ U("ID") ] = web::json::value::string( utility::conversions::to_string_t( name + "_" + group ));
		payload[ U("Name") ] = web::json::value::string( utility::conversions::to_string_t( name ));
		payload[ U("Address") ] = web::json::value::string( U("127.0.0.1" ));
		payload[ U("Port") ] = web::json::value::number( port );

		if( !group.empty() ){
			tags.as_array()[0] = web::json::value::string( utility::conversions::to_string_t( group ));
		}
		payload[ U("tags") ] = tags;

		payloadCheck[ U( "HTTP" ) ] = web::json::value::string( utility::conversions::to_string_t( fmt::format( "http://127.0.0.1:{}/ping", port ) ));;
		payloadCheck[ U( "interval" ) ] = web::json::value::string( U( "110s" ) );
		payload[ U("Check") ] = payloadCheck;

		req.headers().set_content_type( U("application/json; charset=utf-8") );
		req.set_body( payload );
		client.request( req ).then([]( web::http::http_response response ){
			if( response.status_code() == web::http::status_codes::OK ){
				return response.extract_json();
			}
			return pplx::task_from_result( web::json::value() );
		}).then([](pplx::task<web::json::value> previousTask){
			try{
				const auto jsonRes = previousTask.get();

			}catch( web::http::http_exception const & e ){
				std::wcout << e.what() << std::endl;
			}
		})
		.wait();
	}

	void remove( const std::string & id, const std::string & group )
	{
		const std::string 				query = fmt::format( "{}/agent/service/deregister/{}_{}", mConsulAddress, id, group );
		web::http::client::http_client 	client( utility::conversions::to_string_t( query ));
		web::http::http_request			req( web::http::methods::PUT );

		client.request( req ).then([]( web::http::http_response response ){
			if( response.status_code() == web::http::status_codes::OK ){
				return response.extract_json();
			}
			return pplx::task_from_result( web::json::value() );
		}).then([](pplx::task<web::json::value> previousTask){
			try{
				const auto jsonRes = previousTask.get();

			}catch( web::http::http_exception const & e ){
				std::wcout << e.what() << std::endl;
			}
		})
		.wait();
	}
};

}
