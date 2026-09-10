#ifndef CGI_EXECUTOR_HPP
#define CGI_EXECUTOR_HPP

#include <string>
#include <map>
#include "CgiHandler.hpp"

// Döngüsel bağımlılığı engellemek için
class Client; 

class CgiExecutor
{
private:
    std::map<std::string, std::string>  _envMap;
    std::string                         _scriptPath;
    std::string                         _interpreter; // Opsiyonel (örn: /usr/bin/python3)

    char**  _allocateEnvp() const;
    void    _freeEnvp(char** envp) const;

public:
    // Orthodox Canonical Form (C++98)
    CgiExecutor();
    CgiExecutor(const CgiExecutor& copy);
    CgiExecutor& operator=(const CgiExecutor& assign);
    ~CgiExecutor();

    // Setters
    void    setScriptPath(const std::string& path);
    void    setInterpreter(const std::string& interpreter);
    void    addEnv(const std::string& key, const std::string& value);

    // CGI'yi çalıştırır ve iletişim kurmak için bir CgiHandler döndürür
    CgiHandler* execute(Client* client);
};

#endif