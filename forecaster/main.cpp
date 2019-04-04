// https://github.com/jarro2783/cxxopts
#include <cxxopts.hpp>

//
#include <optional>

#include "../utils/otutils.h"

using namespace utility;                    // Common utilities like string conversions
using namespace web;                        // Common features like URIs.
using namespace web::http;                  // Common HTTP functionality
using namespace web::http::client;          // HTTP client features
using namespace web::http::experimental::listener;

class MyHTTPServer: public utils::HTTPServer
{
public:
	explicit MyHTTPServer( std::shared_ptr<spdlog::logger> logger ) : HTTPServer( logger )
	{
	}

	void get( http_request & request ) override
	{
		const auto 			uri = request.request_uri();

		mLogger->debug( "{} {} from {}", request.method(), uri.to_string(), request.remote_address() );

		if( uri.path() == "/forecasting" ){
			auto		span = utils::newSpan( request, "forecasting" );
			const auto	query = web::uri::split_query( uri.query() );

			if( query.count( "symbol" ) > 0  && query.count( "value" ) > 0 ){
				const auto foreMaybe = getForecasting( query.at( "symbol" ), std::stod( query.at( "value" ) ));
				if( foreMaybe ){
					span->SetTag( "http.status_code", status_codes::OK );

					mLogger->debug( "Forecasting for symbol {}: {}", query.at( "symbol" ), foreMaybe.value() );
					request.reply( status_codes::OK, fmt::format( "{{ \"value\": {} }}", foreMaybe.value() ), "application/json; charset=utf-8" );
				}else{
					span->SetTag( "error", true );
					span->SetTag( "http.status_code", status_codes::NotFound );

					mLogger->error( "No forecasting for symbol {}", query.at( "symbol" ) );
					request.reply( status_codes::NotFound, "{}", "application/json; charset=utf-8" );
				}
			}else{
				span->SetTag( "error", true );
				span->SetTag( "http.status_code", status_codes::BadRequest );

				mLogger->error( "Missing required parameters {}", uri.to_string() );
				request.reply( status_codes::BadRequest, "{}", "application/json; charset=utf-8" );
			}
			span->Finish();
		}else{
			mLogger->error( "Unknown route {}", uri.to_string() );
			request.reply( status_codes::NotFound, "{}", "application/json; charset=utf-8" );
		}
	}

private:
	std::optional<float> getForecasting( const std::string & symbol, float currentValue )
	{
		std::optional<float>	res;

		if( symbol == "AAPL" ){
			res = currentValue;
		}else if( symbol == "FB" ){
			res = 0.9f * currentValue;
		}else if( symbol == "AMZN" ){
			res = 1.1f * currentValue;
		}
		return res;
	}
};

int main( int argc, char * argv[])
{
	cxxopts::Options 	options( argv[0], "Forecaster service." );
	int					port = 0;
	bool				verbose = false;
	std::string			logFile;
	std::string			graylogHost;

 	options
	 	.positional_help("[optional args]")
		.show_positional_help();

	options.add_options()
		("help", "Print help")
		("verbose", "Increase log level", cxxopts::value<bool>( verbose )->default_value("false") )
		("log-file", "Log file", cxxopts::value<std::string>( logFile ) )
		("graylog-host", "schema://host:port. Example: http://localhost:12201", cxxopts::value<std::string>( graylogHost ) )
		("p,port", "Port", cxxopts::value<int>( port )->default_value( "16001" ) );

	try{
		const auto result = options.parse(argc, argv);
		if( result.count( "help" )){
			std::cout << options.help({ "", "Group" }) << std::endl;
			exit(0);
		}
	}catch(const cxxopts::OptionException& e){
    	spdlog::critical( "error parsing options: {}", e.what() );
    	exit(1);
	}

	MyHTTPServer	server( utils::newLogger( "forecaster", verbose, logFile, graylogHost ) );

	server.run( "forecaster", port );

	return 0;
}
