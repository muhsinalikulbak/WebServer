*This project has been created as part of the 42 curriculum by mkulbak, bergin, bakarabu.*

# Description

WebServer is a C++98 HTTP server project. It serves static files and supports configurable routes, HTTP methods, file uploads, custom error pages, redirects, and CGI execution.

# Instructions

## Requirements

- A C++ compiler with C++98 support
- `make`
- Linux, for the server's `epoll`-based event handling

No separate installation step is provided; build the executable locally from the repository root.

## Build

Run:

```sh
make
```

This creates the `webserver` executable. The Makefile also provides `make clean` to remove object files, `make fclean` to remove object files and the executable, and `make re` to rebuild from scratch.

## Run

The server requires a configuration file argument. Start it from the repository root with the example configuration:

```sh
./webserver webserver.conf
```

The example configuration listens on `127.0.0.1:8080` and serves files from `www`. To use different settings, edit `webserver.conf` and pass its path when starting the server.

Check that the server is responding by opening `http://127.0.0.1:8080/` in a browser or running:

```sh
curl -i http://127.0.0.1:8080/
```

Stop the server with Ctrl+C.

The example configuration specifies `/usr/bin/python3` and `/usr/bin/php-cgi` for CGI handlers. Install the corresponding interpreter at that path, or update the configuration to point to its location, to use those CGI routes.

# Resources

- RFC 9110, HTTP Semantics: https://www.rfc-editor.org/rfc/rfc9110
- RFC 3875, The Common Gateway Interface (CGI) Version 1.1: https://www.rfc-editor.org/rfc/rfc3875
- GNU Make manual: https://www.gnu.org/software/make/manual/
- AI assistance for this documentation update: AI was used to summarize the project structure, Makefile, and example server configuration and to draft this README. No project source code was generated or modified as part of this README update.
