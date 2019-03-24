#pragma once

// https://github.com/Microsoft/cpprestsdk
#include <cpprest/http_client.h>
#include <cpprest/http_listener.h>

// https://www.jaegertracing.io
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#include <jaegertracing/Tracer.h>
#include <yaml-cpp/yaml.h>
#pragma GCC diagnostic pop

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
			f( iter.first, iter.second );
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
		mRequest.headers().add( key, value );
		return {};
	}

private:
	web::http::http_request & mRequest;
};

std::unique_ptr<opentracing::Span> newSpam( const web::http::http_request & request, const std::string & name )
{
	std::unique_ptr<opentracing::Span>	spam;
	auto								parentContext = opentracing::Tracer::Global()->Extract( CPPRestHeaderReader( request ) );

	if( parentContext ){
		spam = opentracing::Tracer::Global()->StartSpan( name, { opentracing::ChildOf( parentContext.value().get() ) } );
	}else{
		spam = opentracing::Tracer::Global()->StartSpan( name );
	}
	// https://opentracing.io/specification/conventions/
	spam->SetTag( "http.method", request.method() );
	spam->SetTag( "http.url", request.absolute_uri().to_string() );

	return spam;
}

void injectContext( const opentracing::SpanContext & spamContext, web::http::http_request & request )
{
	opentracing::Tracer::Global()->Inject( spamContext, utils::CPPRestHeaderWriter( request ) );
}

class HTTPServer
{
public:
	virtual void get( web::http::http_request & request ) = 0;

	void run( const std::string & name, int port )
	{
		YAML::Node configYAML = YAML::Load( utils::defaultOpenTracingConfig );
		auto config = jaegertracing::Config::parse( configYAML );
		auto tracer = jaegertracing::Tracer::make( name, config, jaegertracing::logging::consoleLogger());
		opentracing::Tracer::InitGlobal( std::static_pointer_cast<opentracing::Tracer>(tracer) );

		const std::string serverAddress = fmt::format("http://{0}:{1}", "127.0.0.1", port );
		auto listener = std::make_unique<web::http::experimental::listener::http_listener>( serverAddress );

		spdlog::debug( "Listener created." );

		listener->support( web::http::methods::GET, [this]( web::http::http_request request ){
			get( request );
		});
		try{
			const auto listenerTask = listener->open().then([ serverAddress ]()
			{
				spdlog::info( "REST server running at {}.", serverAddress );
			});
			const auto result = listenerTask.wait();
			if( result != pplx::completed ){
				spdlog::critical( "REST server fails to start." );
			}
		}catch( std::exception const& e ){
			spdlog::critical( "REST server exception." );
		}
		getchar();

		spdlog::info( "REST server closed." );
		opentracing::Tracer::Global()->Close();
	}
};
}
