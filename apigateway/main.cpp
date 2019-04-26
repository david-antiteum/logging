// https://github.com/jarro2783/cxxopts
#include <cxxopts.hpp>

//
#include <regex>
#include <optional>
#include <chrono>
#include <thread>

#include "../utils/otutils.h"
#include "../utils/consul_client.h"

using namespace utility;                    // Common utilities like string conversions
using namespace web;                        // Common features like URIs.
using namespace web::http;                  // Common HTTP functionality
using namespace web::http::client;          // HTTP client features
using namespace web::http::experimental::listener;

class MyHTTPServer: public utils::HTTPServer
{
public:
	explicit MyHTTPServer( int forePort, int pricePort, std::shared_ptr<spdlog::logger> logger ) : HTTPServer( logger )
	{
		mForecastingPort = forePort;
		mPricePort = pricePort;
	}

	void discover()
	{
		consul::Services	services;
		int					forePort = 0;
		int					pricePort = 0;

		using namespace std::chrono_literals;

		while( forePort == 0 || pricePort == 0 ){
			mLogger->debug( "Looking for services..." );
			if( forePort == 0 ){
				if( auto forecaster = services.get( "forecaster" ); forecaster ){
					forePort = forecaster.value().mPort;
					mLogger->debug( "Forecaster found in port {}", forePort );
				}
			}
			if( pricePort == 0 ){
				if( auto priceReader = services.get( "price-reader" ); priceReader ){
					pricePort = priceReader.value().mPort;
					mLogger->debug( "Price Reader found in port {}", pricePort );
				}
			}
			if( forePort == 0 || pricePort == 0 ){
				std::this_thread::sleep_for( 1s );
			}
		}
		mForecastingPort = forePort;
		mPricePort = pricePort;		
	}

	void get( http_request & request ) override
	{
		const std::string 		uri = utility::conversions::to_utf8string( request.request_uri().to_string() );

		mLogger->debug( "{} {} from {}", utility::conversions::to_utf8string( request.method() ), uri, utility::conversions::to_utf8string( request.remote_address() ));

		if( uri == "/ping" ){
			request.reply( status_codes::OK, "{}", "application/json; charset=utf-8" );
		}else{
			const std::regex 		rgx("/forecasting/(\\w+)");
			std::smatch 			match;

			if( std::regex_search( uri.begin(), uri.end(), match, rgx )){
				auto				span = utils::newSpan( request, "read-forecasting" );
				const std::string	symbol = match[1];
				const auto 			priceMaybe = getPrice( symbol, span->context() );

				span->SetTag( "symbol", symbol );

				if( priceMaybe ){
					mLogger->debug( "Price for symbol {}: {}", symbol, priceMaybe.value() );

					const auto foreMaybe = getForecasting( symbol, priceMaybe.value(), span->context() );
					if( foreMaybe ){
						span->SetTag( "http.status_code", status_codes::OK );

						mLogger->debug( "Forecasting for symbol {}: {}", symbol, foreMaybe.value() );
						request.reply( status_codes::OK, fmt::format( "{{ \"value\": {} }}", foreMaybe.value() ), "application/json; charset=utf-8" );
					}else{
						span->SetTag( "error", true );
						span->SetTag( "http.status_code", status_codes::NotFound );

						mLogger->error( "No forecasting for symbol {}", symbol );
						request.reply( status_codes::NotFound, "{}", "application/json; charset=utf-8" );
					}
				}else{
					span->SetTag( "error", true );
					span->SetTag( "http.status_code", status_codes::NotFound );

					mLogger->error( "No price for symbol {}", symbol );
					request.reply( status_codes::NotFound, "{}", "application/json; charset=utf-8" );
				}
				span->Finish();
			}else{
				mLogger->error( "Unknown route {}", uri );
				request.reply( status_codes::NotFound, "{}", "application/json; charset=utf-8" );
			}
		}
	}

private:
	int		mForecastingPort = 0;
	int		mPricePort = 0;

