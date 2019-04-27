#pragma once

#include "otutils.h"

namespace utils {

class HTTPServer
{
public:
	explicit HTTPServer( std::shared_ptr<spdlog::logger> logger ) : mLogger( logger ) {}

	spdlog::logger & logger() const
	{
		return *mLogger.get();
	}

	void setGroup( const std::string & group )
	{
		mGroup = group;
	}

	virtual void get( web::http::http_request & request ) = 0;

	void run( const std::string & name, int port )
	{
		YAML::Node configYAML = YAML::Load( utils::defaultOpenTracingConfig );
#ifdef _JAEGER_ENABLED		
		auto config = jaegertracing::Config::parse( configYAML );
		auto tracer = jaegertracing::Tracer::make( name, config, jaegertracing::logging::consoleLogger());
		opentracing::Tracer::InitGlobal( std::static_pointer_cast<opentracing::Tracer>(tracer) );
#endif
		const std::string serverAddress = fmt::format("http://{0}:{1}", "127.0.0.1", port );
		auto listener = std::make_unique<web::http::experimental::listener::http_listener>( utility::conversions::to_string_t(serverAddress ));

		mLogger->debug( "Listener created." );

		listener->support( web::http::methods::GET, [this]( web::http::http_request request ){
			get( request );
		});
		try{
			const auto listenerTask = listener->open().then([ serverAddress, this ]()
			{
				mLogger->info( "REST server running at {}.", serverAddress );
			});
			const auto result = listenerTask.wait();
			if( result != pplx::completed ){
				mLogger->critical( "REST server fails to start." );
			}
		}catch( std::exception const& e ){
			mLogger->critical( "REST server exception." );
		}
		
		std::signal( SIGINT, HTTPServer::signalHandler );

		// All set, register service
		consul::Services	services;

		using namespace std::chrono_literals;

		services.add( name, port, mGroup );
		while( mSignalStatus == 0 ){
			std::this_thread::sleep_for( 500ms );
		}
		services.remove( name, mGroup );

		mLogger->info( "REST server closed." );
		opentracing::Tracer::Global()->Close();
	}

protected:
	std::string							mGroup = "primary";
	std::shared_ptr<spdlog::logger>		mLogger;
	static std::sig_atomic_t 			mSignalStatus;

	static void signalHandler( int signal )
	{
		mSignalStatus = signal;
	}
};

std::sig_atomic_t HTTPServer::mSignalStatus;

}
