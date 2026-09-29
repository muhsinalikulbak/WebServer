# WebServer

A single-threaded HTTP/1.1 server written in C++98 on top of Linux `epoll`. It serves static files, handles uploads and deletes, issues redirects, and runs CGI scripts, all driven by an nginx-style configuration file.

## Features

- Non-blocking I/O with a single level-triggered `epoll` event loop
- HTTP/1.1 with persistent (keep-alive) connections
- Pipelined requests: several requests can arrive back to back; they are handled one at a time and answered in order
- `GET`, `POST` and `DELETE`, restricted per location
- Static file serving with index files, directory listings (`autoindex`) and MIME types
- File upload (`POST`) and file removal (`DELETE`)
- Chunked request bodies
- Per-server and per-location request body limits (`client_max_body_size`)
- Redirects (`return`) and custom error pages (`error_page`)
- CGI execution (any interpreter, mapped by file extension)
- Multiple `server` blocks and multiple `listen` addresses
- Graceful shutdown on `SIGINT` / `SIGTERM`

## Getting started

### Requirements

- Linux (the server uses `epoll`)
- A C++ compiler with C++98 support (`c++`, built with `-Wall -Wextra -Werror -std=c++98`)
- `make`
- For the example CGI routes: `python3` and `php-cgi` at the paths used in the configuration

### Build

```
make
```

This produces the `webserver` executable. Other targets: `make clean` (remove object files), `make fclean` (also remove the executable), `make re` (rebuild from scratch).

### Run

```
./webserver webserver.conf
```

The configuration file is a required argument. The example configuration listens on `127.0.0.1:8080` and serves files from `www/`. Stop the server with `Ctrl+C`; it closes all connections, kills and reaps running CGI children, and exits cleanly.

Quick check:

```
curl -i http://127.0.0.1:8080/
```

## Architecture

### Event loop

One thread, one `epoll` instance, level-triggered. Listening sockets, client connections and CGI pipes all derive from a common `EpollHandler` base class, and events are dispatched through `data.ptr`. A set of live handlers protects the dispatch loop against use-after-free when one object is registered under several file descriptors and is deleted while more events for it are pending in the same `epoll_wait` batch. All descriptors are non-blocking and close-on-exec.

### Request lifecycle

```
recv -> RequestParser -> RequestValidator -> Router -> ResponseBuilder -> write buffer -> send
                                                            |
                                                            +-> StaticHandler / UploadHandler / CGI
```

1. **RequestParser** is an incremental state machine (`REQUEST_LINE` → `HEADERS` → `BODY` or `CHUNKED_BODY` → `COMPLETE`, or `ERROR`). It enforces header count and body size limits while parsing and reports the specific status code for a failure.
2. **RequestValidator** checks the method, URI, HTTP version and `Host` header.
3. **Router** picks the `location` with the longest matching prefix (matched on path-segment boundaries).
4. **ResponseBuilder** applies redirects, method restrictions and body limits, resolves the file path, and dispatches to the static, upload or CGI handler.
5. The response is placed in the client's write buffer and sent as the socket becomes writable (`EPOLLOUT`).

### Keep-alive and pipelining

Connections stay open after each response. A single `recv()` may contain more than one request; the extra bytes stay buffered and are parsed only after the current response has been fully sent. Requests are therefore processed strictly one at a time and responses always come back in request order, which is what HTTP/1.1 pipelining requires. A consequence is that a slow request (for example a long-running CGI script) delays the requests queued behind it on the same connection. Different connections are fully independent.

### CGI

The server `fork()`s and `execve()`s the interpreter configured for the script's extension (or the script itself if no interpreter is given). The request body is written to the child's stdin and its stdout is read back through non-blocking pipes registered in the same `epoll` loop, so a CGI script never blocks other clients. Output is parsed as a CGI response (headers, optional `Status:`, body).

