#pragma once

// https://github.com/Microsoft/cpprestsdk
#include <cpprest/http_client.h>
#include <cpprest/http_listener.h>
#include <cpprest/json.h>

#ifdef _JAEGER_ENABLED
// https://www.jaegertracing.io
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <jaegertracing/Tracer.h>
#pragma GCC diagnostic pop
#else
#include <opentracing/tracer.h>
#include <opentracing/span.h>
#endif

#include <yaml-cpp/yaml.h>

//https://github.com/gabime/spdlog
#include <spdlog/spdlog.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/fmt/fmt.h>

#include <chrono>

namespace utils {

static const std::string defaultOpenTracingConfig = "\
disabled: false\n\
reporter:\n\
    logSpans: true\n\
sampler:\n\
  type: const\n\
  param: 1";

class CPPRestHeaderReader: public opentracing::HTTPHeadersReader
{
public:
	explicit CPPRestHeaderReader( const web::http::http_request & request ) : mRequest( request ) {}

	opentracing::expected<void> ForeachKey( std::function<opentracing::expected<void>(opentracing::string_view key, opentracing::string_view value)> f) const override
	{
		for( auto iter: mRequest.headers() ){
			f( utility::conversions::to_utf8string( iter.first ), utility::conversions::to_utf8string( iter.second ) );
		}
		return {};
	}

private:
	const web::http::http_request & mRequest;
};

class CPPRestHeaderWriter: public opentracing::HTTPHeadersWriter
{
public:
	explicit CPPRestHeaderWriter( web::http::http_request & request ) : mRequest( request ) {}

	opentracing::expected<void> Set( opentracing::string_view key, opentracing::string_view value ) const override
	{
		mRequest.headers().add( utility::conversions::to_string_t(key), utility::conversions::to_string_t(value) );
		return {};
	}

private:
	web::http::http_request & mRequest;
};

std::unique_ptr<opentracing::Span> newSpan( const web::http::http_request & request, const std::string & name )
{
	std::unique_ptr<opentracing::Span>	span;
	auto								parentContext = opentracing::Tracer::Global()->Extract( CPPRestHeaderReader( request ) );

	if( parentContext ){
		span = opentracing::Tracer::Global()->StartSpan( name, { opentracing::ChildOf( parentContext.value().get() ) } );
	}else{
		span = opentracing::Tracer::Global()->StartSpan( name );
	}
	// https://opentracing.io/specification/conventions/
	span->SetTag( "http.method", utility::conversions::to_utf8string( request.method() ));
	span->SetTag( "http.url", utility::conversions::to_utf8string( request.absolute_uri().to_string() ));

	return span;
}

void injectContext( const opentracing::SpanContext & spanContext, web::http::http_request & request )
{
	opentracing::Tracer::Global()->Inject( spanContext, utils::CPPRestHeaderWriter( request ) );
}

template<typename Mutex>
class graylog_sink : public spdlog::sinks::base_sink<Mutex>
{
public:
	graylog_sink( const std::string & hostName, const std::string & graylogHost ) 
		: mHostName( hostName )
	{
		mGraylogService = fmt::format( "{}/gelf", graylogHost );
	}

protected:
	void sink_it_(const spdlog::details::log_msg& msg) override
	{
		if( msg.level != spdlog::level::level_enum::off ){
			web::json::value 				logJSON;

			logJSON[U("version")] = web::json::value::string( U("1.1") );
			logJSON[U("host")] = web::json::value::string( utility::conversions::to_string_t(mHostName ));
			logJSON[U("short_message")] = web::json::value::string( utility::conversions::to_string_t(fmt::format( msg.payload ) ));
			logJSON[U("timestamp")] = web::json::value::number( std::chrono::duration_cast<std::chrono::milliseconds>( msg.time.time_since_epoch() ).count() / 1000.0 );
			logJSON[U("level")] = web::json::value::number( toLevel( msg.level ) );

			web::http::client::http_client 	client( utility::conversions::to_string_t(mGraylogService) );
			web::http::http_request			req( web::http::methods::POST );

			req.headers().set_content_type( U("application/json; charset=utf-8") );
			req.set_body( logJSON );

			auto response = client.request( req ).get();
			if( response.status_code() != web::http::status_codes::OK && response.status_code() != web::http::status_codes::Accepted ){
				spdlog::error("Error writting to graylog. Service: {}, error: {}", mGraylogService, response.status_code() );
			}
		}
	}

	void flush_() override 
	{
	}

private:
	std::string			mHostName;
	std::string			mGraylogService;

	int toLevel( spdlog::level::level_enum level ){
		int grayLevel = 1;

		switch( level )
		{
			case spdlog::level::level_enum::trace :
				grayLevel = 7;
			break;

			case spdlog::level::level_enum::debug :
				grayLevel = 7;
			break;

			case spdlog::level::level_enum::info :
				grayLevel = 6;
			break;

			case spdlog::level::level_enum::warn :
				grayLevel = 4;
			break;

			case spdlog::level::level_enum::err :
				grayLevel = 3;
			break;

			case spdlog::level::level_enum::critical :
				grayLevel = 2;
			break;

			default:
			break;			
		}
		return grayLevel;
	}
};

std::shared_ptr<spdlog::logger> newLogger( const std::string & appName, bool verbose, const std::string & logFile, const std::string & graylogHost )
{
	std::shared_ptr<spdlog::logger> 	res;
	std::vector<spdlog::sink_ptr> 		sinks;

	sinks.push_back( std::make_shared<spdlog::sinks::stdout_color_sink_st>() );
	if( !logFile.empty() ){
		sinks.push_back( std::make_shared<spdlog::sinks::rotating_file_sink_mt>( logFile, 1048576 * 5, 3 ) );
	}
	if( !graylogHost.empty() ){
		sinks.push_back( std::make_shared<graylog_sink<spdlog::details::null_mutex>>( appName, graylogHost ) );
	}
	res = std::make_shared<spdlog::logger>( appName, sinks.begin(), sinks.end() );

	if( verbose ){
		res->set_level( spdlog::level::debug );
	}
	return res;
}

class HTTPServer
{
public:
	explicit HTTPServer( std::shared_ptr<spdlog::logger> logger ) : mLogger( logger ) {}

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
		getchar();

		mLogger->info( "REST server closed." );
		opentracing::Tracer::Global()->Close();
	}

protected:
	std::shared_ptr<spdlog::logger>		mLogger;
};

}
