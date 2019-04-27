
#pragma once

#include <string>
#include <optional>
#include <thread>
#include <atomic>

#include <cpprest/http_client.h>
#include <cpprest/json.h>

#include <spdlog/fmt/fmt.h>

namespace consul
{

struct Service
{
	std::string		mId;
	int				mPort = 0;
};

class Services
{
private:
	const std::string mConsulAddress{ "http://127.0.0.1:8500/v1" };

public:
	std::optional<Service> get( const std::string & id ) const
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

	void add( const std::string & name, int port, const std::string & group ) const
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

	void remove( const std::string & id, const std::string & group ) const
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

using Session = std::string;

class Sessions
{
private:
	const std::string mConsulAddress{ "http://127.0.0.1:8500/v1" };

public:
	Session create() const
	{
		const std::string 				query = fmt::format( "{}/session/create", mConsulAddress );
		web::http::client::http_client 	client( utility::conversions::to_string_t( query ));
		web::http::http_request			req( web::http::methods::PUT );
		Session							res;

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
					res = jsonRes.at( utility::conversions::to_string_t( "ID" )).as_string();
				}
			}catch( web::http::http_exception const & e ){
				std::wcout << e.what() << std::endl;
			}
		})
		.wait();
	
		return res;
	}

	void destroy( const Session & id ) const
	{
		const std::string 				query = fmt::format( "{}/session/destroy/{}", mConsulAddress, id );
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

class Leader
{
private:
	const std::string mConsulAddress{ "http://127.0.0.1:8500/v1" };

public:
	enum class Status {
		Yes,
		No,
		Error
	};

	Status acquire( const std::string & service, const Session & session ) const
	{
		Status							res = Status::Error;
		const std::string 				query = fmt::format( "{}/kv/service/{}/leader?acquire={}", mConsulAddress, service, session );
		web::http::client::http_client 	client( utility::conversions::to_string_t( query ));
		web::http::http_request			req( web::http::methods::PUT );

		client.request( req ).then([]( web::http::http_response response ){
			if( response.status_code() == web::http::status_codes::OK ){
				return response.extract_string();
			}
			return pplx::task_from_result( std::string() );
		}).then([ &res ](pplx::task<utility::string_t> previousTask){
			try{
				const auto stringRes = previousTask.get();

				if( stringRes == U( "true" ) ){
					res = Status::Yes;
				}else if( stringRes == U( "true" ) ){
					res = Status::No;
				}
			}catch( web::http::http_exception const & e ){
				std::wcout << e.what() << std::endl;
			}
		})
		.wait();

		return res;
	}

	void release( const std::string & service, const Session & session ) const
	{
		const std::string 				query = fmt::format( "{}/kv/service/{}/leader?release={}", mConsulAddress, service, session );
		web::http::client::http_client 	client( utility::conversions::to_string_t( query ));
		web::http::http_request			req( web::http::methods::PUT );

		client.request( req ).then([]( web::http::http_response response ){
			if( response.status_code() == web::http::status_codes::OK ){
				return response.extract_string();
			}
			return pplx::task_from_result( std::string() );
		}).then([](pplx::task<utility::string_t> previousTask){
			try{
				const auto stringRes = previousTask.get();

			}catch( web::http::http_exception const & e ){
				std::wcout << e.what() << std::endl;
			}
		})
		.wait();
	}
};

class Observer
{
private:
	const std::string mConsulAddress{ "http://127.0.0.1:8500/v1" };

public:
	Observer( const std::string & service, const Session & session )
		: mService( service )
		, mSession( session )
	{

	}

	~Observer()
	{
		if( mThread ){
			mRunThread = 0;
			mThread->join();
		}
	}

	void leader( std::function<void(Session)> leaderObserver )
	{
		mLeaderObserver = leaderObserver;
	}

	void run()
	{
		mRunThread = 1;
		mThread = std::make_unique<std::thread>( &Observer::realRun, this );
	}

private:
	std::string						mService;
	Session							mSession;
	std::function<void(Session)> 	mLeaderObserver;

	std::unique_ptr<std::thread>	mThread;
	std::atomic<short> 				mRunThread;

	void realRun()
	{
		using namespace std::chrono_literals;

		while( mRunThread > 0 ){
			if( mLeaderObserver ){
				const std::string 				query = fmt::format( "{}/kv/service/{}/leader", mConsulAddress, mService );
				web::http::client::http_client 	client( utility::conversions::to_string_t( query ));
				web::http::http_request			req( web::http::methods::GET );

				req.headers().set_content_type( U("application/json; charset=utf-8") );
				client.request( req ).then([]( web::http::http_response response ){
					if( response.status_code() == web::http::status_codes::OK ){
						return response.extract_json();
					}
					return pplx::task_from_result( web::json::value() );
				}).then([ this ](pplx::task<web::json::value> previousTask){
					try{
						const auto jsonRes = previousTask.get();
						Session session;

						if( jsonRes.is_array() ){
							const auto jsonArray = jsonRes.as_array();
							if( jsonArray.size() > 0 ){
								const auto leaderInfo = jsonArray.at(0);

								if( leaderInfo.has_field( utility::conversions::to_string_t( "Session" )) ){
									session = leaderInfo.at( utility::conversions::to_string_t( "Session" )).as_string();
								}
							}
						}
						if( session != mSession ){
							mLeaderObserver( session );
						}
					}catch( web::http::http_exception const & e ){
						std::wcout << e.what() << std::endl;
					}
				})
				.wait();
			}
			std::this_thread::sleep_for( 2s );
		}
	}
};

}
