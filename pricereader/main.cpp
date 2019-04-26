// https://github.com/jarro2783/cxxopts
#include <cxxopts.hpp>

//
#include <regex>
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
	explicit MyHTTPServer( const std::string & apiKey, std::shared_ptr<spdlog::logger> logger ) : HTTPServer( logger )
	{
		mApiKey = apiKey;
		if( mApiKey.empty() ){
			logger->warn( "No API Key, we will use dummy data." );
		}
	}

	void get( http_request & request ) override
	{
		const std::string 	uri = utility::conversions::to_utf8string( request.request_uri().to_string() );

		mLogger->debug( "{} {} from {}", utility::conversions::to_utf8string( request.method() ), uri, utility::conversions::to_utf8string( request.remote_address() ));

		if( uri == "/ping" ){
			request.reply( status_codes::OK, "{}", "application/json; charset=utf-8" );
		}else{
			const std::regex 	rgx("/value/(\\w+)");
			std::smatch			match;

			if( std::regex_search( uri.begin(), uri.end(), match, rgx )){
				auto					span = utils::newSpan( request, "read-symbol" );
				const std::string		symbol = match[1];
				std::optional<float> 	priceMaybe = getPrice( symbol, span->context() );

				if( mApiKey.empty() ){
					priceMaybe = getFakePrice( symbol, span->context() );
				}else{
					priceMaybe = getPrice( symbol, span->context() );
				} 
				if( priceMaybe ){
					span->SetTag( "http.status_code", status_codes::OK );

					mLogger->debug( "Price for symbol {}: {}", symbol, priceMaybe.value() );
					request.reply( status_codes::OK, fmt::format( "{{ \"value\": {} }}", priceMaybe.value() ), "application/json; charset=utf-8" );
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
	std::string 	mApiKey;

	std::optional<float> getFakePrice( const std::string & symbol, const opentracing::SpanContext & /*spanContext*/ )
	{
		mLogger->debug( "Reading last value for symbol {}", symbol );

		std::optional<float>	res;

		if( symbol == "AAPL" ){
			res = 191.05;
		}else if( symbol == "FB" ){
			res = 164.34;
		}else if( symbol == "AMZN" ){
			res = 1764.77;
		}
		return res;
	}

	std::optional<float> getPrice( const std::string & symbol, const opentracing::SpanContext & spanContext )
	{
		mLogger->debug( "Reading last value for symbol {}", symbol );

		std::optional<float>	res;
		const std::string 		query = fmt::format( "https://www.alphavantage.co/query?function=GLOBAL_QUOTE&symbol={}&apikey={}", symbol, mApiKey );
		client::http_client 	client( utility::conversions::to_string_t( query ));
		http_request			req( methods::GET );

		// I doubt that alphavantage uses OpenTracing :)
		utils::injectContext( spanContext, req );

		client.request( req ).then([this](http_response response){
			if( response.status_code() == status_codes::OK ){
				return response.extract_json();
			}else{
				mLogger->error( "Error accessing the symbol price. Nothing returned. Error: {}", response.status_code() );
				return pplx::task_from_result(json::value());
			}
		}).then([ &res, this ](pplx::task<json::value> previousTask){
			try{
				const auto jsonRes = previousTask.get();
				if( jsonRes.has_field( utility::conversions::to_string_t( "Global Quote" ) ) && jsonRes.at( utility::conversions::to_string_t( "Global Quote" ) ).has_field( utility::conversions::to_string_t( "05. price" )) ){
					const auto jsonValue = jsonRes.at( utility::conversions::to_string_t( "Global Quote" ) ).at( utility::conversions::to_string_t( "05. price" ));

					if( !jsonValue.is_null() && (jsonValue.is_number() || jsonValue.is_string())){
						if( jsonValue.is_number() ){
							res = jsonValue.as_number().to_double();
						}else{
							res = std::stod( jsonValue.as_string() );
						}
					}else{
						mLogger->error( "Error accessing the symbol price" );
					}
				}
			}catch( const http_exception & e ){
				mLogger->error( "Error accessing the symbol price {}", e.what() );
			}catch(...){
				mLogger->error( "Error accessing the symbol price" );
			}
		}).wait();

		return res;
	}
};

int main( int argc, char * argv[])
{
	cxxopts::Options 	options( argv[0], "Reads stock values." );
	int					port = 0;
	bool				verbose = false;
	std::string			logFile;
	std::string			apiKey;
	std::string			graylogHost;

 	options
	 	.positional_help("[optional args]")
		.show_positional_help();

	options.add_options()
		("help", "Print help")
		("api-key", "Alphavantage API Key", cxxopts::value<std::string>( apiKey ) )
		("verbose", "Increase log level", cxxopts::value<bool>( verbose )->default_value("false") )
		("log-file", "Log file", cxxopts::value<std::string>( logFile ) )
		("graylog-host", "schema://host:port. Example: http://localhost:12201", cxxopts::value<std::string>( graylogHost ) )
		("p,port", "Port", cxxopts::value<int>( port )->default_value( "16002" ) );

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

	MyHTTPServer	server( apiKey, utils::newLogger( "price-reader", verbose, logFile, graylogHost ) );

	server.run( "price-reader", port );

	return 0;
}
