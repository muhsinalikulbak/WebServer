#include "CgiExecutor.hpp"
#include "FdUtils.hpp"
// Client sınıfının tanımına ihtiyaç duyacağımız için ekliyoruz
// #include "Client.hpp" 

#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <iostream>

CgiExecutor::CgiExecutor() : _scriptPath(""), _interpreter("")
{
}

CgiExecutor::CgiExecutor(const CgiExecutor& copy)
{
    *this = copy;
}

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

CgiExecutor::~CgiExecutor()
{
}

void CgiExecutor::setScriptPath(const std::string& path)
{
    _scriptPath = path;
}

void CgiExecutor::setInterpreter(const std::string& interpreter)
{
    _interpreter = interpreter;
}

void CgiExecutor::addEnv(const std::string& key, const std::string& value)
{
    _envMap[key] = value;
}

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

void CgiExecutor::_freeEnvp(char** envp) const
{
    if (!envp)
        return;
    for (int i = 0; envp[i] != NULL; ++i)
        delete[] envp[i];
    delete[] envp;
}

CgiHandler* CgiExecutor::execute(Client* client)
{
    int pipeStdin[2];
    int pipeStdout[2];

    // İki adet pipe oluşturuyoruz: 
    // Biri CGI'a veri göndermek, diğeri CGI'dan veri okumak için
    if (pipe(pipeStdin) < 0 || pipe(pipeStdout) < 0)
    {
        std::cerr << "Error: pipe() failed." << std::endl;
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
        // --- CHILD PROCESS ---
        
        close(pipeStdin[1]);  // Child stdin'e yazmayacak, okuyacak
        close(pipeStdout[0]); // Child stdout'tan okumayacak, yazacak

        // Standart dosya tanımlayıcılarını pipe'lara yönlendir
        dup2(pipeStdin[0], STDIN_FILENO);
        dup2(pipeStdout[1], STDOUT_FILENO);

        // Kullanılmış pipe fd'lerini temizle
        close(pipeStdin[0]);
        close(pipeStdout[1]);

        char** envp = _allocateEnvp();
        
        // Argümanları hazırla
        // Eğer yorumlayıcı (örn: python veya php-cgi) kullanılıyorsa ona göre argv oluşturulur
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

        // execve sadece hata durumunda return yapar
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
        // --- PARENT PROCESS ---
        
        close(pipeStdin[0]);  // Parent stdin'den okumayacak, child'a yazacak
        close(pipeStdout[1]); // Parent stdout'a yazmayacak, child'dan okuyacak

        FdUtils::setNonBlocking(pipeStdout[0]);
        FdUtils::setCloseOnExec(pipeStdout[0]);
        FdUtils::setNonBlocking(pipeStdin[1]);
        FdUtils::setCloseOnExec(pipeStdin[1]);

        // CgiHandler, Epoll'de okuma/yazma yapmak üzere dönülür
        // Okunacak yer: pipeStdout[0]
        // Yazılacak yer: pipeStdin[1]
        return new CgiHandler(pipeStdout[0], pipeStdin[1], pid, client);
    }
}