Environment variables passed to the script: `GATEWAY_INTERFACE`, `SERVER_PROTOCOL`, `SERVER_SOFTWARE`, `REQUEST_METHOD`, `SCRIPT_FILENAME`, `SCRIPT_NAME`, `REQUEST_URI`, `PATH_INFO`, `QUERY_STRING`, `CONTENT_LENGTH`, `CONTENT_TYPE`, `SERVER_NAME`, `SERVER_PORT`, `REDIRECT_STATUS`, plus every request header as `HTTP_<NAME>`.

A script that runs longer than 60 seconds is killed and the client receives `504`. A script that produces no output and exits with an error (or fails to start) yields `502`.

## Configuration

### Example

```nginx
server {
    listen 127.0.0.1:8080;
    server_name example.com;
    client_max_body_size 10M;

    error_page 404 www/errors/404.html;
    error_page 500 www/errors/500.html;

    location / {
        allow_methods GET POST;
        root www;
        index index.html;
        autoindex on;
    }

    location /upload {
        allow_methods POST DELETE;
        root www/uploads/files;
        upload_store www/uploads/files;
    }

    location /cgi-bin {
        allow_methods GET POST;
        root www/cgi-bin;
        cgi_ext .py /usr/bin/python3;
        cgi_ext .php /usr/bin/php-cgi;
    }

    location /old-path {
        return 301 https://example.com/new-path;
    }
}
```

### `server` directives

