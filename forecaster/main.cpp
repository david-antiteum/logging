// https://github.com/Microsoft/cpprestsdk
#include <cpprest/json.h>

//https://github.com/gabime/spdlog
#include <spdlog/spdlog.h>
#include <spdlog/fmt/fmt.h>

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
	void get( http_request & request ) override
	{
		const auto 			uri = request.request_uri();

		spdlog::debug( "{} {} from {}", request.method(), uri.to_string(), request.remote_address() );

		if( uri.path() == "/forecasting" ){
			auto		spam = utils::newSpam( request, "forecasting" );
			const auto	query = web::uri::split_query( uri.query() );

			if( query.count( "symbol" ) > 0  && query.count( "value" ) > 0 ){
				const auto foreMaybe = getForecasting( query.at( "symbol" ), std::stod( query.at( "value" ) ));
				if( foreMaybe ){
					spam->SetTag( "http.status_code", status_codes::OK );

					spdlog::debug( "Forecasting for symbol {}: {}", query.at( "symbol" ), foreMaybe.value() );
					request.reply( status_codes::OK, fmt::format( "{{ \"value\": {} }}", foreMaybe.value() ), "application/json; charset=utf-8" );
				}else{
					spam->SetTag( "error", true );
					spam->SetTag( "http.status_code", status_codes::NotFound );

					spdlog::error( "No forecasting for symbol {}", query.at( "symbol" ) );
					request.reply( status_codes::NotFound, "{}", "application/json; charset=utf-8" );
				}
			}else{
				spam->SetTag( "error", true );
				spam->SetTag( "http.status_code", status_codes::BadRequest );

				spdlog::error( "Missing required parameters {}", uri.to_string() );
				request.reply( status_codes::BadRequest, "{}", "application/json; charset=utf-8" );
			}
			spam->Finish();
		}else{
			spdlog::error( "Unknown route {}", uri.to_string() );
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

 	options
	 	.positional_help("[optional args]")
		.show_positional_help();

	options.add_options()
		("help", "Print help")
		("verbose", "Increase log level", cxxopts::value<bool>( verbose )->default_value("false") )
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
	if( verbose ){
		spdlog::set_level(spdlog::level::debug);
	}
	MyHTTPServer	server;

	server.run( "forecaster", port );

	return 0;
}
