# logging-meeting

Example code to present logging and tracing in the [Barcelona C++ Meetup group](https://www.meetup.com/es-ES/C-Programmer-Meetup/).

The code shows basic concepts and includes inefficient code, questionable constructions and repetitions.

The example has three services:
- apigateway: calls the internal services. Has a single GET end point /forecasting/{SYMBOL}
- pricereader: returns the price of a ticker symbol. GET end point /value/{SYMBOL}
- forecaster: calculates the forecasting of a ticker symbol. GET end point /forecasting?symbol={SYMBOL}&value={VALUE}

The apigateway will receive the request, call the pricereader to get the last value and pass it to the forecaster. It wil return to the user the forecasted value.

## Building

Use cmake to build the project.

## Dependencies


Header only libraries found in the include folder:

- [spdlog](https://github.com/gabime/spdlog): Very fast, header only, C++ logging library
- [fmt](https://github.com/fmtlib/fmt): {fmt} is an open-source formatting library for C++.
- [cxxopts](https://github.com/jarro2783/cxxopts): A lightweight C++ option parser library, supporting the standard GNU style syntax for options.

Libraries installed by the user:

- [boost](https://www.boost.org): Boost provides free peer-reviewed portable C++ source libraries.
- [C++ REST SDK](https://github.com/Microsoft/cpprestsdk): The C++ REST SDK is a Microsoft project for cloud-based client-server communication in native code using a modern asynchronous C++ API design.
- [OpenTracing API for C++](https://github.com/opentracing/opentracing-cpp): C++ implementation of the [OpenTracing API](http://opentracing.io)
- [jaeger-client-cpp](https://github.com/jaegertracing/jaeger-client-cpp): C++ OpenTracing binding for [Jaeger](https://www.jaegertracing.io/).
- [openssl](https://www.openssl.org): OpenSSL is a robust, commercial-grade, and full-featured toolkit for the Transport Layer Security (TLS) and Secure Sockets Layer (SSL) protocols.
- [yaml-cpp](https://github.com/jbeder/yaml-cpp): yaml-cpp is a YAML parser and emitter in C++ matching the YAML 1.2 spec.

## Running Jaeger

Start Jaeger, either the docker image:

```
docker run -d --name jaeger \
  -e COLLECTOR_ZIPKIN_HTTP_PORT=9411 \
  -p 5775:5775/udp \
  -p 6831:6831/udp \
  -p 6832:6832/udp \
  -p 5778:5778 \
  -p 16686:16686 \
  -p 14268:14268 \
  -p 9411:9411 \
  jaegertracing/all-in-one:1.11
```

or jaeger-all-in-one:

```
./jaeger-all-in-one --collector.zipkin.http-port=9411
```

Then start the three services.

Ask for a forecasting:

```
curl http://127.0.0.1:16000/forecasting/AMZN
```

See the traces in [Jaeger UI](http://localhost:16686).

## Running Graylog

Start Graylog:

```
docker run --name mongo -d mongo:3
docker run --name elasticsearch -e "http.host=0.0.0.0" -e "ES_JAVA_OPTS=-Xms512m -Xmx512m" -d docker.elastic.co/elasticsearch/elasticsearch-oss:6.6.1
docker run --name graylog --link mongo --link elasticsearch -p 9000:9000 -p 12201:12201 -p 1514:1514 -e GRAYLOG_HTTP_EXTERNAL_URI="http://127.0.0.1:9000/" -d graylog/graylog:3.0.0-2
```

Create a new Global Input (System menu) of type GELF HTTP using port 12201.

Start the services with the flag:

```
--graylog-host http://localhost:12201
```
