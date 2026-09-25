#include "CgiExecutor.hpp"
#include "FdUtils.hpp"

#include "HttpRequest.hpp"
#include "LocationConfig.hpp"
#include "ServerConfig.hpp"

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sstream>
#include <cctype>
#include <iostream>

namespace
{
    // Girdiyi tamamen büyük harfe çevirir.
    std::string toUpperCopy(const std::string& s)
    {
        std::string out;
        out.reserve(s.size());

        for (size_t i = 0; i < s.size(); ++i)
            out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(s[i]))));
        return out;
    }

    // HTTP header adını "HTTP_" önekli CGI ortam değişkeni anahtarına dönüştürür.
    std::string toEnvKey(const std::string& key)
    {
        std::string out = "HTTP_";

        for (size_t i = 0; i < key.size(); ++i)
        {
            char ch = key[i];
            if (ch == '-')
                out.push_back('_');
            else
                out.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
        }
        return out;
    }

    // İstek path'inden script adından sonra kalan PATH_INFO bölümünü çıkarır.
    std::string extractPathInfo(const std::string& requestPath, const std::string& resolvedScriptPath)
    {
        std::string scriptName;
        std::string::size_type slashPos = resolvedScriptPath.find_last_of('/');

        if (slashPos == std::string::npos)
            scriptName = resolvedScriptPath;
        else
            scriptName = resolvedScriptPath.substr(slashPos + 1);

        if (scriptName.empty())
            return std::string();

        std::string needle = "/" + scriptName;
        std::string::size_type pos = requestPath.find(needle);

        while (pos != std::string::npos)
        {
            std::string::size_type endPos = pos + needle.length();

            if (endPos == requestPath.length() || requestPath[endPos] == '/')
            {
                if (endPos < requestPath.length())
                    return requestPath.substr(endPos);
                return std::string();
            }
            pos = requestPath.find(needle, pos + 1);
        }
        return std::string();
    }
}

// Boş script path ve interpreter ile varsayılan CgiExecutor oluşturur.
CgiExecutor::CgiExecutor() : _scriptPath(""), _interpreter("")
{
}

// Başka bir CgiExecutor'ın alanlarını kopyalayarak yeni nesne oluşturur.
CgiExecutor::CgiExecutor(const CgiExecutor& copy)
{
    *this = copy;
}

// Bu nesneye başka bir CgiExecutor'ın env map, script path ve interpreter'ını atar.
CgiExecutor& CgiExecutor::operator=(const CgiExecutor& assign)
{
    if (this != &assign)
    {
        this->_envMap = assign._envMap;
        this->_scriptPath = assign._scriptPath;
        this->_interpreter = assign._interpreter;
    }
    return *this;
}

// Ek kaynak yönetimi gerekmediği için boş yıkıcı.
CgiExecutor::~CgiExecutor()
{
}

// Çalıştırılacak CGI script'inin dosya yolunu ayarlar.
void CgiExecutor::setScriptPath(const std::string& path)
{
    _scriptPath = path;
}

// CGI script'ini çalıştıracak yorumlayıcı (örn: python3) yolunu ayarlar.
void CgiExecutor::setInterpreter(const std::string& interpreter)
{
    _interpreter = interpreter;
}

// Env map'e tek bir anahtar/değer ortam değişkeni ekler.
void CgiExecutor::addEnv(const std::string& key, const std::string& value)
{
    _envMap[key] = value;
}