| Directive | Arguments | Description |
| --- | --- | --- |
| `listen` | `host:port` or `port` | Address to bind. Host defaults to `0.0.0.0`. Required at least once per server; the same `host:port` may not be declared twice. |
| `server_name` | `name` | Stored, but currently not used to select a server (see [Limitations](#limitations)). |
| `client_max_body_size` | `N[K\|M\|G]` | Maximum request body size. Default `1M`. |
| `error_page` | `code path` | Custom page for a status code. If the file cannot be read, a built-in page is used. |

### `location` directives

| Directive | Arguments | Description |
| --- | --- | --- |
| `allow_methods` | `METHOD...` | Allowed methods (`GET`, `POST`, `DELETE`). Empty by default, meaning no method is allowed. |
| `root` | `path` | Directory the location maps to. |
| `index` | `file` | Index file for directories. Default `index.html`. |
| `autoindex` | `on` \| `off` | Generate a directory listing when there is no index file. Default `off`. |
| `return` | `code url` | Redirect. Applies to every method. |
| `upload_store` | `dir` | Directory where `POST` uploads and `DELETE` operate. |
| `upload_enable` | `on` \| `off` | Accepted by the parser but currently has no effect. |
| `cgi_ext` | `.ext interpreter` | Run files with this extension through the interpreter. May be repeated. |
| `client_max_body_size` | `N[K\|M\|G]` | Overrides the server-level limit for this location. |

Sizes accept a `K`, `M` or `G` suffix (case-insensitive, powers of 1024).

## Behaviour reference

### Static files

- A location's prefix is stripped from the URI and the remainder is appended to `root`.
- A request for a directory without a trailing slash gets a `301` to the slash-terminated path.
- A directory serves its `index` file if it exists; otherwise a listing if `autoindex on`; otherwise `404`.
- Paths containing `..` are rejected with `403`.
- Known MIME types: `html`/`htm`, `css`, `js`, `json`, `txt`, `csv`, `xml`. Anything else is served as `text/plain`.

### Uploads and deletes

- `POST /location/name` writes the body to `upload_store/name`. The file name is the last URI segment.
- A new file returns `201` with a `Location` header; overwriting an existing file returns `200`.
- `DELETE /location/name` removes the file and returns `204`.
- Missing `upload_store` returns `403`; an invalid file name returns `400`.

### Status codes

| Code | When |
| --- | --- |
| `400` | Malformed request line, missing `Host`, invalid `Content-Length`, both `Content-Length` and `Transfer-Encoding` present |
| `403` | Path traversal, directory used as an upload/delete target, no `upload_store` |
| `404` | No matching location or file |
| `405` | Method other than `GET`/`POST`/`DELETE`, or not in the location's `allow_methods` |
| `413` | Body larger than the effective `client_max_body_size` |
| `431` | More than 100 request headers |
| `501` | `Transfer-Encoding` other than `chunked` |
| `502` | CGI script failed to start or exited with an error and no output |
| `504` | CGI script exceeded the 60 second timeout |
| `505` | HTTP version other than 1.1 |

### Connections

- Every response carries `Connection: keep-alive`.
- A connection that is idle between requests for more than 4 seconds is closed (checked every 5 seconds, so the effective cutoff is 4 to 9 seconds).
- Multiple `listen` addresses, and multiple `server` blocks on different addresses, are supported. The server block is chosen by the socket the client connected to.

## Testing

Assuming the example configuration:

```bash
# Static file and directory listing
curl -i http://127.0.0.1:8080/
curl -i http://127.0.0.1:8080/index.html

# Redirect
curl -i http://127.0.0.1:8080/old-path

# Upload, fetch back, delete
curl -i -X POST --data-binary @file.txt http://127.0.0.1:8080/upload/file.txt
curl -i -X DELETE http://127.0.0.1:8080/upload/file.txt

# Chunked request body
curl -i -X POST -H "Transfer-Encoding: chunked" -d "hello" http://127.0.0.1:8080/upload/chunked.txt

# CGI (place a script in www/cgi-bin)
curl -i http://127.0.0.1:8080/cgi-bin/hello.py
curl -i -X POST -d "name=test" http://127.0.0.1:8080/cgi-bin/hello.py

# Method not allowed
curl -i -X DELETE http://127.0.0.1:8080/

# Two pipelined requests in one write; responses arrive in order
printf 'GET / HTTP/1.1\r\nHost: localhost\r\n\r\nGET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n' | nc 127.0.0.1 8080
```

`EVO.md` contains additional manual test scenarios.

## Project layout

```
.
├── includes/        Class and struct headers
├── src/
│   ├── cgi/         fork/exec, environment, pipe I/O, CGI response parsing
│   ├── http/        Request/response, parser, validator, router, static and upload handlers
│   ├── network/     Listening sockets, client connections, write buffering
│   ├── server/      Config parser, server/location config, epoll event loop
│   ├── utils/       File descriptor, file and MIME helpers
│   └── main.cpp     Entry point and signal setup
├── www/             Example content (pages, error pages, CGI scripts, upload directory)
├── webserver.conf   Example configuration
├── Makefile
└── EVO.md           Manual test scenarios
```

## Limitations

- Linux only (`epoll`); no TLS/HTTPS.
- HTTP/1.1 only. HTTP/1.0 requests are rejected with `505`.
- Only `GET`, `POST` and `DELETE`; no `HEAD`, `PUT`, `PATCH` or `OPTIONS`.
- `Connection: close` in a request is not honoured; connections are closed only on error, peer close or idle timeout. There is no per-connection request limit.
- `server_name` is parsed but not used: servers are selected by the address they listen on, not by the `Host` header.
- `upload_enable` is parsed but has no effect; uploads depend on `upload_store` and `allow_methods`.
- Uploads overwrite existing files without warning.
- Request line and individual header lengths are not limited (only the header count is).
- Only `Transfer-Encoding: chunked` is supported. No `Range`, compression or `sendfile`.
- Static responses cover a small set of MIME types; images are currently served as `text/plain`.
- Requests on one connection are processed sequentially, so a slow CGI script delays the requests queued behind it.

## References

- [RFC 9110: HTTP Semantics](https://www.rfc-editor.org/rfc/rfc9110)
- [RFC 9112: HTTP/1.1](https://www.rfc-editor.org/rfc/rfc9112)
- [RFC 3875: The Common Gateway Interface (CGI) Version 1.1](https://www.rfc-editor.org/rfc/rfc3875)
- [GNU Make manual](https://www.gnu.org/software/make/manual/)