	std::optional<float> getPrice( const std::string & symbol, const opentracing::SpanContext & spanContext )
	{
		mLogger->debug( "Reading last value for symbol {}", symbol );

		std::optional<float>	res;
		const std::string 		query = fmt::format( "http://localhost:{}/value/{}", mPricePort, symbol );
		client::http_client 	client( utility::conversions::to_string_t( query ));
		http_request			req( methods::GET );

		utils::injectContext( spanContext, req );

		client.request( req ).then([ this ](http_response response){
			if( response.status_code() == status_codes::OK ){
				return response.extract_json();
			}else{
				mLogger->error( "Error accessing the symbol price. No value found. Error: {}", response.status_code() );
				return pplx::task_from_result(json::value());
			}
		}).then([ &res, this ](pplx::task<json::value> previousTask){
			try{
				const auto jsonRes = previousTask.get();

				if( jsonRes.has_field( utility::conversions::to_string_t( "value" )) ){
					const auto jsonValue = jsonRes.at( utility::conversions::to_string_t( "value" ));

					if( !jsonValue.is_null() && (jsonValue.is_number() || jsonValue.is_string())){
						if( jsonValue.is_number() ){
							res = jsonValue.as_number().to_double();
						}else{
							res = std::stod( jsonValue.as_string() );
						}
					}else{
						mLogger->error( "Error accessing the symbol price. Invalid response." );
					}
				}else{
					mLogger->error( "Error accessing the symbol price. No value found." );
				}
			}catch( const http_exception & e ){
				mLogger->error( "Error accessing the symbol price {}", e.what() );
			}catch(...){
				mLogger->error( "Error accessing the symbol price" );
			}
		}).wait();

		return res;
	}

	std::optional<float> getForecasting( const std::string & symbol, float currentValue, const opentracing::SpanContext & spanContext )
	{
		mLogger->debug( "Requesting forecasting for symbol {} at {}", symbol, currentValue );

		std::optional<float>	res;
		const std::string 		query = fmt::format( "http://localhost:{}/forecasting?symbol={}&value={}", mForecastingPort, symbol, currentValue );
		client::http_client 	client( utility::conversions::to_string_t( query ));
		http_request			req( methods::GET );

		utils::injectContext( spanContext, req );

		client.request( req ).then([ this ](http_response response){
			if( response.status_code() == status_codes::OK ){
				return response.extract_json();
			}else{
				mLogger->error( "Error accessing the symbol forecasting. Error: {}", response.status_code() );
				return pplx::task_from_result(json::value());
			}
		}).then([ &res, this ](pplx::task<json::value> previousTask){
			try{
				const auto jsonRes = previousTask.get();
				const auto jsonValue = jsonRes.at( utility::conversions::to_string_t( "value" ));

				if( !jsonValue.is_null() && (jsonValue.is_number() || jsonValue.is_string())){
					if( jsonValue.is_number() ){
						res = jsonValue.as_number().to_double();
					}else{
						res = std::stod( jsonValue.as_string() );
					}
				}
			}catch( const http_exception & e ){
				mLogger->error( "Error accessing the symbol forecasting {}", e.what() );
			}
		}).wait();

		return res;
	}
};

int main( int argc, char * argv[])
{
	cxxopts::Options 	options( argv[0], "API Gateway" );
	int					port = 0;
	bool				verbose = false;
	int					forecastingPort = 0;
	int 				pricePort = 0;
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
		("p,port", "Port", cxxopts::value<int>( port )->default_value( "16000" ) )
		("forecasting-port", "Port for the forecasting service.", cxxopts::value<int>( forecastingPort )->default_value( "16001" ) )
		("reader-port", "Port for the symbol reader service.", cxxopts::value<int>( pricePort )->default_value( "16002" ) );

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

	MyHTTPServer	server( forecastingPort, pricePort, utils::newLogger( "api-gateway", verbose, logFile, graylogHost ) );

	server.discover();
	server.run( "api-gateway", port );

	return 0;
}