// İstek, location ve server bilgilerinden CGI için standart ortam değişkenlerini üretir.
void CgiExecutor::buildStandardEnv(const HttpRequest& request,
                                   const LocationConfig& location,
                                   const ServerConfig& serverConfig,
                                   const std::string& resolvedScriptPath)
{
    (void)location;

    addEnv("GATEWAY_INTERFACE", "CGI/1.1");
    addEnv("SERVER_PROTOCOL", toUpperCopy(request.getVersion()));
    addEnv("SERVER_SOFTWARE", "webserv/1.0");
    addEnv("REQUEST_METHOD", toUpperCopy(request.getMethod()));
    addEnv("SCRIPT_FILENAME", resolvedScriptPath);

    std::string requestPath = request.getPath();
    std::string pathInfo = extractPathInfo(requestPath, resolvedScriptPath);
    std::string scriptName = requestPath;

    if (!pathInfo.empty() && scriptName.length() >= pathInfo.length())
        scriptName = scriptName.substr(0, scriptName.length() - pathInfo.length());

    if (pathInfo.empty())
        pathInfo = scriptName;

    addEnv("REQUEST_URI", requestPath);

    addEnv("SCRIPT_NAME", scriptName);
    addEnv("PATH_INFO", pathInfo);
    addEnv("QUERY_STRING", request.getQueryString());

    std::ostringstream bodySize;
    bodySize << request.getBody().size();
    addEnv("CONTENT_LENGTH", bodySize.str());

    addEnv("CONTENT_TYPE", request.getHeader("content-type"));

    std::string serverName = "localhost";
    std::string serverPort;

    if (!serverConfig.listens.empty())
    {
        std::set<std::pair<std::string, int> >::const_iterator it = serverConfig.listens.begin();
        if (!it->first.empty())
            serverName = it->first;

        std::ostringstream portStream;
        portStream << it->second;
        serverPort = portStream.str();
    }

    addEnv("SERVER_NAME", serverName);
    addEnv("SERVER_PORT", serverPort);
    addEnv("REDIRECT_STATUS", "200");

    const std::map<std::string, std::string>& headers = request.getHeaders();
    for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); ++it)
        addEnv(toEnvKey(it->first), it->second);
}

// _envMap içeriğinden execve için char* dizisi (envp) tahsis eder.
char** CgiExecutor::_allocateEnvp() const
{
    char** envp = new char*[_envMap.size() + 1];
    int i = 0;
    
    for (std::map<std::string, std::string>::const_iterator it = _envMap.begin(); it != _envMap.end(); ++it)
    {
        std::string envStr = it->first + "=" + it->second;
        envp[i] = new char[envStr.length() + 1];
        strcpy(envp[i], envStr.c_str());
        i++;
    }
    envp[i] = NULL;
    return envp;
}

// _allocateEnvp ile ayrılan envp dizisini ve elemanlarını serbest bırakır.
void CgiExecutor::_freeEnvp(char** envp) const
{
    if (!envp)
        return;
    for (int i = 0; envp[i] != NULL; ++i)
        delete[] envp[i];
    delete[] envp;
}

// CGI script'ini pipe'lar üzerinden fork/execve ile çalıştırır ve iletişim için CgiHandler döner.
CgiHandler* CgiExecutor::execute(Client* client)
{
    int pipeStdin[2];
    int pipeStdout[2];

    if (pipe(pipeStdin) < 0)
    {
        std::cerr << "Error: pipe() failed." << std::endl;
        return NULL;
    }

    if (pipe(pipeStdout) < 0)
    {
        std::cerr << "Error: pipe() failed." << std::endl;
        close(pipeStdin[0]);
        close(pipeStdin[1]);
        return NULL;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        std::cerr << "Error: fork() failed." << std::endl;
        close(pipeStdin[0]); close(pipeStdin[1]);
        close(pipeStdout[0]); close(pipeStdout[1]);
        return NULL;
    }

    if (pid == 0)
    {
        close(pipeStdin[1]);
        close(pipeStdout[0]);

        dup2(pipeStdin[0], STDIN_FILENO);
        dup2(pipeStdout[1], STDOUT_FILENO);

        close(pipeStdin[0]);
        close(pipeStdout[1]);

        char** envp = _allocateEnvp();
        
        char** argv;
        if (!_interpreter.empty())
        {
            argv = new char*[3];
            argv[0] = strdup(_interpreter.c_str());
            argv[1] = strdup(_scriptPath.c_str());
            argv[2] = NULL;
        }
        else
        {
            argv = new char*[2];
            argv[0] = strdup(_scriptPath.c_str());
            argv[1] = NULL;
        }

        execve(argv[0], argv, envp);

        std::cerr << "Error: execve() failed for " << _scriptPath << std::endl;
        _freeEnvp(envp);
        
        if (!_interpreter.empty())
        {
            free(argv[0]);
            free(argv[1]);
        }
        else
            free(argv[0]);
        delete[] argv;
        
        exit(1); 
    }
    else
    {
        close(pipeStdin[0]);
        close(pipeStdout[1]);

        FdUtils::setNonBlocking(pipeStdout[0]);
        FdUtils::setCloseOnExec(pipeStdout[0]);
        FdUtils::setNonBlocking(pipeStdin[1]);
        FdUtils::setCloseOnExec(pipeStdin[1]);

        return new CgiHandler(pipeStdout[0], pipeStdin[1], pid, client);
    }
}